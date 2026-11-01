/* Real production clock owner: concurrent first-use frequency publication.
 * All clients use the one Platform Atomic ABI, never a volatile race.
 */
#include <clock.h>
#include <platform_thread.h>
#include <stdint.h>
#include <stdio.h>

enum { WORKERS=8, SAMPLES=9000 };
static int worker(void *opaque) {
    uint64_t before=0, now=0;
    int i;
    (void)opaque;
    for (i=0;i<SAMPLES;i++) {
        now=cmeta_hrtime();
        if (now==0 || (before && now<before)) return -1;
        before=now;
    }
    return 29;
}
int main(void) {
    salts_platform_thread *threads[WORKERS] = {0};
    int i, result;
    for(i=0;i<WORKERS;i++)
        if(salts_platform_thread_create(&threads[i], worker, NULL)) return 1;
    for(i=0;i<WORKERS;i++) {
        if(salts_platform_thread_join(&threads[i], &result) || result!=29)
            { fprintf(stderr,"clock worker %d result=%d\n",i,result); return 2; }
    }
    puts("production Platform monotonic clock and atomic frequency initialization PASS");
    return 0;
}
