add_executable(salts_crypto_sha_ed448_vectors sha_ed448_vectors.c)
target_compile_features(salts_crypto_sha_ed448_vectors PRIVATE c_std_11)
target_link_libraries(salts_crypto_sha_ed448_vectors PRIVATE Salts::Crypto)
# Source-tree tests use the verified pinned GmSSL header; installed tests
# resolve their own header from the Crypto SDK instead.
target_include_directories(salts_crypto_sha_ed448_vectors PRIVATE
  "${_gmssl_prefix}/include")
add_test(NAME crypto.algorithms.sha256_sm3_ed448_rfc8032 COMMAND salts_crypto_sha_ed448_vectors)
set_tests_properties(crypto.algorithms.sha256_sm3_ed448_rfc8032 PROPERTIES TIMEOUT 120)

add_executable(crypto_algorithms_cpp_header header_cpp.cpp)
set_target_properties(crypto_algorithms_cpp_header PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(crypto_algorithms_cpp_header PRIVATE Salts::Crypto)
add_test(NAME crypto.algorithms.cpp17_header COMMAND crypto_algorithms_cpp_header)

# First-party C11 SHA256 is compiled into the SAME canonical Salts::Crypto
# archive. Check NIST long/padding/streaming vectors without an SSL provider.
add_executable(salts_crypto_native_sha256_kat sha256_native_vectors.c)
target_compile_features(salts_crypto_native_sha256_kat PRIVATE c_std_11)
target_link_libraries(salts_crypto_native_sha256_kat PRIVATE Salts::Crypto)
add_test(NAME crypto.algorithms.native_sha256_fips
         COMMAND salts_crypto_native_sha256_kat)
set_tests_properties(crypto.algorithms.native_sha256_fips
  PROPERTIES TIMEOUT 120)

# SHA224 uses the ONE canonical native SHA256 compression compiled within
# Salts::Crypto; reject absence of FIPS boundary vectors.
add_executable(salts_crypto_native_sha224_kat sha224_native_vectors.c)
target_compile_features(salts_crypto_native_sha224_kat PRIVATE c_std_11)
target_link_libraries(salts_crypto_native_sha224_kat PRIVATE Salts::Crypto)
add_test(NAME crypto.algorithms.native_sha224_fips COMMAND salts_crypto_native_sha224_kat)
set_tests_properties(crypto.algorithms.native_sha224_fips PROPERTIES TIMEOUT 120)

# One installed Crypto archive owns SHA384/512: native FIPS tests run linked
# against the ACTUAL production Salts::Crypto target, not an external program.
add_executable(salts_crypto_native_sha384_512_kat sha384_512_native_vectors.c)
target_compile_features(salts_crypto_native_sha384_512_kat PRIVATE c_std_11)
target_link_libraries(salts_crypto_native_sha384_512_kat PRIVATE Salts::Crypto)
add_test(NAME crypto.algorithms.native_sha384_512_fips
         COMMAND salts_crypto_native_sha384_512_kat)
set_tests_properties(crypto.algorithms.native_sha384_512_fips
  PROPERTIES TIMEOUT 120)

# New versioned full CNet policy owner is compiled into the SINGLE actual
# Salts::Crypto archive (not a test-only proxy or alternate TLS Provider).
add_executable(salts_crypto_tls_profile_v14 tls_profile_v14_vectors.c)
target_compile_features(salts_crypto_tls_profile_v14 PRIVATE c_std_11)
set_target_properties(salts_crypto_tls_profile_v14 PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(salts_crypto_tls_profile_v14 PRIVATE Salts::Crypto)
if(MSVC)
  target_compile_options(salts_crypto_tls_profile_v14 PRIVATE /W4 /WX)
else()
  target_compile_options(salts_crypto_tls_profile_v14 PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME crypto.algorithms.native_tls_profile_v14
  COMMAND salts_crypto_tls_profile_v14)
set_tests_properties(crypto.algorithms.native_tls_profile_v14
  PROPERTIES TIMEOUT 120)

add_executable(salts_crypto_tls_profile_v14_cpp tls_profile_v14_cpp.cpp)
set_target_properties(salts_crypto_tls_profile_v14_cpp PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(salts_crypto_tls_profile_v14_cpp PRIVATE Salts::Crypto)
add_test(NAME crypto.algorithms.native_tls_profile_v14_cpp17
  COMMAND salts_crypto_tls_profile_v14_cpp)
set_tests_properties(crypto.algorithms.native_tls_profile_v14_cpp17
  PROPERTIES TIMEOUT 120)

# Real GmSSL-backed session created by the versioned Crypto-owned profile,
# without a second TLS engine or test-only CNet adapter. Not handshake parity.
add_executable(salts_crypto_tls_profile_v14_session
  tls_profile_v14_session.c)
target_compile_features(salts_crypto_tls_profile_v14_session PRIVATE c_std_11)
set_target_properties(salts_crypto_tls_profile_v14_session PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_definitions(salts_crypto_tls_profile_v14_session PRIVATE
  "CRYPTO_PROFILE_TEST_CA_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/external-io-ca-cert.pem\""
  "CRYPTO_PROFILE_TEST_SERVER_CERT=\"${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/external-io-server-cert.pem\""
  "CRYPTO_PROFILE_TEST_SERVER_KEY=\"${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/external-io-server-key.pem\"")
target_link_libraries(salts_crypto_tls_profile_v14_session PRIVATE Salts::Crypto)
if(MSVC)
  target_compile_options(salts_crypto_tls_profile_v14_session PRIVATE /W4 /WX)
else()
  target_compile_options(salts_crypto_tls_profile_v14_session PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME crypto.algorithms.native_tls_profile_v14_real_session
  COMMAND salts_crypto_tls_profile_v14_session)
set_tests_properties(crypto.algorithms.native_tls_profile_v14_real_session
  PROPERTIES TIMEOUT 120)
