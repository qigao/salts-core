/* SPDX-License-Identifier: Apache-2.0
 * Test-only REAL Windows IOCP/OVERLAPPED WinSock TCP + installed C11
 * Crypto::TLSCipherBIO, adapted from the qualified POSIX owner logic.
 * No production CNet TLS target or GmSSL registry pin is changed.
 */
#ifndef _WIN32
#error This test requires real Windows IOCP
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
/* Include Winsock2 before Windows/platform headers; never winsock.h. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <salts/native_io.h>
#include <salts/error_codes.h>
#include <cmeta_buffer.h>
#include <crypto/tls.h>
#include <crypto/tls_ext.h>
#include <crypto/cipher_bio.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SDK_CA_FILE
#error SDK_CA_FILE required
#endif
#ifndef SDK_SERVER_CERT_FILE
#error SDK_SERVER_CERT_FILE required
#endif
#ifndef SDK_SERVER_KEY_FILE
#error SDK_SERVER_KEY_FILE required
#endif

enum {
    NATIVE_RECV_BYTES = 331,
    NATIVE_SEND_BYTES = 193,
    COMPLETION_CAPACITY = 8,
    MAX_DRIVES = 180000,
    DRAIN_ATTEMPTS = 8000
};

typedef struct io_peer {
    native_io_endpoint endpoint;
    SOCKET socket_fd;
    crypto_tls_cipher_bio *bio;
    crypto_tls_session *session;
    crypto_tls_io callbacks;
    mem_buffer_t *send_backing; /* immutable from submit to terminal observe */
    native_io_request send_request;
    native_io_request recv_request;
    unsigned char receive_backing[NATIVE_RECV_BYTES];
    size_t send_offset;
    bool recv_active, send_active, eof;
    bool io_leased, handshake_done;
    size_t send_terminal_count;
    size_t recv_terminal_count;
    size_t partial_send_completions;
    size_t input_bytes, output_bytes;
} io_peer;

typedef struct io_owner {
    native_io_backend backend;
    native_io_backend_config config;
    crypto_tls_provider *provider;
    io_peer peers[2];
    bool backend_initialized;
    bool admission_closed;
    bool sockets_attached;
    size_t completed;
} io_owner;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Crypto/NativeIO C11 contract failed line %d: %s\\n", \
                __LINE__, #expr); \
        goto cleanup; \
    } \
} while (0)


static native_io_backend_kind selected_backend(void)
{
    return NATIVE_IO_BACKEND_IOCP;
}

static int set_nonblocking(SOCKET native)
{
    u_long value = 1u;
    return ioctlsocket(native, FIONBIO, &value) == 0 ? 0 : -1;
}

/* WSA_FLAG_OVERLAPPED is essential: a synchronous socket must NEVER
 * qualify IOCP/OVERLAPPED admission, even if synchronous send() works. */
static SOCKET new_overlapped_tcp_socket(void)
{
    return WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                      NULL, 0u, WSA_FLAG_OVERLAPPED);
}

/* Two real connected TCP sockets. Listener/client/accepted sockets are
 * WinSock-native, not socketpair, poll, epoll or a Win32 pipe surrogate.
 */
static int make_connected_tcp(SOCKET *out_client, SOCKET *out_server)
{
    struct sockaddr_in address;
    int address_len = (int)sizeof(address);
    SOCKET listener = INVALID_SOCKET;
    SOCKET client = INVALID_SOCKET;
    SOCKET server = INVALID_SOCKET;
    memset(&address, 0, sizeof(address));
    *out_client = *out_server = INVALID_SOCKET;
    listener = new_overlapped_tcp_socket();
    if (listener == INVALID_SOCKET) goto fail;
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&address, (int)sizeof(address)) != 0
        || getsockname(listener, (struct sockaddr *)&address, &address_len) != 0
        || listen(listener, 2) != 0) goto fail;
    client = new_overlapped_tcp_socket();
    if (client == INVALID_SOCKET ||
        connect(client, (struct sockaddr *)&address, (int)sizeof(address)) != 0)
        goto fail;
    server = accept(listener, NULL, NULL);
    if (server == INVALID_SOCKET ||
        set_nonblocking(client) != 0 || set_nonblocking(server) != 0)
        goto fail;
    (void)closesocket(listener);
    *out_client = client;
    *out_server = server;
    return 0;
fail:
    if (server != INVALID_SOCKET) (void)closesocket(server);
    if (client != INVALID_SOCKET) (void)closesocket(client);
    if (listener != INVALID_SOCKET) (void)closesocket(listener);
    return -1;
}

