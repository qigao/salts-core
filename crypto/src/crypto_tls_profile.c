/* SPDX-License-Identifier: Apache-2.0
 * First-party, bounded and fail-closed TLS policy ownership for CNet.
 * Sole Salts::Crypto STATIC owner; this file has no TLS handshake, record,
 * crypto-provider loader, socket access or fallback implementation.
 */
#include <crypto/tls_profile.h>
#include <crypto/tls_ext.h>
#include "crypto_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct crypto_tls_profile {
    uint32_t role;
    uint32_t security_profile;
    uint32_t trust_mode;
    uint32_t protocol_min;
    uint32_t protocol_max;
    uint32_t verify_depth;
    uint32_t client_auth;
    size_t alpn_count;
    size_t alpn_wire_bytes;
    char verified_name[CRYPTO_TLS_PROFILE_NAME_MAX_BYTES + 1u];
    char sni_name[CRYPTO_TLS_PROFILE_NAME_MAX_BYTES + 1u];
    char ca_file[CRYPTO_TLS_PROFILE_PATH_MAX_BYTES + 1u];
    char ca_path[CRYPTO_TLS_PROFILE_PATH_MAX_BYTES + 1u];
    char certificate_file[CRYPTO_TLS_PROFILE_PATH_MAX_BYTES + 1u];
    char private_key_file[CRYPTO_TLS_PROFILE_PATH_MAX_BYTES + 1u];
    char private_key_password[CRYPTO_TLS_PROFILE_PASSWORD_MAX_BYTES + 1u];
    uint8_t alpn_wire[]; /* immutable length-prefixed, owned, max 65535 */
};

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


/*
 * Live Crypto v1.4 session admission against the ONE existing GmSSL-backed
 * engine. This is a deliberately bounded subset; accepting all v1.4 policy
 * shapes would falsely promise system trust, IP SAN, TLCP, ShangMi, version
 * negotiation or binary ALPN which the current engine cannot implement.
 * Such profiles return UNSUPPORTED before calling any TLS constructor.
 */
static int is_ascii_dns_name(const char *name)
{
    size_t label_length = 0u, length = 0u;
    int found_alpha = 0;
    if (!name || !name[0]) return 0;
    while (name[length]) {
        const unsigned char ch = (unsigned char)name[length];
        const int alpha = (ch >= 'A' && ch <= 'Z') ||
                          (ch >= 'a' && ch <= 'z');
        const int digit = ch >= '0' && ch <= '9';
        if (ch == '.') {
            if (label_length == 0u || name[length - 1u] == '-')
                return 0;
            label_length = 0u;
        } else {
            if (!(alpha || digit || ch == '-') ||
                (ch == '-' && label_length == 0u) ||
                ++label_length > 63u)
                return 0;
            if (alpha) found_alpha = 1;
        }
        if (++length > 253u) return 0;
    }
    if (!label_length || name[length - 1u] == '-') return 0;
    /* A numeric or colon-containing host may require IP SAN verification.
     * Never treat an IP address as a DNS identity for the old provider.
     */
    return found_alpha;
}

static int profile_alpn_legacy_text(
    const crypto_tls_profile *profile,
    char labels[CRYPTO_TLS_ALPN_MAX_PROTOCOLS]
               [CRYPTO_TLS_ALPN_MAX_NAME_BYTES + 1u],
    const char *names[CRYPTO_TLS_ALPN_MAX_PROTOCOLS])
{
    size_t cursor = 0u;
    if (profile->alpn_count > CRYPTO_TLS_ALPN_MAX_PROTOCOLS ||
        profile->alpn_wire_bytes > 128u)
        return 0;
    for (size_t i = 0u; i < profile->alpn_count; ++i) {
        size_t len;
        if (cursor >= profile->alpn_wire_bytes) return 0;
        len = profile->alpn_wire[cursor++];
        if (len == 0u || len > CRYPTO_TLS_ALPN_MAX_NAME_BYTES ||
            len > profile->alpn_wire_bytes - cursor)
            return 0;
        for (size_t j = 0u; j < len; ++j) {
            uint8_t ch = profile->alpn_wire[cursor + j];
            if (ch < 0x21u || ch > 0x7eu) return 0;
            labels[i][j] = (char)ch;
        }
        labels[i][len] = '\0';
        names[i] = labels[i];
        cursor += len;
    }
    return cursor == profile->alpn_wire_bytes;
}

