/**
 * @file salts_fs.h
 * @brief Salts File System Utilities
 * @author "Simple, direct, no bullshit" - Linus philosophy applied to file I/O
 *
 * This header provides simple synchronous file system utilities without exposing libuv.
 */

#ifndef SALTS_FS_H
#define SALTS_FS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Caller-owned byte buffer with no legacy format.
typedef struct {
  char *base;
  size_t len;
} salts_fs_buf_t;

// File information structure
typedef struct {
  uint64_t size;
  uint64_t atime; // Access time (microseconds since epoch)
  uint64_t mtime; // Modify time (microseconds since epoch)
  uint64_t ctime; // Change time (microseconds since epoch)
  int mode;       // File permissions
  bool is_file;
  bool is_directory;
  bool is_symlink;
} salts_fs_stat_t;

// Opaque directory enumeration handle
typedef struct salts_fs_dir_s salts_fs_dir_t;

/*
 * Secure root-relative filesystem capabilities.
 *
 * These identities intentionally hide native fd/HANDLE values. The admitted
 * host root is resolved once; all subsequent path operations are relative to
 * that retained capability and never to process cwd.
 */
typedef struct salts_fs_root_s salts_fs_root_t;
typedef struct salts_fs_root_file_s salts_fs_root_file_t;
typedef struct salts_fs_root_dir_s salts_fs_root_dir_t;
typedef struct salts_fs_root_writer_lease_s salts_fs_root_writer_lease_t;
typedef struct salts_fs_root_revision_pin_s salts_fs_root_revision_pin_t;

typedef enum {
  SALTS_FS_DIRENT_UNKNOWN = 0,
  SALTS_FS_DIRENT_FILE,
  SALTS_FS_DIRENT_DIRECTORY,
  SALTS_FS_DIRENT_SYMLINK,
  SALTS_FS_DIRENT_OTHER
} salts_fs_dirent_type_t;

typedef struct {
  const char *name;
  salts_fs_dirent_type_t type;
} salts_fs_dirent_t;

// =============================================================================
// Synchronous File Operations - simple blocking I/O
// =============================================================================

/**
 * @brief Read an entire file
 *
 * This function reads the entire contents of a file into a buffer.
 * Memory is allocated automatically and must be freed with salts_fs_buf_free().
 *
 * @param path File path to read
 * @param buf Pointer to salts_fs_buf_t where file contents will be stored
 * @return 0 on success, negative error code on failure
 *
 * @note Blocks until the entire file is read or an error occurs
 * @note Use salts_fs_buf_free() to release the buffer when done
 */
int salts_fs_read_file(const char *path, salts_fs_buf_t *buf);

/**
 * @brief Write data to a file
 *
 * This function writes a buffer of data to a file, creating it if necessary.
 *
 * @param path File path to write to
 * @param buf Pointer to salts_fs_buf_t containing data to write
 * @return 0 on success, negative error code on failure
 *
 * @note Blocks until the entire buffer is written or an error occurs
 * @note Creates the file if it doesn't exist, overwrites if it does
 */
int salts_fs_write_file(const char *path, const salts_fs_buf_t *buf);

/**
 * @brief Get file information
 *
 * This function retrieves metadata about a file or directory.
 *
 * @param path Path to file or directory to stat
 * @param stat Pointer to salts_fs_stat_t to fill with file information
 * @return 0 on success, negative error code on failure
 *
 * @note Works on both files and directories
 * @note Provides size, permissions, timestamps, and file type information
 */
int salts_fs_stat(const char *path, salts_fs_stat_t *stat);

/**
 * @brief Get file information without following symbolic links when supported
 *
 * @param path Path to file, directory, or symbolic link
 * @param stat Pointer to salts_fs_stat_t to fill with file information
 * @return 0 on success, negative error code on failure
 *
 * @note On POSIX this uses lstat(). On Windows this reports reparse-point
 *       symlinks using Win32 file attributes.
 */
int salts_fs_lstat(const char *path, salts_fs_stat_t *stat);

/**
 * @brief Change file permissions
 *
 * @param path Path to file
 * @param mode Permission bits, e.g. 0644
 * @return 0 on success, negative error code on failure
 *
 * @note Windows maps this to read-only/read-write CRT permissions.
 */
int salts_fs_chmod(const char *path, int mode);

