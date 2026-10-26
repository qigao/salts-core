/* SPDX-License-Identifier: Apache-2.0
 * REAL IOCP/epoll/kqueue TCP_SEND terminal partial-completion acceptance.
 * Exercise immutable SaltsCore retained mem_buffer backing across a native
 * short send and its exact-prefix resubmission, not a synthetic completion.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <salts/native_io.h>
#include <salts/error_codes.h>
#include <cmeta_buffer.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_tcp_compat.h"

enum { PAYLOAD_SIZE = 256 * 1024, MIN_SEND_BUFFER = 1024,
       MAX_STEPS = 40000, OBSERVE_BATCH = 4 };

static native_io_backend_kind native_kind(void)
{
#if defined(_WIN32)
    return NATIVE_IO_BACKEND_IOCP;
#elif defined(__linux__)
    return NATIVE_IO_BACKEND_EPOLL;
#elif defined(__APPLE__)
    return NATIVE_IO_BACKEND_KQUEUE;
#else
#error Actual NativeIO partial send test requires IOCP/epoll/kqueue
#endif
}

static int drain_receiver(native_tcp_socket fd, const char *original,
                          size_t accepted, size_t *read_cursor)
{
    unsigned char bytes[8192];
    int n;
    while (*read_cursor < accepted) {
        size_t remain = accepted - *read_cursor;
        size_t ask = remain < sizeof(bytes) ? remain : sizeof(bytes);
        n = recv(fd, (char *)bytes, (int)ask, 0);
        if (n < 0 && native_tcp_would_block())
            return 0;
        if (n <= 0 || (size_t)n > remain)
            return -1;
        if (memcmp(bytes, original + *read_cursor, (size_t)n) != 0)
            return -2;
        *read_cursor += (size_t)n;
    }
    return 0;
}

static int run_partial_send(void)
{
    native_io_backend backend = {0};
    native_io_backend_config config = {0};
    native_io_endpoint endpoint = {0};
    native_io_request request = {0};
    native_io_operation operation = {0};
    native_io_completion events[OBSERVE_BATCH];
    mem_buffer_t *payload = NULL;
    size_t accepted = 0u, read_cursor = 0u, step = 0u;
    size_t observed_parts = 0u, short_parts = 0u;
    bool started = false, endpoint_attached = false, outstanding = false;
    bool backend_closed = false;
    native_tcp_socket source_fd = NATIVE_TCP_INVALID_SOCKET;
    native_tcp_socket target_fd = NATIVE_TCP_INVALID_SOCKET;
    int small_buffer = MIN_SEND_BUFFER;
    int rc = 1, status;

    if (native_tcp_connect_pair(&source_fd, &target_fd) != 0) goto done;
    if (setsockopt(source_fd, SOL_SOCKET, SO_SNDBUF,
                   (const char *)&small_buffer, (int)sizeof(small_buffer)) != 0)
        goto done;

    config = (native_io_backend_config){native_kind(), 1u, 2u, 2u};
    if (native_io_backend_init(&backend, &config) != SALTS_OK) goto done;
    started = true;
    if (native_io_backend_attach_socket(&backend, (uintptr_t)source_fd,
                                        &endpoint) != SALTS_OK) goto done;
    endpoint_attached = true;
    payload = mem_get_buffer(mem_global(), PAYLOAD_SIZE);
    if (!payload || mem_buffer_capacity(payload) < PAYLOAD_SIZE) goto done;
    for (size_t i = 0u; i < PAYLOAD_SIZE; ++i)
        mem_buffer_data(payload)[i] = (char)(i * 43u + 11u);
    mem_set_used(payload, PAYLOAD_SIZE);

    operation.kind = NATIVE_IO_OPERATION_TCP_SEND;
    operation.endpoint = endpoint;
    operation.user_data = 0xC1u;

    /* A real 256 KiB overlapped send exceeds SO_SNDBUF. Its immutable
     * mem_buffer_t stays borrowed until observed terminal. On readiness
     * backends we demand a positive short OS completion before reading.
     * On IOCP we first demand genuine asynchronous backpressure, then
     * unblock the outstanding WSASend by receiving from the peer; Windows
     * may complete the *whole* overlapped request, not a short prefix.
     */
    operation.buffer = mem_buffer_data(payload);
    operation.length = PAYLOAD_SIZE;
    if (native_io_backend_submit(&backend, &operation, &request) != SALTS_OK)
        goto done;
    outstanding = true;

