cmake_minimum_required(VERSION 3.21)

# An additive qualification in a completely isolated CNet-free build.
# The real CNet source still depends on pinned GmSSL until TLS/TLCP parity.
find_package(Crypto 0.1 CONFIG REQUIRED)
if(NOT TARGET Salts::Crypto)
  message(FATAL_ERROR "Missing sole Salts::Crypto imported static archive")
endif()
get_target_property(_crypto_kind Salts::Crypto TYPE)
get_target_property(_crypto_imported Salts::Crypto IMPORTED)
if(NOT _crypto_imported OR NOT _crypto_kind STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "Crypto CNet migration requires one installed STATIC Salts::Crypto")
endif()
foreach(_forbidden IN ITEMS
    Salts::TLS Salts::TLSCipherBIO Salts::CryptoProvider
    Crypto::TLS Crypto::TLSCipherBIO Crypto::Provider GmSSL::GmSSL)
  if(TARGET ${_forbidden})
    message(FATAL_ERROR "Static CNet candidate cannot import ${_forbidden}")
  endif()
endforeach()
get_target_property(_crypto_links Salts::Crypto INTERFACE_LINK_LIBRARIES)
if(_crypto_links MATCHES "GmSSL::|Crypto::TLS|Salts::TLS")
  message(FATAL_ERROR "Salts::Crypto SDK still exposes a split GmSSL/TLS owner")
endif()
if(NOT TARGET Salts::Core)
  message(FATAL_ERROR "CNet static admission must use canonical Salts::Core")
endif()

add_executable(saltscore_cnet_crypto_static_admission
  admission.c cnet_crypto_owner_admission.c)
set_target_properties(saltscore_cnet_crypto_static_admission PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(saltscore_cnet_crypto_static_admission PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/src/cnet")
target_link_libraries(saltscore_cnet_crypto_static_admission
  PRIVATE Salts::Core Salts::Crypto)
if(MSVC)
  target_compile_options(saltscore_cnet_crypto_static_admission
    PRIVATE /experimental:c11atomics)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(saltscore_cnet_crypto_static_admission
    PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME saltscore.cnet_crypto_static_policy
  COMMAND saltscore_cnet_crypto_static_admission)
set_tests_properties(saltscore.cnet_crypto_static_policy PROPERTIES TIMEOUT 60)

# Compilation and execution by a true C++17 consumer, same canonical target.
enable_language(CXX)
add_executable(saltscore_cnet_crypto_static_cpp17 consumer.cpp)
target_compile_features(saltscore_cnet_crypto_static_cpp17 PRIVATE cxx_std_17)
target_include_directories(saltscore_cnet_crypto_static_cpp17 PRIVATE
  "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(saltscore_cnet_crypto_static_cpp17
  PRIVATE Salts::Core Salts::Crypto)
add_test(NAME saltscore.cnet_crypto_static_cpp17
  COMMAND saltscore_cnet_crypto_static_cpp17)
set_tests_properties(saltscore.cnet_crypto_static_cpp17 PROPERTIES TIMEOUT 60)

# M1 uses the same opaque CNet private TLS ABI, backed exclusively by the
# installed static Salts::Crypto archive. It DOES exercise real TLS 1.2/1.3
# handshakes/identity/record retention/exporter/close_notify, but deliberately
# does not load or replace the original NativeIO Owner / CNet production DSO.
option(SALTSCORE_CNET_STATIC_CRYPTO_TLS_STATE_TESTS
  "Run real TLS1.2/1.3 CNet state over sole static Salts::Crypto" OFF)
if(SALTSCORE_CNET_STATIC_CRYPTO_TLS_STATE_TESTS)
  if(NOT SALTSCORE_CRYPTO_TEST_FIXTURES)
    message(FATAL_ERROR "Exact installed-source Crypto TLS cert fixture directory is required")
  endif()
  foreach(_fixture IN ITEMS
      external-io-ca-cert.pem external-io-server-cert.pem
      external-io-server-key.pem)
    if(NOT EXISTS "${SALTSCORE_CRYPTO_TEST_FIXTURES}/${_fixture}")
      message(FATAL_ERROR "Missing exact static Crypto TLS fixture: ${_fixture}")
    endif()
  endforeach()
  file(TO_CMAKE_PATH "${SALTSCORE_CRYPTO_TEST_FIXTURES}" _crypto_fixtures)
  add_executable(saltscore_cnet_crypto_static_state
    state.c cnet_crypto_owner_admission.c
    "${PROJECT_SOURCE_DIR}/src/cnet/cnet_tls_crypto_qualification.c")
  set_target_properties(saltscore_cnet_crypto_static_state PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_include_directories(saltscore_cnet_crypto_static_state PRIVATE
    "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src/cnet"
    "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(saltscore_cnet_crypto_static_state
    PRIVATE Salts::Core Salts::Crypto)
  target_compile_definitions(saltscore_cnet_crypto_static_state PRIVATE
    "SDK_CA_FILE=\"${_crypto_fixtures}/external-io-ca-cert.pem\""
    "SDK_SERVER_CERT_FILE=\"${_crypto_fixtures}/external-io-server-cert.pem\""
    "SDK_SERVER_KEY_FILE=\"${_crypto_fixtures}/external-io-server-key.pem\""
    "SDK_WRONG_CA_FILE=\"${PROJECT_SOURCE_DIR}/tests/fixtures/p384-test-wrong-root-cert.pem\"")
  if(MSVC)
    target_compile_options(saltscore_cnet_crypto_static_state
      PRIVATE /experimental:c11atomics)
  elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(saltscore_cnet_crypto_static_state
      PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME saltscore.cnet_crypto_static_state_tls12_tls13
    COMMAND saltscore_cnet_crypto_static_state)
  set_tests_properties(saltscore.cnet_crypto_static_state_tls12_tls13
    PROPERTIES TIMEOUT 120)
endif()