// Access check flags for salts_fs_access()
#define SALTS_FS_ACCESS_EXISTS 0x00
#define SALTS_FS_ACCESS_READ   0x01
#define SALTS_FS_ACCESS_WRITE  0x02
#define SALTS_FS_ACCESS_EXEC   0x04

/**
 * @brief Check file accessibility
 *
 * @param path Path to check
 * @param mode SALTS_FS_ACCESS_* bitmask, or SALTS_FS_ACCESS_EXISTS
 * @return 0 if accessible, negative error code otherwise
 *
 * @note Windows CRT access checks read/write/existence. Execute is treated as
 *       existence on Windows because executable permission is not a mode bit.
 */
int salts_fs_access(const char *path, int mode);

/**
 * @brief Create a symbolic link
 *
 * @param target Link target path
 * @param link_path Path of the symlink to create
 * @param is_directory Non-zero when target is a directory
 * @return 0 on success, negative error code on failure
 */
int salts_fs_symlink(const char *target, const char *link_path, int is_directory);

/**
 * @brief Read a symbolic link target into buffer
 *
 * @param path Symlink path
 * @param buffer Destination buffer
 * @param buffer_size Destination size including trailing NUL
 * @return Number of bytes written, or negative error code on failure
 *
 * @note POSIX returns the symlink payload. Windows returns the resolved final
 *       path for the reparse point because Win32 does not expose POSIX readlink
 *       semantics through the CRT.
 */
int salts_fs_readlink(const char *path, char *buffer, size_t buffer_size);

/**
 * @brief Create a directory
 *
 * @param path Directory path to create
 * @param mode Directory permissions (e.g., 0755)
 * @return 0 on success, negative error code on failure
 *
 * @note Parent directories must exist
 * @note Permissions are ignored on Windows platforms
 */
int salts_fs_mkdir(const char *path, int mode);

/**
 * @brief Remove a directory
 *
 * @param path Directory path to remove
 * @return 0 on success, negative error code on failure
 *
 * @note Directory must be empty
 */
int salts_fs_rmdir(const char *path);

/**
 * @brief Open a directory for enumeration
 *
 * @param path Directory path to enumerate
 * @param dir Pointer that receives an owned directory handle
 * @return 0 on success, negative error code on failure
 *
 * @note The returned handle must be released with salts_fs_closedir().
 * @note A directory handle is not safe for concurrent reads.
 */
int salts_fs_opendir(const char *path, salts_fs_dir_t **dir);

/**
 * @brief Read the next directory entry
 *
 * @param dir Directory handle from salts_fs_opendir()
 * @param entry Pointer that receives the next entry
 * @return 1 when an entry is returned, 0 at end of directory, or a negative
 *         error code on failure
 *
 * @note The "." and ".." entries are skipped.
 * @note entry->name is borrowed from dir and remains valid only until the next
 *       salts_fs_readdir() call or salts_fs_closedir(). Copy it to retain it.
 * @note type may be SALTS_FS_DIRENT_UNKNOWN when the platform does not expose
 *       a file type during enumeration. Use salts_fs_lstat() when required.
 */
int salts_fs_readdir(salts_fs_dir_t *dir, salts_fs_dirent_t *entry);

/**
 * @brief Close a directory enumeration handle
 *
 * @param dir Directory handle from salts_fs_opendir()
 * @return 0 on success, negative error code on failure
 */
int salts_fs_closedir(salts_fs_dir_t *dir);

/**
 * @brief Delete a file
 *
 * @param path File path to delete
 * @return 0 on success, negative error code on failure
 *
 * @note Only works on files, not directories
 */
int salts_fs_unlink(const char *path);

// =============================================================================
// File System Utilities - cross-platform helpers
// =============================================================================

/**
 * @brief Initialize a file system buffer
 *
 * @param base Pointer to the buffer memory
 * @param len Length of the buffer in bytes
 * @return Initialized salts_fs_buf_t structure
 */
salts_fs_buf_t salts_fs_buf_init(char *base, size_t len);

/**
 * @brief Free a file system buffer allocated by Salts
 *
 * @param buf Pointer to salts_fs_buf_t to free
 *
 * @note Only call this on buffers allocated by Salts functions
 */
void salts_fs_buf_free(salts_fs_buf_t *buf);

/**
 * @brief Get temporary directory path (cross-platform)
 *
 * @param buffer Buffer to store the directory path
 * @param buffer_size Size of the buffer in bytes
 * @return 0 on success, negative error code on failure
 */
