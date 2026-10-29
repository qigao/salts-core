/* Independent installed C++17 consumer of the original public regex C11 ABI. */
#include "re.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<re_limits_t>);
static_assert(std::is_standard_layout_v<re_match_result_t>);
static_assert(std::is_same_v<decltype(&re_match_n),
    re_status_t (*)(const char *, std::size_t, const char *, std::size_t,
                    const re_limits_t *, re_match_result_t *)>);

int main()
{
    re_limits_t limits = re_limits_default();
    re_match_result_t match = {RE_NPOS, 0u};
    if (limits.struct_size != sizeof(limits)) return 1;
    constexpr char data[] = {'!', '\0', 'a', 'a', 'b'};
    if (re_match_n("a+b", 3, data, sizeof(data), &limits, &match)
        != RE_STATUS_OK) return 2;
    return (match.index == 2 && match.length == 3) ? 0 : 3;
}
