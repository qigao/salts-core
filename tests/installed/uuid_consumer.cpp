#include <cmeta_uuid.h>
#include <type_traits>
static_assert(sizeof(Salts::UUID)==16, "Original UUID POD ABI");
int main() {
  Salts::UUID u{};
  char str[SALTS_UUID_STRING_SIZE]{};
  if(cmeta_uuid_v4_generate(&u)!=SALTS_OK) return 1;
  if(cmeta_uuid_format(&u,str,sizeof(str))!=SALTS_OK) return 2;
  Salts::UUID v{};
  return cmeta_uuid_parse(str,&v)==SALTS_OK && cmeta_uuid_equal(&u,&v)?0:3;
}
