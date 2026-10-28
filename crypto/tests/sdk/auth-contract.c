/* SPDX-License-Identifier: Apache-2.0
 * Installed C11 contract: additive mTLS ABI, bounded validation, unchanged
 * output on failure, one-owner session lifetime (no peer simulator).
 */
#include <crypto/tls_auth.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef SDK_CA_FILE
#error "Installed SDK test CA fixture is required"
#endif
#ifndef SDK_SERVER_CERT_FILE
#error "Installed SDK test server fixture is required"
#endif
#ifndef SDK_SERVER_KEY_FILE
#error "Installed SDK test key fixture is required"
#endif

static int64_t io_write(void *user, const uint8_t *data, size_t len)
{
    (void)user; (void)data; (void)len;
    return CRYPTO_TLS_IO_AGAIN;
}

static int64_t io_read(void *user, uint8_t *data, size_t len)
{
    (void)user; (void)data; (void)len;
    return CRYPTO_TLS_IO_AGAIN;
}

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "auth contract failed at line %d: %s\n", __LINE__, #x); \
    return 1; \
} } while (0)

int main(void)
{
    crypto_tls_provider *provider = NULL;
    crypto_tls_session *session = NULL;
    crypto_tls_session *sentinel = (crypto_tls_session *)(uintptr_t)0x1200;
    crypto_tls_session_options options = {0};
    crypto_tls_auth_options_v1_3 auth = {0};
    crypto_tls_io io = {0};
    crypto_tls_result result;
    uint32_t count = UINT32_MAX;
    char too_long[CRYPTO_TLS_AUTH_MAX_PATH_BYTES + 2u];
    char bad_hostname[257];
    const size_t original_v1_size = sizeof(crypto_tls_session_options);

    CHECK(crypto_tls_auth_abi_version() == CRYPTO_TLS_AUTH_ABI_VERSION);
    CHECK(crypto_tls_abi_version() == CRYPTO_TLS_ABI_VERSION);
    CHECK(crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider) == CRYPTO_TLS_OK);
    CHECK(provider != NULL);

    options.struct_size = (uint32_t)sizeof(options);
    options.protocol = CRYPTO_TLS_1_2;
    options.role = CRYPTO_TLS_SERVER;
    options.certificate_file = SDK_SERVER_CERT_FILE;
    options.private_key_file = SDK_SERVER_KEY_FILE;
    auth.struct_size = (uint32_t)sizeof(auth);
    auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
    auth.client_ca_file = SDK_CA_FILE;
    io.struct_size = (uint32_t)sizeof(io);
    io.read = io_read;
    io.write = io_write;

    result = crypto_tls_session_create_auth_v1_3(provider, &options, &auth,
        &io, &session);
    CHECK(result == CRYPTO_TLS_OK && session != NULL);
    CHECK(crypto_tls_provider_session_count(provider, &count) == CRYPTO_TLS_OK
        && count == 1u);
    CHECK(crypto_tls_provider_destroy(&provider) == CRYPTO_TLS_BUSY);
    CHECK(crypto_tls_session_cancel(session) == CRYPTO_TLS_OK);
    CHECK(crypto_tls_session_destroy(&session) == CRYPTO_TLS_OK && session == NULL);
    CHECK(crypto_tls_provider_session_count(provider, &count) == CRYPTO_TLS_OK
        && count == 0u);

