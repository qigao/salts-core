/* SPDX-License-Identifier: Apache-2.0
 * Stage-1 live Profile -> ONE real Crypto TLS session acceptance.
 * Does not perform any TCP/TLS handshake: independent network/mTLS/TLCP
 * evidence is still mandatory for the production CNet switchover.
 */
#include <crypto/tls_profile.h>
#include <crypto/tls_ext.h>
#include <crypto/tls_peer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRYPTO_PROFILE_TEST_CA_FILE
#error "Require explicit real CA PEM fixture path"
#endif
#ifndef CRYPTO_PROFILE_TEST_SERVER_CERT
#error "Require explicit real server certificate fixture path"
#endif
#ifndef CRYPTO_PROFILE_TEST_SERVER_KEY
#error "Require explicit real server private key fixture path"
#endif

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "LIVE CRYPTO PROFILE FAIL %s:%d: %s\n", __FILE__,__LINE__,#x); \
    return 1; } } while(0)

static int64_t io_write(void *user,const uint8_t *bytes,size_t count)
{
    (void)user;(void)bytes;(void)count;
    return CRYPTO_TLS_IO_AGAIN;
}
static int64_t io_read(void *user,uint8_t *bytes,size_t count)
{
    (void)user;(void)bytes;(void)count;
    return CRYPTO_TLS_IO_AGAIN;
}

static crypto_tls_profile_options_v1_4 make_client(void)
{
    crypto_tls_profile_options_v1_4 p={0};
    p.struct_size=(uint32_t)sizeof(p);
    p.role=CRYPTO_TLS_CLIENT;
    p.security_profile=CRYPTO_TLS_PROFILE_STANDARD;
    p.protocol_min=CRYPTO_TLS_1_2;
    p.protocol_max=CRYPTO_TLS_1_2;
    p.trust_mode=CRYPTO_TLS_TRUST_EXPLICIT;
    p.verified_name="localhost";
    p.ca_file=CRYPTO_PROFILE_TEST_CA_FILE;
    return p;
}
static crypto_tls_profile_options_v1_4 make_server(void)
{
    crypto_tls_profile_options_v1_4 p={0};
    p.struct_size=(uint32_t)sizeof(p);
    p.role=CRYPTO_TLS_SERVER;
    p.security_profile=CRYPTO_TLS_PROFILE_STANDARD;
    p.protocol_min=CRYPTO_TLS_1_2;
    p.protocol_max=CRYPTO_TLS_1_2;
    p.trust_mode=CRYPTO_TLS_TRUST_NONE;
    p.certificate_file=CRYPTO_PROFILE_TEST_SERVER_CERT;
    p.private_key_file=CRYPTO_PROFILE_TEST_SERVER_KEY;
    return p;
}
static int check_provider_empty(crypto_tls_provider *provider)
{
    uint32_t live=UINT32_MAX;
    CHECK(crypto_tls_provider_session_count(provider,&live)==CRYPTO_TLS_OK);
    CHECK(live==0u);
    return 0;
}

static int admitted(crypto_tls_provider *provider,const crypto_tls_io *io)
{
    crypto_tls_profile_options_v1_4 options=make_client();
    crypto_tls_profile *profile=NULL;
    crypto_tls_session *session=NULL;
    static const uint8_t h2[]={'h','2'};
    static const uint8_t http[]={'h','t','t','p','/','1','.','1'};
    static const crypto_tls_alpn_label_v1_4 labels[]={
        {h2,sizeof(h2)},{http,sizeof(http)}};
    options.alpn=labels;
    options.alpn_count=sizeof(labels)/sizeof(labels[0]);
    CHECK(crypto_tls_profile_create_v1_4(&options,&profile)
          ==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_create_profile_v1_4(provider,profile,io,&session)
          ==CRYPTO_TLS_OK);
    CHECK(session!=NULL);
    CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK);
    /* Session must own a separate lifetime; profile no longer exists. */
    CHECK(crypto_tls_session_destroy(&session)==CRYPTO_TLS_OK);
    CHECK(session==NULL);
    CHECK(check_provider_empty(provider)==0);

    options=make_client();
    options.protocol_min=options.protocol_max=CRYPTO_TLS_1_3;
    CHECK(crypto_tls_profile_create_v1_4(&options,&profile)
          ==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_create_profile_v1_4(provider,profile,io,&session)
          ==CRYPTO_TLS_OK);
    CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_destroy(&session)==CRYPTO_TLS_OK);
    CHECK(check_provider_empty(provider)==0);

    options=make_server();
    CHECK(crypto_tls_profile_create_v1_4(&options,&profile)
          ==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_create_profile_v1_4(provider,profile,io,&session)
          ==CRYPTO_TLS_OK);
    CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_destroy(&session)==CRYPTO_TLS_OK);
    CHECK(check_provider_empty(provider)==0);

    options=make_server();
    options.client_auth=CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
    options.trust_mode=CRYPTO_TLS_TRUST_EXPLICIT;
    options.ca_file=CRYPTO_PROFILE_TEST_CA_FILE;
    CHECK(crypto_tls_profile_create_v1_4(&options,&profile)
          ==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_create_profile_v1_4(provider,profile,io,&session)
          ==CRYPTO_TLS_OK);
    CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_destroy(&session)==CRYPTO_TLS_OK);
    CHECK(check_provider_empty(provider)==0);
    return 0;
}

