/* SPDX-License-Identifier: Apache-2.0
 * First-party, bounded and fail-closed TLS policy ownership for CNet.
 * Sole Salts::Crypto STATIC owner; this file has no TLS handshake, record,
 * crypto-provider loader, socket access or fallback implementation.
 */
#include "crypto_tls_profile_private.h"
#include "crypto_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>



/* Explicitly bounded. NULL means absent for optional fields. An empty
 * supplied path/name is invalid, never silently treated as omitted.
 */
static int text_valid(const char *text, size_t maximum, int empty_allowed)
{
    size_t i;
    if (!text) return 1;
    for (i = 0u; i <= maximum; ++i) {
        if (text[i] == '\0') return empty_allowed || i != 0u;
    }
    return 0;
}

static void text_copy(char *target, const char *source, size_t maximum)
{
    size_t n = 0u;
    if (!source) return;
    while (n < maximum && source[n]) ++n;
    memcpy(target, source, n);
    target[n] = '\0';
}

static crypto_tls_profile_result validate_alpn(
    const crypto_tls_profile_options_v1_4 *o, size_t *out_wire_size)
{
    size_t wire_size = 0u, i;
    if (o->alpn_count == 0u) {
        if (o->alpn != NULL) return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        *out_wire_size = 0u;
        return CRYPTO_TLS_PROFILE_OK;
    }
    /* Every length-prefixed, nonempty item uses at least 2 bytes. */
    if (o->alpn == NULL ||
        o->alpn_count > CRYPTO_TLS_PROFILE_ALPN_MAX_WIRE_BYTES / 2u)
        return CRYPTO_TLS_PROFILE_RANGE;
    for (i = 0u; i < o->alpn_count; ++i) {
        const crypto_tls_alpn_label_v1_4 *item = &o->alpn[i];
        if (!item->data || item->size == 0u || item->size > 255u)
            return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        if (wire_size > CRYPTO_TLS_PROFILE_ALPN_MAX_WIRE_BYTES -
                            (1u + item->size))
            return CRYPTO_TLS_PROFILE_RANGE;
        wire_size += 1u + item->size;
    }
    *out_wire_size = wire_size;
    return CRYPTO_TLS_PROFILE_OK;
}

