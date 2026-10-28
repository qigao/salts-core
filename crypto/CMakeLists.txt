cmake_minimum_required(VERSION 3.21)
project(SaltsCrypto VERSION 0.1.0 LANGUAGES C)

# One static Crypto archive; the sibling ASN1 module owns a separate static target.
# GmSSL source and patches are owned solely by qigao/vcpkg-cache.
# Consume its exact installed static port; never compile/copy upstream GmSSL
# sources inside this repository and never ship a second gmssl binary.
if(NOT DEFINED CRYPTO_GMSSL_ROOT OR
   NOT EXISTS "${CRYPTO_GMSSL_ROOT}/share/gmssl/GmSSLConfig.cmake")
  message(FATAL_ERROR
    "CRYPTO_GMSSL_ROOT must point to a qigao/vcpkg-cache installed gmssl 3.2.0#14 prefix")
endif()
file(TO_CMAKE_PATH "${CRYPTO_GMSSL_ROOT}" _gmssl_prefix)
if(NOT EXISTS "${_gmssl_prefix}/include/gmssl/tls.h" OR
   NOT EXISTS "${_gmssl_prefix}/include/gmssl/sm3.h")
  message(FATAL_ERROR "Missing installed GmSSL TLS/SM3 headers")
endif()
# GmSSLConfig.cmake is from the vcpkg port and uses find_path/find_library.
# During cross-compilation its exact already-installed host-visible absolute
# prefix must not be re-rooted through the Android/iOS sysroot. Restore the
# caller's find modes immediately after importing the pinned package.
set(_gmssl_had_include_root FALSE)
set(_gmssl_had_library_root FALSE)
if(DEFINED CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)
  set(_gmssl_had_include_root TRUE)
  set(_gmssl_old_include_root "${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}")
endif()
if(DEFINED CMAKE_FIND_ROOT_PATH_MODE_LIBRARY)
  set(_gmssl_had_library_root TRUE)
  set(_gmssl_old_library_root "${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
find_package(GmSSL CONFIG REQUIRED
  PATHS "${_gmssl_prefix}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(_gmssl_had_include_root)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE "${_gmssl_old_include_root}")
else()
  unset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)
endif()
if(_gmssl_had_library_root)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "${_gmssl_old_library_root}")
else()
  unset(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY)
endif()
unset(_gmssl_had_include_root)
unset(_gmssl_had_library_root)
unset(_gmssl_old_include_root)
unset(_gmssl_old_library_root)
if(NOT TARGET GmSSL::GmSSL)
  message(FATAL_ERROR "The vcpkg gmssl port must import GmSSL::GmSSL")
endif()
get_target_property(_gmssl_archive GmSSL::GmSSL IMPORTED_LOCATION_RELEASE)
if(NOT _gmssl_archive OR NOT EXISTS "${_gmssl_archive}" OR
   NOT _gmssl_archive MATCHES "[.](a|lib)$")
  message(FATAL_ERROR "GmSSL port must contain a real static archive, not a DSO")
endif()
get_filename_component(_gmssl_archive "${_gmssl_archive}" REALPATH)
file(TO_CMAKE_PATH "${_gmssl_archive}" _gmssl_archive)
if(WIN32)
  set(_gmssl_expected_archive "${_gmssl_prefix}/lib/gmssl.lib")
else()
  set(_gmssl_expected_archive "${_gmssl_prefix}/lib/libgmssl.a")
endif()
if(NOT EXISTS "${_gmssl_expected_archive}")
  message(FATAL_ERROR "Pinned vcpkg GmSSL static archive missing from its installation prefix")
endif()
get_filename_component(_gmssl_expected_archive "${_gmssl_expected_archive}" REALPATH)
file(TO_CMAKE_PATH "${_gmssl_expected_archive}" _gmssl_expected_archive)
if(NOT _gmssl_archive STREQUAL _gmssl_expected_archive)
  message(FATAL_ERROR
    "GmSSL::GmSSL must resolve to the static library in CRYPTO_GMSSL_ROOT; another owner is forbidden")
endif()
unset(_gmssl_expected_archive)
# The imported GmSSL target must remain BUILD-PRIVATE. The only installed
# library target from this project is the combined Salts::Crypto archive.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Ed448 libecc source uses the reviewed 64-bit WORDSIZE ABI")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
find_package(Threads REQUIRED)
# The parent project owns ASN1 first; standalone Crypto consumes its installed SDK.
if(NOT TARGET Salts::ASN1)
  find_package(ASN1 0.1 CONFIG REQUIRED)
endif()
if(NOT TARGET Salts::ASN1)
  message(FATAL_ERROR "Crypto requires canonical Salts::ASN1")
endif()

set(_libecc "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libecc")
# Compile libecc's reviewed Ed448 subset in its own internal OBJECT target.
# Fold these objects directly into the sole installed Salts::Crypto archive.
add_subdirectory(vendor/libecc)

