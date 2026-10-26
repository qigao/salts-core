/* SPDX-License-Identifier: Apache-2.0
 * REAL Windows IOCP/OVERLAPPED TCP retained receive cancellation and wake.
 * The worker may call ONLY native_io_backend_wake(); all submit/cancel,
 * observe/close and terminal buffer retirement belong to the IOCP owner.
 * No readiness, synchronous socket or fake completion event substitutes.
 */
#ifndef _WIN32
#error Real Windows IOCP required
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <salts/native_io.h>
#include <salts/error_codes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { REPEATS = 64, MAX_OBSERVES = 3000, MAX_SPINS = 1000000 };

typedef struct wake_command {
    native_io_backend *backend;
    atomic_bool start;
    atomic_bool published;
    int result;
} wake_command;

static DWORD WINAPI producer_wake(LPVOID value)
{
    wake_command *w = (wake_command *)value;
    unsigned i;
    for (i = 0u; i < MAX_SPINS; ++i) {
        if (atomic_load_explicit(&w->start, memory_order_acquire))
            break;
        Sleep(0);
    }
    if (i == MAX_SPINS) {
        w->result = SALTS_ETIMEDOUT;
        return 1u;
    }
    atomic_store_explicit(&w->published, true, memory_order_release);
    w->result = native_io_backend_wake(w->backend);
    return 0u;
}

static SOCKET overlapped_socket(void)
{
    return WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                      NULL, 0u, WSA_FLAG_OVERLAPPED);
}

static int make_pair(SOCKET *out_client, SOCKET *out_server)
{
    struct sockaddr_in addr;
    int addrlen = (int)sizeof(addr);
    SOCKET listener = INVALID_SOCKET;
    SOCKET client = INVALID_SOCKET;
    SOCKET server = INVALID_SOCKET;
    *out_client = *out_server = INVALID_SOCKET;
    listener = overlapped_socket();
    if (listener == INVALID_SOCKET) goto fail;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&addr, (int)sizeof(addr)) != 0
        || getsockname(listener, (struct sockaddr *)&addr, &addrlen) != 0
        || listen(listener, 1) != 0)
        goto fail;
    client = overlapped_socket();
    if (client == INVALID_SOCKET ||
        connect(client, (struct sockaddr *)&addr, (int)sizeof(addr)) != 0)
        goto fail;
    server = accept(listener, NULL, NULL);
    if (server == INVALID_SOCKET) goto fail;
    (void)closesocket(listener);
    *out_client = client;
    *out_server = server;
    return 0;
fail:
    if (server != INVALID_SOCKET) (void)closesocket(server);
    if (client != INVALID_SOCKET) (void)closesocket(client);
    if (listener != INVALID_SOCKET) (void)closesocket(listener);
    return -1;
}