int salts_fs_get_tmpdir(char *buffer, size_t buffer_size);

/**
 * @brief Check if a path is absolute
 *
 * @param path Path to check
 * @return true if path is absolute, false if relative
 */
bool salts_fs_path_is_absolute(const char *path);

/**
 * @brief Join two path components into a single path
 *
 * @param result Buffer to store the joined path
 * @param result_size Size of the result buffer in bytes
 * @param base Base path component
 * @param path Relative path component to append
 * @return 0 on success, -1 if result buffer is too small
 */
int salts_fs_path_join(char *result, size_t result_size, const char *base,
                                 const char *path);

/**
 * @brief Extract directory component from a path
 *
 * @param path Original path
 * @param dirname Buffer to store the directory component
 * @param dirname_size Size of the dirname buffer in bytes
 * @return 0 on success, -1 if dirname buffer is too small
 */
int salts_fs_path_dirname(const char *path, char *dirname, size_t dirname_size);

/**
 * @brief Extract filename component from a path
 *
 * @param path Original path
 * @param basename Buffer to store the filename component
 * @param basename_size Size of the basename buffer in bytes
 * @return 0 on success, -1 if basename buffer is too small
 */
int salts_fs_path_basename(const char *path, char *basename, size_t basename_size);

// File system constants
#define SALTS_FS_MAX_PATH 260
#define SALTS_FS_DEFAULT_MODE 0644

// File open flags
#define SALTS_FS_O_RDONLY 0x0001
#define SALTS_FS_O_WRONLY 0x0002
#define SALTS_FS_O_RDWR 0x0004
#define SALTS_FS_O_CREAT 0x0100
#define SALTS_FS_O_TRUNC 0x0200
#define SALTS_FS_O_APPEND 0x0400

// Advisory file lock flags for salts_fs_lock()
#define SALTS_FS_LOCK_SHARED    0x01
#define SALTS_FS_LOCK_EXCLUSIVE 0x02
#define SALTS_FS_LOCK_NONBLOCK  0x04

// File handle type
typedef int salts_file_t;
#define SALTS_INVALID_FILE (-1)

// =============================================================================
// Streaming File Operations - for incremental I/O
// =============================================================================

/**
 * @brief Open a file
 *
 * @param path File path to open
 * @param flags Open flags (SALTS_FS_O_*)
 * @param mode File permissions for new files (e.g., 0644)
 * @return File handle on success, SALTS_INVALID_FILE on failure
 */
salts_file_t salts_fs_open(const char *path, int flags, int mode);

/**
 * @brief Read from an open file
 *
 * @param fd File handle from salts_fs_open
 * @param buf Buffer to store read data
 * @param len Maximum number of bytes to read
 * @return Number of bytes read, or negative error code
 *
 * @note len must be <= INT_MAX because the return type is int
 */
int salts_fs_read(salts_file_t fd, char *buf, size_t len);

/**
 * @brief Read from an open file at a specific offset (thread-safe)
 *
 * Uses positional read — does not modify the file descriptor's position.
 * Safe to call concurrently from multiple threads on the same fd.
 *
 * @param fd File handle from salts_fs_open
 * @param buf Buffer to store read data
 * @param len Maximum number of bytes to read
 * @param offset Byte offset in the file to read from
 * @return Number of bytes read, or negative error code
 *
 * @note len must be <= INT_MAX because the return type is int
 */
int salts_fs_pread(salts_file_t fd, char *buf, size_t len, int64_t offset);

/**
 * @brief Write to an open file at a specific offset
 *
 * @param fd File handle from salts_fs_open
 * @param data Data to write
 * @param len Number of bytes to write
 * @param offset Byte offset in the file to write to
 * @return Number of bytes written, or negative error code
 *
 * @note len must be <= INT_MAX because the return type is int
 */
int salts_fs_pwrite(salts_file_t fd, const char *data, size_t len, int64_t offset);

/**
 * @brief Write to an open file
 *
 * @param fd File handle from salts_fs_open
 * @param data Data to write
 * @param len Number of bytes to write
 * @return Number of bytes written, or negative error code
 *
 * @note len must be <= INT_MAX because the return type is int
 */
int salts_fs_write(salts_file_t fd, const char *data, size_t len);

