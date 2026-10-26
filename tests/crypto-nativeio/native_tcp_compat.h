/* SPDX-License-Identifier: Apache-2.0
 * Test-only real TCP fixture. On Windows, the listener and connector are
 * explicitly created as overlapped Winsock sockets; accept() inherits
 * the listener's overlapped property. The data path must still use
 * SaltsCore NativeIO's WSASend/WSARecv -> GetQueuedCompletionStatus,
 * never synchronous send/recv or an emulated completion queue.
 */
#ifndef SALTSCORE_CRYPTO_NATIVE_TCP_COMPAT_H
#define SALTSCORE_CRYPTO_NATIVE_TCP_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
# if !defined(_WIN32_WINNT)
#  define _WIN32_WINNT 0x0600
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN 1
# endif
# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>
typedef SOCKET native_tcp_socket;
typedef int native_tcp_socklen;
# define NATIVE_TCP_INVALID_SOCKET INVALID_SOCKET
static inline int native_tcp_startup(void)
{
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0 ? 0 : -1;
}
static inline void native_tcp_shutdown(void) { (void)WSACleanup(); }
static inline native_tcp_socket native_tcp_open_stream(void)
{
    return WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
                      WSA_FLAG_OVERLAPPED);
}
static inline int native_tcp_set_nonblocking(native_tcp_socket s)
{
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0 ? 0 : -1;
}
static inline void native_tcp_close(native_tcp_socket s)
{
    if (s != NATIVE_TCP_INVALID_SOCKET) (void)closesocket(s);
}
static inline int native_tcp_would_block(void)
{
    return WSAGetLastError() == WSAEWOULDBLOCK;
}
static inline void native_tcp_yield(void) { Sleep(0); }
#else
# include <arpa/inet.h>
# include <errno.h>
# include <fcntl.h>
# include <netinet/in.h>
# include <sched.h>
# include <sys/socket.h>
# include <unistd.h>
typedef int native_tcp_socket;
typedef socklen_t native_tcp_socklen;
# define NATIVE_TCP_INVALID_SOCKET (-1)
static inline int native_tcp_startup(void) { return 0; }
static inline void native_tcp_shutdown(void) {}
static inline native_tcp_socket native_tcp_open_stream(void)
{
    return socket(AF_INET, SOCK_STREAM, 0);
}
static inline int native_tcp_set_nonblocking(native_tcp_socket s)
{
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0 ? 0 : -1;
}
static inline void native_tcp_close(native_tcp_socket s)
{
    if (s != NATIVE_TCP_INVALID_SOCKET) (void)close(s);
}
static inline int native_tcp_would_block(void)
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}
static inline void native_tcp_yield(void) { sched_yield(); }
#endif

static inline int native_tcp_connect_pair(native_tcp_socket *out_client,
                                          native_tcp_socket *out_server)
{
    struct sockaddr_in address;
    native_tcp_socklen length = (native_tcp_socklen)sizeof(address);
    native_tcp_socket listener = NATIVE_TCP_INVALID_SOCKET;
    native_tcp_socket client = NATIVE_TCP_INVALID_SOCKET;
    native_tcp_socket server = NATIVE_TCP_INVALID_SOCKET;

    if (!out_client || !out_server) return -1;
    *out_client = *out_server = NATIVE_TCP_INVALID_SOCKET;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0u);
    listener = native_tcp_open_stream();
    if (listener == NATIVE_TCP_INVALID_SOCKET
        || bind(listener, (const struct sockaddr *)&address,
                (int)sizeof(address)) != 0
        || getsockname(listener, (struct sockaddr *)&address, &length) != 0
        || listen(listener, 2) != 0)
        goto fail;
    client = native_tcp_open_stream();
    if (client == NATIVE_TCP_INVALID_SOCKET
        || connect(client, (const struct sockaddr *)&address,
                   (int)sizeof(address)) != 0)
        goto fail;
    server = accept(listener, NULL, NULL);
    if (server == NATIVE_TCP_INVALID_SOCKET
        || native_tcp_set_nonblocking(client) != 0
        || native_tcp_set_nonblocking(server) != 0)
        goto fail;
    native_tcp_close(listener);
    *out_client = client;
    *out_server = server;
    return 0;
fail:
    native_tcp_close(server);
    native_tcp_close(client);
    native_tcp_close(listener);
    return -1;
}
#endif
