# QUALIFICATION ONLY: a second *build target*, not a second loaded TLS
# provider. The original production saltscore_cnet is never modified.
# CI must never link or load this candidate beside the incumbent CNet
# GmSSL-bearing DSO in the same process.
option(SALTSCORE_CNET_BUILD_CRYPTO_TEST_DSO
  "Build mutually exclusive original CNet Owner DSO against opaque Crypto TLS"
  OFF)

if(SALTSCORE_CNET_BUILD_CRYPTO_TEST_DSO)
  find_package(CryptoTLS 0.1 CONFIG REQUIRED)
  if(NOT TARGET Crypto::TLS OR NOT TARGET Crypto::TLSCipherBIO)
    message(FATAL_ERROR "Test-only CNet requires installed single Crypto::TLS and host BIO")
  endif()
  get_target_property(_crypto_tls_kind Crypto::TLS TYPE)
  get_target_property(_crypto_bio_kind Crypto::TLSCipherBIO TYPE)
  if(NOT _crypto_tls_kind STREQUAL "SHARED_LIBRARY"
     OR NOT _crypto_bio_kind STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "Candidate must import exactly one shared Crypto owner")
  endif()

  # Reuse the *exact* original CNet Owner/application dependency list. Only
  # the private TLS implementation differs. No source copy of the Owner,
  # web socket, resolver, retained-buffer/NativeIO transport or event loop.
  set(_candidate_cnet_sources ${_saltscore_cnet_sources})
  list(REMOVE_ITEM _candidate_cnet_sources src/cnet/cnet_tls.c)
  list(APPEND _candidate_cnet_sources
    src/cnet/cnet_tls_crypto_qualification.c
    src/cnet/cnet_crypto_owner_admission.c
    tests/cnet-crypto-owner/public_context.c)
  list(FIND _candidate_cnet_sources src/cnet/cnet_tls.c _legacy_tls_index)
  if(NOT _legacy_tls_index EQUAL -1)
    message(FATAL_ERROR "Test Crypto CNet DSO must never contain legacy GmSSL TLS")
  endif()
  add_library(saltscore_cnet_crypto_qualification SHARED ${_candidate_cnet_sources})
  set_target_properties(saltscore_cnet_crypto_qualification PROPERTIES
    OUTPUT_NAME saltscore_cnet_crypto_qualification
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}"
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
  target_include_directories(saltscore_cnet_crypto_qualification
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/cnet"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/cnet/wsparser"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/native-io"
      "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/monocypher"
      "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/reed")
  target_link_libraries(saltscore_cnet_crypto_qualification
    PUBLIC SaltsCore::Core SaltsCore::NativeIO SaltsCore::Concurrency
    PRIVATE
      SaltsCore::URI SaltsCore::Platform
      saltscore_cnet_monocypher saltscore_cnet_reed
      c-ares::cares kcp::kcp
      Crypto::TLS Crypto::TLSCipherBIO
      $<$<PLATFORM_ID:Windows>:crypt32>)
  if(APPLE)
    target_link_libraries(saltscore_cnet_crypto_qualification PRIVATE
      "${SALTSCORE_CNET_COREFOUNDATION}"
      "${SALTSCORE_CNET_SECURITY}")
  elseif(UNIX)
    # Missing original CNet Owner dependency MUST fail during the test-only
    # DSO link; do not hide unresolved symbols in a Linux shared object.
    target_link_options(saltscore_cnet_crypto_qualification
      PRIVATE "LINKER:-z,defs")
  endif()
  if(MSVC)
    target_compile_options(saltscore_cnet_crypto_qualification
      PRIVATE /experimental:c11atomics)
  endif()
  # Deliberately NOT installed/exported. Only an explicitly configured
  # standalone test executable can load/link this candidate.
endif()
