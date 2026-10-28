/* SPDX-License-Identifier: Apache-2.0
 * CNet original public TLS peer-chain ABI regression. No GmSSL headers or
 * independent SSL provider. Uses the original 951-byte SaltsNet X.509 DER.
 */
#include "cnet_tls.h"
#include "saltsnet_original_test_cert_data.h"
#include <salts/error_codes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CNet Crypto peer chain line %d: %s\n", __LINE__, #cond); \
        return 1; \
    } \
} while (0)

int cnet_crypto_peer_chain_regression(void)
{
    cnet_tls_peer_certificate_chain chain = {0};
    const cnet_tls_peer_certificate_chain zero = {0};
    uint8_t joined[2u * sizeof(test_cert_der)];
    uint8_t large[CNET_TLS_PEER_CHAIN_MAX_BYTES + 1u] = {0};

    REQUIRE(test_cert_der_len < CNET_TLS_PEER_CHAIN_MAX_BYTES);
    REQUIRE(cnet_tls_peer_certificate_chain_parse(
        test_cert_der, test_cert_der_len, &chain) == SALTS_OK);
    REQUIRE(chain.count == 1u && chain.total_bytes == test_cert_der_len);
    REQUIRE(chain.certificates[0].data == test_cert_der &&
            chain.certificates[0].size == test_cert_der_len);

    memcpy(joined, test_cert_der, test_cert_der_len);
    memcpy(joined + test_cert_der_len, test_cert_der, test_cert_der_len);
    REQUIRE(cnet_tls_peer_certificate_chain_parse(
        joined, sizeof(joined), &chain) == SALTS_OK);
    REQUIRE(chain.count == 2u && chain.total_bytes == sizeof(joined));
    REQUIRE(chain.certificates[0].data == joined &&
            chain.certificates[0].size == test_cert_der_len);
    REQUIRE(chain.certificates[1].data == joined + test_cert_der_len &&
            chain.certificates[1].size == test_cert_der_len);

    joined[test_cert_der_len] = 0xffu;
    REQUIRE(cnet_tls_peer_certificate_chain_parse(
        joined, sizeof(joined), &chain) == SALTS_EPROTO);
    REQUIRE(memcmp(&chain, &zero, sizeof(chain)) == 0);

    REQUIRE(cnet_tls_peer_certificate_chain_parse(
        NULL, 0u, &chain) == SALTS_ENOENT);
    REQUIRE(memcmp(&chain, &zero, sizeof(chain)) == 0);
    REQUIRE(cnet_tls_peer_certificate_chain_parse(
        large, sizeof(large), &chain) == SALTS_ERANGE);
    REQUIRE(memcmp(&chain, &zero, sizeof(chain)) == 0);
    REQUIRE(cnet_tls_peer_certificate_chain_parse(
        test_cert_der, test_cert_der_len, NULL) == SALTS_EINVAL);
    return 0;
}
