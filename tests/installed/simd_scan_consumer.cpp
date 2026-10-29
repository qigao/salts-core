#include <cmeta_simd_scan.h>
#include <cstring>
int main() {
 const char raw[]={'x','a','\0','y'};
 const char *end=raw+sizeof(raw);
 return cmeta_scan_char(raw,end,'\0')==raw+2 &&
        cmeta_scan_to_any3(raw,end,'z','y','!')==raw+3 ? 0:1;
}
