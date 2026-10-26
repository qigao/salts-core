/* SPDX-License-Identifier: Apache-2.0
 * REAL opaque Crypto TLS1.2+1.3 ciphertext via the original CNet-private
 * cnet_tls_state function signatures, with no GmSSL dependency or fake
 * handshake. Original cnet_owner.c is still NOT linked by this test.
 */
#include "cnet_tls_crypto_qualification.h"
#include <salts/error_codes.h>
#include <crypto/tls.h>
#include <crypto/cipher_bio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef SDK_CA_FILE
#error SDK_CA_FILE is required
#endif
#ifndef SDK_SERVER_CERT_FILE
#error SDK_SERVER_CERT_FILE is required
#endif
#ifndef SDK_SERVER_KEY_FILE
#error SDK_SERVER_KEY_FILE is required
#endif

enum { MAX_STEPS = 180000, CIPHER_CHUNK = 193 };
#define CHECK(expr) do {                                                \
    if (!(expr)) {                                                       \
        fprintf(stderr, "CNet opaque Crypto TLS state failed line %d: %s\n", \
                __LINE__, #expr);                                        \
        goto cleanup;                                                    \
    }                                                                    \
} while (0)

static int pump_one(cnet_tls_state *source, cnet_tls_state *destination,
                    size_t *total)
{
    unsigned char cipher[CIPHER_CHUNK];
    size_t got = 0u;
    int status = cnet_tls_take_cipher(source, cipher, sizeof(cipher), &got);
    if (status == SALTS_ENOENT) return 0;
    if (status != SALTS_OK || !got || got > sizeof(cipher))
        return -1;
    if (cnet_tls_feed_cipher(destination, cipher, got) != SALTS_OK)
        return -2;
    *total += got;
    return 1;
}

static int pump_pair(cnet_tls_state *client, cnet_tls_state *server,
                     size_t *total)
{
    int rc = pump_one(client, server, total);
    if (rc < 0) return rc;
    rc = pump_one(server, client, total);
    return rc < 0 ? rc : 0;
}

