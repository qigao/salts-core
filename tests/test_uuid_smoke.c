#include <cmeta_uuid.h>
#include <string.h>
int main(void) {
  cmeta_uuid_t a={{0}},b={{0}};
  char text[SALTS_UUID_STRING_SIZE]={0};
  if(cmeta_uuid_v4_generate(&a)!=SALTS_OK || (a.bytes[6]&0xf0u)!=0x40u) return 1;
  if(cmeta_uuid_v7_generate(&b)!=SALTS_OK || (b.bytes[6]&0xf0u)!=0x70u) return 2;
  if(cmeta_uuid_format(&a,text,sizeof(text))!=SALTS_OK) return 3;
  if(cmeta_uuid_parse(text,&b)!=SALTS_OK || !cmeta_uuid_equal(&a,&b)) return 4;
  if(cmeta_uuid_parse("malformed",&b)!=SALTS_EINVAL) return 5;
  return 0;
}
