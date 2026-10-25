#include <salts_core/bytes.h>
#include <cstring>
int main() {
  salts_core_bytes_view view{};
  return salts_core_bytes_view_init(&view, "hello", 5) == SALTS_CORE_OK &&
         view.len == 5 && std::memcmp(view.data, "hello", 5) == 0 ? 0 : 1;
}