static int run_one(crypto_tls_provider *provider, uint32_t protocol)
{
    cnet_tls_client_config client_policy = {0};
    cnet_tls_server_config server_policy = {0};
    cnet_tls_context *client_context = NULL, *server_context = NULL;
    cnet_tls_state client = {0}, server = {0};
    bool client_ready = false, server_ready = false;
    bool client_shutdown = false, server_shutdown = false;
    static const uint8_t payload[] =
        "Original CNet private TLS ABI, opaque Crypto state, and bounded plaintext retention";
    uint8_t received[sizeof(payload)] = {0};
    unsigned char client_binding[CNET_TLS_CHANNEL_BINDING_BYTES] = {0};
    unsigned char server_binding[CNET_TLS_CHANNEL_BINDING_BYTES] = {0};
    unsigned char bogus[64] = {0};
    char version[32] = {0}, cipher[128] = {0};
    char fingerprint[CNET_TLS_PEER_CERTIFICATE_SHA256_CAPACITY] = {0};
    size_t received_bytes = 0u, delivered = 0u, moved = 0u;
    size_t name_bytes = 0u, unsupported_size = 99u;
    unsigned steps;
    int rc = 1;
    const size_t capacity = CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY;

    client_policy.size = sizeof(client_policy);
    client_policy.ca_file = SDK_CA_FILE;
    client_policy.server_name = "localhost";
    server_policy.size = sizeof(server_policy);
    server_policy.cert_file = SDK_SERVER_CERT_FILE;
    server_policy.key_file = SDK_SERVER_KEY_FILE;

    CHECK(cnet_crypto_tls_test_client_context(
        provider, &client_policy, "localhost", protocol,
        capacity, &client_context) == SALTS_OK);
    CHECK(cnet_crypto_tls_test_server_context(
        provider, &server_policy, protocol,
        capacity, &server_context) == SALTS_OK);
    /* An unsupported range must be rejected BEFORE creating a session.
     * A failed admission does not transfer context ownership.
     */
    CHECK(cnet_tls_state_init(
        &client, client_context, false, "wrong.example",
        capacity) == SALTS_EINVAL);
    CHECK(client.engine == NULL && client.context == NULL);
    CHECK(cnet_tls_state_init(
        &client, client_context, false, "localhost",
        capacity) == SALTS_OK);
    client_context = NULL; /* owned by state, released by state_destroy */
    CHECK(cnet_tls_state_init(
        &server, server_context, true, NULL, capacity) == SALTS_OK);
    server_context = NULL;

    CHECK(cnet_tls_state_read_buffer(&client) != NULL);
    CHECK(cnet_tls_state_write_buffer(&client) != NULL);
    CHECK(cnet_tls_state_read_buffer(&server) != NULL);
    CHECK(cnet_tls_state_write_buffer(&server) != NULL);
    CHECK(cnet_tls_state_io_buffer_bytes(&client) == capacity);
    CHECK(cnet_tls_cipher_input_capacity(&client) == capacity);
    CHECK(cnet_tls_state_set_protocol_range(
        &client, CNET_TLS_PROTOCOL_VERSION_DEFAULT,
        CNET_TLS_PROTOCOL_VERSION_DEFAULT) == SALTS_ENOTSUP);
    CHECK(cnet_tls_state_set_protocol_range(
        &client, protocol == CRYPTO_TLS_1_2 ?
            CNET_TLS_PROTOCOL_VERSION_1_2 : CNET_TLS_PROTOCOL_VERSION_1_3,
        protocol == CRYPTO_TLS_1_2 ?
            CNET_TLS_PROTOCOL_VERSION_1_2 : CNET_TLS_PROTOCOL_VERSION_1_3) == SALTS_OK);
    CHECK(cnet_tls_state_set_protocol_range(
        &client, protocol == CRYPTO_TLS_1_2 ?
            CNET_TLS_PROTOCOL_VERSION_1_3 : CNET_TLS_PROTOCOL_VERSION_1_2,
        protocol == CRYPTO_TLS_1_2 ?
            CNET_TLS_PROTOCOL_VERSION_1_3 : CNET_TLS_PROTOCOL_VERSION_1_2) == SALTS_ENOTSUP);

    for (steps = 0u; steps < MAX_STEPS; ++steps) {
        if (!client_ready) {
            CHECK(cnet_tls_handshake(&client, &client_ready) == SALTS_OK);
        }
        if (!server_ready) {
            CHECK(cnet_tls_handshake(&server, &server_ready) == SALTS_OK);
        }
        CHECK(pump_pair(&client, &server, &moved) == 0);
        if (client_ready && server_ready) break;
    }
    CHECK(client_ready && server_ready && steps < MAX_STEPS);
    CHECK(cnet_tls_state_handshake_complete(&client));
    CHECK(cnet_tls_state_handshake_complete(&server));
    CHECK(cnet_tls_state_set_protocol_range(
        &client, CNET_TLS_PROTOCOL_VERSION_1_3,
        CNET_TLS_PROTOCOL_VERSION_1_3) == SALTS_EBUSY);

    CHECK(cnet_tls_state_negotiated_version(
        &client, version, sizeof(version), &name_bytes) == SALTS_OK);
    CHECK(strcmp(version, protocol == CRYPTO_TLS_1_2 ?
                 "TLSv1.2" : "TLSv1.3") == 0);
    CHECK(cnet_tls_state_negotiated_cipher(
        &client, cipher, sizeof(cipher), &name_bytes) == SALTS_OK);
    CHECK(name_bytes > 0u && cipher[0] != '\0');
    CHECK(cnet_tls_state_peer_certificate_sha256(
        &client, fingerprint) == SALTS_OK);
    CHECK(strlen(fingerprint) == 64u);
    CHECK(cnet_tls_state_export_channel_binding(
        &client, client_binding) == SALTS_OK);
    CHECK(cnet_tls_state_export_channel_binding(
        &server, server_binding) == SALTS_OK);
    CHECK(memcmp(client_binding, server_binding,
                 sizeof(client_binding)) == 0);
    CHECK(cnet_tls_state_server_end_point_binding(
        &client, bogus, sizeof(bogus), &unsupported_size) == SALTS_ENOTSUP);
    CHECK(unsupported_size == 0u);
    {
        const unsigned char *alpn = (const unsigned char *)1;
        size_t size = 17u;
        CHECK(cnet_tls_get_negotiated_alpn(
            &client, &alpn, &size) == SALTS_ENOENT);
        CHECK(alpn == NULL && size == 0u);
    }

    /* The probe is explicitly non-consumptive: it may move decrypted
     * bytes into private bounded engine storage but MUST retain them
     * until the application's CNet read consumes their exact prefix.
     */
    /* Regression for real original CNet Owner M3.9: after handshake,
     * polling a connection with no inbound ciphertext must NOT put the
     * private GmSSL backend into a pending read that prevents the
     * first application send. The original CNet idle owner does exactly
     * this no-demand peer-close probe before a queued application write.
     */
    {
        bool no_data_closed = true, no_data_pending = true;
        CHECK(cnet_tls_cipher_input_capacity(&client) == capacity);
        CHECK(cnet_tls_probe_peer_close(
            &client, &no_data_closed, &no_data_pending) == SALTS_OK);
        CHECK(!no_data_closed && !no_data_pending);
    }

    for (steps = 0u; steps < MAX_STEPS; ++steps) {
        bool closed = false, pending = false, written = false;
        if (delivered != sizeof(payload)) {
            CHECK(cnet_tls_write(
                &client, payload, sizeof(payload), &written) == SALTS_OK);
            if (written) delivered = sizeof(payload);
        }
        CHECK(pump_pair(&client, &server, &moved) == 0);
        CHECK(cnet_tls_probe_peer_close(
            &server, &closed, &pending) == SALTS_OK);
        CHECK(!closed);
        if (delivered && pending) break;
    }
    CHECK(delivered == sizeof(payload) && steps < MAX_STEPS);

    for (steps = 0u; steps < MAX_STEPS &&
                    received_bytes < sizeof(payload); ++steps) {
        size_t n = 0u;
        bool closed = false;
        const size_t limit = sizeof(payload) - received_bytes < 17u ?
            sizeof(payload) - received_bytes : 17u;
        CHECK(cnet_tls_read(&server, received + received_bytes,
                            limit, &n, &closed) == SALTS_OK);
        CHECK(!closed && n <= limit);
        received_bytes += n;
        CHECK(pump_pair(&client, &server, &moved) == 0);
    }
    CHECK(received_bytes == sizeof(payload));
    CHECK(memcmp(received, payload, sizeof(payload)) == 0);

    for (steps = 0u; steps < MAX_STEPS; ++steps) {
        bool produced = false;
        if (!client_shutdown) {
            CHECK(cnet_tls_shutdown(&client, &produced) == SALTS_OK);
            client_shutdown = produced;
        }
        if (!server_shutdown) {
            CHECK(cnet_tls_shutdown(&server, &produced) == SALTS_OK);
            server_shutdown = produced;
        }
        CHECK(pump_pair(&client, &server, &moved) == 0);
        if (client_shutdown && server_shutdown) break;
    }
    CHECK(client_shutdown && server_shutdown && moved > 0u);

    /* A CNet boolean peer_close_notify cannot be honestly synthesized
     * from Crypto CLOSED: no separate authenticated-shutdown inquiry.
     */
    CHECK(cnet_crypto_tls_test_transport_eof(&server) == SALTS_OK);
    CHECK(!server.peer_close_notify);
    rc = 0;
cleanup:
    if (client.engine != NULL) cnet_tls_state_destroy(&client);
    if (server.engine != NULL) cnet_tls_state_destroy(&server);
    if (client_context != NULL) cnet_tls_context_release(client_context);
    if (server_context != NULL) cnet_tls_context_release(server_context);
    if (rc) fprintf(stderr, "CNet TLS state protocol=0x%x failed: "
                    "handshake=%d/%d, delivered=%zu, received=%zu/%zu, moved=%zu\n",
                    protocol, client_ready, server_ready, delivered,
                    received_bytes, sizeof(payload), moved);
    else printf("CNet TLS state 0x%x genuine handshake/plaintext/probe/exporter/"
                "quiescent release PASS\n", protocol);
    return rc;
}

int main(void)
{
    crypto_tls_provider *provider = NULL;
    int rc = 1;
    if (crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION ||
        crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION,
                                   &provider) != CRYPTO_TLS_OK)
        return 2;
    if (run_one(provider, CRYPTO_TLS_1_2) != 0) goto done;
    if (run_one(provider, CRYPTO_TLS_1_3) != 0) goto done;
    rc = 0;
done:
    if (crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK)
        rc = 3;
    return rc;
}
