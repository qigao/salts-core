/* SPDX-License-Identifier: Apache-2.0
 * Additive TLS v1.2 read-only peer certificate and exporter interface.
 *
 * This is an interface epoch, not a TLS wire protocol version:
 * old TLS 1.0 and TLS 1.1 entrypoints, structures and ABI markers
 * remain unchanged. No GmSSL or certificate parser types are public.
 */
#ifndef CRYPTO_TLS_PEER_H_INCLUDED
#define CRYPTO_TLS_PEER_H_INCLUDED

#include <crypto/tls.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTO_TLS_PEER_ABI_VERSION UINT32_C(0x00010002)
#define CRYPTO_TLS_PEER_CHAIN_MAX_BYTES 4096u
#define CRYPTO_TLS_PEER_SHA256_BYTES 32u
#define CRYPTO_TLS_EXPORTER_MAX_BYTES 64u
#define CRYPTO_TLS_EXPORTER_MAX_LABEL_BYTES 127u
#define CRYPTO_TLS_EXPORTER_MAX_CONTEXT_BYTES 1024u

/* Extension-only results without renumbering the original v1 error enum. */
#define CRYPTO_TLS_PEER_CERT_ABSENT ((crypto_tls_result)-7)
#define CRYPTO_TLS_PEER_BUFFER_TOO_SMALL ((crypto_tls_result)-8)

#define CRYPTO_TLS_PEER_FEATURE_DER_CHAIN_COPY  (UINT64_C(1) << 0)
#define CRYPTO_TLS_PEER_FEATURE_CERT_SHA256     (UINT64_C(1) << 1)
#define CRYPTO_TLS_PEER_FEATURE_TLS_EXPORTER    (UINT64_C(1) << 2)

CRYPTO_TLS_API uint32_t crypto_tls_peer_abi_version(void);

/* Report only implemented features; no certificate or trust fallback. */
CRYPTO_TLS_API crypto_tls_result crypto_tls_peer_features_v1_2(
    uint64_t *out_features);

/* Copy the verified-handshake peer certificate chain, exactly as GmSSL
 * internally stores its concatenated DER certificates (NOT a TLS record,
 * not an array of pointers, and not a platform trust decision).
 *
 * out=NULL, capacity=0 is a size query: returns OK and sets *out_size to
 * the needed bytes, without returning an internal pointer.
 * Insufficient non-null capacity returns BUFFER_TOO_SMALL and updates
 * *out_size, but leaves all bytes of out unchanged.
 * On other errors both out and *out_size remain unchanged.
 * A server with no peer certificate returns PEER_CERT_ABSENT.
 * Maximum payload is 4096 bytes; larger chains fail closed.
 * Caller-owned concatenated DER may be inspected by
 * crypto_x509_der_chain_next() after this copy. Structural inspection is
 * NEVER TLS certificate trust, signature or hostname verification.
 */
CRYPTO_TLS_API crypto_tls_result crypto_tls_session_copy_peer_chain_v1_2(
    crypto_tls_session *session, uint8_t *out, size_t capacity,
    size_t *out_size);

/* Raw binary SHA-256 of the first DER peer certificate, NOT the RFC 5929
 * tls-server-end-point digest (whose algorithm depends on cert signature).
 * Caller owns exactly 32 output bytes. All errors leave output unchanged.
 */
CRYPTO_TLS_API crypto_tls_result crypto_tls_session_peer_sha256_v1_2(
    crypto_tls_session *session,
    uint8_t out[CRYPTO_TLS_PEER_SHA256_BYTES]);

/* RFC 5705 (TLS 1.2) / RFC 8446 (TLS 1.3) exporter derived from the
 * authenticated TLS session. Context flag must be 0 or 1; NULL context is
 * allowed only when context length is 0. Label is printable ASCII, 1..127
 * bytes (NUL terminated). Output length must be 1..64 bytes.
 * All failures leave caller-owned output bytes unchanged. Only the real
 * private GmSSL exporter may perform the key schedule; no OpenSSL fallback.
 *
 * CNet RFC 9266 channel binding is:
 *   label="EXPORTER-Channel-Binding", context=NULL, length=0,
 *   use_context=1, output_len=32.
 * The exporter does not itself guarantee client-certificate authentication.
 */
CRYPTO_TLS_API crypto_tls_result crypto_tls_session_export_v1_2(
    crypto_tls_session *session,
    const char *label,
    const uint8_t *context, size_t context_len, uint32_t use_context,
    uint8_t *out, size_t out_len);

#ifdef __cplusplus
}
#endif
#endif
