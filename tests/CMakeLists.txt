if(UNIX)
  add_executable(saltscore_readiness_race_test test_readiness_race.c)
  target_compile_features(saltscore_readiness_race_test PRIVATE c_std_11)
  target_link_libraries(saltscore_readiness_race_test PRIVATE Salts::Platform)
  set_target_properties(saltscore_readiness_race_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  add_test(NAME saltscore.readiness_close_race COMMAND saltscore_readiness_race_test)
  set_tests_properties(saltscore.readiness_close_race PROPERTIES TIMEOUT 35)
endif()

# Real copied readiness state machine with test-only injection of native
# backend hook failures: register, arm, unarm, close, shutdown, stale events.
add_executable(saltscore_readiness_faults_test test_readiness_faults.c)
target_compile_features(saltscore_readiness_faults_test PRIVATE c_std_11)
target_link_libraries(saltscore_readiness_faults_test PRIVATE Salts::Platform)
set_target_properties(saltscore_readiness_faults_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.readiness_faults COMMAND saltscore_readiness_faults_test)
set_tests_properties(saltscore.readiness_faults PROPERTIES TIMEOUT 20)

add_executable(saltscore_readiness_test test_readiness.c)
target_compile_features(saltscore_readiness_test PRIVATE c_std_11)
target_link_libraries(saltscore_readiness_test PRIVATE Salts::Platform)
set_target_properties(saltscore_readiness_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.readiness_owner COMMAND saltscore_readiness_test)
set_tests_properties(saltscore.readiness_owner PROPERTIES TIMEOUT 15)

add_executable(saltscore_utf8_test test_utf8.c)
target_link_libraries(saltscore_utf8_test PRIVATE Salts::Core)
target_compile_features(saltscore_utf8_test PRIVATE c_std_11)
set_target_properties(saltscore_utf8_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.utf8_strict COMMAND saltscore_utf8_test)

add_executable(saltscore_runtime_abi_test test_runtime_abi.c)
target_compile_features(saltscore_runtime_abi_test PRIVATE c_std_11)
target_link_libraries(saltscore_runtime_abi_test PRIVATE Salts::Core)
set_target_properties(saltscore_runtime_abi_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.runtime_abi_0_1 COMMAND saltscore_runtime_abi_test)

# Plain C11 borrowed string view: no reflection headers, allocator or ABI shim.
add_executable(saltscore_vstr_contract_test test_vstr_contract.c)
target_link_libraries(saltscore_vstr_contract_test PRIVATE Salts::Core)
set_target_properties(saltscore_vstr_contract_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.vstr_c11 COMMAND saltscore_vstr_contract_test)

# Base64 must remain one installed native owner, no unrelated vendor codec.
add_executable(saltscore_base64_test test_base64_utils.c)
target_compile_features(saltscore_base64_test PRIVATE c_std_11)
target_link_libraries(saltscore_base64_test PRIVATE Salts::Core)
set_target_properties(saltscore_base64_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(MSVC)
  target_compile_options(saltscore_base64_test PRIVATE /W4 /WX)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(saltscore_base64_test PRIVATE -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Werror)
endif()
add_test(NAME saltscore.base64_unique_owner COMMAND saltscore_base64_test)

add_executable(saltscore_bytes_test test_bytes.c)
target_link_libraries(saltscore_bytes_test PRIVATE Salts::Core)
set_target_properties(saltscore_bytes_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.bytes COMMAND saltscore_bytes_test)

# Core Platform is the sole owner of native fast-key state; no CMeta ABI.
add_executable(saltscore_fastpath_test test_fastpath.c)
target_compile_features(saltscore_fastpath_test PRIVATE c_std_11)
target_link_libraries(saltscore_fastpath_test PRIVATE Salts::Platform)
set_target_properties(saltscore_fastpath_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.fastpath COMMAND saltscore_fastpath_test)

add_executable(saltscore_platform_test test_platform.c)
target_link_libraries(saltscore_platform_test PRIVATE Salts::Platform)
set_target_properties(saltscore_platform_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.platform COMMAND saltscore_platform_test)

add_executable(saltscore_concurrency_test test_concurrency.c)
target_link_libraries(saltscore_concurrency_test PRIVATE Salts::Concurrency)
set_target_properties(saltscore_concurrency_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.concurrency COMMAND saltscore_concurrency_test)

add_executable(saltscore_coroutine_test test_coroutine.c)
target_link_libraries(saltscore_coroutine_test PRIVATE Salts::Coroutine)
set_target_properties(saltscore_coroutine_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.coroutine COMMAND saltscore_coroutine_test)

add_executable(saltscore_native_io_test test_native_io.c)
target_link_libraries(saltscore_native_io_test PRIVATE Salts::NativeIO)
set_target_properties(saltscore_native_io_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.native_io COMMAND saltscore_native_io_test)

add_executable(saltscore_buffer_test test_buffer.c)
target_link_libraries(saltscore_buffer_test PRIVATE Salts::Core)
set_target_properties(saltscore_buffer_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.buffer COMMAND saltscore_buffer_test)

if(TARGET Salts::URI)
  add_executable(saltscore_uri_test test_uri.c)
  target_link_libraries(saltscore_uri_test PRIVATE Salts::URI)
  set_target_properties(saltscore_uri_test PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  add_test(NAME saltscore.uri COMMAND saltscore_uri_test)
endif()

add_executable(saltscore_ring_test test_ring.c)
target_link_libraries(saltscore_ring_test PRIVATE Salts::Core)
set_target_properties(saltscore_ring_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.ring COMMAND saltscore_ring_test)

# Parser-only C11 gate with no TLS, CNet, re2c, CMeta or CFlow provider.
if(TARGET saltscore_wsparser)
  add_executable(saltscore_wsparser_test test_wsparser.c)
  target_compile_features(saltscore_wsparser_test PRIVATE c_std_11)
  set_target_properties(saltscore_wsparser_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_include_directories(saltscore_wsparser_test PRIVATE
    "${PROJECT_SOURCE_DIR}/cnet/src/wsparser")
  target_link_libraries(saltscore_wsparser_test PRIVATE saltscore_wsparser)
  add_test(NAME saltscore.wsparser COMMAND saltscore_wsparser_test)
endif()

# Compile the actual provider-free CNet policy sources on every platform even
# when CNet/TLS itself is disabled. These are test-only, never SDK exports.
add_executable(saltscore_cnet_policy_test
  test_cnet_policy.c
  ../cnet/src/cnet_destination_policy.c
  ../cnet/src/cnet_owner_strategy.c
  ../cnet/src/cnet_recovery_policy.c)
target_compile_features(saltscore_cnet_policy_test PRIVATE c_std_11)
set_target_properties(saltscore_cnet_policy_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(saltscore_cnet_policy_test PRIVATE
  "${PROJECT_SOURCE_DIR}/cnet/include"
  "${PROJECT_SOURCE_DIR}/platform/include"
  "${PROJECT_SOURCE_DIR}/core/include")
target_link_libraries(saltscore_cnet_policy_test PRIVATE Salts::Platform)
if(MSVC)
  target_compile_options(saltscore_cnet_policy_test PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
endif()
add_test(NAME saltscore.cnet_policy COMMAND saltscore_cnet_policy_test)

# Run real pinned-provider ABI and ShangMi KATs only when the optional
# transport provider graph was actually configured. No fake TLS fallback.
# The real CNet target lives in the sibling cnet/ subdirectory.
# Imported provider targets are directory-scoped. Tests that directly link
# the original GmSSL API explicitly discover the same pinned installed
# dependency in their own CMake scope; no second source/backend is built.
if(TARGET Salts::CNet)
  find_package(GmSSL CONFIG REQUIRED)
endif()

if(TARGET Salts::CNet)
  # A separate loadable module checks process-global pool and thread identity
  # across DSO/DLL boundaries. Never resolve these through a static archive.
  add_library(saltscore_runtime_owner_probe SHARED runtime_owner_module.c)
  target_compile_features(saltscore_runtime_owner_probe PRIVATE c_std_11)
  set_target_properties(saltscore_runtime_owner_probe PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF WINDOWS_EXPORT_ALL_SYMBOLS ON)
  target_link_libraries(saltscore_runtime_owner_probe PRIVATE
    Salts::Core Salts::Platform)
  # Genuine loopback TCP acceptance + borrowed/retained ownership + stop drain.
  add_executable(saltscore_cnet_tcp_loopback_test test_cnet_tcp_loopback.c)
  target_compile_features(saltscore_cnet_tcp_loopback_test PRIVATE c_std_11)
  set_target_properties(saltscore_cnet_tcp_loopback_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_cnet_tcp_loopback_test PRIVATE
    Salts::CNet saltscore_runtime_owner_probe)
  if(WIN32)
    target_link_libraries(saltscore_cnet_tcp_loopback_test PRIVATE ws2_32)
  endif()
  if(MSVC)
    target_compile_options(saltscore_cnet_tcp_loopback_test PRIVATE
      $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
  endif()
  add_test(NAME saltscore.cnet_tcp_loopback
    COMMAND saltscore_cnet_tcp_loopback_test)
  set_tests_properties(saltscore.cnet_tcp_loopback PROPERTIES TIMEOUT 20)

  # Load two independent CNet consumers through the native dynamic loader.
  # No CNet library is linked into the executable itself.
  foreach(_plugin IN ITEMS a b)
    add_library(saltscore_runtime_plugin_${_plugin} MODULE runtime_loaded_plugin.c)
    target_compile_features(saltscore_runtime_plugin_${_plugin} PRIVATE c_std_11)
    set_target_properties(saltscore_runtime_plugin_${_plugin} PROPERTIES
      C_STANDARD_REQUIRED ON C_EXTENSIONS OFF WINDOWS_EXPORT_ALL_SYMBOLS ON)
    target_link_libraries(saltscore_runtime_plugin_${_plugin} PRIVATE
      Salts::CNet)
  endforeach()
  add_executable(saltscore_two_plugins_test test_runtime_plugins.c)
  target_compile_features(saltscore_two_plugins_test PRIVATE c_std_11)
  set_target_properties(saltscore_two_plugins_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_two_plugins_test PRIVATE
    Salts::Core Salts::Platform ${CMAKE_DL_LIBS})
  add_dependencies(saltscore_two_plugins_test
    saltscore_runtime_plugin_a saltscore_runtime_plugin_b)
  add_test(NAME saltscore.two_cnet_plugins
    COMMAND saltscore_two_plugins_test
      "$<TARGET_FILE:saltscore_runtime_plugin_a>"
      "$<TARGET_FILE:saltscore_runtime_plugin_b>")
  set_tests_properties(saltscore.two_cnet_plugins PROPERTIES TIMEOUT 30)
endif()

if(TARGET Salts::CNet)
  add_executable(saltscore_gmssl_tls_io_abi_test test_gmssl_tls_io_abi.c)
  target_compile_features(saltscore_gmssl_tls_io_abi_test PRIVATE c_std_11)
  set_target_properties(saltscore_gmssl_tls_io_abi_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_gmssl_tls_io_abi_test PRIVATE GmSSL::GmSSL)
  add_test(NAME saltscore.gmssl_tls_io_abi COMMAND saltscore_gmssl_tls_io_abi_test)

  add_executable(saltscore_gmssl_shangmi_provider_test
    test_gmssl_shangmi_provider.c)
  target_compile_features(saltscore_gmssl_shangmi_provider_test PRIVATE c_std_11)
  set_target_properties(saltscore_gmssl_shangmi_provider_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_gmssl_shangmi_provider_test PRIVATE GmSSL::GmSSL)
  add_test(NAME saltscore.gmssl_shangmi_provider
    COMMAND saltscore_gmssl_shangmi_provider_test)
endif()

if(TARGET Salts::CNet)
  # Real TLS over NativeIO TCP: trusted issuer + correct SAN must connect;
  # wrong root and wrong DNS identity must fail without authenticated callback.
  add_executable(saltscore_cnet_tls_ca_san_test test_cnet_tls_ca_san.c)
  target_compile_features(saltscore_cnet_tls_ca_san_test PRIVATE c_std_11)
  set_target_properties(saltscore_cnet_tls_ca_san_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_cnet_tls_ca_san_test PRIVATE Salts::CNet)
  if(MSVC)
    target_compile_options(saltscore_cnet_tls_ca_san_test PRIVATE
      $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
  endif()
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/p384-test-root-cert.pem"
       _cnet_test_root_ca)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/p384-test-wrong-root-cert.pem"
       _cnet_test_wrong_ca)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/p384-test-server-cert.pem"
       _cnet_test_cert)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/p384-test-server-key.pem"
       _cnet_test_key)
  target_compile_definitions(saltscore_cnet_tls_ca_san_test PRIVATE
    "CORE_TLS_TEST_P384_CA=\"${_cnet_test_root_ca}\""
    "CORE_TLS_TEST_P384_WRONG_CA=\"${_cnet_test_wrong_ca}\""
    "CORE_TLS_TEST_P384_LEAF=\"${_cnet_test_cert}\""
    "CORE_TLS_TEST_P384_KEY=\"${_cnet_test_key}\"")
  add_test(NAME saltscore.cnet_tls_ca_san
    COMMAND saltscore_cnet_tls_ca_san_test)
  set_tests_properties(saltscore.cnet_tls_ca_san PROPERTIES TIMEOUT 65)
endif()

# Cross-compiler-compatible Atomic/Thread runtime; no consumer-native _Atomic.
add_executable(saltscore_portable_concurrency_test test_portable_concurrency.c)
target_compile_features(saltscore_portable_concurrency_test PRIVATE c_std_11)
set_target_properties(saltscore_portable_concurrency_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(saltscore_portable_concurrency_test PRIVATE Salts::Platform)
add_test(NAME saltscore.portable_concurrency COMMAND saltscore_portable_concurrency_test)
set_tests_properties(saltscore.portable_concurrency PROPERTIES TIMEOUT 45)

# M1: relative monotonic timed wait / sleep, same C ABI across compilers.
add_executable(saltscore_portable_timing_test test_portable_timing.c)
target_compile_features(saltscore_portable_timing_test PRIVATE c_std_11)
target_link_libraries(saltscore_portable_timing_test PRIVATE Salts::Platform)
add_test(NAME saltscore.portable_timing COMMAND saltscore_portable_timing_test)
set_tests_properties(saltscore.portable_timing PROPERTIES TIMEOUT 30)

# M1: provider-owned thread-specific keys and destructor lifetime barriers.
add_executable(saltscore_portable_tss_test test_portable_tss.c)
target_compile_features(saltscore_portable_tss_test PRIVATE c_std_11)
target_link_libraries(saltscore_portable_tss_test PRIVATE Salts::Platform)
add_test(NAME saltscore.portable_tss COMMAND saltscore_portable_tss_test)
set_tests_properties(saltscore.portable_tss PROPERTIES TIMEOUT 30)


# Monotonic relative timed mutex (MSVC / Clang / TinyCC C ABI).
add_executable(saltscore_portable_timed_mutex_test test_portable_timed_mutex.c)
target_compile_features(saltscore_portable_timed_mutex_test PRIVATE c_std_11)
target_link_libraries(saltscore_portable_timed_mutex_test PRIVATE Salts::Platform)
add_test(NAME saltscore.portable_timed_mutex COMMAND saltscore_portable_timed_mutex_test)
set_tests_properties(saltscore.portable_timed_mutex PROPERTIES TIMEOUT 35)


# Explicit C11 threads.h source-compatibility admission (not a global CRT override).
add_executable(saltscore_c11_threads_test test_c11_threads.c)
target_include_directories(saltscore_c11_threads_test BEFORE PRIVATE
  "${PROJECT_SOURCE_DIR}/platform/include/salts_core/c11")
target_compile_definitions(saltscore_c11_threads_test PRIVATE
  SALTS_C11_THREADS_EXPERIMENTAL=1)
set_target_properties(saltscore_c11_threads_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(saltscore_c11_threads_test PRIVATE Salts::Platform)
add_test(NAME saltscore.c11_threads_facade COMMAND saltscore_c11_threads_test)
set_tests_properties(saltscore.c11_threads_facade PROPERTIES TIMEOUT 35)


# Regression for Windows cmeta_hrtime once-atomic QPC frequency publication.
add_executable(saltscore_clock_atomic_owner_test test_clock_atomic_owner.c)
target_compile_features(saltscore_clock_atomic_owner_test PRIVATE c_std_11)
target_link_libraries(saltscore_clock_atomic_owner_test PRIVATE Salts::Platform)
add_test(NAME saltscore.clock_atomic_owner COMMAND saltscore_clock_atomic_owner_test)
set_tests_properties(saltscore.clock_atomic_owner PROPERTIES TIMEOUT 30)

# Test-only native Crypto Owner, never part of the production CNet target.
if(SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST)
  add_subdirectory(cnet-crypto-static)
endif()
