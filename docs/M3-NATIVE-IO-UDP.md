# M3: NativeIO real UDP completion qualification

This isolated source-based conformance test proves a real UDP loopback
round-trip across the production-selected native completion/readiness backend:
Windows IOCP, Linux epoll, macOS kqueue.

Unlike the original `saltscore.native_io` smoke, it:
- Binds two real OS UDP sockets to loopback with ephemeral ports.
- Attaches each to a single native I/O backend and submits native receive/send
  operations with explicitly borrowed payload and sockaddr storage.
- Observes both terminal completions and validates request generation, copied
  user tag, byte count, IPv4 peer and payload bytes.
- Requires `SALTS_EBUSY` when releasing an endpoint that still owns an
  unsettled receive, then proves one actual cancelled terminal after cancel.
- Releases sockets after terminal completion, then releases endpoint records,
  closes and destroys the backend.

The test runs on Linux/Windows/macOS as an ordinary CTest and does **not**
use CFlow, CMeta, original Salts targets, a client/server framework or a
fallback transport. All request slots are fixed capacity.

This test is **not** full NativeIO qualification: additional tests are required
for TCP/pipe, partial writes, short/cancel races, wake-before-sleep, IOCP
accept, io_uring independent host/kernels, sharded host leases, DSO quiescence,
sanitizers and mobile SDK compile. It does not authorize production cutover.

Sources: independent NativeIO implementation currently in stacked Core Draft
PRs #6/#9/#11/#12, original pinned Salts
`d6d936a86087a7f9a982126157d154e905580f30`.
