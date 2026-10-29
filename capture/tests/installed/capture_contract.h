#ifndef SALTSCORE_CAPTURE_INSTALLED_CONTRACT_H
#define SALTSCORE_CAPTURE_INSTALLED_CONTRACT_H

#include <salts_capture.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "capture_required_symbols.h"

#define CAPTURE_CHECK(expression) do { if (!(expression)) { \
  fprintf(stderr, "Capture installed contract failed at %s:%d: %s\n", \
          __FILE__, __LINE__, #expression); return 1; \
} } while (0)

static int capture_installed_contract(void) {
  salts_video_native_mode_t mode = {640, 480, 30000, 1001,
                                   SALTS_VIDEO_CAPTURE_FORMAT_MJPEG, UINT64_C(0x12345678)};
  salts_video_native_mode_t untouched;
  salts_video_native_mode_t before;
  salts_capture_t local;
  salts_capture_t *capture = &local;
  size_t count = 91;
  int value = 12345;

  CAPTURE_CHECK(capture_required_symbols_present() == 0);

  CAPTURE_CHECK(SALTS_CAPTURE_OK == 0 && SALTS_CAPTURE_ERR_FORMAT == -3 &&
                SALTS_CAPTURE_ERR_UNSUPPORTED == -5);
  CAPTURE_CHECK(SALTS_CAPTURE_TYPE_AUDIO == 1 && SALTS_CAPTURE_TYPE_GPU == 4);
  CAPTURE_CHECK(SALTS_VIDEO_CAPTURE_FORMAT_I420 == 0 && SALTS_VIDEO_CAPTURE_FORMAT_MJPEG == 4);
  CAPTURE_CHECK(SALTS_CAMERA_CONTROL_AUTO_EXPOSURE == 10);
  CAPTURE_CHECK(salts_video_mode_fps(&mode) == 30);
  CAPTURE_CHECK(salts_video_mode_is_standard_fps(&mode) == 1);
  CAPTURE_CHECK(mode.mode_id == UINT64_C(0x12345678));
  mode.framerate_numerator = 49;
  mode.framerate_denominator = 1;
  CAPTURE_CHECK(salts_video_mode_fps(&mode) == 49);
  CAPTURE_CHECK(salts_video_mode_is_standard_fps(&mode) == 0);
  mode.framerate_numerator = UINT32_MAX;
  CAPTURE_CHECK(salts_video_mode_fps(&mode) == 0);
  mode.framerate_denominator = 0;
  CAPTURE_CHECK(salts_video_mode_fps(&mode) == 0);
  CAPTURE_CHECK(salts_video_mode_fps(NULL) == 0);

  memset(&untouched, 0x5a, sizeof(untouched));
  memcpy(&before, &untouched, sizeof(before));
  CAPTURE_CHECK(salts_video_device_open(NULL, NULL) == SALTS_CAPTURE_ERR_FORMAT);
  CAPTURE_CHECK(salts_video_device_list_modes(NULL, &untouched, 1, &count) == SALTS_CAPTURE_ERR_FORMAT);
  CAPTURE_CHECK(count == 0 && memcmp(&before, &untouched, sizeof(before)) == 0);
  count = 93;
  CAPTURE_CHECK(salts_video_device_list_modes_all(NULL, &untouched, 1, &count) == SALTS_CAPTURE_ERR_FORMAT);
  CAPTURE_CHECK(count == 0 && memcmp(&before, &untouched, sizeof(before)) == 0);
  CAPTURE_CHECK(salts_video_device_create_capture(NULL, &mode, &capture) == SALTS_CAPTURE_ERR_FORMAT);
  CAPTURE_CHECK(capture == NULL);
  salts_video_device_close(NULL);

  CAPTURE_CHECK(salts_capture_list_audio_devices(NULL, 1) == -1);
  CAPTURE_CHECK(salts_capture_list_video_devices(NULL, 1) == -1);
  CAPTURE_CHECK(salts_capture_list_screens(NULL, 1) == -1);
  CAPTURE_CHECK(salts_capture_list_gpu_devices(NULL, 1) == -1);
  CAPTURE_CHECK(salts_capture_start(NULL) == -1);
  CAPTURE_CHECK(salts_capture_get_state(NULL) == SALTS_CAPTURE_STATE_STOPPED);
  salts_capture_stop(NULL);
  salts_capture_destroy(NULL);
  salts_capture_on_state(NULL, NULL);
  salts_audio_capture_set_callback(NULL, NULL, NULL);
  salts_video_capture_set_callback(NULL, NULL, NULL);
  salts_screen_capture_set_callback(NULL, NULL, NULL);

  /* Retain the actual original platform behavior, including its asymmetry.
   * Windows rejects null ownership without touching output. The other original
   * backends zero a supplied control value and report unsupported. */
#if defined(_WIN32)
  CAPTURE_CHECK(salts_video_capture_get_control(NULL, SALTS_CAMERA_CONTROL_EXPOSURE,
                                               &value) == SALTS_CAPTURE_ERR_DEVICE);
  CAPTURE_CHECK(value == 12345);
#else
  CAPTURE_CHECK(salts_video_capture_get_control(NULL, SALTS_CAMERA_CONTROL_EXPOSURE,
                                               &value) == SALTS_CAPTURE_ERR_UNSUPPORTED);
  CAPTURE_CHECK(value == 0);
#endif
#if defined(__ANDROID__)
  {
    salts_android_screen_capture_config_t invalid = {0, 0, 0};
    uint64_t frames = 91;
    jobject surface = NULL;
    capture = &local;
    CAPTURE_CHECK(salts_android_screen_capture_create(&invalid, &capture) == SALTS_CAPTURE_ERR_FORMAT);
    CAPTURE_CHECK(capture == NULL);
    CAPTURE_CHECK(salts_android_screen_capture_get_surface(NULL, NULL, &surface) == SALTS_CAPTURE_ERR_FORMAT);
    CAPTURE_CHECK(surface == NULL);
    CAPTURE_CHECK(salts_android_screen_capture_get_frame_count(NULL, &frames) == SALTS_CAPTURE_ERR_FORMAT);
    CAPTURE_CHECK(frames == 0);
  }
#endif
  puts("Capture installed native ABI, frame-rate math and failure ownership: PASS; device capture remains untested");
  return 0;
}

#endif
