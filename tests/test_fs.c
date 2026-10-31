#include <salts/fs.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
/* No new project, workflow, installer or consumer. Exercise real OS
 * namespace/descriptor operations in the existing Salts::FS native CTest.
 * This M1/M2 test adds native namespace-only no-replace on Windows too,
 * but does NOT validate monotonic generation allocation, distributed leases,
 * process crashes, or Windows durable publication. */
static int stage_revision_file(
    salts_fs_root_t *root, const char *name,
    const char *data, size_t bytes) {
  salts_fs_root_file_t *staged = NULL;
  int rc = salts_fs_root_file_open(root, name,
      SALTS_FS_O_WRONLY | SALTS_FS_O_CREAT | SALTS_FS_ROOT_O_EXCL,
      0600, &staged);
  if (rc) return rc;
  rc = salts_fs_root_file_write(staged, data, bytes);
  if (rc == (int)bytes) rc = salts_fs_root_file_fsync(staged);
  else if (rc >= 0) rc = -EIO;
  {
    int close_rc = salts_fs_root_file_close(staged);
    if (rc == 0) rc = close_rc;
  }
  return rc;
}


#ifndef _WIN32
/* A distinct child process pins the old exact root-relative A1 inode before
 * the parent unlinks its name. This tests real OS descriptor retention, not
 * a simulated in-memory read or a second filesystem implementation. */
static int test_child_pinned_revision(
    salts_fs_root_t *root, const char *revision, const char *expected,
    size_t expected_size) {
  int ready[2] = {-1, -1}, release[2] = {-1, -1};
  pid_t child;
  char signal = 0;
  int status = 0, rc = 0;
  if (pipe(ready) != 0) return 201;
  if (pipe(release) != 0) {
    close(ready[0]); close(ready[1]);
    return 202;
  }
  child = fork();
  if (child < 0) {
    close(ready[0]); close(ready[1]);
    close(release[0]); close(release[1]);
    return 203;
  }
  if (child == 0) {
    salts_fs_root_file_t *reader = NULL;
    char bytes[128] = {0};
    int count;
    close(ready[0]);
    close(release[1]);
    if (expected_size > sizeof(bytes) ||
        salts_fs_root_file_open(root, revision, SALTS_FS_O_RDONLY,
                                0, &reader) != 0 || !reader)
      _exit(4);
    if (write(ready[1], "R", 1) != 1) _exit(5);
    close(ready[1]);
    if (read(release[0], &signal, 1) != 1 || signal != 'G') _exit(6);
    close(release[0]);
    count = salts_fs_root_file_read(reader, bytes, expected_size);
    if (salts_fs_root_file_close(reader) != 0 ||
        count != (int)expected_size ||
        memcmp(bytes, expected, expected_size) != 0)
      _exit(7);
    _exit(0);
  }
  close(ready[1]);
  close(release[0]);
  /* If child fails before pinning, read reaches EOF; never unlink first. */
  if (read(ready[0], &signal, 1) != 1 || signal != 'R') {
    rc = 204;
  } else if (salts_fs_root_unlink(root, revision) != 0) {
    rc = 205;
  }
  close(ready[0]);
  if (rc == 0 && write(release[1], "G", 1) != 1)
    rc = 206;
  close(release[1]);
  if (waitpid(child, &status, 0) != child)
    return 207;
  if (rc != 0) return rc;
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 208;
  return 0;
}

/* Two actual child processes simultaneously attempt the exact same absent
 * destination. Native RENAME_NOREPLACE / RENAME_EXCL must give exactly one
 * durable success and one EEXIST, even though both sources were prepared.
 * The losing stage remains intact. */
