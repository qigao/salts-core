# Keep the minimal foundation target usable without platform/transport providers.
add_library(saltscore_core SHARED src/bytes.c)
add_library(Salts::Core ALIAS saltscore_core)
target_compile_features(saltscore_core PUBLIC c_std_11)
set_target_properties(saltscore_core PROPERTIES
  EXPORT_NAME Core C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_include_directories(saltscore_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# Public mem_pool_t and mem_buffer_t contain C11 atomic fields on MSVC.
# Consumers including cmeta_buffer.h require the same C compiler setting.
if(MSVC)
  target_compile_options(saltscore_core PUBLIC
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:-experimental:c11atomics>)
endif()

# Core-owned buffer/pool, byte utility and sole native Base64 implementation.
# No third-party Base64 DSO, vcpkg port or alternate codec is required.
target_sources(saltscore_core PRIVATE
  src/cmeta_buffer.c src/ring_buffer.c src/base64_utils.c
  src/runtime_abi.c src/utf8.c
  # Original utils/Core ownership restored; no CMeta provider or second core.
  src/vstr.c src/tstr.c src/memory_pool.c src/object_pool.c
  src/bucket_priority_queue.c src/sds/sds.c)
# SDS is original private C string storage, not an exported provider target.
target_include_directories(saltscore_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sds")
if(APPLE)
  target_compile_definitions(saltscore_core PRIVATE _DARWIN_C_SOURCE=1)
elseif(UNIX)
  target_compile_definitions(saltscore_core PRIVATE _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L)
endif()
target_link_libraries(saltscore_core PUBLIC Salts::Platform)

if(WIN32)
  set_target_properties(saltscore_core PROPERTIES
    OUTPUT_NAME "saltscore_core-${SALTSCORE_NATIVE_SOVERSION}")
endif()
install(TARGETS saltscore_core EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/salts_core
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
install(FILES
  include/base64_utils.h include/vstr.h include/cmeta_api.h
  include/cmeta_buffer.h include/ring_buffer.h
  include/tstr.h include/str.h include/memory_pool.h include/object_pool.h
  include/bucket_priority_queue.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Original upstream SDS BSD license is mandatory for this private source.
install(FILES src/sds/LICENSE
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/sds)
