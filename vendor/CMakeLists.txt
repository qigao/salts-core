# Private C11 third-party source ownership, only for explicit CNet builds.
# One renamed Monocypher namespace and one Reed/GF256 target per build.
# Never install or export these source-private libraries as Salts:: providers.
if(NOT SALTSCORE_BUILD_CNET)
  message(FATAL_ERROR "vendor/CMakeLists.txt is only admitted with SALTSCORE_BUILD_CNET=ON")
endif()
find_path(SALTSCORE_CNET_SIMDE_INCLUDE_DIR NAMES simde/x86/sse2.h REQUIRED)

# Single CNet-private Monocypher namespace avoids duplicate crypto_argon2 and
# other generic symbols in programs containing more than one consumer DSO.
add_library(saltscore_cnet_monocypher STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cnet/monocypher/cnet_monocypher.c)
target_compile_features(saltscore_cnet_monocypher PUBLIC c_std_11)
target_include_directories(saltscore_cnet_monocypher
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/cnet/monocypher")
set_target_properties(saltscore_cnet_monocypher PROPERTIES
  POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

add_library(saltscore_cnet_reed STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cnet/reed/gf256.c)
target_compile_features(saltscore_cnet_reed PUBLIC c_std_11)
target_include_directories(saltscore_cnet_reed
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/cnet/reed"
  PRIVATE "${SALTSCORE_CNET_SIMDE_INCLUDE_DIR}")
set_target_properties(saltscore_cnet_reed PROPERTIES
  POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

# Keep the existing third-party license install layout without
# making CNet a second owner of the vendored source or license inputs.
install(FILES cnet/reed/LICENSE
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/reed)
install(FILES cnet/monocypher/UPSTREAM.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/monocypher)
