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

# Preserve the original error registry and borrowed diagnostic lifetimes.
# The native contract includes INT_MIN decoding and concurrent registration.
add_executable(saltscore_error_contract error_native_contract.c)
target_compile_features(saltscore_error_contract PRIVATE c_std_11)
target_link_libraries(saltscore_error_contract PRIVATE Salts::Core)
set_target_properties(saltscore_error_contract PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.error_native_contract COMMAND saltscore_error_contract)
set_tests_properties(saltscore.error_native_contract PROPERTIES TIMEOUT 30)
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_error_test test_cmeta_error.c)
  target_compile_features(saltscore_original_error_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_error_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_error COMMAND saltscore_original_error_test)
endif()

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

# Restore original tstr/vstr/memory-pool behavior, not just public headers.
add_executable(saltscore_original_string_parity_test test_original_string_parity.c)
target_compile_features(saltscore_original_string_parity_test PRIVATE c_std_11)
set_target_properties(saltscore_original_string_parity_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(saltscore_original_string_parity_test PRIVATE Salts::Core)
add_test(NAME saltscore.original_utils_string_parity
  COMMAND saltscore_original_string_parity_test)
set_tests_properties(saltscore.original_utils_string_parity PROPERTIES TIMEOUT 20)

# Literal copy of the original TinyTest tstr v2 contract from Salts utils.
# Optional TinyTest profile runs source-parity expectations; never a stub.
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_tstr_v2_contract test_tstr_v2_contract.c)
  target_compile_features(saltscore_original_tstr_v2_contract PRIVATE c_std_11)
  target_link_libraries(saltscore_original_tstr_v2_contract PRIVATE
    Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_tstr_v2_contract
    COMMAND saltscore_original_tstr_v2_contract)
endif()

# Original object_pool source and TinyTest contract, not a new allocator
# provider. The C11 smoke test also runs when TinyTest is disabled.
add_executable(saltscore_original_object_pool_smoke test_original_object_pool_smoke.c)
target_compile_features(saltscore_original_object_pool_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_original_object_pool_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_object_pool_smoke
  COMMAND saltscore_original_object_pool_smoke)
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_object_pool_contract test_object_pool.c)
  target_compile_features(saltscore_original_object_pool_contract PRIVATE c_std_11)
  target_link_libraries(saltscore_original_object_pool_contract PRIVATE
    Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_object_pool_contract
    COMMAND saltscore_original_object_pool_contract)
  set_tests_properties(saltscore.original_object_pool_contract PROPERTIES TIMEOUT 45)
endif()

# The original single-owner priority queue: FIFO within a priority and
# critical/high/normal/low ordering, with strict bounded storage.
add_executable(saltscore_original_priority_queue_smoke test_original_priority_queue_smoke.c)
target_compile_features(saltscore_original_priority_queue_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_original_priority_queue_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_priority_queue_smoke COMMAND saltscore_original_priority_queue_smoke)
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_priority_queue_contract test_bucket_priority_queue.c)
  target_compile_features(saltscore_original_priority_queue_contract PRIVATE c_std_11)
  target_link_libraries(saltscore_original_priority_queue_contract PRIVATE
    Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_priority_queue_contract
    COMMAND saltscore_original_priority_queue_contract)
endif()

# Original native SPSC ring and bounded multi-priority SPSC queue. Their
# behavior is exercised by the original TinyTest suites, not a mock backend.
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_ring_spsc_test test_ring_buffer_spsc.c)
  target_compile_features(saltscore_original_ring_spsc_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_ring_spsc_test PRIVATE Salts::Concurrency Salts::TinyTest)
  add_test(NAME saltscore.original_ring_spsc COMMAND saltscore_original_ring_spsc_test)

  add_executable(saltscore_original_bucket_spsc_test test_bucket_priority_queue_spsc.c)
  target_compile_features(saltscore_original_bucket_spsc_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_bucket_spsc_test PRIVATE
    Salts::Concurrency Salts::TinyTest)
  add_test(NAME saltscore.original_bucket_priority_spsc
    COMMAND saltscore_original_bucket_spsc_test)
  set_tests_properties(saltscore.original_bucket_priority_spsc PROPERTIES TIMEOUT 50)
