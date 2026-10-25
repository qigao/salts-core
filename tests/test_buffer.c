#include <cmeta_buffer.h>
#include <salts/error_codes.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void external_release(void *data, void *ctx) {
  unsigned *calls = (unsigned *)ctx;
  ++*calls;
  free(data);
}

int main(void) {
  mem_pool_t pool = {0};
  mem_buffer_t *buffer = NULL;
  mem_buffer_t *external = NULL;
  mem_slice_t slice = {0};
  char *owned = NULL;
  unsigned released = 0;
  char *storage = NULL;

  if (mem_init(&pool, 0) != 0) return 1;
  buffer = mem_get_buffer(&pool, 32u);
  if (buffer == NULL || mem_buffer_capacity(buffer) < 32u ||
      mem_buffer_ref_count(buffer) != 1u || mem_is_external(buffer)) return 2;

  memcpy(mem_buffer_data(buffer), "buffer", 6u);
  mem_set_used(buffer, 6u);
  slice = mem_slice(buffer, 1u, 99u);
  if (slice.length != 5u || memcmp(slice.data, "uffer", 5u) != 0 ||
      mem_buffer_ref_count(buffer) != 2u) return 3;
  mem_buffer_release(buffer); /* Slice still owns buffer. */
  if (slice.data == NULL || memcmp(slice.data, "uffer", 5u) != 0)
    return 4;
  mem_slice_release(&slice);
  if (slice.buffer != NULL || slice.data != NULL || slice.length != 0u)
    return 5;

  storage = (char *)malloc(4u);
  if (!storage) return 6;
  memcpy(storage, "abcd", 4u);
  external = mem_wrap_external(storage, 4u, external_release, &released);
  if (!external) { free(storage); return 7; }
  if (!mem_is_external(external) || mem_buffer_used(external) != 4u)
    return 8;
  mem_buffer_retain(external);
  mem_buffer_release(external);
  if (released != 0u) return 9;
  mem_buffer_release(external);
  if (released != 1u) return 10;

  owned = mem_strdup(&pool, "ok");
  if (owned == NULL || strcmp(owned, "ok") != 0) return 11;
  mem_free(&pool, owned);
  if (mem_alloc_array(&pool, SIZE_MAX, 2u) != NULL) return 12;
  mem_destroy(&pool);
  return 0;
}
