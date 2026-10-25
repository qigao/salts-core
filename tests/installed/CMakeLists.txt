cmake_minimum_required(VERSION 3.20)
project(SaltsCoreInstalledConsumer LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore CONFIG REQUIRED)

add_executable(consumer_c consumer.c)
target_compile_features(consumer_c PRIVATE c_std_11)
target_link_libraries(consumer_c PRIVATE
  SaltsCore::Core SaltsCore::Platform SaltsCore::Concurrency SaltsCore::Coroutine SaltsCore::NativeIO)
add_test(NAME sdk.consumer_c COMMAND consumer_c)

add_executable(consumer_cpp consumer.cpp)
target_compile_features(consumer_cpp PRIVATE cxx_std_17)
target_link_libraries(consumer_cpp PRIVATE
  SaltsCore::Core SaltsCore::Platform SaltsCore::Concurrency SaltsCore::Coroutine SaltsCore::NativeIO)
add_test(NAME sdk.consumer_cpp COMMAND consumer_cpp)

if(TARGET SaltsCore::URI)
  add_executable(uri_consumer_c uri_consumer.c)
  target_compile_features(uri_consumer_c PRIVATE c_std_11)
  target_link_libraries(uri_consumer_c PRIVATE SaltsCore::URI)
  add_test(NAME sdk.uri_consumer_c COMMAND uri_consumer_c)

  add_executable(uri_consumer_cpp uri_consumer.cpp)
  target_compile_features(uri_consumer_cpp PRIVATE cxx_std_17)
  target_link_libraries(uri_consumer_cpp PRIVATE SaltsCore::URI)
  add_test(NAME sdk.uri_consumer_cpp COMMAND uri_consumer_cpp)
endif()

# Only present when an exact CNet provider SDK was actually configured and
# installed. The baseline Core installation must work with CNet disabled.
if(TARGET SaltsCore::CNet)
  add_executable(cnet_consumer_c cnet_consumer.c)
  target_compile_features(cnet_consumer_c PRIVATE c_std_11)
  target_link_libraries(cnet_consumer_c PRIVATE SaltsCore::CNet)
  add_test(NAME sdk.cnet_consumer_c COMMAND cnet_consumer_c)

  add_executable(cnet_consumer_cpp cnet_consumer.cpp)
  target_compile_features(cnet_consumer_cpp PRIVATE cxx_std_17)
  target_link_libraries(cnet_consumer_cpp PRIVATE SaltsCore::CNet)
  add_test(NAME sdk.cnet_consumer_cpp COMMAND cnet_consumer_cpp)
endif()
