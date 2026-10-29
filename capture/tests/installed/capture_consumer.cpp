#include "capture_contract.h"
#include <type_traits>

static_assert(std::is_standard_layout<salts_capture_t>::value, "original C capture layout");
static_assert(sizeof(salts_audio_capture_config_t) == 16, "original audio configuration ABI");
static_assert(sizeof(salts_capture_device_t) == 396, "original device descriptor ABI");
static_assert(sizeof(void *) != 8 || sizeof(salts_capture_t) == 40, "64-bit original capture ABI");
static_assert(sizeof(void *) != 8 || offsetof(salts_video_native_mode_t, mode_id) == 24,
              "64-bit original mode identity offset");

int main() { return capture_installed_contract(); }
