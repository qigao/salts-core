/* SPDX-License-Identifier: Apache-2.0
 * Additive bounded TLS client-certificate and server peer-auth contract.
 * No backend handles, platform sockets or OpenSSL types cross this API.
 * This interface never changes crypto_tls_session_options (v1.0 ABI).
 */
#ifndef CRYPTO_TLS_AUTH_H_INCLUDED
#define CRYPTO_TLS_AUTH_H_INCLUDED

#include <crypto/tls.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTO_TLS_AUTH_ABI_VERSION UINT32_C(0x00010003)
#define CRYPTO_TLS_AUTH_MAX_PATH_BYTES 1024u
#define CRYPTO_TLS_AUTH_MAX_PASSWORD_BYTES 256u

/* NONE and REQUIRED are intentionally the only supported policies.
 * OPTIONAL is not silently downgraded to NONE or REQUIRED.
 */
#define CRYPTO_TLS_CLIENT_AUTH_NONE UINT32_C(0)
#define CRYPTO_TLS_CLIENT_AUTH_REQUIRED UINT32_C(1)

/* A fatal peer-auth failure is terminal for this session. The caller must
 * destroy it; it cannot send, receive or export keying material.
 * A transport/protocol/backend failure remains CRYPTO_TLS_BACKEND_ERROR.
 */
#define CRYPTO_TLS_AUTH_REJECTED ((crypto_tls_result)-9)

/* struct_size MUST equal sizeof(crypto_tls_auth_options_v1_3), all reserved
 * fields MUST be zero. All paths and passwords are NUL-terminated and
 * bounded; their storage is borrowed only for the create call.
 *
 * CLIENT:
 *   client_auth MUST be NONE, client_ca_file MUST be NULL.
 *   client_certificate_file and client_private_key_file MUST both be set
 *   or both be NULL; password requires both. Legacy v1.0 options must NOT
 *   also specify certificate_file/private_key_file/private_key_password.
 *   The original options.ca_file remains the explicit SERVER trust anchor.
 *
 * SERVER:
 *   Identity remains in options.certificate_file/private_key_file.
 *   REQUIRED needs a nonempty client_ca_file to verify client chains,
 *   enforce client-auth EKU and demand CertificateVerify. The legacy
 *   options.ca_file MUST be NULL to avoid conflicting trust authorities.
 *   All client-identity fields MUST be NULL. NONE forbids client_ca_file.
 *
 * This v1.3 API accepts ONLY TLS 1.2 / TLS 1.3 standard profiles.
 * TLCP/dual SM2 identities, optional client auth, implicit system CA and
 * anonymous or PSK modes are unsupported, never emulated or downgraded.
 */
typedef struct crypto_tls_auth_options_v1_3 {
    uint32_t struct_size;
    uint32_t client_auth;
    const char *client_certificate_file;
    const char *client_private_key_file;
    const char *client_private_key_password;
    const char *client_ca_file;
    uint32_t reserved_flags;
    uint32_t reserved;
} crypto_tls_auth_options_v1_3;

CRYPTO_TLS_API uint32_t crypto_tls_auth_abi_version(void);

/* New additive constructor. On every error *out_session is UNCHANGED.
 * On success the caller owns one session and must quiescently destroy it.
 * The legacy constructor and original ABI remain unchanged.
 */
CRYPTO_TLS_API crypto_tls_result crypto_tls_session_create_auth_v1_3(
    crypto_tls_provider *provider,
    const crypto_tls_session_options *options,
    const crypto_tls_auth_options_v1_3 *auth,
    const crypto_tls_io *io,
    crypto_tls_session **out_session);

/* Server-only REQUIRED mode. Returns OK and writes 1 ONLY after a real
 * verified peer chain and CertificateVerify completed. On an incomplete or
 * rejected handshake, invalid role/policy, BUSY or cancellation, the
 * caller-owned output is unchanged. This is NOT an anonymous TLS flag.
 */
CRYPTO_TLS_API crypto_tls_result crypto_tls_session_client_authenticated_v1_3(
    crypto_tls_session *session, uint32_t *out_authenticated);

#ifdef __cplusplus
}
#endif
#endif /* CRYPTO_TLS_AUTH_H_INCLUDED */
