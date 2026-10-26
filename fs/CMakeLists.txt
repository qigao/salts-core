# One standalone native filesystem owner, no CMeta/CFlow or old Salts.
add_library(saltscore_fs SHARED src/fs.c src/fs_root.c)
add_library(Salts::FS ALIAS saltscore_fs)
target_compile_features(saltscore_fs PUBLIC c_std_11)
set_target_properties(saltscore_fs PROPERTIES
  EXPORT_NAME FS C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET default WINDOWS_EXPORT_ALL_SYMBOLS ON
  VERSION "${PROJECT_VERSION}" SOVERSION "${SALTSCORE_NATIVE_SOVERSION}")
target_include_directories(saltscore_fs PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(WIN32)
  set_target_properties(saltscore_fs PROPERTIES
    OUTPUT_NAME "saltscore_fs-${SALTSCORE_NATIVE_SOVERSION}")
  target_compile_definitions(saltscore_fs PRIVATE WIN32_LEAN_AND_MEAN)
elseif(APPLE)
  target_compile_definitions(saltscore_fs PRIVATE _DARWIN_C_SOURCE=1)
elseif(UNIX)
  target_compile_definitions(saltscore_fs PRIVATE
    _DEFAULT_SOURCE=1 _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()
install(TARGETS saltscore_fs EXPORT SaltsCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/salts/fs.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/salts)

if(BUILD_TESTING)
  if(NOT TARGET Salts::TinyTest)
    message(FATAL_ERROR "FS native conformance requires SALTSCORE_BUILD_TINYTEST=ON")
  endif()
  foreach(_case IN ITEMS fs fs_root)
    add_executable("saltscore_native_${_case}_test" "tests/test_${_case}.c")
    target_compile_features("saltscore_native_${_case}_test" PRIVATE c_std_11)
    set_target_properties("saltscore_native_${_case}_test" PROPERTIES
      C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_link_libraries("saltscore_native_${_case}_test" PRIVATE
      Salts::FS Salts::TinyTest)
    add_test(NAME "saltscore.native.${_case}"
      COMMAND "saltscore_native_${_case}_test")
    set_tests_properties("saltscore.native.${_case}"
      PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
  endforeach()
endif()