#if defined(_WIN32)
    /* Nonblocking loopback receiver is deliberately idle for 64 real IOCP
     * polling cycles. Full-send completion before peer drain is NOT
     * admitted as evidence of an overlapped retained-backpressure request.
     * POSIX's short completion contract remains separate below.
     */
    for (step = 0u; step < 64u; ++step) {
        size_t count = 0u;
        status = native_io_backend_observe(
            &backend, events, OBSERVE_BATCH, 1u, &count);
        if ((status != SALTS_OK && status != SALTS_ETIMEDOUT)
            || count != 0u)
            goto done;
    }

    /* The request is still in the kernel with an immutable borrowed
     * backing. Remove real TCP backpressure by consuming and verifying
     * bytes through Winsock recv (not a synthetic NativeIO completion).
     * An actual IOCP terminal is still required before buffer release.
     * Bound diagnosis by wall time: Windows 1ms GQCS can sleep longer
     * than 1ms, so a large iteration budget alone can exceed CTest.
     */
    const ULONGLONG started_ms = GetTickCount64();
    const ULONGLONG deadline_ms = started_ms + 6000u;
    ULONGLONG next_diagnostic_ms = started_ms + 1000u;
    for (step = 0u; step < MAX_STEPS &&
                    GetTickCount64() < deadline_ms &&
                    (outstanding || accepted != PAYLOAD_SIZE ||
                     read_cursor != PAYLOAD_SIZE); ++step) {
        size_t count = 0u;
        if (drain_receiver(target_fd, mem_buffer_const_data(payload),
                           PAYLOAD_SIZE, &read_cursor) != 0)
            goto done;
        if (!outstanding && accepted < PAYLOAD_SIZE) {
            operation.buffer = mem_buffer_data(payload) + accepted;
            operation.length = PAYLOAD_SIZE - accepted;
            if (native_io_backend_submit(&backend, &operation,
                                         &request) != SALTS_OK)
                goto done;
            outstanding = true;
        }
        status = native_io_backend_observe(
            &backend, events, OBSERVE_BATCH, 1u, &count);
        if (status != SALTS_OK && status != SALTS_ETIMEDOUT)
            goto done;
        if (GetTickCount64() >= next_diagnostic_ms) {
            fprintf(stderr,
                "IOCP WSASend progress: ms=%llu peer_received=%zu "
                "completed_bytes=%zu pending=%d terminals=%zu short=%zu\\n",
                (unsigned long long)(GetTickCount64() - started_ms),
                read_cursor, accepted, outstanding ? 1 : 0,
                observed_parts, short_parts);
            next_diagnostic_ms += 1000u;
        }
        if (count) {
            const size_t remaining = PAYLOAD_SIZE - accepted;
            if (count != 1u || !outstanding
                || events[0].kind != NATIVE_IO_COMPLETION_OK
                || events[0].request.slot != request.slot
                || events[0].request.generation != request.generation
                || events[0].user_data != 0xC1u
                || !events[0].bytes || events[0].bytes > remaining)
                goto done;
            if (events[0].bytes < remaining) ++short_parts;
            accepted += events[0].bytes;
            ++observed_parts;
            outstanding = false;
            request = (native_io_request){0};
        }
    }
    if (outstanding || accepted != PAYLOAD_SIZE ||
        read_cursor != PAYLOAD_SIZE || observed_parts == 0u) {
        fprintf(stderr,
            "IOCP WSASend deadline: peer_received=%zu/%u "
            "completed_bytes=%zu/%u pending=%d terminals=%zu short=%zu\\n",
            read_cursor, PAYLOAD_SIZE, accepted, PAYLOAD_SIZE,
            outstanding ? 1 : 0, observed_parts, short_parts);
        goto done;
    }
