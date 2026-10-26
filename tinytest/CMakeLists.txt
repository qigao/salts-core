
# TinyTest owns no CMeta metadata or mocking runtime. TinyMock stays in Salts.
add_library(saltscore_tinytest STATIC src/tinytest.c)
add_library(Salts::TinyTest ALIAS saltscore_tinytest)
set_target_properties(saltscore_tinytest PROPERTIES
  EXPORT_NAME TinyTest C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)
target_compile_features(saltscore_tinytest PUBLIC c_std_11)
target_include_directories(saltscore_tinytest PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
install(TARGETS saltscore_tinytest EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/tinytest.h include/tinytest.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/tinytest/detail
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tinytest
  FILES_MATCHING PATTERN "*.h")
