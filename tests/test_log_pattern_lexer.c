#include <log_pattern_lexer.h>
#include <string.h>
#define CHECK(x) do {if(!(x))return __LINE__;}while(0)
int main(void) {
  const char *src="[{time_ms}] {level} {component}: {message} {bogus}";
  const char *cursor=src,*start=0;
  size_t n=0;
  const log_token_t tokens[] = {
    LOG_TOKEN_TEXT, LOG_TOKEN_TIME_MS, LOG_TOKEN_TEXT,
    LOG_TOKEN_LEVEL, LOG_TOKEN_TEXT, LOG_TOKEN_COMPONENT,
    LOG_TOKEN_TEXT, LOG_TOKEN_MESSAGE, LOG_TOKEN_TEXT,
    LOG_TOKEN_UNKNOWN, LOG_TOKEN_END
  };
  for(size_t i=0;i<sizeof(tokens)/sizeof(tokens[0]);++i) {
    CHECK(log_pattern_scan(&cursor,&start,&n)==tokens[i]);
    if(tokens[i]==LOG_TOKEN_UNKNOWN)
      CHECK(n==7 && memcmp(start,"{bogus}",7)==0);
    if(tokens[i]==LOG_TOKEN_TIME_MS)
      CHECK(n==0);
  }
  const char raw[]={'a','{','f','i','l','e','}','{','x'};
  cursor=raw;
  CHECK(log_pattern_scan_n(&cursor,raw+sizeof(raw),&start,&n)==LOG_TOKEN_TEXT);
  CHECK(n==1 && start==raw);
  CHECK(log_pattern_scan_n(&cursor,raw+sizeof(raw),&start,&n)==LOG_TOKEN_FILE);
  CHECK(log_pattern_scan_n(&cursor,raw+sizeof(raw),&start,&n)==LOG_TOKEN_TEXT);
  CHECK(n==1 && *start=='{');
  CHECK(log_pattern_scan_n(&cursor,raw+sizeof(raw),&start,&n)==LOG_TOKEN_TEXT);
  CHECK(n==1 && *start=='x');
  CHECK(log_pattern_scan_n(&cursor,raw+sizeof(raw),&start,&n)==LOG_TOKEN_END);
  cursor="{x{y}";
  CHECK(log_pattern_scan(&cursor,&start,&n)==LOG_TOKEN_UNKNOWN);
  CHECK(n==5 && memcmp(start,"{x{y}",5)==0);
  return 0;
}
