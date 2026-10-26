#include <vstr.h>
#include <string.h>
int main(void) {
    const char raw[] = {'a', '\0', 'b'};
    vstr value = vstr_from_buf(raw, sizeof(raw));
    vstr invalid = vstr_from_buf(NULL, 4u);
    return value.len == sizeof(raw) && vstr_is_valid(value) &&
           value.data == raw && !vstr_is_valid(invalid) &&
           invalid.len == 4u && vstr_from_cstr("ok").len == 2u ? 0 : 1;
}
