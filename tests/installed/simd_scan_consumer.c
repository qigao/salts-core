#include <cmeta_simd_scan.h>
int main(void) {
 const char s[]="abc{def}";
 return cmeta_scan_to_char(s,s+8,'{')==s+3 &&
        cmeta_scan_mem(s,8,"def",3)==s+4 ? 0:1;
}
