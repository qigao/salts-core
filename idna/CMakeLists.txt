
# IDNA is a strict Unicode 17 UTS #46 implementation. No CMeta/CFlow/API shim.
if(NOT TARGET Salts::Unicode)
  message(FATAL_ERROR "Salts::IDNA requires Salts::Unicode in this Core build")
endif()
add_library(saltscore_idna STATIC src/salts_idna.c src/idna_data.h)
add_library(Salts::IDNA ALIAS saltscore_idna)
set_target_properties(saltscore_idna PROPERTIES
  EXPORT_NAME IDNA C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)
target_compile_features(saltscore_idna PUBLIC c_std_11)
target_include_directories(saltscore_idna
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_idna PUBLIC Salts::Unicode)
install(TARGETS saltscore_idna EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/salts_idna.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
