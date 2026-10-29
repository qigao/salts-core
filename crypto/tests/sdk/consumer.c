/* Installed SDK consumer. NO GmSSL header, linker target, or source access. */
#include <crypto/tls.h>
#include <crypto/tls_ext.h>
#include <crypto/tls_peer.h>

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

enum { PIPE_LIMIT = 1024, MAX_DRIVES = 200000 };
typedef struct pipe {
    uint8_t data[PIPE_LIMIT];
    size_t length;
} pipe;
typedef struct channel {
    pipe *send_to;
    pipe *receive_from;
    size_t partial_send;
    size_t partial_recv;
    size_t retry_send;
    size_t retry_recv;
} channel;

static size_t min_size(size_t a, size_t b) { return a < b ? a : b; }

static int64_t on_send(void *user, const uint8_t *data, size_t len)
{
    channel *c = (channel *)user;
    size_t space = PIPE_LIMIT - c->send_to->length;
    size_t n;
    if (!space) {
        c->retry_send++;
        return CRYPTO_TLS_IO_AGAIN;
    }
    n = min_size(len, min_size(space, 73u));
    if (n < len) c->partial_send++;
    memcpy(c->send_to->data + c->send_to->length, data, n);
    c->send_to->length += n;
    return (int64_t)n;
}

static int64_t on_recv(void *user, uint8_t *data, size_t len)
{
    channel *c = (channel *)user;
    size_t n = min_size(len, min_size(41u, c->receive_from->length));
    if (!n) {
        c->retry_recv++;
        return CRYPTO_TLS_IO_AGAIN;
    }
    if (n < len) c->partial_recv++;
    memcpy(data, c->receive_from->data, n);
    c->receive_from->length -= n;
    if (c->receive_from->length)
        memmove(c->receive_from->data,
                c->receive_from->data + n, c->receive_from->length);
    return (int64_t)n;
}

static int acceptable(crypto_tls_result r)
{
    return r == CRYPTO_TLS_OK || r == CRYPTO_TLS_AGAIN;
}

/* DER oracle is the SHA-256 of the frozen public
 * tests/fixtures/external-io-server-cert.pem leaf.
 * Generated independently from PEM base64 -> DER -> SHA-256.
 */
static const uint8_t FROZEN_RSA_LEAF_SHA256[32] = {
    0x84,0x12,0xcb,0xa2,0xb9,0xac,0x94,0xa9,
    0x36,0x2f,0xb6,0xb5,0x8d,0x77,0x4c,0xd8,
    0x72,0x7b,0x8d,0x67,0x0b,0xb9,0xce,0xc2,
    0x1e,0xa8,0x4f,0xf5,0x9b,0xb7,0x95,0x1a
};