#else
    for (step = 0u; step < MAX_STEPS; ++step) {
        size_t count = 0u;
        status = native_io_backend_observe(
            &backend, events, OBSERVE_BATCH, 1u, &count);
        if (status != SALTS_OK && status != SALTS_ETIMEDOUT)
            goto done;
        if (!count) continue;
        if (count != 1u || !outstanding ||
            events[0].kind != NATIVE_IO_COMPLETION_OK ||
            events[0].request.slot != request.slot ||
            events[0].request.generation != request.generation ||
            events[0].user_data != 0xC1u ||
            events[0].bytes == 0u || events[0].bytes >= PAYLOAD_SIZE)
            goto done;
        accepted = events[0].bytes;
        ++observed_parts;
        ++short_parts;
        outstanding = false;
        request = (native_io_request){0};
        break;
    }
    if (outstanding || !short_parts) goto done;

    for (step = 0u; step < MAX_STEPS && read_cursor < PAYLOAD_SIZE; ++step) {
        size_t count = 0u;
        if (drain_receiver(target_fd, mem_buffer_const_data(payload),
                           accepted, &read_cursor) != 0) goto done;
        if (!outstanding && accepted < PAYLOAD_SIZE) {
            operation.buffer = mem_buffer_data(payload) + accepted;
            operation.length = PAYLOAD_SIZE - accepted;
            if (native_io_backend_submit(&backend, &operation,
                                         &request) != SALTS_OK)
                goto done;
            outstanding = true;
        }
        status = native_io_backend_observe(
            &backend, events, OBSERVE_BATCH, 1u, &count);
        if (status != SALTS_OK && status != SALTS_ETIMEDOUT)
            goto done;
        if (count) {
            size_t remaining = PAYLOAD_SIZE - accepted;
            if (count != 1u || !outstanding
                || events[0].kind != NATIVE_IO_COMPLETION_OK
                || events[0].request.slot != request.slot
                || events[0].request.generation != request.generation
                || !events[0].bytes || events[0].bytes > remaining)
                goto done;
            if (events[0].bytes < remaining) ++short_parts;
            accepted += events[0].bytes;
            ++observed_parts;
            outstanding = false;
            request = (native_io_request){0};
        }
        if (accepted == PAYLOAD_SIZE &&
            drain_receiver(target_fd, mem_buffer_const_data(payload),
                           accepted, &read_cursor) != 0)
            goto done;
    }
    if (accepted != PAYLOAD_SIZE || read_cursor != PAYLOAD_SIZE
        || observed_parts < 2u || !short_parts)
        goto done;
#endif

    if (native_io_backend_close(&backend) != SALTS_OK) goto done;
    backend_closed = true;
    rc = 0;
done:
    if (started) {
        if (!backend_closed) {
            (void)native_io_backend_close(&backend);
            backend_closed = true;
        }
        if (outstanding) {
            status = native_io_backend_cancel(&backend, request);
            if (status != SALTS_OK && status != SALTS_EALREADY) rc = 1;
            for (unsigned tries = 0u; tries < MAX_STEPS && outstanding; ++tries) {
                size_t count = 0u;
                status = native_io_backend_observe(
                    &backend, events, OBSERVE_BATCH, 1u, &count);
                if (status != SALTS_OK && status != SALTS_ETIMEDOUT) {
                    rc = 1;
                    break;
                }
                for (size_t i = 0u; i < count; ++i)
                    if (events[i].request.slot == request.slot &&
                        events[i].request.generation == request.generation)
                        outstanding = false;
            }
            if (outstanding) {
                fprintf(stderr, "NativeIO short-send cancellation not terminal: retain backing\n");
                return 4; /* deliberately do not free active OS borrow */
            }
        }
    }
    native_tcp_close(source_fd);
    native_tcp_close(target_fd);
    if (endpoint_attached &&
        native_io_backend_release_socket(&backend, endpoint) != SALTS_OK)
        rc = 1;
    if (started && native_io_backend_destroy(&backend) != SALTS_OK)
        rc = 1;
    if (payload) mem_buffer_release(payload);
    if (rc) fprintf(stderr,
        "NativeIO actual OS short-send contract failed: accepted=%zu/%u "
        "received=%zu terminals=%zu partial=%zu\n",
        accepted, PAYLOAD_SIZE, read_cursor, observed_parts, short_parts);
    else printf("NativeIO real TCP backpressure/short-send PASS: "
        "retained Core mem_buffer until %zu completions, %zu partial\n",
        observed_parts, short_parts);
    return rc;
}

int main(void)
{
    int rc;
    if (native_tcp_startup() != 0) return 2;
    rc = run_partial_send();
    native_tcp_shutdown();
    return rc;
}
