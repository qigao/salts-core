#include <salts_playback.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "Playback contract failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

int main(void) {
  salts_playback_config_t config = {0, 2, SALTS_PLAYBACK_FORMAT_F32, 50};
  salts_playback_t *playback = (salts_playback_t *)(uintptr_t)1;
  salts_playback_device_t device;
  unsigned char samples[8] = {0};
  size_t count = 777;
  size_t written = 777;
  float volume = 1.0f;
  memset(&device, 0, sizeof(device));

  /* This links the production DSO, never the deterministic null-device test
     archive. It qualifies ABI and invalid-input behavior without assuming an
     audio device exists on the host; physical playback remains separate. */
  REQUIRE(salts_playback_list_devices(NULL, 1, &count) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(count == 0);
  REQUIRE(salts_playback_list_devices(&device, 1, NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_get_default_device(NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_create(NULL, &config, &playback) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(playback == NULL);
  REQUIRE(salts_playback_create(NULL, NULL, NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  salts_playback_destroy(NULL);
  REQUIRE(salts_playback_set_state_callback(NULL, NULL, NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_start(NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_stop(NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_pause(NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_resume(NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_get_state(NULL) == SALTS_PLAYBACK_STATE_ERROR);
  REQUIRE(salts_playback_set_volume(NULL, 0.25f) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_get_volume(NULL, &volume) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(volume == 0.0f);
  REQUIRE(salts_playback_write(NULL, samples, sizeof(samples), &written) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(written == 0);
  REQUIRE(salts_playback_get_available(NULL) == 0);
  REQUIRE(salts_playback_get_buffered(NULL) == 0);
  REQUIRE(salts_playback_clear(NULL) == SALTS_PLAYBACK_ERR_FORMAT);
  REQUIRE(salts_playback_drain(NULL, 10) == SALTS_PLAYBACK_ERR_FORMAT);
  puts("Playback production ABI and failure-output contract: PASS");
  return 0;
}
