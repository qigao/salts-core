find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

add_library(salts_cflow_usb SHARED
  src/usb.c)
cmake_config_target(
  salts_cflow_usb
  ALIAS Salts::CFlowUSB
  FOLDER "cflow-usb"
  EXPORT_NAME CFlowUSB)
target_compile_features(salts_cflow_usb PUBLIC c_std_11)
set_target_properties(salts_cflow_usb PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(salts_cflow_usb PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(salts_cflow_usb
  PRIVATE Salts::CFlow Salts::Platform PkgConfig::LIBUSB)

install(
  TARGETS salts_cflow_usb
  EXPORT SaltsUtilsTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  DIRECTORY include/cflow
  DESTINATION include
  FILES_MATCHING PATTERN "*.h")

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
