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

/* Windows independent-process immutable reader acceptance.  The original
 * Salts::FS root API opens each version as a FILE_SHARE_DELETE handle; a
 * native publisher can move current to B2/A3 while A1 remains pinned.
 * GC may retire A1 immediately OR return a documented sharing refusal,
 * but must never corrupt or redirect the already-open A1 reader.
 * This is native process/handle ownership, NOT cross-process lease/GC policy
 * or a crash-durable publisher qualification. */
static int win_child_read_pinned_revision(unsigned long owner) {
  char tmp[1024], source[100], readback[64] = {0};
  const char expected[] = "Windows-old-A1-immutable";
  wchar_t ready_name[100], release_name[100];
  HANDLE ready = NULL, release = NULL;
  salts_fs_root_t *root = NULL;
  salts_fs_root_file_t *old = NULL;
  int rc = 80;
  if (snprintf(source, sizeof(source), "fs-win-pin-%lu-A1", owner) <= 0 ||
      swprintf(ready_name, sizeof(ready_name)/sizeof(ready_name[0]),
               L"Local\\SaltsFSPinReady-%lu", owner) < 0 ||
      swprintf(release_name, sizeof(release_name)/sizeof(release_name[0]),
               L"Local\\SaltsFSPinRelease-%lu", owner) < 0)
    return 80;
  if (salts_fs_get_tmpdir(tmp, sizeof(tmp)) != 0 ||
      salts_fs_root_open(tmp, &root) != 0 || !root)
    return 81;
  ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, ready_name);
  release = OpenEventW(SYNCHRONIZE, FALSE, release_name);
  if (!ready || !release) { rc = 82; goto done; }
  if (salts_fs_root_file_open(root, source, SALTS_FS_O_RDONLY, 0, &old) != 0 ||
      !old) { rc = 83; goto done; }
  if (!SetEvent(ready)) { rc = 84; goto done; }
  if (WaitForSingleObject(release, 10000u) != WAIT_OBJECT_0) {
    rc = 85; goto done;
  }
  if (salts_fs_root_file_read(old, readback, sizeof(expected)-1u) !=
          (int)(sizeof(expected)-1u) ||
      memcmp(readback, expected, sizeof(expected)-1u) != 0) {
    rc = 86; goto done;
  }
  rc = 0;
done:
  if (old && salts_fs_root_file_close(old) != 0 && rc == 0) rc = 87;
  if (root && salts_fs_root_close(root) != 0 && rc == 0) rc = 88;
  if (ready) CloseHandle(ready);
  if (release) CloseHandle(release);
  return rc;
}

