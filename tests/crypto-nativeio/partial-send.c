/* SPDX-License-Identifier: Apache-2.0
 * REAL POSIX NativeIO TCP_SEND terminal partial-completion acceptance.
 * Exercise immutable SaltsCore retained mem_buffer backing across a native
 * short send and its exact-prefix resubmission, not a synthetic completion.
 */
#define _POSIX_C_SOURCE 200809L
#include <salts/native_io.h>
#include <salts/error_codes.h>
#include <cmeta_buffer.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum { PAYLOAD_SIZE = 256 * 1024, MIN_SEND_BUFFER = 1024,
       MAX_STEPS = 40000, OBSERVE_BATCH = 4 };

static native_io_backend_kind native_kind(void)
{
#if defined(__linux__)
    return NATIVE_IO_BACKEND_EPOLL;
#elif defined(__APPLE__)
    return NATIVE_IO_BACKEND_KQUEUE;
#else
#error Actual POSIX NativeIO partial send test requires epoll/kqueue
#endif
}

static int connected(int *out_send, int *out_recv)
{
    struct sockaddr_in address = {0};
    socklen_t address_len = sizeof(address);
    int listener = -1, send_socket = -1, recv_socket = -1, flags;
    *out_send = *out_recv = -1;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) goto fail;
    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1
        || bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0
        || getsockname(listener, (struct sockaddr *)&address,
                       &address_len) != 0
        || listen(listener, 1) != 0) goto fail;
    send_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (send_socket < 0
        || connect(send_socket, (struct sockaddr *)&address,
                   sizeof(address)) != 0) goto fail;
    recv_socket = accept(listener, NULL, NULL);
    if (recv_socket < 0) goto fail;
    flags = fcntl(send_socket, F_GETFL, 0);
    if (flags < 0
        || fcntl(send_socket, F_SETFL, flags | O_NONBLOCK) != 0)
        goto fail;
    flags = fcntl(recv_socket, F_GETFL, 0);
    if (flags < 0
        || fcntl(recv_socket, F_SETFL, flags | O_NONBLOCK) != 0)
        goto fail;
    close(listener);
    *out_send = send_socket;
    *out_recv = recv_socket;
    return 0;
fail:
    if (listener >= 0) close(listener);
    if (send_socket >= 0) close(send_socket);
    if (recv_socket >= 0) close(recv_socket);
    return -1;
}

static int drain_receiver(int fd, const char *original,
                          size_t accepted, size_t *read_cursor)
{
    unsigned char bytes[8192];
    ssize_t n;
    while (*read_cursor < accepted) {
        size_t remain = accepted - *read_cursor;
        size_t ask = remain < sizeof(bytes) ? remain : sizeof(bytes);
        n = recv(fd, bytes, ask, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0;
        if (n <= 0 || (size_t)n > remain)
            return -1;
        if (memcmp(bytes, original + *read_cursor, (size_t)n) != 0)
            return -2;
        *read_cursor += (size_t)n;
    }
    return 0;
}

int main(void)
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
    int source_fd = -1, target_fd = -1;
    int small_buffer = MIN_SEND_BUFFER;
    int rc = 1, status;

    if (connected(&source_fd, &target_fd) != 0) goto done;
    if (setsockopt(source_fd, SOL_SOCKET, SO_SNDBUF,
                   &small_buffer, sizeof(small_buffer)) != 0)
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

    /* First request is intentionally larger than OS SO_SNDBUF and the
     * receiver does not read before its terminal event. The actual OS
     * must produce a partial positive NativeIO completion.
     */
    operation.buffer = mem_buffer_data(payload);
    operation.length = PAYLOAD_SIZE;
    if (native_io_backend_submit(&backend, &operation, &request) != SALTS_OK)
        goto done;
    outstanding = true;

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
    if (source_fd >= 0) close(source_fd);
    if (target_fd >= 0) close(target_fd);
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
    else printf("NativeIO real TCP OS short-send PASS: "
        "retained Core mem_buffer until %zu completions, %zu partial\n",
        observed_parts, short_parts);
    return rc;
}
