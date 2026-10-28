/* SPDX-License-Identifier: Apache-2.0
 * RFC 5929 tls-server-end-point certificate digest, within ONE
 * Salts::Crypto static algorithm owner. No TLS state, trust, hostname
 * verification or third-party types may escape this interface.
 */
#ifndef CRYPTO_X509_BINDING_H_INCLUDED
#define CRYPTO_X509_BINDING_H_INCLUDED
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTO_X509_BINDING_MAX_BYTES 64u

typedef enum crypto_x509_binding_status {
    CRYPTO_X509_BINDING_OK = 0,
    CRYPTO_X509_BINDING_INVALID_ARGUMENT = -1,
    CRYPTO_X509_BINDING_INVALID_DER = -2,
    CRYPTO_X509_BINDING_UNSUPPORTED_ALGORITHM = -3,
    CRYPTO_X509_BINDING_BUFFER_TOO_SMALL = -4,
    CRYPTO_X509_BINDING_DIGEST_FAILURE = -5
} crypto_x509_binding_status;

/* Hash the COMPLETE validated DER certificate, selecting the digest using
 * the certificate SignatureAlgorithmIdentifier per RFC 5929 section 4.1.
 * MD5/SHA1-signed certificates use SHA-256, not MD5/SHA1. Original CNet
 * also supports SHA224/256/384/512 and SM3 signature digest mappings.
 *
 * This is NOT X.509 authentication; the TLS session must already have
 * validated the peer before its certificate is used for channel binding.
 * Output and *out_size remain unchanged on any error, except that
 * BUFFER_TOO_SMALL reports the exact required length via *out_size.
 * No dynamic allocation, no implicit algorithm downgrade or fallback.
 */
crypto_x509_binding_status crypto_x509_der_server_end_point_binding(
    const uint8_t *certificate, size_t certificate_size,
    uint8_t *output, size_t capacity, size_t *out_size);

#ifdef __cplusplus
}
#endif
#endif
