/* SPDX-License-Identifier: Apache-2.0
 * Exact Crypto-owned TLS v1.4 policy construction and lifetime admission.
 * NO TLS handshake is attempted and NO TLS backend is imported here.
 */
#include <crypto/tls_profile.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(x) do { if (!(x)) { \
    fprintf(stderr, "TLS profile v1.4 FAIL line %d: %s\n", __LINE__, #x); \
    return 1; } } while (0)

static crypto_tls_profile_options_v1_4 client(void)
{
    crypto_tls_profile_options_v1_4 o = {0};
    o.struct_size = (uint32_t)sizeof(o);
    o.role = CRYPTO_TLS_CLIENT;
    o.security_profile = CRYPTO_TLS_PROFILE_STANDARD;
    o.trust_mode = CRYPTO_TLS_TRUST_SYSTEM;
    o.protocol_min = CRYPTO_TLS_1_2;
    o.protocol_max = CRYPTO_TLS_1_3;
    o.client_auth = CRYPTO_TLS_CLIENT_AUTH_NONE;
    o.verified_name = "127.0.0.1";
    o.sni_name = "verify.example";
    return o;
}

static crypto_tls_profile_options_v1_4 server(void)
{
    crypto_tls_profile_options_v1_4 o = {0};
    o.struct_size = (uint32_t)sizeof(o);
    o.role = CRYPTO_TLS_SERVER;
    o.security_profile = CRYPTO_TLS_PROFILE_STANDARD;
    o.trust_mode = CRYPTO_TLS_TRUST_NONE;
    o.protocol_min = CRYPTO_TLS_1_2;
    o.protocol_max = CRYPTO_TLS_1_3;
    o.client_auth = CRYPTO_TLS_CLIENT_AUTH_NONE;
    o.certificate_file = "server.pem";
    o.private_key_file = "server.key";
    return o;
}

