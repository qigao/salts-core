#include <salts/error_codes.h>
#include <salts/native_io.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * NativeIO transport integration without CMeta/ACE/CFlow.
 * The backend borrows OS descriptors until all accepted requests have
 * produced their authoritative completion. Cancellation is not terminal.
 */
static native_io_backend_kind selected_backend(void) {
#if defined(__linux__)
  return NATIVE_IO_BACKEND_EPOLL;
#elif defined(__APPLE__)
  return NATIVE_IO_BACKEND_KQUEUE;
#else
  return (native_io_backend_kind)0;
#endif
}

static int make_nonblocking_pipe(int fds[2]) {
  int flags;
  fds[0] = fds[1] = -1;
  if (pipe(fds) != 0) return -1;
  for (int i = 0; i < 2; ++i) {
    flags = fcntl(fds[i], F_GETFL, 0);
    if (flags == -1 || fcntl(fds[i], F_SETFL, flags | O_NONBLOCK) != 0)
      return -1;
  }
  return 0;
}

static void close_pipe(int fds[2]) {
  if (fds[0] >= 0) { (void)close(fds[0]); fds[0] = -1; }
  if (fds[1] >= 0) { (void)close(fds[1]); fds[1] = -1; }
}

static int same_request(native_io_request a, native_io_request b) {
  return a.slot == b.slot && a.generation == b.generation;
}

static int roundtrip(native_io_backend_kind kind) {
  static const unsigned char payload[4] = {0x11u, 0x22u, 0x33u, 0x44u};
  unsigned char result[sizeof(payload)] = {0};
  native_io_backend backend = {0};
  native_io_backend_config config = {kind, 2u, 2u, 2u};
  native_io_endpoint endpoints[2] = {{0}, {0}};
  native_io_request requests[2] = {{0}, {0}};
  native_io_completion events[2] = {{0}, {0}};
  native_io_operation read_op = {0};
  native_io_operation write_op = {0};
  int fds[2] = {-1, -1};
  size_t got = 0;
  int saw_read = 0, saw_write = 0;

  if (native_io_backend_init(&backend, &config) != SALTS_OK) return 1;
  if (make_nonblocking_pipe(fds) != 0) return 2;
  if (native_io_backend_attach_pipe(&backend, (uintptr_t)fds[0],
        NATIVE_IO_PIPE_ENDPOINT_ASYNC_CAPABLE, &endpoints[0]) != SALTS_OK)
    return 3;
  if (native_io_backend_attach_pipe(&backend, (uintptr_t)fds[1],
        NATIVE_IO_PIPE_ENDPOINT_ASYNC_CAPABLE, &endpoints[1]) != SALTS_OK)
    return 4;

  read_op.kind = NATIVE_IO_OPERATION_PIPE_READ;
  read_op.endpoint = endpoints[0];
  read_op.buffer = result;
  read_op.length = sizeof(result);
  read_op.user_data = 17u;
  write_op.kind = NATIVE_IO_OPERATION_PIPE_WRITE;
  write_op.endpoint = endpoints[1];
  write_op.buffer = (void *)payload;
  write_op.length = sizeof(payload);
  write_op.user_data = 19u;

  if (native_io_backend_submit(&backend, &read_op, &requests[0]) != SALTS_OK)
    return 5;
  /* Cancel/close are separate from completion and must not release this lease. */
  if (native_io_backend_release_pipe(&backend, endpoints[0]) != SALTS_EBUSY)
    return 6;
  if (native_io_backend_submit(&backend, &write_op, &requests[1]) != SALTS_OK)
    return 7;

  for (int turn = 0; turn < 10 && !(saw_read && saw_write); ++turn) {
    size_t batch = 0u;
    if (native_io_backend_observe(&backend, events, 2u, 250u, &batch) != SALTS_OK)
      return 8;
    for (size_t i = 0; i < batch; ++i) {
      native_io_completion *e = &events[i];
      if (same_request(e->request, requests[0])) {
        if (saw_read || e->kind != NATIVE_IO_COMPLETION_OK ||
            e->bytes != sizeof(payload) || e->user_data != 17u)
          return 9;
        saw_read = 1;
      } else if (same_request(e->request, requests[1])) {
        if (saw_write || e->kind != NATIVE_IO_COMPLETION_OK ||
            e->bytes != sizeof(payload) || e->user_data != 19u)
          return 10;
        saw_write = 1;
      } else return 11;
    }
    got += batch;
  }
  if (!saw_read || !saw_write || got != 2u ||
      memcmp(payload, result, sizeof(payload)) != 0)
    return 12;

  close_pipe(fds);
  if (native_io_backend_release_pipe(&backend, endpoints[0]) != SALTS_OK ||
      native_io_backend_release_pipe(&backend, endpoints[1]) != SALTS_OK)
    return 13;
  if (native_io_backend_close(&backend) != SALTS_OK) return 14;
  if (native_io_backend_destroy(&backend) != SALTS_OK || backend.impl != NULL)
    return 15;
  return 0;
}

static int cancel_pending_read(native_io_backend_kind kind) {
  native_io_backend backend = {0};
  native_io_backend_config config = {kind, 1u, 1u, 1u};
  native_io_endpoint endpoint = {0};
  native_io_request request = {0};
  native_io_operation read_op = {0};
  native_io_completion event = {0};
  int fds[2] = {-1, -1};
  unsigned char received = 0;
  size_t batch = 0u;

  if (native_io_backend_init(&backend, &config) != SALTS_OK) return 1;
  if (make_nonblocking_pipe(fds) != 0) return 2;
  if (native_io_backend_attach_pipe(&backend, (uintptr_t)fds[0],
        NATIVE_IO_PIPE_ENDPOINT_ASYNC_CAPABLE, &endpoint) != SALTS_OK)
    return 3;

  read_op.kind = NATIVE_IO_OPERATION_PIPE_READ;
  read_op.endpoint = endpoint;
  read_op.buffer = &received;
  read_op.length = 1u;
  read_op.user_data = 47u;
  if (native_io_backend_submit(&backend, &read_op, &request) != SALTS_OK)
    return 4;
  if (native_io_backend_cancel(&backend, request) != SALTS_OK) return 5;
  if (native_io_backend_release_pipe(&backend, endpoint) != SALTS_EBUSY)
    return 6;
  if (native_io_backend_observe(&backend, &event, 1u, 2000u, &batch) != SALTS_OK ||
      batch != 1u || !same_request(event.request, request) ||
      event.kind != NATIVE_IO_COMPLETION_CANCELLED ||
      event.status != SALTS_ECANCELED || event.user_data != 47u)
    return 7;
  close_pipe(fds);
  if (native_io_backend_release_pipe(&backend, endpoint) != SALTS_OK)
    return 8;
  if (native_io_backend_close(&backend) != SALTS_OK) return 9;
  if (native_io_backend_destroy(&backend) != SALTS_OK || backend.impl != NULL)
    return 10;
  return 0;
}

int main(void) {
  const native_io_backend_kind kind = selected_backend();
  int error;
  if (!native_io_backend_kind_supported(kind)) return 0;
  error = roundtrip(kind);
  if (error) { fprintf(stderr, "native pipe roundtrip: %d\n", error); return error; }
  error = cancel_pending_read(kind);
  if (error) { fprintf(stderr, "native pipe cancel: %d\n", error); return 32 + error; }
  return 0;
}
