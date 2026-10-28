#include <fmt.h>
#include <string.h>
int main(void) {
  char b[64]={0};
  fmt_arg_t args[]={fmt_arg_int(7),fmt_arg_uint(15u)};
  if(fmt_print(b,sizeof(b),"n={} x={:04X}",args,2)!=10) return 1;
  if(strcmp(b,"n=7 x=000F")!=0) return 2;
  const char bytes[]={'[','{','}',']'};
  fmt_arg_t x=fmt_arg_int(42);
  if(fmt_print_v(b,sizeof(b),vstr_from_buf(bytes,sizeof(bytes)),&x,1)!=4 ||
     strcmp(b,"[42]")!=0) return 3;
  fmt_type_t kind=FMT_TYPE_NONE;
  if(!fmt_type_t_from_string("FMT_TYPE_STR",&kind) || kind!=FMT_TYPE_STR ||
     strcmp(fmt_type_t_to_string(kind),"str")!=0) return 4;
  tstr s=fmt_print_tstr(NULL,"a={}",args,1);
  if(!s || strcmp(s,"a=7")!=0) { tstr_free(s);return 5; }
  tstr_free(s);
  return 0;
}
