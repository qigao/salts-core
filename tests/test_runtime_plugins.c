/*
 * Load two CNet consumer plugins independently, unload A with a retained
 * Core buffer outstanding, then release it from B. Repeat to catch stale
 * module-local owner identity after unload/reload.
 */
#include <cmeta_buffer.h>
#include <salts/thread.h>
#include <salts/error_codes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
typedef HMODULE test_module;
static test_module load_module(const char *path) { return LoadLibraryA(path); }
static void *load_symbol(test_module m, const char *name) {
  return (void *)(uintptr_t)GetProcAddress(m, name);
}
static void unload_module(test_module m) { if (m) (void)FreeLibrary(m); }
#else
#  include <dlfcn.h>
typedef void *test_module;
static test_module load_module(const char *path) {
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}
static void *load_symbol(test_module m, const char *name) {
  return dlsym(m, name);
}
static void unload_module(test_module m) { if (m) (void)dlclose(m); }
#endif

typedef mem_pool_t *(*pool_fn)(void);
typedef const void *(*token_fn)(void);
typedef mem_slice_t (*alloc_fn)(void);
typedef void (*release_fn)(mem_slice_t *);
typedef int (*smoke_fn)(void);

typedef struct plugin {
  test_module module;
  pool_fn pool;
  token_fn token;
  alloc_fn alloc;
  release_fn release;
  smoke_fn smoke;
} plugin;

static int open_plugin(plugin *p, const char *path) {
  p->module = load_module(path);
  if (!p->module) return -1;
  p->pool = (pool_fn)load_symbol(p->module, "saltscore_test_plugin_pool");
  p->token = (token_fn)load_symbol(p->module, "saltscore_test_plugin_thread");
  p->alloc = (alloc_fn)load_symbol(p->module, "saltscore_test_plugin_alloc");
  p->release = (release_fn)load_symbol(p->module, "saltscore_test_plugin_release");
  p->smoke = (smoke_fn)load_symbol(p->module, "saltscore_test_plugin_cnet_smoke");
  return p->pool && p->token && p->alloc && p->release && p->smoke ? 0 : -1;
}
static void close_plugin(plugin *p) {
  unload_module(p->module);
  p->module = NULL;
}

int main(int argc, char **argv) {
  int result = 0, round;
  mem_pool_t *const pool = mem_global();
  const void *const token = cmeta_thread_current_token();
  if (argc != 3 || !pool || !token) return 1;

  for (round = 0; round != 8; ++round) {
    plugin a = {0}, b = {0};
    mem_slice_t retained = {0};

#define VERIFY(e) do { if (!(e)) { \
  fprintf(stderr, "two CNet plugins failure line %d, round %d: %s\n", \
          __LINE__, round, #e); result = __LINE__; goto cleanup; \
} } while (0)
    VERIFY(open_plugin(&a, argv[1]) == 0);
    VERIFY(open_plugin(&b, argv[2]) == 0);
    VERIFY(a.pool() == pool && b.pool() == pool);
    VERIFY(a.token() == token && b.token() == token);
    VERIFY(a.smoke() == SALTS_OK && b.smoke() == SALTS_OK);
    retained = a.alloc();
    VERIFY(retained.buffer != NULL && retained.length == 1u);
    VERIFY(mem_buffer_pool(retained.buffer) == pool);
    VERIFY((unsigned char)retained.data[0] == 0x7bu);

    /* Plugin A and its direct CNet link are removed while the buffer lives. */
    close_plugin(&a);
    VERIFY(mem_global() == pool && b.pool() == pool && b.token() == token);
    VERIFY((unsigned char)retained.data[0] == 0x7bu);
    b.release(&retained);
    VERIFY(retained.buffer == NULL);
    close_plugin(&b);
    VERIFY(mem_global() == pool && cmeta_thread_current_token() == token);
    continue;

cleanup:
    if (retained.buffer) {
      if (b.module && b.release) b.release(&retained);
      else mem_slice_release(&retained);
    }
    close_plugin(&a);
    close_plugin(&b);
    return result;
#undef VERIFY
  }
  return 0;
}