/**
 * @brief Close an open file
 *
 * @param fd File handle to close
 * @return 0 on success, negative error code on failure
 */
int salts_fs_close(salts_file_t fd);

/**
 * @brief Truncate or extend a file to a specified length
 *
 * @param fd File handle
 * @param length New length in bytes
 * @return 0 on success, negative error code on failure
 */
int salts_fs_ftruncate(salts_file_t fd, int64_t length);

/**
 * @brief Flush file buffers to disk
 *
 * @param fd File handle to flush
 * @return 0 on success, negative error code on failure
 */
int salts_fs_fsync(salts_file_t fd);

/**
 * @brief Acquire an advisory byte-range lock
 *
 * @param fd File handle
 * @param flags SALTS_FS_LOCK_SHARED or SALTS_FS_LOCK_EXCLUSIVE, optionally
 *              OR'd with SALTS_FS_LOCK_NONBLOCK
 * @param offset Start offset
 * @param len Length to lock; 0 means to EOF
 * @return 0 on success, negative error code on failure
 *
 * @note POSIX uses fcntl record locks. Windows uses LockFileEx. Locks are
 *       advisory and process/OS semantics differ across platforms.
 */
int salts_fs_lock(salts_file_t fd, int flags, int64_t offset, uint64_t len);

/**
 * @brief Release an advisory byte-range lock
 *
 * @param fd File handle
 * @param offset Start offset
 * @param len Length to unlock; 0 means to EOF
 * @return 0 on success, negative error code on failure
 */
int salts_fs_unlock(salts_file_t fd, int64_t offset, uint64_t len);

/**
 * @brief Rename/move a file, replacing an existing destination file
 *
 * @param old_path Current file path
 * @param new_path New file path
 * @return 0 on success, negative error code on failure
 *
 * @note The source and destination must be on the same filesystem.
 */
int salts_fs_rename(const char *old_path, const char *new_path);

/**
 * @brief Publication state returned by salts_fs_replace_durable().
 *
 * NOT_PUBLISHED means Salts did not perform the namespace replacement.
 * PUBLISHED_DURABLE means the replacement completed and the platform durability
 * step succeeded.
 * DURABILITY_UNKNOWN means the namespace replacement completed, but a later
 * durability step failed so callers must not assume that retrying the same
 * publication is safe.
 */
typedef enum salts_fs_replace_state_e {
  SALTS_FS_REPLACE_NOT_PUBLISHED = 0,
  SALTS_FS_REPLACE_PUBLISHED_DURABLE = 1,
  SALTS_FS_REPLACE_DURABILITY_UNKNOWN = 2
} salts_fs_replace_state_t;

/**
 * @brief Durably publish a completed staging file over a destination file.
 *
 * The staging file and destination must be on the same filesystem/volume.
 * Cross-filesystem copy is never used as a fallback.
 *
 * POSIX: flush staging, rename, then fsync the destination parent directory.
 * Windows: flush staging, then MoveFileEx(REPLACE_EXISTING|WRITE_THROUGH).
 *
 * @param staging_path Completed staging file. Consumed by a successful
 *                     namespace replacement.
 * @param destination_path Destination file to replace/create.
 * @param state Required output publication state; initialized to
 *              SALTS_FS_REPLACE_NOT_PUBLISHED before validation.
 * @return 0 only when state is SALTS_FS_REPLACE_PUBLISHED_DURABLE. A negative
 *         error may accompany NOT_PUBLISHED or DURABILITY_UNKNOWN.
 */
int salts_fs_replace_durable(
    const char *staging_path, const char *destination_path,
    salts_fs_replace_state_t *state);

/**
 * @brief Get current file position
 *
 * @param fd File handle
 * @return Current position, or negative error code
 */
int64_t salts_fs_tell(salts_file_t fd);

/**
 * @brief Seek to position in file
 *
 * @param fd File handle
 * @param offset Offset from whence
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return New position, or negative error code
 */
int64_t salts_fs_seek(salts_file_t fd, int64_t offset, int whence);

// =============================================================================
// Secure root-relative filesystem capability
// =============================================================================

/*
 * Root-relative paths use '/' or '\\' as separators on every platform and
 * must consist only of non-empty ordinary components. Absolute paths, ".",
 * "..", repeated/trailing separators and Windows drive/ADS ':' components are
 * rejected. V1 never follows symlinks/reparse points below the admitted root:
 * lstat may observe a final link, while stat/open reject it.
 *
 * Component traversal is handle/fd-relative. This is a security boundary; do
 * not emulate it with root + relative_path string concatenation.
 */
