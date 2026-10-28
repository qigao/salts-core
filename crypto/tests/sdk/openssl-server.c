/* SPDX-License-Identifier: Apache-2.0
 * Test-only installed C11 Crypto TLS server, used as an independent
 * OpenSSL 3 s_client exporter oracle. No GmSSL/OpenSSL library is linked.
 */
#define _POSIX_C_SOURCE 200809L
#include <crypto/tls.h>
#include <crypto/tls_ext.h>
#include <crypto/tls_peer.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

enum { MAX_MS = 18000, MAX_STEPS = 22000 };

typedef struct server_transport {
    int fd;
} server_transport;

static int64_t on_write(void *user, const uint8_t *data, size_t len)
{
    server_transport *t = (server_transport *)user;
    size_t chunk = len; /* Diagnose independent OpenSSL peer without forced split. */
    ssize_t n = send(t->fd, data, chunk, 0);
    if (n >= 0) return (int64_t)n;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return CRYPTO_TLS_IO_AGAIN;
    return CRYPTO_TLS_IO_ERROR;
}

static int64_t on_read(void *user, uint8_t *data, size_t len)
{
    server_transport *t = (server_transport *)user;
    size_t chunk = len; /* Native two-peer SDK still tests bounded short I/O. */
    ssize_t n = recv(t->fd, data, chunk, 0);
    if (n >= 0) return (int64_t)n;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return CRYPTO_TLS_IO_AGAIN;
    return CRYPTO_TLS_IO_ERROR;
}

static int64_t millis(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void pause_one_ms(void)
{
    struct timespec ts = {0, 1000000};
    (void)nanosleep(&ts, NULL);
}

static int accept_local(int port)
{
    struct sockaddr_in addr;
    struct timeval tv;
    int server = -1, peer = -1, option = 1, flags;
    fd_set readable;
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return -1;
    (void)setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
                     &option, (socklen_t)sizeof(option));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1
        || bind(server, (struct sockaddr *)&addr, sizeof(addr)) != 0
        || listen(server, 1) != 0)
        goto done;
    FD_ZERO(&readable);
    FD_SET(server, &readable);
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (select(server + 1, &readable, NULL, NULL, &tv) != 1)
        goto done;
    peer = accept(server, NULL, NULL);
    if (peer < 0) goto done;
    flags = fcntl(peer, F_GETFL, 0);
    if (flags < 0 || fcntl(peer, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(peer);
        peer = -1;
    }
done:
    close(server);
    return peer;
}

static int secret_hex_to_private_file(const char *path,
                                      const uint8_t *secret, size_t len)
{
    static const char hexdigits[] = "0123456789ABCDEF";
    char hex[CRYPTO_TLS_PEER_SHA256_BYTES * 2u + 1u];
    size_t i;
    int fd;
    ssize_t wrote;
    if (!path || !secret || len != CRYPTO_TLS_PEER_SHA256_BYTES)
        return -1;
    for (i = 0u; i < len; ++i) {
        hex[i*2u] = hexdigits[secret[i] >> 4u];
        hex[i*2u+1u] = hexdigits[secret[i] & 0x0fu];
    }
    hex[len*2u] = '\n';
    /* The containing work directory is private to the ephemeral test.
     * Exclusive create prevents symbolic-link overwrite and accidental reuse.
     */
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return -1;
    wrote = write(fd, hex, sizeof(hex));
    if (close(fd) != 0 || wrote != (ssize_t)sizeof(hex)) return -1;
    return 0;
}

