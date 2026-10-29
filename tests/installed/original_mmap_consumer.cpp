/* Independent installed C++17 ABI consumer with zero CMeta dependencies. */
#include <cmeta_mmap.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout<cmeta_mmap_t>::value,
              "original native mmap owner needs stable public layout");
static_assert(std::is_same<decltype(&cmeta_mmap_open_range),
                           int (*)(cmeta_mmap_t *, const char *, std::int64_t,
                                   std::size_t, int)>::value,
              "original API signature changed");
static_assert(std::is_same<decltype(&cmeta_mmap_sync_range),
                           int (*)(cmeta_mmap_t *, std::size_t, std::size_t,
                                   bool)>::value,
              "original flush API signature changed");
int main()
{
    cmeta_mmap_t mapping{};
    cmeta_mmap_group_t group{};
    cmeta_mmap_init(&mapping);
    cmeta_mmap_group_init(&group);
    if (cmeta_mmap_open_range(nullptr, "file", 0, 1u, SALTS_MMAP_READ)
        != SALTS_MMAP_EINVAL) return 1;
    if (cmeta_mmap_sync_range(&mapping, 0u, 0u, false)
        != SALTS_MMAP_EINVAL) return 2;
    if (cmeta_mmap_group_open(nullptr, nullptr, 0u, SALTS_MMAP_READ)
        != SALTS_MMAP_EINVAL) return 3;
    cmeta_mmap_unmap(&mapping);
    cmeta_mmap_close(&mapping);
    cmeta_mmap_group_close(&group);
    return cmeta_mmap_page_size() ? 0 : 4;
}