#define SALTS_FS_ROOT_COMPONENT_MAX 1024u
#define SALTS_FS_ROOT_DEPTH_MAX 64u
#define SALTS_FS_ROOT_DIRENT_NAME_MAX 1024u

typedef struct salts_fs_root_dirent_s {
  uint64_t next_cookie;
  size_t name_length;
  salts_fs_dirent_type_t type;
} salts_fs_root_dirent_t;

/* Admit one host directory as an opaque capability. */
int salts_fs_root_open(
    const char *host_root, salts_fs_root_t **out_root);
int salts_fs_root_close(salts_fs_root_t *root);

/* Inspect the admitted root itself. */
int salts_fs_root_fstat(
    const salts_fs_root_t *root, salts_fs_stat_t *out_stat);

/* Open/operate on a regular file without exposing a native handle. */
int salts_fs_root_file_open(
    const salts_fs_root_t *root, const char *relative_path,
    int flags, int mode, salts_fs_root_file_t **out_file);
int salts_fs_root_file_close(salts_fs_root_file_t *file);
/* Flush the byte contents of a root-relative opened staging file.
 * Requires a writable native file handle on platforms that demand it. */
int salts_fs_root_file_fsync(salts_fs_root_file_t *file);
int salts_fs_root_file_read(
    salts_fs_root_file_t *file, char *buffer, size_t length);
int salts_fs_root_file_write(
    salts_fs_root_file_t *file, const char *data, size_t length);
int64_t salts_fs_root_file_seek(
    salts_fs_root_file_t *file, int64_t offset, int whence);
int64_t salts_fs_root_file_tell(salts_fs_root_file_t *file);
int salts_fs_root_file_stat(
    const salts_fs_root_file_t *file, salts_fs_stat_t *out_stat);

#define SALTS_FS_ROOT_MUTATION_VERSION 1
/* Root-file-open only: requires O_CREAT, fails with EEXIST without truncation. */
#define SALTS_FS_ROOT_O_EXCL 0x0800
/* Toggle append without reopening. Single owner; returns 0 or negative errno,
 * preserving flags on failure. Each append write targets EOF atomically. */
int salts_fs_root_file_set_append(salts_fs_root_file_t *file, bool append);
/* Atomically rename/replace between admitted roots. Parent components never
 * follow links; a final link is moved as an entry. Returns 0 or negative errno,
 * including -EXDEV across filesystems. Open handles retain their identities. */
int salts_fs_root_rename(
    const salts_fs_root_t *source_root, const char *source_path,
    const salts_fs_root_t *target_root, const char *target_path);

/* Atomic namespace-only NO-REPLACE FILE move, same admitted root.
 * Linux: renameat2(RENAME_NOREPLACE). macOS: renameatx_np(RENAME_EXCL).
 * Windows: native handle-relative NtSetInformationFile with the original
 * admitted RootDirectory and FILE_RENAME_INFO.ReplaceIfExists = FALSE.
 *
 * On success the name was moved; no parent directory flush has been done.
 * It is NOT a durable publish and MUST NOT be used to mark a generation
 * PUBLISHED_DURABLE. A caller must own an exclusive, single-link staging
 * file; hostile concurrent renames/writes are outside this primitive.
 * Missing kernel/filesystem support fails -ENOSYS; NEVER check then rename
 * or silently fall back to a replacing rename.
 */
int salts_fs_root_rename_file_noreplace(
    const salts_fs_root_t *root, const char *source_path,
    const char *destination_path);

