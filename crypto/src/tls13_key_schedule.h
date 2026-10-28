/* SPDX-License-Identifier: Apache-2.0
 * Private, provider-free TLS 1.3 SHA-256/SHA-384 key-schedule primitives.
 * This is NOT a handshake engine, TLS backend or a production fallback.
 * No GmSSL, OpenSSL, heap-owned session, or platform runtime state.
 */
#ifndef CRYPTO_NATIVE_TLS13_KEY_SCHEDULE_H_INCLUDED
#define CRYPTO_NATIVE_TLS13_KEY_SCHEDULE_H_INCLUDED
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum crypto_native_tls13_hash {
    CRYPTO_NATIVE_TLS13_SHA256 = 1,
    CRYPTO_NATIVE_TLS13_SHA384 = 2
} crypto_native_tls13_hash;

typedef enum crypto_native_tls13_result {
    CRYPTO_NATIVE_TLS13_OK = 0,
    CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT = -1,
    CRYPTO_NATIVE_TLS13_RANGE = -2,
    CRYPTO_NATIVE_TLS13_CRYPTO_ERROR = -3,
    CRYPTO_NATIVE_TLS13_OUT_OF_MEMORY = -4
} crypto_native_tls13_result;

/* RFC 5869 HKDF-Extract. NULL salt with zero size means HashLen zero bytes.
 * NULL ikm is allowed iff ikm_len == 0. On all errors the caller's PRK
 * is unchanged. out_capacity must be at least the selected HashLen.
 * Returns exactly 32 or 48 bytes on success.
 */
crypto_native_tls13_result crypto_native_tls13_hkdf_extract(
    crypto_native_tls13_hash hash, const uint8_t *salt, size_t salt_len,
    const uint8_t *ikm, size_t ikm_len, uint8_t *out_prk,
    size_t out_capacity);

/* RFC 8446 section 7.1 HKDF-Expand-Label:
 * HkdfLabel = uint16 length || byte len("tls13 " + label) ||
 * "tls13 " + label || byte context_len || context.
 * Input secret MUST be exactly HashLen bytes. RFC limits label to
 * 249 bytes, context <=255 bytes, output <=255*HashLen and <=65535.
 * Label is bounded printable ASCII with no NUL. Empty label is legal.
 * All input buffers are borrowed and may overlap output.
 * Output remains unchanged on any error, and output_len=0 is legal.
 */
crypto_native_tls13_result crypto_native_tls13_hkdf_expand_label(
    crypto_native_tls13_hash hash, const uint8_t *secret, size_t secret_len,
    const uint8_t *label, size_t label_len,
    const uint8_t *context, size_t context_len,
    uint8_t *output, size_t output_len);

#ifdef __cplusplus
}
#endif
#endif
