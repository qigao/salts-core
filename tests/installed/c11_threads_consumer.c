#include <threads.h>
#include <stdint.h>
#include <time.h>

static int SALTS_CORE_PLATFORM_CALL run(void *arg) {
    int *value = (int *)arg;
    if (!thrd_current() || !thrd_equal(thrd_current(),thrd_current())) return -1;
    return *value + 1;
}
int main(void) {
    thrd_t thread=NULL;
    mtx_t mutex;
    cnd_t cond;
    struct timespec expired={0,0}, step={0,1000000L};
    int v=41, output=0;
    if (mtx_init(&mutex,mtx_timed) || cnd_init(&cond)) return 1;
    if (mtx_lock(&mutex)) return 2;
    if (mtx_trylock(&mutex)!=thrd_busy) return 3;
    if (mtx_timedlock(&mutex,&expired)!=thrd_timedout) return 4;
    if (cnd_timedwait(&cond,&mutex,&expired)!=thrd_timedout) return 5;
    if (mtx_unlock(&mutex)) return 6;
    if (thrd_create(&thread,run,&v) || thrd_join(thread,&output) || output!=42) return 7;
    if (thrd_sleep(&step,NULL)) return 8;
    cnd_destroy(&cond);
    mtx_destroy(&mutex);
    {
        mtx_t recursive;
        struct timespec future;
        future.tv_sec=time(NULL)+2;
        future.tv_nsec=0;
        if (mtx_init(&recursive,mtx_recursive|mtx_timed) != thrd_success ||
            mtx_lock(&recursive) || mtx_trylock(&recursive) ||
            mtx_timedlock(&recursive,&future)) return 9;
        if (salts_platform_mutex_destroy(&recursive.owner)!=SALTS_EBUSY)
            return 10;
        if (mtx_unlock(&recursive) || mtx_unlock(&recursive) ||
            mtx_unlock(&recursive)) return 11;
        mtx_destroy(&recursive);
        if (recursive.owner) return 12;
    }
    return 0;
}
