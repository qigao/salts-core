if(NOT WIN32 AND NOT ANDROID AND NOT APPLE AND
   NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR
          "Salts Playback supports only Windows, Linux, macOS, Android, and iOS")
endif()

if(APPLE)
  enable_language(OBJC)
  # CoreAudio headers use Blocks even when the public playback API is C.
  set_source_files_properties(src/miniaudio_impl.c PROPERTIES LANGUAGE OBJC)
endif()

add_library(
  salts_playback SHARED
  src/miniaudio_impl.c
  src/playback_buffer.c
  src/salts_playback.c)
cmake_config_target(
  salts_playback
  ALIAS Salts::Playback
  FOLDER "playback"
  EXPORT_NAME Playback)
set_target_properties(
  salts_playback
  PROPERTIES C_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES)
target_compile_features(salts_playback PUBLIC c_std_11)
target_compile_definitions(
  salts_playback
  PRIVATE SALTS_PLAYBACK_SHARED SALTS_PLAYBACK_BUILD
  INTERFACE SALTS_PLAYBACK_SHARED)
target_include_directories(
  salts_playback
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_path(SALTS_PLAYBACK_MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h REQUIRED)
set_source_files_properties(
  src/miniaudio_impl.c
  PROPERTIES COMPILE_OPTIONS
             "$<$<C_COMPILER_ID:MSVC>:/W0>;$<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>")
target_include_directories(
  salts_playback PRIVATE ${SALTS_PLAYBACK_MINIAUDIO_INCLUDE_DIR})
target_link_libraries(salts_playback PRIVATE Salts::Concurrency Salts::Platform)

if(APPLE)
  target_link_options(
    salts_playback
    PRIVATE "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/apple_exports.txt")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
  target_link_options(salts_playback PRIVATE "LINKER:--exclude-libs,ALL")
endif()

if(APPLE)
  find_library(SALTS_PLAYBACK_AUDIO_TOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
  find_library(SALTS_PLAYBACK_CORE_AUDIO_FRAMEWORK CoreAudio REQUIRED)
  find_library(SALTS_PLAYBACK_CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  find_library(SALTS_PLAYBACK_FOUNDATION_FRAMEWORK Foundation REQUIRED)
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_library(SALTS_PLAYBACK_AV_FOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  endif()
  target_link_libraries(
    salts_playback
    PRIVATE ${SALTS_PLAYBACK_AUDIO_TOOLBOX_FRAMEWORK}
            ${SALTS_PLAYBACK_CORE_AUDIO_FRAMEWORK}
            ${SALTS_PLAYBACK_CORE_FOUNDATION_FRAMEWORK}
            ${SALTS_PLAYBACK_FOUNDATION_FRAMEWORK}
            ${SALTS_PLAYBACK_AV_FOUNDATION_FRAMEWORK})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(salts_playback PRIVATE ${CMAKE_DL_LIBS} m)
endif()

install(
  TARGETS salts_playback
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
