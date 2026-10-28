/* SPDX-License-Identifier: Apache-2.0
 * Independent C++17 consumer of the one installed Salts::Crypto SHA224 ABI.
 * The historic PR43 consumer.cpp fixture must remain byte-identical.
 */
#include <crypto/sha224.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

static_assert(SALTS_CRYPTO_SHA224_DIGEST_SIZE == 28u,
              "canonical SHA224 224-bit public digest ABI");

int main() {
    static constexpr std::uint8_t expected[28] = {
        0x23u,0x09u,0x7du,0x22u,0x34u,0x05u,0xd8u,0x22u,
        0x86u,0x42u,0xa4u,0x77u,0xbdu,0xa2u,0x55u,0xb3u,
        0x2au,0xadu,0xbcu,0xe4u,0xbdu,0xa0u,0xb3u,0xf7u,
        0xe3u,0x6cu,0x9du,0xa7u
    };
    std::array<std::uint8_t, SALTS_CRYPTO_SHA224_DIGEST_SIZE> hash{};
    if (salts_crypto_sha224("abc", 3u, hash.data()) != SALTS_CRYPTO_OK)
        return 1;
    if (std::memcmp(hash.data(), expected, hash.size()) != 0)
        return 2;
    return 0;
}