static int observe_completions(io_owner *owner, uint32_t timeout)
{
    native_io_completion events[COMPLETION_CAPACITY];
    size_t count = 0u, i;
    int rc = native_io_backend_observe(
        &owner->backend, events, COMPLETION_CAPACITY, timeout, &count);
    if (rc == SALTS_ETIMEDOUT && count == 0u) return 0;
    if (rc != SALTS_OK) return -1;
    for (i = 0u; i < count; ++i) {
        const native_io_completion *completion = &events[i];
        const uintptr_t tag = completion->user_data;
        const size_t index = (size_t)(tag / 2u);
        const bool is_send = (tag % 2u) == 1u;
        io_peer *peer;
        if (index > 1u) return -2;
        peer = &owner->peers[index];

        if (is_send) {
            size_t pending;
            if (!peer->send_active || !peer->send_backing ||
                !native_io_request_valid(peer->send_request)
                || completion->request.slot != peer->send_request.slot
                || completion->request.generation != peer->send_request.generation)
                return -3;
            peer->send_active = false;
            peer->send_request = (native_io_request){0};
            pending = mem_buffer_used(peer->send_backing) - peer->send_offset;
            if (completion->kind != NATIVE_IO_COMPLETION_OK
                || completion->bytes == 0u || completion->bytes > pending)
                return -4;
            /* NativeIO's terminal completion is the FIRST moment at
             * which CNet may advance or release borrowed send backing.
             */
            peer->send_offset += completion->bytes;
            peer->output_bytes += completion->bytes;
            ++peer->send_terminal_count;
            if (completion->bytes < pending) ++peer->partial_send_completions;
            if (peer->send_offset == mem_buffer_used(peer->send_backing)) {
                mem_buffer_release(peer->send_backing);
                peer->send_backing = NULL;
                peer->send_offset = 0u;
            }
        } else {
            if (!peer->recv_active || !native_io_request_valid(peer->recv_request)
                || completion->request.slot != peer->recv_request.slot
                || completion->request.generation != peer->recv_request.generation)
                return -5;
            peer->recv_active = false;
            peer->recv_request = (native_io_request){0};
            ++peer->recv_terminal_count;
            if (completion->kind == NATIVE_IO_COMPLETION_EOF) {
                if (crypto_tls_cipher_bio_end_input(peer->bio) != CRYPTO_TLS_OK)
                    return -6;
                peer->eof = true;
            } else if (completion->kind == NATIVE_IO_COMPLETION_OK) {
                if (!completion->bytes
                    || completion->bytes > sizeof(peer->receive_backing)
                    || crypto_tls_cipher_bio_feed_cipher(
                        peer->bio, peer->receive_backing, completion->bytes)
                           != CRYPTO_TLS_OK)
                    return -7;
                peer->input_bytes += completion->bytes;
                memset(peer->receive_backing, 0, completion->bytes);
            } else {
                /* Normal data phase never interprets cancellation as EOF. */
                return -8;
            }
        }
        ++owner->completed;
    }
    return (int)count;
}

static int submit_peer_io(io_owner *owner, size_t index)
{
    io_peer *peer = &owner->peers[index];
    crypto_tls_cipher_bio_snapshot snapshot = {0};
    native_io_operation operation = {0};
    size_t taken = 0u;
    int rc;
    snapshot.struct_size = sizeof(snapshot);
    if (crypto_tls_cipher_bio_get_snapshot(peer->bio, &snapshot)
        != CRYPTO_TLS_OK) return -1;

    if (!peer->recv_active && !peer->eof &&
        snapshot.capacity - snapshot.input_pending >=
            sizeof(peer->receive_backing)) {
        operation.kind = NATIVE_IO_OPERATION_TCP_RECV;
        operation.endpoint = peer->endpoint;
        operation.buffer = peer->receive_backing;
        operation.length = sizeof(peer->receive_backing);
        operation.user_data = (uintptr_t)(index * 2u);
        rc = native_io_backend_submit(
            &owner->backend, &operation, &peer->recv_request);
        if (rc != SALTS_OK) return -2;
        peer->recv_active = true;
    }

    if (peer->send_active) return 0;
    if (!peer->send_backing && snapshot.output_pending > 0u) {
        /* The CNet Foundation Pool owns this ciphertext backing. Its
         * reference is held from native submit through terminal observe.
         * No GmSSL or Crypto ring pointer escapes into NativeIO.
         */
        peer->send_backing = mem_get_buffer(mem_global(), NATIVE_SEND_BYTES);
        if (!peer->send_backing) return -3;
        if (mem_buffer_capacity(peer->send_backing) < NATIVE_SEND_BYTES)
            return -4;
        if (crypto_tls_cipher_bio_take_cipher(
                peer->bio, (uint8_t *)mem_buffer_data(peer->send_backing),
                NATIVE_SEND_BYTES, &taken) != CRYPTO_TLS_OK
            || !taken || taken > NATIVE_SEND_BYTES) return -5;
        mem_set_used(peer->send_backing, taken);
        peer->send_offset = 0u;
    }
    if (peer->send_backing) {
        operation = (native_io_operation){0};
        operation.kind = NATIVE_IO_OPERATION_TCP_SEND;
        operation.endpoint = peer->endpoint;
        operation.buffer = mem_buffer_data(peer->send_backing)
            + peer->send_offset;
        operation.length = mem_buffer_used(peer->send_backing)
            - peer->send_offset;
        operation.user_data = (uintptr_t)(index * 2u + 1u);
        if (operation.length == 0u) return -6;
        rc = native_io_backend_submit(
            &owner->backend, &operation, &peer->send_request);
        if (rc != SALTS_OK) return -7;
        peer->send_active = true;
    }
    return 0;
}

