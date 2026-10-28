/* SPDX-License-Identifier: Apache-2.0
 * Independent installed C11 SDK test. No GmSSL header or static library.
 * CNet-like bounded ciphertext feed/take + REAL TLS1.2/TLS1.3 handshake.
 */
#include <crypto/cipher_bio.h>
#include <crypto/tls_ext.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SDK_CA_FILE
#error SDK_CA_FILE
#endif
#ifndef SDK_SERVER_CERT_FILE
#error SDK_SERVER_CERT_FILE
#endif
#ifndef SDK_SERVER_KEY_FILE
#error SDK_SERVER_KEY_FILE
#endif

enum { MAX_DRIVES = 120000, PUMP_CHUNK = 193 };
#define BIO_CAP CRYPTO_TLS_CIPHER_BIO_MIN_CAPACITY

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "cipher BIO check failed line %d: %s\\n", \
                __LINE__, #expr); \
        goto fail; \
    } \
} while (0)

static int check_ring_contract(void)
{
    crypto_tls_cipher_bio *bio = NULL;
    crypto_tls_cipher_bio_snapshot snapshot;
    crypto_tls_io io;
    uint8_t *pattern = NULL, *received = NULL;
    uint8_t extra[53];
    size_t read_bytes = 0u, i;
    int rc = 1;
    pattern = (uint8_t *)malloc(BIO_CAP);
    received = (uint8_t *)malloc(BIO_CAP);
    if (!pattern || !received) goto fail;
    for (i = 0; i < BIO_CAP; ++i) pattern[i] = (uint8_t)(i*37u + 9u);
    memset(extra, 0xde, sizeof(extra));
    memset(received, 0x5a, BIO_CAP);
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.struct_size = sizeof(snapshot);
    memset(&io, 0, sizeof(io));

    CHECK(crypto_tls_cipher_bio_create(BIO_CAP - 1u, &bio)
          == CRYPTO_TLS_INVALID_ARGUMENT && !bio);
    CHECK(crypto_tls_cipher_bio_create(BIO_CAP, &bio) == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_acquire_io(bio, &io) == CRYPTO_TLS_OK);
    CHECK(io.struct_size == sizeof(io) && io.user == bio &&
          io.write && io.read);
    CHECK(crypto_tls_cipher_bio_acquire_io(bio, &io) == CRYPTO_TLS_BUSY);
    CHECK(crypto_tls_cipher_bio_destroy(&bio) == CRYPTO_TLS_BUSY && bio);
    CHECK(crypto_tls_cipher_bio_get_snapshot(bio, &snapshot) == CRYPTO_TLS_OK
        && snapshot.capacity == BIO_CAP && snapshot.input_pending == 0u
        && snapshot.output_pending == 0u);

    /* Entire inbound batch admitted or refused atomically, no partial copy. */
    CHECK(crypto_tls_cipher_bio_feed_cipher(bio, pattern, BIO_CAP)
          == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_feed_cipher(bio, extra, 1u)
          == CRYPTO_TLS_BIO_CAPACITY);
    CHECK(crypto_tls_cipher_bio_get_snapshot(bio, &snapshot) == CRYPTO_TLS_OK
        && snapshot.input_pending == BIO_CAP);
    CHECK(io.read(io.user, received, BIO_CAP - 108u) ==
          (int64_t)(BIO_CAP - 108u));
    CHECK(io.read(io.user, received + BIO_CAP - 108u, 108u) == 108);
    CHECK(memcmp(received, pattern, BIO_CAP) == 0);
    CHECK(io.read(io.user, received, 1u) == CRYPTO_TLS_IO_AGAIN);
    CHECK(crypto_tls_cipher_bio_get_snapshot(bio, &snapshot) == CRYPTO_TLS_OK
        && snapshot.input_pending == 0u);

    /* Outbound ring capacity, wraparound, no duplicate/overwritten bytes. */
    CHECK(io.write(io.user, pattern, BIO_CAP) == (int64_t)BIO_CAP);
    CHECK(io.write(io.user, extra, sizeof(extra)) == CRYPTO_TLS_IO_AGAIN);
    CHECK(crypto_tls_cipher_bio_take_cipher(
          bio, received, sizeof(extra), &read_bytes) == CRYPTO_TLS_OK
        && read_bytes == sizeof(extra));
    CHECK(memcmp(received, pattern, sizeof(extra)) == 0);
    CHECK(io.write(io.user, extra, sizeof(extra)) ==
          (int64_t)sizeof(extra));
    CHECK(crypto_tls_cipher_bio_take_cipher(
          bio, received, BIO_CAP, &read_bytes) == CRYPTO_TLS_OK
        && read_bytes == BIO_CAP);
    CHECK(memcmp(received, pattern + sizeof(extra),
                 BIO_CAP - sizeof(extra)) == 0);
    CHECK(memcmp(received + BIO_CAP - sizeof(extra),
                 extra, sizeof(extra)) == 0);
    memset(received, 0x5a, BIO_CAP);
    read_bytes = SIZE_MAX;
    CHECK(crypto_tls_cipher_bio_take_cipher(
          bio, received, BIO_CAP, &read_bytes) == CRYPTO_TLS_AGAIN &&
          read_bytes == 0u && received[0] == 0x5au);

    /* NativeIO EOF does NOT discard queued ciphertext or claim TLS close. */
    CHECK(crypto_tls_cipher_bio_feed_cipher(bio, extra, sizeof(extra))
          == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_end_input(bio) == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_feed_cipher(bio, extra, 1u)
          == CRYPTO_TLS_CLOSED);
    CHECK(io.read(io.user, received, sizeof(extra)) ==
          (int64_t)sizeof(extra));
    CHECK(memcmp(received, extra, sizeof(extra)) == 0);
    CHECK(io.read(io.user, received, 1u) == 0);
    CHECK(crypto_tls_cipher_bio_get_snapshot(bio, &snapshot) == CRYPTO_TLS_OK
          && snapshot.input_eof == 1u);

    CHECK(crypto_tls_cipher_bio_cancel(bio) == CRYPTO_TLS_OK);
    CHECK(io.read(io.user, received, 1u) == CRYPTO_TLS_IO_ERROR);
    CHECK(io.write(io.user, extra, 1u) == CRYPTO_TLS_IO_ERROR);
    CHECK(crypto_tls_cipher_bio_feed_cipher(bio, extra, 1u)
          == CRYPTO_TLS_CANCELLED);
    CHECK(crypto_tls_cipher_bio_destroy(&bio) == CRYPTO_TLS_BUSY);
    CHECK(crypto_tls_cipher_bio_release_io(bio) == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_destroy(&bio) == CRYPTO_TLS_OK && !bio);
    rc = 0;
fail:
    if (bio) {
        (void)crypto_tls_cipher_bio_release_io(bio);
        (void)crypto_tls_cipher_bio_destroy(&bio);
    }
    free(pattern);
    free(received);
    return rc;
}

