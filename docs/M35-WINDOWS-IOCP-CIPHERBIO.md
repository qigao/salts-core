# M3.5 — Windows IOCP / Overlapped Winsock qualification

**Test-only Draft**, stacked on SaltsCore #26. Issue #27 is the production
CNet Owner gate. This is not an implementation of `cnet_tls_state`, nor a
replacement for the existing `GmSSL::GmSSL` linkage. Production
`vcpkg-cache gmssl 3.2.0#13` remains pinned and unchanged.

## Actual Windows transport and ownership

Windows `tests/crypto-nativeio/native_tcp_compat.h` creates real IPv4
loopback listener and connector sockets with
`WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, ..., WSA_FLAG_OVERLAPPED)`.
Accepted sockets inherit the overlapped mode. The installed
`SaltsCore::NativeIO` backend must select `NATIVE_IO_BACKEND_IOCP`
and report `NATIVE_IO_MODEL_COMPLETION`.

On the one Owner thread, real TCP send and receive operations use
`native_io_backend_submit` and terminal `native_io_backend_observe`.
The shared NativeIO Windows implementation uses
`WSASend/WSARecv` with retained `OVERLAPPED` request records and
`GetQueuedCompletionStatus` for actual completion packets. These tests
never publish forged data completions. Worker threads may call only the
documented wake method.

The C11 TLS test uses two installed opaque Crypto sessions and two
host-only CipherBIOs for real TLS 1.2 and 1.3 handshakes,
authenticated plaintext and close_notify. Ciphertext taken from BIO
is copied into a `mem_buffer_t` owned by the *one shared SaltsCore
Core*. That immutable send backing stays alive until the matching
observed completion; NativeIO receive backing remains exclusively
borrowed until its own matching observed terminal. Each completion
checks slot/generation/user_data; only then may it advance the
acknowledged prefix or return bytes to BIO. Cancellation requests alone
are never terminal proof. BIO callback leases and Crypto sessions are
destroyed only after the NativeIO terminal drain barrier.

## Different OS completion models — no false parity

- **Linux epoll/macOS kqueue:** the existing strict real-short-send
  test requires a *positive, shorter-than-requested* kernel terminal
  against a deliberately small `SO_SNDBUF`. It then resubmits the exact
  remaining immutable prefix and proves all 256 KiB. No relaxed POSIX
  result.
- **Windows IOCP:** `WSASend` may retain an overlapped operation
  without returning a short prefix. Waiting for a positive partial
  completion *before* reading the peer deadlocks under backpressure.
  The test polls up to 64 times without peer reads; **a real early IOCP
  completion is allowed and retired exactly once**, because Windows
  may buffer the entire overlapped request even with a small
  `SO_SNDBUF`. When it remains pending, the peer consumes and verifies
  actual WinSock TCP bytes to unblock it. In both cases, matching
  observed IOCP terminal(s) must account for every transmitted byte;
  if short, the exact remaining prefix is resubmitted. All 256 KiB must
  match. This qualifies real **IOCP send completion/borrow retention**,
  not a claimed positive-short or necessarily pending Windows send.
- **Wake/cancel:** 64 independent Winsock/IOCP rounds, a distinct
  worker calling only `native_io_backend_wake`, an Owner-only
  cancellation, a real matching CANCELLED terminal, and no premature
  receive backing release.
- The Windows installed C++17 executable separately proves ABI and
  package graph. It does not count as the real IOCP data-path test.

## CI and remaining blocks

`.github/workflows/cnet-crypto-iocp.yml` builds pinned original
GmSSL from exact source, installs one shared Crypto TLS owner plus
a static host CipherBIO and separately installs one shared
SaltsCore Core/NativeIO, with `SALTSCORE_BUILD_CNET=OFF`.
MSVC Release must run all four CTests three times and audit final PE
imports for exactly one Crypto TLS DSO and one Core runtime, with no
independent GmSSL Provider, legacy Salts or production CNet target.

At the first native Windows test attempt, TLS failed due the
Win32 fixture path escaping in MSVC compile definitions; forward
slashes fixed this without changing Crypto verification. The next
Windows run successfully executed real IOCP TLS1.2/TLS1.3,
64-round cancellation and C++17 API tests, but the inherited
POSIX-specific short-send test timed out while the overlapped
`WSASend` awaited peer reads. The separate Windows IOCP
backpressure test above is the strict corrected acceptance;
**do not mark this new test green without its own exact-HEAD CI**.

Still outside this PR:

1. Production `cnet_tls_state` and NativeIO Owner state-machine
   adapter with explicit feature/capability negotiation and fail-closed
   rejection of unsupported modes.
2. CA directory and OS trust, P-384/ECDSA, ShangMi/TLCP, mTLS,
   signature-aware RFC5929 binding, negotiated metadata,
   pending plaintext, EOF/error mapping, and CNet transport policies
   tracked in Crypto #18.
3. Combined TLS callback AGAIN, CNet retained `mem_slice_t`,
   NativeIO cancellation/terminal racing, shutdown, module unload,
   single shared foundation owner (#20), and sanitizer evidence.

**Keep all migration PRs Draft, do not merge/publish or switch CNet.**
