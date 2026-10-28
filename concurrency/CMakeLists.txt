# The original bounded primitives have no Reflection/CFlow dependency.
# Keep all six original source units together for full API surface coverage.
add_library(saltscore_concurrency SHARED
  src/deadline_queue.c src/disruptor.c
  src/rcu.c src/spsc_ring.c
  src/thread_pool.c src/thread_pool_lf.c)
add_library(Salts::Concurrency ALIAS saltscore_concurrency)
set_target_properties(saltscore_concurrency PROPERTIES
  EXPORT_NAME Concurrency C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_compile_features(saltscore_concurrency PUBLIC c_std_11)
target_include_directories(saltscore_concurrency PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_concurrency PUBLIC Salts::Platform)
if(MSVC)
  target_compile_options(saltscore_concurrency PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-experimental:c11atomics>)
endif()
if(APPLE)
  target_compile_definitions(saltscore_concurrency PRIVATE _DARWIN_C_SOURCE=1)
elseif(UNIX)
  target_compile_definitions(saltscore_concurrency PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()

if(WIN32)
  set_target_properties(saltscore_concurrency PROPERTIES
    OUTPUT_NAME "saltscore_concurrency-${SALTSCORE_NATIVE_SOVERSION}")
endif()
install(TARGETS saltscore_concurrency EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/salts
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
