#include <cmeta_buffer.h>
#include <stdint.h>
#include <string.h>

static void release_external(void *ptr, void *userdata) {
  unsigned *calls = (unsigned *)userdata;
  if (ptr != NULL && calls != NULL) ++*calls;
}
int main(void) {
  mem_pool_t pool = {0};
  mem_buffer_t *buffer;
  mem_slice_t slice;
  char external[5] = {'h','e','l','l','o'};
  unsigned freed = 0;
  if (mem_init(&pool, 0u) != 0) return 1;
  buffer = mem_get_buffer(&pool, 16u);
  if (buffer == NULL || mem_buffer_capacity(buffer) < 16u) return 2;
  memcpy(mem_buffer_data(buffer), "hello", 5u);
  mem_set_used(buffer, 5u);
  slice = mem_slice(buffer, 1u, 3u);
  if (slice.buffer != buffer || slice.length != 3u ||
      memcmp(slice.data, "ell", 3u) != 0 || mem_buffer_ref_count(buffer) != 2u) return 3;
  mem_slice_release(&slice);
  if (slice.buffer != NULL || mem_buffer_ref_count(buffer) != 1u) return 4;
  mem_buffer_release(buffer);
  mem_trim(&pool);
  mem_destroy(&pool);

  buffer = mem_wrap_external(external, sizeof(external), release_external, &freed);
  if (buffer == NULL || !mem_is_external(buffer) ||
      mem_buffer_const_data(buffer) != external) return 5;
  mem_buffer_release(buffer);
  if (freed != 1u) return 6;
  return 0;
}
