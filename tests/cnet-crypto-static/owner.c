/* SPDX-License-Identifier: Apache-2.0
 * Real ORIGINAL CNet client / Owner command+event runtime with
 * ONE sole static Salts::Crypto archive in the final executable.
 * Both peers are real CNet clients using native platform NativeIO.
 * Public factory fallback is explicitly disabled: only exact test-private
 * verified CA/hostname and one TLS protocol are admitted.
 */
#include "public_context.h"
#include <cnet/cnet.h>
#include <crypto/tls.h>
#include <crypto/cipher_bio.h>
#include <cmeta_buffer.h>
#include <salts/clock.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

enum { APP_TIMEOUT_MS = 15000, MAX_POLL = 24000 };

typedef struct observed_peer {
    size_t connected;
    size_t terminal;
    size_t failed;
    size_t sent;
    size_t plaintext_received;
    size_t borrowed_received;
    cnet_error last_error;
    mem_slice_t retained;
} observed_peer;

static void on_state(void *user, cnet_connection handle,
                     cnet_connection_state state, const cnet_error *error)
{
    observed_peer *peer = (observed_peer *)user;
    (void)handle;
    if (state == CNET_CONNECTION_CONNECTED) peer->connected++;
    if (state == CNET_CONNECTION_CLOSED || state == CNET_CONNECTION_FAILED)
        peer->terminal++;
    if (state == CNET_CONNECTION_FAILED || error) {
        peer->failed++;
        if (error) peer->last_error = *error;
    }
}

static void on_borrowed(void *user, cnet_connection handle,
                        const cnet_receive_view *view)
{
    observed_peer *peer = (observed_peer *)user;
    (void)handle; (void)view;
    peer->borrowed_received++;
}

static void on_owned(void *user, cnet_connection handle,
                     mem_slice_t slice, cnet_message_kind kind)
{
    observed_peer *peer = (observed_peer *)user;
    (void)handle;
    if (kind != CNET_MESSAGE_BYTES || !slice.buffer || !slice.data ||
        peer->retained.buffer != NULL) {
        peer->failed++;
        mem_slice_release(&slice);
        return;
    }
    peer->retained = slice; /* move-owned; caller releases after client destroy */
    peer->plaintext_received++;
}

static void on_sent(void *user, cnet_connection handle, size_t sent)
{
    observed_peer *peer = (observed_peer *)user;
    (void)handle;
    if (!sent) peer->failed++;
    peer->sent++;
}

static native_io_backend_kind current_native_kind(void)
{
#if defined(_WIN32)
    return NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
    return NATIVE_IO_BACKEND_KQUEUE;
#else
    return NATIVE_IO_BACKEND_EPOLL;
#endif
}

static cnet_client_config client_config(void)
{
    cnet_client_config c = {0};
    c.backend = current_native_kind();
    c.connection_capacity = 4u;
    c.command_capacity = 32u;
    c.request_capacity = 16u;
    c.completion_batch_capacity = 16u;
    c.event_capacity = 32u;
    c.max_send_bytes = 1024u;
    c.receive_buffer_bytes = 4096u;
    c.tls_io_buffer_bytes = CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY;
    c.tls_handshake_timeout_ms = APP_TIMEOUT_MS;
    return c;
}

static int pump(cnet_client *dialer, cnet_client *acceptor)
{
    size_t events = 0u;
    if (cnet_client_poll(dialer, 1u, &events) != SALTS_OK) return -1;
    if (cnet_client_poll(acceptor, 1u, &events) != SALTS_OK) return -2;
    return 0;
}

#define CHECK(x) do { \
    if (!(x)) { \
        fprintf(stderr,"CNet full original Crypto TLS Owner failure line %d: %s\\n", \
                __LINE__, #x); \
        goto done; \
    } \
} while (0)

