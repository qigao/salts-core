#include <fmt_lexer.h>
#include <string.h>
int main(void) {
  const char *s="txt:{}";
  const char *cursor=s,*begin=NULL;
  size_t len=0;
  if(fmt_scan(&cursor,&begin,&len)!=FMT_TOKEN_TEXT || len!=4 ||
     memcmp(begin,"txt:",4)!=0) return 1;
  if(fmt_scan(&cursor,&begin,&len)!=FMT_TOKEN_PLACEHOLDER) return 2;
  return fmt_scan(&cursor,&begin,&len)==FMT_TOKEN_END ? 0 : 3;
}