static int test_concurrent_noreplace(
    salts_fs_root_t *root, unsigned long pid) {
  char stage[2][96], target[96], observed[64] = {0}, token = 0;
  const char first[] = "parallel-version-A";
  const char second[] = "parallel-version-B";
  int start[2] = {-1, -1};
  pid_t children[2];
  int status[2] = {0, 0};
  int success = 0, conflict = 0;
  salts_fs_root_file_t *reader = NULL;
  salts_fs_stat_t info;
  int count;
  for (int i = 0; i < 2; ++i) {
    if (snprintf(stage[i], sizeof(stage[i]), "fs-race-%lu-stage-%d",
                 pid, i) <= 0)
      return 220;
  }
  if (snprintf(target, sizeof(target), "fs-race-%lu-revision", pid) <= 0)
    return 221;
  if (stage_revision_file(root, stage[0], first, sizeof(first)-1u) != 0 ||
      stage_revision_file(root, stage[1], second, sizeof(second)-1u) != 0)
    return 222;
  if (pipe(start) != 0) return 223;
  for (int i = 0; i < 2; ++i) {
    children[i] = fork();
    if (children[i] < 0) {
      close(start[0]);
      /* Release the already-forked child, then reap it. */
      if (i == 1) {
        (void)write(start[1], "X", 1);
        (void)waitpid(children[0], &status[0], 0);
      }
      close(start[1]);
      return 224;
    }
    if (children[i] == 0) {
      salts_fs_replace_state_t result = SALTS_FS_REPLACE_NOT_PUBLISHED;
      int rc;
      close(start[1]);
      if (read(start[0], &token, 1) != 1) _exit(10);
      close(start[0]);
      rc = salts_fs_root_publish_file_noreplace(
          root, stage[i], target, &result);
      if (rc == 0 && result == SALTS_FS_REPLACE_PUBLISHED_DURABLE)
        _exit(0);
      if (rc == -EEXIST && result == SALTS_FS_REPLACE_NOT_PUBLISHED)
        _exit(1);
      _exit(11);
    }
  }
  close(start[0]);
  if (write(start[1], "XX", 2) != 2) {
    close(start[1]);
    (void)waitpid(children[0], &status[0], 0);
    (void)waitpid(children[1], &status[1], 0);
    return 225;
  }
  close(start[1]);
  for (int i = 0; i < 2; ++i) {
    if (waitpid(children[i], &status[i], 0) != children[i] ||
        !WIFEXITED(status[i]))
      return 226;
    if (WEXITSTATUS(status[i]) == 0) ++success;
    else if (WEXITSTATUS(status[i]) == 1) ++conflict;
    else return 227;
  }
  if (success != 1 || conflict != 1)
    return 228;
  if (salts_fs_root_file_open(root, target, SALTS_FS_O_RDONLY,
                              0, &reader) != 0 || !reader)
    return 229;
  count = salts_fs_root_file_read(reader, observed, sizeof(first)-1u);
  if (salts_fs_root_file_close(reader) != 0 ||
      count != (int)(sizeof(first)-1u) ||
      (memcmp(observed, first, sizeof(first)-1u) != 0 &&
       memcmp(observed, second, sizeof(second)-1u) != 0))
    return 230;
  for (int i = 0; i < 2; ++i) {
    if (salts_fs_root_stat(root, stage[i], &info) == 0) {
      if (salts_fs_root_unlink(root, stage[i]) != 0)
        return 231;
    } else if (salts_fs_root_stat(root, stage[i], &info) != -ENOENT) {
      return 232;
    }
  }
  if (salts_fs_root_unlink(root, target) != 0) return 233;
  return 0;
}
#endif /* !_WIN32 */



#ifdef _WIN32
/* Two separate, normally launched Windows processes using independently
 * opened Salts::FS root capabilities and one kernel event to race the same
 * ABSENT destination. This is namespace arbitration, NOT crash durability. */
