# Private implementation objects. Only Salts::Crypto is archived/installed.
# The fixed source and feature profile is recorded in source-lock.json.
# Do not invoke upstream CMake, create a gmssl library, or search a provider.
if(TARGET salts_crypto_native_objects)
  message(FATAL_ERROR "Duplicate local Crypto native source owner")
endif()
add_library(salts_crypto_native_objects OBJECT
  src/version.c
  src/debug.c
  src/rsa_ext.c
  src/rsa_private_op.c
  src/aes_key_wrap.c
  src/des.c
  src/md5.c
  src/sm4.c
  src/sm4_cbc.c
  src/sm4_ctr.c
  src/sm4_gcm.c
  src/sm3.c
  src/sm3_hmac.c
  src/sm3_kdf.c
  src/sm3_pbkdf2.c
  src/sm3_digest.c
  src/sm2_z256.c
  src/sm2_z256_table.c
  src/sm2_key.c
  src/sm2_sign.c
  src/sm2_enc.c
  src/sm2_exch.c
  src/block_cipher.c
  src/digest.c
  src/hmac.c
  src/hkdf.c
  src/pbkdf2.c
  src/gf128.c
  src/ghash.c
  src/sm4_cbc_sm3_hmac.c
  src/sm4_ctr_sm3_hmac.c
  src/pkcs8.c
  src/ec.c
  src/asn1.c
  src/hex.c
  src/base64.c
  src/pem.c
  src/x509_alg.c
  src/x509_cer.c
  src/x509_ext.c
  src/x509_vrf.c
  src/x509_req.c
  src/x509_crl.c
  src/x509_new.c
  src/ocsp.c
  src/x509_key.c
  src/rsa.c
  src/socket.c
  src/file.c
  src/sm4_ecb.c
  src/sm4_ofb.c
  src/sm4_cfb.c
  src/sm4_ccm.c
  src/sm4_xts.c
  src/bn.c
  src/secp256r1.c
  src/secp256r1_key.c
  src/ecdsa.c
  src/ecdh.c
  src/secp384r1.c
  src/sha1.c
  src/sha256.c
  src/sha512.c
  src/sha256_hmac.c
  src/aes.c
  src/aes_modes.c
  src/tls.c
  src/tls_cert.c
  src/tls_alpn.c
  src/tls_ext.c
  src/tls_psk.c
  src/tls_sni.c
  src/sct.c
  src/tls_sct.c
  src/tls_ocsp.c
  src/tls_cookie.c
  src/tls_trace.c
  src/tls_vrf.c
  src/tlcp.c
  src/tls12.c
  src/tls13.c
  src/sm4_cbc_mac.c
)
include(CheckSymbolExists)
if(WIN32)
  target_sources(salts_crypto_native_objects PRIVATE src/rand_win.c src/http_win.c)
  target_compile_definitions(salts_crypto_native_objects PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
elseif(APPLE)
  target_sources(salts_crypto_native_objects PRIVATE src/rand_apple.c src/http.c)
else()
  check_symbol_exists(getentropy "unistd.h" SALTS_CRYPTO_HAVE_GETENTROPY)
  if(SALTS_CRYPTO_HAVE_GETENTROPY)
    target_sources(salts_crypto_native_objects PRIVATE src/rand_unix.c src/http.c)
  else()
    target_sources(salts_crypto_native_objects PRIVATE src/rand.c src/http.c)
  endif()
endif()
set_target_properties(salts_crypto_native_objects PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON
  POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
target_include_directories(salts_crypto_native_objects PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(salts_crypto_native_objects PRIVATE
  ENABLE_AES
  ENABLE_ASM_UNDERSCORE_PREFIX
  ENABLE_SECP256R1
  ENABLE_SECP384R1
  ENABLE_SHA1
  ENABLE_SHA2
  ENABLE_SM4_CBC_MAC
  ENABLE_SM4_CCM
  ENABLE_SM4_CFB
  ENABLE_SM4_ECB
  ENABLE_SM4_OFB
  ENABLE_SM4_XTS
  ENABLE_TLS
)
