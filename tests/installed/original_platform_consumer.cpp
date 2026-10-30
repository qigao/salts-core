#include <salts/platform.h>
#include <salts/thread.h>

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <type_traits>

static_assert(std::is_standard_layout_v<cmeta_timeval_t>);
static_assert(std::is_standard_layout_v<cmeta_timezone_t>);
static_assert(std::is_standard_layout_v<cmeta_platform_cpu_info_t>);
static_assert(std::is_standard_layout_v<cmeta_platform_memory_info_t>);
static_assert(std::is_standard_layout_v<cmeta_platform_load_average_t>);
static_assert(std::is_standard_layout_v<cmeta_platform_network_interface_t>);

int main() {
  auto gettimeofday_fn = &cmeta_gettimeofday;
  auto gmtime_fn = &cmeta_gmtime;
  auto localtime_fn = &cmeta_localtime;
  auto timegm_fn = &cmeta_timegm;
  auto mktime_fn = &cmeta_mktime;
  auto strftime_utc_fn = &cmeta_strftime_utc;
  auto strftime_local_fn = &cmeta_strftime_local;
  auto secure_random_fn = &cmeta_secure_random;
  auto os_name_fn = &cmeta_platform_os_name;
  auto os_version_fn = &cmeta_platform_os_version;
  auto arch_fn = &cmeta_platform_arch;
  auto username_fn = &cmeta_platform_username;
  auto hostname_fn = &cmeta_platform_hostname;
  auto cpu_info_fn = &cmeta_platform_cpu_info;
  auto memory_info_fn = &cmeta_platform_memory_info;
  auto load_fn = &cmeta_platform_load_average;
  auto interfaces_fn = &cmeta_platform_network_interfaces;
  auto timer_create_fn = &cmeta_timer_create;
  auto timer_destroy_fn = &cmeta_timer_destroy;
  auto timer_start_fn = &cmeta_timer_start;
  auto timer_stop_fn = &cmeta_timer_stop;
  auto timer_set_data_fn = &cmeta_timer_set_data;
  auto timer_get_data_fn = &cmeta_timer_get_data;
  auto timer_get_repeat_fn = &cmeta_timer_get_repeat;
  auto single_set_fn = &cmeta_sync_set_single_threaded;
  auto single_get_fn = &cmeta_sync_is_single_threaded;
  auto getpid_fn = &cmeta_getpid;

  if (!gettimeofday_fn || !gmtime_fn || !localtime_fn || !timegm_fn ||
      !mktime_fn || !strftime_utc_fn || !strftime_local_fn ||
      !secure_random_fn || !os_name_fn || !os_version_fn || !arch_fn ||
      !username_fn || !hostname_fn || !cpu_info_fn || !memory_info_fn ||
      !load_fn || !interfaces_fn || !timer_create_fn || !timer_destroy_fn ||
      !timer_start_fn || !timer_stop_fn || !timer_set_data_fn ||
      !timer_get_data_fn || !timer_get_repeat_fn || !single_set_fn ||
      !single_get_fn || !getpid_fn)
    return 1;

  single_set_fn(1);
  if (single_get_fn() != 1 || getpid_fn() <= 0) return 2;
  single_set_fn(0);
  return 0;
}
