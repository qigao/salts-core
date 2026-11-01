if(WIN32)
  # Exercise the real Windows control entry points with a fake COM device;
  # including the backend keeps its private capture context out of the SDK ABI.
  add_executable(capture_exposure_windows_test test_capture_exposure_windows.c)
  target_compile_definitions(capture_exposure_windows_test PRIVATE SALTS_CAPTURE_BUILD)
  target_link_libraries(capture_exposure_windows_test PRIVATE
    Salts::Capture Salts::Core Salts::TinyTest yuv
    mfplat mf mfuuid mfreadwrite strmiids ole32 oleaut32)
  add_test(NAME capture_exposure_windows_test COMMAND capture_exposure_windows_test)
  set_target_properties(capture_exposure_windows_test PROPERTIES FOLDER "capture/tests")
  cmake_add_test(
    capture_windows_test
    SOURCES test_capture_windows.c
    LIBS Salts::Capture Salts::TinyTest
    FOLDER "capture/tests")
endif()

if(ANDROID)
  cmake_add_test(
    capture_android_test
    SOURCES test_capture_android.c
    LIBS Salts::Capture Salts::TinyTest
    FOLDER "capture/tests")
endif()

if(UNIX AND NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  cmake_add_test(
    capture_posix_test
    SOURCES test_capture_posix.c
    LIBS Salts::Capture Salts::TinyTest
    FOLDER "capture/tests")
endif()