static int client_owned_binary_alpn(void)
{
    uint8_t protocol1[3]={'h',0u,'2'};
    uint8_t protocol2[4]={'h','t','t','p'};
    crypto_tls_alpn_label_v1_4 labels[2] = {
        {protocol1,sizeof(protocol1)},{protocol2,sizeof(protocol2)}};
    char verified[]="127.0.0.1", sni[]="verify.example";
    crypto_tls_profile_options_v1_4 o=client();
    crypto_tls_profile *p=NULL;
    crypto_tls_profile_info_v1_4 info={0};
    uint8_t copied[32], wire[9] = {3u,'h',0u,'2',4u,'h','t','t','p'};
    char name[64];
    size_t required=0u;

    o.verified_name=verified; o.sni_name=sni;
    o.alpn=labels; o.alpn_count=2u;
    REQUIRE(crypto_tls_profile_abi_version()==CRYPTO_TLS_PROFILE_ABI_VERSION);
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(p!=NULL);
    memset(verified,'x',sizeof(verified)-1u);
    memset(sni,'y',sizeof(sni)-1u);
    memset(protocol1,0,sizeof(protocol1));
    memset(protocol2,0,sizeof(protocol2));
    info.struct_size=(uint32_t)sizeof(info);
    REQUIRE(crypto_tls_profile_get_info_v1_4(p,&info)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(info.protocol_min==CRYPTO_TLS_1_2 &&
            info.protocol_max==CRYPTO_TLS_1_3 &&
            info.role==CRYPTO_TLS_CLIENT &&
            info.trust_mode==CRYPTO_TLS_TRUST_SYSTEM &&
            info.alpn_count==2u && info.alpn_wire_bytes==9u);
    memset(copied,0xa5,sizeof(copied));
    REQUIRE(crypto_tls_profile_copy_alpn_wire_v1_4(
        p,copied,8u,&required)==CRYPTO_TLS_PROFILE_BUFFER_TOO_SMALL);
    REQUIRE(required==9u);
    for(size_t i=0u;i<sizeof(copied);++i) REQUIRE(copied[i]==0xa5u);
    REQUIRE(crypto_tls_profile_copy_alpn_wire_v1_4(
        p,copied,sizeof(copied),&required)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(required==9u && memcmp(copied,wire,9u)==0);
    memset(name,0xa5,sizeof(name));
    required=0u;
    REQUIRE(crypto_tls_profile_copy_name_v1_4(
        p,CRYPTO_TLS_PROFILE_NAME_VERIFIED,name,4u,&required)
        ==CRYPTO_TLS_PROFILE_BUFFER_TOO_SMALL);
    REQUIRE(required==10u);
    for(size_t i=0u;i<sizeof(name);++i) REQUIRE((uint8_t)name[i]==0xa5u);
    REQUIRE(crypto_tls_profile_copy_name_v1_4(
        p,CRYPTO_TLS_PROFILE_NAME_VERIFIED,name,sizeof(name),&required)
        ==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(required==10u && strcmp(name,"127.0.0.1")==0);
    REQUIRE(crypto_tls_profile_copy_name_v1_4(
        p,CRYPTO_TLS_PROFILE_NAME_SNI,name,sizeof(name),&required)
        ==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(strcmp(name,"verify.example")==0);
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(p==NULL);
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_INVALID_ARGUMENT);
    return 0;
}

static int accepted_profiles(void)
{
    crypto_tls_profile_options_v1_4 o=server();
    crypto_tls_profile *p=NULL;
    crypto_tls_profile_info_v1_4 info={0};
    size_t count=123u;
    char name[]="notchanged";
    info.struct_size=(uint32_t)sizeof(info);
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(crypto_tls_profile_copy_alpn_wire_v1_4(
        p,NULL,0u,&count)==CRYPTO_TLS_PROFILE_OK && count==0u);
    count=77u;
    REQUIRE(crypto_tls_profile_copy_name_v1_4(
        p,CRYPTO_TLS_PROFILE_NAME_SNI,name,sizeof(name),&count)
        ==CRYPTO_TLS_PROFILE_ABSENT);
    REQUIRE(count==77u && strcmp(name,"notchanged")==0);
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_OK);

    o=server();
    o.client_auth=CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
    o.trust_mode=CRYPTO_TLS_TRUST_EXPLICIT;
    o.ca_file="client-ca.pem"; o.ca_path="hashed-ca";
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(crypto_tls_profile_get_info_v1_4(p,&info)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(info.client_auth==CRYPTO_TLS_CLIENT_AUTH_REQUIRED);
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_OK);

    /* Syntax parity for genuine TLCP signing+encryption credentials.
     * The profile object alone does NOT perform KeyUsage/keypair validation. */
    o=server();
    o.security_profile=CRYPTO_TLS_PROFILE_TLCP11_MODE;
    o.protocol_min=CRYPTO_TLS_PROFILE_TLCP11;
    o.protocol_max=CRYPTO_TLS_PROFILE_TLCP11;
    o.certificate_file="sign-and-encrypt-chain.pem";
    o.private_key_file="sign-and-encrypt-keys.pem";
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_OK);

    o=client();
    o.security_profile=CRYPTO_TLS_PROFILE_SHANGMI_TLS13;
    o.protocol_min=CRYPTO_TLS_1_3;
    o.protocol_max=CRYPTO_TLS_1_3;
    o.trust_mode=CRYPTO_TLS_TRUST_EXPLICIT;
    o.ca_file="sm2-ca.pem";
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_OK);
    return 0;
}

static int rejects_bad_policy(void)
{
    crypto_tls_profile_options_v1_4 o=client();
    crypto_tls_profile *sentinel=(crypto_tls_profile *)(void *)&o;
    crypto_tls_profile *out=sentinel;
#define REJECT(expr) do { \
    o=client(); expr; out=sentinel; \
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&out) !=CRYPTO_TLS_PROFILE_OK); \
    REQUIRE(out==sentinel); \
} while(0)
    REJECT(o.struct_size=0u);
    REJECT(o.reserved_flags=1u);
    REJECT(o.reserved=1u);
    REJECT(o.role=99u);
    REJECT(o.security_profile=99u);
    REJECT(o.trust_mode=CRYPTO_TLS_TRUST_NONE);
    REJECT(o.trust_mode=CRYPTO_TLS_TRUST_EXPLICIT);
    REJECT(o.ca_file="bad-ca.pem");
    REJECT(o.protocol_min=CRYPTO_TLS_1_3; o.protocol_max=CRYPTO_TLS_1_2);
    REJECT(o.protocol_min=0x0001u);
    REJECT(o.verify_depth=6u);
    REJECT(o.client_auth=CRYPTO_TLS_CLIENT_AUTH_REQUIRED);
    REJECT(o.verified_name=NULL);
    REJECT(o.verified_name="");
    REJECT(o.sni_name="");
    REJECT(o.private_key_password="orphan");
    REJECT(o.certificate_file="client.pem");
    REJECT(o.security_profile=CRYPTO_TLS_PROFILE_SHANGMI_TLS13);
    REJECT(o.security_profile=CRYPTO_TLS_PROFILE_TLCP11_MODE);