static int pump_cipher(crypto_tls_cipher_bio *source,
                       crypto_tls_cipher_bio *destination,
                       size_t *moved)
{
    crypto_tls_cipher_bio_snapshot from, to;
    uint8_t buffer[PUMP_CHUNK];
    size_t available, count, n;
    crypto_tls_result result;
    memset(&from, 0, sizeof(from));
    memset(&to, 0, sizeof(to));
    from.struct_size = sizeof(from);
    to.struct_size = sizeof(to);
    if (crypto_tls_cipher_bio_get_snapshot(source, &from) != CRYPTO_TLS_OK
        || crypto_tls_cipher_bio_get_snapshot(destination, &to)
          != CRYPTO_TLS_OK || from.output_pending > from.capacity
        || to.input_pending > to.capacity)
        return -1;
    available = to.capacity - to.input_pending;
    count = from.output_pending < available ? from.output_pending : available;
    if (count > PUMP_CHUNK) count = PUMP_CHUNK;
    if (!count) return 0;
    n = 0u;
    result = crypto_tls_cipher_bio_take_cipher(
        source, buffer, count, &n);
    if (result != CRYPTO_TLS_OK || n != count
        || crypto_tls_cipher_bio_feed_cipher(destination, buffer, n)
           != CRYPTO_TLS_OK)
        return -1;
    *moved += n;
    return 1;
}

