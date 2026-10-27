/* SPDX-License-Identifier: Apache-2.0
 * Load two INDEPENDENT test plugins that import the SAME candidate CNet DSO.
 * The host must not link CNet or Crypto. All Provider/NativeIO operations
 * are performed only by code inside the shared candidate, never by plugins.
 * This tests physical archive ownership, not just identical target names.
 */
#include "m6_shared_owner.h"
#include <cmeta_buffer.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE m6_library;
static m6_library load_library(const char *path) { return LoadLibraryA(path); }
static void *load_symbol(m6_library lib, const char *name) {
    return (void *)(uintptr_t)GetProcAddress(lib, name);
}
static void close_library(m6_library lib) { if (lib) (void)FreeLibrary(lib); }
#else
#include <dlfcn.h>
typedef void *m6_library;
static m6_library load_library(const char *path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}
static void *load_symbol(m6_library lib, const char *name) {
    return dlsym(lib, name);
}
static void close_library(m6_library lib) { if (lib) (void)dlclose(lib); }
#endif

typedef int (*acquire_fn)(cnet_m6_lease **, const void **, uint64_t *);
typedef int (*release_fn)(cnet_m6_lease **);
typedef int (*snapshot_fn)(const void **, uint32_t *, uint32_t *, uint64_t *);
typedef int (*smoke_fn)(cnet_m6_lease *);
typedef mem_pool_t *(*pool_fn)(void);
typedef const void *(*thread_fn)(void);
typedef struct m6_plugin {
    m6_library module;
    acquire_fn acquire;
    release_fn release;
    snapshot_fn snapshot;
    smoke_fn smoke;
    pool_fn pool;
    thread_fn thread;
} m6_plugin;

static int open_plugin(m6_plugin *p, const char *path)
{
    if (!p) return -1;
    memset(p, 0, sizeof(*p));
    p->module = load_library(path);
    if (!p->module) return -2;
    p->acquire = (acquire_fn)load_symbol(p->module, "m6_plugin_acquire");
    p->release = (release_fn)load_symbol(p->module, "m6_plugin_release");
    p->snapshot = (snapshot_fn)load_symbol(p->module, "m6_plugin_snapshot");
    p->smoke = (smoke_fn)load_symbol(p->module, "m6_plugin_native_smoke");
    p->pool = (pool_fn)load_symbol(p->module, "m6_plugin_core_pool");
    p->thread = (thread_fn)load_symbol(p->module, "m6_plugin_thread_identity");
    if (!p->acquire || !p->release || !p->snapshot || !p->smoke ||
        !p->pool || !p->thread) {
        close_library(p->module);
        memset(p, 0, sizeof(*p));
        return -3;
    }
    return 0;
}
static void close_plugin(m6_plugin *p)
{
    if (p && p->module) close_library(p->module);
    if (p) memset(p, 0, sizeof(*p));
}

#define ASSERT(test) do { if (!(test)) { \
    fprintf(stderr, "M6 two CNet plugins failed line %d: %s\n", __LINE__, #test); \
    result = __LINE__; goto cleanup; \
} } while(0)

enum { ROUNDS = 8, CONCURRENT_WORKERS = 4 };

typedef struct worker {
    acquire_fn acquire;
    release_fn release;
    snapshot_fn snapshot;
    const void *expected_identity;
    uint64_t expected_epoch;
    int completed;
} worker;
static void worker_entry(void *context)
{
    worker *w = (worker *)context;
    cnet_m6_lease *lease = NULL;
    const void *identity = NULL;
    uint64_t epoch = 0u;
    if (w->acquire(&lease, &identity, &epoch) != SALTS_OK ||
        lease == NULL || identity != w->expected_identity ||
        epoch != w->expected_epoch) goto done;
    if (w->release(&lease) != SALTS_OK || lease != NULL) goto done;
    w->completed = 1;
done:
    if (lease) (void)w->release(&lease);
}

