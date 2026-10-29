cmake_minimum_required(VERSION 3.20)
project(SaltsCoreCaptureInstalled LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)
if(NOT TARGET Salts::Capture)
  message(FATAL_ERROR "The installed SDK must contain the real Salts::Capture owner")
endif()
get_target_property(_capture_imported Salts::Capture IMPORTED)
get_target_property(_capture_type Salts::Capture TYPE)
if(NOT _capture_imported OR NOT _capture_type STREQUAL "SHARED_LIBRARY")
  message(FATAL_ERROR "Capture must be one imported shared native owner")
endif()

# These clients see only installed Capture headers, not source/private SDK
# includes, TinyTest, miniaudio, libyuv, PipeWire, X11, or CMeta.
add_executable(capture_consumer_c capture_consumer.c)
set_target_properties(capture_consumer_c PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(capture_consumer_c PRIVATE Salts::Capture)
add_test(NAME sdk.capture_c11 COMMAND capture_consumer_c)

add_executable(capture_consumer_cpp capture_consumer.cpp)
set_target_properties(capture_consumer_cpp PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(capture_consumer_cpp PRIVATE Salts::Capture)
add_test(NAME sdk.capture_cpp17 COMMAND capture_consumer_cpp)
set_tests_properties(sdk.capture_c11 sdk.capture_cpp17 PROPERTIES TIMEOUT 30)
foreach(_client IN ITEMS capture_consumer_c capture_consumer_cpp)
  if(MSVC)
    target_compile_options(${_client} PRIVATE /W4 /WX)
  else()
    target_compile_options(${_client} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
