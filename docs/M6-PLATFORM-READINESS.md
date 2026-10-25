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

## M6.9 — failure-atomic fake backend conformance

`saltscore.readiness_faults` now injects exact backend
`SALTS_EIO` failures into the *real* shared Readiness state machine
for register, arm, unarm, close and shutdown. The test requires
proper handle rollback, retryable registration/close, fixed capacity,
generation fencing (old tokens cannot trigger new registration
callbacks), stale versus duplicate delivery accounting, and exactly
one terminal callback for shutdown/fatal errors. The backend fixture
remains test-local and borrows caller-owned state; no Platform
implementation source is copied into the test or CFlow.

Linux CNet/provider ASan/UBSan/LSan and TSan each repeat this test
alongside the actual pipe/close test; these tests qualify only the
enumerated failure-atomic operations, not every original Salts
Readiness injection case. Extended blocked-callback cancellation and
platform-specific poll/kqueue/epoll conformance remain pending.

## Sanitizer workflow syntax gate

The SDK workflow now runs `tools/check_ci_ctest_regex.py` against the
actual CNet sanitizer workflow before qualification. It fails on a lost
closing quote, missing ^/$ anchors, invalid regular expression or an
empty `ctest -R` selector. This avoids an otherwise silent build-only
run being mistaken for ASan/TSan evidence. Both sanitizer binaries
still execute the full selected suite with LeakSanitizer/TSan enabled.
