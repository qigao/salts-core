/* SPDX-License-Identifier: Apache-2.0
 * Independent FIPS 180-4 SHA256 known-answer tests for installed Crypto.
 * No OpenSSL/GmSSL headers or second algorithm provider in this executable.
 */
#include <crypto/sha256.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define REQUIRE(x) do { if (!(x)) { \
    fprintf(stderr, "Native SHA256 KAT line %d: %s\n", __LINE__, #x); \
    return 1; } } while (0)

static int equals_hex(const uint8_t digest[32], const char hex[65]) {
    static const char table[] = "0123456789abcdef";
    for (size_t i = 0u; i < 32u; ++i) {
        if (table[digest[i] >> 4u] != hex[i * 2u] ||
            table[digest[i] & 0x0fu] != hex[i * 2u + 1u]) return 0;
    }
    return hex[64] == '\0';
}

int main(void) {
    static const char million_a[] =
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    static const char long_input[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    uint8_t digest[32];
    salts_crypto_sha256_ctx_t context = {0};
    uint8_t chunk[1000];
    size_t i;
    memset(chunk, 'a', sizeof(chunk));

    REQUIRE(salts_crypto_sha256(NULL, 0u, digest) == SALTS_CRYPTO_OK);
    REQUIRE(equals_hex(digest,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    REQUIRE(salts_crypto_sha256("abc", 3u, digest) == SALTS_CRYPTO_OK);
    REQUIRE(equals_hex(digest,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));

    REQUIRE(salts_crypto_sha256(long_input, sizeof(long_input) - 1u, digest)
            == SALTS_CRYPTO_OK);
    REQUIRE(equals_hex(digest,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));

    REQUIRE(salts_crypto_sha256_init(&context) == SALTS_CRYPTO_OK);
    for (i = 0u; i < sizeof(long_input) - 1u; ++i)
        REQUIRE(salts_crypto_sha256_update(&context, long_input + i, 1u)
                == SALTS_CRYPTO_OK);
    REQUIRE(salts_crypto_sha256_update(&context, NULL, 0u) == SALTS_CRYPTO_OK);
    REQUIRE(salts_crypto_sha256_final(&context, digest) == SALTS_CRYPTO_OK);
    REQUIRE(equals_hex(digest,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    REQUIRE(salts_crypto_sha256_final(&context, digest) == SALTS_CRYPTO_ESTATE);
    REQUIRE(salts_crypto_sha256_update(&context, "x", 1u) == SALTS_CRYPTO_ESTATE);

    REQUIRE(salts_crypto_sha256_init(&context) == SALTS_CRYPTO_OK);
    for (i = 0u; i < 1000u; ++i)
        REQUIRE(salts_crypto_sha256_update(&context, chunk, sizeof(chunk))
                == SALTS_CRYPTO_OK);
    REQUIRE(salts_crypto_sha256_final(&context, digest) == SALTS_CRYPTO_OK);
    REQUIRE(equals_hex(digest, million_a));

    memset(&context, 0, sizeof(context));
    REQUIRE(salts_crypto_sha256_update(&context, "x", 1u) == SALTS_CRYPTO_ESTATE);
    REQUIRE(salts_crypto_sha256_final(&context, digest) == SALTS_CRYPTO_ESTATE);
    REQUIRE(salts_crypto_sha256_init(NULL) == SALTS_CRYPTO_EINVAL);
    REQUIRE(salts_crypto_sha256_update(NULL, "x", 1u) == SALTS_CRYPTO_EINVAL);
    REQUIRE(salts_crypto_sha256(NULL, 1u, digest) == SALTS_CRYPTO_EINVAL);
    REQUIRE(salts_crypto_sha256("x", 1u, NULL) == SALTS_CRYPTO_EINVAL);

    puts("PASS: FIPS SHA256 empty/abc/448-bit/million-byte and streaming owner");
    return 0;
}
