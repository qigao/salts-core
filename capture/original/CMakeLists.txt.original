add_library(
  salts_capture SHARED
  src/capture_video.c)

cmake_config_target(
  salts_capture
  ALIAS Salts::Capture
  FOLDER "capture"
  EXPORT_NAME Capture)
target_compile_features(salts_capture PUBLIC c_std_11)
target_compile_definitions(
  salts_capture
  PRIVATE SALTS_CAPTURE_SHARED SALTS_CAPTURE_BUILD
  INTERFACE SALTS_CAPTURE_SHARED)

target_include_directories(
  salts_capture
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(salts_capture PRIVATE Salts::Core)

# CMake serializes PRIVATE shared-library targets into installed dependency
# properties. Consume backend usage requirements locally and link their
# artifact paths so third-party target names do not become package contracts.
function(salts_capture_apply_private_usage dependency)
  set(interface_properties
      INTERFACE_COMPILE_DEFINITIONS
      INTERFACE_COMPILE_FEATURES
      INTERFACE_COMPILE_OPTIONS
      INTERFACE_LINK_DIRECTORIES
      INTERFACE_LINK_LIBRARIES
      INTERFACE_LINK_OPTIONS)
  set(interface_commands
      target_compile_definitions
      target_compile_features
      target_compile_options
      target_link_directories
      target_link_libraries
      target_link_options)
  foreach(property_name command_name IN ZIP_LISTS interface_properties
                                                interface_commands)
    get_target_property(property_value ${dependency} ${property_name})
    if(property_value)
      cmake_language(CALL ${command_name} salts_capture PRIVATE
                     ${property_value})
    endif()
  endforeach()

  foreach(include_property INTERFACE_INCLUDE_DIRECTORIES
                           INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
    get_target_property(interface_includes ${dependency} ${include_property})
    if(interface_includes)
      target_include_directories(salts_capture SYSTEM PRIVATE
                                 ${interface_includes})
    endif()
  endforeach()
endfunction()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  find_path(SALTS_CAPTURE_MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h REQUIRED)
  target_sources(salts_capture PRIVATE src/miniaudio_impl.c)
  target_include_directories(
    salts_capture PRIVATE ${SALTS_CAPTURE_MINIAUDIO_INCLUDE_DIR})
endif()

if(WIN32 OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(libyuv CONFIG REQUIRED)
  salts_capture_apply_private_usage(yuv)
  target_link_libraries(salts_capture PRIVATE $<TARGET_LINKER_FILE:yuv>)
endif()

if(WIN32)
  find_package(libjpeg-turbo CONFIG REQUIRED)
  install(FILES "$<TARGET_FILE:yuv>" "$<TARGET_FILE:libjpeg-turbo::jpeg>"
          DESTINATION bin)
endif()

if(ANDROID)
  target_sources(
    salts_capture
    PRIVATE src/android/capture_android.c
            src/android/capture_audio_android.c
            src/android/capture_video_android.c
            src/android/capture_screen_android.c)
  find_library(SALTS_CAPTURE_ANDROID_LIBRARY android REQUIRED)
  find_library(SALTS_CAPTURE_ANDROID_OPENSL_LIBRARY OpenSLES REQUIRED)
  find_library(SALTS_CAPTURE_ANDROID_MEDIA_LIBRARY mediandk REQUIRED)
  find_library(SALTS_CAPTURE_ANDROID_CAMERA_LIBRARY camera2ndk REQUIRED)
  target_compile_definitions(salts_capture PRIVATE SALTS_CAPTURE_ANDROID=1)
  target_link_libraries(
    salts_capture
    PRIVATE ${SALTS_CAPTURE_ANDROID_LIBRARY}
            ${SALTS_CAPTURE_ANDROID_OPENSL_LIBRARY}
            ${SALTS_CAPTURE_ANDROID_MEDIA_LIBRARY}
            ${SALTS_CAPTURE_ANDROID_CAMERA_LIBRARY}
            ${CMAKE_DL_LIBS})
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  enable_language(OBJC)
  target_sources(
    salts_capture
    PRIVATE src/ios/capture_ios.m src/ios/capture_ios_guard.m
            src/ios/capture_audio_ios.m
            src/ios/capture_video_ios.m src/ios/capture_screen_ios.m)
  target_include_directories(salts_capture PRIVATE src/ios)
  target_compile_definitions(salts_capture PRIVATE SALTS_CAPTURE_IOS=1)
  target_compile_options(salts_capture PRIVATE -fobjc-arc)
  find_library(SALTS_CAPTURE_FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(SALTS_CAPTURE_AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  find_library(SALTS_CAPTURE_COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
  find_library(SALTS_CAPTURE_COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
  find_library(SALTS_CAPTURE_AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
  find_library(SALTS_CAPTURE_COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
  find_library(SALTS_CAPTURE_REPLAYKIT_FRAMEWORK ReplayKit REQUIRED)
  find_library(SALTS_CAPTURE_UIKIT_FRAMEWORK UIKit REQUIRED)
  target_link_libraries(
    salts_capture
    PRIVATE ${SALTS_CAPTURE_FOUNDATION_FRAMEWORK}
            ${SALTS_CAPTURE_AVFOUNDATION_FRAMEWORK}
            ${SALTS_CAPTURE_COREMEDIA_FRAMEWORK}
            ${SALTS_CAPTURE_COREVIDEO_FRAMEWORK}
            ${SALTS_CAPTURE_AUDIOTOOLBOX_FRAMEWORK}
            ${SALTS_CAPTURE_COREAUDIO_FRAMEWORK}
            ${SALTS_CAPTURE_REPLAYKIT_FRAMEWORK}
            ${SALTS_CAPTURE_UIKIT_FRAMEWORK})
  set_target_properties(
    salts_capture
    PROPERTIES XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.0"
               XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
elseif(WIN32)
  target_sources(
    salts_capture
    PRIVATE src/capture_audio_miniaudio.c src/capture_win32.c
            src/capture_video_win32.c
            src/capture_screen_win32.c)
  target_link_libraries(
    salts_capture
    PRIVATE mfplat mf mfuuid mfreadwrite strmiids ole32 oleaut32 d3d11 dxgi
            dxguid)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(PkgConfig REQUIRED)
  target_sources(
    salts_capture
    PRIVATE src/capture_audio_miniaudio.c src/capture_linux.c
            src/capture_video_linux.c
            src/capture_screen_linux.c)
  target_compile_definitions(
    salts_capture PRIVATE SALTS_HAS_PIPEWIRE SALTS_HAS_X11)
  pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
  find_package(X11 REQUIRED COMPONENTS Xext)
  salts_capture_apply_private_usage(Threads::Threads)
  target_include_directories(
    salts_capture SYSTEM PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${X11_INCLUDE_DIR})
  target_compile_options(salts_capture PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
  target_link_directories(salts_capture PRIVATE ${PIPEWIRE_LIBRARY_DIRS})
  target_link_options(salts_capture PRIVATE ${PIPEWIRE_LDFLAGS_OTHER})
  target_link_libraries(
    salts_capture
    PRIVATE ${PIPEWIRE_LINK_LIBRARIES}
            ${X11_LIBRARIES}
            ${X11_Xext_LIB}
            ${CMAKE_DL_LIBS}
            m)
elseif(APPLE)
  enable_language(OBJC)
  # Miniaudio's CoreAudio implementation includes Blocks-based system headers.
  set_source_files_properties(src/miniaudio_impl.c PROPERTIES LANGUAGE OBJC)
  target_sources(
    salts_capture
    PRIVATE src/capture_audio_miniaudio.c src/capture_macos.c
            src/capture_video_macos.m
            src/capture_screen_macos.m)
  find_library(SALTS_CAPTURE_FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(SALTS_CAPTURE_AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  find_library(SALTS_CAPTURE_COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
  find_library(SALTS_CAPTURE_AUDIO_TOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
  find_library(SALTS_CAPTURE_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  find_library(SALTS_CAPTURE_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
  find_library(SALTS_CAPTURE_COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
  find_library(SALTS_CAPTURE_COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
  find_library(SALTS_CAPTURE_SCREENCAPTUREKIT_FRAMEWORK ScreenCaptureKit REQUIRED)
  target_link_libraries(
    salts_capture
    PRIVATE ${SALTS_CAPTURE_FOUNDATION_FRAMEWORK}
            ${SALTS_CAPTURE_AVFOUNDATION_FRAMEWORK}
            ${SALTS_CAPTURE_COREAUDIO_FRAMEWORK}
            ${SALTS_CAPTURE_AUDIO_TOOLBOX_FRAMEWORK}
            ${SALTS_CAPTURE_COREFOUNDATION_FRAMEWORK}
            ${SALTS_CAPTURE_COREGRAPHICS_FRAMEWORK}
            ${SALTS_CAPTURE_COREMEDIA_FRAMEWORK}
            ${SALTS_CAPTURE_COREVIDEO_FRAMEWORK}
            ${SALTS_CAPTURE_SCREENCAPTUREKIT_FRAMEWORK})
endif()

install(
  TARGETS salts_capture
  EXPORT SaltsUtilsTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.h")

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
