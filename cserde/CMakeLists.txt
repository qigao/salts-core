
# Independent bounded C11 serialization tokens, not CMeta reflection.
add_library(saltscore_cserde STATIC src/token.c src/reader.c src/writer.c)
add_library(Salts::CSerde ALIAS saltscore_cserde)
set_target_properties(saltscore_cserde PROPERTIES
  EXPORT_NAME CSerde C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)
target_compile_features(saltscore_cserde PUBLIC c_std_11)
target_include_directories(saltscore_cserde PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
install(TARGETS saltscore_cserde EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/cserde DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
