function(salts_capture_original_test target source)
  add_executable(${target} ${source})
  target_compile_features(${target} PRIVATE c_std_11)
  set_target_properties(${target} PROPERTIES
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF FOLDER "capture/tests")
  target_link_libraries(${target} PRIVATE Salts::Capture Salts::TinyTest)
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endfunction()

if(WIN32)
  # Original fake-COM control test compiles the exact Windows backend body.
  # This is an isolated regression, not a replacement production provider.
  add_executable(capture_exposure_windows_test test_capture_exposure_windows.c)
  target_compile_features(capture_exposure_windows_test PRIVATE c_std_11)
  target_compile_definitions(capture_exposure_windows_test PRIVATE SALTS_CAPTURE_BUILD)
  target_link_libraries(capture_exposure_windows_test PRIVATE
    Salts::Capture Salts::Core Salts::TinyTest yuv
    mfplat mf mfuuid mfreadwrite strmiids ole32 oleaut32)
  add_test(NAME capture_exposure_windows_test COMMAND capture_exposure_windows_test)
  set_target_properties(capture_exposure_windows_test PROPERTIES FOLDER "capture/tests")
  set_tests_properties(capture_exposure_windows_test PROPERTIES TIMEOUT 60)
  salts_capture_original_test(capture_windows_test test_capture_windows.c)
endif()

if(ANDROID)
  salts_capture_original_test(capture_android_test test_capture_android.c)
endif()

if(UNIX AND NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  salts_capture_original_test(capture_posix_test test_capture_posix.c)
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME saltscore.capture_source_provenance
  COMMAND "${Python3_EXECUTABLE}" -m unittest discover
    -s "${CMAKE_CURRENT_SOURCE_DIR}" -p test_source_provenance.py -v)
set_tests_properties(saltscore.capture_source_provenance PROPERTIES
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" TIMEOUT 30)
