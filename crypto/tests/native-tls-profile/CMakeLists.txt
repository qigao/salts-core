cmake_minimum_required(VERSION 3.21)
project(SaltsCryptoTLSProfileV14Independent LANGUAGES C)
include(CTest)

# Compile the EXACT production Crypto policy owner without a TLS backend.
# This is a source-level test executable, NOT another production owner.
# Never find_package(Crypto/GmSSL/OpenSSL/other TLS).
add_library(salts_crypto_tls_profile_source OBJECT
  ../../src/crypto_tls_profile.c)
target_compile_features(salts_crypto_tls_profile_source PRIVATE c_std_11)
set_target_properties(salts_crypto_tls_profile_source PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_crypto_tls_profile_source PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

add_executable(salts_crypto_tls_profile_v14_independent
  ../algorithms/tls_profile_v14_vectors.c
  $<TARGET_OBJECTS:salts_crypto_tls_profile_source>)
target_compile_features(salts_crypto_tls_profile_v14_independent PRIVATE c_std_11)
set_target_properties(salts_crypto_tls_profile_v14_independent PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_crypto_tls_profile_v14_independent PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
if(MSVC)
  target_compile_options(salts_crypto_tls_profile_source PRIVATE /W4 /WX)
  target_compile_options(salts_crypto_tls_profile_v14_independent PRIVATE /W4 /WX)
else()
  target_compile_options(salts_crypto_tls_profile_source PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(salts_crypto_tls_profile_v14_independent PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME crypto.native.tls_profile_v14.no_external_provider
  COMMAND salts_crypto_tls_profile_v14_independent)
set_tests_properties(crypto.native.tls_profile_v14.no_external_provider
  PROPERTIES TIMEOUT 120)
