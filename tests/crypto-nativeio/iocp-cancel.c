/* SPDX-License-Identifier: Apache-2.0
 * Windows-only real TCP IOCP/OVERLAPPED cancellation test. No simulated
 * completion, no polling backend, no GmSSL or Crypto private symbols.
 * Borrowed receive backing MUST remain alive until observed CANCELLED.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <salts/native_io.h>
#include <salts/error_codes.h>
#include <cmeta_buffer.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { MAX_OBSERVES = 3000, REAL_CANCEL_CYCLES = 32, RECV_BYTES = 512 };

static int run_cycle(int index)
{
    native_io_backend backend = {0};
    const native_io_backend_config cfg = {
        NATIVE_IO_BACKEND_IOCP, 1u, 2u, 2u
    };
    native_io_endpoint endpoint = {0};
    native_io_request request = {0};
    native_io_operation op = {0};
    native_io_backend_stats stats = {0};
    mem_buffer_t *buffer = NULL;
    SOCKET listener = INVALID_SOCKET;
    SOCKET client = INVALID_SOCKET;
    SOCKET accepted = INVALID_SOCKET;
    struct sockaddr_in peer = {0};
    int address_bytes = (int)sizeof(peer);
    int status = SALTS_EIO, result = 1;
    bool initialized = false, submitted = false, terminal = false, closed = false;
    unsigned i;

    if (native_io_backend_kind_model(NATIVE_IO_BACKEND_IOCP)
            != NATIVE_IO_MODEL_COMPLETION
        || !native_io_backend_kind_supported(NATIVE_IO_BACKEND_IOCP))
        goto cleanup;
    listener = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
                          WSA_FLAG_OVERLAPPED);
    if (listener == INVALID_SOCKET) goto cleanup;
    peer.sin_family = AF_INET;
    peer.sin_port = 0;
    if (inet_pton(AF_INET, "127.0.0.1", &peer.sin_addr) != 1
        || bind(listener, (struct sockaddr *)&peer, sizeof(peer)) != 0
        || getsockname(listener, (struct sockaddr *)&peer, &address_bytes) != 0
        || listen(listener, 1) != 0) goto cleanup;
    client = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
                        WSA_FLAG_OVERLAPPED);
    if (client == INVALID_SOCKET
        || connect(client, (struct sockaddr *)&peer, sizeof(peer)) != 0)
        goto cleanup;
    accepted = accept(listener, NULL, NULL);
    if (accepted == INVALID_SOCKET) goto cleanup;
    if (native_io_backend_init(&backend, &cfg) != SALTS_OK)
        goto cleanup;
    initialized = true;
    if (native_io_backend_attach_socket(&backend, (uintptr_t)accepted,
            &endpoint) != SALTS_OK)
        goto cleanup;

    buffer = mem_get_buffer(mem_global(), RECV_BYTES);
    if (!buffer || mem_buffer_capacity(buffer) < RECV_BYTES)
        goto cleanup;
    memset(mem_buffer_data(buffer), 0xa5, RECV_BYTES);
    op.kind = NATIVE_IO_OPERATION_TCP_RECV;
    op.endpoint = endpoint;
    op.buffer = mem_buffer_data(buffer);
    op.length = RECV_BYTES;
    op.user_data = UINTPTR_C(0xC011CE);
    if (native_io_backend_submit(&backend, &op, &request) != SALTS_OK
        || !native_io_request_valid(request))
        goto cleanup;
    submitted = true;
    /* A successful submit is NOT a terminal completion. Early release
     * must be refused by the actual IOCP backend. */
    if (native_io_backend_release_socket(&backend, endpoint) != SALTS_EBUSY
        || native_io_backend_destroy(&backend) != SALTS_EBUSY)
        goto cleanup;
    if (native_io_backend_close(&backend) != SALTS_OK) goto cleanup;
    closed = true;
    status = native_io_backend_cancel(&backend, request);
    if (status != SALTS_OK) goto cleanup;

    for (i = 0; i < MAX_OBSERVES && !terminal; ++i) {
        native_io_completion c[2] = {{0}};
        size_t count = 0u, j;
        status = native_io_backend_observe(&backend, c, 2u, 2u, &count);
        if (status != SALTS_OK && status != SALTS_ETIMEDOUT)
            goto cleanup;
        for (j = 0u; j < count; ++j) {
            if (c[j].request.slot != request.slot
                || c[j].request.generation != request.generation
                || c[j].endpoint.slot != endpoint.slot
                || c[j].endpoint.generation != endpoint.generation
                || c[j].user_data != (uintptr_t)0xC011CEu
                || c[j].kind != NATIVE_IO_COMPLETION_CANCELLED
                || c[j].status != SALTS_ECANCELED
                || c[j].bytes != 0u
                || c[j].native_status != ERROR_OPERATION_ABORTED)
                goto cleanup;
            terminal = true;
        }
    }
    if (!terminal) goto cleanup;
    if (!native_io_backend_get_stats(&backend, &stats)
        || stats.active_requests != 0u || stats.cancelled == 0u
        || stats.native_cancel_errors != 0u || stats.completed == 0u)
        goto cleanup;
    for (i = 0u; i < RECV_BYTES; ++i)
        if ((unsigned char)mem_buffer_data(buffer)[i] != 0xa5u)
            goto cleanup;
    /* The same buffer remained valid and untouched until the precise
     * ERROR_OPERATION_ABORTED completion was returned by the kernel. */
    result = 0;

