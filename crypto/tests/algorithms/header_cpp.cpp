#include <crypto/sha256.h>
#include <crypto/sha512.h>
#include <crypto/ed448.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(SALTS_CRYPTO_ED448_PRIVATE_KEY_SIZE == 57U);
static_assert(SALTS_CRYPTO_ED448_PUBLIC_KEY_SIZE == 57U);
static_assert(SALTS_CRYPTO_ED448_SIGNATURE_SIZE == 114U);
static_assert(SALTS_CRYPTO_SHA256_DIGEST_SIZE == 32U);
static_assert(
    std::is_same_v<decltype(&salts_crypto_ed448_verify),
                   int (*)(const std::uint8_t *, const void *, std::size_t, const std::uint8_t *)>);
static_assert(std::is_same_v<decltype(&salts_crypto_sha256_update),
                             int (*)(salts_crypto_sha256_ctx_t *, const void *, std::size_t)>);

static_assert(SALTS_CRYPTO_SHA384_DIGEST_SIZE == 48U);
static_assert(SALTS_CRYPTO_SHA512_DIGEST_SIZE == 64U);
static_assert(sizeof(salts_crypto_sha512_ctx_t) >= SALTS_CRYPTO_SHA512_CONTEXT_SIZE);
static_assert(std::is_same_v<decltype(&salts_crypto_sha384_update),
              int (*)(salts_crypto_sha384_ctx_t *, const void *, std::size_t)>);
static_assert(std::is_same_v<decltype(&salts_crypto_sha512_update),
              int (*)(salts_crypto_sha512_ctx_t *, const void *, std::size_t)>);
int main() {
    std::uint8_t digest[64] = {};
    if (salts_crypto_sha384("abc", 3u, digest) != SALTS_CRYPTO_OK ||
        digest[0] != 0xcbu || digest[1] != 0x00u) return 1;
    if (salts_crypto_sha512("abc", 3u, digest) != SALTS_CRYPTO_OK ||
        digest[0] != 0xddu || digest[1] != 0xafu) return 1;
    return 0;
}
