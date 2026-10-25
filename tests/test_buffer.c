#include <cmeta_buffer.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int main(void) {
  mem_pool_t pool = {0};
  mem_buffer_t *buffer = NULL;
  mem_slice_t slice = {0};
  char *data;

  if (mem_init(&pool, 0u) != 0) return 1;
  data = (char *)mem_alloc(&pool, 8u);
  if (data == NULL) return 2;
  memcpy(data, "core", 4u);
  if (memcmp(data, "core", 4u) != 0) return 3;
  mem_free(&pool, data);
  if (mem_alloc_array(&pool, SIZE_MAX, 2u) != NULL) return 4;

  buffer = mem_get_buffer(&pool, 16u);
  if (buffer == NULL || mem_buffer_capacity(buffer) < 16u ||
      mem_buffer_ref_count(buffer) != 1u) return 5;
  memcpy(mem_buffer_data(buffer), "hello", 5u);
  buffer->used = 5u;
  slice = mem_slice(buffer, 0u, 5u);
  if (slice.length != 5u || slice.buffer != buffer ||
      memcmp(slice.data, "hello", 5u) != 0 ||
      mem_buffer_ref_count(buffer) != 2u) return 6;
  mem_buffer_release(buffer);
  if (memcmp(slice.data, "hello", 5u) != 0) return 7;
  mem_slice_release(&slice);
  if (slice.buffer != NULL || slice.data != NULL) return 8;
  mem_destroy(&pool);
  return 0;
}
