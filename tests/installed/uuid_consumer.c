#include <cmeta_uuid.h>
int main(void) {
  cmeta_uuid_t u={{0}},v={{0}};
  char buf[SALTS_UUID_STRING_SIZE]={0};
  if(cmeta_uuid_v7_generate(&u)!=SALTS_OK || cmeta_uuid_format(&u,buf,sizeof(buf))!=SALTS_OK) return 1;
  return cmeta_uuid_parse(buf,&v)==SALTS_OK && cmeta_uuid_equal(&u,&v) ? 0 : 2;
}
