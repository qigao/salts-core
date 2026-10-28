#include <crypto/sha256.h>
#include <crypto/ed448.h>
#include <gmssl/sm3.h>
#include <crypto/tls.h>
#include <crypto/tls_ext.h>
#include <crypto/tls_peer.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(cond) do { if (!(cond)) { \
  fprintf(stderr,"C11 crypto line %d: %s\n", __LINE__, #cond); \
  return __LINE__; } } while (0)
int main(void) {
  static const uint8_t expected[32] = {
    0x66,0xc7,0xf0,0xf4,0x62,0xee,0xed,0xd9,
    0xd1,0xf2,0xd4,0x6b,0xdc,0x10,0xe4,0xe2,
    0x41,0x67,0xc4,0x87,0x5c,0xf2,0xf7,0xa2,
    0x29,0x7d,0xa0,0x2b,0x8f,0x4b,0xa8,0xe0
  };
  uint8_t digest[32];
  SM3_CTX sm3;
  crypto_tls_provider *provider = NULL;
  CHECK(crypto_tls_abi_version() == CRYPTO_TLS_ABI_VERSION);
  CHECK(crypto_tls_extension_abi_version()
        == CRYPTO_TLS_EXTENSION_ABI_VERSION);
  CHECK(crypto_tls_peer_abi_version() == CRYPTO_TLS_PEER_ABI_VERSION);
  CHECK(crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION, &provider)
        == CRYPTO_TLS_OK && provider != NULL);
  CHECK(crypto_tls_provider_destroy(&provider) == CRYPTO_TLS_OK
        && provider == NULL);
  sm3_init(&sm3);
  sm3_update(&sm3, (const uint8_t*)"abc", 3u);
  sm3_finish(&sm3, digest);
  if (memcmp(digest, expected, sizeof(expected)) != 0) {
    size_t j;
    fprintf(stderr, "SM3(abc) observed=");
    for (j=0u;j<sizeof(digest);++j) fprintf(stderr, "%02x", digest[j]);
    fprintf(stderr, " expected=");
    for (j=0u;j<sizeof(expected);++j) fprintf(stderr, "%02x", expected[j]);
    fprintf(stderr, "\\n");
    return __LINE__;
  }
  /* Independently link canonical SHA256/Ed448 APIs from the installed SDK. */
  {
    static const uint8_t seed[SALTS_CRYPTO_ED448_PRIVATE_KEY_SIZE] = {
      0x6c,0x82,0xa5,0x62,0xcb,0x80,0x8d,0x10,0xd6,0x32,0xbe,0x89,0xc8,0x51,0x3e,
      0xbf,0x6c,0x92,0x9f,0x34,0xdd,0xfa,0x8c,0x9f,0x63,0xc9,0x96,0x0e,0xf6,0xe3,
      0x48,0xa3,0x52,0x8c,0x8a,0x3f,0xcc,0x2f,0x04,0x4e,0x39,0xa3,0xfc,0x5b,0x94,
      0x49,0x2f,0x8f,0x03,0x2e,0x75,0x49,0xa2,0x00,0x98,0xf9,0x5b
    };
    uint8_t pub[SALTS_CRYPTO_ED448_PUBLIC_KEY_SIZE];
    CHECK(salts_crypto_sha256("abc", 3u, digest) == SALTS_CRYPTO_OK);
    CHECK(digest[0] == 0xbau && digest[31] == 0xadu);
    CHECK(salts_crypto_ed448_public_key(seed,pub) == SALTS_CRYPTO_OK);
  }
  return 0;
}
