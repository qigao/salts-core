#include <cmeta_crypto.h>

#include <stddef.h>
#include <stdint.h>

static volatile int crypto_surface_sink;

int crypto_original_surface_smoke(void) {
  uint8_t digest[64] = {0};
  uint8_t key[65] = {0};
  uint8_t signature[72] = {0};
  size_t size = 0u;
  int equal = 0;

  crypto_surface_sink += cmeta_md5(NULL, 1u, NULL);
  crypto_surface_sink += cmeta_sha1(NULL, 1u, NULL);
  crypto_surface_sink += cmeta_sha256(NULL, 1u, NULL);
  crypto_surface_sink += cmeta_sha256_stream_create(NULL);
  crypto_surface_sink += cmeta_sha256_stream_reset(NULL);
  crypto_surface_sink += cmeta_sha256_stream_update(NULL, NULL, 0u);
  crypto_surface_sink += cmeta_sha256_stream_finish(NULL, NULL);
  cmeta_sha256_stream_destroy(NULL);

  crypto_surface_sink += cmeta_sm3(NULL, 1u, NULL);
  crypto_surface_sink += cmeta_sm3_stream_create(NULL);
  crypto_surface_sink += cmeta_sm3_stream_reset(NULL);
  crypto_surface_sink += cmeta_sm3_stream_update(NULL, NULL, 0u);
  crypto_surface_sink += cmeta_sm3_stream_finish(NULL, NULL);
  cmeta_sm3_stream_destroy(NULL);

  crypto_surface_sink += cmeta_hmac_md5(NULL, 0u, NULL, 0u, NULL);
  crypto_surface_sink += cmeta_hmac_sha1(NULL, 0u, NULL, 0u, NULL);
  crypto_surface_sink += cmeta_hmac_sha256(NULL, 0u, NULL, 0u, NULL);
  crypto_surface_sink += cmeta_hmac_sm3(NULL, 0u, NULL, 0u, NULL);
  crypto_surface_sink += cmeta_hkdf_sm3(NULL, 0u, NULL, 0u, NULL, 0u,
                                        NULL, 1u);

  crypto_surface_sink += cmeta_des_cbc_encrypt(NULL, NULL, NULL, 1u, NULL);
  crypto_surface_sink += cmeta_des_cbc_decrypt(NULL, NULL, NULL, 1u, NULL);
  crypto_surface_sink += cmeta_aes128_cfb_encrypt(NULL, NULL, NULL, 1u, NULL);
  crypto_surface_sink += cmeta_aes128_cfb_decrypt(NULL, NULL, NULL, 1u, NULL);

  crypto_surface_sink += cmeta_ecdsa_p256_public_key(NULL, NULL);
  crypto_surface_sink += cmeta_ecdsa_p256_sign_sha256(
      NULL, NULL, 0u, NULL, 0u, NULL);
  crypto_surface_sink += cmeta_ecdsa_p256_verify_sha256(
      NULL, NULL, 0u, NULL, 0u);

  crypto_surface_sink += cmeta_ed25519_public_key(NULL, NULL);
  crypto_surface_sink += cmeta_ed25519_sign(NULL, NULL, 0u, NULL);
  crypto_surface_sink += cmeta_ed25519_verify(NULL, NULL, 0u, NULL);

  crypto_surface_sink += cmeta_x25519_public_key(NULL, NULL);
  crypto_surface_sink += cmeta_x25519_derive(NULL, NULL, NULL);

  crypto_surface_sink += cmeta_sm2_sign(
      NULL, 0u, NULL, 0u, NULL, 0u, signature, sizeof(signature), &size);
  crypto_surface_sink += cmeta_sm2_verify(
      NULL, 0u, NULL, 0u, NULL, 0u, NULL, 0u);

  crypto_surface_sink += cmeta_sm4_gcm_seal(
      NULL, NULL, NULL, 0u, NULL, 0u, NULL, 0u, &size, NULL);
  crypto_surface_sink += cmeta_sm4_gcm_open(
      NULL, NULL, NULL, 0u, NULL, 0u, NULL, NULL, 0u, &size);

  crypto_surface_sink += cmeta_crypto_equal(NULL, NULL, 0u, &equal);
  cmeta_crypto_clear(NULL, 0u);

  /* Prevent the compiler from proving the calls irrelevant; the exact value is
   * not a behavior oracle here. The vector consumer owns semantic assertions. */
  digest[0] ^= key[0];
  return crypto_surface_sink == 0x7fffffff ? (int)digest[0] : 0;
}
