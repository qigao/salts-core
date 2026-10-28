/* SPDX-License-Identifier: Apache-2.0
 * CNet RFC 5929 certificate-binding digest policy belongs to Salts::Crypto.
 * The DER signature OID is inspected with first-party Salts::ASN1; the
 * current canonical Crypto archive performs digest algorithms privately.
 * This is NOT a second TLS Provider or a third-party-free TLS engine.
 */
#include <crypto/x509_binding.h>
#include <crypto/x509_der.h>
#include <gmssl/digest.h>
#include <string.h>

static int oid_matches(const crypto_x509_der_view *cert,
                       const uint8_t *oid, size_t length)
{
    return cert->signature_algorithm_oid_size == length &&
        memcmp(cert->signature_algorithm_oid, oid, length) == 0;
}

static const DIGEST *binding_digest(const crypto_x509_der_view *cert)
{
    static const uint8_t rsa_prefix[] = {
        0x2au, 0x86u, 0x48u, 0x86u, 0xf7u, 0x0du, 0x01u, 0x01u
    };
    static const uint8_t ec_sha1[] = {
        0x2au, 0x86u, 0x48u, 0xceu, 0x3du, 0x04u, 0x01u
    };
    static const uint8_t ec_sha2_prefix[] = {
        0x2au, 0x86u, 0x48u, 0xceu, 0x3du, 0x04u, 0x03u
    };
    static const uint8_t sm2_sign_sm3[] = {
        0x2au, 0x81u, 0x1cu, 0xcfu, 0x55u, 0x01u, 0x83u, 0x75u
    };
    static const uint8_t rsa_sign_sm3[] = {
        0x2au, 0x81u, 0x1cu, 0xcfu, 0x55u, 0x01u, 0x83u, 0x78u
    };
    const uint8_t *oid = cert->signature_algorithm_oid;
    const size_t length = cert->signature_algorithm_oid_size;
    if (length == sizeof(rsa_prefix) + 1u &&
        memcmp(oid, rsa_prefix, sizeof(rsa_prefix)) == 0) {
        switch (oid[sizeof(rsa_prefix)]) {
        case 4u: /* md5WithRSAEncryption: RFC 5929 substitutes SHA256. */
        case 5u: /* sha1WithRSAEncryption: RFC 5929 substitutes SHA256. */
        case 11u: return DIGEST_sha256();
        case 14u: return DIGEST_sha224();
        case 12u: return DIGEST_sha384();
        case 13u: return DIGEST_sha512();
        default: return NULL;
        }
    }
    if (oid_matches(cert, ec_sha1, sizeof(ec_sha1)))
        return DIGEST_sha256();
    if (length == sizeof(ec_sha2_prefix) + 1u &&
        memcmp(oid, ec_sha2_prefix, sizeof(ec_sha2_prefix)) == 0) {
        switch (oid[sizeof(ec_sha2_prefix)]) {
        case 1u: return DIGEST_sha224();
        case 2u: return DIGEST_sha256();
        case 3u: return DIGEST_sha384();
        case 4u: return DIGEST_sha512();
        default: return NULL;
        }
    }
    if (oid_matches(cert, sm2_sign_sm3, sizeof(sm2_sign_sm3)) ||
        oid_matches(cert, rsa_sign_sm3, sizeof(rsa_sign_sm3)))
        return DIGEST_sm3();
    return NULL;
}

crypto_x509_binding_status crypto_x509_der_server_end_point_binding(
    const uint8_t *certificate, size_t certificate_size,
    uint8_t *output, size_t capacity, size_t *out_size)
{
    crypto_x509_der_view view = {0};
    const DIGEST *selected;
    uint8_t calculated[DIGEST_MAX_SIZE] = {0};
    size_t bytes = 0u;
    size_t required;

    if (!certificate || !certificate_size || !output || !capacity || !out_size)
        return CRYPTO_X509_BINDING_INVALID_ARGUMENT;
    if (crypto_x509_der_inspect(certificate, certificate_size, &view)
        != SALTS_ASN1_OK)
        return CRYPTO_X509_BINDING_INVALID_DER;
    selected = binding_digest(&view);
    if (!selected) return CRYPTO_X509_BINDING_UNSUPPORTED_ALGORITHM;
    required = selected->digest_size;
    if (required == 0u || required > CRYPTO_X509_BINDING_MAX_BYTES ||
        required > sizeof(calculated))
        return CRYPTO_X509_BINDING_DIGEST_FAILURE;
    if (capacity < required) {
        *out_size = required;
        return CRYPTO_X509_BINDING_BUFFER_TOO_SMALL;
    }
    if (digest(selected, certificate, certificate_size,
               calculated, &bytes) != 1 || bytes != required) {
        memset(calculated, 0, sizeof(calculated));
        return CRYPTO_X509_BINDING_DIGEST_FAILURE;
    }
    memcpy(output, calculated, required);
    *out_size = required;
    memset(calculated, 0, sizeof(calculated));
    return CRYPTO_X509_BINDING_OK;
}
