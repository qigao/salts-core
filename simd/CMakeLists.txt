# Keep the original SIMDe implementation private to one native SIMD owner.
# Missing SIMDe is a configuration error, never an alternate implementation.
find_path(SIMDE_INCLUDE_DIRS "simde/wasm/simd128.h" REQUIRED)

add_library(saltscore_simd SHARED src/simd.c src/simd_types.c)
add_library(Salts::SIMD ALIAS saltscore_simd)
target_compile_features(saltscore_simd PUBLIC c_std_11)
set_target_properties(saltscore_simd PROPERTIES
  EXPORT_NAME SIMD C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_compile_definitions(saltscore_simd PRIVATE SALTS_SIMD_BUILD=1)
target_include_directories(saltscore_simd PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(saltscore_simd SYSTEM PRIVATE "${SIMDE_INCLUDE_DIRS}")

# SIMDe's portable nearest operations use nearbyint/nearbyintf.
if(NOT WIN32)
  target_link_libraries(saltscore_simd PRIVATE m)
endif()
if(WIN32)
  set_target_properties(saltscore_simd PROPERTIES
    OUTPUT_NAME "saltscore_simd-${SALTSCORE_NATIVE_SOVERSION}")
endif()

install(TARGETS saltscore_simd EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/salts DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
