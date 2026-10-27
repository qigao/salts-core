cmake_minimum_required(VERSION 3.20)
project(SaltsCoreInstalledConsumer LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)

# find_package name remains SaltsCore; its ONLY imported link namespace is Salts::.
foreach(_owner IN ITEMS Core Platform Concurrency Coroutine NativeIO)
  if(NOT TARGET "Salts::${_owner}")
    message(FATAL_ERROR "Required canonical Salts::${_owner} target is absent")
  endif()
  get_target_property(_imported "Salts::${_owner}" IMPORTED)
  if(NOT _imported)
    message(FATAL_ERROR "Salts::${_owner} must be imported from installed SaltsCore")
  endif()
endforeach()

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

if(TARGET Salts::URI)
  add_executable(uri_consumer_c uri_consumer.c)
  target_compile_features(uri_consumer_c PRIVATE c_std_11)
  target_link_libraries(uri_consumer_c PRIVATE Salts::URI)
  add_test(NAME sdk.uri_consumer_c COMMAND uri_consumer_c)

  add_executable(uri_consumer_cpp uri_consumer.cpp)
  target_compile_features(uri_consumer_cpp PRIVATE cxx_std_17)
  target_link_libraries(uri_consumer_cpp PRIVATE Salts::URI)
  add_test(NAME sdk.uri_consumer_cpp COMMAND uri_consumer_cpp)
endif()

# Only present when an exact CNet provider SDK was actually configured and
# installed. The baseline Core installation must work with CNet disabled.
if(TARGET Salts::CNet)
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
endif()

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
