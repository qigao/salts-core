/* SPDX-License-Identifier: Apache-2.0
 * No production CNet source or TLS provider is built. Exercise the
 * ACTUAL public CNet config structs against a fail-closed test Owner
 * admission gate linked to the independently installed Crypto DSO.
 */
#include "cnet_crypto_owner_admission.h"
#include <salts/error_codes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define REQUIRE(expr) do {                                               \
    if (!(expr)) {                                                        \
        fprintf(stderr, "CNet Crypto Owner admission failed line %d: %s\\n", \
                __LINE__, #expr);                                         \
        return 1;                                                         \
    }                                                                     \
} while (0)

#define REJECT(call, expected) do {                                      \
    crypto_tls_session_options prior;                                    \
    memset(&out, 0xa5, sizeof(out));                                     \
    prior = out;                                                         \
    int rejected_status = (call);                                        \
    REQUIRE(rejected_status == (expected));                              \
    REQUIRE(memcmp(&out, &prior, sizeof(out)) == 0);                      \
} while (0)

int main(void)
{
    crypto_tls_session_options out = {0};
    cnet_tls_client_config client = {0};
    cnet_tls_server_config server = {0};
    const char *const alpn[] = {"h2"};
    char oversized_identity[300];

    REQUIRE(crypto_tls_abi_version() == CRYPTO_TLS_ABI_VERSION);
    client.size = sizeof(client);
    client.ca_file = "explicit-ca.pem";
    client.server_name = "localhost";
    server.size = sizeof(server);
    server.cert_file = "server-cert.pem";
    server.key_file = "server-key.pem";

    for (uint32_t protocol = CRYPTO_TLS_1_2; protocol <= CRYPTO_TLS_1_3; ++protocol) {
        REQUIRE(cnet_crypto_owner_admit_client(
            &client, "localhost", protocol,
            CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
            CNET_CRYPTO_OWNER_TEST_ONLY, &out) == SALTS_OK);
        REQUIRE(out.struct_size == sizeof(out) &&
                out.protocol == protocol && out.role == CRYPTO_TLS_CLIENT &&
                out.hostname == client.server_name &&
                out.ca_file == client.ca_file &&
                out.certificate_file == NULL &&
                out.private_key_file == NULL &&
                out.verify_depth == 4u && out.reserved_flags == 0u);

        REQUIRE(cnet_crypto_owner_admit_server(
            &server, protocol, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
            CNET_CRYPTO_OWNER_TEST_ONLY, &out) == SALTS_OK);
        REQUIRE(out.struct_size == sizeof(out) &&
                out.protocol == protocol && out.role == CRYPTO_TLS_SERVER &&
                out.hostname == NULL && out.ca_file == NULL &&
                out.certificate_file == server.cert_file &&
                out.private_key_file == server.key_file &&
                out.verify_depth == 4u && out.reserved_flags == 0u);
    }

    /* Production switch must be refused, even for test-admissible policy. */
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_PRODUCTION, &out), SALTS_ENOTSUP);
    REJECT(cnet_crypto_owner_admit_server(
        &server, CRYPTO_TLS_1_3, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_PRODUCTION, &out), SALTS_ENOTSUP);

    /* CNet default uses OS trust; installed Crypto currently requires a
     * file CA. No implicit system-trust or OpenSSL/GmSSL fallback.
     */
    client.ca_file = NULL;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.ca_file = "explicit-ca.pem";

    client.ca_path = "hashed-ca-directory";
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.ca_path = NULL;

    client.cert_file = "client-cert.pem";
    client.key_file = "client-key.pem";
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.cert_file = client.key_file = NULL;

    client.alpn_protocols = alpn;
    client.alpn_protocol_count = 1u;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.alpn_protocols = NULL;
    client.alpn_protocol_count = 0u;

    client.security_profile = CNET_TLS_SECURITY_PROFILE_SHANGMI_TLS13;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.security_profile = CNET_TLS_SECURITY_PROFILE_TLCP11;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.security_profile = CNET_TLS_SECURITY_PROFILE_STANDARD;

    REJECT(cnet_crypto_owner_admit_client(
        &client, "127.0.0.1", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_EINVAL);
    REJECT(cnet_crypto_owner_admit_client(
        &client, "::1", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_EINVAL);
    REJECT(cnet_crypto_owner_admit_client(
        &client, "wrong.example", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_EINVAL);
    memset(oversized_identity, 'x', sizeof(oversized_identity));
    oversized_identity[sizeof(oversized_identity)-1u] = '\0';
    REJECT(cnet_crypto_owner_admit_client(
        &client, oversized_identity, CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_EINVAL);

    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", 0u, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_2,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY-1u,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);

    server.client_auth = CNET_TLS_CLIENT_AUTH_REQUIRED;
    server.ca_file = "explicit-client-ca.pem";
    REJECT(cnet_crypto_owner_admit_server(
        &server, CRYPTO_TLS_1_2, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    server.client_auth = CNET_TLS_CLIENT_AUTH_NONE;
    server.ca_file = NULL;
    server.alpn_protocols = alpn;
    server.alpn_protocol_count = 1u;
    REJECT(cnet_crypto_owner_admit_server(
        &server, CRYPTO_TLS_1_3, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    server.alpn_protocols = NULL;
    server.alpn_protocol_count = 0u;
    server.security_profile = CNET_TLS_SECURITY_PROFILE_TLCP11;
    REJECT(cnet_crypto_owner_admit_server(
        &server, CRYPTO_TLS_1_2, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    server.security_profile = CNET_TLS_SECURITY_PROFILE_STANDARD;
    server.key_file = NULL;
    REJECT(cnet_crypto_owner_admit_server(
        &server, CRYPTO_TLS_1_2, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_EINVAL);

    puts("CNet real policy -> Crypto strict test admission: positive TLS1.2/1.3, "
         "production/system trust/CA path/mTLS/ALPN/ShangMi/TLCP/IP/ABI negatives PASS");
    return 0;
}
