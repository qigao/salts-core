
# Native-only optional modules. No old Salts binary, reflected CMeta metadata
# or adapter target is ever linked into the same process.
option(SALTSCORE_BUILD_CSERDE "Build standalone C11 CSerde owner" OFF)
option(SALTSCORE_BUILD_TINYTEST "Build standalone C11 TinyTest owner" OFF)
option(SALTSCORE_BUILD_UNICODE "Build original pinned Unicode 17 owner" OFF)
option(SALTSCORE_BUILD_IDNA "Build original pinned Unicode 17 IDNA owner" OFF)
if(SALTSCORE_BUILD_CSERDE)
  add_subdirectory(cserde)
endif()
if(SALTSCORE_BUILD_TINYTEST)
  add_subdirectory(tinytest)
endif()
if(SALTSCORE_BUILD_IDNA AND NOT SALTSCORE_BUILD_UNICODE)
  message(FATAL_ERROR "Salts::IDNA requires explicit SALTSCORE_BUILD_UNICODE=ON")
endif()
if(SALTSCORE_BUILD_UNICODE)
  add_subdirectory(unicode)
endif()
if(SALTSCORE_BUILD_IDNA)
  add_subdirectory(idna)
endif()

option(SALTSCORE_BUILD_ASN1 "Build standalone bounded C11 ASN.1 BER/DER codec" OFF)
if(SALTSCORE_BUILD_ASN1)
  add_subdirectory(asn1)
endif()

option(SALTSCORE_NATIVE_CPP_CONFORMANCE
  "Compile original native C++17 source contract against standalone Core" OFF)

# Native rooted filesystem; no reflection, vstr, CFlow or old target aliases.
option(SALTSCORE_BUILD_FS "Build standalone native FS owner" OFF)
if(SALTSCORE_BUILD_FS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SaltsCoreFS.cmake")
endif()
