# Platform native completion/readiness ownership (no CFlow or CMeta).
add_library(saltscore_native_io SHARED
  src/native_io.c
  src/native_io_accept_escrow.c
  src/native_io_sharded.c
  src/native_ipc.c)
add_library(Salts::NativeIO ALIAS saltscore_native_io)
if(WIN32)
  target_sources(saltscore_native_io PRIVATE
    src/native_io_iocp.c src/native_ipc_windows.c)
  target_compile_definitions(saltscore_native_io PRIVATE
    WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(saltscore_native_io PRIVATE
    src/native_io_linux.c src/native_io_epoll.c
    src/native_io_io_uring.c src/native_io_readiness.c
    src/native_ipc_posix.c)
  target_compile_definitions(saltscore_native_io PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
elseif((APPLE OR CMAKE_SYSTEM_NAME MATCHES "^(FreeBSD|OpenBSD|NetBSD|DragonFly)$")
       AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  target_sources(saltscore_native_io PRIVATE
    src/native_io_bsd.c src/native_io_kqueue.c
    src/native_io_readiness.c src/native_ipc_posix.c)
  if(APPLE)
    target_compile_definitions(saltscore_native_io PRIVATE _DARWIN_C_SOURCE=1)
  endif()
else()
  target_sources(saltscore_native_io PRIVATE
    src/native_io_unsupported.c src/native_ipc_posix.c)
endif()
set_target_properties(saltscore_native_io PROPERTIES
  EXPORT_NAME NativeIO C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_compile_features(saltscore_native_io PUBLIC c_std_11)
target_include_directories(saltscore_native_io PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/core/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_native_io
  PUBLIC Salts::Platform
  PRIVATE Salts::Coroutine)
if(WIN32)
  target_link_libraries(saltscore_native_io PRIVATE ws2_32)
endif()

if(WIN32)
  set_target_properties(saltscore_native_io PROPERTIES
    OUTPUT_NAME "saltscore_native_io-${SALTSCORE_NATIVE_SOVERSION}")
endif()
install(TARGETS saltscore_native_io EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/salts
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
