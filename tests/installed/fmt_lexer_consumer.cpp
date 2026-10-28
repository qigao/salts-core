#include <fmt_lexer.h>
#include <type_traits>
#include <cstring>
static_assert(std::is_standard_layout<vstr>::value, "C11 view ABI");
int main() {
  const char s[]={'{',':','f','}','z'};
  const char *cur=s;
  vstr token={nullptr,0u};
  if(fmt_scan_v_n(&cur,s+4,&token)!=FMT_TOKEN_SPECIFIER ||
     token.len!=1u || token.data[0]!='f') return 1;
  return fmt_scan_v_n(&cur,s+4,&token)==FMT_TOKEN_END ? 0 : 2;
}