static int qualify_real_session(uint32_t protocol)
{
    crypto_tls_provider *provider = NULL;
    crypto_tls_cipher_bio *client_bio = NULL, *server_bio = NULL;
    crypto_tls_session *client = NULL, *server = NULL;
    crypto_tls_io cio = {0}, sio = {0};
    crypto_tls_session_options client_opts = {
        sizeof(client_opts), protocol, CRYPTO_TLS_CLIENT, 4u,
        "localhost", SDK_CA_FILE, NULL, NULL, NULL, 0u
    };
    crypto_tls_session_options server_opts = {
        sizeof(server_opts), protocol, CRYPTO_TLS_SERVER, 4u,
        NULL, NULL, SDK_SERVER_CERT_FILE, SDK_SERVER_KEY_FILE, "", 0u
    };
    static const uint8_t message[] =
        "CNet NativeIO bounded ciphertext BIO: exact plaintext acceptance";
    uint8_t received[sizeof(message)] = {0};
    crypto_tls_session_info_v1_1 info = {0};
    crypto_tls_cipher_bio_snapshot snapshot = {0};
    size_t sent = 0u, length = 0u, delivered = 0u, moved = 0u, i;
    int client_done = 0, server_done = 0, write_done = 0;
    int client_notify = 0, server_notify = 0;
    crypto_tls_result code;
    int rc = 1, client_io_leased = 0, server_io_leased = 0;

    CHECK(crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider)
          == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_create(BIO_CAP, &client_bio) == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_create(BIO_CAP, &server_bio) == CRYPTO_TLS_OK);
    CHECK(crypto_tls_cipher_bio_acquire_io(client_bio, &cio) == CRYPTO_TLS_OK);
    client_io_leased = 1;
    CHECK(crypto_tls_cipher_bio_acquire_io(server_bio, &sio) == CRYPTO_TLS_OK);
    server_io_leased = 1;
    CHECK(crypto_tls_session_create(provider, &client_opts, &cio, &client)
          == CRYPTO_TLS_OK);
    CHECK(crypto_tls_session_create(provider, &server_opts, &sio, &server)
          == CRYPTO_TLS_OK);

    for (i = 0u; i < MAX_DRIVES; ++i) {
        if (!client_done) {
            code = crypto_tls_session_handshake(client);
            CHECK(code == CRYPTO_TLS_OK || code == CRYPTO_TLS_AGAIN);
            client_done = code == CRYPTO_TLS_OK;
        }
        if (!server_done) {
            code = crypto_tls_session_handshake(server);
            CHECK(code == CRYPTO_TLS_OK || code == CRYPTO_TLS_AGAIN);
            server_done = code == CRYPTO_TLS_OK;
        }
        CHECK(pump_cipher(client_bio, server_bio, &moved) >= 0);
        CHECK(pump_cipher(server_bio, client_bio, &moved) >= 0);
        if (client_done && server_done) break;
    }
    CHECK(client_done && server_done && i < MAX_DRIVES && moved > 0u);

    info.struct_size = sizeof(info);
    CHECK(crypto_tls_session_get_info_v1_1(client, &info) == CRYPTO_TLS_OK
          && info.protocol == protocol && info.cipher_suite != 0u);
    snapshot.struct_size = sizeof(snapshot);
    CHECK(crypto_tls_cipher_bio_get_snapshot(client_bio, &snapshot)
          == CRYPTO_TLS_OK && snapshot.capacity == BIO_CAP);

    for (i = 0u; i < MAX_DRIVES; ++i) {
        if (!write_done) {
            sent = 0u;
            code = crypto_tls_session_send(
                client, message, sizeof(message), &sent);
            CHECK(code == CRYPTO_TLS_OK || code == CRYPTO_TLS_AGAIN);
            if (code == CRYPTO_TLS_OK) {
                CHECK(sent == sizeof(message));
                write_done = 1;
            }
        }
        CHECK(pump_cipher(client_bio, server_bio, &moved) >= 0);
        CHECK(pump_cipher(server_bio, client_bio, &moved) >= 0);
        if (length < sizeof(received)) {
            delivered = 0u;
            code = crypto_tls_session_receive(
                server, received + length, sizeof(received) - length,
                &delivered);
            CHECK(code == CRYPTO_TLS_OK || code == CRYPTO_TLS_AGAIN);
            if (code == CRYPTO_TLS_OK) {
                CHECK(delivered > 0u && delivered <= sizeof(received)-length);
                length += delivered;
            }
        }
        if (write_done && length == sizeof(received)) break;
    }
    CHECK(write_done && length == sizeof(message)
          && memcmp(message, received, sizeof(message)) == 0);

    /* Drain close_notify using the *same* bounded encrypted rings. */
    for (i = 0u; i < MAX_DRIVES; ++i) {
        if (!client_notify) {
            code = crypto_tls_session_shutdown(client);
            CHECK(code == CRYPTO_TLS_OK || code == CRYPTO_TLS_AGAIN);
            client_notify = code == CRYPTO_TLS_OK;
        }
        if (!server_notify) {
            code = crypto_tls_session_shutdown(server);
            CHECK(code == CRYPTO_TLS_OK || code == CRYPTO_TLS_AGAIN);
            server_notify = code == CRYPTO_TLS_OK;
        }
        CHECK(pump_cipher(client_bio, server_bio, &moved) >= 0);
        CHECK(pump_cipher(server_bio, client_bio, &moved) >= 0);
        if (client_notify && server_notify) break;
    }
    CHECK(client_notify && server_notify);

    /* BIO input/output ownership cannot settle before its TLS session. */
    CHECK(crypto_tls_cipher_bio_destroy(&client_bio) == CRYPTO_TLS_BUSY);
    CHECK(crypto_tls_cipher_bio_destroy(&server_bio) == CRYPTO_TLS_BUSY);
    rc = 0;