/* M0: durably publish one completed REGULAR staging FILE at an ABSENT
 * immutable destination. Uses the same admitted root and relative paths;
 * never follows intermediate symlinks/reparse points, never replaces a
 * present destination, never copies across filesystems. No fallback rename.
 *
 * Linux: openat(O_NOFOLLOW), fsync staging, renameat2(RENAME_NOREPLACE),
 *        fsync destination AND source parent directories.
 * macOS: same, but renameatx_np(RENAME_EXCL).
 * Windows: explicitly -ENOSYS / NOT_PUBLISHED until root-relative native
 *        durability and Unicode rename/flush semantics are qualified.
 *
 * The output state is always set to NOT_PUBLISHED before validation.
 * Once the namespace move succeeds, later parent-sync failure yields
 * DURABILITY_UNKNOWN with a negative return: inspect/reconcile the named
 * revision before retrying; NEVER re-use the requested generation blindly.
 * A successful return requires PUBLISHED_DURABLE.
 *
 * Writer contract: a UNIQUE staging inode with no other hard links, kept
 * exclusively owned and unchanged for the duration of publication; this
 * primitive does not authenticate a publisher, mint monotonic generations,
 * enforce the read-only policy, or manage reader leases/GC. Existing opened
 * root-file handles pin the inode; the caller must preserve a published
 * revision until all readers have finished (Windows GC is still unqualified).
 */
int salts_fs_root_publish_file_noreplace(
    const salts_fs_root_t *root, const char *staging_path,
    const char *revision_path, salts_fs_replace_state_t *state);

/* Atomically switch a small, completed CURRENT manifest from one regular
 * staging file, replacing the current regular file if it already exists.
 * Unlike immutable generation data, the CURRENT pointer is replaceable.
 * This API does NOT mint a generation, authenticate manifest bytes, perform
 * an optimistic compare-and-swap or grant reader leases: exactly ONE trusted
 * write owner must serialize the current pointer and publish immutable data
 * BEFORE switching to it. Readers with an opened previous manifest handle
 * retain that exact content across the switch on supported POSIX systems.
 *
 * Same admitted root-relative no-follow path traversal, same filesystem,
 * regular single-link staging file, complete staging fsync, native renameat,
 * dest/source parent fsync. No user-space copy, read-modify-rename race,
 * absolute path construction or fallback. The final destination entry is
 * replaced, never followed even when it is a symlink. If native rename
 * succeeds but a later verification/sync fails, state is DURABILITY_UNKNOWN
 * and caller must inspect the full manifest and its revision files.
 *
 * Windows: strictly -ENOSYS / NOT_PUBLISHED before namespace change until
 * native root-relative parent durability and crash recovery are qualified.
 * PUBLISHED_DURABLE success is POSIX/local filesystem only and does not
 * prove physical power-cut behavior on every filesystem/volume.
 */
int salts_fs_root_replace_file_durable(
    const salts_fs_root_t *root, const char *staging_path,
    const char *current_path, salts_fs_replace_state_t *state);

/* One trusted writer's crash-stable generation HIGH-WATER in an already
 * admitted root. Record format is exactly:
 * "SFS-GEN1 " + 20 decimal digits + " " + 32 lowercase hex request-id + "\n".
 * Generation 0 means no record yet; zero is never published/reserved.
 *
 * Each (publisher, asset) MUST have a distinct stable highwater pathname.
 * The publisher itself must arbitrate single-writer ownership, keep this
 * record forever (including after immutable content GC), preserve the root,
 * and never submit an old request-id as a NEW request. This is NOT a CAS,
 * global actor, authenticated ID, or a replay journal for arbitrary old IDs.
 * Only the CURRENT highwater's request ID is idempotently recognized.
 *
 * read(): -ENOENT if highwater does not exist; outputs cleared on failure,
 * -EINVAL for malformed/torn/unsafe records, no partial generation accepted.
 *
 * reserve(): validates the token and reads CURRENT highwater, allocates
 * N+1 (rejects overflow), then creates a UNIQUE staging file O_EXCL,
 * writes+fsyncs its exact record, and uses the existing root-relative
 * durable file replace. A repeated latest request first reflushes the
 * highwater file and BOTH original source/destination parents, then checks
 * the exact same generation/ID again. Visible bytes after an UNKNOWN
 * return ALONE never authorize a durable-success claim. Reconciliation
 * must reuse the original staging path, including its parent directory.
 * A successful return reserves N+1 BEFORE the caller publishes immutable
 * content, even if content later aborts.
 * On a failed staging write the source name may remain and MUST be
 * reconciled/cleaned by the single owner, not silently discarded.
 * A post-rename failure is DURABILITY_UNKNOWN: reread the highwater
 * and reconcile request ID/generation; NEVER guess or blindly retry.
 * request_id: exactly 32 lowercase ASCII hex bytes plus trailing NUL.
 * stage and highwater must be distinct root-relative file paths.
 *
 * Windows: reserve() is explicitly -ENOSYS/NOT_PUBLISHED until
 * root-relative durable replacement is qualified; read() is supported
 * for inspection only. POSIX local-volume durable publication follows
 * the existing file+both-parent fsync contract. Physical power-loss,
 * concurrent external writers and arbitrary old-request idempotency are
 * separate gates.
 */
