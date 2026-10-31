/**
 * @file salts_fs_root.c
 * @brief Secure root-relative filesystem capabilities.
 *
 * The capability path never concatenates an admitted host root with a guest
 * path. POSIX walks directory descriptors with openat(..., O_NOFOLLOW).
 * Windows walks native directory HANDLEs with NtCreateFile RootDirectory and
 * FILE_OPEN_REPARSE_POINT. Intermediate links/reparse points are rejected.
 */
#include <salts/fs.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ELOOP
#define ELOOP EINVAL
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG ERANGE
#endif
#ifndef EOVERFLOW
#define EOVERFLOW ERANGE
#endif

typedef struct salts_fs_root_path_cursor_s {
  const char *path;
  size_t offset;
  unsigned depth;
} salts_fs_root_path_cursor_t;

static bool salts_fs_root_is_separator(char ch) {
  return ch == '/' || ch == '\\';
}

static bool salts_fs_root_host_path_is_absolute(const char *path) {
  size_t length;
  if (!path || path[0] == '\0')
    return false;
  length = strlen(path);
#ifdef _WIN32
  if (length >= 3u && path[1] == ':' &&
      salts_fs_root_is_separator(path[2]))
    return true;
  return length >= 2u &&
      salts_fs_root_is_separator(path[0]) &&
      salts_fs_root_is_separator(path[1]);
#else
  return path[0] == '/';
#endif
}

static int salts_fs_root_next_component(
    salts_fs_root_path_cursor_t *cursor,
    char component[SALTS_FS_ROOT_COMPONENT_MAX],
    bool *out_last) {
  size_t start;
  size_t end;
  size_t length;

  if (!cursor || !cursor->path || !component || !out_last)
    return -EINVAL;
  if (cursor->offset == 0u) {
    if (cursor->path[0] == '\0' ||
        salts_fs_root_is_separator(cursor->path[0]))
      return -EINVAL;
  }
  if (++cursor->depth > SALTS_FS_ROOT_DEPTH_MAX)
    return -ENAMETOOLONG;

  start = cursor->offset;
  end = start;
  while (cursor->path[end] != '\0' &&
         !salts_fs_root_is_separator(cursor->path[end]))
    ++end;
  length = end - start;
  if (length == 0u || length >= SALTS_FS_ROOT_COMPONENT_MAX)
    return length == 0u ? -EINVAL : -ENAMETOOLONG;
  if ((length == 1u && cursor->path[start] == '.') ||
      (length == 2u && cursor->path[start] == '.' &&
       cursor->path[start + 1u] == '.'))
    return -EINVAL;
#ifdef _WIN32
  for (size_t i = start; i < end; ++i)
    if (cursor->path[i] == ':')
      return -EINVAL;
#endif
  memcpy(component, cursor->path + start, length);
  component[length] = '\0';

  if (cursor->path[end] == '\0') {
    cursor->offset = end;
    *out_last = true;
    return 0;
  }
  if (cursor->path[end + 1u] == '\0' ||
      salts_fs_root_is_separator(cursor->path[end + 1u]))
    return -EINVAL;
  cursor->offset = end + 1u;
  *out_last = false;
  return 0;
}

static int salts_fs_root_validate_relative_path(const char *path) {
  salts_fs_root_path_cursor_t cursor = {path, 0u, 0u};
  for (;;) {
    char component[SALTS_FS_ROOT_COMPONENT_MAX];
    bool last = false;
    int rc = salts_fs_root_next_component(&cursor, component, &last);
    if (rc != 0)
      return rc;
    if (last)
      return 0;
  }
}

static int salts_fs_root_validate_open_flags(int flags) {
  const int access =
      flags & (SALTS_FS_O_RDONLY | SALTS_FS_O_WRONLY | SALTS_FS_O_RDWR);
  const int known =
      SALTS_FS_O_RDONLY | SALTS_FS_O_WRONLY | SALTS_FS_O_RDWR |
      SALTS_FS_O_CREAT | SALTS_FS_O_TRUNC | SALTS_FS_O_APPEND | SALTS_FS_ROOT_O_EXCL;
  if ((flags & ~known) != 0)
    return -EINVAL;
  if (access != SALTS_FS_O_RDONLY &&
      access != SALTS_FS_O_WRONLY &&
      access != SALTS_FS_O_RDWR)
    return -EINVAL;
  if ((flags & SALTS_FS_O_TRUNC) != 0 &&
      access == SALTS_FS_O_RDONLY)
    return -EINVAL;
  return 0;
}

#ifdef _WIN32

#include <sys/stat.h>
#include <winternl.h>

#ifndef FILE_DIRECTORY_FILE
#define FILE_DIRECTORY_FILE 0x00000001UL
#endif
#ifndef FILE_NON_DIRECTORY_FILE
#define FILE_NON_DIRECTORY_FILE 0x00000040UL
#endif
#ifndef FILE_SYNCHRONOUS_IO_NONALERT
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020UL
#endif
#ifndef FILE_OPEN_REPARSE_POINT
#define FILE_OPEN_REPARSE_POINT 0x00200000UL
#endif
#ifndef FILE_OPEN
#define FILE_OPEN 0x00000001UL
#endif
#ifndef FILE_CREATE
#define FILE_CREATE 0x00000002UL
#endif
#ifndef FILE_OPEN_IF
#define FILE_OPEN_IF 0x00000003UL
#endif
#ifndef FILE_OVERWRITE
#define FILE_OVERWRITE 0x00000004UL
#endif
#ifndef FILE_OVERWRITE_IF
#define FILE_OVERWRITE_IF 0x00000005UL
#endif
#ifndef OBJ_CASE_INSENSITIVE
#define OBJ_CASE_INSENSITIVE 0x00000040L
#endif

