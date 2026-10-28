/* SPDX-License-Identifier: Apache-2.0
 * RFC 5929 tls-server-end-point: real RSA/SHA256 and ECDSA/SHA384 DER,
 * independent Python hashlib expected values of complete certificates.
 * This fixture parser is TEST-ONLY; no TLS trust/authentication is claimed.
 */
#include "cnet_tls.h"
#include <crypto/sha256.h>
#include <crypto/sha224.h>
#include <salts/error_codes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef SDK_RSA_SHA256_PEM
#error SDK_RSA_SHA256_PEM required from exact Crypto test fixtures
#endif
#ifndef SDK_ECDSA_SHA384_PEM
#error SDK_ECDSA_SHA384_PEM required from exact Crypto test fixtures
#endif
#define REQUIRE(x) do { if (!(x)) { \
  fprintf(stderr, "RFC5929 regression line %d: %s\n", __LINE__, #x); \
  return 1; } } while (0)

static int b64(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return (int)(c - 'a') + 26;
  if (c >= '0' && c <= '9') return (int)(c - '0') + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}
static int read_der(const char *file, uint8_t *der, size_t capacity,
                    size_t *out_len) {
  FILE *in = fopen(file, "rb");
  char pem[8192], *p, *end;
  size_t n, written = 0u;
  uint32_t acc = 0u;
  unsigned bits = 0u;
  if (!in) return 0;
  n = fread(pem, 1u, sizeof(pem) - 1u, in);
  if (ferror(in) || !feof(in)) { fclose(in); return 0; }
  fclose(in);
  pem[n] = '\0';
  p = strstr(pem, "-----BEGIN CERTIFICATE-----");
  end = strstr(pem, "-----END CERTIFICATE-----");
  if (!p || !end || end <= p) return 0;
  p += strlen("-----BEGIN CERTIFICATE-----");
  for (; p < end; ++p) {
    int value;
    unsigned char c = (unsigned char)*p;
    if (c == '\r' || c == '\n' || c == '\t' || c == ' ') continue;
    if (c == '=') break;
    value = b64(c);
    if (value < 0) return 0;
    acc = (acc << 6) | (uint32_t)value;
    bits += 6u;
    if (bits >= 8u) {
      bits -= 8u;
      if (written == capacity) return 0;
      der[written++] = (uint8_t)(acc >> bits);
      acc &= (uint32_t)((1u << bits) - 1u);
    }
  }
  if (!written || bits > 4u) return 0;
  *out_len = written;
  return 1;
}
/* Modify BOTH inner and outer RSA SignatureAlgorithmIdentifier OIDs so
 * first-party DER inspection still accepts their equality. These are
 * deliberately unsigned synthetic policy inputs, never TLS peers. */
static int rewrite_rsa_oid(uint8_t *der, size_t length, uint8_t last) {
  static const uint8_t rsa_sha256[9] = {
    0x2au,0x86u,0x48u,0x86u,0xf7u,0x0du,0x01u,0x01u,0x0bu
  };
  unsigned matched = 0u;
  for (size_t i = 0u; i + sizeof(rsa_sha256) <= length; ++i) {
    if (memcmp(der + i, rsa_sha256, sizeof(rsa_sha256)) == 0) {
      der[i + sizeof(rsa_sha256) - 1u] = last;
      ++matched;
    }
  }
  return matched == 2u;
}

