if(UNIX)
  add_executable(saltscore_readiness_race_test test_readiness_race.c)
  target_compile_features(saltscore_readiness_race_test PRIVATE c_std_11)
  target_link_libraries(saltscore_readiness_race_test PRIVATE SaltsCore::Platform)
  set_target_properties(saltscore_readiness_race_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  add_test(NAME saltscore.readiness_close_race COMMAND saltscore_readiness_race_test)
  set_tests_properties(saltscore.readiness_close_race PROPERTIES TIMEOUT 35)
endif()

# Real copied readiness state machine with test-only injection of native
# backend hook failures: register, arm, unarm, close, shutdown, stale events.
add_executable(saltscore_readiness_faults_test test_readiness_faults.c)
target_compile_features(saltscore_readiness_faults_test PRIVATE c_std_11)
target_link_libraries(saltscore_readiness_faults_test PRIVATE SaltsCore::Platform)
set_target_properties(saltscore_readiness_faults_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.readiness_faults COMMAND saltscore_readiness_faults_test)
set_tests_properties(saltscore.readiness_faults PROPERTIES TIMEOUT 20)

add_executable(saltscore_readiness_test test_readiness.c)
target_compile_features(saltscore_readiness_test PRIVATE c_std_11)
target_link_libraries(saltscore_readiness_test PRIVATE SaltsCore::Platform)
set_target_properties(saltscore_readiness_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.readiness_owner COMMAND saltscore_readiness_test)
set_tests_properties(saltscore.readiness_owner PROPERTIES TIMEOUT 15)

add_executable(saltscore_utf8_test test_utf8.c)
target_link_libraries(saltscore_utf8_test PRIVATE SaltsCore::Core)
target_compile_features(saltscore_utf8_test PRIVATE c_std_11)
set_target_properties(saltscore_utf8_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.utf8_strict COMMAND saltscore_utf8_test)

add_executable(saltscore_runtime_abi_test test_runtime_abi.c)
target_compile_features(saltscore_runtime_abi_test PRIVATE c_std_11)
target_link_libraries(saltscore_runtime_abi_test PRIVATE SaltsCore::Core)
set_target_properties(saltscore_runtime_abi_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.runtime_abi_0_1 COMMAND saltscore_runtime_abi_test)

add_executable(saltscore_bytes_test test_bytes.c)
target_link_libraries(saltscore_bytes_test PRIVATE SaltsCore::Core)
set_target_properties(saltscore_bytes_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.bytes COMMAND saltscore_bytes_test)

add_executable(saltscore_platform_test test_platform.c)
target_link_libraries(saltscore_platform_test PRIVATE SaltsCore::Platform)
set_target_properties(saltscore_platform_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.platform COMMAND saltscore_platform_test)

add_executable(saltscore_concurrency_test test_concurrency.c)
target_link_libraries(saltscore_concurrency_test PRIVATE SaltsCore::Concurrency)
set_target_properties(saltscore_concurrency_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.concurrency COMMAND saltscore_concurrency_test)

add_executable(saltscore_coroutine_test test_coroutine.c)
target_link_libraries(saltscore_coroutine_test PRIVATE SaltsCore::Coroutine)
set_target_properties(saltscore_coroutine_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.coroutine COMMAND saltscore_coroutine_test)

add_executable(saltscore_native_io_test test_native_io.c)
target_link_libraries(saltscore_native_io_test PRIVATE SaltsCore::NativeIO)
set_target_properties(saltscore_native_io_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.native_io COMMAND saltscore_native_io_test)

add_executable(saltscore_buffer_test test_buffer.c)
target_link_libraries(saltscore_buffer_test PRIVATE SaltsCore::Core)
set_target_properties(saltscore_buffer_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.buffer COMMAND saltscore_buffer_test)

if(TARGET SaltsCore::URI)
  add_executable(saltscore_uri_test test_uri.c)
  target_link_libraries(saltscore_uri_test PRIVATE SaltsCore::URI)
  set_target_properties(saltscore_uri_test PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  add_test(NAME saltscore.uri COMMAND saltscore_uri_test)
endif()

add_executable(saltscore_ring_test test_ring.c)
target_link_libraries(saltscore_ring_test PRIVATE SaltsCore::Core)
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
    "${PROJECT_SOURCE_DIR}/src/cnet/wsparser")
  target_link_libraries(saltscore_wsparser_test PRIVATE saltscore_wsparser)
  add_test(NAME saltscore.wsparser COMMAND saltscore_wsparser_test)
endif()

# Compile the actual provider-free CNet policy sources on every platform even
# when CNet/TLS itself is disabled. These are test-only, never SDK exports.
add_executable(saltscore_cnet_policy_test
  test_cnet_policy.c
  ../src/cnet/cnet_destination_policy.c
  ../src/cnet/cnet_owner_strategy.c
  ../src/cnet/cnet_recovery_policy.c)
target_compile_features(saltscore_cnet_policy_test PRIVATE c_std_11)
set_target_properties(saltscore_cnet_policy_test PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(saltscore_cnet_policy_test PRIVATE
  "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(saltscore_cnet_policy_test PRIVATE SaltsCore::Platform)
if(MSVC)
  target_compile_options(saltscore_cnet_policy_test PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
endif()
add_test(NAME saltscore.cnet_policy COMMAND saltscore_cnet_policy_test)

# Run real pinned-provider ABI and ShangMi KATs only when the optional
# transport provider graph was actually configured. No fake TLS fallback.
if(TARGET SaltsCore::CNet)
  # A separate loadable module checks process-global pool and thread identity
  # across DSO/DLL boundaries. Never resolve these through a static archive.
  add_library(saltscore_runtime_owner_probe SHARED runtime_owner_module.c)
  target_compile_features(saltscore_runtime_owner_probe PRIVATE c_std_11)
  set_target_properties(saltscore_runtime_owner_probe PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF WINDOWS_EXPORT_ALL_SYMBOLS ON)
  target_link_libraries(saltscore_runtime_owner_probe PRIVATE
    SaltsCore::Core SaltsCore::Platform)
  # Genuine loopback TCP acceptance + borrowed/retained ownership + stop drain.
  add_executable(saltscore_cnet_tcp_loopback_test test_cnet_tcp_loopback.c)
  target_compile_features(saltscore_cnet_tcp_loopback_test PRIVATE c_std_11)
  set_target_properties(saltscore_cnet_tcp_loopback_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_cnet_tcp_loopback_test PRIVATE
    SaltsCore::CNet saltscore_runtime_owner_probe)
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
      SaltsCore::CNet)
  endforeach()
  add_executable(saltscore_two_plugins_test test_runtime_plugins.c)
  target_compile_features(saltscore_two_plugins_test PRIVATE c_std_11)
  set_target_properties(saltscore_two_plugins_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_two_plugins_test PRIVATE
    SaltsCore::Core SaltsCore::Platform ${CMAKE_DL_LIBS})
  add_dependencies(saltscore_two_plugins_test
    saltscore_runtime_plugin_a saltscore_runtime_plugin_b)
  add_test(NAME saltscore.two_cnet_plugins
    COMMAND saltscore_two_plugins_test
      "$<TARGET_FILE:saltscore_runtime_plugin_a>"
      "$<TARGET_FILE:saltscore_runtime_plugin_b>")
  set_tests_properties(saltscore.two_cnet_plugins PROPERTIES TIMEOUT 30)
endif()

if(TARGET SaltsCore::CNet)
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

if(TARGET SaltsCore::CNet)
  # Real TLS over NativeIO TCP: trusted issuer + correct SAN must connect;
  # wrong root and wrong DNS identity must fail without authenticated callback.
  add_executable(saltscore_cnet_tls_ca_san_test test_cnet_tls_ca_san.c)
  target_compile_features(saltscore_cnet_tls_ca_san_test PRIVATE c_std_11)
  set_target_properties(saltscore_cnet_tls_ca_san_test PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_link_libraries(saltscore_cnet_tls_ca_san_test PRIVATE SaltsCore::CNet)
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