static int win_native_child_move(unsigned long owner, int slot) {
  char tmp[1024], stage[96], destination[96];
  wchar_t event_name[96];
  HANDLE start;
  salts_fs_root_t *root = NULL;
  int rc;
  if (slot < 0 || slot > 1 ||
      snprintf(stage, sizeof(stage), "fs-win-par-%lu-stage-%d",
               owner, slot) <= 0 ||
      snprintf(destination, sizeof(destination),
               "fs-win-par-%lu-dest", owner) <= 0 ||
      swprintf(event_name, sizeof(event_name)/sizeof(event_name[0]),
               L"Local\\SaltsFSRace-%lu", owner) < 0)
    return 40;
  if (salts_fs_get_tmpdir(tmp, sizeof(tmp)) != 0 ||
      salts_fs_root_open(tmp, &root) != 0 || !root)
    return 41;
  start = OpenEventW(SYNCHRONIZE, FALSE, event_name);
  if (!start) {
    (void)salts_fs_root_close(root);
    return 42;
  }
  if (WaitForSingleObject(start, 10000u) != WAIT_OBJECT_0) {
    CloseHandle(start);
    (void)salts_fs_root_close(root);
    return 43;
  }
  CloseHandle(start);
  rc = salts_fs_root_rename_file_noreplace(root, stage, destination);
  if (salts_fs_root_close(root) != 0)
    return 44;
  if (rc == 0) return 0;
  if (rc == -EEXIST) return 1;
  return 45;
}

static int win_native_two_process_race(
    salts_fs_root_t *root, unsigned long owner) {
  char stage[2][96], target[96], observed[64] = {0};
  const char a[] = "Windows-version-A";
  const char b[] = "Windows-version-B";
  wchar_t event_name[96], executable[32768];
  HANDLE begin = NULL, processes[2] = {NULL, NULL};
  DWORD codes[2] = {99, 99};
  DWORD exe_length;
  salts_fs_root_file_t *reader = NULL;
  salts_fs_stat_t statbuf;
  int rc;
  if (snprintf(stage[0], sizeof(stage[0]),
               "fs-win-par-%lu-stage-0", owner) <= 0 ||
      snprintf(stage[1], sizeof(stage[1]),
               "fs-win-par-%lu-stage-1", owner) <= 0 ||
      snprintf(target, sizeof(target),
               "fs-win-par-%lu-dest", owner) <= 0 ||
      swprintf(event_name, sizeof(event_name)/sizeof(event_name[0]),
               L"Local\\SaltsFSRace-%lu", owner) < 0)
    return 50;
  if (stage_revision_file(root, stage[0], a, sizeof(a)-1u) != 0 ||
      stage_revision_file(root, stage[1], b, sizeof(b)-1u) != 0)
    return 51;
  exe_length = GetModuleFileNameW(
      NULL, executable, (DWORD)(sizeof(executable)/sizeof(executable[0])));
  if (exe_length == 0 ||
      exe_length >= sizeof(executable)/sizeof(executable[0]))
    return 52;
  begin = CreateEventW(NULL, TRUE, FALSE, event_name);
  if (!begin || GetLastError() == ERROR_ALREADY_EXISTS) {
    if (begin) CloseHandle(begin);
    return 53;
  }
  for (int i = 0; i < 2; ++i) {
    wchar_t cmd[33000];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION pi = {0};
    int n = swprintf(cmd, sizeof(cmd)/sizeof(cmd[0]),
                     L"\"%ls\" --fs-native-race-child %lu %d",
                     executable, owner, i);
    if (n < 0 || n >= (int)(sizeof(cmd)/sizeof(cmd[0]))) {
      rc = 54; goto fail;
    }
    startup.cb = sizeof(startup);
    if (!CreateProcessW(executable, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &startup, &pi)) {
      rc = 55; goto fail;
    }
    processes[i] = pi.hProcess;
    CloseHandle(pi.hThread);
  }
  if (!SetEvent(begin)) { rc = 56; goto fail; }
  for (int i = 0; i < 2; ++i) {
    if (WaitForSingleObject(processes[i], 10000u) != WAIT_OBJECT_0 ||
        !GetExitCodeProcess(processes[i], &codes[i])) {
      rc = 57; goto fail;
    }
  }
  if (!((codes[0] == 0u && codes[1] == 1u) ||
        (codes[0] == 1u && codes[1] == 0u))) {
    fprintf(stderr, "Windows two-process no-replace codes=%lu,%lu\n",
            (unsigned long)codes[0], (unsigned long)codes[1]);
    rc = 58; goto fail;
  }
  if (salts_fs_root_file_open(root, target, SALTS_FS_O_RDONLY,
                              0, &reader) != 0 || !reader) {
    rc = 59; goto fail;
  }
  rc = salts_fs_root_file_read(reader, observed, sizeof(a)-1u);
  if (salts_fs_root_file_close(reader) != 0 ||
      rc != (int)(sizeof(a)-1u) ||
      (memcmp(observed, a, sizeof(a)-1u) != 0 &&
       memcmp(observed, b, sizeof(b)-1u) != 0)) {
    reader = NULL; rc = 60; goto fail;
  }
  reader = NULL;
  for (int i = 0; i < 2; ++i) {
    const int present = salts_fs_root_stat(root, stage[i], &statbuf);
    if (codes[i] == 0u && present != -ENOENT) {
      rc = 61; goto fail;
    }
    if (codes[i] == 1u && present != 0) {
      rc = 62; goto fail;
    }
    if (codes[i] == 1u && salts_fs_root_unlink(root, stage[i]) != 0) {
      rc = 63; goto fail;
    }
  }
  if (salts_fs_root_unlink(root, target) != 0) {
    rc = 64; goto fail;
  }
  for (int i = 0; i < 2; ++i) CloseHandle(processes[i]);
  CloseHandle(begin);
  puts("Salts::FS Windows two independent processes: one NOREPLACE winner PASS (not durable)");
  return 0;
fail:
  /* Never leave a child blocked on the barrier if the second spawn fails. */
  (void)SetEvent(begin);
  for (int i = 0; i < 2; ++i) {
    if (processes[i]) {
      if (WaitForSingleObject(processes[i], 10000u) != WAIT_OBJECT_0)
        (void)TerminateProcess(processes[i], 9u);
      CloseHandle(processes[i]);
    }
  }
  if (begin) CloseHandle(begin);
  return rc;
}
#endif

