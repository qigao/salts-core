/* Original Salts Core-owned mmap C++17 installed SDK consumer.
 * This validates the actual C ABI exported by the one Salts::FS binary.
 */
#include <cmeta_mmap.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <cstring>

static_assert(std::is_standard_layout_v<cmeta_mmap_t>);
static_assert(std::is_standard_layout_v<cmeta_mmap_group_t>);
static_assert(std::is_same_v<decltype(&cmeta_mmap_open),
  int (*)(cmeta_mmap_t *, const char *, int)>);
static_assert(std::is_same_v<decltype(&cmeta_mmap_open_range),
  int (*)(cmeta_mmap_t *, const char *, std::int64_t, std::size_t, int)>);
static_assert(std::is_same_v<decltype(&cmeta_mmap_group_close),
  void (*)(cmeta_mmap_group_t *)>);

int main() {
  cmeta_mmap_t map{};
  cmeta_mmap_init(&map);
  if (cmeta_mmap_is_open(&map)) return 1;
  if (cmeta_mmap_size(&map)!=0u) return 2;
  if (cmeta_mmap_sync(&map,false)!=SALTS_MMAP_EINVAL) return 3;
  cmeta_mmap_close(&map);
  if (cmeta_mmap_page_size()==0u) return 4;
  cmeta_mmap_group_t group{};
  cmeta_mmap_group_init(&group);
  cmeta_mmap_group_close(&group);
  return 0;
}
