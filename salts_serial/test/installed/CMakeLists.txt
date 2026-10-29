cmake_minimum_required(VERSION 3.20)
project(SaltsCoreSerialInstalled LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)
if(NOT TARGET Salts::Serial)
  message(FATAL_ERROR "Installed Salts::Serial is required; missing owner cannot skip conformance")
endif()

add_executable(serial_consumer_c serial_consumer.c)
set_target_properties(serial_consumer_c PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(serial_consumer_c PRIVATE Salts::Serial)
add_test(NAME sdk.serial_c11 COMMAND serial_consumer_c)

add_executable(serial_consumer_cpp serial_consumer.cpp)
set_target_properties(serial_consumer_cpp PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(serial_consumer_cpp PRIVATE Salts::Serial)
add_test(NAME sdk.serial_cpp17 COMMAND serial_consumer_cpp)
set_tests_properties(sdk.serial_c11 sdk.serial_cpp17 PROPERTIES TIMEOUT 30)
