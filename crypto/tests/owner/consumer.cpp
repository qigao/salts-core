#include <crypto/sha256.h>
#include <crypto/ed448.h>
#include <gmssl/sm3.h>
#include <crypto/tls.h>
#include <crypto/tls_ext.h>
#include <crypto/tls_peer.h>
#include <crypto/tls_auth.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <type_traits>

static_assert(std::is_standard_layout_v<crypto_tls_auth_options_v1_3>,
              "C11/C++17 additive TLS auth descriptor must remain standard layout");
int main() {
  if (crypto_tls_auth_abi_version() != CRYPTO_TLS_AUTH_ABI_VERSION)
    return 9;
  if (crypto_tls_extension_abi_version() != CRYPTO_TLS_EXTENSION_ABI_VERSION)
    return 7;
  if (crypto_tls_peer_abi_version() != CRYPTO_TLS_PEER_ABI_VERSION)
    return 8;
  if (crypto_tls_abi_version() != CRYPTO_TLS_ABI_VERSION)
    return 4;
  crypto_tls_provider* instance = nullptr;
  if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &instance)
      != CRYPTO_TLS_OK || instance == nullptr)
    return 5;
  if (crypto_tls_provider_destroy(&instance) != CRYPTO_TLS_OK
      || instance != nullptr)
    return 6;
  std::array<std::uint8_t, SM3_DIGEST_SIZE> out{};
  static constexpr std::uint8_t expected[] = {
    0x66,0xc7,0xf0,0xf4,0x62,0xee,0xed,0xd9,
    0xd1,0xf2,0xd4,0x6b,0xdc,0x10,0xe4,0xe2,
    0x41,0x67,0xc4,0x87,0x5c,0xf2,0xf7,0xa2,
    0x29,0x7d,0xa0,0x2b,0x8f,0x4b,0xa8,0xe0
  };
  SM3_CTX ctx{};
  sm3_init(&ctx);
  sm3_update(&ctx, reinterpret_cast<const uint8_t*>("abc"), 3u);
  sm3_finish(&ctx, out.data());
  if (std::memcmp(out.data(), expected, out.size())) {
    std::fprintf(stderr, "C++17 SM3(abc) observed=");
    for(auto byte:out) std::fprintf(stderr, "%02x", byte);
    std::fprintf(stderr, "\\n");
    return 3;
  }
  /* Independently link canonical SHA256/Ed448 APIs from the installed SDK. */
  static constexpr std::uint8_t seed[SALTS_CRYPTO_ED448_PRIVATE_KEY_SIZE] = {
      0x6c,0x82,0xa5,0x62,0xcb,0x80,0x8d,0x10,0xd6,0x32,0xbe,0x89,0xc8,0x51,0x3e,
      0xbf,0x6c,0x92,0x9f,0x34,0xdd,0xfa,0x8c,0x9f,0x63,0xc9,0x96,0x0e,0xf6,0xe3,
      0x48,0xa3,0x52,0x8c,0x8a,0x3f,0xcc,0x2f,0x04,0x4e,0x39,0xa3,0xfc,0x5b,0x94,
      0x49,0x2f,0x8f,0x03,0x2e,0x75,0x49,0xa2,0x00,0x98,0xf9,0x5b
  };
  std::array<std::uint8_t, SALTS_CRYPTO_SHA256_DIGEST_SIZE> sha{};
  std::array<std::uint8_t, SALTS_CRYPTO_ED448_PUBLIC_KEY_SIZE> pub{};
  if (salts_crypto_sha256("abc", 3u, sha.data()) != SALTS_CRYPTO_OK
      || sha[0] != 0xbau || sha[31] != 0xadu)
    return 9;
  if (salts_crypto_ed448_public_key(seed,pub.data()) != SALTS_CRYPTO_OK)
    return 10;
  return 0;
}