typedef NTSTATUS (NTAPI *salts_nt_create_file_fn)(
    PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
    PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
typedef ULONG (WINAPI *salts_rtl_status_to_dos_fn)(NTSTATUS);
typedef NTSTATUS (NTAPI *salts_nt_set_information_file_fn)(
    HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
/* winternl.h's user-mode enum omits the documented FileRenameInformation. */
enum { SALTS_FS_FILE_RENAME_INFORMATION = 10 };

static INIT_ONCE salts_fs_root_nt_once = INIT_ONCE_STATIC_INIT;
static salts_nt_create_file_fn salts_fs_root_nt_create_file = NULL;
static salts_rtl_status_to_dos_fn salts_fs_root_status_to_dos = NULL;
static salts_nt_set_information_file_fn salts_fs_root_nt_set_information = NULL;

static BOOL CALLBACK salts_fs_root_init_nt(
    PINIT_ONCE once, PVOID parameter, PVOID *context) {
  HMODULE module;
  (void)once;
  (void)parameter;
  (void)context;
  module = GetModuleHandleW(L"ntdll.dll");
  if (!module)
    return FALSE;
  salts_fs_root_nt_create_file =
      (salts_nt_create_file_fn)GetProcAddress(module, "NtCreateFile");
  salts_fs_root_status_to_dos =
      (salts_rtl_status_to_dos_fn)GetProcAddress(
          module, "RtlNtStatusToDosError");
  salts_fs_root_nt_set_information =
      (salts_nt_set_information_file_fn)GetProcAddress(module, "NtSetInformationFile");
  return salts_fs_root_nt_create_file != NULL &&
         salts_fs_root_status_to_dos != NULL &&
         salts_fs_root_nt_set_information != NULL;
}

static int salts_fs_root_win32_error(DWORD error) {
  switch (error) {
  case ERROR_SUCCESS: return 0;
  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
  case ERROR_INVALID_DRIVE:
    return -ENOENT;
  case ERROR_ACCESS_DENIED:
  case ERROR_PRIVILEGE_NOT_HELD:
  case ERROR_SHARING_VIOLATION:
    return -EACCES;
  case ERROR_ALREADY_EXISTS:
  case ERROR_FILE_EXISTS:
    return -EEXIST;
  case ERROR_NOT_SAME_DEVICE:
    return -EXDEV;
  case ERROR_DIRECTORY:
    return -ENOTDIR;
  case ERROR_DIR_NOT_EMPTY:
    return -ENOTEMPTY;
  case ERROR_FILENAME_EXCED_RANGE:
    return -ENAMETOOLONG;
  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
    return -ENOMEM;
  case ERROR_DISK_FULL:
    return -ENOSPC;
  case ERROR_INVALID_PARAMETER:
  case ERROR_INVALID_NAME:
    return -EINVAL;
  default:
    return -EIO;
  }
}

static int salts_fs_root_nt_error(NTSTATUS status) {
  if (!InitOnceExecuteOnce(
          &salts_fs_root_nt_once, salts_fs_root_init_nt, NULL, NULL))
    return -EIO;
  return salts_fs_root_win32_error(
      (DWORD)salts_fs_root_status_to_dos(status));
}

struct salts_fs_root_s {
  HANDLE handle;
};

struct salts_fs_root_file_s {
  HANDLE handle;
  bool append;
};

#define SALTS_FS_ROOT_WIN_DIR_BUFFER 16384u

struct salts_fs_root_dir_s {
  HANDLE handle;
  uint64_t cookie;
  unsigned char buffer[SALTS_FS_ROOT_WIN_DIR_BUFFER];
  ULONG offset;
  bool have_buffer;
  bool restart;
};

static int salts_fs_root_utf8_component_to_wide(
    const char *component,
    WCHAR wide[SALTS_FS_ROOT_COMPONENT_MAX],
    USHORT *out_length_bytes) {
  int count;
  if (!component || !wide || !out_length_bytes)
    return -EINVAL;
  count = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, component, -1,
      wide, (int)SALTS_FS_ROOT_COMPONENT_MAX);
  if (count <= 0)
    return salts_fs_root_win32_error(GetLastError());
  if ((size_t)count * sizeof(WCHAR) > USHRT_MAX)
    return -ENAMETOOLONG;
  *out_length_bytes = (USHORT)((count - 1) * sizeof(WCHAR));
  return 0;
}

static int salts_fs_root_open_component_win(
    HANDLE parent,
    const char *component,
    ACCESS_MASK desired_access,
    ULONG disposition,
    ULONG create_options,
    ULONG file_attributes,
    bool allow_reparse,
    HANDLE *out_handle) {
  WCHAR wide[SALTS_FS_ROOT_COMPONENT_MAX];
  USHORT length_bytes;
  UNICODE_STRING name;
  OBJECT_ATTRIBUTES attributes;
  IO_STATUS_BLOCK io;
  HANDLE handle = INVALID_HANDLE_VALUE;
  FILE_ATTRIBUTE_TAG_INFO tag;
  NTSTATUS status;
  int rc;

  if (!out_handle || parent == NULL || parent == INVALID_HANDLE_VALUE)
    return -EINVAL;
  *out_handle = INVALID_HANDLE_VALUE;
  rc = salts_fs_root_utf8_component_to_wide(
      component, wide, &length_bytes);
  if (rc != 0)
    return rc;
  if (!InitOnceExecuteOnce(
          &salts_fs_root_nt_once, salts_fs_root_init_nt, NULL, NULL))
    return -EIO;

  name.Length = length_bytes;
  name.MaximumLength = (USHORT)(length_bytes + sizeof(WCHAR));
  name.Buffer = wide;
  memset(&attributes, 0, sizeof(attributes));
  attributes.Length = sizeof(attributes);
  attributes.RootDirectory = parent;
  attributes.ObjectName = &name;
  attributes.Attributes = OBJ_CASE_INSENSITIVE;
  memset(&io, 0, sizeof(io));

  status = salts_fs_root_nt_create_file(
      &handle,
      desired_access | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      &attributes,
      &io,
      NULL,
      file_attributes,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      disposition,
      create_options | FILE_OPEN_REPARSE_POINT |
          FILE_SYNCHRONOUS_IO_NONALERT,
      NULL,
      0u);
  if (status < 0)
    return salts_fs_root_nt_error(status);

  if (!GetFileInformationByHandleEx(
          handle, FileAttributeTagInfo, &tag, sizeof(tag))) {
    rc = salts_fs_root_win32_error(GetLastError());
    CloseHandle(handle);
    return rc;
  }
  if (!allow_reparse &&
      (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) {
    CloseHandle(handle);
    return -ELOOP;
  }

  *out_handle = handle;
  return 0;
}

static int salts_fs_root_open_parent_win(
    const salts_fs_root_t *root,
    const char *path,
    HANDLE *out_parent,
    bool *out_parent_owned,
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX]) {
  salts_fs_root_path_cursor_t cursor = {path, 0u, 0u};
  HANDLE current;
  bool current_owned = false;

  if (!root || !path || !out_parent || !out_parent_owned ||
      !final_component || root->handle == INVALID_HANDLE_VALUE)
    return -EINVAL;
  {
    int rc = salts_fs_root_validate_relative_path(path);
    if (rc != 0)
      return rc;
  }
  current = root->handle;
  for (;;) {
    char component[SALTS_FS_ROOT_COMPONENT_MAX];
    bool last = false;
    int rc = salts_fs_root_next_component(&cursor, component, &last);
    if (rc != 0) {
      if (current_owned) CloseHandle(current);
      return rc;
    }
    if (last) {
      memcpy(final_component, component, strlen(component) + 1u);
      *out_parent = current;
      *out_parent_owned = current_owned;
      return 0;
    }

    HANDLE next = INVALID_HANDLE_VALUE;
    rc = salts_fs_root_open_component_win(
        current, component,
        FILE_LIST_DIRECTORY | FILE_TRAVERSE,
        FILE_OPEN, FILE_DIRECTORY_FILE,
        FILE_ATTRIBUTE_NORMAL, false, &next);
    if (current_owned) CloseHandle(current);
    if (rc != 0)
      return rc;
    current = next;
    current_owned = true;
  }
}

static uint64_t salts_fs_root_filetime_us(LARGE_INTEGER time) {
  const uint64_t epoch = 116444736000000000ULL;
  uint64_t ticks = (uint64_t)time.QuadPart;
  return ticks < epoch ? 0u : (ticks - epoch) / 10u;
}

static int salts_fs_root_stat_handle_win(
    HANDLE handle, salts_fs_stat_t *out_stat) {
  FILE_BASIC_INFO basic;
  FILE_STANDARD_INFO standard;
  if (!out_stat || handle == NULL || handle == INVALID_HANDLE_VALUE)
    return -EINVAL;
  if (!GetFileInformationByHandleEx(
          handle, FileBasicInfo, &basic, sizeof(basic)))
    return salts_fs_root_win32_error(GetLastError());
  if (!GetFileInformationByHandleEx(
          handle, FileStandardInfo, &standard, sizeof(standard)))
    return salts_fs_root_win32_error(GetLastError());

  memset(out_stat, 0, sizeof(*out_stat));
  out_stat->size = (uint64_t)standard.EndOfFile.QuadPart;
  out_stat->atime = salts_fs_root_filetime_us(basic.LastAccessTime);
  out_stat->mtime = salts_fs_root_filetime_us(basic.LastWriteTime);
  out_stat->ctime = salts_fs_root_filetime_us(basic.ChangeTime);
  out_stat->is_symlink =
      (basic.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u;
  out_stat->is_directory =
      (basic.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u;
  out_stat->is_file = !out_stat->is_directory && !out_stat->is_symlink;
  out_stat->mode = out_stat->is_directory ? _S_IFDIR : _S_IFREG;
  out_stat->mode |= _S_IREAD;
  if ((basic.FileAttributes & FILE_ATTRIBUTE_READONLY) == 0u)
    out_stat->mode |= _S_IWRITE;
  return 0;
}

static ACCESS_MASK salts_fs_root_access_win(int flags) {
  const int access =
      flags & (SALTS_FS_O_RDONLY | SALTS_FS_O_WRONLY | SALTS_FS_O_RDWR);
  ACCESS_MASK desired = 0u;

  if (access == SALTS_FS_O_RDONLY || access == SALTS_FS_O_RDWR)
    desired |= FILE_READ_DATA | FILE_READ_EA;
  if (access == SALTS_FS_O_WRONLY || access == SALTS_FS_O_RDWR)
    desired |= (ACCESS_MASK)(FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA);
  return desired;
}

static ULONG salts_fs_root_disposition_win(int flags) {
  if (flags & SALTS_FS_ROOT_O_EXCL) return FILE_CREATE;
  if ((flags & SALTS_FS_O_CREAT) != 0) {
    return (flags & SALTS_FS_O_TRUNC) != 0
        ? FILE_OVERWRITE_IF : FILE_OPEN_IF;
  }
  return (flags & SALTS_FS_O_TRUNC) != 0
      ? FILE_OVERWRITE : FILE_OPEN;
}

static int salts_fs_root_open_path_win(
    const salts_fs_root_t *root,
    const char *path,
    ACCESS_MASK access,
    ULONG disposition,
    ULONG options,
    ULONG attributes,
    bool allow_reparse,
    HANDLE *out_handle) {
  HANDLE parent = INVALID_HANDLE_VALUE;
  bool parent_owned = false;
  char final_component[SALTS_FS_ROOT_COMPONENT_MAX];
  int rc = salts_fs_root_open_parent_win(
      root, path, &parent, &parent_owned, final_component);
  if (rc == 0)
    rc = salts_fs_root_open_component_win(
        parent, final_component, access, disposition, options,
        attributes, allow_reparse, out_handle);
  if (parent_owned) CloseHandle(parent);
  return rc;
}

static int salts_fs_root_delete_handle_win(HANDLE handle) {
  FILE_DISPOSITION_INFO disposition;
  disposition.DeleteFile = TRUE;
  if (!SetFileInformationByHandle(
          handle, FileDispositionInfo, &disposition, sizeof(disposition)))
    return salts_fs_root_win32_error(GetLastError());
  return 0;
}

static bool salts_fs_root_dir_name_is_dot_win(
    const FILE_FULL_DIR_INFO *info) {
  size_t chars;
  if (!info) return false;
  chars = info->FileNameLength / sizeof(WCHAR);
  return (chars == 1u && info->FileName[0] == L'.') ||
         (chars == 2u && info->FileName[0] == L'.' &&
          info->FileName[1] == L'.');
}

static int salts_fs_root_dir_fill_win(
    salts_fs_root_dir_t *dir, bool restart) {
  FILE_INFO_BY_HANDLE_CLASS info_class =
      restart ? FileFullDirectoryRestartInfo : FileFullDirectoryInfo;
  if (!GetFileInformationByHandleEx(
          dir->handle, info_class, dir->buffer, sizeof(dir->buffer))) {
    DWORD error = GetLastError();
    if (error == ERROR_NO_MORE_FILES) {
      dir->have_buffer = false;
      return 0;
    }
    return salts_fs_root_win32_error(error);
  }
  dir->offset = 0u;
  dir->have_buffer = true;
  dir->restart = false;
  return 1;
}

static int salts_fs_root_dir_next_win(
    salts_fs_root_dir_t *dir,
    const FILE_FULL_DIR_INFO **out_info) {
  if (!dir || !out_info)
    return -EINVAL;
  for (;;) {
    const FILE_FULL_DIR_INFO *info;
    if (!dir->have_buffer) {
      int fill = salts_fs_root_dir_fill_win(dir, dir->restart);
      if (fill <= 0) return fill;
    }
    info = (const FILE_FULL_DIR_INFO *)(const void *)
        (dir->buffer + dir->offset);
    if (info->NextEntryOffset == 0u) {
      dir->have_buffer = false;
      dir->offset = 0u;
    } else {
      if (info->NextEntryOffset >= sizeof(dir->buffer) ||
          dir->offset > sizeof(dir->buffer) - info->NextEntryOffset)
        return -EIO;
      dir->offset += info->NextEntryOffset;
    }
    if (salts_fs_root_dir_name_is_dot_win(info))
      continue;
    *out_info = info;
    return 1;
  }
}

static int salts_fs_root_dir_seek_cookie_win(
    salts_fs_root_dir_t *dir, uint64_t cookie) {
  if (!dir) return -EINVAL;
  if (cookie == dir->cookie)
    return 0;
  dir->cookie = 0u;
  dir->have_buffer = false;
  dir->offset = 0u;
  dir->restart = true;
  while (dir->cookie < cookie) {
    const FILE_FULL_DIR_INFO *info = NULL;
    int rc = salts_fs_root_dir_next_win(dir, &info);
    if (rc <= 0)
      return rc == 0 ? -EINVAL : rc;
    ++dir->cookie;
  }
  return 0;
}

#else

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#ifdef SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS
#include <stdatomic.h>
#include <signal.h>
#endif
#ifdef __linux__
#include <sys/syscall.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE 1u
#endif
#endif

struct salts_fs_root_s {
  int fd;
};

struct salts_fs_root_file_s {
  int fd;
};

struct salts_fs_root_dir_s {
  DIR *stream;
  uint64_t cookie;
};

static int salts_fs_root_dup_dir_fd(int fd) {
#ifdef F_DUPFD_CLOEXEC
  return fcntl(fd, F_DUPFD_CLOEXEC, 0);
#else
  int duplicate = dup(fd);
  if (duplicate >= 0)
    (void)fcntl(duplicate, F_SETFD, FD_CLOEXEC);
  return duplicate;
#endif
}

static int salts_fs_root_open_parent_posix(
    const salts_fs_root_t *root,
    const char *path,
    int *out_parent,
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX]) {
  salts_fs_root_path_cursor_t cursor = {path, 0u, 0u};
  int current;

  if (!root || root->fd < 0 || !path || !out_parent || !final_component)
    return -EINVAL;
  {
    int rc = salts_fs_root_validate_relative_path(path);
    if (rc != 0)
      return rc;
  }
  current = salts_fs_root_dup_dir_fd(root->fd);
  if (current < 0)
    return -errno;

  for (;;) {
    char component[SALTS_FS_ROOT_COMPONENT_MAX];
    bool last = false;
    int rc = salts_fs_root_next_component(&cursor, component, &last);
    if (rc != 0) {
      close(current);
      return rc;
    }
    if (last) {
      memcpy(final_component, component, strlen(component) + 1u);
      *out_parent = current;
      return 0;
    }

    int next = openat(
        current, component,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next < 0) {
      rc = -errno;
      close(current);
      return rc;
    }
    close(current);
    current = next;
  }
}

static int salts_fs_root_flags_posix(int flags, int *out_flags) {
  const int access =
      flags & (SALTS_FS_O_RDONLY | SALTS_FS_O_WRONLY | SALTS_FS_O_RDWR);
  int native = 0;
  int rc = salts_fs_root_validate_open_flags(flags);
  if (rc != 0 || !out_flags)
    return rc != 0 ? rc : -EINVAL;
  if (access == SALTS_FS_O_RDONLY) native |= O_RDONLY;
  else if (access == SALTS_FS_O_WRONLY) native |= O_WRONLY;
  else native |= O_RDWR;
  if ((flags & SALTS_FS_O_CREAT) != 0) native |= O_CREAT;
  if ((flags & SALTS_FS_O_TRUNC) != 0) native |= O_TRUNC;
  if ((flags & SALTS_FS_O_APPEND) != 0) native |= O_APPEND;
  if ((flags & SALTS_FS_ROOT_O_EXCL) != 0) native |= O_EXCL;
  native |= O_NOFOLLOW | O_CLOEXEC;
  *out_flags = native;
  return 0;
}

static void salts_fs_root_stat_native_posix(
    const struct stat *native, salts_fs_stat_t *out_stat) {
  memset(out_stat, 0, sizeof(*out_stat));
  out_stat->size = (uint64_t)native->st_size;
  out_stat->mode = (int)native->st_mode;
  out_stat->is_file = S_ISREG(native->st_mode) != 0;
  out_stat->is_directory = S_ISDIR(native->st_mode) != 0;
  out_stat->is_symlink = S_ISLNK(native->st_mode) != 0;
#ifdef __APPLE__
  out_stat->atime =
      (uint64_t)native->st_atimespec.tv_sec * 1000000ULL +
      (uint64_t)native->st_atimespec.tv_nsec / 1000ULL;
  out_stat->mtime =
      (uint64_t)native->st_mtimespec.tv_sec * 1000000ULL +
      (uint64_t)native->st_mtimespec.tv_nsec / 1000ULL;
  out_stat->ctime =
      (uint64_t)native->st_ctimespec.tv_sec * 1000000ULL +
      (uint64_t)native->st_ctimespec.tv_nsec / 1000ULL;
#else
  out_stat->atime =
      (uint64_t)native->st_atim.tv_sec * 1000000ULL +
      (uint64_t)native->st_atim.tv_nsec / 1000ULL;
  out_stat->mtime =
      (uint64_t)native->st_mtim.tv_sec * 1000000ULL +
      (uint64_t)native->st_mtim.tv_nsec / 1000ULL;
  out_stat->ctime =
      (uint64_t)native->st_ctim.tv_sec * 1000000ULL +
      (uint64_t)native->st_ctim.tv_nsec / 1000ULL;
#endif
}

static salts_fs_dirent_type_t salts_fs_root_dirent_type_posix(
    unsigned char type) {
#ifdef DT_REG
  if (type == DT_REG) return SALTS_FS_DIRENT_FILE;
#endif
#ifdef DT_DIR
  if (type == DT_DIR) return SALTS_FS_DIRENT_DIRECTORY;
#endif
#ifdef DT_LNK
  if (type == DT_LNK) return SALTS_FS_DIRENT_SYMLINK;
#endif
#ifdef DT_UNKNOWN
  if (type == DT_UNKNOWN) return SALTS_FS_DIRENT_UNKNOWN;
#endif
  return SALTS_FS_DIRENT_OTHER;
}

static bool salts_fs_root_dir_name_is_dot_posix(const char *name) {
  return name && name[0] == '.' &&
      (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

static int salts_fs_root_dir_next_posix(
    salts_fs_root_dir_t *dir, const struct dirent **out_entry) {
  if (!dir || !dir->stream || !out_entry)
    return -EINVAL;
  for (;;) {
    struct dirent *entry;
    errno = 0;
    entry = readdir(dir->stream);
    if (!entry)
      return errno == 0 ? 0 : -errno;
    if (salts_fs_root_dir_name_is_dot_posix(entry->d_name))
      continue;
    *out_entry = entry;
    return 1;
  }
}

static int salts_fs_root_dir_seek_cookie_posix(
    salts_fs_root_dir_t *dir, uint64_t cookie) {
  if (!dir || !dir->stream)
    return -EINVAL;
  if (cookie == dir->cookie)
    return 0;
  rewinddir(dir->stream);
  dir->cookie = 0u;
  while (dir->cookie < cookie) {
    const struct dirent *entry = NULL;
    int rc = salts_fs_root_dir_next_posix(dir, &entry);
    if (rc <= 0)
      return rc == 0 ? -EINVAL : rc;
    ++dir->cookie;
  }
  return 0;
}

#endif

int salts_fs_root_open(
    const char *host_root, salts_fs_root_t **out_root) {
  salts_fs_root_t *root;
  if (!out_root)
    return -EINVAL;
  *out_root = NULL;
  if (!salts_fs_root_host_path_is_absolute(host_root))
    return -EINVAL;

  root = (salts_fs_root_t *)calloc(1, sizeof(*root));
  if (!root)
    return -ENOMEM;

#ifdef _WIN32
  {
    int needed = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, host_root, -1, NULL, 0);
    WCHAR *wide;
    FILE_ATTRIBUTE_TAG_INFO tag;
    if (needed <= 0 || needed > 32767) {
      free(root);
      return needed <= 0
          ? salts_fs_root_win32_error(GetLastError())
          : -ENAMETOOLONG;
    }
    wide = (WCHAR *)calloc((size_t)needed, sizeof(WCHAR));
    if (!wide) {
      free(root);
      return -ENOMEM;
    }
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, host_root, -1,
            wide, needed) <= 0) {
      int rc = salts_fs_root_win32_error(GetLastError());
      free(wide);
      free(root);
      return rc;
    }
    root->handle = CreateFileW(
        wide,
        FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    free(wide);
    if (root->handle == INVALID_HANDLE_VALUE) {
      int rc = salts_fs_root_win32_error(GetLastError());
      free(root);
      return rc;
    }
    if (!GetFileInformationByHandleEx(
            root->handle, FileAttributeTagInfo, &tag, sizeof(tag))) {
      int rc = salts_fs_root_win32_error(GetLastError());
      CloseHandle(root->handle);
      free(root);
      return rc;
    }
    if ((tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
      CloseHandle(root->handle);
      free(root);
      return -ENOTDIR;
    }
  }
#else
  root->fd = open(host_root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (root->fd < 0) {
    int rc = -errno;
    free(root);
    return rc;
  }
#endif

  *out_root = root;
  return 0;
}

int salts_fs_root_close(salts_fs_root_t *root) {
  if (!root)
    return -EINVAL;
#ifdef _WIN32
  if (root->handle == INVALID_HANDLE_VALUE) {
    free(root);
    return -EINVAL;
  }
  if (!CloseHandle(root->handle)) {
    int rc = salts_fs_root_win32_error(GetLastError());
    free(root);
    return rc;
  }
#else
  if (root->fd < 0) {
    free(root);
    return -EINVAL;
  }
  if (close(root->fd) != 0) {
    int rc = -errno;
    free(root);
    return rc;
  }
#endif
  free(root);
  return 0;
}

int salts_fs_root_fstat(
    const salts_fs_root_t *root, salts_fs_stat_t *out_stat) {
  if (!root || !out_stat)
    return -EINVAL;
#ifdef _WIN32
  return salts_fs_root_stat_handle_win(root->handle, out_stat);
#else
  {
    struct stat native;
    if (fstat(root->fd, &native) != 0)
      return -errno;
    salts_fs_root_stat_native_posix(&native, out_stat);
    return 0;
  }
#endif
}

int salts_fs_root_file_open(
    const salts_fs_root_t *root, const char *relative_path,
    int flags, int mode, salts_fs_root_file_t **out_file) {
  salts_fs_root_file_t *file;
  int rc;
  if (!out_file)
    return -EINVAL;
  *out_file = NULL;
  rc = salts_fs_root_validate_open_flags(flags);
  if ((flags & SALTS_FS_ROOT_O_EXCL) && !(flags & SALTS_FS_O_CREAT))
    return -EINVAL;
  if (rc != 0)
    return rc;
  if (!root || !relative_path)
    return -EINVAL;

  file = (salts_fs_root_file_t *)calloc(1, sizeof(*file));
  if (!file)
    return -ENOMEM;

#ifdef _WIN32
  (void)mode;
  file->handle = INVALID_HANDLE_VALUE;
  file->append = (flags & SALTS_FS_O_APPEND) != 0;
  rc = salts_fs_root_open_path_win(
      root, relative_path,
      salts_fs_root_access_win(flags),
      salts_fs_root_disposition_win(flags),
      FILE_NON_DIRECTORY_FILE,
      FILE_ATTRIBUTE_NORMAL,
      false,
      &file->handle);
#else
  {
    int parent = -1;
    int native_flags = 0;
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX];
    file->fd = -1;
    rc = salts_fs_root_flags_posix(flags, &native_flags);
    if (rc == 0)
      rc = salts_fs_root_open_parent_posix(
          root, relative_path, &parent, final_component);
    if (rc == 0) {
      file->fd = openat(parent, final_component, native_flags, mode);
      if (file->fd < 0)
        rc = -errno;
    }
    if (parent >= 0)
      close(parent);
  }
#endif
  if (rc != 0) {
    free(file);
    return rc;
  }
  *out_file = file;
  return 0;
}

int salts_fs_root_file_close(salts_fs_root_file_t *file) {
  if (!file)
    return -EINVAL;
#ifdef _WIN32
  if (file->handle == INVALID_HANDLE_VALUE) {
    free(file);
    return -EINVAL;
  }
  if (!CloseHandle(file->handle)) {
    int rc = salts_fs_root_win32_error(GetLastError());
    free(file);
    return rc;
  }
#else
  if (file->fd < 0) {
    free(file);
    return -EINVAL;
  }
  if (close(file->fd) != 0) {
    int rc = -errno;
    free(file);
    return rc;
  }
#endif
  free(file);
  return 0;
}

int salts_fs_root_file_fsync(salts_fs_root_file_t *file) {
  if (!file) return -EINVAL;
#ifdef _WIN32
  if (!FlushFileBuffers(file->handle))
    return salts_fs_root_win32_error(GetLastError());
  return 0;
#else
  for (;;) {
    if (fsync(file->fd) == 0) return 0;
    if (errno != EINTR) return -errno;
  }
#endif
}

int salts_fs_root_file_read(
    salts_fs_root_file_t *file, char *buffer, size_t length) {
  if (!file || (!buffer && length != 0u) || length > (size_t)INT_MAX)
    return length > (size_t)INT_MAX ? -EOVERFLOW : -EINVAL;
  if (length == 0u)
    return 0;
#ifdef _WIN32
  {
    DWORD count = 0u;
    if (!ReadFile(file->handle, buffer, (DWORD)length, &count, NULL))
      return salts_fs_root_win32_error(GetLastError());
    return (int)count;
  }
#else
  for (;;) {
    ssize_t count = read(file->fd, buffer, length);
    if (count < 0 && errno == EINTR) continue;
    return count < 0 ? -errno : (int)count;
  }
#endif
}

int salts_fs_root_file_write(
    salts_fs_root_file_t *file, const char *data, size_t length) {
  if (!file || (!data && length != 0u) || length > (size_t)INT_MAX)
    return length > (size_t)INT_MAX ? -EOVERFLOW : -EINVAL;
  if (length == 0u)
    return 0;
#ifdef _WIN32
  {
    DWORD count = 0u;
    OVERLAPPED position = {0};
    position.Offset = MAXDWORD;
    position.OffsetHigh = MAXDWORD;
    if (!WriteFile(file->handle, data, (DWORD)length, &count,
                   file->append ? &position : NULL))
      return salts_fs_root_win32_error(GetLastError());
    return (int)count;
  }
#else
  for (;;) {
    ssize_t count = write(file->fd, data, length);
    if (count < 0 && errno == EINTR) continue;
    return count < 0 ? -errno : (int)count;
  }
#endif
}

int64_t salts_fs_root_file_seek(
    salts_fs_root_file_t *file, int64_t offset, int whence) {
  if (!file)
    return -EINVAL;
#ifdef _WIN32
  {
    LARGE_INTEGER distance;
    LARGE_INTEGER result;
    DWORD method;
    if (whence == SEEK_SET) method = FILE_BEGIN;
    else if (whence == SEEK_CUR) method = FILE_CURRENT;
    else if (whence == SEEK_END) method = FILE_END;
    else return -EINVAL;
    distance.QuadPart = offset;
    if (!SetFilePointerEx(file->handle, distance, &result, method))
      return salts_fs_root_win32_error(GetLastError());
    return result.QuadPart;
  }
#else
  {
    off_t native = (off_t)offset;
    off_t result;
    if ((int64_t)native != offset)
      return -EOVERFLOW;
    result = lseek(file->fd, native, whence);
    return result < 0 ? (int64_t)-errno : (int64_t)result;
  }
#endif
}

int64_t salts_fs_root_file_tell(salts_fs_root_file_t *file) {
  return salts_fs_root_file_seek(file, 0, SEEK_CUR);
}

int salts_fs_root_file_set_append(salts_fs_root_file_t *file, bool append) {
  if (!file) return -EINVAL;
#ifdef _WIN32
  file->append = append;
  return 0;
#else
  int flags = fcntl(file->fd, F_GETFL);
  if (flags < 0) return -errno;
  int updated = append ? (flags | O_APPEND) : (flags & ~O_APPEND);
  return fcntl(file->fd, F_SETFL, updated) < 0 ? -errno : 0;
#endif
}

int salts_fs_root_rename(
    const salts_fs_root_t *source_root, const char *source_path,
    const salts_fs_root_t *target_root, const char *target_path) {
  char source_name[SALTS_FS_ROOT_COMPONENT_MAX];
  char target_name[SALTS_FS_ROOT_COMPONENT_MAX];
#ifdef _WIN32
  HANDLE source_parent = INVALID_HANDLE_VALUE, target_parent = INVALID_HANDLE_VALUE;
  HANDLE source = INVALID_HANDLE_VALUE;
  bool source_owned = false, target_owned = false;
  int rc = salts_fs_root_open_parent_win(source_root, source_path,
      &source_parent, &source_owned, source_name);
  if (!rc) rc = salts_fs_root_open_parent_win(target_root, target_path,
      &target_parent, &target_owned, target_name);
  if (!rc) rc = salts_fs_root_open_component_win(source_parent, source_name,
      DELETE | SYNCHRONIZE, FILE_OPEN, 0, FILE_ATTRIBUTE_NORMAL, true, &source);
  if (!rc) {
    WCHAR wide[SALTS_FS_ROOT_COMPONENT_MAX];
    USHORT length = 0;
    rc = salts_fs_root_utf8_component_to_wide(target_name, wide, &length);
    if (!rc) {
      size_t size = offsetof(FILE_RENAME_INFO, FileName) + length + sizeof(WCHAR);
      FILE_RENAME_INFO *info = (FILE_RENAME_INFO *)calloc(1, size);
      if (!info) rc = -ENOMEM;
      else {
        info->ReplaceIfExists = TRUE;
        info->RootDirectory = target_parent;
        info->FileNameLength = length;
        memcpy(info->FileName, wide, length);
        /* Preserve the admitted parent HANDLE all the way to the NT operation.
         * Win32 path normalization is not part of this capability boundary. */
        IO_STATUS_BLOCK io = {0};
        NTSTATUS status = salts_fs_root_nt_set_information(source, &io, info,
            (ULONG)size, (FILE_INFORMATION_CLASS)SALTS_FS_FILE_RENAME_INFORMATION);
        if (status < 0) rc = salts_fs_root_nt_error(status);
        free(info);
      }
    }
  }
  if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
  if (source_owned) CloseHandle(source_parent);
  if (target_owned) CloseHandle(target_parent);
  return rc;
#else
  int source_parent = -1, target_parent = -1;
  int rc = salts_fs_root_open_parent_posix(source_root, source_path,
      &source_parent, source_name);
  if (!rc) rc = salts_fs_root_open_parent_posix(target_root, target_path,
      &target_parent, target_name);
  if (!rc && renameat(source_parent, source_name, target_parent, target_name))
    rc = -errno;
  if (source_parent >= 0) close(source_parent);
  if (target_parent >= 0) close(target_parent);
  return rc;
#endif
}

/* One native no-replace namespace operation, reused by the POSIX durable
 * publisher and public Windows/Linux/macOS namespace-only primitive.
 * Never emulate with check-then-rename. */
#ifndef _WIN32
static int salts_fs_root_native_noreplace_posix(
    int source_parent, const char *source,
    int target_parent, const char *target) {
#if defined(__linux__) && defined(SYS_renameat2)
  if (syscall(SYS_renameat2, source_parent, source, target_parent,
              target, RENAME_NOREPLACE) == 0)
    return 0;
  return (errno == ENOSYS || errno == EINVAL || errno == ENOTSUP)
      ? -ENOSYS : -errno;
#elif defined(__APPLE__) && defined(RENAME_EXCL)
  if (renameatx_np(source_parent, source, target_parent,
                   target, RENAME_EXCL) == 0)
    return 0;
  return (errno == ENOTSUP || errno == EINVAL)
      ? -ENOSYS : -errno;
#else
  (void)source_parent; (void)source;
  (void)target_parent; (void)target;
  return -ENOSYS;
#endif
}
#endif

int salts_fs_root_rename_file_noreplace(
    const salts_fs_root_t *root, const char *source_path,
    const char *destination_path) {
  char source_name[SALTS_FS_ROOT_COMPONENT_MAX];
  char target_name[SALTS_FS_ROOT_COMPONENT_MAX];
  int rc;
  if (!root || !source_path || !destination_path ||
      strcmp(source_path, destination_path) == 0)
    return -EINVAL;
  rc = salts_fs_root_validate_relative_path(source_path);
  if (rc != 0) return rc;
  rc = salts_fs_root_validate_relative_path(destination_path);
  if (rc != 0) return rc;
#ifdef _WIN32
  HANDLE source_parent = INVALID_HANDLE_VALUE;
  HANDLE target_parent = INVALID_HANDLE_VALUE;
  HANDLE source = INVALID_HANDLE_VALUE;
  bool source_owned = false, target_owned = false;
  FILE_STANDARD_INFO standard;
  rc = salts_fs_root_open_parent_win(root, source_path,
      &source_parent, &source_owned, source_name);
  if (rc == 0)
    rc = salts_fs_root_open_parent_win(root, destination_path,
        &target_parent, &target_owned, target_name);
  if (rc == 0)
    rc = salts_fs_root_open_component_win(source_parent, source_name,
        DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_OPEN,
        FILE_NON_DIRECTORY_FILE, FILE_ATTRIBUTE_NORMAL, false, &source);
  if (rc == 0 &&
      !GetFileInformationByHandleEx(
          source, FileStandardInfo, &standard, sizeof(standard)))
    rc = salts_fs_root_win32_error(GetLastError());
  if (rc == 0 && (standard.Directory || standard.NumberOfLinks != 1u))
    rc = -EINVAL;
  if (rc == 0) {
    WCHAR wide[SALTS_FS_ROOT_COMPONENT_MAX];
    USHORT bytes = 0u;
    rc = salts_fs_root_utf8_component_to_wide(target_name, wide, &bytes);
    if (rc == 0) {
      const size_t size =
          offsetof(FILE_RENAME_INFO, FileName) + bytes + sizeof(WCHAR);
      FILE_RENAME_INFO *info = (FILE_RENAME_INFO *)calloc(1, size);
      if (!info) rc = -ENOMEM;
      else {
        IO_STATUS_BLOCK io = {0};
        NTSTATUS status;
        info->ReplaceIfExists = FALSE;
        info->RootDirectory = target_parent;
        info->FileNameLength = bytes;
        memcpy(info->FileName, wide, bytes);
        /* No absolute or ANSI path, and no POSIX-style REPLACE flag.
         * The NT namespace operation chooses at most one winner. */
        status = salts_fs_root_nt_set_information(
            source, &io, info, (ULONG)size,
            (FILE_INFORMATION_CLASS)SALTS_FS_FILE_RENAME_INFORMATION);
        if (status < 0) rc = salts_fs_root_nt_error(status);
        free(info);
      }
    }
  }
  if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
  if (source_owned) CloseHandle(source_parent);
  if (target_owned) CloseHandle(target_parent);
  return rc;
#else
  int source_parent = -1, target_parent = -1, file = -1;
  struct stat source_stat;
  rc = salts_fs_root_open_parent_posix(root, source_path,
      &source_parent, source_name);
  if (rc == 0)
    rc = salts_fs_root_open_parent_posix(root, destination_path,
        &target_parent, target_name);
  if (rc != 0) goto done;
  file = openat(source_parent, source_name,
      O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (file < 0) { rc = -errno; goto done; }
  if (fstat(file, &source_stat) != 0) {
    rc = -errno;
    goto done;
  }
  if (!S_ISREG(source_stat.st_mode) || source_stat.st_nlink != 1) {
    rc = -EINVAL;
    goto done;
  }
  rc = salts_fs_root_native_noreplace_posix(
      source_parent, source_name, target_parent, target_name);
done:
  if (file >= 0) (void)close(file);
  if (source_parent >= 0) (void)close(source_parent);
  if (target_parent >= 0) (void)close(target_parent);
  return rc;
#endif
}

#if defined(SALTS_FS_NATIVE_PUBLISH_TEST_FAULTS) && !defined(_WIN32)
/* Native-test-only deterministic interruptions. Absent from production
 * BUILD_TESTING=OFF; no environment override or public header alias. */
static atomic_int salts_fs_test_publish_fail_stage = ATOMIC_VAR_INIT(0);
int salts_fs_test_set_publish_fail_stage(int stage) {
  /* Positive 1..4 returns -EIO. Negative -1..-4 kills only the
   * test owner process with SIGKILL inside the real publish sequence. */
  if (stage < -8 || stage > 8) return -EINVAL;
  atomic_store_explicit(
      &salts_fs_test_publish_fail_stage, stage, memory_order_release);
  return 0;
}
static int salts_fs_test_maybe_fail_publish(int stage) {
  const int injected = atomic_load_explicit(
      &salts_fs_test_publish_fail_stage, memory_order_acquire);
  if (injected == -stage) {
    /* Real uncatchable process death, no C++/C exit cleanup or normal
     * staged-handle close. Only compiled for POSIX BUILD_TESTING=ON.
     * Not a storage power cut or guarantee of parent-directory durability. */
    (void)kill(getpid(), SIGKILL);
    _exit(240);
  }
  return injected == stage ? -EIO : 0;
}
#define SALTS_FS_PUBLISH_TEST_FAIL_AT(STAGE) do {\
  rc = salts_fs_test_maybe_fail_publish(STAGE);\
  if (rc != 0) goto done;\
} while (0)
#else
#define SALTS_FS_PUBLISH_TEST_FAIL_AT(STAGE) ((void)0)
#endif

int salts_fs_root_publish_file_noreplace(
    const salts_fs_root_t *root, const char *staging_path,
    const char *revision_path, salts_fs_replace_state_t *state) {
  if (!state) return -EINVAL;
  *state = SALTS_FS_REPLACE_NOT_PUBLISHED;
  if (!root || !staging_path || !revision_path ||
      strcmp(staging_path, revision_path) == 0)
    return -EINVAL;
  /* Validation belongs to the existing root capability path parser.
   * Unlike file replacement, immutable generation publication MUST NOT
   * silently degrade into a check-then-rename TOCTOU. */
  {
    int rc = salts_fs_root_validate_relative_path(staging_path);
    if (rc != 0) return rc;
    rc = salts_fs_root_validate_relative_path(revision_path);
    if (rc != 0) return rc;
  }
#ifdef _WIN32
  /* The current root-relative Windows rename primitive supports replacement
   * but its after-rename directory durability and pinned-reader GC contract
   * have not yet been qualified. Do not mutate namespace and then claim
   * a durable immutable version: explicit unsupported, no fallback. */
  return -ENOSYS;
#else
  int old_parent = -1, new_parent = -1, staged = -1;
  char old_name[SALTS_FS_ROOT_COMPONENT_MAX];
  char new_name[SALTS_FS_ROOT_COMPONENT_MAX];
  struct stat native;
  int rc = salts_fs_root_open_parent_posix(
      root, staging_path, &old_parent, old_name);
  if (rc == 0)
    rc = salts_fs_root_open_parent_posix(
        root, revision_path, &new_parent, new_name);
  if (rc != 0) goto done;

  staged = openat(old_parent, old_name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (staged < 0) { rc = -errno; goto done; }
  if (fstat(staged, &native) != 0) {
    rc = -errno;
    goto done;
  }
  /* No hard-link alias to mutate an otherwise published inode.
   * Open writers with an existing file handle are forbidden by the
   * cooperative single-publisher contract (not enforceable by fstat). */
  if (!S_ISREG(native.st_mode) || native.st_nlink != 1) {
    rc = -EINVAL;
    goto done;
  }
  /* Stage 1: failure before staging fsync cannot publish. */
  SALTS_FS_PUBLISH_TEST_FAIL_AT(1);
  for (;;) {
    if (fsync(staged) == 0) break;
    if (errno != EINTR) { rc = -errno; goto done; }
  }
  /* Retain the exact staged inode handle while the native atomic rename
   * runs. The source pathname alone cannot prove the renamed identity. */

  /* Stage 2: failure before namespace move must leave staging intact. */
  SALTS_FS_PUBLISH_TEST_FAIL_AT(2);
  rc = salts_fs_root_native_noreplace_posix(
      old_parent, old_name, new_parent, new_name);
  if (rc != 0) goto done;

  /* Once rename succeeded, the namespace has changed. A later error is
   * UNKNOWN and may not be retried as if no publication took place. */
  *state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
  /* Stage 3: the namespace already moved; durability is unproven. */
  SALTS_FS_PUBLISH_TEST_FAIL_AT(3);
  {
    struct stat published;
    if (fstatat(new_parent, new_name, &published, AT_SYMLINK_NOFOLLOW) != 0) {
      rc = -errno;
      goto done;
    }
    if (!S_ISREG(published.st_mode) || published.st_dev != native.st_dev ||
        published.st_ino != native.st_ino) {
      rc = -EIO;
      goto done;
    }
  }
  if (close(staged) != 0) {
    staged = -1;
    rc = -errno;
    goto done;
  }
  staged = -1;
  for (;;) {
    if (fsync(new_parent) == 0) break;
    if (errno != EINTR) { rc = -errno; goto done; }
  }
  /* Stage 4: destination parent sync done, old parent still pending.
   * UNKNOWN is conservative even for identical source/dest parents. */
  SALTS_FS_PUBLISH_TEST_FAIL_AT(4);
  for (;;) {
    if (fsync(old_parent) == 0) break;
    if (errno != EINTR) { rc = -errno; goto done; }
  }
  *state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
  rc = 0;
done:
  if (staged >= 0) (void)close(staged);
  if (old_parent >= 0) (void)close(old_parent);
  if (new_parent >= 0) (void)close(new_parent);
  return rc;
#endif
}


int salts_fs_root_replace_file_durable(
    const salts_fs_root_t *root, const char *staging_path,
    const char *current_path, salts_fs_replace_state_t *state) {
  if (!state) return -EINVAL;
  *state = SALTS_FS_REPLACE_NOT_PUBLISHED;
  if (!root || !staging_path || !current_path ||
      strcmp(staging_path, current_path) == 0)
    return -EINVAL;
  {
    int rc = salts_fs_root_validate_relative_path(staging_path);
    if (rc != 0) return rc;
    rc = salts_fs_root_validate_relative_path(current_path);
    if (rc != 0) return rc;
  }
#ifdef _WIN32
  /* Windows namespace-only rename was separately qualified under #119.
   * Its after-rename parent directory durability remains unproven.
   * Fail BEFORE a namespace mutation, never report durable success. */
  return -ENOSYS;
#else
  int stage_parent = -1, current_parent = -1, staged_fd = -1;
  char stage_name[SALTS_FS_ROOT_COMPONENT_MAX];
  char current_name[SALTS_FS_ROOT_COMPONENT_MAX];
  struct stat staging_identity;
  int rc = salts_fs_root_open_parent_posix(
      root, staging_path, &stage_parent, stage_name);
  if (rc == 0)
    rc = salts_fs_root_open_parent_posix(
        root, current_path, &current_parent, current_name);
  if (rc != 0) goto done;

  staged_fd = openat(
      stage_parent, stage_name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (staged_fd < 0) { rc = -errno; goto done; }
  if (fstat(staged_fd, &staging_identity) != 0) {
    rc = -errno; goto done;
  }
  if (!S_ISREG(staging_identity.st_mode) ||
      staging_identity.st_nlink != 1) {
    rc = -EINVAL; goto done;
  }
  /* Test-only CURRENT/highwater replace points 5..8; production has no
   * failpoint and performs the exact original native syscall sequence. */
  SALTS_FS_PUBLISH_TEST_FAIL_AT(5);
  for (;;) {
    if (fsync(staged_fd) == 0) break;
    if (errno != EINTR) { rc = -errno; goto done; }
  }

  /* Single owner only. renameat atomically switches the directory name
   * without following an existing final symlink. A concurrent independent
   * writer of CURRENT would be last-writer-wins, NOT a CAS transaction. */
  SALTS_FS_PUBLISH_TEST_FAIL_AT(6);
  if (renameat(
          stage_parent, stage_name,
          current_parent, current_name) != 0) {
    rc = -errno; goto done;
  }
  *state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
  /* An interrupted returned status cannot prove persistence just because
   * the CURRENT/highwater namespace name has already moved. */
  SALTS_FS_PUBLISH_TEST_FAIL_AT(7);
  {
    struct stat current_identity;
    if (fstatat(current_parent, current_name,
                &current_identity, AT_SYMLINK_NOFOLLOW) != 0) {
      rc = -errno; goto done;
    }
    if (!S_ISREG(current_identity.st_mode) ||
        current_identity.st_dev != staging_identity.st_dev ||
        current_identity.st_ino != staging_identity.st_ino) {
      rc = -EIO; goto done;
    }
  }
  if (close(staged_fd) != 0) {
    staged_fd = -1;
    rc = -errno; goto done;
  }
  staged_fd = -1;
  for (;;) {
    if (fsync(current_parent) == 0) break;
    if (errno != EINTR) { rc = -errno; goto done; }
  }
  SALTS_FS_PUBLISH_TEST_FAIL_AT(8);
  for (;;) {
    if (fsync(stage_parent) == 0) break;
    if (errno != EINTR) { rc = -errno; goto done; }
  }
  *state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
  rc = 0;
done:
  if (staged_fd >= 0) (void)close(staged_fd);
  if (stage_parent >= 0) (void)close(stage_parent);
  if (current_parent >= 0) (void)close(current_parent);
  return rc;
#endif
}


/* Fixed small high-water record: no high-level parser or alternate DB.
 * String length is constant so malformed/truncated input fails closed. */
#define SALTS_FS_GEN_RECORD_BYTES 63u
static int salts_fs_gen_is_hex32(const char *value) {
  size_t i;
  if (!value) return 0;
  for (i = 0; i < 32u; ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return 0;
  }
  return value[32] == '\0';
}

static int salts_fs_gen_read_record(
    const salts_fs_root_t *root, const char *relative_path,
    const char expected_magic[9],
    uint64_t *out_generation, char out_request_id[33]) {
  salts_fs_root_file_t *reader = NULL;
  salts_fs_stat_t info = {0};
  char record[SALTS_FS_GEN_RECORD_BYTES + 1u] = {0};
  uint64_t generation = 0u;
  int rc;
  size_t i;
  if (!out_generation || !out_request_id) return -EINVAL;
  *out_generation = 0u;
  out_request_id[0] = '\0';
  if (!root || !relative_path || !expected_magic) return -EINVAL;
  rc = salts_fs_root_file_open(
      root, relative_path, SALTS_FS_O_RDONLY, 0, &reader);
  if (rc != 0) return rc;
  rc = salts_fs_root_file_stat(reader, &info);
  if (rc == 0 && (!info.is_file ||
                   info.size != SALTS_FS_GEN_RECORD_BYTES))
    rc = -EINVAL;
  if (rc == 0) {
    const int n = salts_fs_root_file_read(
        reader, record, SALTS_FS_GEN_RECORD_BYTES);
    if (n < 0) rc = n;
    else if ((size_t)n != SALTS_FS_GEN_RECORD_BYTES) rc = -EINVAL;
  }
  if (rc == 0) {
    if (memcmp(record, expected_magic, 9u) != 0 ||
        record[29] != ' ' || record[62] != '\n')
      rc = -EINVAL;
    for (i = 9u; rc == 0 && i < 29u; ++i) {
      const unsigned char d = (unsigned char)record[i];
      if (d < (unsigned char)'0' || d > (unsigned char)'9') {
        rc = -EINVAL;
        break;
      }
      const uint64_t digit = (uint64_t)(d - (unsigned char)'0');
      if (generation > (UINT64_MAX - digit) / 10u) {
        rc = -EINVAL;
        break;
      }
      generation = generation * 10u + digit;
    }
    if (rc == 0 && generation == 0u) rc = -EINVAL;
    if (rc == 0) {
      char request_id[33];
      memcpy(request_id, record + 30u, 32u);
      request_id[32] = '\0';
      if (!salts_fs_gen_is_hex32(request_id)) rc = -EINVAL;
      else {
        /* No state escapes on a failed close. */
        const int close_rc = salts_fs_root_file_close(reader);
        reader = NULL;
        if (close_rc != 0) return close_rc;
        *out_generation = generation;
        memcpy(out_request_id, request_id, sizeof(request_id));
        return 0;
      }
    }
  }
  if (reader) (void)salts_fs_root_file_close(reader);
  return rc;
}

int salts_fs_root_generation_read(
    const salts_fs_root_t *root, const char *highwater_path,
    uint64_t *out_generation, char out_request_id[33]) {
  return salts_fs_gen_read_record(
      root, highwater_path, "SFS-GEN1 ",
      out_generation, out_request_id);
}

#ifndef _WIN32
/* A visible highwater record may be the result of a previous post-rename
 * DURABILITY_UNKNOWN. A matching request ID is NOT a successful durable
 * retry until we flush the retained file and BOTH original parent dirs.
 * This stays inside the existing trusted single-writer FS owner. */
static int salts_fs_gen_complete_visible_reservation(
    const salts_fs_root_t *root, const char *highwater_path,
    const char *unique_staging_path) {
  salts_fs_root_file_t *file = NULL;
  int current_parent = -1, stage_parent = -1;
  char current_name[SALTS_FS_ROOT_COMPONENT_MAX];
  char stage_name[SALTS_FS_ROOT_COMPONENT_MAX];
  int rc = salts_fs_root_file_open(
      root, highwater_path, SALTS_FS_O_RDONLY, 0, &file);
  if (rc != 0) return rc;
  rc = salts_fs_root_file_fsync(file);
  {
    const int close_rc = salts_fs_root_file_close(file);
    if (rc == 0) rc = close_rc;
  }
  if (rc != 0) return rc;
  rc = salts_fs_root_open_parent_posix(
      root, highwater_path, &current_parent, current_name);
  if (rc == 0)
    rc = salts_fs_root_open_parent_posix(
        root, unique_staging_path, &stage_parent, stage_name);
  if (rc == 0) {
    for (;;) {
      if (fsync(current_parent) == 0) break;
      if (errno != EINTR) { rc = -errno; break; }
    }
  }
  if (rc == 0) {
    for (;;) {
      if (fsync(stage_parent) == 0) break;
      if (errno != EINTR) { rc = -errno; break; }
    }
  }
  if (current_parent >= 0) (void)close(current_parent);
  if (stage_parent >= 0) (void)close(stage_parent);
  return rc;
}
#endif

int salts_fs_root_reserve_generation(
    const salts_fs_root_t *root,
    const char *highwater_path, const char *unique_staging_path,
    const char *request_id,
    uint64_t *out_generation, salts_fs_replace_state_t *state) {
  char previous_request[33] = {0};
  uint64_t previous = 0u, next;
  salts_fs_root_file_t *stage = NULL;
  char record[SALTS_FS_GEN_RECORD_BYTES + 1u];
  int rc, count;
  if (!state || !out_generation) return -EINVAL;
  *state = SALTS_FS_REPLACE_NOT_PUBLISHED;
  *out_generation = 0u;
  if (!root || !highwater_path || !unique_staging_path ||
      strcmp(highwater_path, unique_staging_path) == 0 ||
      !salts_fs_gen_is_hex32(request_id))
    return -EINVAL;
#ifdef _WIN32
  (void)previous_request;
  (void)previous;
  (void)next;
  (void)stage;
  (void)record;
  (void)count;
  return -ENOSYS;
#else
  rc = salts_fs_root_generation_read(
      root, highwater_path, &previous, previous_request);
  if (rc == -ENOENT) previous = 0u;
  else if (rc != 0) return rc;
  if (previous != 0u &&
      memcmp(previous_request, request_id, 32u) == 0) {
    /* A prior call can have returned DURABILITY_UNKNOWN after rename.
     * Merely finding the same bytes is NOT evidence of durability.
     * Reflush the retained highwater and BOTH parent directories, then
     * recheck the exact request and generation before declaring success. */
    uint64_t rechecked = 0u;
    char rechecked_request[33] = {0};
    *state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
    rc = salts_fs_gen_complete_visible_reservation(
        root, highwater_path, unique_staging_path);
    if (rc != 0) return rc;
    rc = salts_fs_root_generation_read(
        root, highwater_path, &rechecked, rechecked_request);
    if (rc != 0) return rc;
    if (rechecked != previous ||
        memcmp(rechecked_request, request_id, 32u) != 0)
      return -EAGAIN;
    *out_generation = previous;
    *state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
    return 0;
  }
  if (previous == UINT64_MAX) return -EOVERFLOW;
  next = previous + 1u;
  count = snprintf(record, sizeof(record),
                   "SFS-GEN1 %020llu %s\n",
                   (unsigned long long)next, request_id);
  if (count != (int)SALTS_FS_GEN_RECORD_BYTES) return -EOVERFLOW;

  rc = salts_fs_root_file_open(root, unique_staging_path,
      SALTS_FS_O_WRONLY | SALTS_FS_O_CREAT | SALTS_FS_ROOT_O_EXCL,
      0600, &stage);
  if (rc != 0) return rc;
  {
    const int written = salts_fs_root_file_write(
        stage, record, SALTS_FS_GEN_RECORD_BYTES);
    if (written < 0) rc = written;
    else if (written != (int)SALTS_FS_GEN_RECORD_BYTES) rc = -EIO;
    if (rc == 0) rc = salts_fs_root_file_fsync(stage);
  }
  {
    const int close_rc = salts_fs_root_file_close(stage);
    stage = NULL;
    if (rc == 0) rc = close_rc;
  }
  if (rc != 0) return rc;
  rc = salts_fs_root_replace_file_durable(
      root, unique_staging_path, highwater_path, state);
  if (rc == 0 && *state == SALTS_FS_REPLACE_PUBLISHED_DURABLE)
    *out_generation = next;
  return rc;
#endif
}


#ifndef _WIN32
/* M9 request journal uses immutable, fixed-extent files in the SAME
 * highwater parent directory. Names are derived from the highwater and
 * immutable 32-hex request token; callers cannot pass an alias path.
 * The single publisher must prevent external removal or in-place edits. */
#define SALTS_FS_GEN_JOURNAL_PATH_MAX 2048u
static int salts_fs_gen_same_parent(
    const char *first, const char *second) {
  const char *fsep = NULL, *ssep = NULL;
  const char *p;
  if (!first || !second) return 0;
  for (p = first; *p; ++p)
    if (*p == '/' || *p == '\\') fsep = p;
  for (p = second; *p; ++p)
    if (*p == '/' || *p == '\\') ssep = p;
  if ((fsep == NULL) != (ssep == NULL)) return 0;
  if (!fsep) return 1;
  const size_t fsize = (size_t)(fsep - first);
  const size_t ssize = (size_t)(ssep - second);
  return fsize == ssize && memcmp(first, second, fsize) == 0;
}
static int salts_fs_gen_request_paths(
    const char *highwater, const char *request_id,
    char published[SALTS_FS_GEN_JOURNAL_PATH_MAX],
    char stage[SALTS_FS_GEN_JOURNAL_PATH_MAX]) {
  int n;
  if (!highwater || !salts_fs_gen_is_hex32(request_id))
    return -EINVAL;
  n = snprintf(published, SALTS_FS_GEN_JOURNAL_PATH_MAX,
               "%s.r.%s", highwater, request_id);
  if (n < 0 || (size_t)n >= SALTS_FS_GEN_JOURNAL_PATH_MAX)
    return -ENAMETOOLONG;
  n = snprintf(stage, SALTS_FS_GEN_JOURNAL_PATH_MAX,
               "%s.s.%s", highwater, request_id);
  if (n < 0 || (size_t)n >= SALTS_FS_GEN_JOURNAL_PATH_MAX)
    return -ENAMETOOLONG;
  int rc = salts_fs_root_validate_relative_path(published);
  if (rc != 0) return rc;
  return salts_fs_root_validate_relative_path(stage);
}
static int salts_fs_gen_request_read(
    const salts_fs_root_t *root, const char *journal_path,
    const char request_id[33], uint64_t *out_generation) {
  char observed[33] = {0};
  uint64_t gen = 0u;
  int rc = salts_fs_gen_read_record(
      root, journal_path, "SFS-REQ1 ", &gen, observed);
  if (rc != 0) return rc;
  if (memcmp(observed, request_id, 32u) != 0) return -EINVAL;
  *out_generation = gen;
  return 0;
}
static int salts_fs_gen_request_reflush(
    const salts_fs_root_t *root,
    const char *journal_path, const char *stage_path,
    const char request_id[33], uint64_t expected) {
  uint64_t observed = 0u;
  int rc = salts_fs_gen_request_read(
      root, journal_path, request_id, &observed);
  if (rc != 0) return rc;
  if (observed != expected) return -EINVAL;
  rc = salts_fs_gen_complete_visible_reservation(
      root, journal_path, stage_path);
  if (rc != 0) return rc;
  observed = 0u;
  rc = salts_fs_gen_request_read(
      root, journal_path, request_id, &observed);
  return rc == 0 && observed != expected ? -EAGAIN : rc;
}
static int salts_fs_gen_request_ensure(
    const salts_fs_root_t *root, const char *highwater,
    const char request_id[33], uint64_t generation) {
  char record_path[SALTS_FS_GEN_JOURNAL_PATH_MAX];
  char staging_path[SALTS_FS_GEN_JOURNAL_PATH_MAX];
  char record[SALTS_FS_GEN_RECORD_BYTES + 1u];
  salts_fs_root_file_t *stage = NULL;
  salts_fs_replace_state_t publish = SALTS_FS_REPLACE_NOT_PUBLISHED;
  uint64_t observed = 0u;
  int rc = salts_fs_gen_request_paths(
      highwater, request_id, record_path, staging_path);
  if (rc != 0) return rc;
  rc = salts_fs_gen_request_read(
      root, record_path, request_id, &observed);
  if (rc == 0)
    return observed == generation
        ? salts_fs_gen_request_reflush(
            root, record_path, staging_path, request_id, generation)
        : -EINVAL;
  if (rc != -ENOENT) return rc;

  /* A killed publisher can leave the deterministic STAGE after the
   * highwater was already committed, so validate and reuse that exact
   * immutable request stage rather than silently allocating a new one. */
  const int n = snprintf(record, sizeof(record),
      "SFS-REQ1 %020llu %s\n",
      (unsigned long long)generation, request_id);
  if (n != (int)SALTS_FS_GEN_RECORD_BYTES) return -EOVERFLOW;
  rc = salts_fs_root_file_open(
      root, staging_path,
      SALTS_FS_O_WRONLY | SALTS_FS_O_CREAT | SALTS_FS_ROOT_O_EXCL,
      0600, &stage);
  if (rc == -EEXIST) {
    uint64_t staged_gen = 0u;
    rc = salts_fs_gen_request_read(
        root, staging_path, request_id, &staged_gen);
    if (rc != 0 || staged_gen != generation)
      return rc != 0 ? rc : -EINVAL;
    rc = salts_fs_gen_complete_visible_reservation(
        root, staging_path, staging_path);
    if (rc != 0) return rc;
  } else {
    if (rc != 0) return rc;
    const int written = salts_fs_root_file_write(
        stage, record, SALTS_FS_GEN_RECORD_BYTES);
    if (written < 0) rc = written;
    else if (written != (int)SALTS_FS_GEN_RECORD_BYTES)
      rc = -EIO;
    if (rc == 0) rc = salts_fs_root_file_fsync(stage);
    const int closed = salts_fs_root_file_close(stage);
    if (rc == 0) rc = closed;
    if (rc != 0) return rc;
  }
  rc = salts_fs_root_publish_file_noreplace(
      root, staging_path, record_path, &publish);
  if (rc == 0) return 0;
  if (publish == SALTS_FS_REPLACE_DURABILITY_UNKNOWN ||
      rc == -EEXIST) {
    /* If the namespace move occurred before the caller died, exact
     * record identity and two-parent fsync are mandatory before success. */
    const int recovered = salts_fs_gen_request_reflush(
        root, record_path, staging_path, request_id, generation);
    if (recovered == 0) return 0;
  }
  return rc;
}
#endif /* !_WIN32 */

int salts_fs_root_reserve_generation_recorded(
    const salts_fs_root_t *root,
    const char *highwater_path, const char *highwater_staging_path,
    const char *request_id, uint64_t *out_generation,
    salts_fs_replace_state_t *state) {
  if (!state || !out_generation) return -EINVAL;
  *state = SALTS_FS_REPLACE_NOT_PUBLISHED;
  *out_generation = 0u;
  if (!root || !highwater_path || !highwater_staging_path ||
      !salts_fs_gen_is_hex32(request_id) ||
      strcmp(highwater_path, highwater_staging_path) == 0)
    return -EINVAL;
#ifdef _WIN32
  /* Durable file publication is not qualified on Windows. */
  return -ENOSYS;
#else
  char last_id[33] = {0};
  char checked_id[33] = {0};
  uint64_t highest = 0u, last_seen = 0u, requested = 0u;
  char recorded[SALTS_FS_GEN_JOURNAL_PATH_MAX];
  char journal_stage[SALTS_FS_GEN_JOURNAL_PATH_MAX];
  int rc = salts_fs_gen_request_paths(
      highwater_path, request_id, recorded, journal_stage);
  if (rc != 0) return rc;
  rc = salts_fs_root_validate_relative_path(highwater_path);
  if (rc != 0) return rc;
  rc = salts_fs_root_validate_relative_path(highwater_staging_path);
  if (rc != 0) return rc;
  if (!salts_fs_gen_same_parent(
          highwater_path, highwater_staging_path))
    return -EINVAL;

  rc = salts_fs_root_generation_read(
      root, highwater_path, &highest, last_id);
  if (rc != 0 && rc != -ENOENT) return rc;
  if (rc == -ENOENT) highest = 0u;
  if (highest > 0u) {
    /* The current highwater may be visible after an UNKNOWN rename,
     * so sync its file and SAME parent before allowing any next ID. */
    *state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
    rc = salts_fs_gen_complete_visible_reservation(
        root, highwater_path, highwater_staging_path);
    if (rc != 0) return rc;
    rc = salts_fs_root_generation_read(
        root, highwater_path, &last_seen, checked_id);
    if (rc != 0 || last_seen != highest ||
        memcmp(checked_id, last_id, 32u) != 0)
      return rc != 0 ? rc : -EAGAIN;
    /* Commit any journal record the previous generation's writer
     * did not finish before SIGKILL. At most latest may be missing
     * because no next generation advances until this succeeds. */
    rc = salts_fs_gen_request_ensure(
        root, highwater_path, last_id, highest);
    if (rc != 0) return rc;
  }
  rc = salts_fs_gen_request_read(
      root, recorded, request_id, &requested);
  if (rc == 0) {
    if (highest == 0u || requested > highest)
      return -EIO;
    *state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
    rc = salts_fs_gen_request_reflush(
        root, recorded, journal_stage, request_id, requested);
    if (rc != 0) return rc;
    *out_generation = requested;
    *state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
    return 0;
  }
  if (rc != -ENOENT) return rc;
  if (highest == UINT64_MAX) return -EOVERFLOW;

  /* A pre-rename SIGKILL can have left the same REQUEST's highwater
   * stage in place with no new highwater visible. Verify its complete
   * generation and ID before reusing the exact staged transaction. */
  salts_fs_stat_t statbuf = {0};
  rc = salts_fs_root_stat(root, highwater_staging_path, &statbuf);
  if (rc == 0) {
    uint64_t staged_gen = 0u;
    char staged_id[33] = {0};
    if (!statbuf.is_file) return -EINVAL;
    rc = salts_fs_gen_read_record(
        root, highwater_staging_path, "SFS-GEN1 ",
        &staged_gen, staged_id);
    if (rc != 0) return rc;
    if (staged_gen != highest + 1u ||
        memcmp(staged_id, request_id, 32u) != 0)
      return -EEXIST;
    rc = salts_fs_root_replace_file_durable(
        root, highwater_staging_path, highwater_path, state);
  } else if (rc == -ENOENT) {
    rc = salts_fs_root_reserve_generation(
        root, highwater_path, highwater_staging_path,
        request_id, &requested, state);
  } else return rc;

  if (rc != 0) {
    if (*state != SALTS_FS_REPLACE_DURABILITY_UNKNOWN) return rc;
    /* Namespace may already contain this newly allocated request.
     * Re-fsync and compare exact ID/generation before journal commit. */
    rc = salts_fs_gen_complete_visible_reservation(
        root, highwater_path, highwater_staging_path);
    if (rc != 0) return rc;
  }
  uint64_t observed = 0u;
  char observed_id[33] = {0};
  rc = salts_fs_root_generation_read(
      root, highwater_path, &observed, observed_id);
  if (rc != 0) return rc;
  if (observed != highest + 1u ||
      memcmp(observed_id, request_id, 32u) != 0) {
    *state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
    return -EAGAIN;
  }
  *state = SALTS_FS_REPLACE_DURABILITY_UNKNOWN;
  rc = salts_fs_gen_request_ensure(
      root, highwater_path, request_id, observed);
  if (rc != 0) return rc;
  *out_generation = observed;
  *state = SALTS_FS_REPLACE_PUBLISHED_DURABLE;
  return 0;
#endif
}


/* One native live writer lock per immutable highwater namespace.
 * It belongs to the SAME Salts::FS C11 module: no registry, second FS
 * backend, advisory check-then-rename or external lock daemon. */
#define SALTS_FS_WRITER_LOCK_PATH_CAP 2048u
struct salts_fs_root_writer_lease_s {
  const salts_fs_root_t *root;
  salts_fs_root_file_t *file;
  char highwater_path[SALTS_FS_WRITER_LOCK_PATH_CAP];
#ifdef _WIN32
  DWORD process_id;
  OVERLAPPED position;
#else
  pid_t process_id;
#endif
};

int salts_fs_root_writer_lease_acquire(
    const salts_fs_root_t *root, const char *highwater_path,
    salts_fs_root_writer_lease_t **out_lease) {
  salts_fs_root_writer_lease_t *lease = NULL;
  char lock_path[SALTS_FS_WRITER_LOCK_PATH_CAP];
  int n, rc;
  if (!out_lease) return -EINVAL;
  *out_lease = NULL;
  if (!root || !highwater_path) return -EINVAL;
  rc = salts_fs_root_validate_relative_path(highwater_path);
  if (rc != 0) return rc;
  n = snprintf(lock_path, sizeof(lock_path),
               "%s.writer", highwater_path);
  if (n < 0 || (size_t)n >= sizeof(lock_path))
    return -ENAMETOOLONG;
  rc = salts_fs_root_validate_relative_path(lock_path);
  if (rc != 0) return rc;
  lease = (salts_fs_root_writer_lease_t *)calloc(1u, sizeof(*lease));
  if (!lease) return -ENOMEM;
  lease->root = root;
  memcpy(lease->highwater_path, highwater_path,
         strlen(highwater_path) + 1u);
#ifdef _WIN32
  lease->process_id = GetCurrentProcessId();
#else
  lease->process_id = getpid();
#endif
  rc = salts_fs_root_file_open(
      root, lock_path, SALTS_FS_O_RDWR | SALTS_FS_O_CREAT,
      0600, &lease->file);
  if (rc != 0) goto failed;
#ifdef _WIN32
  {
    FILE_STANDARD_INFO standard = {0};
    if (!GetFileInformationByHandleEx(
            lease->file->handle, FileStandardInfo,
            &standard, sizeof(standard))) {
      rc = salts_fs_root_win32_error(GetLastError());
      goto failed;
    }
    if (standard.Directory || standard.NumberOfLinks != 1u) {
      rc = -EINVAL; goto failed;
    }
    memset(&lease->position, 0, sizeof(lease->position));
    if (!LockFileEx(
            lease->file->handle,
            LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
            0u, 1u, 0u, &lease->position)) {
      const DWORD err = GetLastError();
      rc = (err == ERROR_LOCK_VIOLATION ||
            err == ERROR_SHARING_VIOLATION)
          ? -EBUSY : salts_fs_root_win32_error(err);
      goto failed;
    }
  }
#else
  {
    struct stat native;
    if (fstat(lease->file->fd, &native) != 0) {
      rc = -errno; goto failed;
    }
    if (!S_ISREG(native.st_mode) || native.st_nlink != 1) {
      rc = -EINVAL; goto failed;
    }
    if (flock(lease->file->fd, LOCK_EX | LOCK_NB) != 0) {
      rc = (errno == EWOULDBLOCK || errno == EAGAIN)
          ? -EBUSY : -errno;
      goto failed;
    }
    /* Lock identity must be durably present under its rooted parent
     * before the owner starts writing highwater or immutable journal. */
    rc = salts_fs_gen_complete_visible_reservation(
        root, lock_path, lock_path);
    if (rc != 0) {
      (void)flock(lease->file->fd, LOCK_UN);
      goto failed;
    }
  }
#endif
  *out_lease = lease;
  return 0;
failed:
  if (lease->file) (void)salts_fs_root_file_close(lease->file);
  free(lease);
  return rc;
}

int salts_fs_root_writer_lease_release(
    salts_fs_root_writer_lease_t *lease) {
  int rc = 0;
  if (!lease || !lease->file) return -EINVAL;
#ifdef _WIN32
  if (lease->process_id != GetCurrentProcessId())
    return -EPERM;
  if (!UnlockFileEx(
          lease->file->handle, 0u, 1u, 0u, &lease->position))
    rc = salts_fs_root_win32_error(GetLastError());
#else
  if (lease->process_id != getpid()) return -EPERM;
  if (flock(lease->file->fd, LOCK_UN) != 0)
    rc = -errno;
#endif
  {
    const int closed = salts_fs_root_file_close(lease->file);
    if (rc == 0) rc = closed;
  }
  free(lease);
  return rc;
}

int salts_fs_root_reserve_generation_recorded_owned(
    const salts_fs_root_writer_lease_t *lease,
    const char *unique_highwater_staging_path,
    const char *request_id, uint64_t *out_generation,
    salts_fs_replace_state_t *state) {
  if (!state || !out_generation) return -EINVAL;
  *state = SALTS_FS_REPLACE_NOT_PUBLISHED;
  *out_generation = 0u;
  if (!lease || !lease->root || !lease->file ||
      !unique_highwater_staging_path || !request_id)
    return -EINVAL;
#ifdef _WIN32
  if (lease->process_id != GetCurrentProcessId()) return -EPERM;
#else
  if (lease->process_id != getpid()) return -EPERM;
#endif
  return salts_fs_root_reserve_generation_recorded(
      lease->root, lease->highwater_path,
      unique_highwater_staging_path, request_id,
      out_generation, state);
}

int salts_fs_root_file_stat(
    const salts_fs_root_file_t *file, salts_fs_stat_t *out_stat) {
  if (!file || !out_stat)
    return -EINVAL;
#ifdef _WIN32
  return salts_fs_root_stat_handle_win(file->handle, out_stat);
#else
  {
    struct stat native;
    if (fstat(file->fd, &native) != 0)
      return -errno;
    salts_fs_root_stat_native_posix(&native, out_stat);
    return 0;
  }
#endif
}

static int salts_fs_root_path_stat_impl(
    const salts_fs_root_t *root,
    const char *relative_path,
    bool allow_final_link,
    salts_fs_stat_t *out_stat) {
  if (!root || !relative_path || !out_stat)
    return -EINVAL;
#ifdef _WIN32
  {
    HANDLE handle = INVALID_HANDLE_VALUE;
    int rc = salts_fs_root_open_path_win(
        root, relative_path,
        FILE_READ_ATTRIBUTES,
        FILE_OPEN, 0u, FILE_ATTRIBUTE_NORMAL,
        true, &handle);
    if (rc != 0)
      return rc;
    rc = salts_fs_root_stat_handle_win(handle, out_stat);
    CloseHandle(handle);
    if (rc != 0)
      return rc;
    if (!allow_final_link && out_stat->is_symlink)
      return -ELOOP;
    return 0;
  }
#else
  {
    int parent = -1;
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX];
    struct stat native;
    int rc = salts_fs_root_open_parent_posix(
        root, relative_path, &parent, final_component);
    if (rc != 0)
      return rc;
    if (fstatat(parent, final_component, &native, AT_SYMLINK_NOFOLLOW) != 0)
      rc = -errno;
    close(parent);
    if (rc != 0)
      return rc;
    salts_fs_root_stat_native_posix(&native, out_stat);
    if (!allow_final_link && out_stat->is_symlink)
      return -ELOOP;
    return 0;
  }
#endif
}

int salts_fs_root_stat(
    const salts_fs_root_t *root, const char *relative_path,
    salts_fs_stat_t *out_stat) {
  return salts_fs_root_path_stat_impl(
      root, relative_path, false, out_stat);
}

int salts_fs_root_lstat(
    const salts_fs_root_t *root, const char *relative_path,
    salts_fs_stat_t *out_stat) {
  return salts_fs_root_path_stat_impl(
      root, relative_path, true, out_stat);
}

int salts_fs_root_mkdir(
    const salts_fs_root_t *root, const char *relative_path, int mode) {
  if (!root || !relative_path)
    return -EINVAL;
#ifdef _WIN32
  {
    HANDLE handle = INVALID_HANDLE_VALUE;
    int rc;
    (void)mode;
    rc = salts_fs_root_open_path_win(
        root, relative_path,
        FILE_LIST_DIRECTORY | FILE_TRAVERSE,
        FILE_CREATE, FILE_DIRECTORY_FILE,
        FILE_ATTRIBUTE_DIRECTORY, false, &handle);
    if (rc == 0)
      CloseHandle(handle);
    return rc;
  }
#else
  {
    int parent = -1;
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX];
    int rc = salts_fs_root_open_parent_posix(
        root, relative_path, &parent, final_component);
    if (rc == 0 && mkdirat(parent, final_component, (mode_t)mode) != 0)
      rc = -errno;
    if (parent >= 0) close(parent);
    return rc;
  }
#endif
}

int salts_fs_root_rmdir(
    const salts_fs_root_t *root, const char *relative_path) {
  if (!root || !relative_path)
    return -EINVAL;
#ifdef _WIN32
  {
    HANDLE handle = INVALID_HANDLE_VALUE;
    int rc = salts_fs_root_open_path_win(
        root, relative_path, DELETE,
        FILE_OPEN, FILE_DIRECTORY_FILE,
        FILE_ATTRIBUTE_NORMAL, false, &handle);
    if (rc == 0) {
      rc = salts_fs_root_delete_handle_win(handle);
      CloseHandle(handle);
    }
    return rc;
  }
#else
  {
    int parent = -1;
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX];
    int rc = salts_fs_root_open_parent_posix(
        root, relative_path, &parent, final_component);
    if (rc == 0 &&
        unlinkat(parent, final_component, AT_REMOVEDIR) != 0)
      rc = -errno;
    if (parent >= 0) close(parent);
    return rc;
  }
#endif
}

int salts_fs_root_unlink(
    const salts_fs_root_t *root, const char *relative_path) {
  if (!root || !relative_path)
    return -EINVAL;
#ifdef _WIN32
  {
    HANDLE handle = INVALID_HANDLE_VALUE;
    salts_fs_stat_t stat_value;
    int rc = salts_fs_root_open_path_win(
        root, relative_path, DELETE,
        FILE_OPEN, 0u, FILE_ATTRIBUTE_NORMAL, true, &handle);
    if (rc != 0)
      return rc;
    rc = salts_fs_root_stat_handle_win(handle, &stat_value);
    if (rc == 0 && stat_value.is_directory && !stat_value.is_symlink)
      rc = -EISDIR;
    if (rc == 0)
      rc = salts_fs_root_delete_handle_win(handle);
    CloseHandle(handle);
    return rc;
  }
#else
  {
    int parent = -1;
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX];
    int rc = salts_fs_root_open_parent_posix(
        root, relative_path, &parent, final_component);
    if (rc == 0 && unlinkat(parent, final_component, 0) != 0)
      rc = -errno;
    if (parent >= 0) close(parent);
    return rc;
  }
#endif
}

int salts_fs_root_opendir_self(
    const salts_fs_root_t *root, salts_fs_root_dir_t **out_dir) {
  salts_fs_root_dir_t *dir;
  if (!out_dir)
    return -EINVAL;
  *out_dir = NULL;
  if (!root)
    return -EINVAL;

  dir = (salts_fs_root_dir_t *)calloc(1, sizeof(*dir));
  if (!dir)
    return -ENOMEM;

#ifdef _WIN32
  dir->handle = INVALID_HANDLE_VALUE;
  if (!DuplicateHandle(
          GetCurrentProcess(), root->handle,
          GetCurrentProcess(), &dir->handle,
          0u, FALSE, DUPLICATE_SAME_ACCESS)) {
    int rc = salts_fs_root_win32_error(GetLastError());
    free(dir);
    return rc;
  }
  dir->restart = true;
#else
  {
    /* dup() shares the directory offset. Each stream must start independently
     * while remaining anchored to the admitted root, even after a rename. */
    int fd = openat(
        root->fd, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
      int rc = -errno;
      free(dir);
      return rc;
    }
    dir->stream = fdopendir(fd);
    if (!dir->stream) {
      int rc = -errno;
      close(fd);
      free(dir);
      return rc;
    }
  }
#endif
  *out_dir = dir;
  return 0;
}

int salts_fs_root_opendir(
    const salts_fs_root_t *root, const char *relative_path,
    salts_fs_root_dir_t **out_dir) {
  salts_fs_root_dir_t *dir;
  int rc = 0;
  if (!out_dir)
    return -EINVAL;
  *out_dir = NULL;
  if (!root || !relative_path)
    return -EINVAL;

  dir = (salts_fs_root_dir_t *)calloc(1, sizeof(*dir));
  if (!dir)
    return -ENOMEM;

#ifdef _WIN32
  dir->handle = INVALID_HANDLE_VALUE;
  rc = salts_fs_root_open_path_win(
      root, relative_path,
      FILE_LIST_DIRECTORY | FILE_TRAVERSE,
      FILE_OPEN, FILE_DIRECTORY_FILE,
      FILE_ATTRIBUTE_NORMAL, false, &dir->handle);
  if (rc == 0)
    dir->restart = true;
#else
  {
    int parent = -1;
    int fd = -1;
    char final_component[SALTS_FS_ROOT_COMPONENT_MAX];
    rc = salts_fs_root_open_parent_posix(
        root, relative_path, &parent, final_component);
    if (rc == 0) {
      fd = openat(
          parent, final_component,
          O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      if (fd < 0)
        rc = -errno;
    }
    if (parent >= 0) close(parent);
    if (rc == 0) {
      dir->stream = fdopendir(fd);
      if (!dir->stream) {
        rc = -errno;
        close(fd);
      }
    }
  }
#endif

  if (rc != 0) {
    free(dir);
    return rc;
  }
  *out_dir = dir;
  return 0;
}

int salts_fs_root_readdir(
    salts_fs_root_dir_t *dir, uint64_t cookie,
    char *name_buffer, size_t name_buffer_size,
    salts_fs_root_dirent_t *out_entry) {
  int rc;
  if (!dir || !name_buffer || name_buffer_size == 0u || !out_entry)
    return -EINVAL;
  memset(out_entry, 0, sizeof(*out_entry));
  name_buffer[0] = '\0';

#ifdef _WIN32
  {
    const FILE_FULL_DIR_INFO *info = NULL;
    int needed;
    rc = salts_fs_root_dir_seek_cookie_win(dir, cookie);
    if (rc != 0) return rc;
    rc = salts_fs_root_dir_next_win(dir, &info);
    if (rc <= 0) return rc;

    needed = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        info->FileName, (int)(info->FileNameLength / sizeof(WCHAR)),
        NULL, 0, NULL, NULL);
    if (needed <= 0)
      return salts_fs_root_win32_error(GetLastError());
    if ((size_t)needed + 1u > name_buffer_size ||
        (size_t)needed > SALTS_FS_ROOT_DIRENT_NAME_MAX) {
      dir->cookie = UINT64_MAX;
      return -ERANGE;
    }
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            info->FileName, (int)(info->FileNameLength / sizeof(WCHAR)),
            name_buffer, needed, NULL, NULL) != needed) {
      dir->cookie = UINT64_MAX;
      return salts_fs_root_win32_error(GetLastError());
    }
    name_buffer[needed] = '\0';
    out_entry->name_length = (size_t)needed;
    if ((info->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
      out_entry->type = SALTS_FS_DIRENT_SYMLINK;
    else if ((info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u)
      out_entry->type = SALTS_FS_DIRENT_DIRECTORY;
    else
      out_entry->type = SALTS_FS_DIRENT_FILE;
  }
#else
  {
    const struct dirent *entry = NULL;
    size_t name_length;
    rc = salts_fs_root_dir_seek_cookie_posix(dir, cookie);
    if (rc != 0) return rc;
    rc = salts_fs_root_dir_next_posix(dir, &entry);
    if (rc <= 0) return rc;
    name_length = strlen(entry->d_name);
    if (name_length + 1u > name_buffer_size ||
        name_length > SALTS_FS_ROOT_DIRENT_NAME_MAX) {
      dir->cookie = UINT64_MAX;
      return -ERANGE;
    }
    memcpy(name_buffer, entry->d_name, name_length + 1u);
    out_entry->name_length = name_length;
    out_entry->type = salts_fs_root_dirent_type_posix(entry->d_type);
  }
#endif

  if (cookie == UINT64_MAX)
    return -EOVERFLOW;
  dir->cookie = cookie + 1u;
  out_entry->next_cookie = dir->cookie;
  return 1;
}

int salts_fs_root_closedir(salts_fs_root_dir_t *dir) {
  if (!dir)
    return -EINVAL;
#ifdef _WIN32
  if (dir->handle == INVALID_HANDLE_VALUE) {
    free(dir);
    return -EINVAL;
  }
  if (!CloseHandle(dir->handle)) {
    int rc = salts_fs_root_win32_error(GetLastError());
    free(dir);
    return rc;
  }
#else
  if (!dir->stream) {
    free(dir);
    return -EINVAL;
  }
  if (closedir(dir->stream) != 0) {
    int rc = -errno;
    free(dir);
    return rc;
  }
#endif
  free(dir);
  return 0;
}
