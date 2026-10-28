/* SPDX-License-Identifier: Apache-2.0
 * Native FIPS 180-4 SHA-384/SHA-512 in the one Salts::Crypto archive.
 * No GmSSL, OpenSSL, libecc or alternate/fallback hash implementation.
 */
#ifndef CRYPTO_SHA512_H_INCLUDED
#define CRYPTO_SHA512_H_INCLUDED
#include <crypto/status.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SALTS_CRYPTO_SHA384_DIGEST_SIZE 48u
#define SALTS_CRYPTO_SHA512_DIGEST_SIZE 64u
#define SALTS_CRYPTO_SHA512_CONTEXT_SIZE 256u
/* Caller-owned, no heap; not thread-safe; don't copy while live.
 * SHA384 and SHA512 use distinct checked state markers and cannot be mixed.
 * final() permanently retires the context; invalid calls preserve output.
 */
typedef union salts_crypto_sha512_ctx_u {
    void *pointer_alignment;
    uint64_t integer_alignment;
    long double floating_alignment;
    uint8_t bytes[SALTS_CRYPTO_SHA512_CONTEXT_SIZE];
} salts_crypto_sha512_ctx_t;
typedef salts_crypto_sha512_ctx_t salts_crypto_sha384_ctx_t;
int salts_crypto_sha384_init(salts_crypto_sha384_ctx_t *context);
int salts_crypto_sha384_update(salts_crypto_sha384_ctx_t *context,
                               const void *data, size_t data_size);
int salts_crypto_sha384_final(salts_crypto_sha384_ctx_t *context,
                              uint8_t digest[SALTS_CRYPTO_SHA384_DIGEST_SIZE]);
int salts_crypto_sha384(const void *data, size_t data_size,
                        uint8_t digest[SALTS_CRYPTO_SHA384_DIGEST_SIZE]);
int salts_crypto_sha512_init(salts_crypto_sha512_ctx_t *context);
int salts_crypto_sha512_update(salts_crypto_sha512_ctx_t *context,
                               const void *data, size_t data_size);
int salts_crypto_sha512_final(salts_crypto_sha512_ctx_t *context,
                              uint8_t digest[SALTS_CRYPTO_SHA512_DIGEST_SIZE]);
int salts_crypto_sha512(const void *data, size_t data_size,
                        uint8_t digest[SALTS_CRYPTO_SHA512_DIGEST_SIZE]);
#ifdef __cplusplus
}
#endif
#endif