fail:
    if (client && crypto_tls_session_destroy(&client) != CRYPTO_TLS_OK)
        rc = 1;
    if (server && crypto_tls_session_destroy(&server) != CRYPTO_TLS_OK)
        rc = 1;
    if (client_io_leased && client_bio &&
        crypto_tls_cipher_bio_release_io(client_bio) != CRYPTO_TLS_OK)
        rc = 1;
    if (server_io_leased && server_bio &&
        crypto_tls_cipher_bio_release_io(server_bio) != CRYPTO_TLS_OK)
        rc = 1;
    if (client_bio &&
        crypto_tls_cipher_bio_destroy(&client_bio) != CRYPTO_TLS_OK)
        rc = 1;
    if (server_bio &&
        crypto_tls_cipher_bio_destroy(&server_bio) != CRYPTO_TLS_OK)
        rc = 1;
    if (provider && crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK)
        rc = 1;
    if (rc)
        fprintf(stderr,
            "Crypto C11 cipher BIO real TLS0x%04x failed"
            " handshake=%d/%d write=%d read=%zu/%zu"
            " close=%d/%d moved=%zu\\n",
            protocol, client_done, server_done, write_done,
            length, sizeof(message), client_notify, server_notify, moved);
    return rc;
}

int main(void)
{
    if (check_ring_contract() != 0
        || qualify_real_session(CRYPTO_TLS_1_2) != 0
        || qualify_real_session(CRYPTO_TLS_1_3) != 0)
        return 1;
    puts("Crypto Installed C11 CipherBIO: bounded feed/take, wrap, EOF, "
         "cancel, lease and true TLS 1.2/1.3: PASS");
    return 0;
}
