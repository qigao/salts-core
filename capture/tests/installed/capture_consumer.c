#include "capture_contract.h"

_Static_assert(sizeof(salts_audio_capture_config_t) == 16, "original audio configuration ABI");
_Static_assert(sizeof(salts_capture_device_t) == 396, "original device descriptor ABI");
_Static_assert(sizeof(void *) != 8 || sizeof(salts_capture_t) == 40, "64-bit original capture ABI");
_Static_assert(sizeof(void *) != 8 || offsetof(salts_video_native_mode_t, mode_id) == 24,
               "64-bit original mode identity offset");

int main(void) { return capture_installed_contract(); }
