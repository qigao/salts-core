#include <salts/fs.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
/* No new project, workflow, installer or consumer. Exercise real OS
 * namespace/descriptor operations in the existing Salts::FS native CTest.
 * This M1 test checks native competing OS processes and old-inode pins, but
 * does NOT validate monotonic generation allocation, distributed leases,
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

int main(void) {
  char tmp[1024] = {0}, path[1400] = {0}, name[120] = {0};
  const char body[] = "salts core fs native\n";
  salts_fs_buf_t input = salts_fs_buf_init((char *)body, sizeof(body) - 1u);
  salts_fs_buf_t output = {0};
  salts_fs_root_t *root = NULL;
  salts_fs_root_file_t *file = NULL;
  char scratch[128] = {0};
  int bytes;
#ifdef _WIN32
  unsigned long pid = GetCurrentProcessId();
#else
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
  if (salts_fs_root_close(root)) return 12;
  if (salts_fs_unlink(path)) return 13;
  puts("Salts::FS root ownership and path rejection PASS");
  return 0;
}
