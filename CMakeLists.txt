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

# Keep the minimal foundation target usable without platform/transport providers.
add_library(saltscore_core SHARED src/bytes.c)
add_library(Salts::Core ALIAS saltscore_core)
target_compile_features(saltscore_core PUBLIC c_std_11)
set_target_properties(saltscore_core PROPERTIES
  EXPORT_NAME Core C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_include_directories(saltscore_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# Public mem_pool_t and mem_buffer_t contain C11 atomic fields on MSVC.
# Consumers including cmeta_buffer.h require the same C compiler setting.
if(MSVC)
  target_compile_options(saltscore_core PUBLIC
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:-experimental:c11atomics>)
endif()

# Platform extraction from Salts: OS-owned time, CSPRNG, and synchronization.
# Historical cmeta_ symbol names remain until a separately qualified ABI epoch.
# Platform owns the original registration/dispatch state. Never compile
# another copy into CFlow or NativeIO. No provider/ACE dependency.
option(SALTSCORE_ENABLE_EPOLL_READINESS
  "Explicitly include Linux epoll readiness backend" OFF)
if(SALTSCORE_ENABLE_EPOLL_READINESS AND
   NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "epoll readiness is available only on Linux")
endif()
set(_saltscore_platform_sources
    src/platform/clock.c src/platform/random.c src/platform/readiness.c
    src/platform/portable_atomic.c src/platform/portable_once.c
    src/platform/c11_threads.c src/platform/fastpath.c)
if(WIN32)
  list(APPEND _saltscore_platform_sources
    src/platform/thread_win32.c src/platform/portable_thread_win32.c src/platform/portable_tss_win32.c)
else()
  list(APPEND _saltscore_platform_sources
    src/platform/thread_posix.c src/platform/portable_thread_posix.c src/platform/portable_tss_posix.c)
endif()
if(UNIX)
  list(APPEND _saltscore_platform_sources src/platform/readiness_poll.c)
  if(APPLE)
    list(APPEND _saltscore_platform_sources src/platform/readiness_kqueue.c)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
         SALTSCORE_ENABLE_EPOLL_READINESS)
    list(APPEND _saltscore_platform_sources src/platform/readiness_epoll.c)
  endif()
endif()
add_library(saltscore_platform SHARED ${_saltscore_platform_sources})
add_library(Salts::Platform ALIAS saltscore_platform)
set_target_properties(saltscore_platform PROPERTIES
  EXPORT_NAME Platform C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_compile_features(saltscore_platform PUBLIC c_std_11)
# Export one owner-controlled C ABI to TinyCC, MSVC, and llvm-mingw clients.
target_compile_definitions(saltscore_platform PRIVATE SALTS_CORE_PLATFORM_BUILD=1)
target_include_directories(saltscore_platform PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_platform PUBLIC Threads::Threads)
# Windows MSVC C11 atomic contract is mandatory for the readiness handles.
if(MSVC)
  target_compile_options(saltscore_platform PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
endif()
if(UNIX)
  target_compile_definitions(saltscore_platform PRIVATE
    SALTS_ENABLE_POLL_READINESS=1)
endif()
if(APPLE)
  target_compile_definitions(saltscore_platform PRIVATE
    SALTS_ENABLE_KQUEUE_READINESS=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
       SALTSCORE_ENABLE_EPOLL_READINESS)
  target_compile_definitions(saltscore_platform PRIVATE
    SALTS_ENABLE_EPOLL_READINESS=1)
endif()
if(WIN32)
  target_link_libraries(saltscore_platform PRIVATE bcrypt)
elseif(APPLE)
  target_compile_definitions(saltscore_platform PRIVATE _DARWIN_C_SOURCE=1)
elseif(UNIX)
  target_compile_definitions(saltscore_platform PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()

# Salts Core buffer/pool primitive, no reflection. Static Core consumes
# only the OS synchronization provided by Platform.
target_sources(saltscore_core PRIVATE
  src/core/cmeta_buffer.c src/core/ring_buffer.c src/runtime_abi.c src/utf8.c)
target_link_libraries(saltscore_core PUBLIC Salts::Platform)

# The original bounded primitives have no Reflection/CFlow dependency.
# Keep all six original source units together for full API surface coverage.
add_library(saltscore_concurrency SHARED
  src/concurrency/deadline_queue.c src/concurrency/disruptor.c
  src/concurrency/rcu.c src/concurrency/spsc_ring.c
  src/concurrency/thread_pool.c src/concurrency/thread_pool_lf.c)
add_library(Salts::Concurrency ALIAS saltscore_concurrency)
set_target_properties(saltscore_concurrency PROPERTIES
  EXPORT_NAME Concurrency C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_compile_features(saltscore_concurrency PUBLIC c_std_11)
target_include_directories(saltscore_concurrency PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_concurrency PUBLIC Salts::Platform)
if(MSVC)
  target_compile_options(saltscore_concurrency PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
endif()
if(APPLE)
  target_compile_definitions(saltscore_concurrency PRIVATE _DARWIN_C_SOURCE=1)
elseif(UNIX)
  target_compile_definitions(saltscore_concurrency PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()

# Portable coroutine engine (minicoro remains private, no CMeta facade).
add_library(saltscore_coroutine SHARED
  src/coroutine/coro.c
  src/coroutine/coro_pool.c
  src/coroutine/coro_executor.c)
add_library(Salts::Coroutine ALIAS saltscore_coroutine)
set_target_properties(saltscore_coroutine PROPERTIES
  EXPORT_NAME Coroutine C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_compile_features(saltscore_coroutine PUBLIC c_std_11)
target_include_directories(saltscore_coroutine
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/minicoro)
target_link_libraries(saltscore_coroutine PUBLIC Salts::Concurrency)
if(APPLE)
  target_compile_definitions(saltscore_coroutine PRIVATE _DARWIN_C_SOURCE=1)
elseif(UNIX)
  target_compile_definitions(saltscore_coroutine PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()

# Platform native completion/readiness ownership (no CFlow or CMeta).
add_library(saltscore_native_io SHARED
  src/native-io/native_io.c
  src/native-io/native_io_accept_escrow.c
  src/native-io/native_io_sharded.c
  src/native-io/native_ipc.c)
add_library(Salts::NativeIO ALIAS saltscore_native_io)
if(WIN32)
  target_sources(saltscore_native_io PRIVATE
    src/native-io/native_io_iocp.c src/native-io/native_ipc_windows.c)
  target_compile_definitions(saltscore_native_io PRIVATE
    WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(saltscore_native_io PRIVATE
    src/native-io/native_io_linux.c src/native-io/native_io_epoll.c
    src/native-io/native_io_io_uring.c src/native-io/native_io_readiness.c
    src/native-io/native_ipc_posix.c)
  target_compile_definitions(saltscore_native_io PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
elseif((APPLE OR CMAKE_SYSTEM_NAME MATCHES "^(FreeBSD|OpenBSD|NetBSD|DragonFly)$")
       AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  target_sources(saltscore_native_io PRIVATE
    src/native-io/native_io_bsd.c src/native-io/native_io_kqueue.c
    src/native-io/native_io_readiness.c src/native-io/native_ipc_posix.c)
  if(APPLE)
    target_compile_definitions(saltscore_native_io PRIVATE _DARWIN_C_SOURCE=1)
  endif()
else()
  target_sources(saltscore_native_io PRIVATE
    src/native-io/native_io_unsupported.c src/native-io/native_ipc_posix.c)
endif()
set_target_properties(saltscore_native_io PROPERTIES
  EXPORT_NAME NativeIO C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_compile_features(saltscore_native_io PUBLIC c_std_11)
target_include_directories(saltscore_native_io PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_native_io
  PUBLIC Salts::Platform
  PRIVATE Salts::Coroutine)
if(WIN32)
  target_link_libraries(saltscore_native_io PRIVATE ws2_32)
endif()

# The URI parser is independent C11 code generated by an explicit host re2c
# binary. Keep this optional so minimal Core never requires parser tooling.
option(SALTSCORE_BUILD_URI "Build the standalone re2c-generated URI parser" OFF)
if(SALTSCORE_BUILD_URI)
  find_program(SALTSCORE_RE2C_EXECUTABLE NAMES re2c REQUIRED)
  if(NOT IS_ABSOLUTE "${SALTSCORE_RE2C_EXECUTABLE}")
    message(FATAL_ERROR "SaltsCore URI requires an absolute host re2c path")
  endif()
  set(_saltscore_uri_generated "${CMAKE_CURRENT_BINARY_DIR}/uri_lexer_gen.c")
  add_custom_command(
    OUTPUT "${_saltscore_uri_generated}"
    COMMAND "${SALTSCORE_RE2C_EXECUTABLE}" -o "${_saltscore_uri_generated}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/uri/parser/uri_lexer.re"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/uri/parser/uri_lexer.re"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/uri/parser/uri_parser_internal.h"
            "${SALTSCORE_RE2C_EXECUTABLE}"
    COMMENT "Generating source-compatible URI lexer with re2c"
    VERBATIM)
  add_library(saltscore_uri STATIC
    src/uri/uri_parser.c "${_saltscore_uri_generated}")
  add_library(Salts::URI ALIAS saltscore_uri)
  target_compile_features(saltscore_uri PUBLIC c_std_11)
  set_target_properties(saltscore_uri PROPERTIES
    EXPORT_NAME URI C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden)
  target_include_directories(saltscore_uri
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/uri/parser")
  install(TARGETS saltscore_uri EXPORT SaltsCoreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES include/uri_parser.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

# Provider-free C11 WebSocket frame parser. This is a private CNet build
# component, not a public SDK target. Minimal Core never links it.
option(SALTSCORE_BUILD_CNET "Build standalone CNet with required pinned providers" OFF)
option(SALTSCORE_BUILD_WEBSOCKET_PARSER "Qualify the CNet-private WebSocket frame parser" OFF)
if(SALTSCORE_BUILD_WEBSOCKET_PARSER OR SALTSCORE_BUILD_CNET)
  add_library(saltscore_wsparser OBJECT
    src/cnet/wsparser/websocket_frame_parser.c)
  target_compile_features(saltscore_wsparser PUBLIC c_std_11)
  set_target_properties(saltscore_wsparser PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
  target_include_directories(saltscore_wsparser PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cnet/wsparser")
endif()

# Optional CNet uses exactly the native Core targets and explicit provider
# packages. There is no implicit source fallback or direct old Salts import.
if(SALTSCORE_BUILD_CNET)
  if(NOT TARGET Salts::URI)
    message(FATAL_ERROR
      "Salts::CNet requires SALTSCORE_BUILD_URI=ON (real re2c at build time)")
  endif()
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SaltsCoreCNet.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SaltsCoreExtraModules.cmake")

# One optional native C11 SCTP source owner; never an old package or fallback.
option(SALTSCORE_BUILD_SCTP "Build standalone C11 SCTP Wire and Session" OFF)
if(SALTSCORE_BUILD_SCTP)
  add_subdirectory(cnet/sctp)
endif()

# PE/COFF does not honor ELF/Mach-O SOVERSION. Encode the pre-stable 0.1 ABI
# epoch in each Windows DLL and its import library, without a filename alias.
# A future 0.2 DLL must not replace an older consumer's 0.1 dependency.
if(WIN32)
  foreach(_runtime IN ITEMS core platform concurrency coroutine native_io)
    set_target_properties(saltscore_${_runtime} PROPERTIES
      OUTPUT_NAME "saltscore_${_runtime}-${SALTSCORE_NATIVE_SOVERSION}")
  endforeach()
endif()

install(TARGETS saltscore_core saltscore_platform saltscore_concurrency saltscore_coroutine saltscore_native_io
  EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/salts_core include/salts
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")

install(FILES include/coro.h include/coro_pool.h include/coro_executor.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES vendor/minicoro/LICENSE
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/minicoro)

install(FILES include/vstr.h include/cmeta_api.h include/cmeta_buffer.h include/ring_buffer.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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
     SALTSCORE_BUILD_CSERDE OR SALTSCORE_BUILD_TINYTEST OR
     SALTSCORE_BUILD_ASN1)
    if(NOT SALTSCORE_BUILD_TINYTEST)
      message(FATAL_ERROR "Native conformance requires SALTSCORE_BUILD_TINYTEST=ON")
    endif()
    add_subdirectory(tests/native-modules)
  endif()
endif()
