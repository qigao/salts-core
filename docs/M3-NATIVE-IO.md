# M3: NativeIO source extraction (Draft)

Parent: [salts-core#3](https://github.com/qigao/salts-core/issues/3).
Source: qigao/salts master `d6d936a86087a7f9a982126157d154e905580f30`.

## Implementation provenance
- Copied without algorithm changes: all `native-io/src/*.c` and supporting
  private headers from old Salts native-io module.
- Public headers copied: `native_io.h`, `native_io_sharded.h`,
  `native_ipc.h`; keep their original C11 ABI for independent inspection.
- **Not copied:** `native_io_ace_token.h` — this POSA/ACE projection and any
  CMeta Interface bindings remain in Salts as adapters over Core NativeIO.
- `Salts::NativeIO` links publicly to Platform and privately to
  Coroutine (which depends on Concurrency). It does not link old
  Salts/CMeta/CFlow or own a second service/owner hierarchy.
- The private OS implementation branches retain IOCP (Windows), epoll/
  io_uring/readiness (Linux), kqueue/readiness (macOS/BSD), native pipes and
  IPC, and the sharded owner/external host lease interfaces.

## Explicit outstanding safety/conformance
- No production dual linking: the source-level `native_io_*` symbols are
  identical to old Salts. Old binary owner is removed only upon coordinated
  downstream cutover, not on this Draft PR.
- Validate terminal completion and retention of borrowed buffers across
  successful admission, cancellation, close and quiescence on each OS backend.
- Re-run original installed SDK + full NativeIO/IPC/Sharded test matrix
  (including races, lost wakes, cancel/close, generation reuse, host leases,
  shared ownership, TSan/ASan/UBSan).
- Linux IOURING is optional and must return unsupported rather than silently
  falling back to epoll when explicitly selected and unavailable.
- Qualify Android ARM64/iOS ARM64 toolchains separately; native desktop CTest
  runs are not equivalent to mobile execution.
- No GmSSL/CNet/CMeta/ACE types cross the public NativeIO ABI.
