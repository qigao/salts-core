/* Original Platform Readiness on the single installed shared owner. */
#include <readiness.h>
#include <error_codes.h>
#include <thread.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(_WIN32)
#include <stdatomic.h>
#include <unistd.h>
#endif

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

#if !defined(_WIN32)
typedef struct probe {
  atomic_int ready;
  atomic_int unexpected;
} probe;

static void on_readiness(void *user, cmeta_readiness_events events, int status) {
  probe *state = (probe *)user;
  if (status != SALTS_OK || (events & SALTS_READINESS_EVENT_READ) == 0u)
    atomic_store_explicit(&state->unexpected, 1, memory_order_release);
  atomic_store_explicit(&state->ready, 1, memory_order_release);
}

static int run_poll_pipe(void) {
  cmeta_readiness_reactor reactor = {0};
  cmeta_readiness_registration reg = {0};
  cmeta_readiness_config config = {2u, 2u};
  cmeta_readiness_stats stats = {0};
  probe state = {0};
  int fds[2] = {-1, -1};
  unsigned attempt;
  const char byte = 'X';
  CHECK(pipe(fds) == 0);
  CHECK(cmeta_readiness_reactor_init_kind(
      &reactor, &config, SALTS_READINESS_BACKEND_POLL) == SALTS_OK);
  CHECK(cmeta_readiness_reactor_stats(&reactor, &stats) == SALTS_OK);
  CHECK(stats.capacity == 2u && stats.registered_count == 0u);
  CHECK(cmeta_readiness_register(&reactor, fds[0], &reg) == SALTS_OK);
  CHECK(cmeta_readiness_arm(
      &reg, SALTS_READINESS_EVENT_READ, on_readiness, &state) == SALTS_OK);
  CHECK(write(fds[1], &byte, 1u) == 1);
  for (attempt = 0u; attempt < 1000u; ++attempt) {
    if (atomic_load_explicit(&state.ready, memory_order_acquire)) break;
    cmeta_sleep_ms(1u);
  }
  CHECK(atomic_load_explicit(&state.ready, memory_order_acquire));
  CHECK(!atomic_load_explicit(&state.unexpected, memory_order_acquire));
  CHECK(cmeta_readiness_close(&reg) == SALTS_OK);
  CHECK(reg.impl == NULL);
  CHECK(cmeta_readiness_reactor_shutdown(&reactor) == SALTS_OK);
  CHECK(cmeta_readiness_reactor_destroy(&reactor) == SALTS_OK);
  CHECK(reactor.impl == NULL);
  CHECK(close(fds[0]) == 0);
  CHECK(close(fds[1]) == 0);
  return 0;
}
#endif

int main(void) {
  cmeta_readiness_reactor reactor = {0};
  cmeta_readiness_config config = {2u, 2u};
  CHECK(!cmeta_readiness_backend_supported((cmeta_readiness_backend_kind)99));
#if defined(_WIN32)
  CHECK(!cmeta_readiness_backend_supported(SALTS_READINESS_BACKEND_POLL));
  CHECK(!cmeta_readiness_backend_supported(SALTS_READINESS_BACKEND_KQUEUE));
  CHECK(!cmeta_readiness_backend_supported(SALTS_READINESS_BACKEND_EPOLL));
  CHECK(cmeta_readiness_reactor_init_kind(
      &reactor, &config, SALTS_READINESS_BACKEND_POLL) == SALTS_ENOTSUP);
  CHECK(reactor.impl == NULL);
#else
  CHECK(cmeta_readiness_backend_supported(SALTS_READINESS_BACKEND_POLL));
  CHECK(run_poll_pipe() == 0);
# if defined(__APPLE__)
  CHECK(cmeta_readiness_backend_supported(SALTS_READINESS_BACKEND_KQUEUE));
  CHECK(cmeta_readiness_reactor_init_kind(
      &reactor, &config, SALTS_READINESS_BACKEND_KQUEUE) == SALTS_OK);
  CHECK(cmeta_readiness_reactor_shutdown(&reactor) == SALTS_OK);
  CHECK(cmeta_readiness_reactor_destroy(&reactor) == SALTS_OK);
# endif
#endif
  return 0;
}
