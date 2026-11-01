#include <object_pool.h>
#include <stdint.h>
#include <string.h>
int main(void) {
  object_pool_config_t config = {0};
  config.object_size = sizeof(uint64_t);
  config.initial_capacity = 4u;
  config.max_capacity = 4u;
  config.zero_on_alloc = true;
  object_pool_t *pool = object_pool_create(&config);
  if (!pool) return 1;
  uint64_t *p = (uint64_t *)object_pool_alloc(pool);
  if (!p) { object_pool_destroy(pool); return 2; }
  if (!object_pool_contains(pool, p) || !object_pool_is_allocated(pool, p) ||
      object_pool_allocated_count(pool) != 1u) {
    object_pool_destroy(pool); return 3;
  }
  *p = UINT64_C(0xdeadbeef);
  object_pool_free(pool, p);
  p = (uint64_t *)object_pool_alloc(pool);
  if (!p || *p != 0u || object_pool_allocated_count(pool) != 1u) {
    object_pool_destroy(pool); return 4;
  }
  object_pool_free(pool, p);
  object_pool_destroy(pool);
  return 0;
}