int salts_fs_root_generation_read(
    const salts_fs_root_t *root, const char *highwater_path,
    uint64_t *out_generation, char out_request_id[33]);
int salts_fs_root_reserve_generation(
    const salts_fs_root_t *root,
    const char *highwater_path, const char *unique_staging_path,
    const char *request_id,
    uint64_t *out_generation, salts_fs_replace_state_t *state);

/* M9: historical request-ID reservation journal for a SOLE trusted writer.
 * All names derive deterministically from the highwater path:
 *   <highwater>.r.<32-lowerhex-id> = immutable 63-byte SFS-REQ1 entry
 *   <highwater>.s.<32-lowerhex-id> = exclusive journal staging file
 * Both are in the same already-admitted parent as highwater.
 *
 * The supplied HIGHWATER staging path MUST be in that same parent and
 * uniquely owned by this request. A journal entry is published with the
 * original no-replace durable file API, *after* the highwater was reserved.
 * Every new reservation reconciles the immediately previous highwater
 * request's journal BEFORE advancing the highwater. This invariant means
 * at most the latest entry may be missing after a crash; older durable
 * journal entries are never discarded, even after content GC.
 *
 * A previously recorded historical request returns its ORIGINAL generation
 * (including after A->B->A), without creating any new stage or incrementing
 * the highwater. A crash after highwater replacement but before journal
 * rename is recovered from the latest highwater record. A visible journal
 * after UNKNOWN gets re-synced and checked before durable acknowledgement.
 * Any torn, mismatched or malformed entry fails closed, no reset/reuse.
 *
 * One trusted single writer, trusted stable root and immutable journal
 * ownership are HARD preconditions. No multi-writer CAS/fencing, cryptographic
 * identity, or power-loss guarantee on arbitrary filesystems. An external
 * writer deleting an old journal record violates this contract. Only
 * the latest highwater lacking a journal can be repaired automatically.
 *
 * On Windows this operation is explicitly unsupported (-ENOSYS /
 * NOT_PUBLISHED) before mutation because durable highwater and no-replace
 * publication are not qualified there.
 */
int salts_fs_root_reserve_generation_recorded(
    const salts_fs_root_t *root,
    const char *highwater_path, const char *highwater_staging_path,
    const char *request_id, uint64_t *out_generation,
    salts_fs_replace_state_t *state);

/* M10: live cross-process, cooperative SOLE-WRITER lease.
 * Each highwater has a permanent same-parent <highwater>.writer file,
 * held through the ENTIRE producer transaction (generation reservation,
 * immutable revision publication and CURRENT switch). Never unlink,
 * rename or replace this lock file while a publisher namespace exists,
 * including after GC or process restart; otherwise a second inode could
 * admit an uncoordinated writer.
 *
 * Linux/macOS: native nonblocking flock on root-relative no-follow
 * regular single-link file; Windows: LockFileEx on its NT handle.
 * Acquire rejects competing processes with -EBUSY; OS releases the lock
 * when the final owning handle is closed/process exits. A forked child
 * cannot reuse the parent's lease as its own writer capability.
 *
 * The acquired lease is an opaque in-process capability tied to the
 * exact admitted root and highwater path. It DOES NOT authenticate
 * hostile external processes, enforce a distributed-fencing epoch,
 * guarantee NFS/remote-FS lock coherence or provide crash durability
 * for the lock namespace. It does not turn file operations into CAS.
 * Root must remain open and lease must be released before root close.
 */
int salts_fs_root_writer_lease_acquire(
    const salts_fs_root_t *root, const char *highwater_path,
    salts_fs_root_writer_lease_t **out_lease);
int salts_fs_root_writer_lease_release(salts_fs_root_writer_lease_t *lease);

/* Prefer this strict owner-scoped entry point for production M9 journals:
 * a live writer lease is required, tied to its stored highwater/root.
 * The existing lower-level recorded reservation remains a trusted-owner
 * primitive for source migration tests, not a competing FS implementation.
 * Retain the lease through content and CURRENT publication after reserve.
 */
