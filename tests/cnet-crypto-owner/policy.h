/* SPDX-License-Identifier: Apache-2.0
 * TEST-ONLY CNet policy admission for a future opaque Crypto TLS Owner.
 * This is not a new public CNet ABI and is NOT compiled in production CNet.
 *
 * The entire caller-supplied CNet policy must be representable by one
 * opaque Crypto TLS session before the candidate can be considered. An
 * unsupported capability is rejected, NEVER approximated or silently
 * downgraded to TLS 1.2, a different trust source or an unauthenticated
 * mode. The strings are borrowed only for immediate session_create().
 */
#ifndef SALTSCORE_TEST_CNET_CRYPTO_CANDIDATE_POLICY_H
#define SALTSCORE_TEST_CNET_CRYPTO_CANDIDATE_POLICY_H

#include <cnet/cnet.h>
#include <crypto/tls.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit protocol is mandatory: opaque Crypto's current API does not
 * implement the CNet 1.2..1.3 negotiation-range contract. */
int cnet_crypto_candidate_prepare_client(
    const cnet_tls_client_config *policy,
    const char *effective_verified_hostname, uint32_t protocol,
    crypto_tls_session_options *out_session);

int cnet_crypto_candidate_prepare_server(
    const cnet_tls_server_config *policy, uint32_t protocol,
    crypto_tls_session_options *out_session);

#ifdef __cplusplus
}
#endif
#endif
