/*
 * CNet client speaking real TLS 1.2/1.3 to an independent OpenSSL 3 s_server.
 * This does not embed or link OpenSSL into the CNet executable. OpenSSL is
 * started independently by tests/interop/verify_openssl_server.py.
 */
#include <cnet/cnet.h>
#include <cmeta_buffer.h>
#include <salts/clock.h>
#include <salts/error_codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CLIENT_TIMEOUT_MS = 6000 };

typedef struct interop_probe {
  cnet_client *client;
  cnet_connection connection;
  unsigned connected;
  unsigned terminal;
  unsigned failed;
  unsigned sent;
  unsigned received;
  int bad;
  char negotiated_version[24];
  char negotiated_cipher[128];
  char certificate_sha256[CNET_TLS_PEER_CERTIFICATE_SHA256_CAPACITY];
  char failure_stage[64];
  char response[32];
  size_t response_bytes;
} interop_probe;

static void on_state(void *ctx, cnet_connection connection,
                     cnet_connection_state state, const cnet_error *error) {
  interop_probe *p = (interop_probe *)ctx;
  p->connection = connection;
  if (state == CNET_CONNECTION_CONNECTED) {
    size_t len = 0u;
    ++p->connected;
    if (cnet_tls_negotiated_version(p->client, connection,
                                    p->negotiated_version,
                                    sizeof p->negotiated_version, &len) != SALTS_OK ||
        cnet_tls_negotiated_cipher(p->client, connection,
                                   p->negotiated_cipher,
                                   sizeof p->negotiated_cipher, &len) != SALTS_OK ||
        cnet_tls_peer_certificate_sha256(
            p->client, connection, p->certificate_sha256) != SALTS_OK) {
      p->bad = 1;
    }
  } else if (state == CNET_CONNECTION_CLOSED ||
             state == CNET_CONNECTION_FAILED) {
    ++p->terminal;
    if (state == CNET_CONNECTION_FAILED || error != NULL) {
      ++p->failed;
      if (error && error->stage) {
        size_t n = strlen(error->stage);
        if (n >= sizeof p->failure_stage) p->bad = 1;
        else memcpy(p->failure_stage, error->stage, n + 1u);
      }
    }
  }
}
static void on_receive(void *ctx, cnet_connection connection,
                       const cnet_receive_view *value) {
  interop_probe *p = (interop_probe *)ctx;
  (void)connection;
  if (!value || value->kind != CNET_MESSAGE_BYTES ||
      !value->data || !value->size) {
    p->bad = 1;
    return;
  }
  ++p->received;
  if (p->response_bytes < sizeof p->response) {
    const size_t remaining = sizeof p->response - p->response_bytes;
    const size_t take = value->size < remaining ? value->size : remaining;
    memcpy(p->response + p->response_bytes, value->data, take);
    p->response_bytes += take;
  }
}
static void on_send(void *ctx, cnet_connection connection, size_t size) {
  interop_probe *p = (interop_probe *)ctx;
  (void)connection;
  ++p->sent;
  if (size != sizeof("GET / HTTP/1.0\r\nHost: localhost\r\n\r\n") - 1u)
    p->bad = 1;
}

static cnet_client_config client_config(void) {
  cnet_client_config c = {0};
#if defined(_WIN32)
  c.backend = NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
  c.backend = NATIVE_IO_BACKEND_KQUEUE;
#else
  c.backend = NATIVE_IO_BACKEND_EPOLL;
#endif
  c.connection_capacity = 2u;
  c.command_capacity = 16u;
  c.request_capacity = 8u;
  c.completion_batch_capacity = 8u;
  c.event_capacity = 16u;
  c.max_send_bytes = 1024u;
  c.receive_buffer_bytes = 4096u;
  c.connect_timeout_ms = 3000u;
  c.read_timeout_ms = 4000u;
  c.write_timeout_ms = 4000u;
  c.tls_io_buffer_bytes = CNET_TLS_MIN_IO_BUFFER_BYTES;
  c.tls_handshake_timeout_ms = 4000u;
  return c;
}

static int send_http_request(cnet_client *client,
                             cnet_connection connection) {
  static const char request[] = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
  mem_buffer_t *buffer = mem_get_buffer(mem_global(), sizeof request - 1u);
  int result;
  if (buffer == NULL) return SALTS_ENOMEM;
  memcpy(mem_buffer_data(buffer), request, sizeof request - 1u);
  mem_set_used(buffer, sizeof request - 1u);
  result = cnet_send_buffer(client, connection, buffer);
  mem_buffer_release(buffer); /* CNet owns accepted request until terminal. */
  return result;
}