int cnet_crypto_rfc5929_regression(void) {
  static const uint8_t rsa_expected[32] = {0x84u, 0x12u, 0xcbu, 0xa2u, 0xb9u, 0xacu, 0x94u, 0xa9u, 0x36u, 0x2fu, 0xb6u, 0xb5u, 0x8du, 0x77u, 0x4cu, 0xd8u, 0x72u, 0x7bu, 0x8du, 0x67u, 0x0bu, 0xb9u, 0xceu, 0xc2u, 0x1eu, 0xa8u, 0x4fu, 0xf5u, 0x9bu, 0xb7u, 0x95u, 0x1au};
  static const uint8_t ec_expected[48] = {0x98u, 0xb3u, 0x9cu, 0xfau, 0x9du, 0xe0u, 0x6du, 0xb6u, 0xcbu, 0x63u, 0x42u, 0x41u, 0x81u, 0x03u, 0x08u, 0xd5u, 0xe8u, 0x37u, 0xe9u, 0x66u, 0x40u, 0xefu, 0x40u, 0xc1u, 0x36u, 0x19u, 0x93u, 0x2du, 0x61u, 0x51u, 0x4fu, 0xd7u, 0x97u, 0x1bu, 0x74u, 0x13u, 0x47u, 0xf9u, 0xa6u, 0x1cu, 0xd8u, 0x2fu, 0x32u, 0x27u, 0x3cu, 0xfau, 0xbau, 0x52u};
  uint8_t rsa[4096], ec[4096], modified[4096], out[64], fallback[32];
  size_t rsa_size = 0u, ec_size = 0u, length = 0u;
  REQUIRE(read_der(SDK_RSA_SHA256_PEM, rsa, sizeof(rsa), &rsa_size));
  REQUIRE(read_der(SDK_ECDSA_SHA384_PEM, ec, sizeof(ec), &ec_size));
  REQUIRE(rsa_size == 856u && ec_size == 390u);
  memset(out, 0xa5, sizeof(out));
  REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
      rsa, rsa_size, out, sizeof(out), &length) == SALTS_OK);
  REQUIRE(length == sizeof(rsa_expected));
  REQUIRE(memcmp(out, rsa_expected, length) == 0);
  memset(out, 0xa5, sizeof(out));
  REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
      ec, ec_size, out, sizeof(out), &length) == SALTS_OK);
  REQUIRE(length == sizeof(ec_expected));
  REQUIRE(memcmp(out, ec_expected, length) == 0);

  memset(out, 0xa5, sizeof(out));
  length = 77u;
  REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
      ec, ec_size, out, 16u, &length) == SALTS_EMSGSIZE);
  REQUIRE(length == sizeof(ec_expected));
  for (size_t i = 0u; i < 16u; ++i) REQUIRE(out[i] == 0u);
  REQUIRE(out[16] == 0xa5u);

  memcpy(modified, rsa, rsa_size);
  modified[0] = 0x31u;
  memset(out, 0xa5, sizeof(out)); length = 77u;
  REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
      modified, rsa_size, out, sizeof(out), &length) == SALTS_EPROTO);
  REQUIRE(length == 0u && out[0] == 0u);

  memcpy(modified, rsa, rsa_size);
  REQUIRE(rewrite_rsa_oid(modified, rsa_size, 0x7fu));
  memset(out, 0xa5, sizeof(out)); length = 77u;
  REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
      modified, rsa_size, out, sizeof(out), &length) == SALTS_ENOTSUP);
  REQUIRE(length == 0u && out[0] == 0u);

  /* Two offline RSA SHA224 AlgorithmIdentifier OIDs are synthetically
   * rewritten from the real 856-byte RSA cert. This altered signature
   * is intentionally NOT authenticated and must never be a TLS peer.
   * Python hashlib.sha224 on the full modified DER is the independent oracle.
   */
  {
    static const uint8_t expected_sha224[28] = {
      0xd4u,0x94u,0x45u,0xc0u,0xc5u,0x7du,0xaeu,0x85u,
      0x16u,0xc9u,0x4eu,0xe5u,0xf8u,0x32u,0x7au,0xc2u,
      0x02u,0x52u,0xf8u,0xbeu,0xb0u,0x19u,0xbcu,0x54u,
      0x2bu,0x24u,0x36u,0xc8u
    };
    memcpy(modified,rsa,rsa_size);
    REQUIRE(rewrite_rsa_oid(modified,rsa_size,0x0eu));
    memset(out,0xa5,sizeof(out)); length=0u;
    REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
        modified,rsa_size,out,sizeof(out),&length)==SALTS_OK);
    REQUIRE(length==sizeof(expected_sha224));
    REQUIRE(memcmp(out,expected_sha224,sizeof(expected_sha224))==0);
    memset(out,0xa5,sizeof(out)); length=0u;
    REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
        modified,rsa_size,out,8u,&length)==SALTS_EMSGSIZE);
    REQUIRE(length==sizeof(expected_sha224));
    for(size_t i=0u;i<8u;++i) REQUIRE(out[i]==0u);
    REQUIRE(out[8]==0xa5u);
  }

  /* RFC5929 section 4.1 requires SHA256 for SHA1-signed certificates. */
  memcpy(modified, rsa, rsa_size);
  REQUIRE(rewrite_rsa_oid(modified, rsa_size, 0x05u));
  REQUIRE(salts_crypto_sha256(modified, rsa_size, fallback) == SALTS_CRYPTO_OK);
  memset(out, 0xa5, sizeof(out));
  REQUIRE(cnet_tls_server_end_point_binding_from_certificate(
      modified, rsa_size, out, sizeof(out), &length) == SALTS_OK);
  REQUIRE(length == sizeof(fallback) && memcmp(out, fallback, length) == 0);
  return 0;
}
