# The public Serial ABI stays at its original module-local header path.
set(SALTS_SERIAL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SALTS_SERIAL_SOURCES salts_serial.c)
set(SALTS_SERIAL_PLATFORM_LIBS)
if(WIN32)
  list(APPEND SALTS_SERIAL_SOURCES backend_win32.c)
  list(APPEND SALTS_SERIAL_PLATFORM_LIBS setupapi cfgmgr32)
else()
  list(APPEND SALTS_SERIAL_SOURCES backend_posix.c)
endif()

add_library(saltscore_serial SHARED ${SALTS_SERIAL_SOURCES})
add_library(Salts::Serial ALIAS saltscore_serial)
target_compile_features(saltscore_serial PUBLIC c_std_11)
set_target_properties(saltscore_serial PROPERTIES
  EXPORT_NAME Serial C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  DEFINE_SYMBOL SALTS_SERIAL_EXPORTS C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_include_directories(saltscore_serial PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_serial PRIVATE
  Salts::Core Salts::Platform Salts::Concurrency ${SALTS_SERIAL_PLATFORM_LIBS})
if(MSVC)
  target_compile_options(saltscore_serial PRIVATE /wd4996)
  set_target_properties(saltscore_serial PROPERTIES
    OUTPUT_NAME "saltscore_serial-${SALTSCORE_NATIVE_SOVERSION}")
elseif(UNIX)
  # Darwin hides the original high-baud termios constants in strict POSIX mode.
  target_compile_definitions(saltscore_serial PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L
    $<$<BOOL:${APPLE}>:_DARWIN_C_SOURCE=1>)
endif()

install(TARGETS saltscore_serial EXPORT SaltsCoreTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES salts_serial.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(BUILD_TESTING)
  if(NOT TARGET Salts::TinyTest)
    message(FATAL_ERROR "Serial original conformance requires SALTSCORE_BUILD_TINYTEST=ON")
  endif()
  add_subdirectory(test)
endif()
option(SALTSCORE_BUILD_SERIAL_EXAMPLES "Build all six retained Serial examples" OFF)
if(SALTSCORE_BUILD_SERIAL_EXAMPLES)
  add_subdirectory(examples)
endif()
