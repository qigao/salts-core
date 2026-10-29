#include <tstr.h>
#include <str.h>
#include <type_traits>
#include <cstring>
static_assert(std::is_standard_layout<vstr>::value, "borrowed native C layout");
int main() {
  tstr s = tstr_dup("C++");
  if (!s) return 1;
  if (!tstr_append(&s, VSTR_LIT("17"))) { tstr_free(s); return 2; }
  bool ok = tstr_len(s) == 5 && std::strcmp(s,"C++17")==0;
  tstr_free(s);
  return ok ? 0 : 3;
}