static int unsupported(crypto_tls_provider *provider,const crypto_tls_io *io)
{
    crypto_tls_profile_options_v1_4 options=make_client();
    crypto_tls_profile *profile=NULL;
    crypto_tls_session *session=NULL;
    static const uint8_t binary[]={'h',0u,'2'};
    static const crypto_tls_alpn_label_v1_4 alpn[]={ {binary,sizeof(binary)} };
#define REJECT(expr) do { \
    options=make_client(); expr; \
    CHECK(crypto_tls_profile_create_v1_4(&options,&profile)==CRYPTO_TLS_PROFILE_OK); \
    CHECK(crypto_tls_session_create_profile_v1_4(provider,profile,io,&session) \
        ==CRYPTO_TLS_PROFILE_UNSUPPORTED); \
    CHECK(session==NULL); \
    CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK); \
    CHECK(check_provider_empty(provider)==0); \
} while(0)
    REJECT(options.protocol_max=CRYPTO_TLS_1_3);
    REJECT(options.trust_mode=CRYPTO_TLS_TRUST_SYSTEM; options.ca_file=NULL);
    REJECT(options.ca_file=NULL; options.ca_path="hashed-certs");
    REJECT(options.alpn=alpn; options.alpn_count=1u);
    REJECT(options.sni_name="other.example");
    REJECT(options.verified_name="127.0.0.1");
    REJECT(options.verified_name="::1");
    REJECT(options.security_profile=CRYPTO_TLS_PROFILE_SHANGMI_TLS13;
           options.protocol_min=options.protocol_max=CRYPTO_TLS_1_3);
    REJECT(options.security_profile=CRYPTO_TLS_PROFILE_TLCP11_MODE;
           options.protocol_min=options.protocol_max=CRYPTO_TLS_PROFILE_TLCP11);
#undef REJECT


    /* The opaque profile accepts duplicate ALPN wire labels as transport
     * policy bytes, but the one incumbent TLS engine MUST refuse such an
     * admission without leaking a candidate TLS session or trying fallback.
     */
    {
        static const uint8_t h2[]={'h','2'};
        static const crypto_tls_alpn_label_v1_4 duplicates[]={
            {h2,sizeof(h2)},{h2,sizeof(h2)}};
        options=make_client();
        options.alpn=duplicates;
        options.alpn_count=2u;
        CHECK(crypto_tls_profile_create_v1_4(&options,&profile)==CRYPTO_TLS_PROFILE_OK);
        CHECK(crypto_tls_session_create_profile_v1_4(provider,profile,io,&session)
            !=CRYPTO_TLS_OK);
        CHECK(session==NULL);
        CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK);
        CHECK(check_provider_empty(provider)==0);
    }

    /* A syntactically valid path that does not exist is a real backend
     * failure; do not convert it to a positive session or system trust.
     */
    options=make_client();
    options.ca_file="__missing_crypto_profile_ca_fixture__.pem";
    CHECK(crypto_tls_profile_create_v1_4(&options,&profile)==CRYPTO_TLS_PROFILE_OK);
    CHECK(crypto_tls_session_create_profile_v1_4(provider,profile,io,&session)
        !=CRYPTO_TLS_OK);
    CHECK(session==NULL);
    CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK);
    CHECK(check_provider_empty(provider)==0);

    options=make_client();
    CHECK(crypto_tls_profile_create_v1_4(&options,&profile)==CRYPTO_TLS_PROFILE_OK);
    {
        /* Already occupied output handle: never replace or release it. */
        crypto_tls_session *sentinel=(crypto_tls_session *)(void *)&options;
        session=sentinel;
        CHECK(crypto_tls_session_create_profile_v1_4(
            provider,profile,io,&session)==CRYPTO_TLS_INVALID_ARGUMENT);
        CHECK(session==sentinel);
        session=NULL;
    }
    CHECK(crypto_tls_profile_destroy_v1_4(&profile)==CRYPTO_TLS_PROFILE_OK);
    CHECK(check_provider_empty(provider)==0);
    return 0;
}

int main(void)
{
    crypto_tls_provider *provider=NULL;
    crypto_tls_io io={0};
    io.struct_size=(uint32_t)sizeof(io);
    io.write=io_write;
    io.read=io_read;
    CHECK(crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION,&provider)
          ==CRYPTO_TLS_OK && provider!=NULL);
    CHECK(admitted(provider,&io)==0);
    CHECK(unsupported(provider,&io)==0);
    CHECK(check_provider_empty(provider)==0);
    CHECK(crypto_tls_provider_destroy(&provider)==CRYPTO_TLS_OK);
    CHECK(provider==NULL);
    puts("PASS v1.4 real single TLS session create: exact profiles, mTLS, ALPN and unsupported negatives");
    return 0;
}
