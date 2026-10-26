#include <salts_core/platform_thread.h>
#ifndef _WIN32
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>

/* The worker and caller each hold one reference. Detach drops only the
 * caller's reference; the live worker must never access freed storage.
 */
struct salts_platform_thread {
    pthread_t native;
    salts_platform_thread_fn callback;
    void *argument;
    int result;
    salts_atomic32 refs;
};
struct salts_platform_mutex { pthread_mutex_t native; };
struct salts_platform_condition { pthread_cond_t native; };

static void release_thread(salts_platform_thread *thread) {
    uint32_t before = 0;
    (void)salts_atomic32_fetch_add(&thread->refs, UINT32_MAX,
                                    SALTS_MEMORY_SEQ_CST, &before);
    if (before == 1u) free(thread);
}
static void *thread_entry(void *context) {
    salts_platform_thread *thread = (salts_platform_thread *)context;
    thread->result = thread->callback(thread->argument);
    release_thread(thread);
    return NULL;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_create(salts_platform_thread **out,
                              salts_platform_thread_fn fn, void *arg) {
    salts_platform_thread *thread;
    if (!out || !fn || *out) return SALTS_EINVAL;
    thread = (salts_platform_thread *)calloc(1u, sizeof(*thread));
    if (!thread) return SALTS_ENOMEM;
    thread->callback = fn;
    thread->argument = arg;
    (void)salts_atomic32_init(&thread->refs, 2u);
    if (pthread_create(&thread->native, NULL, thread_entry, thread) != 0) {
        free(thread);
        return SALTS_EBUSY;
    }
    *out = thread;
    return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_join(salts_platform_thread **owner, int *result) {
    salts_platform_thread *thread;
    if (!owner || !(thread = *owner)) return SALTS_EINVAL;
    if (pthread_equal(pthread_self(), thread->native)) return SALTS_EINVAL;
    if (pthread_join(thread->native, NULL) != 0) return SALTS_EBUSY;
    if (result) *result = thread->result;
    *owner = NULL;
    release_thread(thread);
    return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_thread_detach(salts_platform_thread **owner) {
    salts_platform_thread *thread;
    if (!owner || !(thread = *owner)) return SALTS_EINVAL;
    if (pthread_detach(thread->native) != 0) return SALTS_EBUSY;
    *owner = NULL;
    release_thread(thread);
    return SALTS_OK;
}
void SALTS_CORE_PLATFORM_CALL salts_platform_thread_yield(void) { (void)sched_yield(); }

int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_create(salts_platform_mutex **out) {
    salts_platform_mutex *m;
    if (!out || *out) return SALTS_EINVAL;
    m = (salts_platform_mutex *)malloc(sizeof(*m));
    if (!m) return SALTS_ENOMEM;
    if (pthread_mutex_init(&m->native, NULL) != 0) { free(m); return SALTS_EBUSY; }
    *out = m; return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_lock(salts_platform_mutex *m) {
    return m && pthread_mutex_lock(&m->native) == 0 ? SALTS_OK : SALTS_EINVAL;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_unlock(salts_platform_mutex *m) {
    return m && pthread_mutex_unlock(&m->native) == 0 ? SALTS_OK : SALTS_EINVAL;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_mutex_destroy(salts_platform_mutex **owner) {
    salts_platform_mutex *m;
    if (!owner || !(m = *owner)) return SALTS_EINVAL;
    if (pthread_mutex_destroy(&m->native) != 0) return SALTS_EBUSY;
    *owner = NULL;
    free(m); return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_create(salts_platform_condition **out) {
    salts_platform_condition *c;
    if (!out || *out) return SALTS_EINVAL;
    c = (salts_platform_condition *)malloc(sizeof(*c));
    if (!c) return SALTS_ENOMEM;
    if (pthread_cond_init(&c->native, NULL) != 0) { free(c); return SALTS_EBUSY; }
    *out = c; return SALTS_OK;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_wait(salts_platform_condition *c, salts_platform_mutex *m) {
    if (!c || !m) return SALTS_EINVAL;
    return pthread_cond_wait(&c->native, &m->native) == 0 ? SALTS_OK : SALTS_EINVAL;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_signal(salts_platform_condition *c) {
    return c && pthread_cond_signal(&c->native) == 0 ? SALTS_OK : SALTS_EINVAL;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_broadcast(salts_platform_condition *c) {
    return c && pthread_cond_broadcast(&c->native) == 0 ? SALTS_OK : SALTS_EINVAL;
}
int SALTS_CORE_PLATFORM_CALL salts_platform_condition_destroy(salts_platform_condition **owner) {
    salts_platform_condition *c;
    if (!owner || !(c = *owner)) return SALTS_EINVAL;
    if (pthread_cond_destroy(&c->native) != 0) return SALTS_EBUSY;
    *owner = NULL;
    free(c); return SALTS_OK;
}
#endif
