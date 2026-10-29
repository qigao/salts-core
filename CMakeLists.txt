cmake_minimum_required(VERSION 3.21)
project(SaltsCore VERSION 0.1.0 LANGUAGES C)
# Pre-stable ABI epoch: 0.2 must not silently satisfy an installed 0.1 DLL/DSO.
set(SALTSCORE_NATIVE_SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

# All test executables and runtime DLLs share a directory on multi-config Windows.
# This is DLL discovery, not a copy of the runtime or a static fallback.
if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
# Crypto's C++17 header tests are siblings of the other Core source tests.
# Enable the language at the package root so all test subdirectories see it.
if(BUILD_TESTING)
  enable_language(CXX)
endif()
find_package(Threads REQUIRED)

# Package root assembles canonical modules; modules own sources and installs.
# Original logging uses existing FS and Concurrency implementations;
# compile those single owners first, without a second logger provider.
add_subdirectory(platform)
add_subdirectory(concurrency)
add_subdirectory(fs)
add_subdirectory(core)
add_subdirectory(coroutine)
add_subdirectory(native-io)
# Original formal concurrency/ownership models, source-owned and retained.
add_subdirectory(verification)

# ASN1 is an always-assembled, first-party, provider-neutral codec.
# Never import a second ASN1 implementation or an external source repository.
if(TARGET Salts::ASN1)
  message(FATAL_ERROR "Duplicate ASN1 owner: never import and build twice")
endif()
add_subdirectory(asn1)

# ASN1, Crypto, URI and CNet are fixed local modules of the Core SDK.
# Retired inputs are errors so an old OFF or foreign prefix cannot silently
# select a different dependency graph.
foreach(_retired_input IN ITEMS SALTSCORE_BUILD_CNET CRYPTO_GMSSL_ROOT
    SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST)
  if(DEFINED ${_retired_input})
    message(FATAL_ERROR "${_retired_input} was removed; Core builds local Crypto and CNet unconditionally")
  endif()
endforeach()
if(TARGET Salts::Crypto)
  message(FATAL_ERROR "Duplicate Crypto owner: use exactly one static archive")
endif()
add_subdirectory(crypto)
# Keep explicit URI=ON callers compatible; disabling a mandatory dependency
# must fail rather than omit source or silently change parsing behavior.
if(DEFINED SALTSCORE_BUILD_URI AND NOT SALTSCORE_BUILD_URI)
  message(FATAL_ERROR "Core CNet requires its local URI parser")
endif()
add_subdirectory(uri)
option(SALTSCORE_BUILD_WEBSOCKET_PARSER "Qualify CNet-private WebSocket frame parser" OFF)
add_subdirectory(vendor)
add_subdirectory(cnet)

option(SALTSCORE_TEST_CNET_CRYPTO_QUALIFICATION
  "Run additional test-only Crypto facade and original Owner qualification" OFF)
if(SALTSCORE_TEST_CNET_CRYPTO_QUALIFICATION AND NOT BUILD_TESTING)
  message(FATAL_ERROR "Crypto facade qualification requires BUILD_TESTING")
endif()

option(SALTSCORE_BUILD_CSERDE "Build standalone C11 CSerde owner" OFF)
option(SALTSCORE_BUILD_TINYTEST "Build standalone C11 TinyTest owner" OFF)
option(SALTSCORE_BUILD_SIMD "Build the independent static C11 SIMD execution module" OFF)
option(SALTSCORE_BUILD_UNICODE "Build original pinned Unicode 17 owner" OFF)
option(SALTSCORE_BUILD_IDNA "Build original pinned Unicode 17 IDNA owner" OFF)
option(SALTSCORE_NATIVE_CPP_CONFORMANCE
  "Compile original native C++17 source contract against standalone Core" OFF)
if(SALTSCORE_BUILD_CSERDE)
  add_subdirectory(cserde)
endif()
if(SALTSCORE_BUILD_TINYTEST)
  add_subdirectory(tinytest)
endif()
if(SALTSCORE_BUILD_SIMD)
  add_subdirectory(simd)
endif()
option(SALTSCORE_BUILD_USB "Build the native libusb USB owner" OFF)
if(SALTSCORE_BUILD_USB)
  add_subdirectory(usb)
endif()
option(SALTSCORE_BUILD_PLAYBACK "Build the native PCM Playback owner" OFF)
if(SALTSCORE_BUILD_PLAYBACK)
  add_subdirectory(playback)
endif()
option(SALTSCORE_BUILD_SERIAL "Build the native Serial owner" OFF)
if(SALTSCORE_BUILD_SERIAL)
  add_subdirectory(salts_serial)
endif()
option(SALTSCORE_BUILD_CAPTURE "Build the native audio/video/screen Capture owner" OFF)
if(SALTSCORE_BUILD_CAPTURE)
  add_subdirectory(capture)
endif()
if(SALTSCORE_BUILD_IDNA AND NOT SALTSCORE_BUILD_UNICODE)
  message(FATAL_ERROR "Salts::IDNA requires explicit SALTSCORE_BUILD_UNICODE=ON")
endif()
if(SALTSCORE_BUILD_UNICODE)
  add_subdirectory(unicode)
endif()
if(SALTSCORE_BUILD_IDNA)
  add_subdirectory(idna)
endif()
# Salts::FS is an unconditional Core logger dependency, not a fallback.
# A caller cannot silently disable the real file sink while preserving TLog.
if(DEFINED SALTSCORE_BUILD_FS AND NOT SALTSCORE_BUILD_FS)
  message(FATAL_ERROR "Restored Core/TLog requires canonical Salts::FS")
endif()
option(SALTSCORE_BUILD_PROCESS "Build standalone Process owner" OFF)
if(SALTSCORE_BUILD_PROCESS)
  add_subdirectory(process)
endif()

option(SALTSCORE_CNET_OWNER_SOURCE_TESTS
  "Qualify original CNet Owner and actual retained NativeIO terminal" OFF)
if(SALTSCORE_CNET_OWNER_SOURCE_TESTS)
  if(NOT BUILD_TESTING OR NOT SALTSCORE_BUILD_TINYTEST OR
     NOT TARGET Salts::TinyTest)
    message(FATAL_ERROR
      "CNet original source tests require BUILD_TESTING and Core TinyTest")
  endif()
  add_subdirectory(tests/cnet-owner)
endif()
option(SALTSCORE_BUILD_SCTP "Build standalone C11 SCTP Wire and Session" OFF)
if(SALTSCORE_BUILD_SCTP)
  add_subdirectory(sctp)
endif()

# Package config/version and export assembly are global concerns.
set(_saltscore_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/SaltsCore")
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SaltsCoreConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfig.cmake"
  INSTALL_DESTINATION "${_saltscore_package_dir}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfigVersion.cmake"
  DESTINATION "${_saltscore_package_dir}")
install(EXPORT SaltsCoreTargets FILE SaltsCoreTargets.cmake
  NAMESPACE Salts:: DESTINATION "${_saltscore_package_dir}")

if(BUILD_TESTING)
  add_subdirectory(tests)
  if(TARGET Salts::TinyTest)
    add_subdirectory(concurrency/tests)
  endif()
  if(SALTSCORE_BUILD_UNICODE OR SALTSCORE_BUILD_IDNA OR
     SALTSCORE_BUILD_CSERDE OR SALTSCORE_BUILD_TINYTEST)
    if(NOT SALTSCORE_BUILD_TINYTEST)
      message(FATAL_ERROR "Native conformance requires SALTSCORE_BUILD_TINYTEST=ON")
    endif()
    add_subdirectory(tests/native-modules)
  endif()
endif()
