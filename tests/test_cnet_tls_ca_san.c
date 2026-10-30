/*
 * Public CNet/NativeIO real-TCP TLS acceptance and negative trust gate.
 * Certificate fixtures are exact copies of original Salts TLS conformance
 * corpus. No TLS implementation/type is accessed directly in this file.
 */
#include <cnet/cnet.h>
#include <salts/clock.h>
#include <salts/error_codes.h>
#include <cmeta_buffer.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef CORE_TLS_TEST_P384_CA
#error CORE_TLS_TEST_P384_CA is required
#endif
#ifndef CORE_TLS_TEST_P384_WRONG_CA
#error CORE_TLS_TEST_P384_WRONG_CA is required
#endif
#ifndef CORE_TLS_TEST_P384_LEAF
#error CORE_TLS_TEST_P384_LEAF is required
#endif
#ifndef CORE_TLS_TEST_P384_KEY
#error CORE_TLS_TEST_P384_KEY is required
#endif

typedef struct tls_probe {
  cnet_client *client;
  cnet_connection connection;
  unsigned connected_count;
  unsigned terminal_count;
  unsigned failed;
  unsigned sent_count;
  unsigned received_count;
  unsigned char received_byte;
  int tls_version_status;
  int tls_cipher_status;
  char tls_version[24];
  char tls_cipher[128];
  char failure_stage[32];
} tls_probe;

static void state_callback(void *user, cnet_connection connection,
                           cnet_connection_state state, const cnet_error *error) {
  tls_probe *p = (tls_probe *)user;
  p->connection = connection;
  if (state == CNET_CONNECTION_CONNECTED) {
    ++p->connected_count;
    size_t version_size = 0u, cipher_size = 0u;
    p->tls_version_status = cnet_tls_negotiated_version(
        p->client, connection, p->tls_version, sizeof(p->tls_version),
        &version_size);
    p->tls_cipher_status = cnet_tls_negotiated_cipher(
        p->client, connection, p->tls_cipher, sizeof(p->tls_cipher),
        &cipher_size);
  } else if (state == CNET_CONNECTION_FAILED || state == CNET_CONNECTION_CLOSED) {
    ++p->terminal_count;
    if (state == CNET_CONNECTION_FAILED || error != NULL) {
      ++p->failed;
      if (error && error->stage) {
        const size_t n = strlen(error->stage);
        if (n >= sizeof(p->failure_stage)) {
          ++p->failed;
        } else {
          memcpy(p->failure_stage, error->stage, n + 1u);
        }
      }
    }
  }
}
static void receive_callback(void *user, cnet_connection connection,
                             const cnet_receive_view *view) {
  tls_probe *p = (tls_probe *)user;
  (void)connection;
  if (view == NULL || view->kind != CNET_MESSAGE_BYTES ||
      view->size != 1u || view->data == NULL || p->received_count != 0u) {
    ++p->failed;
    return;
  }
  p->received_byte = *(const unsigned char *)view->data;
  ++p->received_count;
}
static void send_callback(void *user, cnet_connection connection, size_t size) {
  tls_probe *p = (tls_probe *)user;
  (void)connection;
  if (size != 1u) ++p->failed;
  ++p->sent_count;
}

static cnet_client_config config(void) {
  cnet_client_config result = {0};
#if defined(_WIN32)
  result.backend = NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
  result.backend = NATIVE_IO_BACKEND_KQUEUE;
#else
  result.backend = NATIVE_IO_BACKEND_EPOLL;
#endif
  result.connection_capacity = 2u;
  result.command_capacity = 16u;
  result.request_capacity = 8u;
  result.completion_batch_capacity = 8u;
  result.event_capacity = 16u;
  result.max_send_bytes = 1024u;
  result.receive_buffer_bytes = 1024u;
  result.connect_timeout_ms = 2000u;
  result.read_timeout_ms = 2000u;
  result.write_timeout_ms = 2000u;
  result.tls_io_buffer_bytes = CNET_TLS_MIN_IO_BUFFER_BYTES;
  result.tls_handshake_timeout_ms = 2500u;
  return result;
}