cleanup:
    if (initialized) {
        if (!closed) {
            (void)native_io_backend_close(&backend);
            closed = true;
        }
        if (submitted && !terminal) {
            (void)native_io_backend_cancel(&backend, request);
            for (i = 0u; i < MAX_OBSERVES && !terminal; ++i) {
                native_io_completion c[2] = {{0}};
                size_t count = 0u, j;
                status = native_io_backend_observe(&backend, c, 2u, 2u, &count);
                if (status != SALTS_OK && status != SALTS_ETIMEDOUT)
                    break;
                for (j = 0u; j < count; ++j)
                    if (c[j].request.slot == request.slot
                        && c[j].request.generation == request.generation)
                        terminal = true;
            }
        }
        if (submitted && !terminal) {
            /* Explicit safety fail: never free kernel-borrowed memory or
             * tear down its overlapped request if no terminal was seen.
             */
            fprintf(stderr, "IOCP cycle %d: terminal missing; retained buffers not freed\n",
                    index);
            return 9;
        }
        if (accepted != INVALID_SOCKET) {
            (void)closesocket(accepted);
            accepted = INVALID_SOCKET;
        }
        if (native_io_endpoint_valid(endpoint)
            && native_io_backend_release_socket(&backend, endpoint) != SALTS_OK)
            result = 1;
        if (native_io_backend_destroy(&backend) != SALTS_OK)
            result = 1;
    } else if (accepted != INVALID_SOCKET) {
        (void)closesocket(accepted);
    }
    if (buffer) mem_buffer_release(buffer);
    if (client != INVALID_SOCKET) (void)closesocket(client);
    if (listener != INVALID_SOCKET) (void)closesocket(listener);
    if (result)
        fprintf(stderr, "Windows real overlapped IOCP cancellation cycle %d failed\n",
                index);
    return result;
}

int main(void)
{
    WSADATA data;
    int result = 0, i;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 2;
    for (i = 0; i < REAL_CANCEL_CYCLES; ++i) {
        if (run_cycle(i) != 0) {
            result = 1;
            break;
        }
    }
    (void)WSACleanup();
    if (!result)
        puts("Real Windows IOCP OVERLAPPED TCP_RECV CancelIoEx terminal and retained Core buffer: PASS (32 cycles)");
    return result;
}