static int one_cycle(unsigned cycle)
{
    native_io_backend backend = {0};
    native_io_endpoint endpoint = {0};
    native_io_request request = {0};
    native_io_operation operation = {0};
    native_io_completion completed[2];
    native_io_backend_config config = {
        NATIVE_IO_BACKEND_IOCP, 1u, 2u, 2u
    };
    wake_command command;
    HANDLE worker = NULL;
    SOCKET client = INVALID_SOCKET, server = INVALID_SOCKET;
    unsigned char recv_backing[127];
    size_t count = 0u, n, tries;
    bool live = false, started = false, attached = false;
    bool thread_joined = false, closed = false, ready = false;
    int result = 1, status;
    memset(recv_backing, 0xa5, sizeof(recv_backing));
    memset(&command, 0, sizeof(command));
    command.backend = &backend;
    command.result = SALTS_ETIMEDOUT;
    atomic_init(&command.start, false);
    atomic_init(&command.published, false);

    if (!native_io_backend_kind_supported(NATIVE_IO_BACKEND_IOCP) ||
        native_io_backend_kind_model(NATIVE_IO_BACKEND_IOCP)
            != NATIVE_IO_MODEL_COMPLETION)
        goto done;
    if (make_pair(&client, &server) != 0 ||
        native_io_backend_init(&backend, &config) != SALTS_OK)
        goto done;
    started = true;
    if (native_io_backend_attach_socket(
            &backend, (uintptr_t)server, &endpoint) != SALTS_OK)
        goto done;
    attached = true;
    operation.kind = NATIVE_IO_OPERATION_TCP_RECV;
    operation.endpoint = endpoint;
    operation.buffer = recv_backing;
    operation.length = sizeof(recv_backing);
    operation.user_data = 0xc7u;
    if (native_io_backend_submit(&backend, &operation, &request)
        != SALTS_OK)
        goto done;
    live = true;

    worker = CreateThread(NULL, 0u, producer_wake, &command, 0u, NULL);
    if (!worker) goto done;
    atomic_store_explicit(&command.start, true, memory_order_release);
    count = 0u;
    status = native_io_backend_observe(&backend, completed, 2u, 2500u, &count);
    if (WaitForSingleObject(worker, 5000u) != WAIT_OBJECT_0)
        goto done;
    thread_joined = true;
    if (status != SALTS_OK || count != 0u ||
        command.result != SALTS_OK ||
        !atomic_load_explicit(&command.published, memory_order_acquire))
        goto done;
    if (native_io_backend_close(&backend) != SALTS_OK)
        goto done;
    closed = true;
    if (native_io_backend_destroy(&backend) != SALTS_EBUSY)
        goto done;
    status = native_io_backend_cancel(&backend, request);
    if (status != SALTS_OK && status != SALTS_EALREADY)
        goto done;

    for (tries = 0u; tries < MAX_OBSERVES; ++tries) {
        count = 0u;
        status = native_io_backend_observe(&backend, completed,
                                          2u, 1u, &count);
        if (status != SALTS_OK && status != SALTS_ETIMEDOUT)
            goto done;
        if (!count) continue;
        if (count != 1u ||
            completed[0].request.slot != request.slot ||
            completed[0].request.generation != request.generation ||
            completed[0].user_data != 0xc7u ||
            completed[0].kind != NATIVE_IO_COMPLETION_CANCELLED ||
            completed[0].bytes != 0u)
            goto done;
        live = false;
        ready = true;
        break;
    }
    if (!ready) goto done;
    for (n = 0u; n < sizeof(recv_backing); ++n)
        if (recv_backing[n] != 0xa5u) goto done;
    result = 0;

done:
    if (worker) {
        atomic_store_explicit(&command.start, true, memory_order_release);
        if (!thread_joined && WaitForSingleObject(worker, 5000u)
            != WAIT_OBJECT_0) {
            /* Do not destroy NativeIO while wake worker may still enter. */
            fprintf(stderr, "IOCP wake worker did not settle: unsafe free\n");
            return 3;
        }
        (void)CloseHandle(worker);
    }
    if (started) {
        if (!closed) (void)native_io_backend_close(&backend);
        if (live) {
            (void)native_io_backend_cancel(&backend, request);
            for (tries = 0u; tries < MAX_OBSERVES && live; ++tries) {
                count = 0u;
                status = native_io_backend_observe(
                    &backend, completed, 2u, 1u, &count);
                if (status != SALTS_OK && status != SALTS_ETIMEDOUT) break;
                for (n = 0u; n < count; ++n)
                    if (completed[n].request.slot == request.slot &&
                        completed[n].request.generation == request.generation)
                        live = false;
            }
            if (live) {
                fprintf(stderr,
                    "IOCP request %u still borrowed: refusing unsafe free\n",
                    cycle);
                return 4;
            }
        }
    }
    if (client != INVALID_SOCKET) (void)closesocket(client);
    if (server != INVALID_SOCKET) (void)closesocket(server);
    if (attached &&
        native_io_backend_release_socket(&backend, endpoint) != SALTS_OK)
        result = 1;
    if (started && native_io_backend_destroy(&backend) != SALTS_OK)
        result = 1;
    if (result)
        fprintf(stderr, "Real IOCP wake/close/cancel round %u FAILED\n", cycle);
    return result;
}

int main(void)
{
    WSADATA wsa;
    unsigned i;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 2;
    for (i = 0u; i < REPEATS; ++i) {
        if (one_cycle(i) != 0) {
            (void)WSACleanup();
            return 1;
        }
    }
    if (WSACleanup() != 0) return 3;
    puts("Real overlapped IOCP wake/cancel/drain 64 cycles: PASS");
    return 0;
}
