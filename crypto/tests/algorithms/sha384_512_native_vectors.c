/* SPDX-License-Identifier: Apache-2.0
 * Native SHA384/512 known-answer and lifecycle regressions. Uses exactly
 * the canonical Salts::Crypto algorithm functions in both builds:
 * one installed/archive consumer and one no-provider C11 source consumer.
 */
#include <crypto/sha512.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "SHA384/512 test line %d: %s\n", __LINE__, #x); \
    return 1; } } while (0)

static int digest_eq(const uint8_t *digest, size_t length, const char *expected) {
    static const char hex[] = "0123456789abcdef";
    size_t i;
    for (i=0u; i<length; ++i) {
        if (hex[digest[i]>>4u] != expected[2u*i] ||
            hex[digest[i]&15u] != expected[2u*i+1u]) return 0;
    }
    return expected[2u*length]=='\0';
}
static void print_digest(const uint8_t *digest, size_t count) {
    for (size_t i=0u;i<count;++i) printf("%02x",(unsigned)digest[i]);
    putchar('\n');
}
/* All lengths use the same deterministic pattern as the Python oracle. */
static int oracle_mode(size_t length) {
    uint8_t *payload, d384[48], d512[64];
    payload=(uint8_t *)malloc(length?length:1u);
    CHECK(payload!=NULL);
    for (size_t i=0u;i<length;++i) payload[i]=(uint8_t)(i*73u+19u);
    CHECK(salts_crypto_sha384(payload,length,d384)==SALTS_CRYPTO_OK);
    CHECK(salts_crypto_sha512(payload,length,d512)==SALTS_CRYPTO_OK);
    print_digest(d384,sizeof(d384));
    print_digest(d512,sizeof(d512));
    free(payload);
    return 0;
}
static int stream_cases(void) {
    const size_t lengths[]={0u,1u,2u,55u,56u,57u,63u,64u,65u,111u,112u,
        113u,119u,120u,121u,127u,128u,129u,200u,255u,256u,257u,
        511u,512u,513u,1000u,4096u};
    uint8_t payload[4096],expected384[48],expected512[64],actual[64];
    salts_crypto_sha384_ctx_t a;
    salts_crypto_sha512_ctx_t b;
    size_t index,offset,take;
    for (index=0u;index<sizeof(payload);++index)
        payload[index]=(uint8_t)(index*73u+19u);
    for (index=0u;index<sizeof(lengths)/sizeof(lengths[0]);++index) {
        const size_t length=lengths[index];
        CHECK(salts_crypto_sha384(payload,length,expected384)==SALTS_CRYPTO_OK);
        CHECK(salts_crypto_sha512(payload,length,expected512)==SALTS_CRYPTO_OK);
        CHECK(salts_crypto_sha384_init(&a)==SALTS_CRYPTO_OK);
        CHECK(salts_crypto_sha512_init(&b)==SALTS_CRYPTO_OK);
        offset=0u;
        while (offset<length) {
            take=1u+(offset%97u);
            if (take>length-offset) take=length-offset;
            CHECK(salts_crypto_sha384_update(&a,payload+offset,take)==SALTS_CRYPTO_OK);
            CHECK(salts_crypto_sha512_update(&b,payload+offset,take)==SALTS_CRYPTO_OK);
            offset+=take;
        }
        CHECK(salts_crypto_sha384_update(&a,NULL,0u)==SALTS_CRYPTO_OK);
        CHECK(salts_crypto_sha512_update(&b,NULL,0u)==SALTS_CRYPTO_OK);
        CHECK(salts_crypto_sha384_final(&a,actual)==SALTS_CRYPTO_OK);
        CHECK(memcmp(actual,expected384,48u)==0);
        CHECK(salts_crypto_sha512_final(&b,actual)==SALTS_CRYPTO_OK);
        CHECK(memcmp(actual,expected512,64u)==0);
        CHECK(salts_crypto_sha384_final(&a,actual)==SALTS_CRYPTO_ESTATE);
        CHECK(salts_crypto_sha512_final(&b,actual)==SALTS_CRYPTO_ESTATE);
        CHECK(salts_crypto_sha384_update(&a,"x",1u)==SALTS_CRYPTO_ESTATE);
        CHECK(salts_crypto_sha512_update(&b,"x",1u)==SALTS_CRYPTO_ESTATE);
    }
    return 0;
}
int main(int argc,char **argv) {
    static const char *abc384=
        "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5"
        "bed8086072ba1e7cc2358baeca134c825a7";
    static const char *abc512=
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55"
        "d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f";
    static const char *million384=
        "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5"
        "b07b8b3dc38ecc4ebae97ddd87f3d8985";
    static const char *million512=
        "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973"
        "ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b";
    uint8_t d384[48],d512[64],untouched[64],million[1000000];
    salts_crypto_sha384_ctx_t a={0};
    salts_crypto_sha512_ctx_t b={0};
    if (argc==3 && strcmp(argv[1],"--oracle")==0) {
        char *end=NULL;
        unsigned long long n=strtoull(argv[2],&end,10);
        if (argv[2][0]=='\0'||!end||*end!='\0'||n>1000000ull) return 2;
        return oracle_mode((size_t)n);
    }
    CHECK(argc==1);
    CHECK(salts_crypto_sha384("abc",3u,d384)==SALTS_CRYPTO_OK);
    CHECK(digest_eq(d384,sizeof(d384),abc384));
    CHECK(salts_crypto_sha512("abc",3u,d512)==SALTS_CRYPTO_OK);
    CHECK(digest_eq(d512,sizeof(d512),abc512));
    memset(million,'a',sizeof(million));
    CHECK(salts_crypto_sha384(million,sizeof(million),d384)==SALTS_CRYPTO_OK);
    CHECK(digest_eq(d384,sizeof(d384),million384));
    CHECK(salts_crypto_sha512(million,sizeof(million),d512)==SALTS_CRYPTO_OK);
    CHECK(digest_eq(d512,sizeof(d512),million512));
    memset(untouched,0xa5u,sizeof(untouched));
    CHECK(salts_crypto_sha384(NULL,1u,untouched)==SALTS_CRYPTO_EINVAL);
    CHECK(salts_crypto_sha512(NULL,1u,untouched)==SALTS_CRYPTO_EINVAL);
    for (size_t i=0u;i<sizeof(untouched);++i) CHECK(untouched[i]==0xa5u);
    CHECK(salts_crypto_sha384("x",1u,NULL)==SALTS_CRYPTO_EINVAL);
    CHECK(salts_crypto_sha512("x",1u,NULL)==SALTS_CRYPTO_EINVAL);
    CHECK(salts_crypto_sha384_init(&a)==SALTS_CRYPTO_OK);
    CHECK(salts_crypto_sha512_init(&b)==SALTS_CRYPTO_OK);
    CHECK(salts_crypto_sha384_final(&b,untouched)==SALTS_CRYPTO_ESTATE);
    CHECK(salts_crypto_sha512_final(&a,untouched)==SALTS_CRYPTO_ESTATE);
    for (size_t i=0u;i<sizeof(untouched);++i) CHECK(untouched[i]==0xa5u);
    CHECK(salts_crypto_sha384_final(&a,untouched)==SALTS_CRYPTO_OK);
    CHECK(salts_crypto_sha512_final(&b,untouched)==SALTS_CRYPTO_OK);
    CHECK(salts_crypto_sha384_final(&a,untouched)==SALTS_CRYPTO_ESTATE);
    CHECK(salts_crypto_sha512_final(&b,untouched)==SALTS_CRYPTO_ESTATE);
    CHECK(stream_cases()==0);
    puts("PASS SHA384/SHA512 FIPS and 27 boundary-streaming vectors");
    return 0;
}