static int validate_peer_and_exporter(crypto_tls_session *client,
                                      crypto_tls_session *server,
                                      uint32_t protocol)
{
    uint8_t chain[CRYPTO_TLS_PEER_CHAIN_MAX_BYTES];
    uint8_t sha[CRYPTO_TLS_PEER_SHA256_BYTES];
    uint8_t absent[CRYPTO_TLS_PEER_SHA256_BYTES];
    uint8_t binding_client[32], binding_server[32];
    uint8_t repeat[32], no_context[32], changed_context[32], changed_label[32];
    uint8_t untouched[CRYPTO_TLS_EXPORTER_MAX_BYTES + 1u];
    static const char label[] = "EXPORTER-Channel-Binding";
    static const char different_label[] = "EXPORTER-Other-Channel-Binding";
    static const uint8_t ctx[] = {'a','b','c'};
    size_t required = 0u, copied = 0u, i;

    if (crypto_tls_session_copy_peer_chain_v1_2(client, NULL, 0u, &required)
            != CRYPTO_TLS_OK
        || !required || required > sizeof(chain))
        return __LINE__;
    memset(chain, 0xa5, sizeof(chain));
    copied = 4u;
    if (crypto_tls_session_copy_peer_chain_v1_2(
            client, chain, required-1u, &copied)
            != CRYPTO_TLS_PEER_BUFFER_TOO_SMALL || copied != required)
        return __LINE__;
    for (i = 0u; i < sizeof(chain); ++i)
        if (chain[i] != 0xa5u) return __LINE__;
    if (crypto_tls_session_copy_peer_chain_v1_2(
            client, chain, sizeof(chain), &copied) != CRYPTO_TLS_OK
        || copied != required || chain[0] != 0x30u)
        return __LINE__;

    if (crypto_tls_session_peer_sha256_v1_2(client, sha) != CRYPTO_TLS_OK
        || memcmp(sha, FROZEN_RSA_LEAF_SHA256, sizeof(sha)) != 0)
        return __LINE__;

    /* The server's TLS connection is configured not to authenticate clients.
     * Absent peer cert is distinct from a transport EOF or a valid fingerprint.
     */
    memset(absent, 0xa5, sizeof(absent));
    copied = 0xa5u;
    if (crypto_tls_session_peer_sha256_v1_2(server, absent)
            != CRYPTO_TLS_PEER_CERT_ABSENT
        || crypto_tls_session_copy_peer_chain_v1_2(
            server, NULL, 0u, &copied) != CRYPTO_TLS_PEER_CERT_ABSENT
        || copied != 0xa5u)
        return __LINE__;
    for (i = 0u; i < sizeof(absent); ++i)
        if (absent[i] != 0xa5u) return __LINE__;

    /* The same real TLS transcript and key schedule must agree on both
     * sides; this is not a mocked exporter or a duplicated OpenSSL backend.
     */
    if (crypto_tls_session_export_v1_2(
            client, label, NULL, 0u, 1u, binding_client, sizeof(binding_client))
                != CRYPTO_TLS_OK
        || crypto_tls_session_export_v1_2(
            server, label, NULL, 0u, 1u, binding_server, sizeof(binding_server))
                != CRYPTO_TLS_OK
        || memcmp(binding_client, binding_server, sizeof(binding_client)) != 0
        || crypto_tls_session_export_v1_2(
            client, label, NULL, 0u, 1u, repeat, sizeof(repeat))
                != CRYPTO_TLS_OK
        || memcmp(binding_client, repeat, sizeof(repeat)) != 0)
        return __LINE__;
    for (i = 0u; i < sizeof(binding_client); ++i)
        if (binding_client[i] != 0u) break;
    if (i == sizeof(binding_client)) return __LINE__;

    if (crypto_tls_session_export_v1_2(
            client, label, NULL, 0u, 0u, no_context, sizeof(no_context))
                != CRYPTO_TLS_OK
        || crypto_tls_session_export_v1_2(
            client, label, ctx, sizeof(ctx), 1u,
            changed_context, sizeof(changed_context)) != CRYPTO_TLS_OK
        || crypto_tls_session_export_v1_2(
            client, different_label, NULL, 0u, 1u,
            changed_label, sizeof(changed_label)) != CRYPTO_TLS_OK)
        return __LINE__;
    if (memcmp(binding_client, changed_context, sizeof(binding_client)) == 0
        || memcmp(binding_client, changed_label, sizeof(binding_client)) == 0)
        return __LINE__;
    if ((protocol == CRYPTO_TLS_1_2 &&
         memcmp(binding_client, no_context, sizeof(binding_client)) == 0)
        || (protocol == CRYPTO_TLS_1_3 &&
         memcmp(binding_client, no_context, sizeof(binding_client)) != 0))
        return __LINE__;

    /* Invalid requests MUST NOT alter any caller-owned output bytes. */
    memset(untouched, 0xa5, sizeof(untouched));
    if (crypto_tls_session_export_v1_2(
            client, "", NULL, 0u, 1u, untouched, 32u)
                != CRYPTO_TLS_INVALID_ARGUMENT
        || crypto_tls_session_export_v1_2(
            client, label, NULL, 0u, 2u, untouched, 32u)
                != CRYPTO_TLS_INVALID_ARGUMENT
        || crypto_tls_session_export_v1_2(
            client, label, NULL, 1u, 1u, untouched, 32u)
                != CRYPTO_TLS_INVALID_ARGUMENT
        || crypto_tls_session_export_v1_2(
            client, label, NULL, 0u, 1u,
            untouched, CRYPTO_TLS_EXPORTER_MAX_BYTES+1u)
                != CRYPTO_TLS_INVALID_ARGUMENT)
        return __LINE__;
    for (i = 0u; i < sizeof(untouched); ++i)
        if (untouched[i] != 0xa5u) return __LINE__;
    return 0;
}

