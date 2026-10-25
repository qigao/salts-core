cmake_minimum_required(VERSION 3.20)
project(SaltsCore VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

# Only dependency-free foundation code is built at M0. Optional modules will
# receive separate targets as their source/ABI ownership is transferred.
add_library(saltscore_core STATIC src/bytes.c)
add_library(SaltsCore::Core ALIAS saltscore_core)
target_compile_features(saltscore_core PUBLIC c_std_11)
set_target_properties(saltscore_core PROPERTIES
  EXPORT_NAME Core
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden)
target_include_directories(saltscore_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(TARGETS saltscore_core EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/salts_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")

set(_saltscore_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/SaltsCore")
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SaltsCoreConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfig.cmake"
  INSTALL_DESTINATION "${_saltscore_package_dir}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsCoreConfigVersion.cmake"
  DESTINATION "${_saltscore_package_dir}")
install(EXPORT SaltsCoreTargets FILE SaltsCoreTargets.cmake
  NAMESPACE SaltsCore:: DESTINATION "${_saltscore_package_dir}")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