/* Namespace-only no-replace is portable on the real native FS but explicitly
 * NOT crash durable. Especially on Windows, this cannot replace the separate
 * ENOSYS/NOT_PUBLISHED durable publisher gate. */
static int test_native_namespace_noreplace(
    salts_fs_root_t *root, unsigned long pid) {
  char first[100], contender[100], target[120];
  const char content1[] = "noreplace-first";
  const char content2[] = "noreplace-other";
  char result[sizeof(content1)] = {0};
  salts_fs_root_file_t *file = NULL;
  salts_fs_stat_t info;
  int rc;
  if (snprintf(first, sizeof(first), "fs-namespace-%lu-stage1", pid) <= 0 ||
      snprintf(contender, sizeof(contender), "fs-namespace-%lu-stage2", pid) <= 0 ||
      snprintf(target, sizeof(target),
          "fs-namespace-%lu-\xE7\x89\x88\xE6\x9C\xAC-A1", pid) <= 0)
    return 301;
  if (stage_revision_file(root, first, content1, sizeof(content1)-1u) != 0 ||
      stage_revision_file(root, contender, content2, sizeof(content2)-1u) != 0)
    return 302;
  rc = salts_fs_root_rename_file_noreplace(root, "../escape", target);
  if (rc != -EINVAL) return 303;
  rc = salts_fs_root_rename_file_noreplace(root, first, first);
  if (rc != -EINVAL) return 304;
  rc = salts_fs_root_rename_file_noreplace(root, first, target);
  if (rc != 0) {
    fprintf(stderr, "native no-replace namespace move failed rc=%d\n", rc);
    return 305;
  }
  if (salts_fs_root_stat(root, first, &info) != -ENOENT ||
      salts_fs_root_stat(root, target, &info) != 0 || !info.is_file)
    return 306;
  rc = salts_fs_root_rename_file_noreplace(root, contender, target);
  if (rc != -EEXIST ||
      salts_fs_root_stat(root, contender, &info) != 0)
    return 307;
  if (salts_fs_root_file_open(root, target, SALTS_FS_O_RDONLY,
                              0, &file) != 0 || !file)
    return 308;
  rc = salts_fs_root_file_read(file, result, sizeof(content1)-1u);
  if (salts_fs_root_file_close(file) != 0 ||
      rc != (int)(sizeof(content1)-1u) ||
      memcmp(result, content1, sizeof(content1)-1u) != 0)
    return 309;
  if (salts_fs_root_unlink(root, target) != 0 ||
      salts_fs_root_unlink(root, contender) != 0)
    return 310;
#ifdef _WIN32
  rc = win_native_two_process_race(root, pid);
  if (rc != 0) return rc;
#endif
  puts("Salts::FS native NOREPLACE namespace + UTF-8 target + collision PASS (not durable)");
  return 0;
}