# No separate libecc archive; only its required Ed448 code is compiled.
# SHA256 is the existing, pinned GmSSL implementation, not a duplicate libecc SHA.
# Generic ASN.1 BER/DER now comes solely from imported Salts::ASN1.
# Crypto retains only ECDSA DER and RFC 8410 key-format functions.
add_library(salts_crypto STATIC
  src/crypto_tls_gmssl.c
  src/crypto_tls_cipher_bio.c
  src/crypto_ed448.c
  src/crypto_sha256.c
  src/crypto_random.c
  src/asn1/crypto.c
  src/crypto_x509_der.c
  $<TARGET_OBJECTS:crypto_libecc_ed448_objects>)
add_library(Salts::Crypto ALIAS salts_crypto)
set_target_properties(salts_crypto PROPERTIES
  OUTPUT_NAME salts_crypto EXPORT_NAME Crypto
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
target_compile_features(salts_crypto PUBLIC c_std_11)
target_link_libraries(salts_crypto PUBLIC Threads::Threads Salts::ASN1)
target_include_directories(salts_crypto
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE "${_gmssl_prefix}/include"
          "${_libecc}/include")
target_compile_definitions(salts_crypto PRIVATE
  ENABLE_TLS ENABLE_SECP256R1 ENABLE_SECP384R1
  ENABLE_SHA1 ENABLE_SHA2 ENABLE_AES)

# Keep the Ed448 facade's configuration consistent with the private object
# target. No SHA256/SM3 libecc implementation or alternate RNG is built.
set_source_files_properties(src/crypto_ed448.c
  PROPERTIES COMPILE_DEFINITIONS
  "WITH_STDLIB;WITH_LIBECC_CONFIG_OVERRIDE;WITH_CURVE_WEI448;WITH_HASH_SHAKE256;WITH_SIG_EDDSA448;WORDSIZE=64")

if(MSVC)
  target_compile_options(salts_crypto PRIVATE /experimental:c11atomics)
  target_link_libraries(salts_crypto PUBLIC bcrypt ws2_32)
elseif(WIN32)
  target_link_libraries(salts_crypto PUBLIC bcrypt ws2_32)
elseif(APPLE)
  target_link_libraries(salts_crypto PUBLIC "-framework Security")
elseif(UNIX)
  target_link_libraries(salts_crypto PUBLIC m "${CMAKE_DL_LIBS}")
endif()

# Fold ALL compiled native GmSSL objects into this one installed archive.
# PrivateGmSSL is neither an installed/imported target nor a consumer dependency.
add_custom_command(TARGET salts_crypto POST_BUILD
  COMMAND "${CMAKE_COMMAND}"
    "-DARCHIVE=$<TARGET_FILE:salts_crypto>"
    "-DGMSSL_ARCHIVE=${_gmssl_archive}"
    "-DARCHIVER=${CMAKE_AR}"
    "-DRANLIB=${CMAKE_RANLIB}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/merge_static_crypto.cmake"
  VERBATIM)

install(TARGETS salts_crypto EXPORT SaltsCryptoTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/crypto
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
# Expose the vetted GmSSL SM3 API directly from the same static archive;
# no Crypto Provider header, trampoline or independently installed GmSSL lib.
# Explicitly canonicalize the absolute source path before generating
# cmake_install.cmake: on Windows GITHUB_WORKSPACE may contain backslashes.
file(TO_CMAKE_PATH "${_gmssl_prefix}/include/gmssl/sm3.h"
  _crypto_gmssl_sm3_header)
install(FILES "${_crypto_gmssl_sm3_header}"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/gmssl")
unset(_crypto_gmssl_sm3_header)
# The linked GmSSL source and public sm3.h are Apache-2.0. Redistribute
# the license from the exact installed vcpkg package, not a copied vendor tree.
if(NOT EXISTS "${_gmssl_prefix}/share/gmssl/copyright")
  message(FATAL_ERROR "vcpkg GmSSL license missing from installed package")
endif()
install(FILES "${_gmssl_prefix}/share/gmssl/copyright"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/Crypto/GmSSL"
  RENAME LICENSE)
install(FILES "${_libecc}/LICENSE"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/Crypto/libecc)

set(_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Crypto")
install(EXPORT SaltsCryptoTargets FILE CryptoTargets.cmake
  NAMESPACE Salts:: DESTINATION "${_package_dir}")
configure_package_config_file(cmake/CryptoConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/CryptoConfig.cmake"
  INSTALL_DESTINATION "${_package_dir}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/CryptoConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/CryptoConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/CryptoConfigVersion.cmake"
  DESTINATION "${_package_dir}")

if(BUILD_TESTING)
  enable_language(CXX)
  add_subdirectory(tests/algorithms)
  add_subdirectory(tests/asn1)
  add_subdirectory(tests/x509)
endif()
