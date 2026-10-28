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
