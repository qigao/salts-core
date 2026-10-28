cmake_minimum_required(VERSION 3.21)
project(SaltsNativeSHA256Contract LANGUAGES C)
include(CTest)

# This independent project intentionally has NO find_package(Crypto), GmSSL,
# OpenSSL, libecc or other provider. Compile the canonical Salts::Crypto
# first-party SHA256 directly with strict C11 and real known-answer tests.
add_library(salts_native_sha256 OBJECT ../../src/crypto_sha256.c)
target_compile_features(salts_native_sha256 PRIVATE c_std_11)
set_target_properties(salts_native_sha256 PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_native_sha256 PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(salts_native_sha256_independent
  ../algorithms/sha256_native_vectors.c
  $<TARGET_OBJECTS:salts_native_sha256>)
target_compile_features(salts_native_sha256_independent PRIVATE c_std_11)
set_target_properties(salts_native_sha256_independent PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_native_sha256_independent PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
if(MSVC)
  target_compile_options(salts_native_sha256 PRIVATE /W4 /WX)
  target_compile_options(salts_native_sha256_independent PRIVATE /W4 /WX)
else()
  target_compile_options(salts_native_sha256 PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(salts_native_sha256_independent PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME crypto.native.sha256.no_gmssl
  COMMAND salts_native_sha256_independent)
set_tests_properties(crypto.native.sha256.no_gmssl PROPERTIES TIMEOUT 120)