static crypto_tls_profile_result validate_profile(
    const crypto_tls_profile_options_v1_4 *o, size_t *out_wire_size)
{
    crypto_tls_profile_result alpn_status;
    if (!o || o->struct_size != sizeof(*o) || o->reserved_flags || o->reserved ||
        (o->role != CRYPTO_TLS_CLIENT && o->role != CRYPTO_TLS_SERVER) ||
        o->security_profile > CRYPTO_TLS_PROFILE_TLCP11_MODE ||
        o->trust_mode > CRYPTO_TLS_TRUST_SYSTEM ||
        o->client_auth > CRYPTO_TLS_CLIENT_AUTH_REQUIRED ||
        o->verify_depth > 5u)
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;

    if (o->security_profile == CRYPTO_TLS_PROFILE_STANDARD) {
        if ((o->protocol_min != CRYPTO_TLS_1_2 &&
             o->protocol_min != CRYPTO_TLS_1_3) ||
            (o->protocol_max != CRYPTO_TLS_1_2 &&
             o->protocol_max != CRYPTO_TLS_1_3) ||
            o->protocol_min > o->protocol_max)
            return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    } else if (o->security_profile == CRYPTO_TLS_PROFILE_SHANGMI_TLS13) {
        if (o->protocol_min != CRYPTO_TLS_1_3 ||
            o->protocol_max != CRYPTO_TLS_1_3)
            return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    } else if (o->protocol_min != CRYPTO_TLS_PROFILE_TLCP11 ||
               o->protocol_max != CRYPTO_TLS_PROFILE_TLCP11) {
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    }

    if (!text_valid(o->verified_name, CRYPTO_TLS_PROFILE_NAME_MAX_BYTES, 0) ||
        !text_valid(o->sni_name, CRYPTO_TLS_PROFILE_NAME_MAX_BYTES, 0) ||
        !text_valid(o->ca_file, CRYPTO_TLS_PROFILE_PATH_MAX_BYTES, 0) ||
        !text_valid(o->ca_path, CRYPTO_TLS_PROFILE_PATH_MAX_BYTES, 0) ||
        !text_valid(o->certificate_file, CRYPTO_TLS_PROFILE_PATH_MAX_BYTES, 0) ||
        !text_valid(o->private_key_file, CRYPTO_TLS_PROFILE_PATH_MAX_BYTES, 0) ||
        !text_valid(o->private_key_password, CRYPTO_TLS_PROFILE_PASSWORD_MAX_BYTES, 1))
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;

    if (!!o->certificate_file != !!o->private_key_file ||
        (o->private_key_password && !o->private_key_file))
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;

    if (o->role == CRYPTO_TLS_CLIENT) {
        if (!o->verified_name || o->client_auth != CRYPTO_TLS_CLIENT_AUTH_NONE ||
            o->trust_mode == CRYPTO_TLS_TRUST_NONE)
            return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        if (o->security_profile == CRYPTO_TLS_PROFILE_TLCP11_MODE &&
            o->certificate_file)
            return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        if (o->trust_mode == CRYPTO_TLS_TRUST_SYSTEM) {
            if (o->ca_file || o->ca_path)
                return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        } else if (!o->ca_file && !o->ca_path) {
            return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        }
    } else {
        if (o->verified_name || o->sni_name ||
            !o->certificate_file || !o->private_key_file)
            return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        if (o->client_auth == CRYPTO_TLS_CLIENT_AUTH_NONE) {
            if (o->trust_mode != CRYPTO_TLS_TRUST_NONE ||
                o->ca_file || o->ca_path)
                return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        } else {
            if (o->trust_mode != CRYPTO_TLS_TRUST_EXPLICIT ||
                (!o->ca_file && !o->ca_path) ||
                o->security_profile == CRYPTO_TLS_PROFILE_TLCP11_MODE)
                return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        }
    }
    alpn_status = validate_alpn(o, out_wire_size);
    return alpn_status;
}

uint32_t crypto_tls_profile_abi_version(void)
{
    return CRYPTO_TLS_PROFILE_ABI_VERSION;
}

crypto_tls_profile_result crypto_tls_profile_create_v1_4(
    const crypto_tls_profile_options_v1_4 *options,
    crypto_tls_profile **out_profile)
{
    crypto_tls_profile *p;
    crypto_tls_profile_result status;
    size_t wire_size = 0u, cursor = 0u, i;
    if (!out_profile) return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    status = validate_profile(options, &wire_size);
    if (status != CRYPTO_TLS_PROFILE_OK) return status;
    if (wire_size > SIZE_MAX - sizeof(*p))
        return CRYPTO_TLS_PROFILE_RANGE;
    p = (crypto_tls_profile *)calloc(1u, sizeof(*p) + wire_size);
    if (!p) return CRYPTO_TLS_PROFILE_OUT_OF_MEMORY;
    p->role = options->role;
    p->security_profile = options->security_profile;
    p->trust_mode = options->trust_mode;
    p->protocol_min = options->protocol_min;
    p->protocol_max = options->protocol_max;
    p->verify_depth = options->verify_depth;
    p->client_auth = options->client_auth;
    p->alpn_count = options->alpn_count;
    p->alpn_wire_bytes = wire_size;

    text_copy(p->verified_name, options->verified_name,
              CRYPTO_TLS_PROFILE_NAME_MAX_BYTES);
    text_copy(p->sni_name, options->sni_name, CRYPTO_TLS_PROFILE_NAME_MAX_BYTES);
    text_copy(p->ca_file, options->ca_file, CRYPTO_TLS_PROFILE_PATH_MAX_BYTES);
    text_copy(p->ca_path, options->ca_path, CRYPTO_TLS_PROFILE_PATH_MAX_BYTES);
    text_copy(p->certificate_file, options->certificate_file,
              CRYPTO_TLS_PROFILE_PATH_MAX_BYTES);
    text_copy(p->private_key_file, options->private_key_file,
              CRYPTO_TLS_PROFILE_PATH_MAX_BYTES);
    text_copy(p->private_key_password, options->private_key_password,
              CRYPTO_TLS_PROFILE_PASSWORD_MAX_BYTES);
    for (i = 0u; i < options->alpn_count; ++i) {
        const crypto_tls_alpn_label_v1_4 *item = &options->alpn[i];
        p->alpn_wire[cursor++] = (uint8_t)item->size;
        memcpy(p->alpn_wire + cursor, item->data, item->size);
        cursor += item->size;
    }
    *out_profile = p; /* One fully formed owner published exactly once. */
    return CRYPTO_TLS_PROFILE_OK;
}

