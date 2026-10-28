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

# Same independent C11 target without any Crypto/GmSSL/OpenSSL package.
# SHA224 is emitted by the single canonical crypto_sha256.c compilation.
add_executable(salts_native_sha224_independent
  ../algorithms/sha224_native_vectors.c
  $<TARGET_OBJECTS:salts_native_sha256>)
target_compile_features(salts_native_sha224_independent PRIVATE c_std_11)
set_target_properties(salts_native_sha224_independent PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_native_sha224_independent PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
if(MSVC)
  target_compile_options(salts_native_sha224_independent PRIVATE /W4 /WX)
else()
  target_compile_options(salts_native_sha224_independent PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME crypto.native.sha224.no_gmssl COMMAND salts_native_sha224_independent)
set_tests_properties(crypto.native.sha224.no_gmssl PROPERTIES TIMEOUT 120)

# SHA384/512 share the same first-party SHA2 source object, but use FIPS 64-bit
# compression. The independent project has ZERO TLS or other provider imports.
add_executable(salts_native_sha384_512_independent
  ../algorithms/sha384_512_native_vectors.c
  $<TARGET_OBJECTS:salts_native_sha256>)
target_compile_features(salts_native_sha384_512_independent PRIVATE c_std_11)
set_target_properties(salts_native_sha384_512_independent PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_native_sha384_512_independent PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
if(MSVC)
  target_compile_options(salts_native_sha384_512_independent PRIVATE /W4 /WX)
else()
  target_compile_options(salts_native_sha384_512_independent PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME crypto.native.sha384_512.no_gmssl
  COMMAND salts_native_sha384_512_independent)
set_tests_properties(crypto.native.sha384_512.no_gmssl PROPERTIES TIMEOUT 120)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME crypto.native.sha384_512.hashlib
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../algorithms/sha384_512_hashlib_oracle.py"
    "$<TARGET_FILE:salts_native_sha384_512_independent>")
set_tests_properties(crypto.native.sha384_512.hashlib PROPERTIES TIMEOUT 120)
