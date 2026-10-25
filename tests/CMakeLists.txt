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

# Real pipe I/O conformance on POSIX readiness backends; Windows IOCP needs
# a separately qualified overlapped named-pipe or TCP socket harness.
if(UNIX AND NOT CMAKE_CROSSCOMPILING AND
   (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR APPLE))
  add_executable(saltscore_native_pipe_test test_native_io_posix_pipe.c)
  target_link_libraries(saltscore_native_pipe_test PRIVATE SaltsCore::NativeIO)
  set_target_properties(saltscore_native_pipe_test PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  add_test(NAME saltscore.native_pipe COMMAND saltscore_native_pipe_test)
  set_tests_properties(saltscore.native_pipe PROPERTIES TIMEOUT 12)
endif()

# Re-run original source-repository conformance against standalone SDK modules.
add_subdirectory(conformance)
