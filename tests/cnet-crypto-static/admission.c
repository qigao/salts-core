/* SPDX-License-Identifier: Apache-2.0
 * No production CNet source or TLS provider is built. Exercise the
 * ACTUAL public CNet config structs against a fail-closed test Owner
 * admission gate linked to the sole installed static Salts::Crypto archive.
 */
#include "cnet_crypto_owner_admission.h"
#include <salts/error_codes.h>
#include <salts_core/runtime_abi.h>
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
    const char *const alpn[] = {"h2", "http/1.1"};
    const char *const duplicate_alpn[] = {"h2", "h2"};
    const char *const nine_alpn[] = {
        "h2", "http/1.1", "a", "b", "c", "d", "e", "f", "g"};
    const char *const invalid_ascii_alpn[] = {"h2\001x"};
    char too_long_alpn[129];
    char sixty_four_a[65], sixty_four_b[65];
    const char *long_name[1] = {too_long_alpn};
    const char *long_pair[2] = {sixty_four_a, sixty_four_b};
    char oversized_identity[300];

    /* Force a physical shared SaltsCore Core import in the same process
     * as the single Crypto::TLS image. Headers-only linkage is not proof.
     */
    salts_core_runtime_abi_0_1_required();
    REQUIRE(salts_core_runtime_abi_version() == SALTS_CORE_RUNTIME_ABI_VERSION);
    REQUIRE(crypto_tls_abi_version() == CRYPTO_TLS_ABI_VERSION);
    /* Force the linked TLS/GmSSL and host BIO objects from one installed
     * static Salts::Crypto. These are not header-only/link-only claims.
     * No sockets or TLS sessions are constructed in this admission gate.
     */
    crypto_tls_provider *provider = NULL;
    crypto_tls_cipher_bio *bio = NULL;
    uint32_t active_sessions = UINT32_MAX;
    REQUIRE(crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider)
            == CRYPTO_TLS_OK && provider != NULL);
    REQUIRE(crypto_tls_provider_session_count(provider, &active_sessions)
            == CRYPTO_TLS_OK && active_sessions == 0u);
    REQUIRE(crypto_tls_cipher_bio_create(CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
                                         &bio) == CRYPTO_TLS_OK && bio != NULL);
    REQUIRE(crypto_tls_cipher_bio_destroy(&bio) == CRYPTO_TLS_OK && bio == NULL);
    REQUIRE(crypto_tls_provider_destroy(&provider) == CRYPTO_TLS_OK && provider == NULL);

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

    /* Same CNet config structure, exact representable text subset.
     * Rejection must never partially publish an options descriptor.
     */
    client.alpn_protocols = alpn;
    client.alpn_protocol_count = 2u;
    REQUIRE(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out) == SALTS_OK);
    client.alpn_protocols = duplicate_alpn;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.alpn_protocols = invalid_ascii_alpn;
    client.alpn_protocol_count = 1u;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    memset(too_long_alpn, 'x', sizeof(too_long_alpn) - 1u);
    too_long_alpn[sizeof(too_long_alpn) - 1u] = '\0';
    client.alpn_protocols = long_name;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    memset(sixty_four_a, 'a', 64u);
    memset(sixty_four_b, 'b', 64u);
    sixty_four_a[64] = sixty_four_b[64] = '\0';
    client.alpn_protocols = long_pair;
    client.alpn_protocol_count = 2u;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.alpn_protocols = nine_alpn;
    client.alpn_protocol_count = 9u;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    client.alpn_protocols = alpn;
    client.alpn_protocol_count = 0u;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_EINVAL);
    client.alpn_protocols = NULL;
    client.alpn_protocol_count = 1u;
    REJECT(cnet_crypto_owner_admit_client(
        &client, "localhost", CRYPTO_TLS_1_3,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_EINVAL);
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
    server.alpn_protocol_count = 2u;
    REQUIRE(cnet_crypto_owner_admit_server(
        &server, CRYPTO_TLS_1_3, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out) == SALTS_OK);
    server.alpn_protocols = duplicate_alpn;
    REJECT(cnet_crypto_owner_admit_server(
        &server, CRYPTO_TLS_1_3, CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY,
        CNET_CRYPTO_OWNER_TEST_ONLY, &out), SALTS_ENOTSUP);
    server.alpn_protocols = long_pair;
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

    puts("CNet real policy -> one static Crypto: TLS1.2/1.3, bounded ALPN positive,"
         " overflow/duplicate/binary ALPN and system CA/mTLS/TLCP/IP negatives PASS");
    return 0;
}