#undef REJECT

    o=server();
    o.certificate_file=NULL;
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&out)!=CRYPTO_TLS_PROFILE_OK);
    REQUIRE(out==sentinel);
    o=server();
    o.verified_name="inappropriate-server-identity";
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&out)!=CRYPTO_TLS_PROFILE_OK);
    REQUIRE(out==sentinel);
    o=server(); o.trust_mode=CRYPTO_TLS_TRUST_SYSTEM;
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&out)!=CRYPTO_TLS_PROFILE_OK);
    REQUIRE(out==sentinel);
    o=server(); o.client_auth=CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&out)!=CRYPTO_TLS_PROFILE_OK);
    REQUIRE(out==sentinel);
    o=server();
    o.security_profile=CRYPTO_TLS_PROFILE_TLCP11_MODE;
    o.protocol_min=o.protocol_max=CRYPTO_TLS_PROFILE_TLCP11;
    o.client_auth=CRYPTO_TLS_CLIENT_AUTH_REQUIRED;
    o.trust_mode=CRYPTO_TLS_TRUST_EXPLICIT;
    o.ca_file="ca.pem";
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&out)!=CRYPTO_TLS_PROFILE_OK);
    REQUIRE(out==sentinel);
    return 0;
}

static int bounds_and_output_stability(void)
{
    crypto_tls_profile_options_v1_4 o=client();
    crypto_tls_profile *p=NULL;
    uint8_t protocol[256]={0};
    crypto_tls_alpn_label_v1_4 label={protocol,sizeof(protocol)};
    crypto_tls_profile_info_v1_4 info={0};
    char longname[CRYPTO_TLS_PROFILE_NAME_MAX_BYTES+2u];
    char longpath[CRYPTO_TLS_PROFILE_PATH_MAX_BYTES+2u];
    uint8_t output[2]={0xa5u,0xa5u};
    size_t len=17u;
    o.alpn=&label; o.alpn_count=1u;
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)
            ==CRYPTO_TLS_PROFILE_INVALID_ARGUMENT && p==NULL);
    label.size=255u;
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_OK);
    memset(longname,'a',sizeof(longname)); longname[sizeof(longname)-1u]=0;
    o.alpn=NULL; o.alpn_count=0u; o.verified_name=longname;
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)
            ==CRYPTO_TLS_PROFILE_INVALID_ARGUMENT && p==NULL);
    memset(longpath,'b',sizeof(longpath)); longpath[sizeof(longpath)-1u]=0;
    o=client(); o.trust_mode=CRYPTO_TLS_TRUST_EXPLICIT; o.ca_file=longpath;
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)
            ==CRYPTO_TLS_PROFILE_INVALID_ARGUMENT && p==NULL);

    o=client();
    REQUIRE(crypto_tls_profile_create_v1_4(&o,&p)==CRYPTO_TLS_PROFILE_OK);
    REQUIRE(crypto_tls_profile_copy_name_v1_4(
        p,99u,(char *)output,sizeof(output),&len)
        ==CRYPTO_TLS_PROFILE_INVALID_ARGUMENT);
    REQUIRE(output[0]==0xa5u && output[1]==0xa5u && len==17u);
    memset(&info,0xa5,sizeof(info));
    REQUIRE(crypto_tls_profile_get_info_v1_4(p,&info)
            ==CRYPTO_TLS_PROFILE_INVALID_ARGUMENT);
    REQUIRE(info.struct_size!=sizeof(info));
    REQUIRE(crypto_tls_profile_destroy_v1_4(&p)==CRYPTO_TLS_PROFILE_OK);
    return 0;
}

int main(void)
{
    REQUIRE(client_owned_binary_alpn()==0);
    REQUIRE(accepted_profiles()==0);
    REQUIRE(rejects_bad_policy()==0);
    REQUIRE(bounds_and_output_stability()==0);
    puts("PASS Crypto v1.4 profile: owned version/trust/identity/binary ALPN and fail-closed negatives");
    return 0;
}
