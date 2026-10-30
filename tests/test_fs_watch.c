#include <salts/fs.h>
#include <salts/fs_watch.h>
#include <salts/clock.h>
#include <salts/error_codes.h>
#include <salts/thread.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
  WATCH_EVENT_CAPACITY = 64,
  WATCH_PATH_CAPACITY = 256
};

typedef struct watch_probe {
  cflow_fs_watch_event_kind kinds[WATCH_EVENT_CAPACITY];
  char paths[WATCH_EVENT_CAPACITY][WATCH_PATH_CAPACITY];
  char old_paths[WATCH_EVENT_CAPACITY][WATCH_PATH_CAPACITY];
  size_t count;
} watch_probe;

static void watch_event(void *user, const cflow_fs_watch_event *event) {
  watch_probe *probe = (watch_probe *)user;
  if (!probe || !event || probe->count >= WATCH_EVENT_CAPACITY) return;
  probe->kinds[probe->count] = event->kind;
  if (event->path) {
    snprintf(probe->paths[probe->count], WATCH_PATH_CAPACITY, "%s", event->path);
  }
  if (event->old_path) {
    snprintf(probe->old_paths[probe->count], WATCH_PATH_CAPACITY, "%s", event->old_path);
  }
  ++probe->count;
}

static cflow_fs_watch_config make_config(watch_probe *probe, size_t capacity) {
  cflow_fs_watch_config config;
  memset(&config, 0, sizeof(config));
  config.event_capacity = capacity;
  config.watch_capacity = 16u;
  config.path_capacity = WATCH_PATH_CAPACITY;
  config.native_buffer_capacity = 4096u;
  config.event = watch_event;
  config.event_user = probe;
  return config;
}

static bool saw(const watch_probe *probe, cflow_fs_watch_event_kind kind,
                const char *path) {
  size_t i;
  for (i = 0u; i < probe->count; ++i) {
    if (probe->kinds[i] == kind &&
        (!path || strcmp(probe->paths[i], path) == 0))
      return true;
  }
  return false;
}

static size_t count_kind(const watch_probe *probe,
                         cflow_fs_watch_event_kind kind) {
  size_t i, count = 0u;
  for (i = 0u; i < probe->count; ++i)
    if (probe->kinds[i] == kind) ++count;
  return count;
}

static int drive_until(cflow_fs_watch *watch, watch_probe *probe,
                       cflow_fs_watch_event_kind kind, const char *path,
                       uint64_t timeout_ms) {
  const uint64_t deadline = cmeta_monotonic_ms() + timeout_ms;
  while (!saw(probe, kind, path)) {
    size_t delivered = 0u;
    int status = cflow_fs_watch_run_ready(watch, 8u, &delivered);
    if (status != SALTS_OK) return status;
    if (cmeta_monotonic_ms() >= deadline) return SALTS_ETIMEDOUT;
    if (delivered == 0u) cmeta_sleep_ms(1u);
  }
  return SALTS_OK;
}

static int drain_close(cflow_fs_watch *watch) {
  const uint64_t deadline = cmeta_monotonic_ms() + 5000u;
  int status = cflow_fs_watch_close(watch);
  if (status != SALTS_OK && status != SALTS_EALREADY) return status;
  while (!cflow_fs_watch_is_quiescent(watch)) {
    size_t delivered = 0u;
    status = cflow_fs_watch_run_ready(watch, 8u, &delivered);
    if (status != SALTS_OK) return status;
    if (cmeta_monotonic_ms() >= deadline) return SALTS_ETIMEDOUT;
    if (delivered == 0u) cmeta_sleep_ms(1u);
  }
  return cflow_fs_watch_destroy(watch);
}

static int join_path(char *out, size_t out_size, const char *base,
                     const char *name) {
  return salts_fs_path_join(out, out_size, base, name);
}

static int make_unique_root(char *root, size_t root_size, const char *tag) {
  char tmp[1024] = {0};
  char name[160];
  unsigned attempt;
  if (salts_fs_get_tmpdir(tmp, sizeof(tmp)) != SALTS_OK) return SALTS_EIO;
  for (attempt = 0u; attempt < 32u; ++attempt) {
    int written = snprintf(name, sizeof(name), "saltscore-watch-%s-%d-%llu-%u",
                           tag, cmeta_getpid(),
                           (unsigned long long)cmeta_monotonic_ms(), attempt);
    if (written <= 0 || (size_t)written >= sizeof(name)) return SALTS_ERANGE;
    if (join_path(root, root_size, tmp, name) != SALTS_OK) return SALTS_ERANGE;
    if (salts_fs_mkdir(root, 0700) == SALTS_OK) return SALTS_OK;
    cmeta_sleep_ms(1u);
  }
  return SALTS_EIO;
}

static int write_text(const char *path, const char *text) {
  salts_fs_buf_t buf = salts_fs_buf_init((char *)text, strlen(text));
  return salts_fs_write_file(path, &buf);
}