#define EXPECT_BAD() do { \
    crypto_tls_session *out = sentinel; \
    CHECK(crypto_tls_session_create_auth_v1_3(provider, &options, &auth, \
        &io, &out) == CRYPTO_TLS_INVALID_ARGUMENT); \
    CHECK(out == sentinel); \
    CHECK(crypto_tls_provider_session_count(provider, &count) == CRYPTO_TLS_OK \
        && count == 0u); \
} while (0)

    auth.client_ca_file = NULL; EXPECT_BAD();
    auth.client_ca_file = SDK_CA_FILE;
    auth.client_auth = 2u; EXPECT_BAD();
    auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
    options.ca_file = SDK_CA_FILE; EXPECT_BAD();
    options.ca_file = NULL;
    auth.client_certificate_file = SDK_SERVER_CERT_FILE; EXPECT_BAD();
    auth.client_certificate_file = NULL;
    auth.reserved = 1u; EXPECT_BAD();
    auth.reserved = 0u;
    auth.reserved_flags = 1u; EXPECT_BAD();
    auth.reserved_flags = 0u;
    auth.struct_size--; EXPECT_BAD();
    auth.struct_size++;

    memset(too_long, 'X', sizeof(too_long));
    too_long[sizeof(too_long) - 1u] = '\0';
    auth.client_ca_file = too_long; EXPECT_BAD();
    auth.client_ca_file = SDK_CA_FILE;

    options.protocol = 0x101u; EXPECT_BAD();
    options.protocol = CRYPTO_TLS_1_2;
    options.role = CRYPTO_TLS_CLIENT;
    options.certificate_file = NULL;
    options.private_key_file = NULL;
    options.hostname = "localhost";
    options.ca_file = SDK_CA_FILE;
    auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_NONE;
    auth.client_ca_file = NULL;

    /* Exact 256 non-NUL bytes must fail BEFORE the legacy strlen validator.
     * This test must also remain valid under source ASan/UBSan.
     */
    memset(bad_hostname, 'H', sizeof(bad_hostname));
    bad_hostname[256] = '\0';
    options.hostname = bad_hostname;
    EXPECT_BAD();
    options.hostname = "localhost";

    auth.client_certificate_file = SDK_SERVER_CERT_FILE; EXPECT_BAD();
    auth.client_certificate_file = NULL;
    auth.client_private_key_file = SDK_SERVER_KEY_FILE; EXPECT_BAD();
    auth.client_private_key_file = NULL;
    auth.client_private_key_password = "unexpected"; EXPECT_BAD();
    auth.client_private_key_password = NULL;
    options.certificate_file = SDK_SERVER_CERT_FILE; EXPECT_BAD();
    options.certificate_file = NULL;

    auth.client_certificate_file = too_long;
    auth.client_private_key_file = SDK_SERVER_KEY_FILE; EXPECT_BAD();
    auth.client_certificate_file = NULL;
    auth.client_private_key_file = NULL;
    auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_REQUIRED; EXPECT_BAD();
    auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_NONE;
    CHECK(options.struct_size == original_v1_size);

    /* A backend failure during CA/key loading is a true rejected create.
     * No half-initialized owner, callback borrow or output mutation remains.
     */
    options.role = CRYPTO_TLS_SERVER;
    options.hostname = NULL;
    options.ca_file = NULL;
    options.certificate_file = SDK_SERVER_CERT_FILE;
    options.private_key_file = SDK_SERVER_KEY_FILE;
    auth.client_auth = CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
    auth.client_ca_file = "crypto-tls-mtls-CA-must-not-exist.pem";
    for (int attempt = 0; attempt < 48; ++attempt) {
        crypto_tls_session *out = sentinel;
        CHECK(crypto_tls_session_create_auth_v1_3(
            provider, &options, &auth, &io, &out) == CRYPTO_TLS_BACKEND_ERROR);
        CHECK(out == sentinel);
        CHECK(crypto_tls_provider_session_count(provider, &count) == CRYPTO_TLS_OK
            && count == 0u);
    }
    auth.client_ca_file = SDK_CA_FILE;
    options.private_key_file = "crypto-tls-mtls-key-must-not-exist.pem";
    for (int attempt = 0; attempt < 48; ++attempt) {
        crypto_tls_session *out = sentinel;
        CHECK(crypto_tls_session_create_auth_v1_3(
            provider, &options, &auth, &io, &out) == CRYPTO_TLS_BACKEND_ERROR);
        CHECK(out == sentinel);
        CHECK(crypto_tls_provider_session_count(provider, &count) == CRYPTO_TLS_OK
            && count == 0u);
    }
    options.private_key_file = SDK_SERVER_KEY_FILE;
    session = NULL;
    CHECK(crypto_tls_session_create_auth_v1_3(
        provider, &options, &auth, &io, &session) == CRYPTO_TLS_OK);
    CHECK(session != NULL);
    /* Never report authenticated mTLS before peer CertificateVerify. */
    {
        uint32_t authenticated = 77u;
        CHECK(crypto_tls_session_client_authenticated_v1_3(
            session, &authenticated) == CRYPTO_TLS_AGAIN);
        CHECK(authenticated == 77u);
        CHECK(crypto_tls_session_cancel(session) == CRYPTO_TLS_OK);
        CHECK(crypto_tls_session_client_authenticated_v1_3(
            session, &authenticated) == CRYPTO_TLS_CANCELLED);
        CHECK(authenticated == 77u);
    }
    CHECK(crypto_tls_session_destroy(&session) == CRYPTO_TLS_OK);
    CHECK(crypto_tls_provider_session_count(provider, &count) == CRYPTO_TLS_OK
        && count == 0u);

    CHECK(crypto_tls_provider_destroy(&provider) == CRYPTO_TLS_OK);
    CHECK(provider == NULL);
    puts("Installed Salts::Crypto mTLS auth contract: PASS");
    return 0;
}
