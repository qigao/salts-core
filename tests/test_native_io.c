#include <salts/native_io.h>
#include <salts/native_io_sharded.h>
#include <salts/error_codes.h>

#include <stdio.h>

static native_io_backend_kind native_backend(void) {
#if defined(_WIN32)
  return NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
  return NATIVE_IO_BACKEND_KQUEUE;
#elif defined(__linux__)
  return NATIVE_IO_BACKEND_EPOLL;
#else
  return (native_io_backend_kind)0;
#endif
}

int main(void) {
  native_io_backend backend = {0};
  native_io_backend_stats stats = {0};
  native_io_backend_config config = {
      native_backend(), 2u, 4u, 2u };
  native_io_sharded_endpoint none = {0};
  int status;

  if (config.kind == 0) return 0; /* Unsupported compile-target; no fallback. */
  if (!native_io_backend_kind_supported(config.kind) ||
      native_io_backend_kind_model(config.kind) == NATIVE_IO_MODEL_NONE)
    return 1;
  if (native_io_endpoint_valid((native_io_endpoint){0}) ||
      native_io_request_valid((native_io_request){0}) ||
      native_io_sharded_endpoint_valid(none))
    return 2;
  status = native_io_backend_init(&backend, &config);
  if (status != SALTS_OK) {
    fprintf(stderr, "native_io_backend_init status=%d\n", status);
    return 3;
  }
  if (!native_io_backend_get_stats(&backend, &stats) ||
      stats.endpoint_capacity != config.endpoint_capacity ||
      stats.request_capacity != config.request_capacity ||
      stats.active_requests != 0u)
    return 4;
  status = native_io_backend_close(&backend);
  if (status != SALTS_OK) {
    fprintf(stderr, "native_io_backend_close status=%d\n", status);
    return 5;
  }
  status = native_io_backend_destroy(&backend);
  if (status != SALTS_OK || backend.impl != NULL) {
    fprintf(stderr, "native_io_backend_destroy status=%d\n", status);
    return 6;
  }
  return 0;
}
