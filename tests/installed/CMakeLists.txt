cmake_minimum_required(VERSION 3.20)
project(SaltsCoreInstalledConsumer LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)

option(SALTSCORE_REQUIRE_SIMD "Require the separately qualified installed static SIMD module" OFF)
if(SALTSCORE_REQUIRE_SIMD AND NOT TARGET Salts::SIMD)
  message(FATAL_ERROR "SIMD qualification requires the installed Salts::SIMD target")
endif()
if(TARGET Salts::SIMD)
  get_target_property(_simd_imported Salts::SIMD IMPORTED)
  get_target_property(_simd_type Salts::SIMD TYPE)
  if(NOT _simd_imported OR NOT _simd_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "SIMD consumers require the installed static module")
  endif()
  add_executable(simd_consumer_c simd_consumer.c)
  target_compile_features(simd_consumer_c PRIVATE c_std_11)
  target_link_libraries(simd_consumer_c PRIVATE Salts::SIMD)
  add_test(NAME sdk.simd_c11 COMMAND simd_consumer_c)
  add_executable(simd_consumer_cpp simd_consumer.cpp)
  target_compile_features(simd_consumer_cpp PRIVATE cxx_std_17)
  target_link_libraries(simd_consumer_cpp PRIVATE Salts::SIMD)
  add_test(NAME sdk.simd_cpp17 COMMAND simd_consumer_cpp)
  foreach(_simd_consumer IN ITEMS simd_consumer_c simd_consumer_cpp)
    if(MSVC)
      target_compile_options(${_simd_consumer} PRIVATE /W4 /WX)
    else()
      target_compile_options(${_simd_consumer} PRIVATE -Wall -Wextra -Werror)
    endif()
  endforeach()
endif()

# find_package name remains SaltsCore; its ONLY imported link namespace is Salts::.
foreach(_owner IN ITEMS Core Platform Concurrency Coroutine NativeIO ASN1 Crypto URI CNet)
  if(NOT TARGET "Salts::${_owner}")
    message(FATAL_ERROR "Required canonical Salts::${_owner} target is absent")
  endif()
  get_target_property(_imported "Salts::${_owner}" IMPORTED)
  if(NOT _imported)
    message(FATAL_ERROR "Salts::${_owner} must be imported from installed SaltsCore")
  endif()
endforeach()

# The combined Core SDK must expose its one source-owned ASN1 archive through
# find_package(SaltsCore), including real codec calls in both language modes.
get_target_property(_asn1_kind Salts::ASN1 TYPE)
if(NOT _asn1_kind STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "SaltsCore must import its single static ASN1 owner")
endif()
add_executable(asn1_from_core_c ../../asn1/tests/installed/consumer.c)
target_compile_features(asn1_from_core_c PRIVATE c_std_11)
target_link_libraries(asn1_from_core_c PRIVATE Salts::ASN1)
add_test(NAME sdk.asn1_from_core_c11 COMMAND asn1_from_core_c)
add_executable(asn1_from_core_cpp ../../asn1/tests/installed/consumer.cpp)
target_compile_features(asn1_from_core_cpp PRIVATE cxx_std_17)
target_link_libraries(asn1_from_core_cpp PRIVATE Salts::ASN1)
add_test(NAME sdk.asn1_from_core_cpp17 COMMAND asn1_from_core_cpp)

# The same installed SDK owns the native Crypto archive. These original
# consumers exercise SM3/SHA256/Ed448 and TLS-provider lifecycle contracts.
get_target_property(_crypto_kind Salts::Crypto TYPE)
if(NOT _crypto_kind STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "SaltsCore must import its single static Crypto owner")
endif()
add_executable(crypto_from_core_c ../../crypto/tests/owner/consumer.c)
target_compile_features(crypto_from_core_c PRIVATE c_std_11)
target_link_libraries(crypto_from_core_c PRIVATE Salts::Crypto)
add_test(NAME sdk.crypto_from_core_c11 COMMAND crypto_from_core_c)
add_executable(crypto_from_core_cpp ../../crypto/tests/owner/consumer.cpp)
target_compile_features(crypto_from_core_cpp PRIVATE cxx_std_17)
target_link_libraries(crypto_from_core_cpp PRIVATE Salts::Crypto)
add_test(NAME sdk.crypto_from_core_cpp17 COMMAND crypto_from_core_cpp)

