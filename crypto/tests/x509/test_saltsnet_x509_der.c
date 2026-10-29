/* SPDX-License-Identifier: Apache-2.0
 * Original SaltsNet X.509 DER fixture migrated to sole Crypto owner.
 * Not a trust, signature, hostname or lifetime authorization test.
 */
#include <crypto/x509_der.h>
#include "fixtures/saltsnet_original_test_cert_data.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: fail: %s\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

int main(void) {
    crypto_x509_der_view v, unchanged;
    crypto_x509_der_public_key_kind kind = CRYPTO_X509_DER_KEY_UNKNOWN;
    const uint8_t *name = NULL, *issuer = NULL;
    size_t size = 0u, issuer_size = 0u, cursor = 0u;
    uint8_t extra[sizeof(test_cert_der) + 1u];
    static const uint8_t common_name[] = "test.example.com";
    CHECK(test_cert_der_len == 951u);
    CHECK(crypto_x509_der_inspect(test_cert_der, test_cert_der_len, &v) ==
          SALTS_ASN1_OK);
    CHECK(v.version == 2u);
    CHECK(v.tbs_der && v.tbs_size > 128u);
    CHECK(v.signature && v.signature_size > 128u);
    CHECK(v.serial_number && v.serial_number_size > 0u);
    CHECK(v.spki_der && v.spki_size > 128u);
    CHECK(v.signature_algorithm_oid && v.signature_algorithm_oid_size > 0u);
    CHECK(crypto_x509_der_get_public_key_kind(&v,&kind) == SALTS_ASN1_OK);
    CHECK(kind == CRYPTO_X509_DER_KEY_RSA);
    CHECK(crypto_x509_der_common_name(&v,0u,&name,&size) == SALTS_ASN1_OK);
    CHECK(size == sizeof(common_name)-1u);
    CHECK(memcmp(name,common_name,size) == 0);
    CHECK(crypto_x509_der_common_name(&v,1u,&issuer,&issuer_size) ==
          SALTS_ASN1_OK);
    CHECK(issuer_size == size && memcmp(issuer,name,size) == 0);
    CHECK(v.issuer_size == v.subject_size);
    CHECK(memcmp(v.issuer_der,v.subject_der,v.issuer_size) == 0);
    /* Issuer and subject bytes being equal only means self-issued by this
     * structural comparison. It NEVER proves the signature self-signed. */
    CHECK(crypto_x509_der_chain_next(test_cert_der,test_cert_der_len,
                                     &cursor,&v) == SALTS_ASN1_OK);
    CHECK(cursor == test_cert_der_len);
    unchanged = v;
    CHECK(crypto_x509_der_chain_next(test_cert_der,test_cert_der_len,
                                     &cursor,&v) == SALTS_ASN1_DONE);
    CHECK(cursor == test_cert_der_len);
    CHECK(memcmp(&v,&unchanged,sizeof(v)) == 0);
    memcpy(extra,test_cert_der,test_cert_der_len);
    extra[test_cert_der_len] = 0u;
    CHECK(crypto_x509_der_inspect(test_cert_der,test_cert_der_len-1u,&v)
          != SALTS_ASN1_OK);
    CHECK(crypto_x509_der_inspect(extra,sizeof(extra),&v)
          == SALTS_ASN1_TRAILING_DATA);
    CHECK(crypto_x509_der_common_name(&unchanged,2u,&name,&size)
          == SALTS_ASN1_INVALID_ARGUMENT);
    puts("PASS moved SaltsNet 951-byte RSA X.509 DER regression fixture");
    return 0;
}
