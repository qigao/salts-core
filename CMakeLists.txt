cmake_minimum_required(VERSION 3.20)
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
find_package(Threads REQUIRED)

# Package root assembles canonical modules; modules own sources and installs.
add_subdirectory(platform)
add_subdirectory(core)
add_subdirectory(concurrency)
add_subdirectory(coroutine)
add_subdirectory(native-io)

option(SALTSCORE_BUILD_URI "Build standalone re2c-generated URI parser" OFF)
if(SALTSCORE_BUILD_URI)
  add_subdirectory(uri)
endif()
option(SALTSCORE_BUILD_CNET "Build standalone CNet with required pinned providers" OFF)
option(SALTSCORE_BUILD_WEBSOCKET_PARSER "Qualify CNet-private WebSocket frame parser" OFF)
if(SALTSCORE_BUILD_CNET AND NOT TARGET Salts::URI)
  message(FATAL_ERROR "Salts::CNet requires SALTSCORE_BUILD_URI=ON (real re2c at build time)")
endif()
if(SALTSCORE_BUILD_WEBSOCKET_PARSER OR SALTSCORE_BUILD_CNET)
  add_subdirectory(cnet)
endif()

option(SALTSCORE_BUILD_CSERDE "Build standalone C11 CSerde owner" OFF)
option(SALTSCORE_BUILD_TINYTEST "Build standalone C11 TinyTest owner" OFF)
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
if(SALTSCORE_BUILD_IDNA AND NOT SALTSCORE_BUILD_UNICODE)
  message(FATAL_ERROR "Salts::IDNA requires explicit SALTSCORE_BUILD_UNICODE=ON")
endif()
if(SALTSCORE_BUILD_UNICODE)
  add_subdirectory(unicode)
endif()
if(SALTSCORE_BUILD_IDNA)
  add_subdirectory(idna)
endif()
option(SALTSCORE_BUILD_FS "Build standalone native FS owner" OFF)
if(SALTSCORE_BUILD_FS)
  add_subdirectory(fs)
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
  if(SALTSCORE_BUILD_UNICODE OR SALTSCORE_BUILD_IDNA OR
     SALTSCORE_BUILD_CSERDE OR SALTSCORE_BUILD_TINYTEST)
    if(NOT SALTSCORE_BUILD_TINYTEST)
      message(FATAL_ERROR "Native conformance requires SALTSCORE_BUILD_TINYTEST=ON")
    endif()
    add_subdirectory(tests/native-modules)
  endif()
endif()