# Obsolete names are forbidden, never provided as aliases.
set(_forbidden_ns "SaltsCore")
foreach(_owner IN ITEMS Core Platform Concurrency Coroutine NativeIO)
  if(TARGET "${_forbidden_ns}::${_owner}")
    message(FATAL_ERROR "Unexpected noncanonical imported target for ${_owner}")
  endif()
endforeach()

add_executable(consumer_c consumer.c)
target_compile_features(consumer_c PRIVATE c_std_11)
target_link_libraries(consumer_c PRIVATE
  Salts::Core Salts::Platform Salts::Concurrency Salts::Coroutine Salts::NativeIO)
add_test(NAME sdk.consumer_c COMMAND consumer_c)

add_executable(consumer_cpp consumer.cpp)
target_compile_features(consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(consumer_cpp PRIVATE
  Salts::Core Salts::Platform Salts::Concurrency Salts::Coroutine Salts::NativeIO)
add_test(NAME sdk.consumer_cpp COMMAND consumer_cpp)

add_executable(uri_consumer_c uri_consumer.c)
target_compile_features(uri_consumer_c PRIVATE c_std_11)
target_link_libraries(uri_consumer_c PRIVATE Salts::URI)
add_test(NAME sdk.uri_consumer_c COMMAND uri_consumer_c)

add_executable(uri_consumer_cpp uri_consumer.cpp)
target_compile_features(uri_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(uri_consumer_cpp PRIVATE Salts::URI)
add_test(NAME sdk.uri_consumer_cpp COMMAND uri_consumer_cpp)

# CNet is mandatory in the local Core SDK; an absent target cannot skip these
# real installed C/C++ and single-runtime ownership consumers.
get_target_property(_cnet_kind Salts::CNet TYPE)
if(NOT _cnet_kind STREQUAL "SHARED_LIBRARY")
  message(FATAL_ERROR "SaltsCore must import its single shared CNet owner")
endif()
add_executable(cnet_consumer_c cnet_consumer.c)
target_compile_features(cnet_consumer_c PRIVATE c_std_11)
target_link_libraries(cnet_consumer_c PRIVATE Salts::CNet)
add_test(NAME sdk.cnet_consumer_c COMMAND cnet_consumer_c)

add_executable(cnet_consumer_cpp cnet_consumer.cpp)
target_compile_features(cnet_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(cnet_consumer_cpp PRIVATE Salts::CNet)
add_test(NAME sdk.cnet_consumer_cpp COMMAND cnet_consumer_cpp)

# Independent installed SDK build: EXE and plugin both import one Core/Platform
# runtime while CNet is simultaneously loaded. No source-build references.
add_library(owner_probe_installed SHARED ../runtime_owner_module.c)
target_compile_features(owner_probe_installed PRIVATE c_std_11)
set_target_properties(owner_probe_installed PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_link_libraries(owner_probe_installed PRIVATE
  Salts::Core Salts::Platform)
add_executable(abi_owner_consumer abi_owner_consumer.c)
target_compile_features(abi_owner_consumer PRIVATE c_std_11)
target_link_libraries(abi_owner_consumer PRIVATE
  Salts::CNet Salts::Core Salts::Platform owner_probe_installed)
add_test(NAME sdk.abi_owner_consumer COMMAND abi_owner_consumer)

# Canonical original tn_base64_* ABI: installed C11 and C++17 clients each
# link the *same* Salts::Core DSO. No second codec/library or build-tree include.
add_executable(base64_consumer_c base64_consumer.c)
target_compile_features(base64_consumer_c PRIVATE c_std_11)
target_link_libraries(base64_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.base64_consumer_c COMMAND base64_consumer_c)

add_executable(base64_consumer_cpp base64_consumer.cpp)
target_compile_features(base64_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(base64_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.base64_consumer_cpp COMMAND base64_consumer_cpp)

add_executable(error_consumer_c error_consumer.c)
target_compile_features(error_consumer_c PRIVATE c_std_11)
target_link_libraries(error_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.error_c11 COMMAND error_consumer_c)
add_executable(error_consumer_cpp error_consumer.cpp)
target_compile_features(error_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(error_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.error_cpp17 COMMAND error_consumer_cpp)
foreach(_error_consumer IN ITEMS error_consumer_c error_consumer_cpp)
  if(MSVC)
    target_compile_options(${_error_consumer} PRIVATE /W4 /WX)
  else()
    target_compile_options(${_error_consumer} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()

add_executable(mpmc_consumer_c mpmc_consumer.c)
target_compile_features(mpmc_consumer_c PRIVATE c_std_11)
target_link_libraries(mpmc_consumer_c PRIVATE Salts::Concurrency)
add_test(NAME sdk.mpmc_c11 COMMAND mpmc_consumer_c)
add_executable(mpmc_consumer_cpp mpmc_consumer.cpp)
target_compile_features(mpmc_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(mpmc_consumer_cpp PRIVATE Salts::Concurrency)
add_test(NAME sdk.mpmc_cpp17 COMMAND mpmc_consumer_cpp)
add_executable(original_lf_consumer_c
  ../../concurrency/tests/package_config/leader_followers/main.c)
target_compile_features(original_lf_consumer_c PRIVATE c_std_11)
target_link_libraries(original_lf_consumer_c PRIVATE Salts::Concurrency)
add_test(NAME sdk.original_threadpool_lf_c11 COMMAND original_lf_consumer_c)
add_executable(original_lf_consumer_cpp
  ../../concurrency/tests/package_config/leader_followers/main.cpp)
target_compile_features(original_lf_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(original_lf_consumer_cpp PRIVATE Salts::Concurrency)
add_test(NAME sdk.original_threadpool_lf_cpp17 COMMAND original_lf_consumer_cpp)
set_tests_properties(sdk.mpmc_c11 sdk.mpmc_cpp17 PROPERTIES TIMEOUT 30)
foreach(_mpmc_consumer IN ITEMS mpmc_consumer_c mpmc_consumer_cpp)
  if(MSVC)
    target_compile_options(${_mpmc_consumer} PRIVATE /W4 /WX)
  else()
    target_compile_options(${_mpmc_consumer} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()

# Source-independent consumers. CMake package and all native targets come from
# this installed Core prefix; there is no Salts package or reflected adapter.
add_executable(vstr_consumer_c vstr_consumer.c)
target_compile_features(vstr_consumer_c PRIVATE c_std_11)
target_link_libraries(vstr_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.vstr_c11 COMMAND vstr_consumer_c)

add_executable(vstr_consumer_cpp vstr_consumer.cpp)
target_compile_features(vstr_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(vstr_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.vstr_cpp17 COMMAND vstr_consumer_cpp)

if(TARGET Salts::CSerde)
  add_executable(cserde_consumer_c cserde_consumer.c)
  target_compile_features(cserde_consumer_c PRIVATE c_std_11)
  target_link_libraries(cserde_consumer_c PRIVATE Salts::CSerde)
  add_test(NAME sdk.cserde_c11 COMMAND cserde_consumer_c)

  if(TARGET Salts::TinyTest)
    add_executable(cserde_consumer_cpp ../../cserde/tests/cserde_header_cpp_test.cpp)
    target_compile_features(cserde_consumer_cpp PRIVATE cxx_std_17)
    target_link_libraries(cserde_consumer_cpp PRIVATE Salts::CSerde Salts::TinyTest)
    add_test(NAME sdk.cserde_cpp17 COMMAND cserde_consumer_cpp)
  endif()
endif()

if(TARGET Salts::Unicode AND TARGET Salts::IDNA)
  add_executable(unicode_idna_consumer_c ../../unicode/test/install_consumer/main.c)
  target_compile_features(unicode_idna_consumer_c PRIVATE c_std_11)
  target_link_libraries(unicode_idna_consumer_c PRIVATE Salts::Unicode Salts::IDNA)
  add_test(NAME sdk.unicode_idna_c11 COMMAND unicode_idna_consumer_c)

  add_executable(unicode_idna_consumer_cpp ../../unicode/test/install_consumer/main.cpp)
  target_compile_features(unicode_idna_consumer_cpp PRIVATE cxx_std_17)
  target_link_libraries(unicode_idna_consumer_cpp PRIVATE Salts::Unicode Salts::IDNA)
  add_test(NAME sdk.unicode_idna_cpp17 COMMAND unicode_idna_consumer_cpp)
endif()

# Installed client must use ONLY Salts::Platform (not CMeta, stdatomic, or old Salts).
add_executable(portable_concurrency_consumer_c portable_concurrency_consumer.c)
target_compile_features(portable_concurrency_consumer_c PRIVATE c_std_11)
target_link_libraries(portable_concurrency_consumer_c PRIVATE Salts::Platform)
add_test(NAME sdk.portable_concurrency_c COMMAND portable_concurrency_consumer_c)

add_executable(portable_concurrency_consumer_cpp portable_concurrency_consumer.cpp)
target_compile_features(portable_concurrency_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(portable_concurrency_consumer_cpp PRIVATE Salts::Platform)
add_test(NAME sdk.portable_concurrency_cpp COMMAND portable_concurrency_consumer_cpp)


# Installed C11 source compatibility: explicitly select our threads.h, never
# assume the provider's private build tree or system compiler's threads.h.
get_target_property(_platform_sdk_includes Salts::Platform INTERFACE_INCLUDE_DIRECTORIES)
set(_salts_sdk_c11_dir "")
foreach(_candidate IN LISTS _platform_sdk_includes)
  if(IS_DIRECTORY "${_candidate}") # replaced below with per-item expression
    if(EXISTS "${_candidate}/salts_core/c11/threads.h")
      set(_salts_sdk_c11_dir "${_candidate}/salts_core/c11")
      break()
    endif()
  endif()
endforeach()
if(_salts_sdk_c11_dir STREQUAL "")
  message(FATAL_ERROR "Installed Salts::Platform is missing its opt-in C11 threads.h facade")
endif()
foreach(_lang IN ITEMS c cpp)
  add_executable(c11_threads_installed_${_lang} c11_threads_consumer.${_lang})
  target_include_directories(c11_threads_installed_${_lang} BEFORE PRIVATE
    "${_salts_sdk_c11_dir}")
  target_compile_definitions(c11_threads_installed_${_lang} PRIVATE
    SALTS_C11_THREADS_EXPERIMENTAL=1)
  target_link_libraries(c11_threads_installed_${_lang} PRIVATE Salts::Platform)
  add_test(NAME sdk.c11_threads_${_lang} COMMAND c11_threads_installed_${_lang})
endforeach()

# Module-local original tstr/vstr/memory-pool installed contract: consumers
# must link the one existing Salts::Core DSO, not any build-tree SDS archive.
add_executable(original_utils_string_consumer_c original_utils_string_consumer.c)
target_compile_features(original_utils_string_consumer_c PRIVATE c_std_11)
target_link_libraries(original_utils_string_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.original_utils_string_c11 COMMAND original_utils_string_consumer_c)

add_executable(original_utils_string_consumer_cpp original_utils_string_consumer.cpp)
target_compile_features(original_utils_string_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(original_utils_string_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.original_utils_string_cpp17 COMMAND original_utils_string_consumer_cpp)

# Installed-only SPSC consumers must link to one canonical Concurrency owner.
add_executable(spsc_consumer_c spsc_consumer.c)
target_compile_features(spsc_consumer_c PRIVATE c_std_11)
target_link_libraries(spsc_consumer_c PRIVATE Salts::Concurrency)
add_test(NAME sdk.spsc_c11 COMMAND spsc_consumer_c)
add_executable(spsc_consumer_cpp spsc_consumer.cpp)
target_compile_features(spsc_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(spsc_consumer_cpp PRIVATE Salts::Concurrency)
add_test(NAME sdk.spsc_cpp17 COMMAND spsc_consumer_cpp)

# Original formatter-lexer API must resolve from an installed Salts::Core DSO.
add_executable(fmt_lexer_consumer_c fmt_lexer_consumer.c)
target_compile_features(fmt_lexer_consumer_c PRIVATE c_std_11)
target_link_libraries(fmt_lexer_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.fmt_lexer_c11 COMMAND fmt_lexer_consumer_c)
add_executable(fmt_lexer_consumer_cpp fmt_lexer_consumer.cpp)
target_compile_features(fmt_lexer_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(fmt_lexer_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.fmt_lexer_cpp17 COMMAND fmt_lexer_consumer_cpp)

# Installed formatter type tags, enum metadata and C11/C++17 wrappers.
add_executable(fmt_consumer_c fmt_consumer.c)
target_compile_features(fmt_consumer_c PRIVATE c_std_11)
target_link_libraries(fmt_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.fmt_c11 COMMAND fmt_consumer_c)
add_executable(fmt_consumer_cpp fmt_consumer.cpp)
target_compile_features(fmt_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(fmt_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.fmt_cpp17 COMMAND fmt_consumer_cpp)

add_executable(log_pattern_consumer_c log_pattern_consumer.c)
target_compile_features(log_pattern_consumer_c PRIVATE c_std_11)
target_link_libraries(log_pattern_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.log_pattern_c11 COMMAND log_pattern_consumer_c)
add_executable(log_pattern_consumer_cpp log_pattern_consumer.cpp)
target_compile_features(log_pattern_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(log_pattern_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.log_pattern_cpp17 COMMAND log_pattern_consumer_cpp)

add_executable(tlog_consumer_c tlog_consumer.c)
target_compile_features(tlog_consumer_c PRIVATE c_std_11)
target_link_libraries(tlog_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.tlog_c11 COMMAND tlog_consumer_c)
add_executable(tlog_consumer_cpp tlog_consumer.cpp)
target_compile_features(tlog_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(tlog_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.tlog_cpp17 COMMAND tlog_consumer_cpp)

add_executable(uuid_consumer_c uuid_consumer.c)
target_compile_features(uuid_consumer_c PRIVATE c_std_11)
target_link_libraries(uuid_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.original_uuid_c11 COMMAND uuid_consumer_c)
add_executable(uuid_consumer_cpp uuid_consumer.cpp)
target_compile_features(uuid_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(uuid_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.original_uuid_cpp17 COMMAND uuid_consumer_cpp)

add_executable(bytes_accumulator_consumer_c cmeta_bytes_consumer.c)
target_compile_features(bytes_accumulator_consumer_c PRIVATE c_std_11)
target_link_libraries(bytes_accumulator_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.original_bytes_accumulator_c11 COMMAND bytes_accumulator_consumer_c)
add_executable(bytes_accumulator_consumer_cpp cmeta_bytes_consumer.cpp)
target_compile_features(bytes_accumulator_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(bytes_accumulator_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.original_bytes_accumulator_cpp17 COMMAND bytes_accumulator_consumer_cpp)

add_executable(simd_scan_consumer_c simd_scan_consumer.c)
target_compile_features(simd_scan_consumer_c PRIVATE c_std_11)
target_link_libraries(simd_scan_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.original_simd_scan_c11 COMMAND simd_scan_consumer_c)
add_executable(simd_scan_consumer_cpp simd_scan_consumer.cpp)
target_compile_features(simd_scan_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(simd_scan_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.original_simd_scan_cpp17 COMMAND simd_scan_consumer_cpp)


# Independently installed original native mmap API, one Salts::Core shared
# owner. ABI and runtime are exercised by both C11 and C++17 consumers.
add_executable(original_mmap_consumer_c original_mmap_consumer.c)
target_compile_features(original_mmap_consumer_c PRIVATE c_std_11)
target_link_libraries(original_mmap_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.original_native_mmap_c11 COMMAND original_mmap_consumer_c)

add_executable(original_mmap_consumer_cpp original_mmap_consumer.cpp)
target_compile_features(original_mmap_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(original_mmap_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.original_native_mmap_cpp17 COMMAND original_mmap_consumer_cpp)


# Original ten-function bounded regex C11 SDK API, compiled by Salts::Core.
# This imports the installed library; no second regex parser or private source.
add_executable(re_consumer_c re_consumer.c)
target_compile_features(re_consumer_c PRIVATE c_std_11)
target_link_libraries(re_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.original_re_c11 COMMAND re_consumer_c)

add_executable(re_consumer_cpp re_consumer.cpp)
target_compile_features(re_consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(re_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.original_re_cpp17 COMMAND re_consumer_cpp)
