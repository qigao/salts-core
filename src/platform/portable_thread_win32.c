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
struct salts_platform_mutex {
    int recursive;
    salts_atomic32 active_depth; /* includes threads released into condition wait */
    union {
        SRWLOCK srw;
        CRITICAL_SECTION critical;
    } native;
};
struct salts_platform_condition { CONDITION_VARIABLE native; };
#if defined(_MSC_VER)
static __declspec(thread) salts_platform_thread *salts_platform_current;
#elif defined(__clang__) || defined(__GNUC__)
static _Thread_local salts_platform_thread *salts_platform_current;
#else
#error "Windows Platform Thread TLS requires supported compiler TLS, not a global fallback"
#endif
uint64_t SALTS_CORE_PLATFORM_CALL salts_platform_thread_current_id(void) {
    return (uint64_t)GetCurrentThreadId();
}
uint64_t SALTS_CORE_PLATFORM_CALL salts_platform_thread_id(const salts_platform_thread *thread) {
    return thread ? (uint64_t)thread->id : 0u;
}

static void release_thread(salts_platform_thread *thread) {
    uint32_t before = 0;
    (void)salts_atomic32_fetch_add(&thread->refs, UINT32_MAX,
                                    SALTS_MEMORY_SEQ_CST, &before);
    if (before == 1u) free(thread);
}
static unsigned __stdcall thread_entry(void *opaque) {
    salts_platform_thread *thread = (salts_platform_thread *)opaque;
    salts_platform_current = thread;
    thread->result = thread->callback(thread->argument); /* user: cdecl, OS: stdcall */
    salts_platform_current = NULL;
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
void SALTS_CORE_PLATFORM_CALL salts_platform_thread_exit(int result) {
    salts_platform_thread *thread = salts_platform_current;
    if (thread) {
        thread->result = result;
        salts_platform_current = NULL;
        release_thread(thread);
    }
    _endthreadex(0);
    for (;;) ExitThread(0); /* The CRT entrypoint must not return. */
}
void SALTS_CORE_PLATFORM_CALL salts_platform_thread_yield(void) { (void)SwitchToThread(); }
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_sleep_ns(uint64_t duration_ns) {
    if (duration_ns > SALTS_PLATFORM_MAX_RELATIVE_WAIT_NS) return SALTS_EINVAL;
    Sleep((DWORD)((duration_ns + 999999ull) / 1000000ull));
    return SALTS_OK;
}
static int mutex_create_internal(salts_platform_mutex **out, int recursive) {
    salts_platform_mutex *m;
    if (!out || *out) return SALTS_EINVAL;
    m = (salts_platform_mutex *)calloc(1u, sizeof(*m));
    if (!m) return SALTS_ENOMEM;
    m->recursive = recursive;
    if (salts_atomic32_init(&m->active_depth, 0u) != SALTS_OK) {
        free(m); return SALTS_EINVAL;
    }
    if (recursive) InitializeCriticalSection(&m->native.critical);
    else InitializeSRWLock(&m->native.srw);
    *out = m;
    return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_create(salts_platform_mutex **out) {
    return mutex_create_internal(out, 0);
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_create_recursive(salts_platform_mutex **out) {
    return mutex_create_internal(out, 1);
}
static int count_recursive_lock(salts_platform_mutex *m) {
    uint32_t before = 0;
    if (!m->recursive) return SALTS_OK;
    return salts_atomic32_fetch_add(&m->active_depth, 1u,
                 SALTS_MEMORY_ACQ_REL, &before);
}
static int try_acquire_native(salts_platform_mutex *m) {
    if (m->recursive) return TryEnterCriticalSection(&m->native.critical) ? 1 : 0;
    return TryAcquireSRWLockExclusive(&m->native.srw) ? 1 : 0;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_lock(salts_platform_mutex *m) {
    if (!m) return SALTS_EINVAL;
    if (m->recursive) EnterCriticalSection(&m->native.critical);
    else AcquireSRWLockExclusive(&m->native.srw);
    return count_recursive_lock(m);
}
/* GetTickCount64 is monotonic, independent of realtime changes.
 * Both recursive CRITICAL_SECTION and plain SRWLOCK use bounded OS
 * sleep/yield acquisition; no volatile spinning or compiler-native locks.
 */
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_timedlock_ns(
    salts_platform_mutex *m, uint64_t timeout_ns) {
    ULONGLONG start, elapsed, requested_ms;
    if (!m || timeout_ns > SALTS_PLATFORM_MAX_RELATIVE_WAIT_NS)
        return SALTS_EINVAL;
    if (try_acquire_native(m)) return count_recursive_lock(m);
    if (timeout_ns == 0) return SALTS_ETIMEDOUT;
    requested_ms = (ULONGLONG)((timeout_ns + 999999ull) / 1000000ull);
    start = GetTickCount64();
    for (;;) {
        elapsed = GetTickCount64() - start;
        if (elapsed >= requested_ms) return SALTS_ETIMEDOUT;
        if (requested_ms - elapsed > 2ull) Sleep(1);
        else (void)SwitchToThread();
        if (GetTickCount64() - start >= requested_ms) return SALTS_ETIMEDOUT;
        if (try_acquire_native(m)) return count_recursive_lock(m);
    }
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_unlock(salts_platform_mutex *m) {
    uint32_t old = 0;
    if (!m) return SALTS_EINVAL;
    if (m->recursive) {
        /* The caller must own one recursive level (C11 precondition).
         * Condition wait is permitted only at depth one.
         */
        LeaveCriticalSection(&m->native.critical);
        return salts_atomic32_fetch_add(&m->active_depth, UINT32_MAX,
                                        SALTS_MEMORY_ACQ_REL, &old);
    }
    ReleaseSRWLockExclusive(&m->native.srw);
    return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_destroy(salts_platform_mutex **owner) {
    salts_platform_mutex *m;
    uint32_t depth = 0;
    if (!owner || !(m = *owner)) return SALTS_EINVAL;
    /* Caller must quiesce new entrants. Critically: recursive TryEnter
     * succeeds for its own owner, so reject live depths before probing.
     */
    if (m->recursive) {
        if (salts_atomic32_load(&m->active_depth, SALTS_MEMORY_ACQUIRE, &depth)
            != SALTS_OK) return SALTS_EINVAL;
        if (depth) return SALTS_EBUSY;
    }
    if (!try_acquire_native(m)) return SALTS_EBUSY;
    if (m->recursive) {
        LeaveCriticalSection(&m->native.critical);
        DeleteCriticalSection(&m->native.critical);
    } else {
        ReleaseSRWLockExclusive(&m->native.srw);
    }
    *owner = NULL;
    free(m);
    return SALTS_OK;
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
    if (m->recursive)
        return SleepConditionVariableCS(&c->native, &m->native.critical,
                                         INFINITE) ? SALTS_OK : SALTS_EBUSY;
    return SleepConditionVariableSRW(&c->native, &m->native.srw,
                                      INFINITE, 0) ? SALTS_OK : SALTS_EBUSY;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_timedwait_ns(
    salts_platform_condition *c, salts_platform_mutex *m, uint64_t timeout_ns) {
    DWORD milliseconds;
    if (!c || !m || timeout_ns > SALTS_PLATFORM_MAX_RELATIVE_WAIT_NS)
        return SALTS_EINVAL;
    milliseconds = (DWORD)((timeout_ns + 999999ull) / 1000000ull);
    if ((m->recursive ?
          SleepConditionVariableCS(&c->native, &m->native.critical, milliseconds) :
          SleepConditionVariableSRW(&c->native, &m->native.srw, milliseconds, 0)))
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
