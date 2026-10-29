# All three original tests remain enabled against the original production
# source. The fake libusb target is confined to deterministic state tests.
add_executable(saltscore_usb_original_c11 cflow_usb_test.c)
target_compile_features(saltscore_usb_original_c11 PRIVATE c_std_11)
set_target_properties(saltscore_usb_original_c11 PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(saltscore_usb_original_c11 PRIVATE
  Salts::USB Salts::Platform Salts::TinyTest)
add_test(NAME saltscore.usb_original_c11 COMMAND saltscore_usb_original_c11)

add_executable(saltscore_usb_original_cpp17 cflow_usb_header_cpp_test.cpp)
target_compile_features(saltscore_usb_original_cpp17 PRIVATE cxx_std_17)
set_target_properties(saltscore_usb_original_cpp17 PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(saltscore_usb_original_cpp17 PRIVATE
  Salts::USB Salts::TinyTest)
add_test(NAME saltscore.usb_original_cpp17 COMMAND saltscore_usb_original_cpp17)

add_library(saltscore_usb_test_impl STATIC ../src/usb.c fake/fake_libusb.c)
target_compile_features(saltscore_usb_test_impl PRIVATE c_std_11)
set_target_properties(saltscore_usb_test_impl PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(saltscore_usb_test_impl PRIVATE fake ../include)
target_link_libraries(saltscore_usb_test_impl PRIVATE Salts::Platform)

foreach(_usb_suite IN ITEMS cflow_usb_state_test usb_lifecycle_test)
  add_executable(saltscore_${_usb_suite} ${_usb_suite}.c)
  target_compile_features(saltscore_${_usb_suite} PRIVATE c_std_11)
  set_target_properties(saltscore_${_usb_suite} PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_include_directories(saltscore_${_usb_suite} PRIVATE fake ../include)
  target_link_libraries(saltscore_${_usb_suite} PRIVATE
    saltscore_usb_test_impl Salts::Platform Salts::TinyTest)
endforeach()
add_test(NAME saltscore.usb_original_state COMMAND saltscore_cflow_usb_state_test)
add_test(NAME saltscore.usb_lifecycle COMMAND saltscore_usb_lifecycle_test)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME saltscore.usb_source_provenance
  COMMAND "${CMAKE_COMMAND}" -E env "SALTSCORE_USB_TEST_CMAKE=${CMAKE_COMMAND}"
    "${Python3_EXECUTABLE}" -m unittest discover
    -s "${CMAKE_CURRENT_SOURCE_DIR}" -p test_usb_provenance.py -v)
set_tests_properties(saltscore.usb_original_c11 saltscore.usb_original_cpp17
  saltscore.usb_original_state saltscore.usb_lifecycle
  saltscore.usb_source_provenance PROPERTIES TIMEOUT 60)
