#include <log_pattern_lexer.h>
#include <cstring>
int main() {
 const char raw[]={'{','t','h','r','e','a','d','}'};
 const char *cur=raw,*start=nullptr;
 std::size_t len=0;
 if(log_pattern_scan_n(&cur,raw+sizeof(raw),&start,&len)!=LOG_TOKEN_THREAD)
   return 1;
 return log_pattern_scan_n(&cur,raw+sizeof(raw),&start,&len)==LOG_TOKEN_END ? 0 : 2;
}
