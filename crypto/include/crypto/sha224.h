/* SPDX-License-Identifier: Apache-2.0
 * Additive FIPS 180-4 SHA-224 from the ONE native Salts::Crypto archive.
 * Shares the canonical SHA-256 compression with a different IV/truncation.
 * No GmSSL, OpenSSL, libecc or runtime provider fallback.
 */
#ifndef CRYPTO_SHA224_H_INCLUDED
#define CRYPTO_SHA224_H_INCLUDED
#include <crypto/status.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SALTS_CRYPTO_SHA224_DIGEST_SIZE 28u
/* NULL data is valid iff size is zero. Invalid arguments never modify
 * output; errors after an accepted argument clear the 28 output bytes. */
int salts_crypto_sha224(const void *data, size_t data_size,
                        uint8_t digest[SALTS_CRYPTO_SHA224_DIGEST_SIZE]);
#ifdef __cplusplus
}
#endif
#endif
