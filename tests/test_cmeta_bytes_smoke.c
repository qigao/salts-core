#include <cmeta_bytes.h>
#include <salts/error_codes.h>
#include <string.h>
int main(void) {
  cmeta_bytes_t b=cmeta_bytes_INIT;
  cmeta_bytes_view_t view={0};
  if(cmeta_bytes_init(&b,6)!=SALTS_OK) return 1;
  if(cmeta_bytes_append(&b,"abc",3)!=SALTS_OK ||
     cmeta_bytes_append(&b,"def",3)!=SALTS_OK) return 2;
  if(cmeta_bytes_size(&b)!=6 || cmeta_bytes_available(&b)!=0) return 3;
  if(cmeta_bytes_append(&b,"x",1)!=SALTS_ENOSPC) return 4;
  if(cmeta_bytes_consume(&b,2)!=SALTS_OK) return 5;
  if(cmeta_bytes_view(&b,&view)!=SALTS_OK || view.size!=4 ||
     memcmp(view.data,"cdef",4)!=0) return 6;
  if(cmeta_bytes_append(&b,view.data+1,3)!=SALTS_ENOSPC) return 7;
  if(cmeta_bytes_consume(&b,4)!=SALTS_OK) return 8;
  cmeta_bytes_reset(&b);
  if(cmeta_bytes_size(&b)!=0)return 9;
  cmeta_bytes_destroy(&b);
  return 0;
}