endif()

add_executable(saltscore_original_spsc_smoke test_original_spsc_smoke.c)
target_compile_features(saltscore_original_spsc_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_original_spsc_smoke PRIVATE Salts::Concurrency)
add_test(NAME saltscore.original_spsc_smoke COMMAND saltscore_original_spsc_smoke)

# The original seven-function MPMC API is owned by the existing concurrency
# module. Its original test and independent delivery oracle exercise that DSO.
add_executable(saltscore_mpmc_contract installed/mpmc_consumer.c)
target_compile_features(saltscore_mpmc_contract PRIVATE c_std_11)
target_link_libraries(saltscore_mpmc_contract PRIVATE Salts::Concurrency)
add_test(NAME saltscore.mpmc_delivery_contract COMMAND saltscore_mpmc_contract)
set_tests_properties(saltscore.mpmc_delivery_contract PROPERTIES TIMEOUT 30)
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_bucket_mpmc_test test_bucket_priority_queue_mpmc.c)
  target_compile_features(saltscore_original_bucket_mpmc_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_bucket_mpmc_test PRIVATE
    Salts::Concurrency Salts::TinyTest)
  add_test(NAME saltscore.original_bucket_mpmc COMMAND saltscore_original_bucket_mpmc_test)
  set_tests_properties(saltscore.original_bucket_mpmc PROPERTIES TIMEOUT 30)
endif()

# Original 8-case formatter lexer TinyTest suite plus dependency-free C11 smoke.
add_executable(saltscore_fmt_lexer_smoke test_fmt_lexer_smoke.c)
target_compile_features(saltscore_fmt_lexer_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_fmt_lexer_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_fmt_lexer_smoke COMMAND saltscore_fmt_lexer_smoke)
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_fmt_lexer_test test_fmt_re2c.c)
  target_compile_features(saltscore_original_fmt_lexer_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_fmt_lexer_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_fmt_lexer_contract COMMAND saltscore_original_fmt_lexer_test)
endif()

# Original formatter implementation: typed C11 and C++17 metadata, bounded
# rendering, invalid specifier rejection, time formatting, dynamic tstr and views.
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_fmt_test test_fmt.c)
  target_compile_features(saltscore_original_fmt_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_fmt_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_fmt_c11 COMMAND saltscore_original_fmt_test)

  add_executable(saltscore_original_fmt_cpp_test test_fmt_cpp.cpp)
  target_compile_features(saltscore_original_fmt_cpp_test PRIVATE cxx_std_17)
  target_link_libraries(saltscore_original_fmt_cpp_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_fmt_cpp17 COMMAND saltscore_original_fmt_cpp_test)

  add_executable(saltscore_original_fmt_view_test test_fmt_vstr_pattern.c)
  target_compile_features(saltscore_original_fmt_view_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_fmt_view_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_fmt_bounded_vstr COMMAND saltscore_original_fmt_view_test)
endif()
# Always-on no-framework formatter contract (also when TinyTest is OFF).
add_executable(saltscore_fmt_smoke test_fmt_smoke.c)
target_compile_features(saltscore_fmt_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_fmt_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_fmt_smoke COMMAND saltscore_fmt_smoke)

# Original log pattern lexer tokens, C11 parser/format ownership.
add_executable(saltscore_log_pattern_lexer_test test_log_pattern_lexer.c)
target_compile_features(saltscore_log_pattern_lexer_test PRIVATE c_std_11)
target_link_libraries(saltscore_log_pattern_lexer_test PRIVATE Salts::Core)
add_test(NAME saltscore.original_log_pattern_lexer
  COMMAND saltscore_log_pattern_lexer_test)

