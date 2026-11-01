cmake_add_test(
  playback_buffer_test
  SOURCES test_playback_buffer.c ../src/playback_buffer.c
  LIBS Salts::Concurrency Salts::TinyTest
  INCLUDES ../src
  FOLDER "playback/tests")

add_library(
  playback_null_test_impl STATIC
  ../src/miniaudio_impl.c
  ../src/playback_buffer.c
  ../src/salts_playback.c)
cmake_config_target(playback_null_test_impl FOLDER "playback/tests")
if(APPLE)
  set_source_files_properties(../src/miniaudio_impl.c PROPERTIES LANGUAGE OBJC)
  target_link_libraries(playback_null_test_impl PRIVATE
    ${SALTS_PLAYBACK_AUDIO_TOOLBOX_FRAMEWORK}
    ${SALTS_PLAYBACK_CORE_AUDIO_FRAMEWORK}
    ${SALTS_PLAYBACK_CORE_FOUNDATION_FRAMEWORK}
    ${SALTS_PLAYBACK_FOUNDATION_FRAMEWORK}
    ${SALTS_PLAYBACK_AV_FOUNDATION_FRAMEWORK})
endif()
target_compile_features(playback_null_test_impl PUBLIC c_std_11)
target_compile_definitions(
  playback_null_test_impl PRIVATE SALTS_PLAYBACK_TEST_NULL_BACKEND=1)
set_target_properties(playback_null_test_impl PROPERTIES C_VISIBILITY_PRESET hidden)
target_include_directories(
  playback_null_test_impl
  PUBLIC ../include
  PRIVATE ../src ${SALTS_PLAYBACK_MINIAUDIO_INCLUDE_DIR})
target_link_libraries(
  playback_null_test_impl PRIVATE Salts::Concurrency Salts::Platform)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(playback_null_test_impl PRIVATE m)
endif()

cmake_add_test(
  playback_lifecycle_test
  SOURCES test_playback_lifecycle.c
  LIBS playback_null_test_impl Salts::TinyTest
  FOLDER "playback/tests")
