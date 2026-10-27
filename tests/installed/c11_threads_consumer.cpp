#include <cstdint>
#include <ctime>
#include <threads.h>
static int SALTS_CORE_PLATFORM_CALL run(void *arg) {
    int *v=static_cast<int *>(arg);
    return *v+7;
}
int main() {
    thrd_t thread=nullptr;
    mtx_t m;
    struct timespec expired={0,0};
    int n=35, result=0;
    if (mtx_init(&m,mtx_timed) || mtx_lock(&m)) return 1;
    if (mtx_timedlock(&m,&expired)!=thrd_timedout || mtx_unlock(&m)) return 2;
    if (thrd_create(&thread,run,&n) || thrd_join(thread,&result) || result!=42) return 3;
    mtx_destroy(&m);
    return 0;
}
