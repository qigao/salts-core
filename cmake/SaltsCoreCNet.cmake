# Core CNet: explicit provider-backed network transport. No fallback and
# no target/source import from the historical Salts or SaltsUtils packages.
if(NOT TARGET Salts::URI OR NOT TARGET saltscore_wsparser)
  message(FATAL_ERROR "CNet requires native Core URI and WebSocket parser targets")
endif()
find_package(c-ares CONFIG REQUIRED)
find_package(kcp CONFIG REQUIRED)
find_package(GmSSL CONFIG REQUIRED)
find_path(SALTSCORE_CNET_SIMDE_INCLUDE_DIR NAMES simde/x86/sse2.h REQUIRED)
if(NOT TARGET c-ares::cares OR NOT TARGET kcp::kcp OR NOT TARGET GmSSL::GmSSL)
  message(FATAL_ERROR "CNet requires c-ares::cares, kcp::kcp and GmSSL::GmSSL")
endif()

# Never fold a second copy of any stateful runtime into CNet. Core/Platform/
# Concurrency/Coroutine/NativeIO are one installed shared owner each.
foreach(_owner IN ITEMS saltscore_core saltscore_platform saltscore_concurrency
                        saltscore_coroutine saltscore_native_io)
  get_target_property(_owner_kind ${_owner} TYPE)
  if(NOT _owner_kind STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR "CNet requires shared foundation owner: ${_owner}")
  endif()
endforeach()
# The stateless URI parser may remain an independent PIC archive.
set_target_properties(saltscore_uri PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Single CNet-private Monocypher namespace avoids duplicate crypto_argon2 and
# other generic symbols in programs containing more than one consumer DSO.
add_library(saltscore_cnet_monocypher STATIC
  vendor/cnet/monocypher/cnet_monocypher.c)
target_compile_features(saltscore_cnet_monocypher PUBLIC c_std_11)
target_include_directories(saltscore_cnet_monocypher
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/monocypher")
set_target_properties(saltscore_cnet_monocypher PROPERTIES
  POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

add_library(saltscore_cnet_reed STATIC
  vendor/cnet/reed/gf256.c)
target_compile_features(saltscore_cnet_reed PUBLIC c_std_11)
target_include_directories(saltscore_cnet_reed
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/reed"
  PRIVATE "${SALTSCORE_CNET_SIMDE_INCLUDE_DIR}")
set_target_properties(saltscore_cnet_reed PROPERTIES
  POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

# Salts source at refactor/cnet-ipc-name-native: native C11 FIFO, indexed peers,
# and caller-owned IPC path. Keep the entire CNet DSO in one ownership domain.
set(_saltscore_cnet_sources
  cnet/src/cnet_client.c
  cnet/src/cnet_command.c
  cnet/src/cnet_datagram.c
  cnet/src/cnet_kcp.c
  cnet/src/cnet_kcp_fec.c
  cnet/src/cnet_kcp_secure.c
  cnet/src/cnet_secure_kcp.c
  cnet/src/cnet_packet_endpoint.c
  cnet/src/cnet_dispatcher.c
  cnet/src/cnet_event.c
  cnet/src/cnet_listener.c
  cnet/src/cnet_module.c
  cnet/src/cnet_owner.c
  cnet/src/cnet_owner_strategy.c
  cnet/src/cnet_destination_policy.c
  cnet/src/cnet_client_pool.c
  cnet/src/cnet_recovery_policy.c
  cnet/src/cnet_managed_dial.c
  cnet/src/cnet_sg_host.c
  cnet/src/cnet_resolver.c
  cnet/src/cnet_session.c
  cnet/src/cnet_shards.c
  cnet/src/cnet_tls.c
  cnet/src/cnet_transport.c
  cnet/src/cnet_transport_pipe.c
  cnet/src/cnet_transport_ipc.c
  cnet/src/cnet_ipc.c
  cnet/src/cnet_transport_vsock.c
  cnet/src/cnet_uri.c
  cnet/src/cnet_write_queue.c
  cnet/src/cnet_websocket.c
  cnet/src/cnet_manager.c
  cnet/src/cnet_handoff.c
  $<TARGET_OBJECTS:saltscore_wsparser>)

add_library(saltscore_cnet SHARED ${_saltscore_cnet_sources})
add_library(Salts::CNet ALIAS saltscore_cnet)
target_compile_features(saltscore_cnet PUBLIC c_std_11)
set_target_properties(saltscore_cnet PROPERTIES
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}"
  EXPORT_NAME CNet C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(WIN32)
  set_target_properties(saltscore_cnet PROPERTIES
    OUTPUT_NAME "saltscore_cnet-${SALTSCORE_NATIVE_SOVERSION}")
endif()
target_include_directories(saltscore_cnet
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cnet/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cnet/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/cnet/src/wsparser"
    "${CMAKE_CURRENT_SOURCE_DIR}/native-io/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/monocypher"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cnet/reed")
target_link_libraries(saltscore_cnet
  PUBLIC
    Salts::Core
    Salts::NativeIO
    Salts::Concurrency
  PRIVATE
    Salts::URI
    Salts::Platform
    saltscore_cnet_monocypher
    saltscore_cnet_reed
    c-ares::cares
    kcp::kcp
    GmSSL::GmSSL
    $<$<PLATFORM_ID:Windows>:crypt32>)
if(APPLE)
  find_library(SALTSCORE_CNET_COREFOUNDATION CoreFoundation REQUIRED)
  find_library(SALTSCORE_CNET_SECURITY Security REQUIRED)
  target_link_libraries(saltscore_cnet PRIVATE
    "${SALTSCORE_CNET_COREFOUNDATION}"
    "${SALTSCORE_CNET_SECURITY}")
endif()

install(TARGETS saltscore_cnet EXPORT SaltsCoreTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY cnet/include/cnet
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
install(FILES vendor/cnet/reed/LICENSE
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/reed)
install(FILES vendor/cnet/monocypher/UPSTREAM.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/monocypher)
