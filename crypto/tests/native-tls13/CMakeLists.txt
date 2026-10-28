cmake_minimum_required(VERSION 3.21)
project(SaltsNativeTLS13KeySchedule LANGUAGES C)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# ISOLATED native candidate primitives: not a second TLS backend or fallback.
# No GmSSL/OpenSSL/libecc/Crypto package import; compile canonical SHA2 source.
add_library(salts_native_tls13_kdf OBJECT
  ../../src/crypto_sha256.c ../../src/tls13_key_schedule.c)
target_compile_features(salts_native_tls13_kdf PRIVATE c_std_11)
set_target_properties(salts_native_tls13_kdf PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_native_tls13_kdf PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
add_executable(salts_native_tls13_kdf_vectors
  tls13_key_schedule_vectors.c $<TARGET_OBJECTS:salts_native_tls13_kdf>)
target_compile_features(salts_native_tls13_kdf_vectors PRIVATE c_std_11)
set_target_properties(salts_native_tls13_kdf_vectors PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(salts_native_tls13_kdf_vectors PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
if(MSVC)
  target_compile_options(salts_native_tls13_kdf PRIVATE /W4 /WX)
  target_compile_options(salts_native_tls13_kdf_vectors PRIVATE /W4 /WX)
else()
  target_compile_options(salts_native_tls13_kdf PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(salts_native_tls13_kdf_vectors PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME crypto.native.tls13.kdf_vectors COMMAND salts_native_tls13_kdf_vectors)
set_tests_properties(crypto.native.tls13.kdf_vectors PROPERTIES TIMEOUT 120)
add_test(NAME crypto.native.tls13.python_oracle
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tls13_key_schedule_oracle.py"
    "$<TARGET_FILE:salts_native_tls13_kdf_vectors>")
set_tests_properties(crypto.native.tls13.python_oracle PROPERTIES TIMEOUT 120)
