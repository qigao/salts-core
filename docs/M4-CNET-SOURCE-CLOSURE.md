# M4.2 — standalone CNet source extraction (provider qualification pending)

**Draft only. No existing Salts binary, provider, or consumer switches.**

## Immutable provenance

CNet sources taken from `qigao/salts`, head
`36889896e7e79fb9842accd9a35ac6e59f0ddab4` of
`refactor/cnet-ipc-name-native`, which stacks:
- Salts #1290: fixed-capacity IPC accepted-child FIFO instead of CSTL deque;
- Salts #1291: private indexed peer map instead of CMeta/CSTL HashMap;
- Salts #1293: caller-owned native IPC path instead of tstr/SDS.

All 51 private CNet source/header files and 13 public CNet headers are
migrated to `src/cnet` / `include/cnet`. Private Monocypher namespace
sources and Reed-Solomon source/attribution live under `vendor/cnet`.
Only *documentation* in headers is changed to reflect the new target owner.
The RFC 6455 parser is consumed from separate M4.1 (PR #14) and compiled
as one private object; URI comes from the re2c-based PR #11.

## Optional target and dependency closure

Build flag: `SALTSCORE_BUILD_CNET=ON`. Core must not enable network/TLS by
default; both `SALTSCORE_BUILD_CNET=ON` and `SALTSCORE_BUILD_URI=ON` are
explicit choices. A configured provider-backed build requires:

- `c-ares::cares`: asynchronous DNS; exact private provider package;
- `kcp::kcp`: KCP transport;
- `GmSSL::GmSSL`: pinned, security-reviewed GmSSL 3.2 overlay with matching
  public headers/ABI (including TLS external I/O and X.509 verification);
- SIMDe includes for Reed private implementation;
- host re2c (build-time URI lexer only);
- platform socket/security frameworks (Win32 Crypt32/Winsock; Apple Security
  and CoreFoundation where appropriate).

No fallback to system OpenSSL, libuv, plaintext, old Salts libraries, a
fake CMeta/CFlow provider or unversioned vendored CNet is accepted. CMake
uses REQUIRED provider lookups, with no production target when disabled.

One SHARED `SaltsCore::CNet` owns the production CNet Client/Manager/
Handoff/Listener/UDP/KCP/TLS/WebSocket/Resolver implementations. It links
publicly to new Core/Concurrency/NativeIO and privately to URI, Platform,
KCP, c-ares, GmSSL and one namespaced Monocypher/Reed implementation.
All lower static libraries use PIC when linked into this DSO. The binary
is a new ABI identity; original Salts::CNet symbols cannot be linked as a
second owner in the same process.

## What the provider-free CI actually validates

`saltscore.cnet_policy` compiles *actual* original C11 source for CNet
destination selection, owner placement and retry/recovery decisions without
GmSSL/KCP/c-ares. It exercises no-retry-on-security failure, expiry, strict
pinning versus eligibility, weighted RR and failure-output semantics.

Standard SaltsCore CI exercises this test on Linux/Windows/macOS, runs
cross-language installed minimal-SDK consumers and the no-Salts dependency
scanner. The M4.1 WebSocket parser has its own 3-platform RFC 6455 CI.

## Full CNet qualification required before switching anyone

1. **Provider closure**: configure/compile/link all source against exact
   GmSSL + kcp + c-ares + SIMDe packages on Linux/Windows/macOS; include
   installed SDK C11/C++17 link and runtime test. Verify Windows runtime DLL
   closure and relocation. Fail closed when any provider is missing.
2. **DNS/TLS**: real CA/SAN/trust rejection, ALPN, fatal-alert drain, TLS1.2/
   TLS1.3 and GmSSL ShangMi suites; independent OpenSSL/libwebrtc peer
   interop where applicable. No self-loop-only proof.
3. **Transport**: real TCP, UDP, KCP, WebSocket RFC6455 over live sockets,
   IPC/pipe/VSOCK platform limits, retained sends and exact terminal callback
   matching under cancellation.
4. **Concurrency**: NativeIO sharded host leases and cross-shard transfers,
   ownership races, TSan/ASan/UBSan, unload/quiescent drain on real systems.
5. **API/ABI**: C11/C++17 exported headers, public layout, symbol list,
   provider visibility and one binary owner. Only then switch Salts CMeta/ACE
   downstream integration tests; no accidental duplicate target definitions.
6. **Packaging**: exact source SHA + test run/attempt + RID + GmSSL provider
   artifact identity; Android/iOS cross build is separate from device runtime
   acceptance.

**Nothing in this PR qualifies production TLS, GmSSL, or full CNet.** It is a
complete source/import boundary candidate plus provider-free contract tests.
