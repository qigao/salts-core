#include <salts/native_io.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET test_socket;
  typedef int test_socklen;
  #define TEST_INVALID_SOCKET INVALID_SOCKET
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int test_socket;
  typedef socklen_t test_socklen;
  #define TEST_INVALID_SOCKET (-1)
#endif

#define REQUIRE(expression) do { \
  if (!(expression)) { \
    fprintf(stderr, "NativeIO real-UDP contract failed: line %d: %s\\n", \
            __LINE__, #expression); \
    return __LINE__; \
  } \
} while (0)

static void socket_close(test_socket socket_value) {
#if defined(_WIN32)
  if (socket_value != TEST_INVALID_SOCKET) (void)closesocket(socket_value);
#else
  if (socket_value != TEST_INVALID_SOCKET) (void)close(socket_value);
#endif
}

static native_io_backend_kind platform_backend(void) {
#if defined(_WIN32)
  return NATIVE_IO_BACKEND_IOCP;
#elif defined(__linux__)
  return NATIVE_IO_BACKEND_EPOLL;
#elif defined(__APPLE__)
  return NATIVE_IO_BACKEND_KQUEUE;
#else
  return (native_io_backend_kind)0;
#endif
}

static int bind_loopback(test_socket socket_value, struct sockaddr_in *address) {
  test_socklen length = (test_socklen)sizeof(*address);
  memset(address, 0, sizeof(*address));
  address->sin_family = AF_INET;
  address->sin_port = 0;
  address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(socket_value, (const struct sockaddr *)address,
           (test_socklen)sizeof(*address)) != 0) return -1;
  return getsockname(socket_value, (struct sockaddr *)address, &length) == 0 ? 0 : -1;
}

/* Cross-platform native backend round-trip. The OS completion owner retains
 * all borrowed payload and address storage through observe, including cancel. */
int main(void) {
  static const unsigned char payload[] = {0x41u, 0x42u, 0x43u, 0x44u};
  native_io_backend backend = {0};
  native_io_backend_config config = {platform_backend(), 2u, 3u, 2u};
  test_socket sockets[2] = {TEST_INVALID_SOCKET, TEST_INVALID_SOCKET};
  struct sockaddr_in addresses[2] = {0};
  struct sockaddr_storage peer_address = {0};
  native_io_endpoint endpoints[2] = {0};
  native_io_request requests[2] = {0};
  native_io_completion events[2] = {0};
  unsigned char received[sizeof(payload)] = {0};
  native_io_operation recv_op = {0};
  native_io_operation send_op = {0};
  size_t total = 0u;
  unsigned seen = 0u;

  if (config.kind == 0) return 0; /* Explicitly unsupported platform build. */
  REQUIRE(native_io_backend_kind_supported(config.kind));
  REQUIRE(native_io_backend_init(&backend, &config) == SALTS_OK);
  for (size_t i = 0u; i < 2u; ++i) {
    sockets[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    REQUIRE(sockets[i] != TEST_INVALID_SOCKET);
    REQUIRE(bind_loopback(sockets[i], &addresses[i]) == 0);
    REQUIRE(native_io_backend_attach_socket(&backend, (uintptr_t)sockets[i],
                                            &endpoints[i]) == SALTS_OK);
  }

  recv_op.kind = NATIVE_IO_OPERATION_UDP_RECV_FROM;
  recv_op.endpoint = endpoints[1];
  recv_op.buffer = received;
  recv_op.length = sizeof(received);
  recv_op.user_data = 41u;
  recv_op.address = &peer_address;
  recv_op.address_capacity = sizeof(peer_address);

  send_op.kind = NATIVE_IO_OPERATION_UDP_SEND_TO;
  send_op.endpoint = endpoints[0];
  send_op.buffer = (void *)payload;
  send_op.length = sizeof(payload);
  send_op.user_data = 42u;
  send_op.address = &addresses[1];
  send_op.address_capacity = sizeof(addresses[1]);
  send_op.address_length = sizeof(addresses[1]);

  REQUIRE(native_io_backend_submit(&backend, &recv_op, &requests[0]) == SALTS_OK);
  REQUIRE(native_io_backend_release_socket(&backend, endpoints[1]) == SALTS_EBUSY);
  REQUIRE(native_io_backend_submit(&backend, &send_op, &requests[1]) == SALTS_OK);
  REQUIRE(native_io_request_valid(requests[0]) && native_io_request_valid(requests[1]));
  for (size_t pass = 0u; pass < 12u && total < 2u; ++pass) {
    size_t delivered = 0u;
    REQUIRE(native_io_backend_observe(&backend, events + total, 2u - total,
                                      500u, &delivered) == SALTS_OK);
    total += delivered;
  }
  REQUIRE(total == 2u);
  for (size_t i = 0u; i < total; ++i) {
    REQUIRE(events[i].kind == NATIVE_IO_COMPLETION_OK);
    REQUIRE(events[i].status == SALTS_OK);
    REQUIRE(events[i].bytes == sizeof(payload));
    if (events[i].user_data == 41u) {
      const struct sockaddr_in *peer = (const struct sockaddr_in *)&peer_address;
      REQUIRE(events[i].request.slot == requests[0].slot);
      REQUIRE(events[i].request.generation == requests[0].generation);
      REQUIRE(events[i].address_length == sizeof(*peer));
      REQUIRE(peer->sin_family == AF_INET && peer->sin_port == addresses[0].sin_port);
      seen |= 1u;
    } else if (events[i].user_data == 42u) {
      REQUIRE(events[i].request.slot == requests[1].slot);
      REQUIRE(events[i].request.generation == requests[1].generation);
      REQUIRE(events[i].address_length == 0u);
      seen |= 2u;
    } else {
      return __LINE__;
    }
  }
  REQUIRE(seen == 3u && memcmp(received, payload, sizeof(payload)) == 0);

  /* A second unsatisfied receive holds its borrow until real cancellation
   * completes. Mere cancel acceptance is not a terminal completion. */
  {
    native_io_request pending = {0};
    native_io_completion cancelled = {0};
    size_t delivered = 0u;
    REQUIRE(native_io_backend_submit(&backend, &recv_op, &pending) == SALTS_OK);
    REQUIRE(native_io_backend_release_socket(&backend, endpoints[1]) == SALTS_EBUSY);
    REQUIRE(native_io_backend_cancel(&backend, pending) == SALTS_OK);
    for (size_t pass = 0u; pass < 12u && delivered == 0u; ++pass) {
      REQUIRE(native_io_backend_observe(&backend, &cancelled, 1u, 500u,
                                        &delivered) == SALTS_OK);
    }
    REQUIRE(delivered == 1u && cancelled.kind == NATIVE_IO_COMPLETION_CANCELLED);
    REQUIRE(cancelled.status == SALTS_ECANCELED);
    REQUIRE(cancelled.request.slot == pending.slot &&
            cancelled.request.generation == pending.generation);
    REQUIRE(native_io_backend_cancel(&backend, pending) == SALTS_ENOENT);
  }

  for (size_t i = 0u; i < 2u; ++i) {
    socket_close(sockets[i]); /* Caller-owned native socket, not backend-owned. */
    REQUIRE(native_io_backend_release_socket(&backend, endpoints[i]) == SALTS_OK);
  }
  REQUIRE(native_io_backend_close(&backend) == SALTS_OK);
  REQUIRE(native_io_backend_destroy(&backend) == SALTS_OK && backend.impl == NULL);
  return 0;
}