static int pump_once(io_owner *owner)
{
    size_t index;
    /* Owner thread does not call NativeIO from a callback or another worker.
     * All TLS drive and backend completion mutations are serialized here.
     */
    for (index = 0u; index < 2u; ++index)
        if (submit_peer_io(owner, index) != 0) return -1;
    return observe_completions(owner, 1u) < 0 ? -2 : 0;
}

static int drain_terminals(io_owner *owner)
{
    unsigned i;
    size_t index;
    int remaining = 0;
    int rc;
    for (index = 0u; index < 2u; ++index) {
        io_peer *p = &owner->peers[index];
        if (p->send_active) {
            rc = native_io_backend_cancel(&owner->backend, p->send_request);
            if (rc != SALTS_OK && rc != SALTS_EALREADY) return -1;
        }
        if (p->recv_active) {
            rc = native_io_backend_cancel(&owner->backend, p->recv_request);
            if (rc != SALTS_OK && rc != SALTS_EALREADY) return -2;
        }
    }

    /* Close and cancellation are NOT terminal. Completion.kind, not cancel()
     * returning OK, releases the borrowed send/receive buffers.
     */
    for (i = 0u; i < DRAIN_ATTEMPTS; ++i) {
        native_io_completion completion[COMPLETION_CAPACITY];
        size_t count = 0u, n;
        remaining = 0;
        for (index = 0u; index < 2u; ++index) {
            if (owner->peers[index].send_active) ++remaining;
            if (owner->peers[index].recv_active) ++remaining;
        }
        if (!remaining) return 0;
        rc = native_io_backend_observe(
            &owner->backend, completion, COMPLETION_CAPACITY, 1u, &count);
        if (rc != SALTS_OK && rc != SALTS_ETIMEDOUT) return -3;
        for (n = 0u; n < count; ++n) {
            size_t peer_index = (size_t)(completion[n].user_data / 2u);
            bool sending = (completion[n].user_data % 2u) != 0u;
            io_peer *p;
            if (peer_index >= 2u) return -4;
            p = &owner->peers[peer_index];
            if (sending) {
                if (!p->send_active) return -5;
                p->send_active = false;
                p->send_request = (native_io_request){0};
                if (p->send_backing) {
                    mem_buffer_release(p->send_backing);
                    p->send_backing = NULL;
                    p->send_offset = 0u;
                }
            } else {
                if (!p->recv_active) return -6;
                p->recv_active = false;
                p->recv_request = (native_io_request){0};
            }
            ++owner->completed;
        }
    }
    return -7;
}

