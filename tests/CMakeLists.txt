add_executable(saltscore_bytes_test test_bytes.c)
target_link_libraries(saltscore_bytes_test PRIVATE SaltsCore::Core)
set_target_properties(saltscore_bytes_test PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME saltscore.bytes COMMAND saltscore_bytes_test)