static int test_create_rename_remove(void) {
  char root[1400], first[1600], second[1600];
  cflow_fs_watch watch = {0};
  watch_probe probe = {0};
  cflow_fs_watch_config config = make_config(&probe, 16u);
  int status;

  if (make_unique_root(root, sizeof(root), "basic") != SALTS_OK) return 10;
  if (join_path(first, sizeof(first), root, "first.txt") != SALTS_OK ||
      join_path(second, sizeof(second), root, "second.txt") != SALTS_OK)
    return 11;
  if (cflow_fs_watch_open(NULL, root, &config) != SALTS_EINVAL) return 12;
  if (cflow_fs_watch_open(&watch, root, &config) != SALTS_OK) return 13;

  if (write_text(first, "value") != SALTS_OK) return 14;
  {
    const uint64_t deadline = cmeta_monotonic_ms() + 5000u;
    while (!saw(&probe, CFLOW_FS_WATCH_CREATED, "first.txt") &&
           !saw(&probe, CFLOW_FS_WATCH_MODIFIED, "first.txt")) {
      size_t delivered = 0u;
      if (cflow_fs_watch_run_ready(&watch, 8u, &delivered) != SALTS_OK) return 15;
      if (cmeta_monotonic_ms() >= deadline) return 16;
      if (delivered == 0u) cmeta_sleep_ms(1u);
    }
  }

  if (salts_fs_rename(first, second) != SALTS_OK) return 17;
#if defined(__APPLE__) && !defined(__ANDROID__)
  status = drive_until(&watch, &probe, CFLOW_FS_WATCH_RESCAN_REQUIRED,
                       NULL, 5000u);
  if (status != SALTS_OK) return 18;
  if (cflow_fs_watch_acknowledge_rescan(&watch) != SALTS_OK) return 19;
#else
  status = drive_until(&watch, &probe, CFLOW_FS_WATCH_RENAMED,
                       "second.txt", 5000u);
  if (status != SALTS_OK) return 20;
#endif

  if (salts_fs_unlink(second) != SALTS_OK) return 21;
#if defined(__APPLE__) && !defined(__ANDROID__)
  {
    const size_t before = count_kind(&probe, CFLOW_FS_WATCH_RESCAN_REQUIRED);
    const uint64_t deadline = cmeta_monotonic_ms() + 5000u;
    while (!saw(&probe, CFLOW_FS_WATCH_REMOVED, "second.txt") &&
           count_kind(&probe, CFLOW_FS_WATCH_RESCAN_REQUIRED) == before) {
      size_t delivered = 0u;
      if (cflow_fs_watch_run_ready(&watch, 8u, &delivered) != SALTS_OK) return 22;
      if (cmeta_monotonic_ms() >= deadline) return 23;
      if (delivered == 0u) cmeta_sleep_ms(1u);
    }
    if (count_kind(&probe, CFLOW_FS_WATCH_RESCAN_REQUIRED) > before &&
        cflow_fs_watch_acknowledge_rescan(&watch) != SALTS_OK)
      return 24;
  }
#else
  if (drive_until(&watch, &probe, CFLOW_FS_WATCH_REMOVED,
                  "second.txt", 5000u) != SALTS_OK)
    return 25;
#endif

  if (drain_close(&watch) != SALTS_OK || watch.impl != NULL) return 26;
  if (salts_fs_rmdir(root) != SALTS_OK) return 27;
  return 0;
}

static int test_bounded_loss(void) {
  char root[1400], one[1600], two[1600], three[1600];
  cflow_fs_watch watch = {0};
  watch_probe probe = {0};
  cflow_fs_watch_config config = make_config(&probe, 1u);
  cflow_fs_watch_stats stats = {0};
  uint64_t deadline;

  if (make_unique_root(root, sizeof(root), "loss") != SALTS_OK) return 30;
  if (join_path(one, sizeof(one), root, "one.txt") != SALTS_OK ||
      join_path(two, sizeof(two), root, "two.txt") != SALTS_OK ||
      join_path(three, sizeof(three), root, "three.txt") != SALTS_OK)
    return 31;
  if (cflow_fs_watch_open(&watch, root, &config) != SALTS_OK) return 32;
  if (write_text(one, "1") != SALTS_OK || write_text(two, "2") != SALTS_OK)
    return 33;

  deadline = cmeta_monotonic_ms() + 5000u;
  do {
    if (!cflow_fs_watch_get_stats(&watch, &stats)) return 34;
    if (stats.awaiting_rescan) break;
    if (cmeta_monotonic_ms() >= deadline) return 35;
    cmeta_sleep_ms(1u);
  } while (true);

  if (drive_until(&watch, &probe, CFLOW_FS_WATCH_RESCAN_REQUIRED,
                  NULL, 5000u) != SALTS_OK)
    return 36;
  {
    const size_t suppressed = stats.suppressed;
    if (write_text(three, "3") != SALTS_OK) return 37;
    deadline = cmeta_monotonic_ms() + 5000u;
    do {
      if (!cflow_fs_watch_get_stats(&watch, &stats)) return 38;
      if (stats.suppressed > suppressed) break;
      if (cmeta_monotonic_ms() >= deadline) return 39;
      cmeta_sleep_ms(1u);
    } while (true);
  }

  if (cflow_fs_watch_acknowledge_rescan(&watch) != SALTS_OK) return 40;
  if (!cflow_fs_watch_get_stats(&watch, &stats) || !stats.awaiting_rescan)
    return 41;
  {
    const size_t markers = count_kind(&probe, CFLOW_FS_WATCH_RESCAN_REQUIRED);
    deadline = cmeta_monotonic_ms() + 5000u;
    while (count_kind(&probe, CFLOW_FS_WATCH_RESCAN_REQUIRED) == markers) {
      size_t delivered = 0u;
      if (cflow_fs_watch_run_ready(&watch, 8u, &delivered) != SALTS_OK) return 42;
      if (cmeta_monotonic_ms() >= deadline) return 43;
      if (delivered == 0u) cmeta_sleep_ms(1u);
    }
  }
  if (cflow_fs_watch_acknowledge_rescan(&watch) != SALTS_OK) return 44;

  if (drain_close(&watch) != SALTS_OK) return 45;
  (void)salts_fs_unlink(one);
  (void)salts_fs_unlink(two);
  (void)salts_fs_unlink(three);
  if (salts_fs_rmdir(root) != SALTS_OK) return 46;
  return 0;
}

