#include <fmt_lexer.h>
#include <string.h>
int main(void) {
  const char raw[6] = {'a','{',':','0','4','}'};
  const char *cursor=raw;
  vstr tok=vstr_from_buf(NULL,0);
  if (fmt_scan_v_n(&cursor,raw+sizeof(raw),&tok)!=FMT_TOKEN_TEXT ||
      tok.len!=1u || tok.data[0]!='a') return 1;
  if (fmt_scan_v_n(&cursor,raw+sizeof(raw),&tok)!=FMT_TOKEN_SPECIFIER ||
      tok.len!=2u || memcmp(tok.data,"04",2u)!=0) return 2;
  if (fmt_scan_v_n(&cursor,raw+sizeof(raw),&tok)!=FMT_TOKEN_END) return 3;
  const char *input="{{}}";
  cursor=input;
  if(fmt_scan_v(&cursor,&tok)!=FMT_TOKEN_LBRACE_ESC) return 4;
  if(fmt_scan_v(&cursor,&tok)!=FMT_TOKEN_RBRACE_ESC) return 5;
  return 0;
}
