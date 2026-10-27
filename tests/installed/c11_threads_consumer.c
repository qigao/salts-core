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
    return 0;
}
