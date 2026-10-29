#include <cmeta_bytes.h>
#include <salts/error_codes.h>
#include <type_traits>
#include <cstring>
static_assert(std::is_standard_layout<cmeta_bytes_t>::value,"Native Core accumulator");
int main() {
 cmeta_bytes_t b=cmeta_bytes_INIT;
 cmeta_bytes_view_t v{};
 if(cmeta_bytes_init(&b,7)!=SALTS_OK)return 1;
 if(cmeta_bytes_append(&b,"C++17",5)!=SALTS_OK ||
    cmeta_bytes_view(&b,&v)!=SALTS_OK ||
    v.size!=5 || std::memcmp(v.data,"C++17",5)!=0)return 2;
 cmeta_bytes_destroy(&b);return 0;
}