# Original async logger: the original TinyTest and native callback exercise
# real Disruptor/FS providers, thread drain, shutdown, not a logger mock.
add_executable(saltscore_tlog_smoke test_tlog_smoke.c)
target_compile_features(saltscore_tlog_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_tlog_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_tlog_smoke COMMAND saltscore_tlog_smoke)
set_tests_properties(saltscore.original_tlog_smoke PROPERTIES TIMEOUT 40)
if(TARGET Salts::TinyTest)
  foreach(_tlog_suite IN ITEMS tlog tlog_vstr_abi tlog_header_collision)
    add_executable(saltscore_original_${_tlog_suite}_test test_${_tlog_suite}.c)
    target_compile_features(saltscore_original_${_tlog_suite}_test PRIVATE c_std_11)
    target_link_libraries(saltscore_original_${_tlog_suite}_test PRIVATE
      Salts::Core Salts::TinyTest)
    add_test(NAME saltscore.original_${_tlog_suite}
      COMMAND saltscore_original_${_tlog_suite}_test)
    set_tests_properties(saltscore.original_${_tlog_suite} PROPERTIES TIMEOUT 120)
  endforeach()
  add_executable(saltscore_original_tlog_cpp_test test_tlog_cpp.cpp)
  target_compile_features(saltscore_original_tlog_cpp_test PRIVATE cxx_std_17)
  target_link_libraries(saltscore_original_tlog_cpp_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_tlog_cpp COMMAND saltscore_original_tlog_cpp_test)
  set_tests_properties(saltscore.original_tlog_cpp PROPERTIES TIMEOUT 120)
endif()

# Original RFC 4122/9562 UUID v4/v7 and canonical parse/format.
add_executable(saltscore_uuid_smoke test_uuid_smoke.c)
target_compile_features(saltscore_uuid_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_uuid_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_uuid_smoke COMMAND saltscore_uuid_smoke)
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_uuid_test test_cmeta_uuid.c)
  target_compile_features(saltscore_original_uuid_test PRIVATE c_std_11)
  target_link_libraries(saltscore_original_uuid_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_uuid_c11 COMMAND saltscore_original_uuid_test)
  add_executable(saltscore_original_uuid_cpp_test test_cmeta_uuid_cpp.cpp)
  target_compile_features(saltscore_original_uuid_cpp_test PRIVATE cxx_std_17)
  target_link_libraries(saltscore_original_uuid_cpp_test PRIVATE Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_uuid_cpp17 COMMAND saltscore_original_uuid_cpp_test)
endif()

# Restore the original single-owner, hard-capped byte accumulator. This is not
# the borrowed-only salts_core_bytes_view/slice contract.
add_executable(saltscore_cmeta_bytes_smoke test_cmeta_bytes_smoke.c)
target_compile_features(saltscore_cmeta_bytes_smoke PRIVATE c_std_11)
target_link_libraries(saltscore_cmeta_bytes_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_bytes_accumulator_smoke COMMAND saltscore_cmeta_bytes_smoke)
if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_bytes_accumulator test_cmeta_bytes.c)
  target_compile_features(saltscore_original_bytes_accumulator PRIVATE c_std_11)
  target_link_libraries(saltscore_original_bytes_accumulator PRIVATE
    Salts::Core Salts::TinyTest)
  add_test(NAME saltscore.original_bytes_accumulator_contract
    COMMAND saltscore_original_bytes_accumulator)
endif()

# Original seven-function bounded byte scan contract. This is the canonical
# portable C11 implementation; SIMD throughput parity is tracked separately.
add_executable(saltscore_simd_scan_contract test_simd_scan_contract.c)
target_compile_features(saltscore_simd_scan_contract PRIVATE c_std_11)
target_link_libraries(saltscore_simd_scan_contract PRIVATE Salts::Core)
add_test(NAME saltscore.original_simd_scan_contract COMMAND saltscore_simd_scan_contract)


# Actual original qigao/salts utils native memory mapping, not a mock
# and not a CMeta/CFlow runtime. Real file page / range / write / group.
add_executable(saltscore_original_native_mmap_test test_original_native_mmap.c)
target_compile_features(saltscore_original_native_mmap_test PRIVATE c_std_11)
set_target_properties(saltscore_original_native_mmap_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(saltscore_original_native_mmap_test PRIVATE Salts::Core)
add_test(NAME saltscore.original_native_mmap
  COMMAND saltscore_original_native_mmap_test)
set_tests_properties(saltscore.original_native_mmap PROPERTIES TIMEOUT 30)

# The complete 1554+1421 Git Blob provenance contract is a non-waivable
# structural gate: unresolved original native paths remain explicit debt.
# Do NOT run --require-complete here and fake success: the independent
# final admission remains RED until every function/test is qualified.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME saltscore.all_original_source_provenance
  COMMAND "${Python3_EXECUTABLE}"
          "${PROJECT_SOURCE_DIR}/tools/check_full_source_migration.py")
