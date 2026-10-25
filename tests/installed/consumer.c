#include <salts_core/bytes.h>
#include <string.h>
int main(void) {
  salts_core_bytes_view v = {0};
  return salts_core_bytes_view_init(&v, "abc", 3u) == SALTS_CORE_OK &&
         v.len == 3u && memcmp(v.data, "abc", 3u) == 0 ? 0 : 1;
}