static int drive(cnet_client *client, cnet_client *server,
                 cnet_listener *listener, cnet_tls_server *context,
                 tls_probe *server_probe, bool *accepted) {
  size_t events = 0u;
  int ready = 0;
  int status = cnet_client_poll(client, 1u, &events);
  if (status != SALTS_OK) return status;
  if (!*accepted) {
    status = cnet_listener_wait(listener, 0u, &ready);
    if (status != SALTS_OK) return status;
    if (ready) {
      const cnet_observer observer = {
          state_callback, receive_callback, server_probe, send_callback};
      status = cnet_listener_accept_tls(
          listener, server, context, &observer, &server_probe->connection);
      if (status != SALTS_OK) return status;
      *accepted = true;
    }
  }
  return cnet_client_poll(server, 1u, &events);
}

#define REQUIRE(expr) do {                                                    \
  if (!(expr)) {                                                              \
    fprintf(stderr, "CNet TLS case %s failed at %d: %s\n",                    \
            label, __LINE__, #expr);                                          \
    result = __LINE__;                                                        \
    goto cleanup;                                                             \
  }                                                                           \
} while (0)

typedef struct tls_case_options {
  const char *label;
  const char *ca_file;
  const char *verified_hostname;
  bool expect_success;
  bool expect_server_rejection;
  const char *server_ca_file;
  const char *server_cert_file;
  const char *server_key_file;
  const char *client_cert_file;
  const char *client_key_file;
} tls_case_options;