static int test_protocol(crypto_tls_provider *provider, uint32_t protocol,
                         int configure_alpn)
{
    pipe cs = {{0},0u}, sc = {{0},0u};
    channel client_channel = {&cs,&sc,0,0,0,0};
    channel server_channel = {&sc,&cs,0,0,0,0};
    crypto_tls_io ci = {sizeof(ci),0,&client_channel,on_send,on_recv};
    crypto_tls_io si = {sizeof(si),0,&server_channel,on_send,on_recv};
    crypto_tls_session_options client_options = {
        sizeof(client_options),protocol,CRYPTO_TLS_CLIENT,4,
        "localhost",SDK_CA_FILE,NULL,NULL,NULL,0
    };
    crypto_tls_session_options server_options = {
        sizeof(server_options),protocol,CRYPTO_TLS_SERVER,4,
        NULL,NULL,SDK_SERVER_CERT_FILE,SDK_SERVER_KEY_FILE,"",0
    };
    crypto_tls_session *client = NULL, *server = NULL;
    char client_http[] = "http/1.1", client_h2[] = "h2";
    char server_http[] = "http/1.1", server_h2[] = "h2";
    const char *client_names[] = {client_http, client_h2};
    const char *server_names[] = {server_h2, server_http};
    crypto_tls_session_info_v1_1 client_info, server_info, untouched;
    uint8_t pre_sha[CRYPTO_TLS_PEER_SHA256_BYTES];
    uint8_t pre_material[32];
    uint8_t pre_chain[16];
    size_t pre_chain_len = 0x5a5au, pre_i;
    static const char *invalid_names[] = {"", "h2"};
    char too_long_name[CRYPTO_TLS_ALPN_MAX_NAME_BYTES + 2u];
    const char *too_long[] = {too_long_name};
    uint32_t sessions = 0;
    const uint8_t sent[] = "Crypto shared C11 TLS opaque session real record";
    uint8_t received[sizeof(sent)] = {0};
    size_t received_count = 0;
    size_t sent_count = 0;
    int client_ready = 0, server_ready = 0, sent_done = 0;
    int client_closed = 0, server_closed = 0;
    int step = 0, rc = 1;
    crypto_tls_result result;

    if (crypto_tls_session_create(provider,&client_options,&ci,&client) != CRYPTO_TLS_OK
        || crypto_tls_session_create(provider,&server_options,&si,&server) != CRYPTO_TLS_OK)
        goto cleanup;
    if (crypto_tls_provider_session_count(provider,&sessions) != CRYPTO_TLS_OK
        || sessions != 2) goto cleanup;

    memset(pre_sha, 0xa5, sizeof(pre_sha));
    memset(pre_material, 0xa5, sizeof(pre_material));
    memset(pre_chain, 0xa5, sizeof(pre_chain));
    if (crypto_tls_session_peer_sha256_v1_2(client, pre_sha) != CRYPTO_TLS_AGAIN
        || crypto_tls_session_copy_peer_chain_v1_2(
            client, pre_chain, sizeof(pre_chain), &pre_chain_len)
            != CRYPTO_TLS_AGAIN
        || pre_chain_len != 0x5a5au
        || crypto_tls_session_export_v1_2(
            client, "EXPORTER-Channel-Binding", NULL, 0u, 1u,
            pre_material, sizeof(pre_material)) != CRYPTO_TLS_AGAIN)
        goto cleanup;
    for (pre_i = 0u; pre_i < sizeof(pre_material); ++pre_i)
        if (pre_material[pre_i] != 0xa5u || pre_sha[pre_i] != 0xa5u)
            goto cleanup;
    for (pre_i = 0u; pre_i < sizeof(pre_chain); ++pre_i)
        if (pre_chain[pre_i] != 0xa5u) goto cleanup;

    memset(&client_info, 0xA5, sizeof(client_info));
    client_info.struct_size = sizeof(client_info);
    untouched = client_info;
    if (crypto_tls_session_get_info_v1_1(client, &client_info)
            != CRYPTO_TLS_AGAIN
        || memcmp(&client_info, &untouched, sizeof(client_info)) != 0)
        goto cleanup;

    if (configure_alpn) {
        memset(too_long_name, 'a', sizeof(too_long_name));
        too_long_name[sizeof(too_long_name)-1] = '\0';
        if (crypto_tls_session_set_alpn_v1_1(client, invalid_names, 2)
                != CRYPTO_TLS_INVALID_ARGUMENT
            || crypto_tls_session_set_alpn_v1_1(client, too_long, 1)
                != CRYPTO_TLS_INVALID_ARGUMENT
            || crypto_tls_session_set_alpn_v1_1(client, client_names, 2)
                != CRYPTO_TLS_OK
            || crypto_tls_session_set_alpn_v1_1(server, server_names, 2)
                != CRYPTO_TLS_OK
            || crypto_tls_session_set_alpn_v1_1(client, client_names, 2)
                != CRYPTO_TLS_INVALID_ARGUMENT)
            goto cleanup;

        /* The native TLS_CTX must use session-owned deep copies, not the
         * caller's now-corrupted stack buffers and char** list.
         */
        memset(client_http, 'x', strlen(client_http));
        memset(client_h2, 'y', strlen(client_h2));
        memset(server_http, 'z', strlen(server_http));
        memset(server_h2, 'w', strlen(server_h2));
        client_names[0] = NULL;
        client_names[1] = NULL;
        server_names[0] = NULL;
        server_names[1] = NULL;
    }

    {
        crypto_tls_provider *borrowed = provider;
        if (crypto_tls_provider_destroy(&borrowed) != CRYPTO_TLS_BUSY
            || borrowed != provider) goto cleanup;
    }
    for (step=0;step<MAX_DRIVES;step++) {
        if (!client_ready) {
            result = crypto_tls_session_handshake(client);
            if (!acceptable(result)) goto cleanup;
            client_ready = result == CRYPTO_TLS_OK;
        }
        if (!server_ready) {
            result = crypto_tls_session_handshake(server);
            if (!acceptable(result)) goto cleanup;
            server_ready = result == CRYPTO_TLS_OK;
        }
        if (client_ready && server_ready) break;
    }
    if (!client_ready || !server_ready) goto cleanup;

    client_info.struct_size = sizeof(client_info);
    memset(&server_info, 0, sizeof(server_info));
    server_info.struct_size = sizeof(server_info);
    if (crypto_tls_session_get_info_v1_1(client, &client_info) != CRYPTO_TLS_OK
        || crypto_tls_session_get_info_v1_1(server, &server_info)
            != CRYPTO_TLS_OK
        || client_info.protocol != protocol
        || server_info.protocol != protocol
        || !client_info.cipher_suite || !server_info.cipher_suite
        || client_info.cipher_suite != server_info.cipher_suite
        || !client_info.cipher_name[0] || !server_info.cipher_name[0]
        || strcmp(client_info.cipher_name, server_info.cipher_name) != 0
        || crypto_tls_session_set_alpn_v1_1(client, client_names, 2)
            != CRYPTO_TLS_INVALID_ARGUMENT)
        goto cleanup;
    if (configure_alpn) {
        if (client_info.alpn_size != 2u || server_info.alpn_size != 2u
            || strcmp(client_info.alpn, "h2") != 0
            || strcmp(server_info.alpn, "h2") != 0)
            goto cleanup;
    } else if (client_info.alpn_size || server_info.alpn_size
               || client_info.alpn[0] || server_info.alpn[0])
        goto cleanup;

    if (validate_peer_and_exporter(client,server,protocol) != 0)
        goto cleanup;

    for (step=0;step<MAX_DRIVES;step++) {
        if (!sent_done) {
            result = crypto_tls_session_send(client,sent,sizeof(sent),&sent_count);
            if (!acceptable(result)) goto cleanup;
            if (result == CRYPTO_TLS_OK) {
                if (sent_count != sizeof(sent)) goto cleanup;
                sent_done = 1;
            }
        }
        if (received_count < sizeof(received)) {
            size_t count = 0;
            result = crypto_tls_session_receive(server,
                received + received_count,
                sizeof(received) - received_count,&count);
            if (!acceptable(result)) goto cleanup;
            if (result == CRYPTO_TLS_OK) {
                if (!count || count > sizeof(received)-received_count)
                    goto cleanup;
                received_count += count;
            }
        }
        if (sent_done && received_count == sizeof(received)) break;
    }
    if (!sent_done || received_count != sizeof(received)
        || memcmp(sent,received,sizeof(sent)) != 0)
        goto cleanup;
    for (step=0;step<MAX_DRIVES;step++) {
        if (!client_closed) {
            result = crypto_tls_session_shutdown(client);
            if (!acceptable(result)) goto cleanup;
            client_closed = result == CRYPTO_TLS_OK;
        }
        if (!server_closed) {
            result = crypto_tls_session_shutdown(server);
            if (!acceptable(result)) goto cleanup;
            server_closed = result == CRYPTO_TLS_OK;
        }
        if (client_closed && server_closed) break;
    }
    if (!client_closed || !server_closed) goto cleanup;
    if (!client_channel.partial_send || !client_channel.partial_recv
        || !server_channel.partial_send || !server_channel.partial_recv
        || !client_channel.retry_recv || !server_channel.retry_recv
        || cs.length || sc.length)
        goto cleanup;
    rc = 0;
cleanup:
    if (server && crypto_tls_session_destroy(&server) != CRYPTO_TLS_OK) rc = 1;
    if (client && crypto_tls_session_destroy(&client) != CRYPTO_TLS_OK) rc = 1;
    if (rc) fprintf(stderr,"Crypto shared TLS 0x%04x smoke FAILED step=%d handshake=%d/%d sent=%d recv=%zu close=%d/%d\n",
        protocol,step,client_ready,server_ready,sent_done,received_count,client_closed,server_closed);
    return rc;
}

