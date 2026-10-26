cmake_minimum_required(VERSION 3.20)
project(SaltsCoreInstalledConsumer LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)

# find_package name remains SaltsCore; its ONLY imported link namespace is Salts::.
foreach(_owner IN ITEMS Core Platform Concurrency Coroutine NativeIO)
  if(NOT TARGET "Salts::${_owner}")
    message(FATAL_ERROR "Required canonical Salts::${_owner} target is absent")
  endif()
  get_target_property(_imported "Salts::${_owner}" IMPORTED)
  if(NOT _imported)
    message(FATAL_ERROR "Salts::${_owner} must be imported from installed SaltsCore")
  endif()
endforeach()

add_executable(consumer_c consumer.c)
target_compile_features(consumer_c PRIVATE c_std_11)
target_link_libraries(consumer_c PRIVATE
  Salts::Core Salts::Platform Salts::Concurrency Salts::Coroutine Salts::NativeIO)
add_test(NAME sdk.consumer_c COMMAND consumer_c)

add_executable(consumer_cpp consumer.cpp)
target_compile_features(consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(consumer_cpp PRIVATE
  Salts::Core Salts::Platform Salts::Concurrency Salts::Coroutine Salts::NativeIO)
add_test(NAME sdk.consumer_cpp COMMAND consumer_cpp)

if(TARGET Salts::URI)
  add_executable(uri_consumer_c uri_consumer.c)
  target_compile_features(uri_consumer_c PRIVATE c_std_11)
  target_link_libraries(uri_consumer_c PRIVATE Salts::URI)
  add_test(NAME sdk.uri_consumer_c COMMAND uri_consumer_c)

  add_executable(uri_consumer_cpp uri_consumer.cpp)
  target_compile_features(uri_consumer_cpp PRIVATE cxx_std_17)
  target_link_libraries(uri_consumer_cpp PRIVATE Salts::URI)
  add_test(NAME sdk.uri_consumer_cpp COMMAND uri_consumer_cpp)
endif()

# Only present when an exact CNet provider SDK was actually configured and
# installed. The baseline Core installation must work with CNet disabled.
if(TARGET Salts::CNet)
  add_executable(cnet_consumer_c cnet_consumer.c)
  target_compile_features(cnet_consumer_c PRIVATE c_std_11)
  target_link_libraries(cnet_consumer_c PRIVATE Salts::CNet)
  add_test(NAME sdk.cnet_consumer_c COMMAND cnet_consumer_c)

  add_executable(cnet_consumer_cpp cnet_consumer.cpp)
  target_compile_features(cnet_consumer_cpp PRIVATE cxx_std_17)
  target_link_libraries(cnet_consumer_cpp PRIVATE Salts::CNet)
  add_test(NAME sdk.cnet_consumer_cpp COMMAND cnet_consumer_cpp)

  # Independent installed SDK build: EXE and plugin both import one Core/Platform
  # runtime while CNet is simultaneously loaded. No source-build references.
  add_library(owner_probe_installed SHARED ../runtime_owner_module.c)
  target_compile_features(owner_probe_installed PRIVATE c_std_11)
  set_target_properties(owner_probe_installed PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF WINDOWS_EXPORT_ALL_SYMBOLS ON)
  target_link_libraries(owner_probe_installed PRIVATE
    Salts::Core Salts::Platform)
  add_executable(abi_owner_consumer abi_owner_consumer.c)
  target_compile_features(abi_owner_consumer PRIVATE c_std_11)
  target_link_libraries(abi_owner_consumer PRIVATE
    Salts::CNet Salts::Core Salts::Platform owner_probe_installed)
  add_test(NAME sdk.abi_owner_consumer COMMAND abi_owner_consumer)
endif()
