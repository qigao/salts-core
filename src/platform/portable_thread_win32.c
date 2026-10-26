#include <salts_core/platform_thread.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <process.h>
#include <stdlib.h>

/* The runtime owns both the native HANDLE and allocated storage.
 * A detached thread still retains its worker reference until it returns.
 */
struct salts_platform_thread {
    HANDLE native;
    unsigned id;
    salts_platform_thread_fn callback;
    void *argument;
    int result;
    salts_atomic32 refs;
};
struct salts_platform_mutex { SRWLOCK native; };
struct salts_platform_condition { CONDITION_VARIABLE native; };

static void release_thread(salts_platform_thread *thread) {
    uint32_t before = 0;
    (void)salts_atomic32_fetch_add(&thread->refs, UINT32_MAX,
                                    SALTS_MEMORY_SEQ_CST, &before);
    if (before == 1u) free(thread);
}
static unsigned __stdcall thread_entry(void *opaque) {
    salts_platform_thread *thread = (salts_platform_thread *)opaque;
    thread->result = thread->callback(thread->argument); /* user: cdecl, OS: stdcall */
    release_thread(thread);
    return 0;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_create(salts_platform_thread **out,
                              salts_platform_thread_fn fn, void *arg) {
    salts_platform_thread *thread;
    uintptr_t native;
    if (!out || !fn || *out) return SALTS_EINVAL;
    thread = (salts_platform_thread *)calloc(1u, sizeof(*thread));
    if (!thread) return SALTS_ENOMEM;
    thread->callback = fn;
    thread->argument = arg;
    (void)salts_atomic32_init(&thread->refs, 2u);
    native = _beginthreadex(NULL, 0, thread_entry, thread, 0, &thread->id);
    if (!native) { free(thread); return SALTS_EBUSY; }
    thread->native = (HANDLE)native;
    *out = thread;
    return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_join(salts_platform_thread **owner, int *result) {
    salts_platform_thread *thread;
    if (!owner || !(thread = *owner)) return SALTS_EINVAL;
    if (GetCurrentThreadId() == thread->id) return SALTS_EINVAL;
    if (WaitForSingleObject(thread->native, INFINITE) != WAIT_OBJECT_0) return SALTS_EBUSY;
    if (!CloseHandle(thread->native)) return SALTS_EBUSY;
    if (result) *result = thread->result;
    *owner = NULL;
    release_thread(thread);
    return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_detach(salts_platform_thread **owner) {
    salts_platform_thread *thread;
    if (!owner || !(thread = *owner)) return SALTS_EINVAL;
    if (!CloseHandle(thread->native)) return SALTS_EBUSY;
    *owner = NULL;
    release_thread(thread);
    return SALTS_OK;
}
void SALTS_CORE_PLATFORM_CALL salts_platform_thread_yield(void) { (void)SwitchToThread(); }
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_sleep_ns(uint64_t duration_ns) {
    if (duration_ns > SALTS_PLATFORM_MAX_RELATIVE_WAIT_NS) return SALTS_EINVAL;
    Sleep((DWORD)((duration_ns + 999999ull) / 1000000ull));
    return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_create(salts_platform_mutex **out) {
    salts_platform_mutex *m;
    if (!out || *out) return SALTS_EINVAL;
    m = (salts_platform_mutex *)malloc(sizeof(*m));
    if (!m) return SALTS_ENOMEM;
    InitializeSRWLock(&m->native);
    *out = m; return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_lock(salts_platform_mutex *m) {
    if (!m) return SALTS_EINVAL;
    AcquireSRWLockExclusive(&m->native); return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_unlock(salts_platform_mutex *m) {
    if (!m) return SALTS_EINVAL;
    ReleaseSRWLockExclusive(&m->native); return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_destroy(salts_platform_mutex **owner) {
    salts_platform_mutex *m;
    if (!owner || !(m = *owner)) return SALTS_EINVAL;
    /* No Windows OS primitive can guard a destroy racing a new entrant;
     * the caller must guarantee quiescence before destroy. */
    if (!TryAcquireSRWLockExclusive(&m->native)) return SALTS_EBUSY;
    ReleaseSRWLockExclusive(&m->native);
    *owner = NULL;
    free(m); return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_create(salts_platform_condition **out) {
    salts_platform_condition *c;
    if (!out || *out) return SALTS_EINVAL;
    c = (salts_platform_condition *)malloc(sizeof(*c));
    if (!c) return SALTS_ENOMEM;
    InitializeConditionVariable(&c->native);
    *out = c; return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_wait(salts_platform_condition *c, salts_platform_mutex *m) {
    if (!c || !m) return SALTS_EINVAL;
    return SleepConditionVariableSRW(&c->native, &m->native, INFINITE, 0) ? SALTS_OK : SALTS_EBUSY;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_timedwait_ns(
    salts_platform_condition *c, salts_platform_mutex *m, uint64_t timeout_ns) {
    DWORD milliseconds;
    if (!c || !m || timeout_ns > SALTS_PLATFORM_MAX_RELATIVE_WAIT_NS)
        return SALTS_EINVAL;
    milliseconds = (DWORD)((timeout_ns + 999999ull) / 1000000ull);
    if (SleepConditionVariableSRW(&c->native, &m->native, milliseconds, 0))
        return SALTS_OK;
    return GetLastError() == ERROR_TIMEOUT ? SALTS_ETIMEDOUT : SALTS_EBUSY;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_signal(salts_platform_condition *c) {
    if (!c) return SALTS_EINVAL;
    WakeConditionVariable(&c->native); return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_broadcast(salts_platform_condition *c) {
    if (!c) return SALTS_EINVAL;
    WakeAllConditionVariable(&c->native); return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_destroy(salts_platform_condition **owner) {
    salts_platform_condition *c;
    if (!owner || !(c = *owner)) return SALTS_EINVAL;
    /* Condition variable must be quiescent: OS has no destroy primitive. */
    *owner = NULL;
    free(c); return SALTS_OK;
}
#endif
