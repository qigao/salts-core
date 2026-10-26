/* SPDX-License-Identifier: Apache-2.0
 * Qualification-only alternate CNet TLS state implementation.
 * NOT installed or linked by production SaltsCore::CNet.
 *
 * Exact-wire-version selection and CNet policy admission precede context
 * creation; no default TLS1.2/1.3 range, trust fallback or GmSSL types.
 */
#ifndef CNET_TLS_CRYPTO_QUALIFICATION_H
#define CNET_TLS_CRYPTO_QUALIFICATION_H

#include "cnet_tls.h"
#include <crypto/tls.h>
#include <crypto/cipher_bio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Each context borrows the given provider for its entire life. Its owner
 * must outlive every session, context and NativeIO terminal. On success,
 * cnet_tls_state_init consumes the caller's context reference exactly as
 * the canonical legacy CNet implementation does.
 */
int cnet_crypto_tls_test_client_context(
    crypto_tls_provider *provider, const cnet_tls_client_config *policy,
    const char *effective_dns_name, uint32_t exact_protocol,
    size_t cipher_capacity, cnet_tls_context **out_context);
int cnet_crypto_tls_test_server_context(
    crypto_tls_provider *provider, const cnet_tls_server_config *policy,
    uint32_t exact_protocol, size_t cipher_capacity,
    cnet_tls_context **out_context);

/* A private test helper for explicitly simulating transport EOF without
 * forging TLS close_notify. Once the buffered BIO ciphertext drains,
 * provider CLOSED is treated as unauthenticated transport shutdown, not
 * as CNet peer_close_notify.
 */
int cnet_crypto_tls_test_transport_eof(cnet_tls_state *state);

#ifdef __cplusplus
}
#endif
#endif
