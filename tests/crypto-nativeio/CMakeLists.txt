cmake_minimum_required(VERSION 3.21)
project(SaltsCoreCNetCryptoNativeIOQualification LANGUAGES C)
include(CTest)

# Install-tree consumer only. This must NOT be built using uninstalled
# providers or through the legacy CNet/GmSSL target graph.
find_package(SaltsCore 0.1 CONFIG REQUIRED)
find_package(CryptoTLS 0.1 CONFIG REQUIRED)

foreach(_target IN ITEMS
    SaltsCore::NativeIO SaltsCore::Core
    Crypto::TLS Crypto::TLSCipherBIO)
  if(NOT TARGET ${_target})
    message(FATAL_ERROR "Missing exact installed native target: ${_target}")
  endif()
endforeach()

get_target_property(_core_type SaltsCore::Core TYPE)
get_target_property(_io_type SaltsCore::NativeIO TYPE)
get_target_property(_crypto_type Crypto::TLS TYPE)
get_target_property(_bio_type Crypto::TLSCipherBIO TYPE)
if(NOT _core_type STREQUAL "SHARED_LIBRARY" OR
   NOT _io_type STREQUAL "SHARED_LIBRARY" OR
   NOT _crypto_type STREQUAL "SHARED_LIBRARY" OR
   NOT _bio_type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "NativeIO/Crypto must preserve one shared foundation and one private GmSSL owner")
endif()

if(NOT UNIX OR WIN32)
  message(FATAL_ERROR
    "Initial real TCP NativeIO handoff uses Linux epoll/macOS kqueue. "
    "Windows IOCP is a separately required follow-on.")
endif()
if(NOT DEFINED CRYPTO_TEST_FIXTURES)
  message(FATAL_ERROR "CRYPTO_TEST_FIXTURES must be explicit")
endif()
foreach(_fixture IN ITEMS
    external-io-ca-cert.pem external-io-server-cert.pem
    external-io-server-key.pem)
  if(NOT EXISTS "${CRYPTO_TEST_FIXTURES}/${_fixture}")
    message(FATAL_ERROR "Missing immutable Crypto TLS fixture: ${_fixture}")
  endif()
endforeach()

add_executable(saltscore_crypto_nativeio_real_tcp_tls owner.c)
set_target_properties(saltscore_crypto_nativeio_real_tcp_tls PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(saltscore_crypto_nativeio_real_tcp_tls PRIVATE
  SaltsCore::NativeIO SaltsCore::Core
  Crypto::TLS Crypto::TLSCipherBIO)
target_compile_definitions(saltscore_crypto_nativeio_real_tcp_tls PRIVATE
  "SDK_CA_FILE=\"${CRYPTO_TEST_FIXTURES}/external-io-ca-cert.pem\""
  "SDK_SERVER_CERT_FILE=\"${CRYPTO_TEST_FIXTURES}/external-io-server-cert.pem\""
  "SDK_SERVER_KEY_FILE=\"${CRYPTO_TEST_FIXTURES}/external-io-server-key.pem\"")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(saltscore_crypto_nativeio_real_tcp_tls
    PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME cnet.crypto_nativeio_real_tcp_tls12_tls13
  COMMAND saltscore_crypto_nativeio_real_tcp_tls)
set_tests_properties(cnet.crypto_nativeio_real_tcp_tls12_tls13
  PROPERTIES TIMEOUT 120)