static int test_immutable_revision_file(salts_fs_root_t *root,
                                         unsigned long pid) {
  char stage1[100], stage2[100], stage3[100], collision[100];
  char version1[100], version2[100], version3[100];
  const char a[] = "same publication bytes";
  const char b[] = "different publication";
  char scratch[sizeof(a)] = {0};
  salts_fs_root_file_t *pin_a = NULL;
  salts_fs_replace_state_t state;
  salts_fs_stat_t statbuf;
  int rc;
  if (snprintf(stage1, sizeof(stage1), "fs-m0-%lu-stage1", pid) <= 0 ||
      snprintf(stage2, sizeof(stage2), "fs-m0-%lu-stage2", pid) <= 0 ||
      snprintf(stage3, sizeof(stage3), "fs-m0-%lu-stage3", pid) <= 0 ||
      snprintf(collision, sizeof(collision), "fs-m0-%lu-collision", pid) <= 0 ||
      snprintf(version1, sizeof(version1), "fs-m0-%lu-A1", pid) <= 0 ||
      snprintf(version2, sizeof(version2), "fs-m0-%lu-B2", pid) <= 0 ||
      snprintf(version3, sizeof(version3), "fs-m0-%lu-A3", pid) <= 0)
    return 101;
  if (test_native_namespace_noreplace(root, pid) != 0) return 100;
  if (stage_revision_file(root, stage1, a, sizeof(a) - 1u)) return 102;
  state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
  rc = salts_fs_root_publish_file_noreplace(root, "../escape", version1, &state);
  if (rc != -EINVAL || state != SALTS_FS_REPLACE_NOT_PUBLISHED) return 103;
  rc = salts_fs_root_publish_file_noreplace(root, stage1, stage1, &state);
  if (rc != -EINVAL || state != SALTS_FS_REPLACE_NOT_PUBLISHED) return 104;

#ifdef _WIN32
  rc = salts_fs_root_publish_file_noreplace(root, stage1, version1, &state);
  if (rc != -ENOSYS || state != SALTS_FS_REPLACE_NOT_PUBLISHED ||
      salts_fs_root_stat(root, stage1, &statbuf) != 0 ||
      salts_fs_root_stat(root, version1, &statbuf) == 0)
    return 105;
  if (salts_fs_root_unlink(root, stage1)) return 106;
  puts("Salts::FS immutable file publish UNSUPPORTED on Windows until durable native qualification");
  return 0;
#else
  rc = salts_fs_root_publish_file_noreplace(root, stage1, version1, &state);
  if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
    return 107;
  if (salts_fs_root_file_open(root, version1, SALTS_FS_O_RDONLY,
                              0, &pin_a) || !pin_a) return 108;
  /* The immutable A1 pin is held across B2 + A3 publications and unlink. */
  if (stage_revision_file(root, stage2, b, sizeof(b) - 1u)) return 109;
  rc = salts_fs_root_publish_file_noreplace(root, stage2, version2, &state);
  if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE) return 110;
  if (stage_revision_file(root, stage3, a, sizeof(a) - 1u)) return 111;
  rc = salts_fs_root_publish_file_noreplace(root, stage3, version3, &state);
  if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE) return 112;
  if (stage_revision_file(root, collision, b, sizeof(b) - 1u)) return 113;
  state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
  rc = salts_fs_root_publish_file_noreplace(root, collision, version1, &state);
  if (rc != -EEXIST || state != SALTS_FS_REPLACE_NOT_PUBLISHED ||
      salts_fs_root_stat(root, collision, &statbuf) != 0)
    return 114;
  if (salts_fs_root_unlink(root, collision)) return 115;
  if (test_child_pinned_revision(root, version1, a, sizeof(a)-1u))
    return 116;
  if (salts_fs_root_file_seek(pin_a, 0, SEEK_SET) != 0 ||
      salts_fs_root_file_read(pin_a, scratch, sizeof(a) - 1u) !=
          (int)(sizeof(a) - 1u) ||
      memcmp(scratch, a, sizeof(a) - 1u) != 0)
    return 117;
  if (salts_fs_root_file_close(pin_a)) return 118;
  pin_a = NULL;
  if (salts_fs_root_file_open(root, version3, SALTS_FS_O_RDONLY,
                              0, &pin_a) || !pin_a) return 119;
  memset(scratch, 0, sizeof(scratch));
  if (salts_fs_root_file_read(pin_a, scratch, sizeof(a) - 1u) !=
          (int)(sizeof(a) - 1u) ||
      memcmp(scratch, a, sizeof(a) - 1u) != 0)
    return 120;
  if (salts_fs_root_file_close(pin_a)) return 121;
  if (salts_fs_root_unlink(root, version2) ||
      salts_fs_root_unlink(root, version3))
    return 122;
  if (test_concurrent_noreplace(root, pid) != 0) return 123;
  puts("Salts::FS native durable A1/B2/A3 + child pin + 2-process competing no-replace PASS");
  return 0;