static int run_case(const tls_case_options *options) {
  const char *label = options->label;
  const bool should_verify = options->expect_success;
  cnet_client client = {0};
  cnet_client server = {0};
  cnet_listener listener = {0};
  cnet_tls_server tls_server = {0};
  const cnet_client_config client_cfg = config();
  const cnet_client_config server_cfg = config();
  const cnet_listener_config listener_cfg = {
      client_cfg.backend, "127.0.0.1", 0u, 2u};
  const cnet_tls_server_config server_tls_cfg = {
      .size = sizeof(cnet_tls_server_config),
      .cert_file = options->server_cert_file != NULL
                       ? options->server_cert_file : CORE_TLS_TEST_P384_LEAF,
      .key_file = options->server_key_file != NULL
                      ? options->server_key_file : CORE_TLS_TEST_P384_KEY,
      .ca_file = options->server_ca_file,
      .client_auth = options->server_ca_file != NULL
                         ? CNET_TLS_CLIENT_AUTH_REQUIRED
                         : CNET_TLS_CLIENT_AUTH_NONE};
  const cnet_tls_client_config client_tls_cfg = {
      .size = sizeof(cnet_tls_client_config),
      .ca_file = options->ca_file,
      .cert_file = options->client_cert_file,
      .key_file = options->client_key_file,
      .server_name = options->verified_hostname};
  tls_probe client_probe = {0}, server_probe = {0};
  cnet_connect_options connect_options = {0};
  cnet_connection outbound = {0};
  bool tls_live = false, client_live = false;
  bool server_live = false, listener_live = false, accepted = false;
  char uri[80] = {0};
  uint16_t port = 0u;
  uint64_t deadline;
  int result = 0;
  mem_buffer_t *outgoing = NULL;

  client_probe.client = &client;
  server_probe.client = &server;
  REQUIRE(cnet_tls_server_init(&tls_server, &server_tls_cfg) == SALTS_OK);
  tls_live = true;
  REQUIRE(cnet_client_init(&client, &client_cfg) == SALTS_OK);
  client_live = true;
  REQUIRE(cnet_client_init(&server, &server_cfg) == SALTS_OK);
  server_live = true;
  REQUIRE(cnet_listener_init(&listener, &listener_cfg) == SALTS_OK);
  listener_live = true;
  REQUIRE(cnet_listener_port(&listener, &port) == SALTS_OK && port != 0u);
  REQUIRE(snprintf(uri, sizeof(uri), "tls://127.0.0.1:%u",
                   (unsigned int)port) > 0);
  connect_options = (cnet_connect_options){
      .uri = uri,
      .observer = {state_callback, receive_callback, &client_probe, send_callback},
      .tls = &client_tls_cfg};
  REQUIRE(cnet_connect(&client, &connect_options, &outbound) == SALTS_OK);

  deadline = cmeta_monotonic_ms() + 7000u;
  while (cmeta_monotonic_ms() < deadline) {
    if (should_verify) {
      if (client_probe.connected_count && server_probe.connected_count) break;
      if (client_probe.terminal_count || server_probe.terminal_count) break;
    } else if (options->expect_server_rejection) {
      if (server_probe.terminal_count) break;
    } else if (client_probe.terminal_count) break;
    REQUIRE(drive(&client, &server, &listener, &tls_server,
                  &server_probe, &accepted) == SALTS_OK);
  }

  REQUIRE(accepted);
  if (!should_verify) {
    /* A rejection must never deliver plaintext or publish a verified
     * server connection. Only the handshake-owning side determines rejection. */
    REQUIRE(client_probe.received_count == 0u);
    REQUIRE(server_probe.received_count == 0u);
    REQUIRE(client_probe.sent_count == 0u);
    REQUIRE(server_probe.sent_count == 0u);
    if (options->expect_server_rejection) {
      REQUIRE(server_probe.terminal_count == 1u);
      REQUIRE(server_probe.failed != 0u);
      REQUIRE(server_probe.connected_count == 0u);
      REQUIRE(strcmp(server_probe.failure_stage, "handshake") == 0);
    } else {
      REQUIRE(client_probe.terminal_count == 1u);
      REQUIRE(client_probe.failed != 0u);
      REQUIRE(client_probe.connected_count == 0u);
      REQUIRE(strcmp(client_probe.failure_stage, "handshake") == 0);
    }
  } else {
    char fingerprint[CNET_TLS_PEER_CERTIFICATE_SHA256_CAPACITY] = {0};
    REQUIRE(client_probe.connected_count == 1u);
    REQUIRE(server_probe.connected_count == 1u);
    REQUIRE(client_probe.failed == 0u && server_probe.failed == 0u);
    REQUIRE(client_probe.tls_version_status == SALTS_OK);
    REQUIRE(server_probe.tls_version_status == SALTS_OK);
    REQUIRE(client_probe.tls_cipher_status == SALTS_OK);
    REQUIRE(strlen(client_probe.tls_cipher) != 0u);
    REQUIRE(strcmp(client_probe.tls_version, "TLSv1.3") == 0 ||
            strcmp(client_probe.tls_version, "TLSv1.2") == 0);
    REQUIRE(cnet_tls_peer_certificate_sha256(&client, outbound,
                                             fingerprint) == SALTS_OK);
    REQUIRE(strlen(fingerprint) == 64u);
    if (options->server_ca_file != NULL) {
      char client_certificate_hash[CNET_TLS_PEER_CERTIFICATE_SHA256_CAPACITY] = {0};
      REQUIRE(cnet_tls_peer_certificate_sha256(
          &server, server_probe.connection, client_certificate_hash) == SALTS_OK);
      REQUIRE(strlen(client_certificate_hash) == 64u);
    }
    REQUIRE(cnet_receive(&server, server_probe.connection, 1u) == SALTS_OK);

    outgoing = mem_get_buffer(mem_global(), 1u);
    REQUIRE(outgoing != NULL);
    *mem_buffer_data(outgoing) = 0x46;
    mem_set_used(outgoing, 1u);
    REQUIRE(cnet_send_buffer(&client, outbound, outgoing) == SALTS_OK);
    mem_buffer_release(outgoing);
    outgoing = NULL;
    deadline = cmeta_monotonic_ms() + 7000u;
    while ((server_probe.received_count < 1u ||
            client_probe.sent_count < 1u) &&
           !client_probe.terminal_count && !server_probe.terminal_count &&
           cmeta_monotonic_ms() < deadline)
      REQUIRE(drive(&client, &server, &listener, &tls_server,
                    &server_probe, &accepted) == SALTS_OK);
    REQUIRE(server_probe.received_count == 1u);
    REQUIRE(server_probe.received_byte == 0x46);
    REQUIRE(client_probe.sent_count == 1u);
    REQUIRE(client_probe.failed == 0u && server_probe.failed == 0u);

    REQUIRE(cnet_close(&client, outbound) == SALTS_OK);
    deadline = cmeta_monotonic_ms() + 7000u;
    while ((!client_probe.terminal_count || !server_probe.terminal_count) &&
           cmeta_monotonic_ms() < deadline)
      REQUIRE(drive(&client, &server, &listener, &tls_server,
                    &server_probe, &accepted) == SALTS_OK);
    REQUIRE(client_probe.terminal_count == 1u);
    REQUIRE(server_probe.terminal_count == 1u);
    REQUIRE(client_probe.failed == 0u && server_probe.failed == 0u);
  }

cleanup:
  if (outgoing) mem_buffer_release(outgoing);
  if (accepted && !server_probe.terminal_count && server_live)
    (void)cnet_close(&server, server_probe.connection);
  if (listener_live) {
    (void)cnet_listener_close(&listener);
    (void)cnet_listener_destroy(&listener);
  }
  if (client_live) {
    (void)cnet_client_stop(&client, 5000u);
    (void)cnet_client_destroy(&client);
  }
  if (server_live) {
    (void)cnet_client_stop(&server, 5000u);
    (void)cnet_client_destroy(&server);
  }
  if (tls_live) (void)cnet_tls_server_destroy(&tls_server);
  return result;
}