int main(int argc, char **argv)
{
    const mem_pool_t *const core_pool = mem_global();
    const void *const core_thread = cmeta_thread_current_token();
    unsigned round;
    if (argc != 3 || !core_pool || !core_thread) return 2;
    for (round = 0u; round != ROUNDS; ++round) {
        m6_plugin a = {0}, b = {0};
        cnet_m6_lease *first = NULL, *second = NULL, *reopened = NULL;
        const void *identity_a = NULL, *identity_b = NULL, *snapshot_id = NULL;
        uint32_t refs = UINT32_MAX, sessions = UINT32_MAX;
        uint64_t epoch_a = 0u, epoch_b = 0u, epoch_snapshot = 0u;
        cmeta_thread_t threads[CONCURRENT_WORKERS] = {0};
        worker workers[CONCURRENT_WORKERS] = {{0}};
        size_t joined = 0u, started = 0u, i;
        int join_failure = 0, result = 0;

        ASSERT(open_plugin(&a, argv[1]) == 0);
        ASSERT(open_plugin(&b, argv[2]) == 0);
        ASSERT(a.pool() == core_pool && b.pool() == core_pool);
        ASSERT(a.thread() == core_thread && b.thread() == core_thread);
        ASSERT(a.snapshot(&snapshot_id, &refs, &sessions, &epoch_snapshot)
               == SALTS_OK);
        ASSERT(refs == 0u && sessions == 0u && snapshot_id != NULL);
        ASSERT(a.acquire(&first, &identity_a, &epoch_a) == SALTS_OK);
        ASSERT(b.acquire(&second, &identity_b, &epoch_b) == SALTS_OK);
        ASSERT(first != NULL && second != NULL && first != second);
        ASSERT(identity_a == identity_b && identity_b == snapshot_id);
        ASSERT(epoch_a == epoch_b && epoch_a != 0u);
        ASSERT(b.snapshot(&snapshot_id, &refs, &sessions, &epoch_snapshot)
               == SALTS_OK);
        ASSERT(refs == 2u && sessions == 0u && epoch_snapshot == epoch_a);
        ASSERT(a.smoke(first) == SALTS_OK);
        ASSERT(b.smoke(second) == SALTS_OK);
        ASSERT(a.snapshot(&snapshot_id, &refs, &sessions, &epoch_snapshot)
               == SALTS_OK && refs == 2u && sessions == 0u);

        for (i = 0u; i < CONCURRENT_WORKERS; ++i) {
            workers[i].acquire = (i & 1u) ? a.acquire : b.acquire;
            workers[i].release = (i & 1u) ? b.release : a.release;
            workers[i].snapshot = b.snapshot;
            workers[i].expected_identity = identity_a;
            workers[i].expected_epoch = epoch_a;
            ASSERT(cmeta_thread_create(&threads[i], worker_entry,
                                       &workers[i]) == 0);
            ++started;
        }
        for (i = 0u; i < started; ++i) {
            if (cmeta_thread_join(&threads[i]) != 0) join_failure = 1;
            ++joined;
        }
        ASSERT(join_failure == 0);
        for (i = 0u; i < started; ++i) ASSERT(workers[i].completed);
        ASSERT(b.snapshot(&snapshot_id, &refs, &sessions, &epoch_snapshot)
               == SALTS_OK && refs == 2u && sessions == 0u);

        /* Deliberately unload A with its provider lease outstanding.
         * B must retire A's lease through the common DSO, without code
         * callbacks into the unloaded plugin or freeing the Provider early.
         */
        close_plugin(&a);
        ASSERT(b.release(&first) == SALTS_OK && first == NULL);
        ASSERT(b.snapshot(&snapshot_id, &refs, &sessions, &epoch_snapshot)
               == SALTS_OK && refs == 1u && sessions == 0u);
        ASSERT(b.smoke(second) == SALTS_OK);
        ASSERT(b.release(&second) == SALTS_OK && second == NULL);
        ASSERT(b.snapshot(&snapshot_id, &refs, &sessions, &epoch_snapshot)
               == SALTS_OK && refs == 0u && sessions == 0u);

        /* Reopening must create another provider epoch in the SAME loaded
         * candidate DSO; this catches a wrongly immortal/leaked Provider.
         */
        ASSERT(b.acquire(&reopened, &identity_b, &epoch_b) == SALTS_OK);
        ASSERT(identity_b == identity_a && epoch_b == epoch_a + 1u);
        ASSERT(b.release(&reopened) == SALTS_OK && reopened == NULL);
        close_plugin(&b);
        ASSERT(mem_global() == core_pool &&
               cmeta_thread_current_token() == core_thread);
        printf("M6 shared candidate DSO round %u: one Crypto provider, "
               "real CNet NativeIO, two plugins, unload/reopen PASS\n", round);
        continue;
cleanup:
        /* Always retire all created threads before unloading their code. */
        for (i = joined; i < started; ++i)
            (void)cmeta_thread_join(&threads[i]);
        if (first && b.module && b.release) (void)b.release(&first);
        else if (first && a.module && a.release) (void)a.release(&first);
        if (second && b.module && b.release) (void)b.release(&second);
        if (reopened && b.module && b.release) (void)b.release(&reopened);
        close_plugin(&a);
        close_plugin(&b);
        return result;
    }
    puts("M6 two-plugin one-Crypto-DSO original CNet ownership PASS");
    return 0;
}