#endif
}


#if !defined(_WIN32) && defined(SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS)
/* Test-only symbol from the SAME original FS shared library, compiled only
 * for BUILD_TESTING on POSIX. No installed public header or second engine. */
extern int salts_fs_test_set_publish_fail_stage(int stage);

static int test_durable_file_publish_failpoints(
    salts_fs_root_t *root, unsigned long pid) {
  static const char original[] = "immutable-commit-revision";
  for (int step = 1; step <= 4; ++step) {
    char stage_path[96], revision_path[96];
    salts_fs_replace_state_t state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
    salts_fs_stat_t staged = {0}, published = {0};
    salts_fs_root_file_t *reader = NULL;
    char actual[sizeof(original)] = {0};
    int rc, src_stat, dst_stat;
    if (snprintf(stage_path, sizeof(stage_path),
                 "fs-fault-%lu-stage-%d", pid, step) <= 0 ||
        snprintf(revision_path, sizeof(revision_path),
                 "fs-fault-%lu-revision-%d", pid, step) <= 0)
      return 401;
    if (stage_revision_file(root, stage_path,
                            original, sizeof(original)-1u) != 0)
      return 402;
    if (salts_fs_test_set_publish_fail_stage(step) != 0)
      return 403;
    rc = salts_fs_root_publish_file_noreplace(
        root, stage_path, revision_path, &state);
    if (salts_fs_test_set_publish_fail_stage(0) != 0)
      return 404;
    if (rc != -EIO)
      return 405;
    if (state != (step < 3 ? SALTS_FS_REPLACE_NOT_PUBLISHED :
                              SALTS_FS_REPLACE_DURABILITY_UNKNOWN))
      return 406;
    src_stat = salts_fs_root_stat(root, stage_path, &staged);
    dst_stat = salts_fs_root_stat(root, revision_path, &published);
    if (step < 3) {
      /* Only a confirmed non-publication permits retry. */
      if (src_stat != 0 || dst_stat != -ENOENT)
        return 407;
      state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
      rc = salts_fs_root_publish_file_noreplace(
          root, stage_path, revision_path, &state);
      if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
        return 408;
    } else {
      /* Post-rename error MUST NOT masquerade as NOT_PUBLISHED.
       * Reconcile by named destination bytes, never retry the old stage. */
      if (src_stat != -ENOENT || dst_stat != 0 ||
          !published.is_file ||
          published.size != sizeof(original)-1u)
        return 409;
      state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
      rc = salts_fs_root_publish_file_noreplace(
          root, stage_path, revision_path, &state);
      if (rc != -ENOENT || state != SALTS_FS_REPLACE_NOT_PUBLISHED)
        return 410;
    }
    if (salts_fs_root_file_open(root, revision_path, SALTS_FS_O_RDONLY,
                                0, &reader) != 0 || !reader)
      return 411;
    rc = salts_fs_root_file_read(
        reader, actual, sizeof(original)-1u);
    if (salts_fs_root_file_close(reader) != 0 ||
        rc != (int)(sizeof(original)-1u) ||
        memcmp(actual, original, sizeof(original)-1u) != 0)
      return 412;
    if (salts_fs_root_unlink(root, revision_path) != 0)
      return 413;
  }
  puts("Salts::FS POSIX publish failpoints: NOT_PUBLISHED vs DURABILITY_UNKNOWN/reconcile PASS");
  return 0;
}
#endif


