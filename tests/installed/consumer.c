#include <ring_buffer.h>
#include <cmeta_buffer.h>
#include <salts_core/bytes.h>
#include <salts_core/runtime_abi.h>
#include <salts/native_io.h>
#include <coro_executor.h>
#include <salts/clock.h>
#include <salts/deadline_queue.h>
#include <salts/error_codes.h>
#include <salts/random.h>
#include <salts/rcu.h>
#include <salts/thread.h>

#include <string.h>

int main(void) {
  salts_core_bytes_view v = {0};
  salts_core_runtime_abi_0_1_required();
  if (salts_core_runtime_abi_version() != SALTS_CORE_RUNTIME_ABI_VERSION ||
      salts_core_runtime_mem_pool_size() != sizeof(mem_pool_t) ||
      salts_core_runtime_mem_buffer_size() != sizeof(mem_buffer_t) ||
      salts_core_runtime_mem_slice_size() != sizeof(mem_slice_t)) return 11;
  cmeta_deadline_queue q = {0};
  cmeta_deadline_event event = {0};
  cmeta_deadline_id id = 0u;
  unsigned char random_bytes[8] = {0};

  if (salts_core_bytes_view_init(&v, "abc", 3u) != SALTS_CORE_OK ||
      v.len != 3u || memcmp(v.data, "abc", 3u) != 0) return 1;
  if (cmeta_platform_secure_random(random_bytes, sizeof(random_bytes)) != SALTS_OK ||
      cmeta_hrtime() == 0u) return 2;
  if (cmeta_deadline_queue_init(&q, 1u) != SALTS_OK ||
      cmeta_deadline_queue_schedule(&q, 12u, 42u, &id) != SALTS_OK ||
      cmeta_deadline_queue_take_ready(&q, 12u, &event) != SALTS_OK ||
      event.id != id || event.token != 42u ||
      cmeta_deadline_queue_destroy(&q) != SALTS_OK) return 3;
  if (coro_executor_current() != NULL || coro_executor_yield() != SALTS_EINVAL) return 4;
  if (native_io_endpoint_valid((native_io_endpoint){0})) return 5;
  mem_pool_t pool = {0};
  if (mem_init(&pool, 0) != 0) return 6;
  mem_buffer_t *buffer = mem_get_buffer(&pool, 3u);
  if (!buffer || mem_buffer_capacity(buffer) < 3u) return 7;
  mem_buffer_release(buffer);
  mem_destroy(&pool);
  unsigned char area[4] = {0};
  ring_data_type ring = {0};
  ring_init(&ring, area, sizeof(area));
  if (ring_write_acquire(&ring, 1u) == NULL) return 8;
  return 0;
}
