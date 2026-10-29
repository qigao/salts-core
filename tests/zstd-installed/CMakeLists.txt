cmake_minimum_required(VERSION 3.20)
project(SaltsCoreZstdInstalled LANGUAGES C CXX)
include(CTest)

# The private archive must be contained by Core, not rediscovered by clients.
set(CMAKE_DISABLE_FIND_PACKAGE_zstd TRUE)
find_package(SaltsCore 0.1 CONFIG REQUIRED)
get_target_property(_core_definitions Salts::Core INTERFACE_COMPILE_DEFINITIONS)
if(NOT "SALTSCORE_HAS_ZSTD=1" IN_LIST _core_definitions)
  message(FATAL_ERROR "Installed Salts::Core must contain its original Zstd API; cannot skip conformance")
endif()

add_executable(zstd_consumer_c zstd_consumer.c)
set_target_properties(zstd_consumer_c PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(zstd_consumer_c PRIVATE Salts::Core)
add_test(NAME sdk.zstd_c11 COMMAND zstd_consumer_c)

add_executable(zstd_consumer_cpp zstd_consumer.cpp)
set_target_properties(zstd_consumer_cpp PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(zstd_consumer_cpp PRIVATE Salts::Core)
add_test(NAME sdk.zstd_cpp17 COMMAND zstd_consumer_cpp)
foreach(_consumer IN ITEMS zstd_consumer_c zstd_consumer_cpp)
  if(MSVC)
    target_compile_options(${_consumer} PRIVATE /W4 /WX)
  else()
    target_compile_options(${_consumer} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
set_tests_properties(sdk.zstd_c11 sdk.zstd_cpp17 PROPERTIES TIMEOUT 30)