static int run_tls_nativeio(uint32_t protocol)
{
    io_owner owner = {0};
    crypto_tls_session_options options[2];
    native_io_backend_stats stats = {0};
    static const unsigned char payload[] =
        "CNet NativeIO terminal retention and exact Crypto TLS ciphertext";
    unsigned char response[sizeof(payload)] = {0};
    size_t received = 0u, total_cipher = 0u, i, got;
    bool sent = false, ready = false, shutdown_done[2] = {false,false};
    crypto_tls_result status;
    int rc = 1, remaining = 0;
    SOCKET sockets[2] = {INVALID_SOCKET, INVALID_SOCKET};
    WSADATA wsa;
    bool wsa_started = false;

    /* Explicit host WSA lifetime covers socket creation BEFORE NativeIO
     * independently increments its own WSAStartup reference.
     */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 5;
    wsa_started = true;
    owner.config = (native_io_backend_config){
        selected_backend(), 2u, 8u, COMPLETION_CAPACITY
    };
    for (i = 0u; i < 2u; ++i) owner.peers[i].socket_fd = INVALID_SOCKET;
    CHECK(make_connected_tcp(&sockets[0], &sockets[1]) == 0);
    owner.peers[0].socket_fd = sockets[0];
    owner.peers[1].socket_fd = sockets[1];
    CHECK(native_io_backend_kind_supported(owner.config.kind)
          && native_io_backend_init(&owner.backend, &owner.config) == SALTS_OK);
    owner.backend_initialized = true;
    for (i = 0u; i < 2u; ++i) {
        CHECK(native_io_backend_attach_socket(&owner.backend,
            (uintptr_t)owner.peers[i].socket_fd,
            &owner.peers[i].endpoint) == SALTS_OK);
    }
    owner.sockets_attached = true;

    CHECK(crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION,
                                     &owner.provider) == CRYPTO_TLS_OK);
    options[0] = (crypto_tls_session_options){
        sizeof(options[0]), protocol, CRYPTO_TLS_CLIENT, 4u,
        "localhost", SDK_CA_FILE, NULL, NULL, NULL, 0u
    };
    options[1] = (crypto_tls_session_options){
        sizeof(options[1]), protocol, CRYPTO_TLS_SERVER, 4u,
        NULL, NULL, SDK_SERVER_CERT_FILE, SDK_SERVER_KEY_FILE, "", 0u
    };
    for (i = 0u; i < 2u; ++i) {
        io_peer *peer = &owner.peers[i];
        CHECK(crypto_tls_cipher_bio_create(
            CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY, &peer->bio) == CRYPTO_TLS_OK);
        CHECK(crypto_tls_cipher_bio_acquire_io(peer->bio, &peer->callbacks)
              == CRYPTO_TLS_OK);
        peer->io_leased = true;
        CHECK(crypto_tls_session_create(owner.provider, &options[i],
            &peer->callbacks, &peer->session) == CRYPTO_TLS_OK);
    }

    for (i = 0u; i < MAX_DRIVES; ++i) {
        size_t side;
        for (side = 0u; side < 2u; ++side) {
            io_peer *peer = &owner.peers[side];
            if (!peer->handshake_done) {
                status = crypto_tls_session_handshake(peer->session);
                CHECK(status == CRYPTO_TLS_AGAIN || status == CRYPTO_TLS_OK);
                peer->handshake_done = status == CRYPTO_TLS_OK;
            }
        }
        CHECK(pump_once(&owner) == 0);
        if (owner.peers[0].handshake_done &&
            owner.peers[1].handshake_done) {
            ready = true;
            break;
        }
    }
    CHECK(ready && i < MAX_DRIVES);

    for (i = 0u; i < MAX_DRIVES; ++i) {
        if (!sent) {
            size_t accepted = 0u;
            status = crypto_tls_session_send(owner.peers[0].session,
                payload, sizeof(payload), &accepted);
            CHECK(status == CRYPTO_TLS_AGAIN || status == CRYPTO_TLS_OK);
            if (status == CRYPTO_TLS_OK) {
                CHECK(accepted == sizeof(payload));
                sent = true;
            }
        }
        CHECK(pump_once(&owner) == 0);
        if (received < sizeof(response)) {
            got = 0u;
            status = crypto_tls_session_receive(
                owner.peers[1].session, response + received,
                sizeof(response) - received, &got);
            CHECK(status == CRYPTO_TLS_AGAIN || status == CRYPTO_TLS_OK);
            if (status == CRYPTO_TLS_OK) {
                CHECK(got > 0u && got <= sizeof(response)-received);
                received += got;
            }
        }
        if (sent && received == sizeof(response)) break;
    }
    CHECK(sent && received == sizeof(payload)
          && memcmp(response, payload, sizeof(payload)) == 0);

    for (i = 0u; i < MAX_DRIVES; ++i) {
        size_t side;
        for (side = 0u; side < 2u; ++side) {
            if (!shutdown_done[side]) {
                status = crypto_tls_session_shutdown(owner.peers[side].session);
                CHECK(status == CRYPTO_TLS_AGAIN || status == CRYPTO_TLS_OK);
                shutdown_done[side] = status == CRYPTO_TLS_OK;
            }
        }
        CHECK(pump_once(&owner) == 0);
        if (shutdown_done[0] && shutdown_done[1]) break;
    }
    CHECK(shutdown_done[0] && shutdown_done[1]);
    for (i = 0u; i < 2u; ++i) {
        CHECK(crypto_tls_cipher_bio_destroy(&owner.peers[i].bio)
              == CRYPTO_TLS_BUSY);
        CHECK(owner.peers[i].send_terminal_count > 0u
              && owner.peers[i].recv_terminal_count > 0u);
        total_cipher += owner.peers[i].input_bytes
                       + owner.peers[i].output_bytes;
    }
    CHECK(total_cipher > 0u);

    /* Ensure a NativeIO receive has been admitted. Closing admission
     * does not retire the borrowed recv buffer, and cancel() cannot
     * substitute for observe() of its terminal completion.
     */
    CHECK(pump_once(&owner) == 0);
    remaining = owner.peers[0].recv_active || owner.peers[1].recv_active;
    CHECK(remaining);
    CHECK(native_io_backend_close(&owner.backend) == SALTS_OK);
    owner.admission_closed = true;
    CHECK(drain_terminals(&owner) == 0);
    CHECK(native_io_backend_get_stats(&owner.backend, &stats)
          && stats.active_requests == 0u);
    rc = 0;

