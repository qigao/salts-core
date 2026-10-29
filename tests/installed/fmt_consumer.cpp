#include <fmt.h>
#include <string>
#include <string_view>
#include <cstring>
int main() {
 char b[64]={0};
 if(fmt(b,sizeof(b),"{}{}",std::string("C"),17)!=3 || std::strcmp(b,"C17")!=0)
   return 1;
 auto arg=FMT_ARG(std::string_view("hello"));
 if(arg.type!=FMT_TYPE_STRV || arg.val.sv.len!=5) return 2;
 tstr s=tstr_format("v={}",std::string("hello"));
 if(!s || std::strcmp(s,"v=hello")!=0) {tstr_free(s);return 3;}
 tstr_free(s);
 return 0;
}
