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

if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
  message(FATAL_ERROR "NativeIO real TCP qualification requires IOCP/epoll/kqueue")
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
if(WIN32)
  target_link_libraries(saltscore_crypto_nativeio_real_tcp_tls PRIVATE Ws2_32)
  target_compile_definitions(saltscore_crypto_nativeio_real_tcp_tls PRIVATE
    _WIN32_WINNT=0x0600 WIN32_LEAN_AND_MEAN)
endif()
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

if(NOT WIN32)
# This cannot be simulated by deliberately small Crypto transport chunks:
# require a genuine OS-generated partial TCP_SEND terminal completion on a
# real loopback socket with constrained SO_SNDBUF, then resubmit its prefix.
add_executable(saltscore_crypto_nativeio_partial_send partial-send.c)
set_target_properties(saltscore_crypto_nativeio_partial_send PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(saltscore_crypto_nativeio_partial_send PRIVATE
  SaltsCore::NativeIO SaltsCore::Core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(saltscore_crypto_nativeio_partial_send PRIVATE
    -Wall -Wextra -Werror)
endif()
add_test(NAME cnet.crypto_nativeio_real_partial_tcp_send
  COMMAND saltscore_crypto_nativeio_partial_send)
set_tests_properties(cnet.crypto_nativeio_real_partial_tcp_send
  PROPERTIES TIMEOUT 120)

# Only native_io_backend_wake() is used by the other thread. The NativeIO
# Owner alone cancels, observes terminal completion, closes and destroys.
# Positive retained socket I/O is NOT a mock or a fabricated completion.
find_package(Threads REQUIRED)
add_executable(saltscore_crypto_nativeio_cancel_wake cancel.c)
set_target_properties(saltscore_crypto_nativeio_cancel_wake PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(saltscore_crypto_nativeio_cancel_wake PRIVATE
  SaltsCore::NativeIO Threads::Threads)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(saltscore_crypto_nativeio_cancel_wake
    PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME cnet.crypto_nativeio_cancel_wake_terminal
  COMMAND saltscore_crypto_nativeio_cancel_wake)
set_tests_properties(cnet.crypto_nativeio_cancel_wake_terminal
  PROPERTIES TIMEOUT 120)
endif() # existing POSIX OS-short-send and pthread-wake tests

if(WIN32)
  enable_language(CXX)
  add_executable(saltscore_crypto_nativeio_installed_cpp17 installed-cpp17.cpp)
  set_target_properties(saltscore_crypto_nativeio_installed_cpp17 PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  target_link_libraries(saltscore_crypto_nativeio_installed_cpp17 PRIVATE
    SaltsCore::NativeIO SaltsCore::Core Crypto::TLS Crypto::TLSCipherBIO)
  add_test(NAME cnet.crypto_nativeio_windows_cpp17_sdk
    COMMAND saltscore_crypto_nativeio_installed_cpp17)
  set_tests_properties(cnet.crypto_nativeio_windows_cpp17_sdk PROPERTIES TIMEOUT 60)
endif()