set_tests_properties(saltscore.all_original_source_provenance
  PROPERTIES TIMEOUT 45)
add_test(NAME saltscore.all_original_source_negative
  COMMAND "${Python3_EXECUTABLE}" -m unittest discover
          -s "${PROJECT_SOURCE_DIR}/tests"
          -p "test_full_source_migration.py" -v)
set_tests_properties(saltscore.all_original_source_negative
  PROPERTIES TIMEOUT 45)


# qigao/salts original utils native byte regex, 10 owned public C11 functions.
# Always-on compiled source/runtime; TinyTest retains all 25 donor test cases.
add_executable(saltscore_original_re_smoke test_re_smoke.c)
target_compile_features(saltscore_original_re_smoke PRIVATE c_std_11)
set_target_properties(saltscore_original_re_smoke PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(saltscore_original_re_smoke PRIVATE Salts::Core)
add_test(NAME saltscore.original_re_smoke COMMAND saltscore_original_re_smoke)
set_tests_properties(saltscore.original_re_smoke PROPERTIES TIMEOUT 25)

if(TARGET Salts::TinyTest)
  add_executable(saltscore_original_re_contract test_re.c)
  target_compile_features(saltscore_original_re_contract PRIVATE c_std_11)
  target_link_libraries(saltscore_original_re_contract PRIVATE
    Salts::Core Salts::TinyTest)
  # The first-byte scan is PRIVATE. Only the original tests may inspect it.
  target_include_directories(saltscore_original_re_contract PRIVATE
    "${PROJECT_SOURCE_DIR}/core/src")
  add_test(NAME saltscore.original_re_25_case_contract
    COMMAND saltscore_original_re_contract)
  set_tests_properties(saltscore.original_re_25_case_contract
    PROPERTIES TIMEOUT 60)
endif()


# Fail closed if any original 25 regex tests, ten public functions,
# private SSE2/NEON scanner or installed C11/C++17 acceptance vanishes.
add_test(NAME saltscore.original_re_source_provenance
  COMMAND "${Python3_EXECUTABLE}" -m unittest discover
          -s "${PROJECT_SOURCE_DIR}/tests"
          -p "test_original_re_provenance.py" -v)
set_tests_properties(saltscore.original_re_source_provenance
  PROPERTIES TIMEOUT 45)

# The frozen 610-function inventory did not parse the original SIMD header's
# unannotated C declarations. Keep its separate 32-function supplement intact.
add_test(NAME saltscore.original_simd_source_provenance
  COMMAND "${Python3_EXECUTABLE}" -m unittest discover
          -s "${PROJECT_SOURCE_DIR}/tests"
          -p "test_original_simd_provenance.py" -v)
set_tests_properties(saltscore.original_simd_source_provenance
  PROPERTIES TIMEOUT 45)

add_test(NAME saltscore.original_error_source_provenance
  COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
          -s "${PROJECT_SOURCE_DIR}/tests"
          -p "test_original_error_provenance.py" -v)
set_tests_properties(saltscore.original_error_source_provenance PROPERTIES TIMEOUT 45)

add_test(NAME saltscore.migration_evidence_negative
  COMMAND "${CMAKE_COMMAND}" -E env
          "SALTSCORE_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
          "SALTSCORE_CMAKE_COMMAND=${CMAKE_COMMAND}"
          "${Python3_EXECUTABLE}" -B -m unittest discover
          -s "${PROJECT_SOURCE_DIR}/tests" -p "test_migration_evidence.py" -v)
set_tests_properties(saltscore.migration_evidence_negative PROPERTIES TIMEOUT 60)