static int test_root_change(void) {
  char root[1400];
  cflow_fs_watch watch = {0};
  watch_probe probe = {0};
  cflow_fs_watch_config config = make_config(&probe, 8u);

  if (make_unique_root(root, sizeof(root), "root") != SALTS_OK) return 50;
  if (cflow_fs_watch_open(&watch, root, &config) != SALTS_OK) return 51;
  if (salts_fs_rmdir(root) != SALTS_OK) return 52;
  if (drive_until(&watch, &probe, CFLOW_FS_WATCH_ROOT_CHANGED,
                  NULL, 5000u) != SALTS_OK)
    return 53;
  if (drain_close(&watch) != SALTS_OK) return 54;
  return 0;
}

static int test_recursive(void) {
  char root[1400], nested[1600], child[1800], dynamic[1600], dynamic_child[1800];
  cflow_fs_watch watch = {0};
  watch_probe probe = {0};
  cflow_fs_watch_config config = make_config(&probe, 16u);

  config.recursive = true;
  if (make_unique_root(root, sizeof(root), "recursive") != SALTS_OK) return 60;
  if (join_path(nested, sizeof(nested), root, "nested") != SALTS_OK ||
      join_path(child, sizeof(child), nested, "child.txt") != SALTS_OK ||
      join_path(dynamic, sizeof(dynamic), root, "dynamic") != SALTS_OK ||
      join_path(dynamic_child, sizeof(dynamic_child), dynamic, "later.txt") != SALTS_OK)
    return 61;
  if (salts_fs_mkdir(nested, 0700) != SALTS_OK) return 62;
  if (cflow_fs_watch_open(&watch, root, &config) != SALTS_OK) return 63;
  if (write_text(child, "x") != SALTS_OK) return 64;
  if (drive_until(&watch, &probe, CFLOW_FS_WATCH_CREATED,
                  "nested/child.txt", 5000u) != SALTS_OK)
    return 65;

  if (salts_fs_mkdir(dynamic, 0700) != SALTS_OK) return 66;
  if (drive_until(&watch, &probe, CFLOW_FS_WATCH_CREATED,
                  "dynamic", 5000u) != SALTS_OK)
    return 67;
  if (write_text(dynamic_child, "y") != SALTS_OK) return 68;
  if (drive_until(&watch, &probe, CFLOW_FS_WATCH_CREATED,
                  "dynamic/later.txt", 5000u) != SALTS_OK)
    return 69;

  if (drain_close(&watch) != SALTS_OK) return 70;
  (void)salts_fs_unlink(child);
  (void)salts_fs_unlink(dynamic_child);
  (void)salts_fs_rmdir(nested);
  (void)salts_fs_rmdir(dynamic);
  if (salts_fs_rmdir(root) != SALTS_OK) return 71;
  return 0;
}

int main(void) {
#if defined(_WIN32) || defined(__linux__) ||     (defined(__APPLE__) && !defined(TARGET_OS_IPHONE))
  int status = test_create_rename_remove();
  if (status != 0) return status;
  status = test_bounded_loss();
  if (status != 0) return status;
  status = test_root_change();
  if (status != 0) return status;
  status = test_recursive();
  if (status != 0) return status;
#else
  cflow_fs_watch watch = {0};
  watch_probe probe = {0};
  cflow_fs_watch_config config = make_config(&probe, 1u);
  if (cflow_fs_watch_open(&watch, ".", &config) != SALTS_ENOTSUP ||
      watch.impl != NULL)
    return 80;
#endif
  puts("Salts::FS callback-only native watch contract PASS");
  return 0;
}
