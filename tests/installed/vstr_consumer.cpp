#include <vstr.h>
#include <type_traits>
#include <cstddef>
static_assert(std::is_standard_layout<vstr>::value,
              "Core borrowed view must be native C, not reflected CMeta");
static_assert(offsetof(vstr, len) == sizeof(const char *),
              "No metadata or hidden ownership");
int main() {
    vstr value = vstr_from_cstr("native");
    return value.len == 6u && vstr_is_valid(value) ? 0 : 1;
}
