#include <cmeta_bytes.h>
#include <salts/error_codes.h>
#include <string.h>
int main(void) {
  cmeta_bytes_t b=cmeta_bytes_INIT; cmeta_bytes_view_t view={0};
  if(cmeta_bytes_init(&b,32)!=SALTS_OK) return 1;
  if(cmeta_bytes_append(&b,"SDK",3)!=SALTS_OK ||
     cmeta_bytes_view(&b,&view)!=SALTS_OK ||
     view.size!=3 || memcmp(view.data,"SDK",3)!=0) return 2;
  cmeta_bytes_destroy(&b);return 0;
}