static int win_independent_process_pinned_reader(
    salts_fs_root_t *root, unsigned long owner) {
  const char red[] = "Windows-old-A1-immutable";
  const char green[] = "Windows-new-B2-immutable";
  char stage[3][100], version[3][100], readback[64] = {0};
  wchar_t ready_name[100], release_name[100], executable[32768], command[33000];
  HANDLE ready = NULL, release = NULL, child = NULL;
  salts_fs_root_file_t *reader = NULL;
  salts_fs_stat_t info;
  STARTUPINFOW startup = {0};
  PROCESS_INFORMATION process = {0};
  DWORD code = 99u;
  int rc = 0, deletion;
  for (int i = 0; i < 3; ++i) {
    if (snprintf(stage[i], sizeof(stage[i]),
                 "fs-win-pin-%lu-stage%d", owner, i) <= 0 ||
        snprintf(version[i], sizeof(version[i]),
                 "fs-win-pin-%lu-%s", owner,
                 i == 0 ? "A1" : i == 1 ? "B2" : "A3") <= 0)
      return 90;
  }
  if (swprintf(ready_name, sizeof(ready_name)/sizeof(ready_name[0]),
               L"Local\\SaltsFSPinReady-%lu", owner) < 0 ||
      swprintf(release_name, sizeof(release_name)/sizeof(release_name[0]),
               L"Local\\SaltsFSPinRelease-%lu", owner) < 0)
    return 91;
  if (stage_revision_file(root, stage[0], red, sizeof(red)-1u) != 0 ||
      salts_fs_root_rename_file_noreplace(root, stage[0], version[0]) != 0)
    return 92;
  ready = CreateEventW(NULL, TRUE, FALSE, ready_name);
  release = CreateEventW(NULL, TRUE, FALSE, release_name);
  if (!ready || !release) { rc = 93; goto done; }
  {
    DWORD length = GetModuleFileNameW(
        NULL, executable, (DWORD)(sizeof(executable)/sizeof(executable[0])));
    if (length == 0 || length >= sizeof(executable)/sizeof(executable[0]) ||
        swprintf(command, sizeof(command)/sizeof(command[0]),
                 L"\"%ls\" --fs-native-pin-child %lu", executable, owner) < 0) {
      rc = 94; goto done;
    }
  }
  startup.cb = sizeof(startup);
  if (!CreateProcessW(executable, command, NULL, NULL, FALSE,
                      CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
    rc = 95; goto done;
  }
  child = process.hProcess;
  CloseHandle(process.hThread);
  if (WaitForSingleObject(ready, 10000u) != WAIT_OBJECT_0) {
    rc = 96; goto done;
  }
  /* A1 is now owned by an independent process. The parent publishes B2
   * and identical-content A3 at distinct names without reopening A1. */
  if (stage_revision_file(root, stage[1], green, sizeof(green)-1u) != 0 ||
      salts_fs_root_rename_file_noreplace(root, stage[1], version[1]) != 0 ||
      stage_revision_file(root, stage[2], red, sizeof(red)-1u) != 0 ||
      salts_fs_root_rename_file_noreplace(root, stage[2], version[2]) != 0) {
    rc = 97; goto done;
  }
  deletion = salts_fs_root_unlink(root, version[0]);
  /* Windows might mark the file delete-pending until the last pin closes
   * or reject retirement with an explicit sharing/access error. Neither
   * can invalidate a successfully opened old-version reader. */
  if (deletion != 0 && deletion != -EACCES && deletion != -EBUSY) {
    rc = 98; goto done;
  }
  if (!SetEvent(release) ||
      WaitForSingleObject(child, 10000u) != WAIT_OBJECT_0 ||
      !GetExitCodeProcess(child, &code) || code != 0u) {
    rc = 99; goto done;
  }
  /* Reap and close the last external pin before any final cleanup. */
  CloseHandle(child);
  child = NULL;
  {
    const int old_visible = salts_fs_root_stat(root, version[0], &info);
    if (old_visible == 0) {
      if (salts_fs_root_unlink(root, version[0]) != 0) {
        rc = 100; goto done;
      }
    } else if (old_visible != -ENOENT) {
      rc = 101; goto done;
    }
  }
  for (int i = 1; i < 3; ++i) {
    const char *expected = i == 1 ? green : red;
    size_t length = i == 1 ? sizeof(green)-1u : sizeof(red)-1u;
    memset(readback, 0, sizeof(readback));
    if (salts_fs_root_file_open(root, version[i], SALTS_FS_O_RDONLY,
                                0, &reader) != 0 || !reader) {
      rc = 102; goto done;
    }
    const int n = salts_fs_root_file_read(reader, readback, length);
    if (salts_fs_root_file_close(reader) != 0 ||
        n != (int)length || memcmp(expected, readback, length) != 0) {
      reader = NULL; rc = 103; goto done;
    }
    reader = NULL;
    if (salts_fs_root_unlink(root, version[i]) != 0) {
      rc = 104; goto done;
    }
  }
  puts("Salts::FS Windows independent old-A1 pin across B2/A3 and GC attempt PASS (not durable)");
done:
  /* Always release a child before joining, including on failed setup. */
  if (release) (void)SetEvent(release);
  if (child) {
    if (WaitForSingleObject(child, 10000u) != WAIT_OBJECT_0)
      (void)TerminateProcess(child, 9u);
    CloseHandle(child);
  }
  if (reader) (void)salts_fs_root_file_close(reader);
  if (ready) CloseHandle(ready);
  if (release) CloseHandle(release);
  return rc;
}

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
  rc = win_independent_process_pinned_reader(root, pid);
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


/* This tests the lowest-level CURRENT manifest namespace/flush primitive,
 * not a monotonic generation allocator or an authenticated metadata schema.
 * A1 and A3 refer to byte-identical original content but carry different
 * externally assigned generation IDs; the caller must enforce that policy. */
static int test_current_manifest_switch(
    salts_fs_root_t *root, unsigned long pid) {
  char stage[3][96], current[96], actual[128] = {0};
  const char *records[3] = {
    "publisher=mesa;asset=icon;generation=1;content=red",
    "publisher=mesa;asset=icon;generation=2;content=green",
    "publisher=mesa;asset=icon;generation=3;content=red"
  };
  salts_fs_replace_state_t state;
  salts_fs_root_file_t *pinned_a1 = NULL, *pinned_b2 = NULL, *live = NULL;
  salts_fs_stat_t statbuf;
  int rc;
  if (snprintf(current, sizeof(current),
               "fs-current-%lu-manifest", pid) <= 0)
    return 601;
  for (int i = 0; i < 3; ++i) {
    if (snprintf(stage[i], sizeof(stage[i]),
                 "fs-current-%lu-stage-%d", pid, i) <= 0)
      return 602;
  }
  if (stage_revision_file(root, stage[0],
                          records[0], strlen(records[0])) != 0)
    return 603;
  state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
  rc = salts_fs_root_replace_file_durable(
      root, "../escape", current, &state);
  if (rc != -EINVAL || state != SALTS_FS_REPLACE_NOT_PUBLISHED)
    return 604;
  rc = salts_fs_root_replace_file_durable(
      root, stage[0], stage[0], &state);
  if (rc != -EINVAL || state != SALTS_FS_REPLACE_NOT_PUBLISHED)
    return 605;
#ifdef _WIN32
  rc = salts_fs_root_replace_file_durable(
      root, stage[0], current, &state);
  if (rc != -ENOSYS || state != SALTS_FS_REPLACE_NOT_PUBLISHED ||
      salts_fs_root_stat(root, stage[0], &statbuf) != 0 ||
      salts_fs_root_stat(root, current, &statbuf) != -ENOENT)
    return 606;
  if (salts_fs_root_unlink(root, stage[0]) != 0)
    return 607;
  puts("Salts::FS CURRENT manifest durable switch explicitly UNSUPPORTED on Windows");
  return 0;
#else
  rc = salts_fs_root_replace_file_durable(
      root, stage[0], current, &state);
  if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
    return 608;
  if (salts_fs_root_file_open(root, current, SALTS_FS_O_RDONLY,
                              0, &pinned_a1) != 0 || !pinned_a1)
    return 609;
  if (stage_revision_file(root, stage[1],
                          records[1], strlen(records[1])) != 0)
    return 610;
  rc = salts_fs_root_replace_file_durable(
      root, stage[1], current, &state);
  if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
    return 611;
  if (salts_fs_root_file_open(root, current, SALTS_FS_O_RDONLY,
                              0, &pinned_b2) != 0 || !pinned_b2)
    return 612;
  if (stage_revision_file(root, stage[2],
                          records[2], strlen(records[2])) != 0)
    return 613;
  rc = salts_fs_root_replace_file_durable(
      root, stage[2], current, &state);
  if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
    return 614;
  if (salts_fs_root_stat(root, stage[2], &statbuf) != -ENOENT ||
      salts_fs_root_stat(root, current, &statbuf) != 0 ||
      !statbuf.is_file || statbuf.size != strlen(records[2]))
    return 615;
  {
    salts_fs_root_file_t *versions[3] = {pinned_a1, pinned_b2, NULL};
    if (salts_fs_root_file_open(root, current, SALTS_FS_O_RDONLY,
                                0, &live) != 0 || !live)
      return 616;
    versions[2] = live;
    for (int i = 0; i < 3; ++i) {
      memset(actual, 0, sizeof(actual));
      const size_t length = strlen(records[i]);
      if (salts_fs_root_file_seek(versions[i], 0, SEEK_SET) != 0 ||
          salts_fs_root_file_read(versions[i], actual, length) != (int)length ||
          memcmp(actual, records[i], length) != 0)
        return 617;
    }
  }
  if (salts_fs_root_file_close(pinned_a1) != 0 ||
      salts_fs_root_file_close(pinned_b2) != 0 ||
      salts_fs_root_file_close(live) != 0)
    return 618;
  if (salts_fs_root_unlink(root, current) != 0)
    return 619;
  puts("Salts::FS root-relative durable CURRENT A1-B2-A3 atomic switch + pinned old manifests PASS (POSIX)");
  return 0;
#endif
}


/* A single-authorized-writer's crash-stable high-water is reserved
 * BEFORE immutable A1/B2/A3 content and CURRENT publication. The same
 * encoded red bytes at A1 and A3 still have distinct generation numbers.
 * This is NOT an authenticated identity service or a multiwriter lock. */
static int test_generation_reservation_before_current(
    salts_fs_root_t *root, unsigned long pid) {
  static const char *requests[4] = {
    "00112233445566778899aabbccddeeff",
    "10112233445566778899aabbccddeeff",
    "20112233445566778899aabbccddeeff",
    "30112233445566778899aabbccddeeff"
  };
  static const char red[] = "original-red-revision";
  static const char green[] = "later-green-revision";
  char highwater[100], current[100], stages[4][100];
  char contents[3][100], revisions[3][100], manifests[3][100];
  char expected[180] = {0}, bytes[180] = {0}, read_request[33] = {0};
  salts_fs_replace_state_t state;
  uint64_t id = 99u, read_generation = 0u;
  salts_fs_stat_t statbuf;
  salts_fs_root_file_t *reader = NULL;
  int rc;
  if (snprintf(highwater, sizeof(highwater),
               "fs-gen-%lu-highwater", pid) <= 0 ||
      snprintf(current, sizeof(current),
               "fs-gen-%lu-current", pid) <= 0)
    return 701;
  for (int i = 0; i < 4; ++i) {
    if (snprintf(stages[i], sizeof(stages[i]),
                 "fs-gen-%lu-counter-stage%d", pid, i) <= 0)
      return 702;
    if (i < 3) {
      if (snprintf(contents[i], sizeof(contents[i]),
                   "fs-gen-%lu-encoded-stage%d", pid, i) <= 0 ||
          snprintf(revisions[i], sizeof(revisions[i]),
                   "fs-gen-%lu-immutable-revision%d", pid, i+1) <= 0 ||
          snprintf(manifests[i], sizeof(manifests[i]),
                   "fs-gen-%lu-manifest-stage%d", pid, i) <= 0)
        return 703;
    }
  }
  state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
  rc = salts_fs_root_generation_read(
      root, highwater, &read_generation, read_request);
  if (rc != -ENOENT || read_generation != 0u || read_request[0] != '\0')
    return 704;
  rc = salts_fs_root_reserve_generation(
      root, highwater, stages[0], "invalid-request",
      &id, &state);
  if (rc != -EINVAL || id != 0u ||
      state != SALTS_FS_REPLACE_NOT_PUBLISHED)
    return 705;
#ifdef _WIN32
  rc = salts_fs_root_reserve_generation(
      root, highwater, stages[0], requests[0], &id, &state);
  if (rc != -ENOSYS || state != SALTS_FS_REPLACE_NOT_PUBLISHED ||
      id != 0u ||
      salts_fs_root_stat(root, highwater, &statbuf) != -ENOENT ||
      salts_fs_root_stat(root, stages[0], &statbuf) != -ENOENT)
    return 706;
  puts("Salts::FS durable high-water generation reserve UNSUPPORTED on Windows");
  return 0;
#else
  for (int i = 0; i < 3; ++i) {
    const char *payload = i == 1 ? green : red;
    const size_t payload_len = strlen(payload);
    const int expect = i+1;
    const char *request = requests[i];
    state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
    id = 0u;
    rc = salts_fs_root_reserve_generation(
        root, highwater, stages[i], request, &id, &state);
    if (rc != 0 || id != (uint64_t)expect ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 707;
    read_generation = 0u;
    rc = salts_fs_root_generation_read(
        root, highwater, &read_generation, read_request);
    if (rc != 0 || read_generation != id ||
        strcmp(read_request, request) != 0)
      return 708;
    if (i == 0) {
      /* Repeated newest request is idempotent and does not create a stage. */
      uint64_t repeated = 0u;
      rc = salts_fs_root_reserve_generation(
          root, highwater, stages[0], request, &repeated, &state);
      if (rc != 0 || repeated != 1u ||
          state != SALTS_FS_REPLACE_PUBLISHED_DURABLE ||
          salts_fs_root_stat(root, stages[0], &statbuf) != -ENOENT)
        return 709;
    }

    /* Real immutable content must be separately published durably
     * AFTER the owner first reserves the matching generation. */
    if (stage_revision_file(root, contents[i], payload, payload_len) != 0)
      return 710;
    rc = salts_fs_root_publish_file_noreplace(
        root, contents[i], revisions[i], &state);
    if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 711;
    const int n = snprintf(expected, sizeof(expected),
        "publisher=mesa;asset=icon;generation=%llu;revision=%s",
        (unsigned long long)id, revisions[i]);
    if (n <= 0 || n >= (int)sizeof(expected))
      return 712;
    if (stage_revision_file(root, manifests[i],
                            expected, (size_t)n) != 0)
      return 713;
    rc = salts_fs_root_replace_file_durable(
        root, manifests[i], current, &state);
    if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 714;
    if (salts_fs_root_file_open(root, current, SALTS_FS_O_RDONLY,
                                0, &reader) != 0 || !reader)
      return 715;
    memset(bytes, 0, sizeof(bytes));
    rc = salts_fs_root_file_read(reader, bytes, (size_t)n);
    if (salts_fs_root_file_close(reader) != 0 ||
        rc != n || memcmp(bytes, expected, (size_t)n) != 0)
      return 716;
    reader = NULL;
  }
  /* A distinct root capability observes the already-persisted high-water,
   * as a restarted producer does, then allocates generation 4 despite
   * content A3 == A1 and despite CURRENT still being generation 3. */
  {
    char parent_dir[1024] = {0};
    salts_fs_root_t *reopened = NULL;
    if (salts_fs_get_tmpdir(parent_dir, sizeof(parent_dir)) != 0 ||
        salts_fs_root_open(parent_dir, &reopened) != 0 || !reopened)
      return 717;
    read_generation = 0u;
    rc = salts_fs_root_generation_read(
        reopened, highwater, &read_generation, read_request);
    if (rc != 0 || read_generation != 3u ||
        strcmp(read_request, requests[2]) != 0)
      return 718;
    rc = salts_fs_root_reserve_generation(
        reopened, highwater, stages[3], requests[3], &id, &state);
    if (rc != 0 || id != 4u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 719;
    if (salts_fs_root_close(reopened) != 0)
      return 720;
  }
  read_generation = 0u;
  rc = salts_fs_root_generation_read(
      root, highwater, &read_generation, read_request);
  if (rc != 0 || read_generation != 4u ||
      strcmp(read_request, requests[3]) != 0)
    return 721;
  /* Original A1 and A3 source contents are equal; names and generation
   * identity stay distinct. Content GC must NEVER delete highwater. */
  for (int i = 0; i < 3; ++i) {
    if (salts_fs_root_file_open(root, revisions[i], SALTS_FS_O_RDONLY,
                                0, &reader) != 0 || !reader)
      return 722;
    const char *payload = i == 1 ? green : red;
    const size_t length = strlen(payload);
    memset(bytes, 0, sizeof(bytes));
    rc = salts_fs_root_file_read(reader, bytes, length);
    if (salts_fs_root_file_close(reader) != 0 ||
        rc != (int)length || memcmp(bytes, payload, length) != 0)
      return 723;
    reader = NULL;
    if (salts_fs_root_unlink(root, revisions[i]) != 0)
      return 724;
  }
  /* Even after content GC, highwater remains at 4, preventing ABA reuse. */
  rc = salts_fs_root_generation_read(
      root, highwater, &read_generation, read_request);
  if (rc != 0 || read_generation != 4u)
    return 725;
  /* Reject a malformed/torn highwater instead of silently resetting to 1.
   * A saturated 64-bit record must fail rather than wrap to generation 0. */
  {
    char bad_path[100], max_path[100], unused_stage[100];
    static const char max_record[] =
        "SFS-GEN1 18446744073709551615 00112233445566778899aabbccddeeff\n";
    if (snprintf(bad_path, sizeof(bad_path),
                 "fs-gen-%lu-malformed", pid) <= 0 ||
        snprintf(max_path, sizeof(max_path),
                 "fs-gen-%lu-overflow", pid) <= 0 ||
        snprintf(unused_stage, sizeof(unused_stage),
                 "fs-gen-%lu-should-not-stage", pid) <= 0)
      return 727;
    if (stage_revision_file(
            root, bad_path, "not-a-generation", 16u) != 0)
      return 728;
    read_generation = 99u;
    read_request[0] = 'x';
    rc = salts_fs_root_generation_read(
        root, bad_path, &read_generation, read_request);
    if (rc != -EINVAL || read_generation != 0u ||
        read_request[0] != '\0')
      return 729;
    state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
    id = 99u;
    rc = salts_fs_root_reserve_generation(
        root, bad_path, unused_stage, requests[0], &id, &state);
    if (rc != -EINVAL || id != 0u ||
        state != SALTS_FS_REPLACE_NOT_PUBLISHED)
      return 730;
    if (stage_revision_file(
            root, max_path, max_record, sizeof(max_record)-1u) != 0)
      return 731;
    rc = salts_fs_root_generation_read(
        root, max_path, &read_generation, read_request);
    if (rc != 0 || read_generation != UINT64_MAX ||
        strcmp(read_request, requests[0]) != 0)
      return 732;
    state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
    id = 99u;
    rc = salts_fs_root_reserve_generation(
        root, max_path, unused_stage, requests[1], &id, &state);
    if (rc != -EOVERFLOW || id != 0u ||
        state != SALTS_FS_REPLACE_NOT_PUBLISHED ||
        salts_fs_root_stat(root, unused_stage, &statbuf) != -ENOENT)
      return 733;
    if (salts_fs_root_unlink(root, bad_path) != 0 ||
        salts_fs_root_unlink(root, max_path) != 0)
      return 734;
  }
  if (salts_fs_root_unlink(root, current) != 0 ||
      salts_fs_root_unlink(root, highwater) != 0)
    return 726;
  puts("Salts::FS durable single-writer highwater + A1/B2/A3 reserve-before-content + reopen + no ABA reuse PASS (POSIX)");
  return 0;
#endif
}


#if !defined(_WIN32) && defined(SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS)
/* M8: actual child death while replacing the mutable CURRENT pointer
 * and the independent generation highwater. A parent can reconcile
 * actual namespace content after SIGKILL, but CANNOT infer power-cut
 * durability solely from visible bytes. */
static int test_current_highwater_sigkill_reconcile(
    salts_fs_root_t *root, unsigned long pid) {
  static const char old_current[] = "CURRENT old A1 generation=1";
  static const char next_current[] = "CURRENT new B2 generation=2";
  static const char req1[] = "00112233445566778899aabbccddeeff";
  static const char req2[] = "10112233445566778899aabbccddeeff";
  for (int failpoint = 5; failpoint <= 8; ++failpoint) {
    char current[96], first[96], second[96], highwater[96],
         reserve_one[96], reserve_two[96], actual[128] = {0},
         observed_request[33] = {0};
    salts_fs_root_file_t *reader = NULL;
    salts_fs_replace_state_t result = SALTS_FS_REPLACE_NOT_PUBLISHED;
    salts_fs_stat_t statbuf = {0};
    uint64_t generation = 0u;
    int rc, status = 0, from_stage, from_current;
    pid_t child;
    if (snprintf(current, sizeof(current),
                 "fs-m8-%lu-current-%d", pid, failpoint) <= 0 ||
        snprintf(first, sizeof(first),
                 "fs-m8-%lu-first-%d", pid, failpoint) <= 0 ||
        snprintf(second, sizeof(second),
                 "fs-m8-%lu-second-%d", pid, failpoint) <= 0 ||
        snprintf(highwater, sizeof(highwater),
                 "fs-m8-%lu-highwater-%d", pid, failpoint) <= 0 ||
        snprintf(reserve_one, sizeof(reserve_one),
                 "fs-m8-%lu-reserve1-%d", pid, failpoint) <= 0 ||
        snprintf(reserve_two, sizeof(reserve_two),
                 "fs-m8-%lu-reserve2-%d", pid, failpoint) <= 0)
      return 801;

    /* The old pointer is durably published before the child starts. */
    if (stage_revision_file(root, first, old_current,
                            sizeof(old_current)-1u) != 0 ||
        salts_fs_root_replace_file_durable(
            root, first, current, &result) != 0 ||
        result != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 802;
    if (stage_revision_file(root, second, next_current,
                            sizeof(next_current)-1u) != 0)
      return 803;
    child = fork();
    if (child < 0) return 804;
    if (child == 0) {
      if (salts_fs_test_set_publish_fail_stage(-failpoint) != 0)
        _exit(241);
      (void)salts_fs_root_replace_file_durable(
          root, second, current, &result);
      _exit(242);
    }
    if (waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      return 805;
    from_stage = salts_fs_root_stat(root, second, &statbuf);
    from_current = salts_fs_root_stat(root, current, &statbuf);
    if (from_current != 0 || !statbuf.is_file)
      return 806;
    if (salts_fs_root_file_open(root, current, SALTS_FS_O_RDONLY,
                                0, &reader) != 0 || !reader)
      return 807;
    const char *expected = failpoint <= 6 ? old_current : next_current;
    const size_t expected_length = strlen(expected);
    rc = salts_fs_root_file_read(reader, actual, expected_length);
    if (salts_fs_root_file_close(reader) != 0 ||
        rc != (int)expected_length ||
        memcmp(actual, expected, expected_length) != 0)
      return 808;
    reader = NULL;
    if (failpoint <= 6) {
      if (from_stage != 0) return 809;
      rc = salts_fs_root_replace_file_durable(
          root, second, current, &result);
      if (rc != 0 || result != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
        return 810;
    } else {
      if (from_stage != -ENOENT) return 811;
      /* No safe replay after a namespace move, even though the
       * original child died without returning a commit status. */
      rc = salts_fs_root_replace_file_durable(
          root, second, current, &result);
      if (rc != -ENOENT ||
          result != SALTS_FS_REPLACE_NOT_PUBLISHED)
        return 812;
    }
    if (salts_fs_root_unlink(root, current) != 0)
      return 813;

    /* Now kill the generation reservation owner at the same four points.
     * Before rename: highwater is still 1 and stage2 is retained.
     * After rename: highwater may show 2, but the retry must reflush
     * BOTH original parents and check the request ID before success. */
    rc = salts_fs_root_reserve_generation(
        root, highwater, reserve_one, req1, &generation, &result);
    if (rc != 0 || generation != 1u ||
        result != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 814;
    child = fork();
    if (child < 0) return 815;
    if (child == 0) {
      if (salts_fs_test_set_publish_fail_stage(-failpoint) != 0)
        _exit(241);
      (void)salts_fs_root_reserve_generation(
          root, highwater, reserve_two, req2, &generation, &result);
      _exit(242);
    }
    if (waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      return 816;
    rc = salts_fs_root_generation_read(
        root, highwater, &generation, observed_request);
    if (rc != 0 ||
        generation != (failpoint <= 6 ? 1u : 2u) ||
        strcmp(observed_request, failpoint <= 6 ? req1 : req2) != 0)
      return 817;
    const int reserve_stage_state =
        salts_fs_root_stat(root, reserve_two, &statbuf);
    if (failpoint <= 6) {
      if (reserve_stage_state != 0) return 818;
      /* The child never performed the rename. Reconcile the complete
       * staged record, then finish precisely that committed replacement. */
      rc = salts_fs_root_replace_file_durable(
          root, reserve_two, highwater, &result);
      if (rc != 0 || result != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
        return 819;
    } else {
      if (reserve_stage_state != -ENOENT) return 820;
      /* Idempotent resubmission of the same latest request must perform
       * real file and both-parent fsyncs BEFORE a durable-success reply. */
      generation = 0u;
      rc = salts_fs_root_reserve_generation(
          root, highwater, reserve_two, req2, &generation, &result);
      if (rc != 0 || result != SALTS_FS_REPLACE_PUBLISHED_DURABLE ||
          generation != 2u)
        return 821;
    }
    rc = salts_fs_root_generation_read(
        root, highwater, &generation, observed_request);
    if (rc != 0 || generation != 2u ||
        strcmp(observed_request, req2) != 0)
      return 822;
    if (salts_fs_root_unlink(root, highwater) != 0)
      return 823;
  }
  puts("Salts::FS true SIGKILL CURRENT+highwater replay: pre-move old, post-move unknown and resync PASS (not power loss)");
  return 0;
}
#endif


/* M9: per-request immutable journal + trusted sole-owner generation highwater.
 * The journal key survives content GC and cannot be recreated with a new
 * generation when an older request is re-submitted. Actual SIGKILL around
 * journal rename verifies recovery when the highwater advanced but the
 * latest request entry did not yet finish publishing. */
static int test_generation_historical_journal(
    salts_fs_root_t *root, unsigned long pid) {
  static const char *ids[7] = {
    "01112233445566778899aabbccddeeff",
    "11112233445566778899aabbccddeeff",
    "21112233445566778899aabbccddeeff",
    "31112233445566778899aabbccddeeff",
    "41112233445566778899aabbccddeeff",
    "51112233445566778899aabbccddeeff",
    "61112233445566778899aabbccddeeff"
  };
  static const char *source_bytes[3] = {
    "journal-red-encoded", "journal-green-encoded", "journal-red-encoded"
  };
  char highwater[96], stages[9][96], request_path[128],
       journal_stage[128], content_stage[96], revision[96];
  char read_id[33] = {0};
  salts_fs_replace_state_t state = SALTS_FS_REPLACE_NOT_PUBLISHED;
  salts_fs_stat_t statbuf;
  uint64_t generation = 0u, highest = 0u;
  int rc;
  if (snprintf(highwater, sizeof(highwater),
               "fs-m9-%lu-highwater", pid) <= 0)
    return 901;
  for (int i = 0; i < 9; ++i) {
    if (snprintf(stages[i], sizeof(stages[i]),
                 "fs-m9-%lu-highwater-stage-%d", pid, i) <= 0)
      return 902;
  }
  rc = salts_fs_root_reserve_generation_recorded(
      root, highwater, stages[0], "invalid",
      &generation, &state);
  if (rc != -EINVAL || generation != 0u ||
      state != SALTS_FS_REPLACE_NOT_PUBLISHED)
    return 903;
#ifdef _WIN32
  rc = salts_fs_root_reserve_generation_recorded(
      root, highwater, stages[0], ids[0],
      &generation, &state);
  if (rc != -ENOSYS || generation != 0u ||
      state != SALTS_FS_REPLACE_NOT_PUBLISHED ||
      salts_fs_root_stat(root, highwater, &statbuf) != -ENOENT ||
      salts_fs_root_stat(root, stages[0], &statbuf) != -ENOENT)
    return 904;
  puts("Salts::FS historical request journal explicitly UNSUPPORTED on Windows");
  return 0;
#else
  for (int i = 0; i < 3; ++i) {
    const char *payload = source_bytes[i];
    const size_t payload_size = strlen(payload);
    rc = salts_fs_root_reserve_generation_recorded(
        root, highwater, stages[i], ids[i], &generation, &state);
    if (rc != 0 || generation != (uint64_t)i+1u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 905;
    if (snprintf(request_path, sizeof(request_path),
                 "%s.r.%s", highwater, ids[i]) <= 0 ||
        salts_fs_root_stat(root, request_path, &statbuf) != 0 ||
        !statbuf.is_file || statbuf.size != 63u)
      return 906;
    if (snprintf(content_stage, sizeof(content_stage),
                 "fs-m9-%lu-content-%d", pid, i) <= 0 ||
        snprintf(revision, sizeof(revision),
                 "fs-m9-%lu-revision-%d", pid, i) <= 0)
      return 907;
    if (stage_revision_file(root, content_stage, payload, payload_size))
      return 908;
    rc = salts_fs_root_publish_file_noreplace(
        root, content_stage, revision, &state);
    if (rc != 0 || state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 909;
    if (salts_fs_root_unlink(root, revision) != 0)
      return 910; /* Content GC cannot delete the immutable journal. */
  }
  /* At generation 3 the old A1 & B2 request identities must survive
   * content GC and must NOT allocate generation 4 when re-submitted. */
  for (int i = 0; i < 3; ++i) {
    generation = 0u;
    rc = salts_fs_root_reserve_generation_recorded(
        root, highwater, stages[7], ids[i], &generation, &state);
    if (rc != 0 || generation != (uint64_t)i+1u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE ||
        salts_fs_root_stat(root, stages[7], &statbuf) != -ENOENT)
      return 911;
  }
  rc = salts_fs_root_generation_read(
      root, highwater, &highest, read_id);
  if (rc != 0 || highest != 3u || strcmp(read_id, ids[2]) != 0)
    return 912;
  {
    char tmp[1024] = {0};
    salts_fs_root_t *new_owner = NULL;
    if (salts_fs_get_tmpdir(tmp, sizeof(tmp)) != 0 ||
        salts_fs_root_open(tmp, &new_owner) != 0 || !new_owner)
      return 913;
    generation = 0u;
    rc = salts_fs_root_reserve_generation_recorded(
        new_owner, highwater, stages[7], ids[0],
        &generation, &state);
    if (rc != 0 || generation != 1u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE ||
        salts_fs_root_close(new_owner) != 0)
      return 914;
  }

#if defined(SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS)
  /* Kill generation-4 writer *before journal's native rename*. Its
   * generation-4 highwater has been reserved; the journal stage
   * exists but its immutable request record is not yet visible. */
  {
    pid_t child = fork();
    int status = 0;
    if (child < 0) return 915;
    if (child == 0) {
      if (salts_fs_test_set_publish_fail_stage(-2) != 0)
        _exit(241);
      (void)salts_fs_root_reserve_generation_recorded(
          root, highwater, stages[3], ids[3], &generation, &state);
      _exit(242);
    }
    if (waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      return 916;
    rc = salts_fs_root_generation_read(
        root, highwater, &highest, read_id);
    if (rc != 0 || highest != 4u || strcmp(read_id, ids[3]) != 0)
      return 917;
    if (snprintf(request_path, sizeof(request_path),
                 "%s.r.%s", highwater, ids[3]) <= 0 ||
        snprintf(journal_stage, sizeof(journal_stage),
                 "%s.s.%s", highwater, ids[3]) <= 0 ||
        salts_fs_root_stat(root, request_path, &statbuf) != -ENOENT ||
        salts_fs_root_stat(root, journal_stage, &statbuf) != 0)
      return 918;
    /* A new request must first finish journal4, not lose gen4. */
    rc = salts_fs_root_reserve_generation_recorded(
        root, highwater, stages[4], ids[4], &generation, &state);
    if (rc != 0 || generation != 5u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 919;
    if (salts_fs_root_stat(root, request_path, &statbuf) != 0 ||
        salts_fs_root_stat(root, journal_stage, &statbuf) != -ENOENT)
      return 920;
    rc = salts_fs_root_reserve_generation_recorded(
        root, highwater, stages[7], ids[3], &generation, &state);
    if (rc != 0 || generation != 4u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 921;
  }
  /* Kill gen-6 writer *after* the immutable journal rename but
   * before both parent fsyncs. Visible record is not durable proof:
   * same request must reflush and recheck exactly before ACK. */
  {
    pid_t child = fork();
    int status = 0;
    if (child < 0) return 922;
    if (child == 0) {
      if (salts_fs_test_set_publish_fail_stage(-3) != 0)
        _exit(241);
      (void)salts_fs_root_reserve_generation_recorded(
          root, highwater, stages[5], ids[5], &generation, &state);
      _exit(242);
    }
    if (waitpid(child, &status, 0) != child ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      return 923;
    rc = salts_fs_root_generation_read(
        root, highwater, &highest, read_id);
    if (rc != 0 || highest != 6u || strcmp(read_id, ids[5]) != 0)
      return 924;
    rc = salts_fs_root_reserve_generation_recorded(
        root, highwater, stages[5], ids[5], &generation, &state);
    if (rc != 0 || generation != 6u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
      return 925;
  }
  /* The historical gen-4 and gen-1 records remain addressable. */
  rc = salts_fs_root_reserve_generation_recorded(
      root, highwater, stages[6], ids[6], &generation, &state);
  if (rc != 0 || generation != 7u ||
      state != SALTS_FS_REPLACE_PUBLISHED_DURABLE)
    return 926;
  for (int i = 0; i < 7; ++i) {
    generation = 0u;
    rc = salts_fs_root_reserve_generation_recorded(
        root, highwater, stages[7], ids[i], &generation, &state);
    if (rc != 0 || generation != (uint64_t)i+1u ||
        state != SALTS_FS_REPLACE_PUBLISHED_DURABLE ||
        salts_fs_root_stat(root, stages[7], &statbuf) != -ENOENT)
      return 927;
  }
  rc = salts_fs_root_generation_read(
      root, highwater, &highest, read_id);
  if (rc != 0 || highest != 7u || strcmp(read_id, ids[6]) != 0)
    return 928;
  /* Deliberately corrupt an old journal entry. Caller must fail closed
   * rather than treating its old request as new and allocating 8. */
  {
    salts_fs_root_file_t *corrupt = NULL;
    if (snprintf(request_path, sizeof(request_path),
                 "%s.r.%s", highwater, ids[0]) <= 0)
      return 929;
    if (salts_fs_root_file_open(root, request_path,
          SALTS_FS_O_WRONLY | SALTS_FS_O_TRUNC, 0, &corrupt) != 0 ||
        !corrupt)
      return 930;
    if (salts_fs_root_file_close(corrupt) != 0)
      return 931;
    generation = 0u;
    rc = salts_fs_root_reserve_generation_recorded(
        root, highwater, stages[8], ids[0], &generation, &state);
    if (rc != -EINVAL || generation != 0u ||
        salts_fs_root_stat(root, stages[8], &statbuf) != -ENOENT)
      return 932;
    rc = salts_fs_root_generation_read(
        root, highwater, &highest, read_id);
    if (rc != 0 || highest != 7u)
      return 933;
  }
#endif /* SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS */
  for (int i = 0; i < 7; ++i) {
    if (snprintf(request_path, sizeof(request_path),
                 "%s.r.%s", highwater, ids[i]) <= 0 ||
        salts_fs_root_unlink(root, request_path) != 0)
      return 934;
  }
  if (salts_fs_root_unlink(root, highwater) != 0)
    return 935;
  puts("Salts::FS historical request journal A1/B2/A3 -> old IDs stable, kill-recovery/GC/corrupt refusal PASS (POSIX)");
  return 0;
#endif
}

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
  if (argc == 3 && strcmp(argv[1], "--fs-native-pin-child") == 0) {
    char *end = NULL;
    unsigned long owner = strtoul(argv[2], &end, 10);
    if (!end || *end != '\0' || owner == 0u) return 73;
    return win_child_read_pinned_revision(owner);
  }
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
  {
    int manifest_result = test_current_manifest_switch(root, pid);
    if (manifest_result != 0) {
      fprintf(stderr, "Salts::FS CURRENT manifest switch FAIL: %d\n",
              manifest_result);
      return manifest_result;
    }
  }
  {
    int gen_result = test_generation_reservation_before_current(root, pid);
    if (gen_result != 0) {
      fprintf(stderr, "Salts::FS durable generation reserve FAIL: %d\n",
              gen_result);
      return gen_result;
    }
  }
#if !defined(_WIN32) && defined(SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS)
  {
    int recovery = test_current_highwater_sigkill_reconcile(root, pid);
    if (recovery != 0) {
      fprintf(stderr, "Salts::FS CURRENT/highwater SIGKILL replay FAIL: %d\n",
              recovery);
      return recovery;
    }
  }
#endif
  {
    int journal_result = test_generation_historical_journal(root, pid);
    if (journal_result != 0) {
      fprintf(stderr, "Salts::FS historical request journal FAIL: %d\n",
              journal_result);
      return journal_result;
    }
  }
  if (salts_fs_root_close(root)) return 12;
  if (salts_fs_unlink(path)) return 13;
  puts("Salts::FS root ownership and path rejection PASS");
  return 0;
}
