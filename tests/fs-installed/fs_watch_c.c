#include <salts/fs_watch.h>
#include <salts/error_codes.h>

#include <stddef.h>

static void event_cb(void *user, const cflow_fs_watch_event *event) {
  (void)user;
  (void)event;
}

int main(void) {
  cflow_fs_watch watch = {0};
  cflow_fs_watch_config config = {0};
  cflow_fs_watch_stats stats = {0};
  size_t delivered = 99u;

  int (*open_fn)(cflow_fs_watch *, const char *, const cflow_fs_watch_config *) =
      cflow_fs_watch_open;
  int (*run_fn)(cflow_fs_watch *, size_t, size_t *) =
      cflow_fs_watch_run_ready;
  int (*ack_fn)(cflow_fs_watch *) = cflow_fs_watch_acknowledge_rescan;
  int (*close_fn)(cflow_fs_watch *) = cflow_fs_watch_close;
  bool (*quiet_fn)(const cflow_fs_watch *) = cflow_fs_watch_is_quiescent;
  bool (*stats_fn)(const cflow_fs_watch *, cflow_fs_watch_stats *) =
      cflow_fs_watch_get_stats;
  int (*destroy_fn)(cflow_fs_watch *) = cflow_fs_watch_destroy;

  if (!open_fn || !run_fn || !ack_fn || !close_fn || !quiet_fn ||
      !stats_fn || !destroy_fn)
    return 1;

  config.event_capacity = 1u;
  config.watch_capacity = 1u;
  config.path_capacity = 16u;
  config.native_buffer_capacity = 1024u;
  config.event = event_cb;

  if (open_fn(NULL, ".", &config) != SALTS_EINVAL) return 2;
  if (run_fn(&watch, 1u, &delivered) != SALTS_EINVAL || delivered != 99u)
    return 3;
  if (ack_fn(&watch) != SALTS_EINVAL) return 4;
  if (close_fn(&watch) != SALTS_EINVAL) return 5;
  if (quiet_fn(&watch)) return 6;
  if (stats_fn(&watch, &stats)) return 7;
  if (destroy_fn(&watch) != SALTS_EINVAL) return 8;
  return 0;
}
