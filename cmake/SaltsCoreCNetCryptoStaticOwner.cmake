# SPDX-License-Identifier: Apache-2.0
# Test-only original real CNet Owner in ONE executable, no second static Crypto.
option(SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST
  "Qualify original CNet Owner in a sole static Salts::Crypto executable" OFF)
if(SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST)
  if(NOT BUILD_TESTING OR NOT SALTSCORE_BUILD_CNET)
    message(FATAL_ERROR "Original CNet Crypto Owner requires BUILD_TESTING and pinned CNet graph")
  endif()
  if(NOT DEFINED SALTSCORE_CRYPTO_TEST_FIXTURES)
    message(FATAL_ERROR "Exact pinned static Crypto TLS fixtures required")
  endif()
  find_package(Crypto 0.1 CONFIG REQUIRED)
  if(NOT TARGET Salts::Crypto)
    message(FATAL_ERROR "Static candidate requires sole Salts::Crypto")
  endif()
  get_target_property(_static_kind Salts::Crypto TYPE)
  get_target_property(_static_imported Salts::Crypto IMPORTED)
  if(NOT _static_imported OR NOT _static_kind STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "Candidate needs one imported STATIC Salts::Crypto")
  endif()
  foreach(_forbidden IN ITEMS
      Salts::TLS Salts::TLSCipherBIO Salts::CryptoProvider
      Crypto::TLS Crypto::TLSCipherBIO Crypto::Provider)
    if(TARGET ${_forbidden})
      message(FATAL_ERROR "Second native Crypto owner forbidden: ${_forbidden}")
    endif()
  endforeach()
  set(_static_owner_sources ${_saltscore_cnet_sources})
  list(FIND _static_owner_sources cnet/src/cnet_tls.c _old_tls_index)
  list(FIND _static_owner_sources cnet/src/cnet_owner.c _original_owner_index)
  if(_old_tls_index LESS 0 OR _original_owner_index LESS 0)
    message(FATAL_ERROR "Canonical original CNet Owner/TLS source unavailable")
  endif()
  list(REMOVE_ITEM _static_owner_sources cnet/src/cnet_tls.c)
  list(APPEND _static_owner_sources
    cnet/src/cnet_tls_crypto_qualification.c
    tests/cnet-crypto-static/cnet_crypto_owner_admission.c
    tests/cnet-crypto-static/public_context.c
    tests/cnet-crypto-static/owner.c)
  foreach(_fixture IN ITEMS
      external-io-ca-cert.pem external-io-server-cert.pem
      external-io-server-key.pem)
    if(NOT EXISTS "${SALTSCORE_CRYPTO_TEST_FIXTURES}/${_fixture}")
      message(FATAL_ERROR "Missing pinned Crypto TLS fixture: ${_fixture}")
    endif()
  endforeach()
  file(TO_CMAKE_PATH "${SALTSCORE_CRYPTO_TEST_FIXTURES}" _owner_fixtures)
  add_executable(saltscore_cnet_crypto_static_original_owner
    ${_static_owner_sources})
  target_compile_features(saltscore_cnet_crypto_static_original_owner
    PRIVATE c_std_11)
  set_target_properties(saltscore_cnet_crypto_static_original_owner PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_include_directories(saltscore_cnet_crypto_static_original_owner PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cnet/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/cnet/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/cnet/src/wsparser"
    "${CMAKE_CURRENT_SOURCE_DIR}/native-io/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/monocypher"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/reed"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cnet-crypto-static")
  target_compile_definitions(saltscore_cnet_crypto_static_original_owner PRIVATE
    "SDK_CA_FILE=\"${_owner_fixtures}/external-io-ca-cert.pem\""
    "SDK_SERVER_CERT_FILE=\"${_owner_fixtures}/external-io-server-cert.pem\""
    "SDK_SERVER_KEY_FILE=\"${_owner_fixtures}/external-io-server-key.pem\"")
  target_link_libraries(saltscore_cnet_crypto_static_original_owner PRIVATE
    Salts::Core Salts::NativeIO Salts::Concurrency
    Salts::URI Salts::Platform Salts::Crypto
    saltscore_cnet_monocypher saltscore_cnet_reed
    c-ares::cares kcp::kcp
    $<$<PLATFORM_ID:Windows>:crypt32>)
  # Original CNet resolver uses POSIX strnlen. Preserve strict C11 and
  # explicit system feature admission, never suppress implicit prototypes.
  if(APPLE)
    target_compile_definitions(saltscore_cnet_crypto_static_original_owner
      PRIVATE _DARWIN_C_SOURCE=1)
  elseif(UNIX)
    target_compile_definitions(saltscore_cnet_crypto_static_original_owner
      PRIVATE _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L)
  endif()
  # Production Salts::CNet and GmSSL::GmSSL are NOT linked here.
  if(APPLE)
    target_link_libraries(saltscore_cnet_crypto_static_original_owner PRIVATE
      "${SALTSCORE_CNET_COREFOUNDATION}"
      "${SALTSCORE_CNET_SECURITY}")
  endif()
  if(MSVC)
    target_compile_options(saltscore_cnet_crypto_static_original_owner
      PRIVATE /experimental:c11atomics)
  elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(saltscore_cnet_crypto_static_original_owner PRIVATE
      -Werror=implicit-function-declaration
      -Werror=incompatible-pointer-types -Werror=return-type)
  endif()
  add_test(NAME saltscore.cnet_crypto_static_original_owner_tls12_tls13
    COMMAND saltscore_cnet_crypto_static_original_owner)
  set_tests_properties(saltscore.cnet_crypto_static_original_owner_tls12_tls13
    PROPERTIES TIMEOUT 120)

  # True client SAN mismatch requires SNI-neutral independent server:
  # pinned GmSSL refuses unrecognized SNI *on the server* first.
  option(SALTSCORE_CNET_STATIC_CRYPTO_OPENSSL_SAN_TEST
    "Qualify static CNet client DNS SAN against independent OpenSSL peer" OFF)
  if(SALTSCORE_CNET_STATIC_CRYPTO_OPENSSL_SAN_TEST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    foreach(_protocol IN ITEMS tls12 tls13)
      add_test(NAME saltscore.cnet_crypto_static_openssl_san_${_protocol}
        COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/cnet-crypto-static/external_san_peer.py"
          --binary "$<TARGET_FILE:saltscore_cnet_crypto_static_original_owner>"
          --cert "${_owner_fixtures}/external-io-server-cert.pem"
          --key "${_owner_fixtures}/external-io-server-key.pem"
          --protocol "${_protocol}")
      set_tests_properties(saltscore.cnet_crypto_static_openssl_san_${_protocol}
        PROPERTIES TIMEOUT 75)
    endforeach()
  endif()
endif()
