/* Linux native source TSan gate: actual C11 shared Provider cancellation at
 * an in-flight borrowed callback. No GmSSL headers or private library link.
 * Retain host-side pointer lifetimes until the worker has joined.
 */
#include <crypto/tls.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef SDK_CA_FILE
#error SDK_CA_FILE is required
#endif

enum { CYCLES = 64 };

typedef struct gate {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int entered;
    int release;
} gate;

typedef struct worker {
    crypto_tls_session *session;
    crypto_tls_result result;
} worker;

static int64_t blocking_write(void *user,const uint8_t *bytes,size_t len)
{
    gate *g=(gate *)user;
    (void)bytes; (void)len;
    if (pthread_mutex_lock(&g->mutex)) return CRYPTO_TLS_IO_ERROR;
    g->entered=1;
    pthread_cond_broadcast(&g->cond);
    while (!g->release)
        pthread_cond_wait(&g->cond,&g->mutex);
    pthread_mutex_unlock(&g->mutex);
    return CRYPTO_TLS_IO_AGAIN;
}

static int64_t nonblocking_read(void *user,uint8_t *bytes,size_t len)
{
    (void)user;(void)bytes;(void)len;
    return CRYPTO_TLS_IO_AGAIN;
}

static void *drive(void *user)
{
    worker *w=(worker *)user;
    w->result=crypto_tls_session_handshake(w->session);
    return NULL;
}

static int cycle(int iteration)
{
    crypto_tls_provider *provider=NULL;
    crypto_tls_session *session=NULL;
    crypto_tls_session_options options={
        sizeof(options),CRYPTO_TLS_1_3,CRYPTO_TLS_CLIENT,4,
        "localhost",SDK_CA_FILE,NULL,NULL,NULL,0
    };
    gate g;
    crypto_tls_io io={sizeof(io),0,&g,blocking_write,nonblocking_read};
    worker w={NULL,CRYPTO_TLS_INVALID_ARGUMENT};
    pthread_t tid;
    int thread_started=0,ready=0,ok=0;
    uint32_t count=0;
    if (pthread_mutex_init(&g.mutex,NULL)!=0) return 10;
    if (pthread_cond_init(&g.cond,NULL)!=0) {
        pthread_mutex_destroy(&g.mutex);
        return 11;
    }
    g.entered=0;
    g.release=0;

    if (crypto_tls_provider_create(CRYPTO_TLS_ABI_VERSION,&provider)!=CRYPTO_TLS_OK
        || crypto_tls_session_create(provider,&options,&io,&session)!=CRYPTO_TLS_OK)
        goto done;
    w.session=session;
    if (pthread_create(&tid,NULL,drive,&w)!=0) goto done;
    thread_started=1;
    {
        struct timespec deadline;
        int entered;
        if (timespec_get(&deadline,TIME_UTC) != TIME_UTC)
            goto done;
        deadline.tv_sec += 10;
        pthread_mutex_lock(&g.mutex);
        while (!g.entered) {
            if (pthread_cond_timedwait(&g.cond,&g.mutex,&deadline)!=0)
                break;
        }
        entered=g.entered;
        pthread_mutex_unlock(&g.mutex);
        if (!entered) goto done;
    }

    /* The TLS driver is currently inside a real borrowed send callback.
     * Another drive and any free must refuse without consuming the handle. */
    if (crypto_tls_session_handshake(session)!=CRYPTO_TLS_BUSY
        || crypto_tls_session_destroy(&session)!=CRYPTO_TLS_BUSY
        || !session
        || crypto_tls_provider_session_count(provider,&count)!=CRYPTO_TLS_OK
        || count!=1
        || crypto_tls_provider_destroy(&provider)!=CRYPTO_TLS_BUSY
        || !provider)
        goto done;
    if (crypto_tls_session_cancel(session)!=CRYPTO_TLS_OK) goto done;
    ready=1;

done:
    if (thread_started) {
        pthread_mutex_lock(&g.mutex);
        g.release=1;
        pthread_cond_broadcast(&g.cond);
        pthread_mutex_unlock(&g.mutex);
        pthread_join(tid,NULL);
        if (w.result!=CRYPTO_TLS_CANCELLED) ready=0;
    }
    if (session) {
        if (crypto_tls_session_destroy(&session)!=CRYPTO_TLS_OK || session)
            ready=0;
    }
    if (provider) {
        if (crypto_tls_provider_session_count(provider,&count)!=CRYPTO_TLS_OK
            || count!=0 || crypto_tls_provider_destroy(&provider)!=CRYPTO_TLS_OK
            || provider)
            ready=0;
    }
    ok=ready;
    pthread_cond_destroy(&g.cond);
    pthread_mutex_destroy(&g.mutex);
    if (!ok) fprintf(stderr,"Provider cancellation/close race %d failed\n",iteration);
    return ok?0:1;
}

int main(void)
{
    int i;
    for (i=0;i<CYCLES;i++) {
        if (cycle(i)!=0) return 1;
    }
    puts("Crypto true callback-in-flight cancel, BUSY destroy, quiescent release: PASS (64 cycles)");
    return 0;
}
