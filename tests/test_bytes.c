#include <bytes.h>

#include <stdint.h>
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
#include <stddef.h>
#include <string.h>

_Static_assert(offsetof(salts_core_bytes_view, data) == 0u,
               "byte view data offset drift");
_Static_assert(offsetof(salts_core_bytes_view, len) == sizeof(const char *),
               "byte view size offset drift");

int main(void) {
  static const char input[] = "a\0bc";
  salts_core_bytes_view root = {"sentinel", 100u};
  salts_core_bytes_view slice = {"unchanged", 17u};

  CHECK(salts_core_bytes_view_init(&root, input, sizeof(input) - 1u) == SALTS_CORE_OK);
  CHECK(root.data == input && root.len == 4u);
  CHECK(salts_core_bytes_view_slice(root, 1u, 3u, &slice) == SALTS_CORE_OK);
  CHECK(slice.len == 3u && memcmp(slice.data, "\0bc", 3u) == 0);

  CHECK(salts_core_bytes_view_slice(root, 5u, 0u, &slice) == SALTS_CORE_EINVAL);
  CHECK(slice.len == 3u && slice.data == input + 1u);
  CHECK(salts_core_bytes_view_slice(root, 1u, SIZE_MAX, &slice) == SALTS_CORE_EINVAL);
  CHECK(slice.len == 3u && slice.data == input + 1u);
  CHECK(salts_core_bytes_view_init(&root, NULL, 1u) == SALTS_CORE_EINVAL);
  CHECK(root.data == input && root.len == 4u);
  CHECK(salts_core_bytes_view_init(&root, NULL, 0u) == SALTS_CORE_OK);
  CHECK(salts_core_bytes_view_slice(root, 0u, 0u, &slice) == SALTS_CORE_OK);
  CHECK(slice.data == NULL && slice.len == 0u);
  return 0;
}
