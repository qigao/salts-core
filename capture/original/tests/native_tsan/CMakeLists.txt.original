cmake_minimum_required(VERSION 3.25)
project(SaltsUtilsOriginalCaptureNativeTSan LANGUAGES C CXX)
include(CTest)
enable_testing()

# Intentionally independent of the full SDK/vcpkg graph: the EXACT original
# Capture production Linux source (from this PR) and miniaudio are instrumented
# together, including video/screen dispatchers. Nothing substitutes
# salts_capture_* symbols and no user/physical device can be opened by CI.
get_filename_component(SALTS_UTILS_REPO_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CAPTURE_ROOT "${SALTS_UTILS_REPO_ROOT}/capture")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "Private PulseAudio source TSan regression is Linux only")
endif()
if(NOT CMAKE_C_FLAGS MATCHES "fsanitize=thread" OR
   NOT CMAKE_EXE_LINKER_FLAGS MATCHES "fsanitize=thread" OR
   NOT CMAKE_SHARED_LINKER_FLAGS MATCHES "fsanitize=thread")
  message(FATAL_ERROR "Original Capture callback race test must be SOURCE ThreadSanitizer instrumented")
endif()
if(NOT DEFINED NATIVE_TSAN_MINIAUDIO_ROOT OR
   NOT EXISTS "${NATIVE_TSAN_MINIAUDIO_ROOT}/miniaudio.h")
  message(FATAL_ERROR "Exact upstream miniaudio v0.11.22 source is mandatory")
endif()
if(NOT DEFINED ENV{SALTS_ROOT} OR NOT IS_DIRECTORY "$ENV{SALTS_ROOT}")
  message(FATAL_ERROR "Pinned original published Salts.Native SDK is mandatory")
endif()
find_package(Salts CONFIG REQUIRED
  PATHS "$ENV{SALTS_ROOT}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(NOT TARGET Salts::Core)
  message(FATAL_ERROR "The original published Salts::Core is mandatory")
endif()
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
find_package(X11 REQUIRED COMPONENTS Xext)

add_library(salts_capture_original_source_tsan SHARED
  "${CAPTURE_ROOT}/src/capture_video.c"
  "${CAPTURE_ROOT}/src/miniaudio_impl.c"
  "${CAPTURE_ROOT}/src/capture_audio_miniaudio.c"
  "${CAPTURE_ROOT}/src/capture_linux.c"
  "${CAPTURE_ROOT}/src/capture_video_linux.c"
  "${CAPTURE_ROOT}/src/capture_screen_linux.c")
set_target_properties(salts_capture_original_source_tsan PROPERTIES
  OUTPUT_NAME salts_capture
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(salts_capture_original_source_tsan PRIVATE
  SALTS_CAPTURE_BUILD SALTS_CAPTURE_SHARED SALTS_HAS_PIPEWIRE SALTS_HAS_X11
  _GNU_SOURCE=1)
target_include_directories(salts_capture_original_source_tsan PRIVATE
  "${CAPTURE_ROOT}/include"
  "${CAPTURE_ROOT}/src"
  "${NATIVE_TSAN_MINIAUDIO_ROOT}"
  "$ENV{SALTS_ROOT}/include"
  ${X11_INCLUDE_DIR})
target_link_libraries(salts_capture_original_source_tsan PRIVATE
  Salts::Core
  Threads::Threads
  PkgConfig::PIPEWIRE
  ${X11_LIBRARIES} ${X11_Xext_LIB}
  "${CMAKE_DL_LIBS}" m)
# Upstream miniaudio third-party header is compiled with full TSan, but
# suppress its unrelated compiler warnings, never any sanitizer reports.
set_source_files_properties("${CAPTURE_ROOT}/src/miniaudio_impl.c"
  PROPERTIES COMPILE_OPTIONS "-w")

add_executable(salts_capture_native_tsan_regression
  test_native_capture_tsan.c)
set_target_properties(salts_capture_native_tsan_regression PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_include_directories(salts_capture_native_tsan_regression PRIVATE
  "${CAPTURE_ROOT}/include")
target_link_libraries(salts_capture_native_tsan_regression PRIVATE
  salts_capture_original_source_tsan Threads::Threads)
target_compile_options(salts_capture_native_tsan_regression PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_test(NAME salts_capture_native_tsan_virtual_monitor
  COMMAND salts_capture_native_tsan_regression)
set_tests_properties(salts_capture_native_tsan_virtual_monitor
  PROPERTIES TIMEOUT 30)
