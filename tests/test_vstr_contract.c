#include <vstr.h>
#include <stddef.h>
#include <string.h>

_Static_assert(offsetof(vstr, data) == 0, "borrowed data starts at byte zero");
_Static_assert(offsetof(vstr, len) == sizeof(const char *),
    "view contains only pointer and length");
_Static_assert(sizeof(vstr) == sizeof(const char *) + sizeof(size_t),
    "view has no hidden reflection metadata or owner");

int main(void) {
    const char not_terminated[3] = {'a', '\0', 'b'};
    const vstr view = vstr_from_buf(not_terminated, sizeof(not_terminated));
    const vstr empty = vstr_from_cstr(NULL);
    const vstr invalid = vstr_from_buf(NULL, 2u);
    const vstr text = vstr_from_cstr("abc");
    if (!vstr_is_valid(view) || view.data != not_terminated ||
        view.len != sizeof(not_terminated) ||
        memcmp(view.data, not_terminated, view.len) != 0) return 1;
    if (!vstr_is_valid(empty) || empty.len != 0u) return 2;
    if (vstr_is_valid(invalid) || invalid.len != 2u) return 3;
    if (!vstr_is_valid(text) || text.len != 3u) return 4;
    return 0;
}