static int run_protocol(uint32_t protocol, bool with_alpn)
{
    crypto_tls_provider *provider = NULL;
    cnet_tls_client profile = {0};
    cnet_tls_server identity = {0};
    cnet_tls_client_config client_policy = {0};
    cnet_tls_server_config server_policy = {0};
    cnet_client dialer = {0}, acceptor = {0};
    cnet_listener listener = {0};
    observed_peer client_seen = {0}, server_seen = {0};
    cnet_connection dialed = {0}, accepted = {0};
    cnet_observer client_observer = {on_state, on_borrowed,
                                     &client_seen, on_sent};
    cnet_observer server_observer = {on_state, on_borrowed,
                                     &server_seen, on_sent};
    const cnet_client_config options = client_config();
    cnet_listener_config listen_cfg = {current_native_kind(),
                                       "127.0.0.1", 0u, 4u};
    static const uint8_t PAYLOAD[] =
        "Original CNet Owner / Crypto opaque TLS and retained NativeIO completion";
    mem_buffer_t *send_backing = NULL;
    char uri[128], version[32], cipher[128], fingerprint[80];
    char selected_alpn[256] = {0};
    unsigned char client_binding[CNET_TLS_CHANNEL_BINDING_BYTES] = {0};
    unsigned char server_binding[CNET_TLS_CHANNEL_BINDING_BYTES] = {0};
    uint16_t port = 0u;
    size_t name_size = 0u;
    uint64_t deadline;
    int listener_live = 0, dialer_live = 0, acceptor_live = 0;
    int rc = 1, listener_ready = 0, poll_rc, status;
    unsigned poll_count;
    const char *expected_version =
        protocol == CRYPTO_TLS_1_2 ? "TLSv1.2" : "TLSv1.3";
    char client_h2[] = "h2", client_http[] = "http/1.1";
    char server_h2[] = "h2";
    const char *const client_offers[] = {client_h2, client_http};
    const char *const server_offers[] = {server_h2};

    CHECK(crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider)
          == CRYPTO_TLS_OK);
    client_policy.size = sizeof(client_policy);
    client_policy.ca_file = SDK_CA_FILE;
    client_policy.server_name = "localhost";
    server_policy.size = sizeof(server_policy);
    server_policy.cert_file = SDK_SERVER_CERT_FILE;
    server_policy.key_file = SDK_SERVER_KEY_FILE;
    if (with_alpn) {
        client_policy.alpn_protocols = client_offers;
        client_policy.alpn_protocol_count = 2u;
        server_policy.alpn_protocols = server_offers;
        server_policy.alpn_protocol_count = 1u;
    }

    CHECK(cnet_crypto_test_bind_client(
        &profile, provider, &client_policy, "localhost", protocol,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY) == SALTS_OK);
    CHECK(cnet_crypto_test_bind_server(
        &identity, provider, &server_policy, protocol,
        CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY) == SALTS_OK);
    if (with_alpn) {
        /* Reusable client/server policies are synchronously consumed.
         * Force mutation before original CNet Owner admits the TCP session;
         * selected ALPN must still be h2, not x2.
         */
        client_h2[0] = 'x';
        client_http[0] = 'x';
        server_h2[0] = 'x';
    }
    CHECK(cnet_listener_init(&listener, &listen_cfg) == SALTS_OK);
    listener_live = 1;
    CHECK(cnet_listener_port(&listener, &port) == SALTS_OK && port != 0u);
    CHECK(cnet_client_init(&dialer, &options) == SALTS_OK);
    dialer_live = 1;
    CHECK(cnet_client_init(&acceptor, &options) == SALTS_OK);
    acceptor_live = 1;
    CHECK(snprintf(uri, sizeof(uri), "tls://127.0.0.1:%u",
                   (unsigned)port) > 0);
    {
        cnet_connect_options connect = {0};
        connect.uri = uri;
        connect.tls_client = &profile;
        connect.observer = client_observer;
        CHECK(cnet_connect(&dialer, &connect, &dialed) == SALTS_OK);
    }

    deadline = cmeta_monotonic_ms() + APP_TIMEOUT_MS;
    for (poll_count = 0u; poll_count < MAX_POLL &&
         cmeta_monotonic_ms() < deadline; ++poll_count) {
        CHECK(pump(&dialer, &acceptor) == 0);
        if (!accepted.slot) {
            listener_ready = 0;
            status = cnet_listener_wait(&listener, 1u, &listener_ready);
            CHECK(status == SALTS_OK || status == SALTS_ETIMEDOUT);
            if (status == SALTS_OK && listener_ready) {
                CHECK(cnet_listener_accept_tls(
                    &listener, &acceptor, &identity, &server_observer,
                    &accepted) == SALTS_OK);
                CHECK(accepted.slot && accepted.generation);
            }
        }
        CHECK(!client_seen.failed && !server_seen.failed);
        if (accepted.slot && client_seen.connected == 1u &&
            server_seen.connected == 1u) break;
    }
    CHECK(client_seen.connected == 1u && server_seen.connected == 1u);
    CHECK(!client_seen.failed && !server_seen.failed);
    CHECK(accepted.slot != 0u);

    memset(version, 0, sizeof(version));
    memset(cipher, 0, sizeof(cipher));
    memset(fingerprint, 0, sizeof(fingerprint));
    CHECK(cnet_tls_negotiated_version(&dialer, dialed, version,
           sizeof(version), &name_size) == SALTS_OK);
    CHECK(strcmp(version, expected_version) == 0);
    CHECK(cnet_tls_negotiated_cipher(&dialer, dialed, cipher,
           sizeof(cipher), &name_size) == SALTS_OK && name_size > 0u);
    if (with_alpn) {
        CHECK(cnet_tls_negotiated_alpn(
            &dialer, dialed, selected_alpn, sizeof(selected_alpn),
            &name_size) == SALTS_OK);
        CHECK(name_size == 2u && strcmp(selected_alpn, "h2") == 0);
        CHECK(cnet_tls_negotiated_alpn(
            &acceptor, accepted, selected_alpn, sizeof(selected_alpn),
            &name_size) == SALTS_OK);
        CHECK(name_size == 2u && strcmp(selected_alpn, "h2") == 0);
        CHECK(cnet_tls_negotiated_alpn(
            &dialer, dialed, selected_alpn, 2u, &name_size)
            == SALTS_EMSGSIZE && name_size == 0u);
    } else {
        CHECK(cnet_tls_negotiated_alpn(
            &dialer, dialed, selected_alpn, sizeof(selected_alpn),
            &name_size) == SALTS_ENOENT && name_size == 0u);
    }
    CHECK(cnet_tls_peer_certificate_sha256(
        &dialer, dialed, fingerprint) == SALTS_OK);
    CHECK(strlen(fingerprint) == 64u);
    CHECK(cnet_tls_export_channel_binding(
          &dialer, dialed, client_binding) == SALTS_OK);
    CHECK(cnet_tls_export_channel_binding(
          &acceptor, accepted, server_binding) == SALTS_OK);
    CHECK(memcmp(client_binding, server_binding,
                 sizeof(client_binding)) == 0);

    CHECK(cnet_set_receive_slice_handler(&acceptor, accepted,
           on_owned, &server_seen) == SALTS_OK);
    CHECK(cnet_receive(&acceptor, accepted, sizeof(PAYLOAD)) == SALTS_OK);
    send_backing = mem_get_buffer(mem_global(), sizeof(PAYLOAD));
    CHECK(send_backing != NULL);
    memcpy(mem_buffer_data(send_backing), PAYLOAD, sizeof(PAYLOAD));
    mem_set_used(send_backing, sizeof(PAYLOAD));
    CHECK(cnet_send_buffer(&dialer, dialed, send_backing) == SALTS_OK);
    mem_buffer_release(send_backing); /* CNet owns ref until terminal */
    send_backing = NULL;

    deadline = cmeta_monotonic_ms() + APP_TIMEOUT_MS;
    for (poll_count = 0u; poll_count < MAX_POLL &&
         cmeta_monotonic_ms() < deadline; ++poll_count) {
        CHECK(pump(&dialer, &acceptor) == 0);
        CHECK(!client_seen.failed && !server_seen.failed);
        if (client_seen.sent == 1u &&
            server_seen.plaintext_received == 1u) break;
    }
    CHECK(client_seen.sent == 1u && server_seen.plaintext_received == 1u);
    CHECK(server_seen.retained.buffer != NULL);
    CHECK(server_seen.retained.length == sizeof(PAYLOAD));
    CHECK(memcmp(server_seen.retained.data, PAYLOAD, sizeof(PAYLOAD)) == 0);
    CHECK(mem_buffer_pool(server_seen.retained.buffer) == mem_global());

    CHECK(cnet_close(&dialer, dialed) == SALTS_OK);
    CHECK(cnet_close(&acceptor, accepted) == SALTS_OK);
    deadline = cmeta_monotonic_ms() + APP_TIMEOUT_MS;
    for (poll_count = 0u; poll_count < MAX_POLL &&
         cmeta_monotonic_ms() < deadline; ++poll_count) {
        poll_rc = pump(&dialer, &acceptor);
        CHECK(poll_rc == 0);
        if (client_seen.terminal >= 1u && server_seen.terminal >= 1u)
            break;
    }
    CHECK(client_seen.terminal >= 1u && server_seen.terminal >= 1u);
    CHECK(!client_seen.failed && !server_seen.failed);
    rc = 0;
