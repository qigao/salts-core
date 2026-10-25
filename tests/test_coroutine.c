#include <coro.h>
#include <coro_executor.h>
#include <salts/error_codes.h>

#include <stdatomic.h>

typedef struct probe {
  atomic_int started;
  atomic_int finished;
  atomic_int cancelled;
  atomic_int finalized;
} probe;

static void manual_entry(coro_t *co, void *arg) {
  int *steps = (int *)arg;
  (void)co;
  *steps = 1;
  (void)coro_yield();
  *steps = 2;
}

static void executor_entry(coro_t *co, void *arg) {
  probe *state = (probe *)arg;
  (void)co;
  atomic_fetch_add(&state->started, 1);
  atomic_fetch_add(&state->finished, 1);
}

static void executor_cancel(void *arg, int status) {
  probe *state = (probe *)arg;
  (void)status;
  atomic_fetch_add(&state->cancelled, 1);
}

static void executor_final(void *arg) {
  probe *state = (probe *)arg;
  atomic_fetch_add(&state->finalized, 1);
}

int main(void) {
  int steps = 0;
  coro_t *co = coro_create(manual_entry, &steps, NULL);
  if (co == NULL || !coro_alive(co) || steps != 0) return 1;
  if (coro_resume(co) != 0 || steps != 1 || coro_state(co) != coro_SUSPENDED)
    return 2;
  if (coro_resume(co) != 0 || steps != 2 || coro_state(co) != coro_DEAD)
    return 3;
  coro_destroy(co);

  {
    coro_executor_config_t config = CORO_EXECUTOR_CONFIG_DEFAULT;
    coro_executor_stats_t stats = {0};
    probe state;
    coro_executor_t *executor;
    coro_executor_task_t task = {
        executor_entry, executor_cancel, executor_final, &state};
    atomic_init(&state.started, 0);
    atomic_init(&state.finished, 0);
    atomic_init(&state.cancelled, 0);
    atomic_init(&state.finalized, 0);
    config.worker_count = 1u;
    config.queue_capacity_per_worker = 2u;
    config.coroutine_pool.max_capacity = 2u;
    executor = coro_executor_create(&config);
    if (executor == NULL) return 4;
    if (coro_executor_try_submit(executor, &task) != SALTS_OK) return 5;
    if (coro_executor_shutdown(executor) != SALTS_OK ||
        coro_executor_wait(executor) != SALTS_OK) return 6;
    coro_executor_get_stats(executor, &stats);
    if (atomic_load(&state.started) != 1 ||
        atomic_load(&state.finished) != 1 ||
        atomic_load(&state.cancelled) != 0 ||
        atomic_load(&state.finalized) != 1 ||
        stats.active_tasks != 0u) return 7;
    if (coro_executor_destroy(executor) != SALTS_OK) return 8;
  }
  if (coro_executor_yield() != SALTS_EINVAL) return 9;
  return 0;
}
