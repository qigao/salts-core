cmake_minimum_required(VERSION 3.21)
project(CryptoX509StandaloneConformance VERSION 0.1.0 LANGUAGES C)
include(CTest)
if(NOT DEFINED CRYPTO_X509_SOURCE_ROOT OR
   NOT IS_DIRECTORY "${CRYPTO_X509_SOURCE_ROOT}")
  message(FATAL_ERROR "Exact qigao/crypto/crypto source root is required")
endif()
if(NOT EXISTS "${CRYPTO_X509_SOURCE_ROOT}/src/crypto_x509_der.c" OR
   NOT EXISTS "${CRYPTO_X509_SOURCE_ROOT}/tests/x509/test_x509_der.c")
  message(FATAL_ERROR "Canonical Crypto X.509 source and tests are missing")
endif()
find_package(ASN1 0.1 CONFIG REQUIRED)
foreach(_forbidden IN ITEMS GmSSL::GmSSL Salts::Core Salts::Crypto)
  if(TARGET "${_forbidden}")
    message(FATAL_ERROR "Provider or full Crypto runtime leaked into X.509 test: ${_forbidden}")
  endif()
endforeach()
add_library(crypto_x509_source_objects OBJECT
  "${CRYPTO_X509_SOURCE_ROOT}/src/crypto_x509_der.c")
target_include_directories(crypto_x509_source_objects PRIVATE
  "${CRYPTO_X509_SOURCE_ROOT}/include")
target_link_libraries(crypto_x509_source_objects PRIVATE Salts::ASN1)
set_target_properties(crypto_x509_source_objects PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_executable(crypto_x509_der_source_test
  "${CRYPTO_X509_SOURCE_ROOT}/tests/x509/test_x509_der.c"
  $<TARGET_OBJECTS:crypto_x509_source_objects>)
target_include_directories(crypto_x509_der_source_test PRIVATE
  "${CRYPTO_X509_SOURCE_ROOT}/include")
target_compile_features(crypto_x509_der_source_test PRIVATE c_std_11)
set_target_properties(crypto_x509_der_source_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(crypto_x509_der_source_test PRIVATE Salts::ASN1)
target_compile_definitions(crypto_x509_der_source_test PRIVATE
  "CRYPTO_X509_TEST_P256_PEM=\"${CRYPTO_X509_SOURCE_ROOT}/tests/fixtures/p256-leaf-sha384.pem\""
  "CRYPTO_X509_TEST_P384_PEM=\"${CRYPTO_X509_SOURCE_ROOT}/tests/fixtures/p384-leaf-sha384.pem\"")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  foreach(_target IN ITEMS crypto_x509_source_objects crypto_x509_der_source_test)
    target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endforeach()
elseif(MSVC)
  foreach(_target IN ITEMS crypto_x509_source_objects crypto_x509_der_source_test)
    target_compile_options(${_target} PRIVATE /W4 /WX)
  endforeach()
endif()
add_test(NAME crypto.x509.source_real_certificates
  COMMAND crypto_x509_der_source_test)
set_tests_properties(crypto.x509.source_real_certificates PROPERTIES TIMEOUT 120)

# Original SaltsNet ASN.1-side X.509 cert metadata regression fixture,
# now qualified against the one Crypto owner instead of a second library.
add_executable(crypto_x509_moved_saltsnet_fixture_test
  "${CRYPTO_X509_SOURCE_ROOT}/tests/x509/test_saltsnet_x509_der.c"
  $<TARGET_OBJECTS:crypto_x509_source_objects>)
target_include_directories(crypto_x509_moved_saltsnet_fixture_test PRIVATE
  "${CRYPTO_X509_SOURCE_ROOT}/include"
  "${CRYPTO_X509_SOURCE_ROOT}/tests/x509")
target_compile_features(crypto_x509_moved_saltsnet_fixture_test PRIVATE c_std_11)
target_link_libraries(crypto_x509_moved_saltsnet_fixture_test PRIVATE Salts::ASN1)
set_target_properties(crypto_x509_moved_saltsnet_fixture_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(crypto_x509_moved_saltsnet_fixture_test PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
elseif(MSVC)
  target_compile_options(crypto_x509_moved_saltsnet_fixture_test PRIVATE /W4 /WX)
endif()
add_test(NAME crypto.x509.migrated_saltsnet_fixture
         COMMAND crypto_x509_moved_saltsnet_fixture_test)
