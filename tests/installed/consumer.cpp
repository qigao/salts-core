#include <ring_buffer.h>
#include <cmeta_buffer.h>
#include <salts_core/bytes.h>
#include <salts_core/runtime_abi.h>
#include <salts/native_io.h>
#include <salts/native_io_sharded.h>
#include <coro.h>
#include <coro_executor.h>
#include <salts/deadline_queue.h>
#include <salts/spsc_ring.h>
#include <salts/thread.h>

#include <cstring>

int main() {
  salts_core_runtime_abi_0_1_required();
  if (salts_core_runtime_abi_version() != SALTS_CORE_RUNTIME_ABI_VERSION ||
      salts_core_runtime_mem_pool_size() != sizeof(mem_pool_t) ||
      salts_core_runtime_mem_buffer_size() != sizeof(mem_buffer_t) ||
      salts_core_runtime_mem_slice_size() != sizeof(mem_slice_t)) return 9;
  salts_core_bytes_view view{};
  cmeta_spsc_ring ring{};
  unsigned char storage[8]{};
  cmeta_deadline_queue queue{};

  if (salts_core_bytes_view_init(&view, "hello", 5) != SALTS_CORE_OK ||
      view.len != 5 || std::memcmp(view.data, "hello", 5) != 0) return 1;
  if (!cmeta_spsc_ring_init(&ring, storage, sizeof(storage)) ||
      cmeta_spsc_ring_write_available(&ring) != 7u) return 2;
  if (cmeta_deadline_queue_init(&queue, 2u) != SALTS_OK ||
      cmeta_deadline_queue_destroy(&queue) != SALTS_OK) return 3;
  if (coro_executor_current() != nullptr || coro_state(nullptr) != coro_DEAD) return 4;
  if (native_io_request_valid(native_io_request{}) ||
      native_io_sharded_endpoint_valid(native_io_sharded_endpoint{})) return 5;
  mem_pool_t pool{};
  if (mem_init(&pool, 0) != 0) return 6;
  mem_buffer_t *buffer = mem_get_buffer(&pool, 3u);
  if (!buffer || mem_buffer_capacity(buffer) < 3u) return 7;
  mem_buffer_release(buffer);
  mem_destroy(&pool);
  unsigned char area[4]{};
  ring_data_type bipartite{};
  ring_init(&bipartite, area, sizeof(area));
  if (ring_write_acquire(&bipartite, 1u) == nullptr) return 8;
  return 0;
}
