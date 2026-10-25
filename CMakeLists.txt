cmake_minimum_required(VERSION 3.20)
project(SaltsCore VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
find_package(Threads REQUIRED)

# Keep the minimal foundation target usable without platform/transport providers.
add_library(saltscore_core STATIC src/bytes.c)
add_library(SaltsCore::Core ALIAS saltscore_core)
target_compile_features(saltscore_core PUBLIC c_std_11)
set_target_properties(saltscore_core PROPERTIES
  EXPORT_NAME Core C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden)
target_include_directories(saltscore_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Platform extraction from Salts: OS-owned time, CSPRNG, and synchronization.
# Historical cmeta_ symbol names remain until a separately qualified ABI epoch.
set(_saltscore_platform_sources
    src/platform/clock.c src/platform/random.c)
if(WIN32)
  list(APPEND _saltscore_platform_sources src/platform/thread_win32.c)
else()
  list(APPEND _saltscore_platform_sources src/platform/thread_posix.c)
endif()
add_library(saltscore_platform STATIC ${_saltscore_platform_sources})
add_library(SaltsCore::Platform ALIAS saltscore_platform)
set_target_properties(saltscore_platform PROPERTIES
  EXPORT_NAME Platform C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden)
target_compile_features(saltscore_platform PUBLIC c_std_11)
target_include_directories(saltscore_platform PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_platform PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(saltscore_platform PRIVATE bcrypt)
elseif(APPLE)
  target_compile_definitions(saltscore_platform PRIVATE _DARWIN_C_SOURCE=1)
elseif(UNIX)
  target_compile_definitions(saltscore_platform PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()

# The original bounded primitives have no Reflection/CFlow dependency.
# Keep all six original source units together for full API surface coverage.
add_library(saltscore_concurrency STATIC
  src/concurrency/deadline_queue.c src/concurrency/disruptor.c
  src/concurrency/rcu.c src/concurrency/spsc_ring.c
  src/concurrency/thread_pool.c src/concurrency/thread_pool_lf.c)
add_library(SaltsCore::Concurrency ALIAS saltscore_concurrency)
set_target_properties(saltscore_concurrency PROPERTIES
  EXPORT_NAME Concurrency C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden)
target_compile_features(saltscore_concurrency PUBLIC c_std_11)
target_include_directories(saltscore_concurrency PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_concurrency PUBLIC SaltsCore::Platform)
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

install(TARGETS saltscore_core saltscore_platform saltscore_concurrency
  EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/salts_core include/salts
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")

set(_saltscore_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/SaltsCore")
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SaltsCoreConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfig.cmake"
  INSTALL_DESTINATION "${_saltscore_package_dir}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfigVersion.cmake"
  DESTINATION "${_saltscore_package_dir}")
install(EXPORT SaltsCoreTargets FILE SaltsCoreTargets.cmake
  NAMESPACE SaltsCore:: DESTINATION "${_saltscore_package_dir}")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
