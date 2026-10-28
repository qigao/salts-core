/* SPDX-License-Identifier: Apache-2.0
 * TEST-ONLY independent OpenSSL 3 server and exporter oracle.
 *
 * This program links OpenSSL::SSL / OpenSSL::Crypto, NEVER Crypto::TLS or
 * GmSSL. The peer is the separately built installed Crypto C11 client.
 * Ephemeral 32-byte exporter output is written only to a 0600 file.
 */
#define _POSIX_C_SOURCE 200809L
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
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
#include <sys/time.h>
#include <unistd.h>

enum { EXPORT_LEN = 32, ACCEPT_SECONDS = 10, IO_SECONDS = 12 };

static int select_h2(SSL *ssl, const unsigned char **out,
    unsigned char *outlen, const unsigned char *input,
    unsigned int input_len, void *userdata)
{
    static const unsigned char server_protos[] = {
        2, 'h', '2', 8, 'h', 't', 't', 'p', '/', '1', '.', '1'
    };
    (void)ssl;
    (void)userdata;
    return SSL_select_next_proto((unsigned char **)out, outlen,
        server_protos, (unsigned int)sizeof(server_protos),
        input, input_len) == OPENSSL_NPN_NEGOTIATED
        ? SSL_TLSEXT_ERR_OK : SSL_TLSEXT_ERR_ALERT_FATAL;
}

static int accept_one(int port)
{
    int listener = -1, peer = -1, option = 1;
    struct sockaddr_in addr;
    fd_set set;
    struct timeval deadline = {ACCEPT_SECONDS, 0};
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return -1;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
        &option, (socklen_t)sizeof(option));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1
        || bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0
        || listen(listener, 1) != 0) goto finish;
    FD_ZERO(&set);
    FD_SET(listener, &set);
    if (select(listener + 1, &set, NULL, NULL, &deadline) != 1)
        goto finish;
    peer = accept(listener, NULL, NULL);
    if (peer >= 0) {
        struct timeval io_deadline = {IO_SECONDS, 0};
        if (setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO,
                &io_deadline, (socklen_t)sizeof(io_deadline)) != 0
            || setsockopt(peer, SOL_SOCKET, SO_SNDTIMEO,
                &io_deadline, (socklen_t)sizeof(io_deadline)) != 0) {
            close(peer);
            peer = -1;
        }
    }
finish:
    close(listener);
    return peer;
}

static int secret_to_file(const char *path, const unsigned char *bytes)
{
    static const char hexdigits[] = "0123456789ABCDEF";
    char hex[EXPORT_LEN * 2 + 1];
    int fd, saved;
    size_t i, offset = 0;
    ssize_t n;
    for (i = 0; i < EXPORT_LEN; ++i) {
        hex[i * 2] = hexdigits[bytes[i] >> 4u];
        hex[i * 2 + 1] = hexdigits[bytes[i] & 15u];
    }
    hex[EXPORT_LEN * 2] = '\n';
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return -1;
    while (offset < sizeof(hex)) {
        n = write(fd, hex + offset, sizeof(hex) - offset);
        if (n <= 0) {
            saved = errno;
            (void)close(fd);
            (void)unlink(path);
            errno = saved;
            OPENSSL_cleanse(hex, sizeof(hex));
            return -1;
        }
        offset += (size_t)n;
    }
    saved = close(fd);
    OPENSSL_cleanse(hex, sizeof(hex));
    return saved == 0 ? 0 : -1;
}

