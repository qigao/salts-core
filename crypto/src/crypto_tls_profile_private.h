/* Private immutable v1.4 TLS policy representation. NOT installed SDK. */
#ifndef CRYPTO_TLS_PROFILE_PRIVATE_H_INCLUDED
#define CRYPTO_TLS_PROFILE_PRIVATE_H_INCLUDED
#include <crypto/tls_profile.h>
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
#endif