#if !defined(_WIN32) && defined(SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS)
/* Unlike the above -EIO return tests, the actual publisher child is
 * SIGKILLed in kernel-visible native FS steps. The parent did not
 * receive a return value; it must reconcile via version-specific paths.
 * This is a PROCESS CRASH test, NOT a power-loss durability oracle. */
static int test_durable_publish_sigkill_recovery(
    salts_fs_root_t *root, unsigned long pid) {
  static const char bytes[] = "hard-killed-publisher-content";
  for (int stage = 1; stage <= 4; ++stage) {
    char source[96], destination[96], actual[sizeof(bytes)] = {0};
    salts_fs_replace_state_t state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
    salts_fs_root_file_t *file = NULL;
    salts_fs_stat_t info = {0};
    pid_t child;
    int status = 0, rc, source_status, target_status;
    if (snprintf(source, sizeof(source),
                 "fs-kill-%lu-stage-%d", pid, stage) <= 0 ||
        snprintf(destination, sizeof(destination),
                 "fs-kill-%lu-revision-%d", pid, stage) <= 0)
      return 501;
    if (stage_revision_file(root, source, bytes, sizeof(bytes)-1u) != 0)
      return 502;
    child = fork();
    if (child < 0) return 503;
    if (child == 0) {
      if (salts_fs_test_set_publish_fail_stage(-stage) != 0)
        _exit(241);
      (void)salts_fs_root_publish_file_noreplace(
          root, source, destination, &state);
      /* A compiled-in hook MUST terminate via SIGKILL, never return. */
      _exit(242);
    }
    if (waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      return 504;
    source_status = salts_fs_root_stat(root, source, &info);
    target_status = salts_fs_root_stat(root, destination, &info);
    if (stage <= 2) {
      /* No native move was reached; staging is still present. A parent
       * may explicitly resubmit only after reconciling the old request. */
      if (source_status != 0 || target_status != -ENOENT)
        return 505;
      rc = salts_fs_root_publish_file_noreplace(
          root, source, destination, &state);
      if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
        return 506;
    } else {
      /* Publication may already be visible, despite the loss of the
       * child's return status. Do NOT reallocate/retry a generation.
       * Reconcile exact destination bytes and preserve this revision. */
      if (source_status != -ENOENT || target_status != 0 ||
          !info.is_file || info.size != sizeof(bytes)-1u)
        return 507;
      state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
      rc = salts_fs_root_publish_file_noreplace(
          root, source, destination, &state);
      if (rc != -ENOENT || state != SALTS_FS_REPLACE_NOT_PUBLISHED)
        return 508;
    }
    if (salts_fs_root_file_open(root, destination, SALTS_FS_O_RDONLY,
                                0, &file) != 0 || !file)
      return 509;
    rc = salts_fs_root_file_read(file, actual, sizeof(bytes)-1u);
    if (salts_fs_root_file_close(file) != 0 ||
        rc != (int)(sizeof(bytes)-1u) ||
        memcmp(actual, bytes, sizeof(bytes)-1u) != 0)
      return 510;
    if (salts_fs_root_unlink(root, destination) != 0)
      return 511;
  }
  puts("Salts::FS native SIGKILL publisher recovery: before/after rename reconciled PASS (not power loss)");
  return 0;
}
#endif

int main(int argc, char **argv) {
  char tmp[1024] = {0}, path[1400] = {0}, name[120] = {0};
  const char body[] = "salts core fs native\n";
  salts_fs_buf_t input = salts_fs_buf_init((char *)body, sizeof(body) - 1u);
  salts_fs_buf_t output = {0};
  salts_fs_root_t *root = NULL;
  salts_fs_root_file_t *file = NULL;
  char scratch[128] = {0};
  int bytes;
#ifdef _WIN32
  if (argc == 4 && strcmp(argv[1], "--fs-native-race-child") == 0) {
    char *end = NULL;
    unsigned long owner = strtoul(argv[2], &end, 10);
    if (!end || *end != '\0') return 70;
    int slot = (int)strtol(argv[3], &end, 10);
    if (!end || *end != '\0') return 71;
    return win_native_child_move(owner, slot);
  }
  if (argc != 1) return 72;
  unsigned long pid = GetCurrentProcessId();
#else
  (void)argc;
  (void)argv;
  unsigned long pid = (unsigned long)getpid();
#endif
  if (salts_fs_get_tmpdir(tmp, sizeof(tmp))) return 1;
  if (snprintf(name, sizeof(name), "saltscore-fs-%lu", pid) < 0) return 2;
  if (salts_fs_path_join(path, sizeof(path), tmp, name)) return 3;
  if (salts_fs_write_file(path, &input)) return 4;
  if (salts_fs_read_file(path, &output)) return 5;
  if (output.len != sizeof(body) - 1u ||
      memcmp(output.base, body, sizeof(body) - 1u)) return 6;
  salts_fs_buf_free(&output);
  if (salts_fs_root_open(tmp, &root) || !root) return 7;
  if (salts_fs_root_file_open(root, name, SALTS_FS_O_RDONLY, 0, &file) || !file)
    return 8;
  bytes = salts_fs_root_file_read(file, scratch, sizeof(scratch));
  if (bytes != (int)(sizeof(body) - 1u) ||
      memcmp(scratch, body, sizeof(body) - 1u)) return 9;
  if (salts_fs_root_file_close(file)) return 10;
  file = NULL;
  if (salts_fs_root_file_open(root, "../forbidden", SALTS_FS_O_RDONLY, 0, &file) == 0 ||
      file != NULL) return 11;
  {
    int pub_result = test_immutable_revision_file(root, pid);
    if (pub_result != 0) {
      fprintf(stderr, "Salts::FS immutable publisher M0 FAIL: %d\n", pub_result);
      return pub_result;
    }
  }
#if !defined(_WIN32) && defined(SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS)
  {
    int fault_result = test_durable_file_publish_failpoints(root, pid);
    if (fault_result != 0) {
      fprintf(stderr, "Salts::FS publisher failure-state FAIL: %d\n",
              fault_result);
      return fault_result;
    }
    fault_result = test_durable_publish_sigkill_recovery(root, pid);
    if (fault_result != 0) {
      fprintf(stderr, "Salts::FS SIGKILL publisher recovery FAIL: %d\n",
              fault_result);
      return fault_result;
    }
  }
#endif
  if (salts_fs_root_close(root)) return 12;
  if (salts_fs_unlink(path)) return 13;
  puts("Salts::FS root ownership and path rejection PASS");
  return 0;
}
