#include <salts/platform.h>
#include <salts/thread.h>

#include <stddef.h>
#include <stdint.h>
#include <time.h>

static int (*volatile ref_gettimeofday)(cmeta_timeval_t *, cmeta_timezone_t *) =
    cmeta_gettimeofday;
static int (*volatile ref_gmtime)(time_t, struct tm *) = cmeta_gmtime;
static int (*volatile ref_localtime)(time_t, struct tm *) = cmeta_localtime;
static time_t (*volatile ref_timegm)(const struct tm *) = cmeta_timegm;
static time_t (*volatile ref_mktime)(struct tm *) = cmeta_mktime;
static int (*volatile ref_strftime_utc)(time_t, const char *, char *, size_t) =
    cmeta_strftime_utc;
static int (*volatile ref_strftime_local)(time_t, const char *, char *, size_t) =
    cmeta_strftime_local;
static int (*volatile ref_secure_random)(void *, size_t) = cmeta_secure_random;
static int (*volatile ref_os_name)(char *, size_t) = cmeta_platform_os_name;
static int (*volatile ref_os_version)(char *, size_t) = cmeta_platform_os_version;
static int (*volatile ref_arch)(char *, size_t) = cmeta_platform_arch;
static int (*volatile ref_username)(char *, size_t) = cmeta_platform_username;
static int (*volatile ref_hostname)(char *, size_t) = cmeta_platform_hostname;
static int (*volatile ref_cpu_info)(cmeta_platform_cpu_info_t *) =
    cmeta_platform_cpu_info;
static int (*volatile ref_memory_info)(cmeta_platform_memory_info_t *) =
    cmeta_platform_memory_info;
static int (*volatile ref_load_average)(cmeta_platform_load_average_t *) =
    cmeta_platform_load_average;
static int (*volatile ref_network_interfaces)(
    cmeta_platform_network_interface_t *, size_t, size_t *) =
    cmeta_platform_network_interfaces;
static cmeta_timer_t *(*volatile ref_timer_create)(void *) = cmeta_timer_create;
static void (*volatile ref_timer_destroy)(cmeta_timer_t *) = cmeta_timer_destroy;
static int (*volatile ref_timer_start)(cmeta_timer_t *, cmeta_timer_cb,
                                       uint64_t, uint64_t) = cmeta_timer_start;
static int (*volatile ref_timer_stop)(cmeta_timer_t *) = cmeta_timer_stop;
static void (*volatile ref_timer_set_data)(cmeta_timer_t *, void *) =
    cmeta_timer_set_data;
static void *(*volatile ref_timer_get_data)(cmeta_timer_t *) =
    cmeta_timer_get_data;
static uint64_t (*volatile ref_timer_get_repeat)(cmeta_timer_t *) =
    cmeta_timer_get_repeat;
static void (*volatile ref_single_set)(int) = cmeta_sync_set_single_threaded;
static int (*volatile ref_single_get)(void) = cmeta_sync_is_single_threaded;
static int (*volatile ref_getpid)(void) = cmeta_getpid;

int main(void) {
  cmeta_timeval_t tv = {0};
  cmeta_timezone_t tz = {0};
  struct tm utc = {0};
  char name[SALTS_PLATFORM_INFO_MAX] = {0};
  cmeta_timer_t *timer;
  int marker = 1;

  if (!ref_gettimeofday || !ref_gmtime || !ref_localtime || !ref_timegm ||
      !ref_mktime || !ref_strftime_utc || !ref_strftime_local ||
      !ref_secure_random || !ref_os_name || !ref_os_version || !ref_arch ||
      !ref_username || !ref_hostname || !ref_cpu_info || !ref_memory_info ||
      !ref_load_average || !ref_network_interfaces || !ref_timer_create ||
      !ref_timer_destroy || !ref_timer_start || !ref_timer_stop ||
      !ref_timer_set_data || !ref_timer_get_data || !ref_timer_get_repeat ||
      !ref_single_set || !ref_single_get || !ref_getpid)
    return 1;

  if (ref_gettimeofday(&tv, &tz) != 0 || tv.tv_sec <= 0) return 2;
  if (ref_gmtime((time_t)1704110400, &utc) != 0) return 3;
  if (ref_os_name(name, sizeof(name)) != 0 || name[0] == '\0') return 4;
  ref_single_set(1);
  if (ref_single_get() != 1 || ref_getpid() <= 0) return 5;
  ref_single_set(0);

  timer = ref_timer_create(NULL);
  if (!timer) return 6;
  ref_timer_set_data(timer, &marker);
  if (ref_timer_get_data(timer) != &marker || ref_timer_get_repeat(timer) != 0u)
    return 7;
  if (ref_timer_stop(timer) != 0) return 8;
  ref_timer_destroy(timer);
  return 0;
}
