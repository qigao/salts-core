# Private C11 third-party source ownership for the canonical CNet build.
# One renamed Monocypher namespace and one Reed/GF256 target per build.
# Never install or export these source-private libraries as Salts:: providers.
find_path(SALTSCORE_CNET_SIMDE_INCLUDE_DIR NAMES simde/x86/sse2.h REQUIRED)

# Monocypher is an INDEPENDENT CMake library under vendor/monocypher.
# Keep its original CNet-private namespace for ABI/wire parity; do not fold
# it into Salts::Crypto or rename its algorithm implementations.
add_subdirectory(monocypher)

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
install(FILES monocypher/UPSTREAM.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/monocypher)