crypto_tls_profile_result crypto_tls_profile_get_info_v1_4(
    const crypto_tls_profile *profile, crypto_tls_profile_info_v1_4 *out_info)
{
    crypto_tls_profile_info_v1_4 info;
    if (!profile || !out_info || out_info->struct_size != sizeof(*out_info))
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    memset(&info, 0, sizeof(info));
    info.struct_size = (uint32_t)sizeof(info);
    info.role = profile->role;
    info.security_profile = profile->security_profile;
    info.trust_mode = profile->trust_mode;
    info.protocol_min = profile->protocol_min;
    info.protocol_max = profile->protocol_max;
    info.verify_depth = profile->verify_depth;
    info.client_auth = profile->client_auth;
    info.alpn_count = profile->alpn_count;
    info.alpn_wire_bytes = profile->alpn_wire_bytes;
    *out_info = info;
    return CRYPTO_TLS_PROFILE_OK;
}

crypto_tls_profile_result crypto_tls_profile_copy_alpn_wire_v1_4(
    const crypto_tls_profile *profile, uint8_t *out_data,
    size_t capacity, size_t *out_size)
{
    size_t required;
    if (!profile || !out_size || (!out_data && capacity != 0u))
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    required = profile->alpn_wire_bytes;
    if (capacity < required) {
        *out_size = required;
        return CRYPTO_TLS_PROFILE_BUFFER_TOO_SMALL;
    }
    if (required) {
        if (!out_data) return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
        memcpy(out_data, profile->alpn_wire, required);
    }
    *out_size = required;
    return CRYPTO_TLS_PROFILE_OK;
}

crypto_tls_profile_result crypto_tls_profile_copy_name_v1_4(
    const crypto_tls_profile *profile, uint32_t name_kind,
    char *out_data, size_t capacity, size_t *out_size)
{
    const char *name;
    size_t required = 0u;
    if (!profile || !out_size || (!out_data && capacity != 0u))
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    if (name_kind == CRYPTO_TLS_PROFILE_NAME_VERIFIED)
        name = profile->verified_name;
    else if (name_kind == CRYPTO_TLS_PROFILE_NAME_SNI)
        name = profile->sni_name;
    else
        return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    if (name[0] == '\0') return CRYPTO_TLS_PROFILE_ABSENT;
    while (name[required]) ++required;
    ++required; /* Include the NUL byte. */
    if (capacity < required) {
        *out_size = required;
        return CRYPTO_TLS_PROFILE_BUFFER_TOO_SMALL;
    }
    if (!out_data) return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    memcpy(out_data, name, required);
    *out_size = required;
    return CRYPTO_TLS_PROFILE_OK;
}

crypto_tls_profile_result crypto_tls_profile_destroy_v1_4(
    crypto_tls_profile **profile)
{
    crypto_tls_profile *p;
    size_t size;
    if (!profile || !*profile) return CRYPTO_TLS_PROFILE_INVALID_ARGUMENT;
    p = *profile;
    size = sizeof(*p) + p->alpn_wire_bytes;
    *profile = NULL;
    salts_crypto_secure_zero(p, size);
    free(p);
    return CRYPTO_TLS_PROFILE_OK;
}

