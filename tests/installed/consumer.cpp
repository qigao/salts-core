#include <salts_core/bytes.h>
#include <salts/deadline_queue.h>
#include <salts/spsc_ring.h>
#include <salts/thread.h>

#include <cstring>

int main() {
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
  return 0;
}