int main(int argc, char **argv) {
  int result;
  if (argc == 8 && strcmp(argv[1], "--required-mtls") == 0) {
    const bool expect_success = strcmp(argv[7], "accept") == 0;
    const bool expect_rejection = strcmp(argv[7], "reject") == 0;
    if (!expect_success && !expect_rejection) return 2;
    if ((strcmp(argv[5], "-") == 0) != (strcmp(argv[6], "-") == 0))
      return 2;
    const tls_case_options required_mtls = {
        .label = "production CNet REQUIRED-mTLS",
        .ca_file = argv[2],
        .verified_hostname = "localhost",
        .expect_success = expect_success,
        .expect_server_rejection = expect_rejection,
        .server_ca_file = argv[2],
        .server_cert_file = argv[3],
        .server_key_file = argv[4],
        .client_cert_file = strcmp(argv[5], "-") == 0 ? NULL : argv[5],
        .client_key_file = strcmp(argv[6], "-") == 0 ? NULL : argv[6]};
    return run_case(&required_mtls);
  }
  if (argc != 1) return 2;
  const tls_case_options trusted = {
      .label = "trusted CA + correct DNS",
      .ca_file = CORE_TLS_TEST_P384_CA,
      .verified_hostname = "localhost",
      .expect_success = true};
  result = run_case(&trusted);
  if (result) return result;
  const tls_case_options wrong_ca = {
      .label = "wrong trusted root",
      .ca_file = CORE_TLS_TEST_P384_WRONG_CA,
      .verified_hostname = "localhost"};
  result = run_case(&wrong_ca);
  if (result) return result;
  const tls_case_options wrong_san = {
      .label = "wrong DNS SAN",
      .ca_file = CORE_TLS_TEST_P384_CA,
      .verified_hostname = "wrong.example"};
  return run_case(&wrong_san);
}
