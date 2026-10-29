/* SPDX-License-Identifier: Apache-2.0
 * Test-only OpenSSL 3 API client for Crypto TLS server interoperability.
 * This native program links ONLY OpenSSL::SSL and OpenSSL::Crypto; never
 * Crypto::TLS, GmSSL or their private source code.
 */
#define _POSIX_C_SOURCE 200809L
#include <openssl/ssl.h>
#include <openssl/ssl3.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { KEY_LEN = 32, HTTP_LIMIT = 4096 };
static const unsigned char HELLO_RETRY_RANDOM[32] = {
    0xcf,0x21,0xad,0x74,0xe5,0x9a,0x61,0x11,
    0xbe,0x1d,0x8c,0x02,0x1e,0x65,0xb8,0x91,
    0xc2,0xa2,0x11,0x16,0x7a,0xbb,0x8c,0x5e,
    0x07,0x9e,0x09,0xe2,0xc8,0xa8,0x33,0x9c
};
typedef struct handshake_observation {
    unsigned hello_retry_count;
    unsigned server_hello_count;
    unsigned client_hello_count;
} handshake_observation;

static void observe_handshake(int write_p, int version, int content_type,
    const void *buffer, size_t length, SSL *ssl, void *user)
{
    handshake_observation *seen = (handshake_observation *)user;
    const unsigned char *record = (const unsigned char *)buffer;
    (void)version;
    (void)ssl;
    /* Observe ONLY public HRR magic at ServerHello.random, never print or
     * store any random, certificate, transcript or keying material bytes.
     */
    if (!seen || content_type != SSL3_RT_HANDSHAKE || !record ||
        length < 4u) return;
    if (write_p && record[0] == SSL3_MT_CLIENT_HELLO) {
        ++seen->client_hello_count;
        return;
    }
    if (write_p || length < 38u || record[0] != SSL3_MT_SERVER_HELLO)
        return;
    ++seen->server_hello_count;
    if (memcmp(record + 6u, HELLO_RETRY_RANDOM, 32u) == 0)
        ++seen->hello_retry_count;
}

static int connect_local(int port)
{
    int fd;
    struct sockaddr_in peer;
    struct timeval bound = {12, 0};
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&peer, 0, sizeof(peer));
    peer.sin_family = AF_INET;
    peer.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, "127.0.0.1", &peer.sin_addr) != 1
        || connect(fd, (struct sockaddr *)&peer, sizeof(peer)) != 0
        || setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
            &bound, (socklen_t)sizeof(bound)) != 0
        || setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
            &bound, (socklen_t)sizeof(bound)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int save_derived(const char *path, const unsigned char secret[KEY_LEN])
{
    static const char HEX[] = "0123456789ABCDEF";
    char representation[KEY_LEN * 2 + 1u];
    size_t i, offset = 0u;
    int fd, result = -1;
    ssize_t count;
    for (i = 0u; i < KEY_LEN; ++i) {
        representation[i * 2] = HEX[secret[i] >> 4];
        representation[i * 2 + 1] = HEX[secret[i] & 15];
    }
    representation[KEY_LEN * 2] = '\n';
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) goto done;
    while (offset < sizeof(representation)) {
        count = write(fd, representation + offset, sizeof(representation) - offset);
        if (count <= 0) break;
        offset += (size_t)count;
    }
    result = (close(fd) == 0 && offset == sizeof(representation)) ? 0 : -1;
    if (result) (void)unlink(path);
done:
    OPENSSL_cleanse(representation, sizeof(representation));
    return result;
}