cleanup:
    /* Any failure follows the SAME cancel-first, terminal-drain and
     * borrowed-pointer release barriers; no unsafe free on timeout.
     */
    if (owner.backend_initialized) {
        if (!owner.admission_closed) {
            (void)native_io_backend_close(&owner.backend);
            owner.admission_closed = true;
        }
        if (drain_terminals(&owner) != 0) {
            fprintf(stderr, "NativeIO terminal drain failed: unsafe to release borrowed buffers\n");
            return 4; /* leak intentionally rather than free in-flight borrow */
        }
    }
    for (i = 0u; i < 2u; ++i) {
        io_peer *peer = &owner.peers[i];
        if (peer->session) {
            (void)crypto_tls_session_cancel(peer->session);
            if (crypto_tls_session_destroy(&peer->session) != CRYPTO_TLS_OK)
                rc = 1;
        }
        if (peer->bio) {
            (void)crypto_tls_cipher_bio_cancel(peer->bio);
            if (peer->io_leased &&
                crypto_tls_cipher_bio_release_io(peer->bio) != CRYPTO_TLS_OK)
                rc = 1;
            if (crypto_tls_cipher_bio_destroy(&peer->bio) != CRYPTO_TLS_OK)
                rc = 1;
        }
        if (peer->send_backing) {
            mem_buffer_release(peer->send_backing);
            peer->send_backing = NULL;
        }
    }
    if (owner.provider &&
        crypto_tls_provider_destroy(&owner.provider) != CRYPTO_TLS_OK)
        rc = 1;
    if (owner.backend_initialized) {
        for (i = 0u; i < 2u; ++i) {
            io_peer *peer = &owner.peers[i];
            if (peer->socket_fd != INVALID_SOCKET) {
                (void)closesocket(peer->socket_fd);
                peer->socket_fd = INVALID_SOCKET;
            }
            if (native_io_endpoint_valid(peer->endpoint) &&
                native_io_backend_release_socket(
                    &owner.backend, peer->endpoint) != SALTS_OK)
                rc = 1;
        }
        if (native_io_backend_destroy(&owner.backend) != SALTS_OK)
            rc = 1;
    } else {
        for (i = 0u; i < 2u; ++i)
            if (owner.peers[i].socket_fd != INVALID_SOCKET)
                (void)closesocket(owner.peers[i].socket_fd);
    }
    if (wsa_started && WSACleanup() != 0) rc = 1;
    if (rc) {
        fprintf(stderr, "Crypto NativeIO real TLS %u failed; "
            "handshake=%u/%u received=%zu/%zu sends=%zu/%zu completions=%zu\n",
            protocol, owner.peers[0].handshake_done,
            owner.peers[1].handshake_done, received,
            sizeof(response), owner.peers[0].send_terminal_count,
            owner.peers[1].send_terminal_count, owner.completed);
    } else {
        printf("CNet-style Crypto/SaltsCore NativeIO real TLS %u PASS: "
               "cipher=%zu terminal=%zu; cancel/drain/one-owner\n",
               protocol, total_cipher, owner.completed);
    }
    return rc;
}

int main(void)
{
    if (crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION)
        return 3;
    if (run_tls_nativeio(CRYPTO_TLS_1_2) != 0) return 1;
    if (run_tls_nativeio(CRYPTO_TLS_1_3) != 0) return 2;
    return 0;
}
