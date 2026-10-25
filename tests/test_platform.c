#include <salts/clock.h>
#include <salts/error_codes.h>
#include <salts/random.h>
#include <salts/thread.h>

#include <stddef.h>
#include <stdint.h>

static void increment(void *arg) {
  int *value = (int *)arg;
  ++*value;
}

int main(void) {
  uint8_t random_bytes[32] = {0};
  cmeta_thread_t thread = NULL;
  cmeta_mutex_t lock = NULL;
  cmeta_thread_affine_state affinity = {0};
  int value = 0;

  if (cmeta_platform_secure_random(NULL, 0u) != SALTS_OK ||
      cmeta_platform_secure_random(NULL, 1u) != SALTS_EINVAL ||
      cmeta_platform_secure_random(random_bytes, sizeof(random_bytes)) != SALTS_OK)
    return 1;
  if (cmeta_hrtime() == 0u || cmeta_realtime_ms() == 0u ||
      cmeta_monotonic_ms() == 0u || cmeta_cpu_count() < 1)
    return 2;

  cmeta_mutex_init(&lock);
  if (lock == NULL) return 3;
  cmeta_mutex_lock(&lock);
  cmeta_mutex_unlock(&lock);
  cmeta_mutex_destroy(&lock);
  if (lock != NULL) return 4;

  if (cmeta_thread_affine_init(&affinity, &affinity) != SALTS_OK ||
      cmeta_thread_affine_check(&affinity, &affinity) != SALTS_OK ||
      cmeta_thread_affine_set_busy(&affinity, &affinity, true) != SALTS_OK ||
      cmeta_thread_affine_check(&affinity, &affinity) != SALTS_EBUSY ||
      cmeta_thread_affine_set_busy(&affinity, &affinity, false) != SALTS_OK ||
      cmeta_thread_affine_reset(&affinity, &affinity) != SALTS_OK)
    return 5;

  if (cmeta_thread_create(&thread, increment, &value) != 0 || thread == NULL)
    return 6;
  if (cmeta_thread_join(&thread) != 0 || thread != NULL || value != 1)
    return 7;
  return 0;
}
