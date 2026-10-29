#include <fmt.h>
#include <string.h>
int main(void) {
 char b[64]={0};
 if(fmt(b,sizeof(b),"{}:{}",42,"ok")!=5 || strcmp(b,"42:ok")!=0) return 1;
 fmt_arg_t tagged=FMT_ARG(17u);
 if(tagged.type!=FMT_TYPE_UINT || tagged.val.u!=17u) return 2;
 tstr s=tstr_format("hex={:04X}",15u);
 if(!s || strcmp(s,"hex=000F")!=0) {tstr_free(s); return 3;}
 tstr_free(s);
 return 0;
}