int main(int argc, char **argv)
{
    SSL_CTX *context = NULL;
    SSL *ssl = NULL;
    int peer = -1, port, version, rc = 1, n, offset;
    unsigned char exporter[EXPORT_LEN] = {0};
    unsigned char exporter_with_empty_context[EXPORT_LEN] = {0};
    unsigned int alpn_size = 0u;
    const unsigned char *alpn = NULL;
    char request[256];
    static const char reply[] =
        "HTTP/1.0 200 OK\r\nContent-Length: 2\r\n"
        "Connection: close\r\n\r\nOK";
    static const char label[] = "EXPORTER-Channel-Binding";

    if (argc != 7) return 2;
    if (strcmp(argv[1], "12") == 0) version = TLS1_2_VERSION;
    else if (strcmp(argv[1], "13") == 0) version = TLS1_3_VERSION;
    else return 2;
    port = atoi(argv[2]);
    if (port <= 0 || port > 65535
        || !argv[3][0] || !argv[4][0] || !argv[5][0] || !argv[6][0]) return 2;
    (void)signal(SIGPIPE, SIG_IGN);
    context = SSL_CTX_new(TLS_server_method());
    if (!context
        || SSL_CTX_set_min_proto_version(context, version) != 1
        || SSL_CTX_set_max_proto_version(context, version) != 1
        || SSL_CTX_use_certificate_chain_file(context, argv[3]) != 1
        || SSL_CTX_use_PrivateKey_file(context, argv[4],
                                      SSL_FILETYPE_PEM) != 1
        || SSL_CTX_check_private_key(context) != 1
        || SSL_CTX_set_cipher_list(context,
            "ECDHE-RSA-AES128-GCM-SHA256") != 1
        || SSL_CTX_set_ciphersuites(context,
            "TLS_AES_128_GCM_SHA256") != 1) goto finish;
    SSL_CTX_set_alpn_select_cb(context, select_h2, NULL);
    peer = accept_one(port);
    if (peer < 0) goto finish;
    ssl = SSL_new(context);
    if (!ssl || SSL_set_fd(ssl, peer) != 1 || SSL_accept(ssl) != 1)
        goto finish;
    SSL_get0_alpn_selected(ssl, &alpn, &alpn_size);
    if (!alpn || alpn_size != 2u || memcmp(alpn, "h2", 2u) != 0)
        goto finish;
    if (SSL_version(ssl) != version
        || SSL_export_keying_material(ssl, exporter, sizeof(exporter),
            label, sizeof(label) - 1u, NULL, 0u, 0) != 1
        || SSL_export_keying_material(ssl, exporter_with_empty_context,
            sizeof(exporter_with_empty_context), label,
            sizeof(label) - 1u, NULL, 0u, 1) != 1
        || secret_to_file(argv[5], exporter) != 0
        || secret_to_file(argv[6], exporter_with_empty_context) != 0)
        goto finish;
    /* RFC 5705 distinguishes an omitted context from a present-but-empty
     * context. TLS 1.3 RFC 8446 unconditionally hashes the empty context.
     */
    if ((version == TLS1_2_VERSION &&
         CRYPTO_memcmp(exporter, exporter_with_empty_context, EXPORT_LEN) == 0)
        || (version == TLS1_3_VERSION &&
            CRYPTO_memcmp(exporter, exporter_with_empty_context, EXPORT_LEN) != 0))
        goto finish;

    n = SSL_read(ssl, request, (int)sizeof(request) - 1);
    if (n <= 0) goto finish;
    request[n] = '\0';
    if (strncmp(request, "GET /", 5) != 0) goto finish;
    offset = 0;
    while (offset < (int)sizeof(reply) - 1) {
        n = SSL_write(ssl, reply + offset,
                      (int)sizeof(reply) - 1 - offset);
        if (n <= 0) goto finish;
        offset += n;
    }
    rc = 0;
finish:
    OPENSSL_cleanse(exporter, sizeof(exporter));
    OPENSSL_cleanse(exporter_with_empty_context, sizeof(exporter_with_empty_context));
    if (ssl) SSL_free(ssl);
    if (peer >= 0) close(peer);
    if (context) SSL_CTX_free(context);
    if (rc) {
        /* Never print session traffic, exporter, or full OpenSSL error logs. */
        fprintf(stderr, "Independent OpenSSL3 exporter server failed (TLS %d)\n",
                version);
    }
    return rc;
}
