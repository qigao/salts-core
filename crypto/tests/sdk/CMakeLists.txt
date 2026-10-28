cmake_minimum_required(VERSION 3.21)
project(CryptoInstalledC11Consumer LANGUAGES C)
include(CTest)
find_package(Crypto 0.1 CONFIG REQUIRED)
if(NOT TARGET Salts::ASN1)
  message(FATAL_ERROR "Installed Crypto must import standalone Salts::ASN1")
endif()
get_target_property(_asn1_type Salts::ASN1 TYPE)
if(NOT _asn1_type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "Salts::ASN1 must remain STATIC")
endif()
if(NOT TARGET Salts::Crypto)
  message(FATAL_ERROR "Require single static Salts::Crypto")
endif()
get_target_property(_type Salts::Crypto TYPE)
if(NOT _type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "Crypto native owner must be static")
endif()
foreach(_split IN ITEMS Salts::TLS Salts::TLSHost Salts::TLSCipherBIO Salts::CryptoProvider Crypto::ASN1)
  if(TARGET "${_split}")
    message(FATAL_ERROR "Unexpected separate crypto target: ${_split}")
  endif()
endforeach()
if(NOT DEFINED CRYPTO_TEST_FIXTURES)
  message(FATAL_ERROR "CRYPTO_TEST_FIXTURES must point to Crypto-owned test-only certificates")
endif()
foreach(_cert IN ITEMS external-io-ca-cert.pem external-io-server-cert.pem external-io-server-key.pem)
  if(NOT EXISTS "${CRYPTO_TEST_FIXTURES}/${_cert}")
    message(FATAL_ERROR "Missing required TLS fixture ${_cert}")
  endif()
endforeach()
file(TO_CMAKE_PATH "${CRYPTO_TEST_FIXTURES}/external-io-ca-cert.pem" _ca)
file(TO_CMAKE_PATH "${CRYPTO_TEST_FIXTURES}/external-io-server-cert.pem" _cert)
file(TO_CMAKE_PATH "${CRYPTO_TEST_FIXTURES}/external-io-server-key.pem" _key)
add_executable(crypto_sdk_consumer consumer.c)
target_compile_features(crypto_sdk_consumer PRIVATE c_std_11)
target_link_libraries(crypto_sdk_consumer PRIVATE Salts::Crypto)
target_compile_definitions(crypto_sdk_consumer PRIVATE
  "SDK_CA_FILE=\"${_ca}\""
  "SDK_SERVER_CERT_FILE=\"${_cert}\""
  "SDK_SERVER_KEY_FILE=\"${_key}\"")
add_test(NAME crypto.installed.real_tls12_tls13 COMMAND crypto_sdk_consumer)
set_tests_properties(crypto.installed.real_tls12_tls13 PROPERTIES TIMEOUT 90)
option(CRYPTO_SDK_ENABLE_CONCURRENT_LIFECYCLE
  "Build real borrowed-callback race test (Unix native only)" OFF)