int main(int argc, char **argv) {
  cnet_client client = {0};
  interop_probe probe = {0};
  cnet_connection connection = {0};
  cnet_observer observer = {on_state, on_receive, &probe, on_send};
  cnet_client_config config = client_config();
  cnet_tls_client_config tls = {0};
  cnet_connect_options options = {0};
  char url[96] = {0};
  const char *expected_version;
  bool expect_success, sent = false;
  uint64_t deadline;
  int result = 0;

  if (argc != 6 ||
      (strcmp(argv[4], "accept") != 0 &&
       strcmp(argv[4], "reject") != 0)) {
    fputs("usage: cnet_openssl_client PORT CA NAME accept|reject TLSv1.2|TLSv1.3\n",
          stderr);
    return 2;
  }
  expected_version = argv[5];
  expect_success = strcmp(argv[4], "accept") == 0;
  if (snprintf(url, sizeof url, "tls://127.0.0.1:%s", argv[1]) >= (int)sizeof url)
    return 3;
  tls.size = sizeof tls;
  tls.ca_file = argv[2];
  tls.server_name = argv[3];
  options.uri = url;
  options.tls = &tls;
  probe.client = &client;
  options.observer = observer;

  if (cnet_client_init(&client, &config) != SALTS_OK) {
    fputs("independent CNet client init failed\n", stderr);
    return 4;
  }
  if (cnet_connect(&client, &options, &connection) != SALTS_OK) {
    fputs("independent CNet TLS connect admission failed\n", stderr);
    result = 5;
    goto done;
  }

  deadline = cmeta_monotonic_ms() + CLIENT_TIMEOUT_MS;
  while (cmeta_monotonic_ms() < deadline) {
    size_t events = 0u;
    int status = cnet_client_poll(&client, 15u, &events);
    if (status != SALTS_OK || probe.bad) {
      fprintf(stderr, "CNet progress failed: status=%d probe_bad=%d\n",
              status, probe.bad);
      result = 6;
      break;
    }
    if (!expect_success && probe.terminal) break;
    if (expect_success && probe.connected && !sent) {
      if (cnet_receive(&client, connection, 4u) != SALTS_OK ||
          send_http_request(&client, connection) != SALTS_OK) {
        fputs("CNet TLS HTTP data admission failed\n", stderr);
        result = 7;
        break;
      }
      sent = true;
    }
    if (expect_success && probe.sent && probe.received &&
        probe.response_bytes >= 8u) break;
    if (probe.terminal && expect_success) break;
  }

  if (!result && !expect_success) {
    if (probe.terminal != 1u || probe.failed != 1u ||
        probe.connected || probe.received || probe.sent ||
        strcmp(probe.failure_stage, "handshake") != 0) {
      fprintf(stderr, "TLS trust/identity rejection failed:"
              " terminal=%u failed=%u connected=%u"
              " received=%u sent=%u stage=%s\n",
              probe.terminal, probe.failed, probe.connected,
              probe.received, probe.sent, probe.failure_stage);
      result = 8;
    }
  } else if (!result) {
    if (probe.connected != 1u || probe.failed != 0u ||
        !probe.sent || !probe.received ||
        strcmp(probe.negotiated_version, expected_version) != 0 ||
        strlen(probe.negotiated_cipher) == 0u ||
        strlen(probe.certificate_sha256) != 64u ||
        probe.response_bytes < 8u ||
        memcmp(probe.response, "HTTP/", 5u) != 0) {
      fprintf(stderr,
              "OpenSSL interop failed: connected=%u failed=%u terminal=%u"
              " sent=%u received=%u version=%s expected=%s"
              " cipher=%s response=%.12s\n",
              probe.connected, probe.failed, probe.terminal,
              probe.sent, probe.received, probe.negotiated_version,
              expected_version, probe.negotiated_cipher, probe.response);
      result = 9;
    }
  }

done:
  if (cnet_client_stop(&client, CLIENT_TIMEOUT_MS) != SALTS_OK) {
    fputs("CNet stop failed to drain OpenSSL peer\n", stderr);
    if (!result) result = 10;
  }
  if (cnet_client_destroy(&client) != SALTS_OK) {
    fputs("CNet destroy failed\n", stderr);
    if (!result) result = 11;
  }
  return result;
}
