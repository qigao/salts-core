#ifndef SALTSCORE_CAPTURE_REQUIRED_SYMBOLS_H
#define SALTSCORE_CAPTURE_REQUIRED_SYMBOLS_H

#if defined(__ANDROID__)
#include <salts_capture_android.h>
#endif

/* Volatile typed references force every public symbol to resolve, including
 * device APIs that this no-device consumer must not start. */
static int capture_required_symbols_present(void) {
  int (*volatile reference_salts_capture_list_audio_devices)(salts_capture_device_t *devices, int max_count) = salts_capture_list_audio_devices;
  if (!reference_salts_capture_list_audio_devices) return 1;
  int (*volatile reference_salts_capture_list_video_devices)(salts_capture_device_t *devices, int max_count) = salts_capture_list_video_devices;
  if (!reference_salts_capture_list_video_devices) return 1;
  int (*volatile reference_salts_capture_list_screens)(salts_capture_device_t *devices, int max_count) = salts_capture_list_screens;
  if (!reference_salts_capture_list_screens) return 1;
  int (*volatile reference_salts_capture_list_gpu_devices)(salts_capture_device_t *devices, int max_count) = salts_capture_list_gpu_devices;
  if (!reference_salts_capture_list_gpu_devices) return 1;
  salts_capture_t * (*volatile reference_salts_audio_capture_create)(const char *device_id, const salts_audio_capture_config_t *config) = salts_audio_capture_create;
  if (!reference_salts_audio_capture_create) return 1;
  void (*volatile reference_salts_audio_capture_set_callback)(salts_capture_t *capture, salts_audio_capture_cb cb, void *user_data) = salts_audio_capture_set_callback;
  if (!reference_salts_audio_capture_set_callback) return 1;
  int (*volatile reference_salts_video_mode_fps)(const salts_video_native_mode_t *mode) = salts_video_mode_fps;
  if (!reference_salts_video_mode_fps) return 1;
  int (*volatile reference_salts_video_mode_is_standard_fps)(const salts_video_native_mode_t *mode) = salts_video_mode_is_standard_fps;
  if (!reference_salts_video_mode_is_standard_fps) return 1;
  int (*volatile reference_salts_video_device_open)(const char *device_id, salts_video_device_t **out_device) = salts_video_device_open;
  if (!reference_salts_video_device_open) return 1;
  void (*volatile reference_salts_video_device_close)(salts_video_device_t *device) = salts_video_device_close;
  if (!reference_salts_video_device_close) return 1;
  int (*volatile reference_salts_video_device_list_modes)(salts_video_device_t *device, salts_video_native_mode_t *modes, size_t capacity, size_t *out_count) = salts_video_device_list_modes;
  if (!reference_salts_video_device_list_modes) return 1;
  int (*volatile reference_salts_video_device_list_modes_all)(salts_video_device_t *device, salts_video_native_mode_t *modes, size_t capacity, size_t *out_count) = salts_video_device_list_modes_all;
  if (!reference_salts_video_device_list_modes_all) return 1;
  int (*volatile reference_salts_video_device_create_capture)(salts_video_device_t *device, const salts_video_native_mode_t *mode, salts_capture_t **out_capture) = salts_video_device_create_capture;
  if (!reference_salts_video_device_create_capture) return 1;
  void (*volatile reference_salts_video_capture_set_callback)(salts_capture_t *capture, salts_video_capture_cb cb, void *user_data) = salts_video_capture_set_callback;
  if (!reference_salts_video_capture_set_callback) return 1;
  int (*volatile reference_salts_video_capture_get_control_range)(salts_capture_t *capture, salts_camera_control_t control, salts_camera_control_range_t *range) = salts_video_capture_get_control_range;
  if (!reference_salts_video_capture_get_control_range) return 1;
  int (*volatile reference_salts_video_capture_set_control)(salts_capture_t *capture, salts_camera_control_t control, int value) = salts_video_capture_set_control;
  if (!reference_salts_video_capture_set_control) return 1;
  int (*volatile reference_salts_video_capture_get_control)(salts_capture_t *capture, salts_camera_control_t control, int *value) = salts_video_capture_get_control;
  if (!reference_salts_video_capture_get_control) return 1;
  int (*volatile reference_salts_video_capture_set_crop)(salts_capture_t *capture, const salts_video_crop_t *crop) = salts_video_capture_set_crop;
  if (!reference_salts_video_capture_set_crop) return 1;
  int (*volatile reference_salts_video_capture_get_crop)(salts_capture_t *capture, salts_video_crop_t *crop) = salts_video_capture_get_crop;
  if (!reference_salts_video_capture_get_crop) return 1;
  salts_capture_t * (*volatile reference_salts_screen_capture_create)(const salts_screen_capture_config_t *config) = salts_screen_capture_create;
  if (!reference_salts_screen_capture_create) return 1;
  void (*volatile reference_salts_screen_capture_set_callback)(salts_capture_t *capture, salts_video_capture_cb cb, void *user_data) = salts_screen_capture_set_callback;
  if (!reference_salts_screen_capture_set_callback) return 1;
  int (*volatile reference_salts_capture_start)(salts_capture_t *capture) = salts_capture_start;
  if (!reference_salts_capture_start) return 1;
  void (*volatile reference_salts_capture_stop)(salts_capture_t *capture) = salts_capture_stop;
  if (!reference_salts_capture_stop) return 1;
  void (*volatile reference_salts_capture_destroy)(salts_capture_t *capture) = salts_capture_destroy;
  if (!reference_salts_capture_destroy) return 1;
  salts_capture_state_t (*volatile reference_salts_capture_get_state)(salts_capture_t *capture) = salts_capture_get_state;
  if (!reference_salts_capture_get_state) return 1;
  void (*volatile reference_salts_capture_on_state)(salts_capture_t *capture, salts_capture_state_cb cb) = salts_capture_on_state;
  if (!reference_salts_capture_on_state) return 1;
#if defined(__ANDROID__)
  int (*volatile reference_salts_android_screen_capture_create)(const salts_android_screen_capture_config_t *config, salts_capture_t **out_capture) = salts_android_screen_capture_create;
  if (!reference_salts_android_screen_capture_create) return 1;
  int (*volatile reference_salts_android_screen_capture_get_surface)(JNIEnv *env, salts_capture_t *capture, jobject *out_surface) = salts_android_screen_capture_get_surface;
  if (!reference_salts_android_screen_capture_get_surface) return 1;
  int (*volatile reference_salts_android_screen_capture_get_frame_count)(const salts_capture_t *capture, uint64_t *out_frame_count) = salts_android_screen_capture_get_frame_count;
  if (!reference_salts_android_screen_capture_get_frame_count) return 1;
#endif
  return 0;
}

#endif
