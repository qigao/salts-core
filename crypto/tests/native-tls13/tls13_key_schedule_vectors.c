/* SPDX-License-Identifier: Apache-2.0
 * RFC5869 and RFC8446 native C11 SHA256/SHA384 key schedule conformance.
 * No TLS handshake/record provider is compiled into this executable.
 */
#include "tls13_key_schedule.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if (!(x)) {\
 fprintf(stderr, "FAIL native TLS13 HKDF at line %d: %s\n",__LINE__,#x);\
 return 1;} } while(0)

static void dump_hex(const char *label,const uint8_t *p,size_t n) {
    printf("%s:",label);
    for(size_t i=0u;i<n;++i) printf("%02x",(unsigned)p[i]);
    putchar('\n');
}
static int equal_hex(const uint8_t *p,size_t n,const char *expected) {
    static const char digits[]="0123456789abcdef";
    for(size_t i=0u;i<n;++i)
        if(expected[2u*i]!=digits[p[i]>>4u]||
           expected[2u*i+1u]!=digits[p[i]&15u])return 0;
    return expected[2u*n]=='\0';
}
static int all_a5(const uint8_t *p,size_t n) {
    for(size_t i=0u;i<n;++i)if(p[i]!=0xa5u)return 0;
    return 1;
}

int main(void) {
    static const char rfc5869_sha256_prk[] =
        "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5";
    static const uint8_t label[] = {'d','e','r','i','v','e','d'};
    static const uint8_t context256[]={0x11u,0x22u};
    static const uint8_t context384[]={0x00u,0x01u,0x02u};
    uint8_t salt[13],ikm[22],prk256[32],prk384[48];
    uint8_t expand256[32],expand384[48],long256[97],long384[101];
    uint8_t untouched[128],secret_alias[48];
    uint8_t max_context[255],max_label[250];
    uint8_t long_salt[200], long_salt_prk256[32],long_salt_prk384[48];
    uint8_t empty_salt_prk256[32],empty_salt_prk384[48];

    for(size_t i=0u;i<sizeof(salt);++i)salt[i]=(uint8_t)i;
    memset(ikm,0x0bu,sizeof(ikm));
    for(size_t i=0u;i<sizeof(long_salt);++i)
        long_salt[i]=(uint8_t)((i*11u+7u)&0xffu);
    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA256,long_salt,sizeof(long_salt),
        ikm,sizeof(ikm),long_salt_prk256,sizeof(long_salt_prk256))
        ==CRYPTO_NATIVE_TLS13_OK);
    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA384,long_salt,sizeof(long_salt),
        ikm,sizeof(ikm),long_salt_prk384,sizeof(long_salt_prk384))
        ==CRYPTO_NATIVE_TLS13_OK);
    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA256,NULL,0u,NULL,0u,
        empty_salt_prk256,sizeof(empty_salt_prk256))==CRYPTO_NATIVE_TLS13_OK);
    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA384,NULL,0u,NULL,0u,
        empty_salt_prk384,sizeof(empty_salt_prk384))==CRYPTO_NATIVE_TLS13_OK);


    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA256,salt,sizeof(salt),ikm,sizeof(ikm),
        prk256,sizeof(prk256))==CRYPTO_NATIVE_TLS13_OK);
    CHECK(equal_hex(prk256,sizeof(prk256),rfc5869_sha256_prk));
    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA384,salt,sizeof(salt),ikm,sizeof(ikm),
        prk384,sizeof(prk384))==CRYPTO_NATIVE_TLS13_OK);

    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,prk256,sizeof(prk256),
        label,sizeof(label),context256,sizeof(context256),
        expand256,sizeof(expand256))==CRYPTO_NATIVE_TLS13_OK);
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA384,prk384,sizeof(prk384),
        label,sizeof(label),context384,sizeof(context384),
        expand384,sizeof(expand384))==CRYPTO_NATIVE_TLS13_OK);
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,prk256,sizeof(prk256),
        label,sizeof(label),context256,sizeof(context256),
        long256,sizeof(long256))==CRYPTO_NATIVE_TLS13_OK);
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA384,prk384,sizeof(prk384),
        label,sizeof(label),context384,sizeof(context384),
        long384,sizeof(long384))==CRYPTO_NATIVE_TLS13_OK);

    dump_hex("LONGSALT256",long_salt_prk256,sizeof(long_salt_prk256));
    dump_hex("LONGSALT384",long_salt_prk384,sizeof(long_salt_prk384));
    dump_hex("EMPTYSALT256",empty_salt_prk256,sizeof(empty_salt_prk256));
    dump_hex("EMPTYSALT384",empty_salt_prk384,sizeof(empty_salt_prk384));
    dump_hex("PRK256",prk256,sizeof(prk256));
    dump_hex("PRK384",prk384,sizeof(prk384));
    dump_hex("LBL256",expand256,sizeof(expand256));
    dump_hex("LBL384",expand384,sizeof(expand384));
    dump_hex("LONG256",long256,sizeof(long256));
    dump_hex("LONG384",long384,sizeof(long384));

    memset(untouched,0xa5u,sizeof(untouched));
    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA256,salt,sizeof(salt),ikm,sizeof(ikm),
        untouched,31u)==CRYPTO_NATIVE_TLS13_RANGE);
    CHECK(all_a5(untouched,sizeof(untouched)));
    CHECK(crypto_native_tls13_hkdf_extract(
        (crypto_native_tls13_hash)99,salt,sizeof(salt),ikm,sizeof(ikm),
        untouched,sizeof(untouched))==CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT);
    CHECK(all_a5(untouched,sizeof(untouched)));
    CHECK(crypto_native_tls13_hkdf_extract(
        CRYPTO_NATIVE_TLS13_SHA384,NULL,1u,ikm,sizeof(ikm),
        untouched,sizeof(untouched))==CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT);
    CHECK(all_a5(untouched,sizeof(untouched)));

    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,prk256,31u,label,sizeof(label),
        context256,sizeof(context256),untouched,32u)
        ==CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT);
    CHECK(all_a5(untouched,sizeof(untouched)));
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,prk256,sizeof(prk256),label,250u,
        NULL,0u,untouched,32u)==CRYPTO_NATIVE_TLS13_RANGE);
    CHECK(all_a5(untouched,sizeof(untouched)));
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,prk256,sizeof(prk256),label,sizeof(label),
        context256,256u,untouched,32u)==CRYPTO_NATIVE_TLS13_RANGE);
    CHECK(all_a5(untouched,sizeof(untouched)));
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,prk256,sizeof(prk256),label,sizeof(label),
        context256,sizeof(context256),untouched,8161u)
        ==CRYPTO_NATIVE_TLS13_RANGE);
    CHECK(all_a5(untouched,sizeof(untouched)));
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,prk256,sizeof(prk256),
        (const uint8_t *)"bad\0label",9u,NULL,0u,untouched,32u)
        ==CRYPTO_NATIVE_TLS13_INVALID_ARGUMENT);
    CHECK(all_a5(untouched,sizeof(untouched)));

    /* Exact allowed field-size limits and in-place output/secret aliasing. */
    memset(max_label,'a',sizeof(max_label));
    memset(max_context,0x7fu,sizeof(max_context));
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA384,prk384,sizeof(prk384),
        max_label,249u,max_context,sizeof(max_context),untouched,64u)
        ==CRYPTO_NATIVE_TLS13_OK);
    memcpy(secret_alias,prk256,sizeof(prk256));
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA256,secret_alias,32u,label,sizeof(label),
        context256,sizeof(context256),secret_alias,32u)
        ==CRYPTO_NATIVE_TLS13_OK);
    CHECK(memcmp(secret_alias,expand256,32u)==0);
    CHECK(crypto_native_tls13_hkdf_expand_label(
        CRYPTO_NATIVE_TLS13_SHA384,prk384,sizeof(prk384),
        NULL,0u,NULL,0u,NULL,0u)==CRYPTO_NATIVE_TLS13_OK);
    puts("PASS: native RFC5869/8446 SHA256/SHA384 HKDF, label/length/alias negatives");
    return 0;
}
