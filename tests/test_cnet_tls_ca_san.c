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
#include <stdlib.h>
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
#ifndef CORE_TLS_TEST_EXTERNAL_CA
#error CORE_TLS_TEST_EXTERNAL_CA is required
#endif

typedef struct tls_probe {
  cnet_client *client;
  cnet_connection connection;
  unsigned connected_count;
  unsigned terminal_count;
  unsigned failed;
  unsigned handshaking_count;
  int last_connection_state;
  int last_error_status;
  int last_native_status;
  unsigned sent_count;
  unsigned received_count;
  unsigned char received_byte;
  /* Python ssl peer sends a complete two-byte TLS application record. */
  bool expect_python_ok_record;
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
  p->last_connection_state = (int)state;
  if (error != NULL) {
    /* Numeric terminal metadata only: never print credential material. */
    p->last_error_status = error->status;
    p->last_native_status = error->native_status;
  }
  if (state == CNET_CONNECTION_TLS_HANDSHAKING)
    ++p->handshaking_count;
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
      view->data == NULL || p->received_count != 0u) {
    ++p->failed;
    return;
  }
  if (p->expect_python_ok_record) {
    /* Receive demand counts events, not bytes. The independent Python ssl
     * server sends exactly "ok" in one authenticated application record. */
    if (view->size != 2u || memcmp(view->data, "ok", 2u) != 0) {
      ++p->failed;
      return;
    }
  } else if (view->size != 1u) {
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
    result = 1;                                                               \
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


/*
 * Native, production Salts::CNet client to independent Python stdlib ssl
 * server. Python fixes TLS1.2 or TLS1.3 and supplies the same valid X.509
 * leaf for both SAN-positive and SAN-negative controls. This mode never
 * imports the alternate test-only CNet TLS facade or its Provider.
 */
static int run_python_ssl_server(const char *port_text,
                                 const char *protocol,
                                 const char *identity,
                                 const char *trust_ca,
                                 bool expect_success) {
  const char *label = "production CNet -> pinned Python ssl TLS peer";
  const bool approved_identity = expect_success;
  const char *expected_version = strcmp(protocol, "tls12") == 0
                                     ? "TLSv1.2" : "TLSv1.3";
  char *end = NULL;
  unsigned long port = strtoul(port_text, &end, 10);
  cnet_client client = {0};
  cnet_connection outbound = {0};
  tls_probe probe = {0};
  cnet_connect_options connect_options = {0};
  cnet_client_config client_cfg = config();
  const cnet_tls_client_config tls_config = {
      .size = sizeof(cnet_tls_client_config),
      /* The existing external peer CLI uses "-" only for the independent
       * Windows native system-root negative. NULL enables the production
       * CNet system trust path, with no test-only TLS provider. */
      .ca_file = strcmp(trust_ca, "-") == 0 ? NULL : trust_ca,
      .server_name = identity};
  char uri[96] = {0};
  bool initialized = false;
  mem_buffer_t *outgoing = NULL;
  uint64_t deadline;
  size_t events = 0u;
  int result = 0;

  if (end == port_text || *end != '\0' || port == 0u || port > 65535u ||
      (strcmp(protocol, "tls12") != 0 && strcmp(protocol, "tls13") != 0) ||
      trust_ca == NULL || trust_ca[0] == '\0' ||
      (strcmp(identity, "localhost") != 0 &&
       strcmp(identity, "wrong.example") != 0 &&
       strcmp(identity, "127.0.0.1") != 0 &&
       strcmp(identity, "127.0.0.2") != 0 &&
       strcmp(identity, "::1") != 0 &&
       strcmp(identity, "::2") != 0))
    return 2;

  probe.client = &client;
  probe.expect_python_ok_record = true;
  REQUIRE(cnet_client_init(&client, &client_cfg) == SALTS_OK);
  initialized = true;
  REQUIRE(snprintf(uri, sizeof(uri), "tls://127.0.0.1:%lu", port) > 0);
  connect_options = (cnet_connect_options){
      .uri = uri,
      .observer = {state_callback, receive_callback, &probe, send_callback},
      .tls = &tls_config};
  REQUIRE(cnet_connect(&client, &connect_options, &outbound) == SALTS_OK);

  deadline = cmeta_monotonic_ms() + 9000u;
  while (!probe.connected_count && !probe.terminal_count &&
         cmeta_monotonic_ms() < deadline)
    REQUIRE(cnet_client_poll(&client, 1u, &events) == SALTS_OK);

  if (!approved_identity) {
    /* This must be a post-TCP handshake authentication rejection, not
     * a timeout, refused connect, or successful encrypted connection. */
    REQUIRE(probe.terminal_count == 1u);
    REQUIRE(probe.failed != 0u);
    REQUIRE(probe.connected_count == 0u);
    REQUIRE(probe.received_count == 0u && probe.sent_count == 0u);
    REQUIRE(strcmp(probe.failure_stage, "handshake") == 0);
  } else {
    char digest[CNET_TLS_PEER_CERTIFICATE_SHA256_CAPACITY] = {0};
    REQUIRE(probe.connected_count == 1u && probe.failed == 0u);
    REQUIRE(probe.tls_version_status == SALTS_OK);
    REQUIRE(strcmp(probe.tls_version, expected_version) == 0);
    REQUIRE(probe.tls_cipher_status == SALTS_OK);
    REQUIRE(probe.tls_cipher[0] != '\0');
    REQUIRE(cnet_tls_peer_certificate_sha256(&client, outbound, digest) == SALTS_OK);
    REQUIRE(strlen(digest) == 64u);
    REQUIRE(cnet_receive(&client, outbound, 1u) == SALTS_OK);

    outgoing = mem_get_buffer(mem_global(), 1u);
    REQUIRE(outgoing != NULL);
    *mem_buffer_data(outgoing) = 0x46u;
    mem_set_used(outgoing, 1u);
    REQUIRE(cnet_send_buffer(&client, outbound, outgoing) == SALTS_OK);
    mem_buffer_release(outgoing);
    outgoing = NULL;
    deadline = cmeta_monotonic_ms() + 9000u;
    while ((!probe.received_count || !probe.sent_count) &&
           !probe.terminal_count && cmeta_monotonic_ms() < deadline)
      REQUIRE(cnet_client_poll(&client, 1u, &events) == SALTS_OK);
    /* Independent Python ssl writes "ok" over the negotiated record;
     * one byte of explicit NativeIO receive demand must deliver 'o'. */
    REQUIRE(probe.received_count == 1u && probe.received_byte == 'o' &&
            probe.failed == 0u);
    REQUIRE(probe.sent_count == 1u && probe.failed == 0u);
    REQUIRE(cnet_close(&client, outbound) == SALTS_OK);
  }

cleanup:
  if (outgoing != NULL) mem_buffer_release(outgoing);
  if (initialized) {
    (void)cnet_client_stop(&client, 5000u);
    (void)cnet_client_destroy(&client);
  }
  return result;
}


/*
 * Production CNet server against a genuinely separate Python stdlib ssl
 * client. The server retains exactly one Salts::CNet native owner; Python
 * supplies ephemeral client credentials and pins TLS 1.2 or TLS 1.3.
 * Port handoff is a private test file to avoid any extra server/consumer app.
 */
static int run_python_ssl_client(const char *port_path,
                                 const char *ca_file,
                                 const char *cert_file,
                                 const char *key_file,
                                 const char *protocol,
                                 const char *outcome,
                                 const char *expected_client_sha256) {
  const char *label = "production CNet <- Python ssl TLS REQUIRED-mTLS";
  const char *expected_version = strcmp(protocol, "12") == 0
                                     ? "TLSv1.2" : "TLSv1.3";
  const bool expect_idle_read_timeout = strcmp(outcome, "idle") == 0;
  const bool expect_orderly_close = strcmp(outcome, "close") == 0;
  const bool should_verify =
      strcmp(outcome, "accept") == 0 || expect_idle_read_timeout ||
      expect_orderly_close;
  cnet_client server = {0};
  cnet_listener listener = {0};
  cnet_tls_server tls_server = {0};
  tls_probe probe = {0};
  const cnet_client_config client_cfg = config();
  const cnet_listener_config listener_cfg = {
      client_cfg.backend, "127.0.0.1", 0u, 2u};
  const cnet_tls_server_config tls_cfg = {
      .size = sizeof(cnet_tls_server_config),
      .cert_file = cert_file,
      .key_file = key_file,
      .ca_file = ca_file,
      .client_auth = CNET_TLS_CLIENT_AUTH_REQUIRED};
  bool tls_live = false, server_live = false, listener_live = false;
  bool accepted = false, armed = false, response_sent = false;
  mem_buffer_t *outgoing = NULL;
  FILE *port_file = NULL;
  uint16_t port = 0u;
  uint64_t deadline;
  size_t events = 0u;
  int result = 0;

  if (port_path == NULL || !*port_path || ca_file == NULL ||
      cert_file == NULL || key_file == NULL ||
      (strcmp(protocol, "12") != 0 && strcmp(protocol, "13") != 0) ||
      (!should_verify && strcmp(outcome, "reject") != 0))
    return 2;
  probe.client = &server;
  REQUIRE(cnet_tls_server_init(&tls_server, &tls_cfg) == SALTS_OK);
  tls_live = true;
  REQUIRE(cnet_client_init(&server, &client_cfg) == SALTS_OK);
  server_live = true;
  REQUIRE(cnet_listener_init(&listener, &listener_cfg) == SALTS_OK);
  listener_live = true;
  REQUIRE(cnet_listener_port(&listener, &port) == SALTS_OK && port != 0u);
  /* Only the test driver knows this private path; publish after bind. */
  port_file = fopen(port_path, "wb");
  REQUIRE(port_file != NULL);
  REQUIRE(fprintf(port_file, "%u\n", (unsigned int)port) > 0);
  REQUIRE(fclose(port_file) == 0);
  port_file = NULL;

  deadline = cmeta_monotonic_ms() + 15000u;
  while (cmeta_monotonic_ms() < deadline) {
    int ready = 0;
    if (!accepted) {
      REQUIRE(cnet_listener_wait(&listener, 0u, &ready) == SALTS_OK);
      if (ready) {
        const cnet_observer observer = {
            state_callback, receive_callback, &probe, send_callback};
        REQUIRE(cnet_listener_accept_tls(
            &listener, &server, &tls_server, &observer,
            &probe.connection) == SALTS_OK);
        accepted = true;
      }
    }
    REQUIRE(cnet_client_poll(&server, 1u, &events) == SALTS_OK);
    if (!should_verify && probe.terminal_count) break;
    if (should_verify && probe.connected_count && !armed) {
      char fingerprint[CNET_TLS_PEER_CERTIFICATE_SHA256_CAPACITY] = {0};
      REQUIRE(probe.connected_count == 1u && probe.failed == 0u);
      REQUIRE(probe.tls_version_status == SALTS_OK);
      REQUIRE(strcmp(probe.tls_version, expected_version) == 0);
      REQUIRE(probe.tls_cipher_status == SALTS_OK);
      REQUIRE(probe.tls_cipher[0] != '\0');
      REQUIRE(cnet_tls_peer_certificate_sha256(
          &server, probe.connection, fingerprint) == SALTS_OK);
      REQUIRE(strlen(fingerprint) == 64u);
      REQUIRE(expected_client_sha256 != NULL &&
              strlen(expected_client_sha256) == 64u &&
              strcmp(fingerprint, expected_client_sha256) == 0);
      REQUIRE(cnet_receive(&server, probe.connection, 1u) == SALTS_OK);
      armed = true;
    }
    if (should_verify && !expect_idle_read_timeout &&
        probe.received_count && !response_sent) {
      REQUIRE(probe.received_count == 1u && probe.received_byte == 0x46u);
      REQUIRE(probe.failed == 0u);
      outgoing = mem_get_buffer(mem_global(), 1u);
      REQUIRE(outgoing != NULL);
      *mem_buffer_data(outgoing) = 0x47u;
      mem_set_used(outgoing, 1u);
      REQUIRE(cnet_send_buffer(
          &server, probe.connection, outgoing) == SALTS_OK);
      mem_buffer_release(outgoing);
      outgoing = NULL;
      response_sent = true;
    }
    /* For close_notify acceptance, keep the real owner alive until the peer's
     * authenticated close and the matching local close_notify are complete.
     * A successful one-byte echo is not sufficient evidence of retirement. */
    if (should_verify && !expect_orderly_close &&
        response_sent && probe.sent_count) break;
    if (probe.terminal_count) break;
  }

  REQUIRE(accepted);
  if (expect_orderly_close) {
    /* Python ssl.unwrap() requires the native server's authenticated
     * close_notify. It does not count TCP EOF as normal TLS shutdown.
     * CNet must also retire its old NativeIO TLS_READ lease exactly once,
     * without a post-auth READ timeout or terminal failure. */
    REQUIRE(armed && probe.connected_count == 1u);
    REQUIRE(probe.received_count == 1u && probe.received_byte == 0x46u);
    REQUIRE(response_sent && probe.sent_count == 1u);
    REQUIRE(probe.terminal_count == 1u && probe.failed == 0u);
    REQUIRE(probe.last_connection_state == CNET_CONNECTION_CLOSED);
  } else if (expect_idle_read_timeout) {
    /* A real, authenticated idle peer keeps TCP open while CNet owns a
     * post-handshake TLS_READ. Never accept a late socket EOF or handshake
     * failure as proof of READ timeout; all public identity checks above
     * must already have passed before receive demand was armed. */
    REQUIRE(armed && probe.connected_count == 1u);
    REQUIRE(probe.terminal_count == 1u && probe.failed == 1u);
    REQUIRE(probe.last_error_status == SALTS_ETIMEDOUT);
    REQUIRE(strcmp(probe.failure_stage, "read") == 0);
    REQUIRE(probe.received_count == 0u && probe.sent_count == 0u);
    REQUIRE(!response_sent);
  } else if (should_verify) {
    if (probe.connected_count != 1u || probe.failed != 0u) {
      /* Log only stable public CNet state on a real TLS/mTLS failure.
       * A peer TLS 1.3 timeout must not hide whether NativeIO accepted
       * TCP, published HANDSHAKING or emitted a terminal callback. */
      fprintf(stderr,
              "CNET_MTLS_PHASE protocol=%s expected=%s accepted=%u "
              "handshaking=%u connected=%u terminal=%u failed=%u "
              "last_state=%d status=%d native_status=%d stage=%s "
              "polled_events=%zu armed=%u reply_sent=%u "
              "plaintext_events=%u send_events=%u\n",
              protocol, outcome, accepted ? 1u : 0u,
              probe.handshaking_count, probe.connected_count,
              probe.terminal_count, probe.failed,
              probe.last_connection_state, probe.last_error_status,
              probe.last_native_status,
              probe.failure_stage[0] != '\0' ? probe.failure_stage : "none",
              events, armed ? 1u : 0u, response_sent ? 1u : 0u,
              probe.received_count, probe.sent_count);
    }
    REQUIRE(probe.connected_count == 1u && probe.failed == 0u);
    REQUIRE(armed && response_sent && probe.sent_count == 1u);
    REQUIRE(probe.received_count == 1u && probe.received_byte == 0x46u);
  } else {
    /* A bad peer may never publish an authenticated connection or plaintext.
     * This must be a TLS handshake failure, not a timeout or socket refusal. */
    REQUIRE(probe.terminal_count == 1u);
    REQUIRE(probe.failed != 0u && probe.connected_count == 0u);
    REQUIRE(probe.sent_count == 0u && probe.received_count == 0u);
    REQUIRE(strcmp(probe.failure_stage, "handshake") == 0);
  }

cleanup:
  if (outgoing != NULL) mem_buffer_release(outgoing);
  if (port_file != NULL) (void)fclose(port_file);
  if (accepted && !probe.terminal_count && server_live)
    (void)cnet_close(&server, probe.connection);
  if (listener_live) {
    (void)cnet_listener_close(&listener);
    (void)cnet_listener_destroy(&listener);
  }
  if (server_live) {
    (void)cnet_client_stop(&server, 5000u);
    (void)cnet_client_destroy(&server);
  }
  if (tls_live) (void)cnet_tls_server_destroy(&tls_server);
  if (result == 0)
    printf("PASS production NativeIO CNet server / TLS%s / %s\n",
           protocol, outcome);
  return result;
}

int main(int argc, char **argv) {
  if (argc == 9 && strcmp(argv[1], "--python-ssl-client") == 0)
    return run_python_ssl_client(
        argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
  if (argc == 5 && strcmp(argv[1], "--external-san") == 0)
    return run_python_ssl_server(
        argv[2], argv[3], argv[4], CORE_TLS_TEST_EXTERNAL_CA,
        strcmp(argv[4], "localhost") == 0);
  if (argc == 7 && strcmp(argv[1], "--external-ip-san") == 0) {
    if (strcmp(argv[6], "accept") != 0 && strcmp(argv[6], "reject") != 0)
      return 2;
    return run_python_ssl_server(
        argv[2], argv[3], argv[4], argv[5],
        strcmp(argv[6], "accept") == 0);
  }
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