if(CRYPTO_SDK_ENABLE_CONCURRENT_LIFECYCLE)
  if(NOT UNIX)
    message(FATAL_ERROR "Concurrent pthread lifecycle gate needs a Unix native target")
  endif()
  find_package(Threads REQUIRED)
  add_executable(crypto_sdk_concurrency concurrency.c)
  set_target_properties(crypto_sdk_concurrency PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(crypto_sdk_concurrency
    PRIVATE Salts::Crypto Threads::Threads)
  target_compile_definitions(crypto_sdk_concurrency PRIVATE
    "SDK_CA_FILE=\"${_ca}\"")
  add_test(NAME crypto.installed.inflight_cancel_busy_destroy
    COMMAND crypto_sdk_concurrency)
  set_tests_properties(crypto.installed.inflight_cancel_busy_destroy
    PROPERTIES TIMEOUT 60)
endif()

option(CRYPTO_SDK_ENABLE_OPENSSL_PEER
  "Compile installed C11 facade TCP client for external OpenSSL 3 peers" OFF)
if(CRYPTO_SDK_ENABLE_OPENSSL_PEER)
  if(NOT UNIX)
    message(FATAL_ERROR "The independent OpenSSL socket-client gate is POSIX only")
  endif()
  add_executable(crypto_sdk_openssl_client openssl-client.c)
  set_target_properties(crypto_sdk_openssl_client PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(crypto_sdk_openssl_client PRIVATE Salts::Crypto)
  # Test-only REAL Crypto TLS socket server. OpenSSL3 s_client will obtain
  # its own exporter bytes from the SAME connection for an independent oracle.
  add_executable(crypto_sdk_openssl_server openssl-server.c)
  set_target_properties(crypto_sdk_openssl_server PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(crypto_sdk_openssl_server PRIVATE Salts::Crypto)

  # The reference peer is a *separate* test-only executable linked
  # exclusively to OpenSSL 3. NO Salts::Crypto target may gain OpenSSL links.
  option(CRYPTO_SDK_ENABLE_INDEPENDENT_EXPORTER
    "Build independent OpenSSL3 keying-material reference server" OFF)
  if(CRYPTO_SDK_ENABLE_INDEPENDENT_EXPORTER)
    find_package(OpenSSL 3 REQUIRED COMPONENTS SSL Crypto)
    if(OPENSSL_VERSION VERSION_GREATER_EQUAL "4.0.0")
      message(FATAL_ERROR "This exact exporter oracle requires OpenSSL3")
    endif()
    add_executable(crypto_sdk_openssl_exporter_oracle
      openssl-exporter-oracle.c)
    set_target_properties(crypto_sdk_openssl_exporter_oracle PROPERTIES
      C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_link_libraries(crypto_sdk_openssl_exporter_oracle PRIVATE
      OpenSSL::SSL OpenSSL::Crypto)
    add_executable(crypto_sdk_openssl_api_peer_client
      openssl-api-peer-client.c)
    set_target_properties(crypto_sdk_openssl_api_peer_client PROPERTIES
      C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    # Pure OpenSSL3 test program: no Salts::Crypto or private GmSSL target.
    target_link_libraries(crypto_sdk_openssl_api_peer_client PRIVATE
      OpenSSL::SSL OpenSSL::Crypto)
  endif()
endif()


# No DSO loader in a single-static-archive Crypto package.
option(CRYPTO_SDK_ENABLE_HOST_LEASE "Removed dynamic Crypto loader test" OFF)
option(CRYPTO_SDK_ENABLE_HOST_LEASE_RACE "Removed dynamic Crypto loader test" OFF)
if(CRYPTO_SDK_ENABLE_HOST_LEASE OR CRYPTO_SDK_ENABLE_HOST_LEASE_RACE)
  message(FATAL_ERROR "Dynamic Crypto host loader is removed; no compatibility implementation")
endif()
option(CRYPTO_SDK_ENABLE_CIPHER_BIO
  "Qualify bounded CNet-style TLS ciphertext feed/take with real TLS" OFF)
if(CRYPTO_SDK_ENABLE_CIPHER_BIO)
  if(NOT TARGET Salts::Crypto)
    message(FATAL_ERROR "Installed Salts::Crypto static owner required")
  endif()
  add_executable(crypto_sdk_cipher_bio cipher-bio.c)
  set_target_properties(crypto_sdk_cipher_bio PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(crypto_sdk_cipher_bio
    PRIVATE Salts::Crypto)
  target_compile_definitions(crypto_sdk_cipher_bio PRIVATE
    "SDK_CA_FILE=\"${_ca}\""
    "SDK_SERVER_CERT_FILE=\"${_cert}\""
    "SDK_SERVER_KEY_FILE=\"${_key}\"")
  add_test(NAME crypto.installed.cipher_bio_tls12_tls13
    COMMAND crypto_sdk_cipher_bio)
  set_tests_properties(crypto.installed.cipher_bio_tls12_tls13
    PROPERTIES TIMEOUT 120)
endif()

# Source TSan: independently cancelled borrowed IO callback, no fixture mocks.
option(CRYPTO_SDK_ENABLE_CIPHER_BIO_RACE
  "Run true C11 CipherBIO borrowed-callback cancellation race under TSan" OFF)
if(CRYPTO_SDK_ENABLE_CIPHER_BIO_RACE)
  if(NOT CRYPTO_SDK_ENABLE_CIPHER_BIO OR NOT UNIX)
    message(FATAL_ERROR "CipherBIO race requires installed host BIO and native pthreads")
  endif()
  find_package(Threads REQUIRED)
  add_executable(crypto_sdk_cipher_bio_cancel_race cipher-bio-cancel-race.c)
  set_target_properties(crypto_sdk_cipher_bio_cancel_race PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(crypto_sdk_cipher_bio_cancel_race
    PRIVATE Salts::Crypto Threads::Threads)
  add_test(NAME crypto.installed.cipher_bio_cancel_race
    COMMAND crypto_sdk_cipher_bio_cancel_race)
  set_tests_properties(crypto.installed.cipher_bio_cancel_race
    PROPERTIES TIMEOUT 90)
endif()
