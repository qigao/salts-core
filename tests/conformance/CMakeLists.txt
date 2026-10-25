# Test-only pinned TinyTest C11 runtime from qigao/salts.
# It is never installed or exported and adds no runtime Core dependency.
add_library(saltscore_test_tinytest STATIC
  vendor/tinytest/tinytest.c)
target_include_directories(saltscore_test_tinytest PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinytest)
target_compile_features(saltscore_test_tinytest PUBLIC c_std_11)
set_target_properties(saltscore_test_tinytest PROPERTIES
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(MSVC)
  target_compile_options(saltscore_test_tinytest PRIVATE /W0)
else()
  target_compile_options(saltscore_test_tinytest PRIVATE -w)
endif()

function(saltscore_add_original_conformance name module)
  add_executable(saltscore_conformance_${name} ${name}.c)
  target_link_libraries(saltscore_conformance_${name}
    PRIVATE saltscore_test_tinytest SaltsCore::${module})
  target_include_directories(saltscore_conformance_${name} PRIVATE
    ${PROJECT_SOURCE_DIR}/src/coroutine
    ${PROJECT_SOURCE_DIR}/src/concurrency)
  set_target_properties(saltscore_conformance_${name} PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  add_test(NAME saltscore.conformance.${name}
    COMMAND saltscore_conformance_${name})
  set_tests_properties(saltscore.conformance.${name}
    PROPERTIES TIMEOUT 90)
endfunction()

saltscore_add_original_conformance(deadline_queue_test Concurrency)
saltscore_add_original_conformance(rcu_test Concurrency)
saltscore_add_original_conformance(thread_pool_test Concurrency)
saltscore_add_original_conformance(thread_pool_lf_test Concurrency)
saltscore_add_original_conformance(disruptor_test Concurrency)
saltscore_add_original_conformance(coro_executor_test Coroutine)
saltscore_add_original_conformance(coroutine_completion_test Coroutine)
saltscore_add_original_conformance(native_io_sharded_host_lease_test NativeIO)
saltscore_add_original_conformance(native_ipc_test NativeIO)
