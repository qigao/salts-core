/* SPDX-License-Identifier: Apache-2.0
 * Private qualification-only capability mapper.
 *
 * No GmSSL dependency, no Crypto Provider owner and no socket are
 * constructed here. An accepted descriptor is still only a candidate:
 * production CNet lifetime, ALPN, trust and peer metadata conformance
 * remain unqualified.
 */
#include "policy.h"

#include <salts/error_codes.h>
#include <string.h>

enum { TEST_PATH_LIMIT = 4096u, TEST_HOST_LIMIT = 254u };

static int bounded_nonempty(const char *s, size_t capacity)
{
    size_t i;
    if (s == NULL || s[0] == '\0') return 0;
    for (i = 0u; i < capacity; ++i)
        if (s[i] == '\0') return 1;
    return 0;
}

static int bounded_optional(const char *s, size_t capacity)
{
    size_t i;
    if (s == NULL) return 1;
    for (i = 0u; i < capacity; ++i)
        if (s[i] == '\0') return 1;
    return 0;
}

static int protocol_supported(uint32_t protocol)
{
    return protocol == (uint32_t)CRYPTO_TLS_1_2
        || protocol == (uint32_t)CRYPTO_TLS_1_3;
}

int cnet_crypto_candidate_prepare_client(
    const cnet_tls_client_config *policy,
    const char *effective_verified_hostname, uint32_t protocol,
    crypto_tls_session_options *out_session)
{
    crypto_tls_session_options prepared = {0};
    if (out_session == NULL) return SALTS_EINVAL;
    memset(out_session, 0, sizeof(*out_session));
    if (policy == NULL || policy->size != sizeof(*policy)
        || !bounded_nonempty(effective_verified_hostname, TEST_HOST_LIMIT))
        return SALTS_EINVAL;
    if (!protocol_supported(protocol)) return SALTS_ENOTSUP;

    /* CNet accepts system trust, hashed CA directories, client certs,
     * ALPN, ShangMi and TLCP. Crypto's current opaque ABI does not
     * promise those features. None may be silently omitted. */
    if (policy->security_profile != CNET_TLS_SECURITY_PROFILE_STANDARD
        || policy->ca_path != NULL
        || policy->cert_file != NULL
        || policy->key_file != NULL
        || policy->key_password != NULL
        || policy->alpn_protocols != NULL
        || policy->alpn_protocol_count != 0u)
        return SALTS_ENOTSUP;
    if (!bounded_nonempty(policy->ca_file, TEST_PATH_LIMIT))
        return SALTS_ENOTSUP; /* never infer a system CA store */
    if (policy->server_name != NULL &&
        (!bounded_nonempty(policy->server_name, TEST_HOST_LIMIT)
         || strcmp(policy->server_name, effective_verified_hostname) != 0))
        return SALTS_ENOTSUP; /* identity override is not silently dropped */

    prepared.struct_size = (uint32_t)sizeof(prepared);
    prepared.protocol = protocol;
    prepared.role = CRYPTO_TLS_CLIENT;
    prepared.verify_depth = 4u;
    prepared.hostname = effective_verified_hostname;
    prepared.ca_file = policy->ca_file;
    /* This is a borrowed descriptor: no session is created here. */
    *out_session = prepared;
    return SALTS_OK;
}

int cnet_crypto_candidate_prepare_server(
    const cnet_tls_server_config *policy, uint32_t protocol,
    crypto_tls_session_options *out_session)
{
    crypto_tls_session_options prepared = {0};
    if (out_session == NULL) return SALTS_EINVAL;
    memset(out_session, 0, sizeof(*out_session));
    if (policy == NULL || policy->size != sizeof(*policy))
        return SALTS_EINVAL;
    if (!protocol_supported(protocol)) return SALTS_ENOTSUP;

    if (policy->security_profile != CNET_TLS_SECURITY_PROFILE_STANDARD
        || policy->client_auth != CNET_TLS_CLIENT_AUTH_NONE
        || policy->ca_file != NULL || policy->ca_path != NULL
        || policy->alpn_protocols != NULL
        || policy->alpn_protocol_count != 0u)
        return SALTS_ENOTSUP;
    if (!bounded_nonempty(policy->cert_file, TEST_PATH_LIMIT)
        || !bounded_nonempty(policy->key_file, TEST_PATH_LIMIT)
        || !bounded_optional(policy->key_password, TEST_PATH_LIMIT))
        return SALTS_EINVAL;

    prepared.struct_size = (uint32_t)sizeof(prepared);
    prepared.protocol = protocol;
    prepared.role = CRYPTO_TLS_SERVER;
    prepared.verify_depth = 4u;
    prepared.certificate_file = policy->cert_file;
    prepared.private_key_file = policy->key_file;
    prepared.private_key_password = policy->key_password;
    *out_session = prepared;
    return SALTS_OK;
}
