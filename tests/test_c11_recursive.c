/* C11 recursive mutex and multiple concurrent condition waiters.
 * All locks are owned by the ONE Salts::Platform provider, not pthread
 * objects or Windows CRITICAL_SECTION exported to the caller.
 */
#include <threads.h>
#include <salts_core/platform_thread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#if defined(_WIN32)
#define RECUR_TRACE(msg) do { fprintf(stderr, "rec-stage:%s\n", msg); fflush(stderr); } while (0)
#else
#define RECUR_TRACE(msg) ((void)0)
#endif

enum { WAITERS=4 };
static mtx_t mutex;
static cnd_t condition;
static cnd_t admissions_condition;
static int go;
static salts_atomic32 ready={0};

static int SALTS_CORE_PLATFORM_CALL waiter(void *arg) {
    uintptr_t id=(uintptr_t)arg;
    uint32_t before=0;
    struct timespec expired={0,0};
    RECUR_TRACE("worker-lock-before");
    if (mtx_lock(&mutex) != thrd_success) return -1;
    RECUR_TRACE("worker-lock-after");
    RECUR_TRACE("worker-zero-timeout-before");
    {
        const int wait_result = cnd_timedwait(&condition, &mutex, &expired);
        if (wait_result != thrd_timedout) {
            fprintf(stderr,"recursive worker %u zero-timeout unexpected rc=%d\n",
                    (unsigned)id, wait_result);
            (void)mtx_unlock(&mutex);
            return -2;
        }
    }
    RECUR_TRACE("worker-zero-timeout-after");
    if (salts_atomic32_fetch_add(&ready,1u,SALTS_MEMORY_RELEASE,&before)) {
        (void)mtx_unlock(&mutex);
        return -3;
    }
    if (cnd_signal(&admissions_condition) != thrd_success) {
        (void)mtx_unlock(&mutex);
        return -7;
    }
    RECUR_TRACE("worker-ready-signal");
    /* All WAITERS are on the same recursive mutex but individually enter
     * cnd_wait at recursion depth one. The owning OS lock is released
     * while each waits; native recursive ownership must be restored.
     */
    while (!go) {
        RECUR_TRACE("worker-cond-wait-before");
        if (cnd_wait(&condition, &mutex) != thrd_success) {
            fprintf(stderr,"recursive worker %u condition wait failure\n",
                    (unsigned)id);
            (void)mtx_unlock(&mutex);
            return -4;
        }
        RECUR_TRACE("worker-cond-wait-woke");
    }
    RECUR_TRACE("worker-go");
    if (mtx_trylock(&mutex) != thrd_success) {
        (void)mtx_unlock(&mutex);
        return -5;
    } /* depth two */
    if (mtx_unlock(&mutex) != thrd_success ||
        mtx_unlock(&mutex) != thrd_success) return -6;
    return (int)id+40;
}
int main(void) {
    salts_platform_mutex *native=NULL;
    thrd_t thread[WAITERS]={0};
    uint32_t admitted=0;
    int i,ret;
    struct timespec later;
    later.tv_sec=time(NULL)+10;
    later.tv_nsec=0;
    RECUR_TRACE("main-init");
    if (mtx_init(&mutex, mtx_recursive | mtx_timed) != thrd_success ||
        cnd_init(&condition) != thrd_success ||
        cnd_init(&admissions_condition) != thrd_success) return 1;
    if (mtx_lock(&mutex) || mtx_trylock(&mutex) ||
        mtx_timedlock(&mutex,&later)) return 2;
    if (salts_platform_mutex_destroy(&mutex.owner) != SALTS_EBUSY)
        return 3;
    RECUR_TRACE("main-recursive-depth3-acquired");
    if (mtx_unlock(&mutex) || mtx_unlock(&mutex) || mtx_unlock(&mutex))
        return 4;
    RECUR_TRACE("main-recursive-depth3-released");
    if (salts_platform_mutex_create_recursive(&native) || !native)
        return 5;
    if (salts_platform_mutex_lock(native) ||
        salts_platform_mutex_lock(native) ||
        salts_platform_mutex_destroy(&native) != SALTS_EBUSY)
        return 6;
    if (salts_platform_mutex_unlock(native) ||
        salts_platform_mutex_destroy(&native) != SALTS_EBUSY)
        return 7;
    if (salts_platform_mutex_unlock(native) ||
        salts_platform_mutex_destroy(&native) || native)
        return 8;
    RECUR_TRACE("main-native-recursive-destroyed");
    for (i=0;i<WAITERS;i++)
        if (thrd_create(&thread[i],waiter,(void *)(uintptr_t)i)) return 9;
    RECUR_TRACE("main-workers-created");
    /* Admission is an explicit condition, not a scheduler-dependent
     * fixed-count yield loop. Both predicates and signaling are guarded
     * by the same native recursive mutex, at recursion depth one.
     */
    if (mtx_lock(&mutex)) return 12;
    later.tv_sec=time(NULL)+10;
    later.tv_nsec=0;
    for (;;) {
        int rc;
        if (salts_atomic32_load(&ready,SALTS_MEMORY_ACQUIRE,&admitted))
            return 10;
        if (admitted==WAITERS) { RECUR_TRACE("main-admitted-all"); break; }
        rc=cnd_timedwait(&admissions_condition,&mutex,&later);
        if (rc!=thrd_success) {
            fprintf(stderr,"recursive admission wait failed: rc=%d admitted=%u\n",
                    rc,admitted);
            return 11;
        }
    }
    go=1;
    RECUR_TRACE("main-go-broadcast-before");
    if (cnd_broadcast(&condition) || mtx_unlock(&mutex)) return 13;
    RECUR_TRACE("main-go-broadcast-after");
    for (i=0;i<WAITERS;i++)
        if (thrd_join(thread[i],&ret) || ret!=40+i) {
            fprintf(stderr,"recursive worker %d result %d\n",i,ret);
            return 14;
        }
    cnd_destroy(&condition);
    cnd_destroy(&admissions_condition);
    mtx_destroy(&mutex);
    if (mutex.owner || condition.owner || admissions_condition.owner) return 15;
    puts("C11 recursive mutex: recursion, timed, busy destroy, 4 condition waiters PASS");
    return 0;
}
