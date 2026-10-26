/* SPDX-License-Identifier: Apache-2.0
 * Real NativeIO wake-before-observe/cancel drain, no fake completions.
 * The producer ONLY calls native_io_backend_wake across threads.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <salts/native_io.h>
#include <salts/error_codes.h>
#include <stdatomic.h>
#if !defined(_WIN32)
#include <pthread.h>
#endif
#include "native_tcp_compat.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum { CYCLES = 64, MAX_STEPS = 3000, MAX_SPINS = 1000000 };
typedef struct producer {
    native_io_backend *backend;
    atomic_bool armed, requested;
    int result;
} producer;

static native_io_backend_kind backend_kind(void)
{
#if defined(_WIN32)
    return NATIVE_IO_BACKEND_IOCP;
#elif defined(__linux__)
    return NATIVE_IO_BACKEND_EPOLL;
#elif defined(__APPLE__)
    return NATIVE_IO_BACKEND_KQUEUE;
#else
#error NativeIO completion/wake test requires IOCP/epoll/kqueue
#endif
}

#if defined(_WIN32)
#define WAKE_THREAD_RETURN DWORD WINAPI
#define WAKE_THREAD_DONE 0u
#else
#define WAKE_THREAD_RETURN void *
#define WAKE_THREAD_DONE NULL
#endif
static WAKE_THREAD_RETURN wake_thread(void *arg)
{
    producer *p = (producer *)arg;
    int i;
    for (i = 0; i < MAX_SPINS; ++i) {
        if (atomic_load_explicit(&p->armed, memory_order_acquire)) break;
        native_tcp_yield();
    }
    if (i == MAX_SPINS) {
        p->result = SALTS_ETIMEDOUT;
        return WAKE_THREAD_DONE;
    }
    atomic_store_explicit(&p->requested, true, memory_order_release);
    p->result = native_io_backend_wake(p->backend);
    return WAKE_THREAD_DONE;
}

static int run(unsigned cycle)
{
    native_io_backend backend = {0};
    native_io_endpoint endpoint = {0};
    native_io_request request = {0};
    native_io_operation operation = {0};
    native_io_completion completion[2];
    const native_io_backend_config config = {backend_kind(),1u,2u,2u};
    unsigned char recv_backing[127];
    producer p;
#if defined(_WIN32)
    HANDLE worker = NULL;
#else
    pthread_t worker;
#endif
    native_tcp_socket client = NATIVE_TCP_INVALID_SOCKET;
    native_tcp_socket server = NATIVE_TCP_INVALID_SOCKET;
    int rc = 1;
    bool started = false, attached = false, request_live = false;
    bool thread_started = false, thread_joined = false, closed = false;
    int status;
    size_t count = 0u, tries;
    memset(recv_backing, 0xa5, sizeof(recv_backing));

    if (native_tcp_connect_pair(&client, &server) != 0
        || native_io_backend_init(&backend, &config) != SALTS_OK) goto done;
    started = true;
    if (native_io_backend_attach_socket(
            &backend, (uintptr_t)server, &endpoint) != SALTS_OK) goto done;
    attached = true;
    operation.kind = NATIVE_IO_OPERATION_TCP_RECV;
    operation.endpoint = endpoint;
    operation.buffer = recv_backing;
    operation.length = sizeof(recv_backing);
    operation.user_data = 0xc7u;
    if (native_io_backend_submit(&backend, &operation, &request)
        != SALTS_OK) goto done;
    request_live = true;

    p.backend = &backend;
    p.result = SALTS_ETIMEDOUT;
    atomic_init(&p.armed, false);
    atomic_init(&p.requested, false);
#if defined(_WIN32)
    worker = CreateThread(NULL, 0, wake_thread, &p, 0, NULL);
    if (worker == NULL) goto done;
#else
    if (pthread_create(&worker, NULL, wake_thread, &p) != 0) goto done;
#endif
    thread_started = true;
    atomic_store_explicit(&p.armed, true, memory_order_release);
    status = native_io_backend_observe(&backend, completion, 2u, 2500u, &count);
#if defined(_WIN32)
    if (WaitForSingleObject(worker, INFINITE) != WAIT_OBJECT_0) goto done;
    (void)CloseHandle(worker);
    worker = NULL;
#else
    if (pthread_join(worker, NULL) != 0) goto done;
#endif
    thread_joined = true;
    if (status != SALTS_OK || count != 0u || p.result != SALTS_OK
        || !atomic_load_explicit(&p.requested, memory_order_acquire))
        goto done;

    if (native_io_backend_close(&backend) != SALTS_OK) goto done;
    closed = true;
    if (native_io_backend_destroy(&backend) != SALTS_EBUSY) goto done;
    status = native_io_backend_cancel(&backend, request);
    if (status != SALTS_OK && status != SALTS_EALREADY) goto done;

    for (tries = 0u; tries < MAX_STEPS; ++tries) {
        count = 0u;
        status = native_io_backend_observe(
            &backend, completion, 2u, 1u, &count);
        if (status != SALTS_OK && status != SALTS_ETIMEDOUT) goto done;
        if (count == 0u) continue;
        if (count != 1u
            || completion[0].request.slot != request.slot
            || completion[0].request.generation != request.generation
            || completion[0].user_data != 0xc7u
            || completion[0].kind != NATIVE_IO_COMPLETION_CANCELLED
            || completion[0].bytes != 0u) goto done;
        request_live = false;
        break;
    }
    if (request_live) goto done;
    for (tries = 0u; tries < sizeof(recv_backing); ++tries)
        if (recv_backing[tries] != 0xa5u) goto done;
    rc = 0;

done:
    if (thread_started && !thread_joined) {
        atomic_store_explicit(&p.armed, true, memory_order_release);
#if defined(_WIN32)
        (void)WaitForSingleObject(worker, INFINITE);
        (void)CloseHandle(worker);
#else
        (void)pthread_join(worker, NULL);
#endif
    }
    if (started) {
        if (!closed) (void)native_io_backend_close(&backend);
        if (request_live) {
            (void)native_io_backend_cancel(&backend, request);
            for (tries = 0u; tries < MAX_STEPS && request_live; ++tries) {
                count = 0u;
                status = native_io_backend_observe(
                    &backend, completion, 2u, 1u, &count);
                if (status != SALTS_OK && status != SALTS_ETIMEDOUT) break;
                for (size_t i = 0u; i < count; ++i)
                    if (completion[i].request.slot == request.slot
                        && completion[i].request.generation == request.generation)
                        request_live = false;
            }
            if (request_live) {
                fprintf(stderr,"NativeIO borrow %u not terminal: refusing unsafe free\n",cycle);
                return 4;
            }
        }
    }
    native_tcp_close(client);
    native_tcp_close(server);
    if (attached &&
        native_io_backend_release_socket(&backend, endpoint) != SALTS_OK)
        rc = 1;
    if (started && native_io_backend_destroy(&backend) != SALTS_OK) rc = 1;
    if (rc) fprintf(stderr,"NativeIO wake/close/cancel race %u FAILED\n",cycle);
    return rc;
}

int main(void)
{
    int rc = 0;
    if (native_tcp_startup() != 0) return 2;
    for (unsigned i = 0u; i < CYCLES; ++i) {
        if (run(i) != 0) { rc = 1; break; }
    }
    native_tcp_shutdown();
    if (!rc)
        puts("NativeIO real IOCP/epoll/kqueue wake-before-observe and cancellation: 64 cycles PASS");
    return rc;
}
