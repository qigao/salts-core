#include <salts/fs_watch.h>
#include <salts/error_codes.h>

#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<cflow_fs_watch>);
static_assert(std::is_standard_layout_v<cflow_fs_watch_event>);
static_assert(std::is_standard_layout_v<cflow_fs_watch_config>);
static_assert(std::is_standard_layout_v<cflow_fs_watch_stats>);

int main() {
  auto open_fn = &cflow_fs_watch_open;
  auto run_fn = &cflow_fs_watch_run_ready;
  auto ack_fn = &cflow_fs_watch_acknowledge_rescan;
  auto close_fn = &cflow_fs_watch_close;
  auto quiet_fn = &cflow_fs_watch_is_quiescent;
  auto stats_fn = &cflow_fs_watch_get_stats;
  auto destroy_fn = &cflow_fs_watch_destroy;

  cflow_fs_watch watch{};
  std::size_t delivered = 0;
  if (!open_fn || !run_fn || !ack_fn || !close_fn || !quiet_fn ||
      !stats_fn || !destroy_fn)
    return 1;
  if (run_fn(&watch, 1, &delivered) != SALTS_EINVAL) return 2;
  if (destroy_fn(&watch) != SALTS_EINVAL) return 3;
  return 0;
}
