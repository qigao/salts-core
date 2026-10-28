/* C++17 installed-style consumer of ONE C11 Salts::Crypto TLS profile ABI.
 * No TLS engine is instantiated, and no GmSSL/OpenSSL headers are included.
 */
#include <crypto/tls_profile.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <cstring>

static_assert(CRYPTO_TLS_PROFILE_ABI_VERSION == UINT32_C(0x00010004));
static_assert(std::is_standard_layout_v<crypto_tls_profile_options_v1_4>);
static_assert(std::is_standard_layout_v<crypto_tls_profile_info_v1_4>);
static_assert(std::is_same_v<decltype(&crypto_tls_profile_create_v1_4),
    crypto_tls_profile_result (*)(const crypto_tls_profile_options_v1_4 *,
                                  crypto_tls_profile **)>);
static_assert(std::is_same_v<decltype(&crypto_tls_profile_copy_alpn_wire_v1_4),
    crypto_tls_profile_result (*)(const crypto_tls_profile *, std::uint8_t *,
                                  std::size_t, std::size_t *)>);

int main() {
    crypto_tls_profile_options_v1_4 o{};
    crypto_tls_profile *owner=nullptr;
    o.struct_size=sizeof(o);
    o.role=CRYPTO_TLS_CLIENT;
    o.security_profile=CRYPTO_TLS_PROFILE_STANDARD;
    o.trust_mode=CRYPTO_TLS_TRUST_SYSTEM;
    o.protocol_min=CRYPTO_TLS_1_2;
    o.protocol_max=CRYPTO_TLS_1_3;
    o.verified_name="127.0.0.1";
    if (crypto_tls_profile_abi_version()!=CRYPTO_TLS_PROFILE_ABI_VERSION)
        return 1;
    if (crypto_tls_profile_create_v1_4(&o,&owner)!=CRYPTO_TLS_PROFILE_OK ||
        owner==nullptr) return 2;
    crypto_tls_profile_info_v1_4 info{};
    info.struct_size=sizeof(info);
    const bool valid=crypto_tls_profile_get_info_v1_4(owner,&info)
        ==CRYPTO_TLS_PROFILE_OK &&
        info.protocol_min==CRYPTO_TLS_1_2 &&
        info.protocol_max==CRYPTO_TLS_1_3;
    if (crypto_tls_profile_destroy_v1_4(&owner)!=CRYPTO_TLS_PROFILE_OK ||
        owner!=nullptr) return 3;
    return valid ? 0 : 4;
}