int salts_fs_root_reserve_generation_recorded_owned(
    const salts_fs_root_writer_lease_t *lease,
    const char *unique_highwater_staging_path,
    const char *request_id, uint64_t *out_generation,
    salts_fs_replace_state_t *state);

/* M11 — cooperative cross-process immutable revision pin and retirement.
 * One permanent, root-relative "<revision>.readers" file is used for each
 * immutable generation-specific revision. NEVER rename/unlink/replace that
 * lock file during production GC; the same inode is the lock identity.
 *
 * pin_open: acquire a native SHARED lock (nonblocking) on .readers BEFORE
 * opening the exact generation file, then hold both handles for the entire
 * read. The caller receives an opaque, process-owned pin, not a pathname
 * lookup that may follow a newer CURRENT. pin_read/seek never reopen.
 * Missing revision returns -ENOENT; competing GC exclusive lock returns
 * -EBUSY. An already-open pin remains valid after CURRENT changes.
 *
 * retire: acquire the same version's native EXCLUSIVE nonblocking lock
 * BEFORE inspecting/unlinking the generation-specific regular file.
 * A live managed reader produces -EBUSY with the destination unchanged.
 * Only after all readers release (or owning processes exit) may GC unlink.
 * GC does NOT remove .readers, the journal, CURRENT, or generation highwater.
 * If unlink happened but a later native close fails, reconcile the name;
 * do not infer the version still exists from a negative result.
 *
 * Both operations reuse original no-follow Salts::FS root capabilities;
 * Linux/macOS use flock(LOCK_SH/LOCK_EX|LOCK_NB), Windows LockFileEx
 * shared/exclusive byte locks. This is NOT a lease for CURRENT or a
 * distributed/remote-FS lock protocol. All readers AND GC must cooperate;
 * untrusted writers with existing handles can still mutate an inode.
 * Pin and root must remain in the same process; close pin before root.
 *
 * Windows accepts this namespace/handle lifecycle ONLY. Durable Windows
 * version publication, CURRENT and highwater remain -ENOSYS.
 */
int salts_fs_root_revision_pin_open(
    const salts_fs_root_t *root, const char *revision_path,
    salts_fs_root_revision_pin_t **out_pin);
int salts_fs_root_revision_pin_read(
    salts_fs_root_revision_pin_t *pin, char *buffer, size_t bytes);
int64_t salts_fs_root_revision_pin_seek(
    salts_fs_root_revision_pin_t *pin, int64_t offset, int whence);
int salts_fs_root_revision_pin_close(salts_fs_root_revision_pin_t *pin);
int salts_fs_root_revision_retire(
    const salts_fs_root_t *root, const char *revision_path);

/*
 * stat is secure no-follow in capability v1: a final symlink/reparse point
 * returns -ELOOP. lstat reports the final link itself. Intermediate links are
 * always rejected.
 */
int salts_fs_root_stat(
    const salts_fs_root_t *root, const char *relative_path,
    salts_fs_stat_t *out_stat);
int salts_fs_root_lstat(
    const salts_fs_root_t *root, const char *relative_path,
    salts_fs_stat_t *out_stat);

int salts_fs_root_mkdir(
    const salts_fs_root_t *root, const char *relative_path, int mode);
int salts_fs_root_rmdir(
    const salts_fs_root_t *root, const char *relative_path);
int salts_fs_root_unlink(
    const salts_fs_root_t *root, const char *relative_path);

/*
 * Bounded directory enumeration. Names are copied into caller-owned storage.
 * cookie 0 starts/restarts enumeration; next_cookie resumes from the returned
 * entry. Cookies are stable only while the directory contents are unchanged.
 */
/* Enumerate the admitted root directory itself without inventing "." syntax. */
int salts_fs_root_opendir_self(
    const salts_fs_root_t *root, salts_fs_root_dir_t **out_dir);
int salts_fs_root_opendir(
    const salts_fs_root_t *root, const char *relative_path,
    salts_fs_root_dir_t **out_dir);
int salts_fs_root_readdir(
    salts_fs_root_dir_t *dir, uint64_t cookie,
    char *name_buffer, size_t name_buffer_size,
    salts_fs_root_dirent_t *out_entry);
int salts_fs_root_closedir(salts_fs_root_dir_t *dir);

#ifdef __cplusplus
}
#endif

#endif // SALTS_FS_H
