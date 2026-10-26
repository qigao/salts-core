/* SPDX-License-Identifier: Apache-2.0
 * Negative-first CNet public-policy admission matrix against installed
 * Crypto::TLS and SaltsCore::Core. The incumbent production CNet is not
 * linked here; no second private GmSSL instance may enter this process.
 */
#include "policy.h"

#include <salts/error_codes.h>
#include <salts_core/runtime_abi.h>
#include <crypto/tls.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr, "CNet/Crypto admission failed at line %d: %s\n", \
            __LINE__, #condition); return 1; \
} } while (0)

static int cleared(const crypto_tls_session_options *value)
{
    const crypto_tls_session_options zero = {0};
    return memcmp(value, &zero, sizeof(zero)) == 0;
}

static int client_contract(void)
{
    cnet_tls_client_config p = {0};
    crypto_tls_session_options opt = {0};
    size_t i;

    p.size = sizeof(p);
    p.ca_file = "explicit-ca.pem";
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_OK);
    REQUIRE(opt.struct_size == sizeof(opt) && opt.role == CRYPTO_TLS_CLIENT);
    REQUIRE(opt.protocol == CRYPTO_TLS_1_2 && opt.verify_depth == 4u);
    REQUIRE(opt.reserved_flags == 0u && opt.hostname != NULL
            && strcmp(opt.hostname, "example.test") == 0);
    REQUIRE(opt.ca_file == p.ca_file && opt.ca_file[0] != '\0');
    REQUIRE(opt.certificate_file == NULL && opt.private_key_file == NULL);

    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_3, &opt) == SALTS_OK);
    REQUIRE(opt.protocol == CRYPTO_TLS_1_3);

    /* Every refusal must atomically clear an earlier valid descriptor. */
    REQUIRE(cnet_crypto_candidate_prepare_client(
        NULL, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_EINVAL);
    REQUIRE(cleared(&opt));
    p.size--;
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_EINVAL);
    REQUIRE(cleared(&opt));
    p.size = sizeof(p);
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "", CRYPTO_TLS_1_2, &opt) == SALTS_EINVAL);
    REQUIRE(cleared(&opt));

    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", 0u, &opt) == SALTS_ENOTSUP);
    REQUIRE(cleared(&opt));
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", (uint32_t)0x0302u, &opt) == SALTS_ENOTSUP);

    p.ca_file = NULL; /* system trust fallback is not implemented */
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    REQUIRE(cleared(&opt));
    p.ca_file = "explicit-ca.pem";
    p.ca_path = "hashed-ca-directory";
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.ca_path = NULL;
    p.cert_file = "client.pem";
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.cert_file = NULL;
    p.key_file = "client.key";
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.key_file = NULL;
    p.key_password = "pass";
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.key_password = NULL;

    {
        const char *offers[] = {"h2"};
        p.alpn_protocols = offers;
        p.alpn_protocol_count = 1u;
        REQUIRE(cnet_crypto_candidate_prepare_client(
            &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
        p.alpn_protocol_count = 0u;
        REQUIRE(cnet_crypto_candidate_prepare_client(
            &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
        p.alpn_protocols = NULL;
    }

    p.security_profile = CNET_TLS_SECURITY_PROFILE_SHANGMI_TLS13;
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_3, &opt) == SALTS_ENOTSUP);
    p.security_profile = CNET_TLS_SECURITY_PROFILE_TLCP11;
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.security_profile = (cnet_tls_security_profile)99;
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.security_profile = CNET_TLS_SECURITY_PROFILE_STANDARD;

    p.server_name = "different.test"; /* never drop SNI/SAN override */
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.server_name = "example.test";
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_3, &opt) == SALTS_OK);
    p.server_name = NULL;

    {
        char too_long[4100];
        for (i = 0u; i < sizeof(too_long)-1u; ++i)
            too_long[i] = 'a';
        too_long[sizeof(too_long)-1u] = '\0';
        p.ca_file = too_long;
        REQUIRE(cnet_crypto_candidate_prepare_client(
            &p, "example.test", CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    }
    p.ca_file = "explicit-ca.pem"; /* avoid retaining a pointer to a stack local */
    REQUIRE(cnet_crypto_candidate_prepare_client(
        &p, "example.test", CRYPTO_TLS_1_2, NULL) == SALTS_EINVAL);
    return 0;
}

static int server_contract(void)
{
    cnet_tls_server_config p = {0};
    crypto_tls_session_options opt = {0};
    p.size = sizeof(p);
    p.cert_file = "server.pem";
    p.key_file = "server.key";
    p.client_auth = CNET_TLS_CLIENT_AUTH_NONE;

    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_OK);
    REQUIRE(opt.struct_size == sizeof(opt) && opt.role == CRYPTO_TLS_SERVER);
    REQUIRE(opt.protocol == CRYPTO_TLS_1_2 && opt.hostname == NULL);
    REQUIRE(opt.certificate_file == p.cert_file && opt.private_key_file == p.key_file);
    REQUIRE(opt.verify_depth == 4u && opt.reserved_flags == 0u);

    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_3, &opt) == SALTS_OK);
    REQUIRE(opt.protocol == CRYPTO_TLS_1_3);
    p.key_password = "nonempty";
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_OK);
    REQUIRE(opt.private_key_password == p.key_password);
    p.key_password = "";
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_OK);
    REQUIRE(opt.private_key_password == p.key_password);
    p.key_password = NULL;

    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, 0u, &opt) == SALTS_ENOTSUP);
    REQUIRE(cleared(&opt));
    p.size = 0u;
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_EINVAL);
    REQUIRE(cleared(&opt));
    p.size = sizeof(p);

    p.client_auth = CNET_TLS_CLIENT_AUTH_REQUIRED;
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.client_auth = (cnet_tls_client_auth)99;
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.client_auth = CNET_TLS_CLIENT_AUTH_NONE;

    p.ca_file = "ca.pem";
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.ca_file = NULL;
    p.ca_path = "ca-dir";
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.ca_path = NULL;

    p.security_profile = CNET_TLS_SECURITY_PROFILE_SHANGMI_TLS13;
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_3, &opt) == SALTS_ENOTSUP);
    p.security_profile = CNET_TLS_SECURITY_PROFILE_TLCP11;
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
    p.security_profile = CNET_TLS_SECURITY_PROFILE_STANDARD;

    {
        const char *alpn[] = {"h2"};
        p.alpn_protocols = alpn;
        p.alpn_protocol_count = 1u;
        REQUIRE(cnet_crypto_candidate_prepare_server(
            &p, CRYPTO_TLS_1_2, &opt) == SALTS_ENOTSUP);
        p.alpn_protocols = NULL;
        p.alpn_protocol_count = 0u;
    }

    p.key_file = NULL;
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_EINVAL);
    REQUIRE(cleared(&opt));
    p.key_file = "server.key";
    p.cert_file = NULL;
    REQUIRE(cnet_crypto_candidate_prepare_server(
        &p, CRYPTO_TLS_1_2, &opt) == SALTS_EINVAL);
    REQUIRE(cleared(&opt));
    p.cert_file = "server.pem";
    REQUIRE(cnet_crypto_candidate_prepare_server(
        NULL, CRYPTO_TLS_1_2, &opt) == SALTS_EINVAL);
    REQUIRE(cleared(&opt));
    return 0;
}

int main(void)
{
    salts_core_runtime_abi_0_1_required();
    REQUIRE(salts_core_runtime_abi_version() == SALTS_CORE_RUNTIME_ABI_VERSION);
    REQUIRE(crypto_tls_abi_version() == CRYPTO_TLS_ABI_VERSION);
    if (client_contract() != 0 || server_contract() != 0) return 1;
    puts("CNet policy -> opaque Crypto TLS: supported subset admitted; "
         "unsafe trust/protocol/ALPN/mTLS/TLCP mismatches refused");
    return 0;
}