int main(int argc, char **argv)
{
    crypto_tls_provider *provider = NULL;
    crypto_tls_session *session = NULL;
    crypto_tls_session_options options;
    crypto_tls_io io;
    crypto_tls_result status;
    server_transport transport = {-1};
    uint8_t secret[32] = {0};
    uint8_t request[128];
    static const uint8_t reply[] =
        "HTTP/1.0 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK";
    int port, proto, step, done_handshake = 0, got_request = 0, phase = 0;
    int64_t deadline;
    size_t bytes = 0u, i;
    int rc = 1;
    status = CRYPTO_TLS_INVALID_ARGUMENT;
    if (argc != 6) return 2;
    if (strcmp(argv[1], "12") == 0) proto = CRYPTO_TLS_1_2;
    else if (strcmp(argv[1], "13") == 0) proto = CRYPTO_TLS_1_3;
    else return 2;
    port = atoi(argv[2]);
    if (port <= 0 || port > 65535 || !argv[3][0] || !argv[4][0]
        || !argv[5][0]) return 2;

    (void)signal(SIGPIPE, SIG_IGN);
    phase = 1;
    transport.fd = accept_local(port);
    if (transport.fd < 0) {
        fprintf(stderr, "Crypto exporter peer: bounded accept failed\n");
        return 3;
    }

    memset(&options, 0, sizeof(options));
    options.struct_size = sizeof(options);
    options.protocol = (uint32_t)proto;
    options.role = CRYPTO_TLS_SERVER;
    options.verify_depth = 4u;
    options.certificate_file = argv[3];
    options.private_key_file = argv[4];
    options.private_key_password = "";
    memset(&io, 0, sizeof(io));
    io.struct_size = sizeof(io);
    io.user = &transport;
    io.read = on_read;
    io.write = on_write;

    phase = 2;
    if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider)
            != CRYPTO_TLS_OK
        || crypto_tls_session_create(provider, &options, &io, &session)
            != CRYPTO_TLS_OK
        || crypto_tls_peer_abi_version() != CRYPTO_TLS_PEER_ABI_VERSION)
        goto done;
    deadline = millis() + MAX_MS;
    for (step = 0; step < MAX_STEPS && millis() < deadline; ++step) {
        phase = 3;
        status = crypto_tls_session_handshake(session);
        if (status == CRYPTO_TLS_OK) {
            done_handshake = 1;
            break;
        }
        if (status != CRYPTO_TLS_AGAIN) goto done;
        pause_one_ms();
    }
    if (!done_handshake) goto done;
    fprintf(stderr, "CRYPTO_TEST_REVERSE_PHASE handshake_complete protocol=%d\n", proto);

    /* OpenSSL s_client -keymatexport uses no exporter context. The CNet
     * context-present flag is tested separately using both native peers.
     */
    phase = 4;
    if (crypto_tls_session_export_v1_2(
            session, "EXPORTER-Channel-Binding", NULL, 0u, 0u,
            secret, sizeof(secret)) != CRYPTO_TLS_OK)
        goto done;
    phase = 5;
    if (secret_hex_to_private_file(argv[5], secret, sizeof(secret)) != 0)
        goto done;

    fprintf(stderr, "CRYPTO_TEST_REVERSE_PHASE exporter_ready protocol=%d\n", proto);
    deadline = millis() + MAX_MS;
    for (step = 0; step < MAX_STEPS && millis() < deadline; ++step) {
        bytes = 0u;
        phase = 6;
        status = crypto_tls_session_receive(
            session, request, sizeof(request), &bytes);
        if (status == CRYPTO_TLS_OK && bytes) {
            got_request = 1;
            break;
        }
        if (status != CRYPTO_TLS_AGAIN) goto done;
        pause_one_ms();
    }
    if (!got_request) goto done;
    fprintf(stderr, "CRYPTO_TEST_REVERSE_PHASE request_received protocol=%d\n", proto);
    deadline = millis() + MAX_MS;
    for (step = 0; step < MAX_STEPS && millis() < deadline; ++step) {
        bytes = 0u;
        phase = 7;
        status = crypto_tls_session_send(
            session, reply, sizeof(reply)-1u, &bytes);
        if (status == CRYPTO_TLS_OK && bytes == sizeof(reply)-1u) {
            rc = 0;
            break;
        }
        if (status != CRYPTO_TLS_AGAIN) goto done;
        pause_one_ms();
    }
    if (rc == 0) {
        fprintf(stderr, "CRYPTO_TEST_REVERSE_PHASE response_sent protocol=%d\n", proto);
        /* Give the external peer a bounded opportunity to read before
         * closing transport. This test is about the exporter, not TLS close. */
        struct timespec settled = {0, 200000000};
        (void)nanosleep(&settled, NULL);
    }

done:
    {
        volatile uint8_t *wipe = secret;
        for (i = 0u; i < sizeof(secret); ++i) wipe[i] = 0u;
    }
    if (session && crypto_tls_session_destroy(&session) != CRYPTO_TLS_OK)
        rc = 1;
    if (provider && crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK)
        rc = 1;
    close(transport.fd);
    if (rc)
        fprintf(stderr, "Crypto test-only TLS server/exporter failed (protocol=%d,phase=%d,status=%d,handshake=%d,request=%d)\n",
                proto, phase, (int)status, done_handshake, got_request);
    return rc;
}
