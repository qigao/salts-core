# M6.8 — original Platform Readiness Reactor single-owner extraction

This stacked Draft is a source-identical extraction of the real Salts
Platform Readiness state machine and OS backends from Salts commit
`df4c697d2f1facb6b54a3568520dd743c628aa81`.
Six Git blobs (public `readiness.h`, private `readiness_internal.h`,
state engine `readiness.c`, and the poll, kqueue and epoll
implementations) have exactly the same SHA as the source repository.

`SaltsCore::Platform` now owns readiness registration, callback
delivery, terminal reservation, generation/handle admission, cancellation,
shutdown and thread/DSO lifetime. The original CFlow/ACE consumer is not
allowed to embed any of these source objects into itself.

- POSIX: explicit **poll** backend is always compiled; macOS also
  compiles and selects kqueue as the default native backend.
- Linux epoll remains **opt-in** under `SALTSCORE_ENABLE_EPOLL_READINESS`
  (default OFF like the original Salts option). Unsupported backend
  requests return `SALTS_ENOTSUP`; the explicit backend selector never
  falls back to poll, and the default native constructor does not
  silently switch providers.
- Windows: the public API and state machine are available, but no
  unsupported POSIX reactor backend is pretended. Explicit selections
  must reject `SALTS_ENOTSUP`.
- The original C11 memory/thread Platform owner remains a single
  shared `SaltsCore::Platform` binary with ABI/SONAME 0.1.

Source CTest exercises real pipe readiness callback, bounded quiescent
close/shutdown/destroy, stats and backend selection where supported.
Installed C11/C++17 and final linked ELF/Mach-O/PE audits enforce
the same public symbols from the one Platform owner. Original backend
contract/fault-injection and repeated TSan/ASan/UBSan tests are required
before stable ownership transfer; plain compile/link is not sufficient.

This is an additive pre-stable 0.1 candidate. Old Salts 2.3 still
defines overlapping readiness symbols; the two must never be linked
together in one process. Downstream Salts CFlow migration is tracked
in Draft PR #1311 stacked over Salts #1304; no production cutover.