int main(int argc, char **argv)
{
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    int fd = -1, rc = 1, version, port, stage = 0;
    int ssl_connect_ret = 0, ssl_connect_error = 0, socket_errno = 0;
    unsigned char derived[KEY_LEN] = {0};
    handshake_observation obs = {0u, 0u};
    static const char LABEL[] = "EXPORTER-Channel-Binding";
    static const char REQUEST[] = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
    char response[HTTP_LIMIT + 1u];
    size_t offset = 0u;
    int count;
    unsigned long error_code;
    const char *mode;

    if (argc != 7) return 2;
    if (!strcmp(argv[1], "12")) version = TLS1_2_VERSION;
    else if (!strcmp(argv[1], "13")) version = TLS1_3_VERSION;
    else return 2;
    port = atoi(argv[2]);
    mode = argv[5];
    if (port < 1 || port > 65535 || !argv[3][0] || !argv[4][0] ||
        !argv[6][0] || (strcmp(mode, "p256") && strcmp(mode, "hrr")))
        return 2;
    (void)signal(SIGPIPE, SIG_IGN);
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) goto done;
    stage = 1;
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_set_min_proto_version(ctx, version) != 1
        || SSL_CTX_set_max_proto_version(ctx, version) != 1
        || SSL_CTX_load_verify_locations(ctx, argv[3], NULL) != 1
        || SSL_CTX_set_cipher_list(ctx, "ECDHE-RSA-AES128-GCM-SHA256") != 1
        || SSL_CTX_set_ciphersuites(ctx, "TLS_AES_128_GCM_SHA256") != 1
        || SSL_CTX_set1_sigalgs_list(ctx, version == TLS1_2_VERSION
            ? "rsa_pkcs1_sha256" : "rsa_pss_rsae_sha256") != 1)
        goto done;
    if (!strcmp(mode, "p256") &&
        SSL_CTX_set1_groups_list(ctx, "P-256") != 1)
        goto done;
    ssl = SSL_new(ctx);
    /* This specifically qualifies HelloRetryRequest. The Crypto server
     * under test has no ALPN policy installed; do not advertise ALPN here.
     * Positive h2 and negative ALPN policy are separately qualified by
     * the existing Crypto client / independent OpenSSL server matrix.
     */
    if (!ssl || SSL_set_tlsext_host_name(ssl, argv[4]) != 1
        || SSL_set1_host(ssl, argv[4]) != 1)
        goto done;
    SSL_set_msg_callback(ssl, observe_handshake);
    SSL_set_msg_callback_arg(ssl, &obs);
    fd = connect_local(port);
    if (fd < 0 || SSL_set_fd(ssl, fd) != 1) goto done;

    stage = 2;
    ssl_connect_ret = SSL_connect(ssl);
    if (ssl_connect_ret != 1) {
        ssl_connect_error = SSL_get_error(ssl, ssl_connect_ret);
        socket_errno = errno;
        goto done;
    }
    if (SSL_get_verify_result(ssl) != X509_V_OK ||
        SSL_version(ssl) != version)
        goto done;
    if (version == TLS1_3_VERSION) {
        if ((!strcmp(mode, "hrr") && obs.hello_retry_count != 1u) ||
            (!strcmp(mode, "p256") && obs.hello_retry_count != 0u))
            goto done;
    }
    stage = 3;
    if (SSL_export_keying_material(ssl, derived, sizeof(derived),
        LABEL, sizeof(LABEL) - 1u, NULL, 0u, 0) != 1)
        goto done;
    while (offset < sizeof(REQUEST) - 1u) {
        count = SSL_write(ssl, REQUEST + offset,
            (int)(sizeof(REQUEST) - 1u - offset));
        if (count <= 0) goto done;
        offset += (size_t)count;
    }
    stage = 4;
    offset = 0u;
    while (offset < HTTP_LIMIT) {
        count = SSL_read(ssl, response + offset, (int)(HTTP_LIMIT - offset));
        if (count <= 0) break;
        offset += (size_t)count;
        response[offset] = 0;
        if (strstr(response, "HTTP/1.0 200 OK") != NULL) break;
    }
    if (!offset || strstr(response, "HTTP/1.0 200 OK") == NULL)
        goto done;
    stage = 5;
    if (save_derived(argv[6], derived) != 0)
        goto done;
    rc = 0;
done:
    error_code = ERR_peek_last_error();
    if (rc) {
        fprintf(stderr,
            "CRYPTO_OPENSSL_API_PEER_RESULT version=%d mode=%s stage=%d"
            " hrr=%u server_hello=%u client_hello=%u"
            " connect_ret=%d ssl_error=%d socket_errno=%d ssl_reason=%lu\n",
            version, mode ? mode : "(null)", stage, obs.hello_retry_count,
            obs.server_hello_count, obs.client_hello_count,
            ssl_connect_ret, ssl_connect_error, socket_errno,
            error_code ? ERR_GET_REASON(error_code) : 0ul);
    } else {
        fprintf(stderr,
            "CRYPTO_OPENSSL_API_PEER_RESULT version=%d mode=%s stage=5"
            " hrr=%u server_hello=%u client_hello=%u result=ok\n",
            version, mode, obs.hello_retry_count,
            obs.server_hello_count, obs.client_hello_count);
    }
    OPENSSL_cleanse(derived, sizeof(derived));
    if (ssl) SSL_free(ssl);
    if (fd >= 0) close(fd);
    if (ctx) SSL_CTX_free(ctx);
    return rc;
}
