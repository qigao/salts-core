# The original libusb state machine has no CFlow runtime dependency. Its
# historical public header and thirteen cflow_usb_* symbols remain unchanged.
# Resolve one real libusb provider; the deterministic test provider below is
# never a production fallback or an installed library.
if(SALTSCORE_USB_LIBUSB_INCLUDE_DIR OR SALTSCORE_USB_LIBUSB_LIBRARY)
  if(NOT SALTSCORE_USB_LIBUSB_INCLUDE_DIR OR NOT SALTSCORE_USB_LIBUSB_LIBRARY)
    message(FATAL_ERROR
      "Explicit libusb discovery requires both SALTSCORE_USB_LIBUSB_INCLUDE_DIR and SALTSCORE_USB_LIBUSB_LIBRARY")
  endif()
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    # vcpkg's Linux/macOS providers can be archives. Include their declared
    # private system dependencies when linking them into the shared USB owner.
    set(_saltscore_usb_pkg_args "${PKG_CONFIG_ARGN}")
    list(APPEND PKG_CONFIG_ARGN --static)
    pkg_check_modules(SALTSCORE_USB_LIBUSB QUIET IMPORTED_TARGET libusb-1.0)
    set(PKG_CONFIG_ARGN "${_saltscore_usb_pkg_args}")
    unset(_saltscore_usb_pkg_args)
  endif()
endif()

if(TARGET PkgConfig::SALTSCORE_USB_LIBUSB)
  set(_saltscore_usb_provider PkgConfig::SALTSCORE_USB_LIBUSB)
else()
  find_path(SALTSCORE_USB_LIBUSB_INCLUDE_DIR libusb.h
    PATH_SUFFIXES libusb-1.0 REQUIRED)
  find_library(SALTSCORE_USB_LIBUSB_LIBRARY NAMES usb-1.0 libusb-1.0 REQUIRED)
  if(SALTSCORE_USB_LIBUSB_LIBRARY MATCHES "\\.a$")
    message(FATAL_ERROR
      "A static libusb provider requires pkg-config with its private system dependencies")
  endif()
  add_library(saltscore_usb_libusb UNKNOWN IMPORTED)
  set_target_properties(saltscore_usb_libusb PROPERTIES
    IMPORTED_LOCATION "${SALTSCORE_USB_LIBUSB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SALTSCORE_USB_LIBUSB_INCLUDE_DIR}")
  set(_saltscore_usb_provider saltscore_usb_libusb)
endif()

if(WIN32)
  get_target_property(_saltscore_usb_includes ${_saltscore_usb_provider}
    INTERFACE_INCLUDE_DIRECTORIES)
  if(TARGET PkgConfig::SALTSCORE_USB_LIBUSB)
    get_target_property(_saltscore_usb_libraries ${_saltscore_usb_provider}
      INTERFACE_LINK_LIBRARIES)
  else()
    get_target_property(_saltscore_usb_libraries ${_saltscore_usb_provider}
      IMPORTED_LOCATION)
  endif()
  include(cmake/WindowsLibusbRuntime.cmake)
  saltscore_usb_windows_provider_files(_saltscore_usb_runtime _saltscore_usb_copyright
    LIBRARIES ${_saltscore_usb_libraries} INCLUDE_DIRS ${_saltscore_usb_includes})
  install(FILES "${_saltscore_usb_runtime}" DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES "${_saltscore_usb_copyright}"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SaltsCore/libusb)
endif()

add_library(saltscore_usb SHARED src/usb.c)
add_library(Salts::USB ALIAS saltscore_usb)
target_compile_features(saltscore_usb PUBLIC c_std_11)
set_target_properties(saltscore_usb PROPERTIES
  EXPORT_NAME USB C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_include_directories(saltscore_usb PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(saltscore_usb PRIVATE
  Salts::Platform ${_saltscore_usb_provider})
if(WIN32)
  set_target_properties(saltscore_usb PROPERTIES
    OUTPUT_NAME "saltscore_usb-${SALTSCORE_NATIVE_SOVERSION}")
  # The pinned provider disables automatic app-local dependency deployment.
  # Native executables share this output directory with the USB owner.
  add_custom_command(TARGET saltscore_usb POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_saltscore_usb_runtime}" "$<TARGET_FILE_DIR:saltscore_usb>"
    COMMENT "Stage the selected libusb runtime beside native USB binaries"
    VERBATIM)
endif()

install(TARGETS saltscore_usb EXPORT SaltsCoreTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/cflow/usb.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cflow)

if(BUILD_TESTING)
  if(NOT TARGET Salts::TinyTest)
    message(FATAL_ERROR "USB source conformance requires SALTSCORE_BUILD_TINYTEST=ON")
  endif()
  add_subdirectory(tests)
endif()