done:
    if (send_backing) mem_buffer_release(send_backing);
    if (dialer_live) {
        if (cnet_client_stop(&dialer, 5000u) != SALTS_OK) rc = 1;
        if (cnet_client_destroy(&dialer) != SALTS_OK) rc = 1;
    }
    if (acceptor_live) {
        if (cnet_client_stop(&acceptor, 5000u) != SALTS_OK) rc = 1;
        if (cnet_client_destroy(&acceptor) != SALTS_OK) rc = 1;
    }
    if (listener_live) {
        if (cnet_listener_close(&listener) != SALTS_OK) rc = 1;
        if (cnet_listener_destroy(&listener) != SALTS_OK) rc = 1;
    }
    /* The CNet DSO and both Owner objects are already destroyed. Their
     * separately retained Core mem_slice must still be valid and releasable.
     */
    if (server_seen.retained.buffer) {
        if (server_seen.retained.length != sizeof(PAYLOAD) ||
            memcmp(server_seen.retained.data, PAYLOAD,
                   sizeof(PAYLOAD)) != 0)
            rc = 1;
        mem_slice_release(&server_seen.retained);
    }
    if (cnet_tls_client_destroy(&profile) != SALTS_OK ||
        cnet_tls_server_destroy(&identity) != SALTS_OK)
        rc = 1;
    if (provider && crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK)
        rc = 1;
    if (rc) fprintf(stderr,
        "Original CNet TLS Owner 0x%x failed client_state=%zu/%zu/%zu"
        " server_state=%zu/%zu/%zu sender=%zu received=%zu error=%d stage=%s\\n",
        protocol, client_seen.connected, client_seen.terminal,
        client_seen.failed, server_seen.connected, server_seen.terminal,
        server_seen.failed, client_seen.sent, server_seen.plaintext_received,
        client_seen.last_error.status,
        client_seen.last_error.stage ? client_seen.last_error.stage : "(none)");
    else printf("Original CNet Owner + sole static Crypto TLS 0x%x native"
                " handshake/callback/slice/exporter/close PASS\\n", protocol);
    return rc;
}

int main(void)
{
    cnet_tls_client dummy = {0};
    cnet_tls_client_config default_policy = {0};
    default_policy.size = sizeof(default_policy);
    if (cnet_tls_client_init(&dummy, &default_policy) != SALTS_ENOTSUP ||
        dummy.impl != NULL)
        return 2; /* Never admit implicit root stores/default TLS range. */
    if (run_protocol(CRYPTO_TLS_1_2, false) != 0) return 1;
    if (run_protocol(CRYPTO_TLS_1_2, true) != 0) return 1;
    if (run_protocol(CRYPTO_TLS_1_3, false) != 0) return 1;
    if (run_protocol(CRYPTO_TLS_1_3, true) != 0) return 1;
    return 0;
}