int main(void)
{
    crypto_tls_provider *provider = NULL;
    crypto_tls_session *s = NULL;
    crypto_tls_session_options bad = {sizeof(bad),CRYPTO_TLS_1_3,CRYPTO_TLS_CLIENT,0,
                                     "localhost",NULL,NULL,NULL,NULL,0};
    pipe out = {{0},0}, in = {{0},0};
    channel ch = {&out,&in,0,0,0,0};
    crypto_tls_io io = {sizeof(io),0,&ch,on_send,on_recv};
    uint32_t count = UINT32_MAX;
    int rc = 1;
    uint64_t features = 0u;
    uint64_t peer_features = 0u;
    const uint64_t required_peer_features =
        CRYPTO_TLS_PEER_FEATURE_DER_CHAIN_COPY |
        CRYPTO_TLS_PEER_FEATURE_CERT_SHA256 |
        CRYPTO_TLS_PEER_FEATURE_TLS_EXPORTER;
    const uint64_t required_features =
        CRYPTO_TLS_FEATURE_TLS12_ECDHE_RSA |
        CRYPTO_TLS_FEATURE_TLS13_RSA_PSS |
        CRYPTO_TLS_FEATURE_EXPLICIT_CA_FILE |
        CRYPTO_TLS_FEATURE_DNS_SAN |
        CRYPTO_TLS_FEATURE_TEXT_ALPN |
        CRYPTO_TLS_FEATURE_SESSION_INFO;

    if (crypto_tls_peer_abi_version() != CRYPTO_TLS_PEER_ABI_VERSION
        || crypto_tls_peer_features_v1_2(&peer_features) != CRYPTO_TLS_OK
        || peer_features != required_peer_features
        || crypto_tls_peer_features_v1_2(NULL) != CRYPTO_TLS_INVALID_ARGUMENT)
        return 1;
    if (crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION
        || crypto_tls_extension_abi_version()
            != CRYPTO_TLS_EXTENSION_ABI_VERSION
        || crypto_tls_get_features_v1_1(&features) != CRYPTO_TLS_OK
        || features != required_features
        || crypto_tls_get_features_v1_1(NULL) != CRYPTO_TLS_INVALID_ARGUMENT)
        return 1;
    if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION+1,&provider)
        != CRYPTO_TLS_INVALID_ARGUMENT || provider) return 2;
    if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION,&provider)
        != CRYPTO_TLS_OK || !provider) return 3;
    if (crypto_tls_session_create(provider,&bad,&io,&s)
        != CRYPTO_TLS_INVALID_ARGUMENT || s) goto done;

    if (test_protocol(provider,CRYPTO_TLS_1_2,0) != 0
        || test_protocol(provider,CRYPTO_TLS_1_3,0) != 0
        || test_protocol(provider,CRYPTO_TLS_1_2,1) != 0
        || test_protocol(provider,CRYPTO_TLS_1_3,1) != 0)
        goto done;

    bad.ca_file = SDK_CA_FILE;
    if (crypto_tls_session_create(provider,&bad,&io,&s) != CRYPTO_TLS_OK)
        goto done;
    if (crypto_tls_session_cancel(s) != CRYPTO_TLS_OK
        || crypto_tls_session_handshake(s) != CRYPTO_TLS_CANCELLED
        || crypto_tls_session_destroy(&s) != CRYPTO_TLS_OK || s)
        goto done;

    if (crypto_tls_provider_session_count(provider,&count) != CRYPTO_TLS_OK || count)
        goto done;
    rc = 0;
done:
    if (s) (void)crypto_tls_session_destroy(&s);
    if (provider && crypto_tls_provider_destroy(&provider) != CRYPTO_TLS_OK)
        rc = 1;
    if (!rc) puts("Crypto installed C11 TLS1.2/1.3 ALPN, DER chain, SHA256, real exporter, record I/O and shutdown: PASS");
    return rc;
}
