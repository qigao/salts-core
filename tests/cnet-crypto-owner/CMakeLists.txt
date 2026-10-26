cmake_minimum_required(VERSION 3.21)
project(CNetCryptoOwnerPolicyQualification LANGUAGES C)
include(CTest)

# Only use independently installed Crypto and SaltsCore binaries. The
# production CNet DSO is deliberately disabled; borrow its public *types*
# from this exact source HEAD to test migration policy, not production TLS.
find_package(SaltsCore 0.1 CONFIG REQUIRED)
find_package(CryptoTLS 0.1 CONFIG REQUIRED)
foreach(_required IN ITEMS SaltsCore::Core Crypto::TLS Crypto::TLSCipherBIO)
  if(NOT TARGET ${_required})
    message(FATAL_ERROR "Missing installed C11 owner: ${_required}")
  endif()
endforeach()
if(TARGET SaltsCore::CNet)
  message(FATAL_ERROR "Test-only Crypto policy must not link production CNet/GmSSL")
endif()

get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_cnet_private_header "${CMAKE_CURRENT_BINARY_DIR}/cnet-private/cnet")
file(MAKE_DIRECTORY "${_cnet_private_header}")
configure_file("${_root}/include/cnet/cnet.h"
               "${_cnet_private_header}/cnet.h" COPYONLY)

add_executable(cnet_crypto_owner_admission
  admission.c
  "${_root}/src/cnet/cnet_crypto_owner_admission.c")
target_include_directories(cnet_crypto_owner_admission PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/cnet-private"
  "${_root}/src/cnet")
target_compile_features(cnet_crypto_owner_admission PRIVATE c_std_11)
set_target_properties(cnet_crypto_owner_admission PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(cnet_crypto_owner_admission PRIVATE
  SaltsCore::Core Crypto::TLS Crypto::TLSCipherBIO)
if(MSVC)
  target_compile_options(cnet_crypto_owner_admission PRIVATE
    /experimental:c11atomics)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cnet_crypto_owner_admission PRIVATE
    -Wall -Wextra -Werror)
endif()
add_test(NAME cnet.crypto_owner_policy_admission
  COMMAND cnet_crypto_owner_admission)
set_tests_properties(cnet.crypto_owner_policy_admission
  PROPERTIES TIMEOUT 60)

# Test the ORIGINAL cnet_tls_state function signatures using only the
# installed opaque Crypto DSO. The old GmSSL-bearing cnet_tls.c is absent.
# This is not yet an original cnet_owner.c / full CNet DSO test.
if(NOT DEFINED CRYPTO_TEST_FIXTURES)
  message(FATAL_ERROR "Exact Crypto test fixture directory is required")
endif()
foreach(_fixture IN ITEMS external-io-ca-cert.pem
                          external-io-server-cert.pem
                          external-io-server-key.pem)
  if(NOT EXISTS "${CRYPTO_TEST_FIXTURES}/${_fixture}")
    message(FATAL_ERROR "Missing pinned TLS fixture: ${_fixture}")
  endif()
endforeach()
file(TO_CMAKE_PATH "${CRYPTO_TEST_FIXTURES}" _crypto_fixture_path)
add_executable(cnet_crypto_tls_state_qualification
  state.c
  "${_root}/src/cnet/cnet_crypto_owner_admission.c"
  "${_root}/src/cnet/cnet_tls_crypto_qualification.c")
target_include_directories(cnet_crypto_tls_state_qualification PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/cnet-private"
  "${_root}/src/cnet")
set_target_properties(cnet_crypto_tls_state_qualification PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(cnet_crypto_tls_state_qualification PRIVATE
  SaltsCore::Core Crypto::TLS Crypto::TLSCipherBIO)
target_compile_definitions(cnet_crypto_tls_state_qualification PRIVATE
  "SDK_CA_FILE=\"${_crypto_fixture_path}/external-io-ca-cert.pem\""
  "SDK_SERVER_CERT_FILE=\"${_crypto_fixture_path}/external-io-server-cert.pem\""
  "SDK_SERVER_KEY_FILE=\"${_crypto_fixture_path}/external-io-server-key.pem\"")
if(MSVC)
  target_compile_options(cnet_crypto_tls_state_qualification PRIVATE
    /experimental:c11atomics)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cnet_crypto_tls_state_qualification PRIVATE
    -Wall -Wextra -Werror)
endif()
add_test(NAME cnet.crypto_tls_state_real_tls12_tls13
  COMMAND cnet_crypto_tls_state_qualification)
set_tests_properties(cnet.crypto_tls_state_real_tls12_tls13
  PROPERTIES TIMEOUT 120)
