#include <log_pattern_lexer.h>
#include <string.h>
int main(void) {
 const char *start=0,*cursor="{time}{level}{message}";
 size_t n=0;
 if(log_pattern_scan(&cursor,&start,&n)!=LOG_TOKEN_TIME) return 1;
 if(log_pattern_scan(&cursor,&start,&n)!=LOG_TOKEN_LEVEL) return 2;
 if(log_pattern_scan(&cursor,&start,&n)!=LOG_TOKEN_MESSAGE) return 3;
 return log_pattern_scan(&cursor,&start,&n)==LOG_TOKEN_END ? 0 : 4;
}