crypto_tls_result crypto_tls_session_create_profile_v1_4(
    crypto_tls_provider *provider, const crypto_tls_profile *profile,
    const crypto_tls_io *io, crypto_tls_session **out_session)
{
    crypto_tls_session_options options = {0};
    crypto_tls_auth_options_v1_3 auth = {0};
    crypto_tls_session *candidate = NULL;
    crypto_tls_result status;
    const crypto_tls_auth_options_v1_3 *auth_ptr = NULL;
    char labels[CRYPTO_TLS_ALPN_MAX_PROTOCOLS]
               [CRYPTO_TLS_ALPN_MAX_NAME_BYTES + 1u] = {{0}};
    const char *names[CRYPTO_TLS_ALPN_MAX_PROTOCOLS] = {0};

    /* Never erase an existing caller-owned session, even on success. */
    if (!provider || !profile || !io || !out_session || *out_session)
        return CRYPTO_TLS_INVALID_ARGUMENT;

    if (profile->security_profile != CRYPTO_TLS_PROFILE_STANDARD ||
        profile->protocol_min != profile->protocol_max ||
        (profile->protocol_min != CRYPTO_TLS_1_2 &&
         profile->protocol_min != CRYPTO_TLS_1_3))
        return CRYPTO_TLS_PROFILE_UNSUPPORTED;

    if (!profile_alpn_legacy_text(profile, labels, names))
        return CRYPTO_TLS_PROFILE_UNSUPPORTED;

    if (profile->role == CRYPTO_TLS_CLIENT) {
        if (profile->trust_mode != CRYPTO_TLS_TRUST_EXPLICIT ||
            profile->ca_file[0] == '\0' ||
            profile->ca_path[0] != '\0' ||
            !is_ascii_dns_name(profile->verified_name) ||
            (profile->sni_name[0] != '\0' &&
             strcmp(profile->sni_name, profile->verified_name) != 0))
            return CRYPTO_TLS_PROFILE_UNSUPPORTED;
    } else {
        if (profile->client_auth == CRYPTO_TLS_CLIENT_AUTH_REQUIRED &&
            (profile->trust_mode != CRYPTO_TLS_TRUST_EXPLICIT ||
             profile->ca_file[0] == '\0' || profile->ca_path[0] != '\0'))
            return CRYPTO_TLS_PROFILE_UNSUPPORTED;
    }

    options.struct_size = (uint32_t)sizeof(options);
    options.protocol = profile->protocol_min;
    options.role = profile->role;
    options.verify_depth = profile->verify_depth;
    if (profile->role == CRYPTO_TLS_CLIENT) {
        options.hostname = profile->verified_name;
        options.ca_file = profile->ca_file;
        if (profile->certificate_file[0]) {
            auth.struct_size = (uint32_t)sizeof(auth);
            auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_NONE;
            auth.client_certificate_file = profile->certificate_file;
            auth.client_private_key_file = profile->private_key_file;
            auth.client_private_key_password =
                profile->private_key_password[0]
                    ? profile->private_key_password : NULL;
            auth_ptr = &auth;
        }
    } else {
        options.certificate_file = profile->certificate_file;
        options.private_key_file = profile->private_key_file;
        options.private_key_password =
            profile->private_key_password[0]
                ? profile->private_key_password : NULL;
        if (profile->client_auth == CRYPTO_TLS_CLIENT_AUTH_REQUIRED) {
            auth.struct_size = (uint32_t)sizeof(auth);
            auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
            auth.client_ca_file = profile->ca_file;
            auth_ptr = &auth;
        }
    }

    if (profile->alpn_count &&
        crypto_tls_extension_abi_version() !=
            CRYPTO_TLS_EXTENSION_ABI_VERSION)
        return CRYPTO_TLS_PROFILE_UNSUPPORTED;

    /* A local candidate prevents v1 constructor error behavior from
     * changing the caller's output on an unsuccessful create.
     */
    status = auth_ptr
        ? crypto_tls_session_create_auth_v1_3(
            provider, &options, auth_ptr, io, &candidate)
        : crypto_tls_session_create(provider, &options, io, &candidate);
    if (status != CRYPTO_TLS_OK) return status;
    if (!candidate) return CRYPTO_TLS_BACKEND_ERROR;
    if (profile->alpn_count) {
        status = crypto_tls_session_set_alpn_v1_1(
            candidate, names, profile->alpn_count);
        if (status != CRYPTO_TLS_OK) {
            /* No I/O or handshake has begun, so retirement is quiescent. */
            (void)crypto_tls_session_destroy(&candidate);
            return status;
        }
    }
    *out_session = candidate;
    salts_crypto_secure_zero(labels, sizeof(labels));
    return CRYPTO_TLS_OK;
}
