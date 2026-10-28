/* SPDX-License-Identifier: Apache-2.0
 * The ONE production Salts::Crypto static TLS engine's v1.4 adapter.
 * This file is NOT built in standalone provider-free policy tests.
 * No second TLS backend, provider loader, fallback or separate library.
 */
#include "crypto_tls_profile_private.h"
#include "crypto_internal.h"
#include <crypto/tls_ext.h>
#include <string.h>

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
