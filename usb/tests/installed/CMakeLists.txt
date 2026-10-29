cmake_minimum_required(VERSION 3.20)
project(SaltsCoreUSBInstalled LANGUAGES C CXX)
include(CTest)
find_package(SaltsCore 0.1 CONFIG REQUIRED)
if(NOT TARGET Salts::USB)
  message(FATAL_ERROR "The installed SDK must contain the real Salts::USB owner")
endif()

# No build-tree header, TinyTest, libusb development header, or source file is
# visible here. Both clients use only the installed native USB target.
add_executable(usb_consumer_c usb_consumer.c)
set_target_properties(usb_consumer_c PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(usb_consumer_c PRIVATE Salts::USB)
add_test(NAME sdk.usb_c11 COMMAND usb_consumer_c)

add_executable(usb_consumer_cpp usb_consumer.cpp)
set_target_properties(usb_consumer_cpp PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(usb_consumer_cpp PRIVATE Salts::USB)
add_test(NAME sdk.usb_cpp17 COMMAND usb_consumer_cpp)
set_tests_properties(sdk.usb_c11 sdk.usb_cpp17 PROPERTIES TIMEOUT 30)
