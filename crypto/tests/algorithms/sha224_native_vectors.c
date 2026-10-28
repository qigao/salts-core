/* SPDX-License-Identifier: Apache-2.0
 * FIPS 180-4 SHA224 vectors plus Python hashlib independently generated
 * 224-bit oracles. Built in installed Crypto and independent zero-provider C11.
 */
#include <crypto/sha224.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define REQUIRE(x) do { if (!(x)) { \
    fprintf(stderr,"Native SHA224 KAT line %d: %s\n",__LINE__,#x); \
    return 1; } } while (0)
static int hex_equals(const uint8_t digest[28], const char *expected) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0u; i < SALTS_CRYPTO_SHA224_DIGEST_SIZE; ++i) {
        if (expected[2u*i] != hex[digest[i] >> 4u] ||
            expected[2u*i+1u] != hex[digest[i] & 0x0fu]) return 0;
    }
    return expected[56] == '\0';
}
static int boundaries(void) {
    static const struct {size_t length; const char *expected;} cases[] = {
        {0u, "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"},
        {1u, "5163813f83b274ddec8a715308724a49b4f32552a2f8b529e28beb4d"},
        {55u, "0ac3e027f4375116ab5ffc5d1fa76fde63555acf026e80aabe13f172"},
        {56u, "503b984c5bc23cca1a0ceacb72c639643998151a5404eec10b9fbcc2"},
        {57u, "ba9e98e7e21bc57d17883f96a3cc5ee7df5725b05144b37ac390996c"},
        {63u, "6bfa43bcc6855d862163ea91afe19d467e9ac9766d14f117c603b085"},
        {64u, "f2387e6e922990751b5f20538a0d737528d9e446dbc8536e1bc5ee61"},
        {65u, "073a06492f70ac7fc84cb5ad63f0ef85c7a8c423d37f65875f754aed"},
        {119u, "1d472599f7dfd7c6463353b581c0fad10c94a0196b0c6293b17a9717"},
        {120u, "8322c666985ab20fcdc43df3434b49a6817f44c87b2e675e0ee8285f"},
        {121u, "b73a24a56d3358115178382bf5a1e32891b254d5397c1712fd8a71d8"},
        {127u, "6d276c705809d1ce2919282964f551d55bb43a5d118193f64d720950"},
        {128u, "9bfd1ebf720793f01a51b37dbd914c5a4d9612f5736621542290b52e"},
        {129u, "0bf9ff5131a8f964bcad74a5e1e1a29449b03da0052710c1742f0729"},
        {255u, "661fd28244e8a5e26f0a9e78885f09df67eafc3690bb8dd85d01c5fd"},
        {256u, "a03a6d0e9e3909858137f51b0b433965122927bb0b48df72bbe2908c"},
        {257u, "aa3e11f72340f9a2e20c1fcfabc4ebd7663db049f37c95e23b5a3f92"},
        {511u, "0f4fe28a8c7ff52a5d306053edeca711386edb2e48beb6ce274977cf"},
        {512u, "ec007f95b44adea2139b89fb8a94d80e9cff23ed8ead0071055f62ba"},
        {513u, "d4555055ddf91298cae731fe709ca02e125986d65755c20d83236bad"},
        {1000u, "2df4a51b10ad16be6d70ba055499685942bf1c2a60997dc199862fed"},
        {4096u, "9c7bb8956f5e8a6bd21a9d2dd3763a1c21b41f8fa3878617cf38dc97"},
    };
    uint8_t data[4096], digest[28];
    for (size_t i=0u;i<sizeof(data);++i) data[i]=(uint8_t)(i*73u+19u);
    for (size_t j=0u;j<sizeof(cases)/sizeof(cases[0]);++j) {
        REQUIRE(salts_crypto_sha224(data,cases[j].length,digest)==SALTS_CRYPTO_OK);
        REQUIRE(hex_equals(digest,cases[j].expected));
    }
    return 0;
}
int main(void) {
    static uint8_t million[1000000];
    static const char long_input[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    uint8_t digest[28], untouched[28];
    memset(million, 'a', sizeof(million));
    REQUIRE(salts_crypto_sha224(NULL,0u,digest)==SALTS_CRYPTO_OK);
    REQUIRE(hex_equals(digest,
        "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"));
    REQUIRE(salts_crypto_sha224("abc",3u,digest)==SALTS_CRYPTO_OK);
    REQUIRE(hex_equals(digest,
        "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"));
    REQUIRE(salts_crypto_sha224(long_input,sizeof(long_input)-1u,digest)
        ==SALTS_CRYPTO_OK);
    REQUIRE(hex_equals(digest,
        "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525"));
    REQUIRE(salts_crypto_sha224(million,sizeof(million),digest)==SALTS_CRYPTO_OK);
    REQUIRE(hex_equals(digest,
        "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67"));
    REQUIRE(boundaries()==0);
    memset(untouched,0xa5,sizeof(untouched));
    REQUIRE(salts_crypto_sha224(NULL,1u,untouched)==SALTS_CRYPTO_EINVAL);
    for (size_t i=0u;i<sizeof(untouched);++i) REQUIRE(untouched[i]==0xa5u);
    REQUIRE(salts_crypto_sha224("x",1u,NULL)==SALTS_CRYPTO_EINVAL);
    puts("PASS SHA224 FIPS+22 independent Python hashlib vectors");
    return 0;
}
