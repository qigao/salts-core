# M4.5 — macOS pinned-provider CNet native execution (Draft)

Stacked on SaltsCore PR #17, where Linux CNet already has a verified TLS
CA/SAN regression. The target is **real macOS runtime**, not merely
cross-compilation or a provider-free Core SDK smoke test.

## Toolchain and dependency closure

The scoped `.github/workflows/cnet-provider.yml` adds a macOS-15 job.
It chooses `arm64-osx` or `x64-osx` from the actual runner architecture,
bootstraps Microsoft vcpkg scripts from exact commit
`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`, requires builtin baseline
`b1b19307e2d2ec1eefbdb7ea069de7d4bcd31f01`, and consumes the
checked-in custom registry that pins GmSSL 3.2.0#13.

Host build tools include re2c and Ninja, installed via Homebrew if missing.
The vcpkg manifest requests the explicit `cnet` feature, building
GmSSL/KCP/c-ares/SIMDe. Binary caching uses a **local files-only provider**,
never Mono/NuGet or an unversioned system TLS fallback.

## Runtime and installed-SDK gates

- Compile/link **all CNet source** and install `Salts::CNet` dylib.
- Run the full source CTest suite in macOS kqueue mode.
- Explicitly run the TCP accepted-loopback retained ownership test,
  real TLS trusted CA + correct SAN / wrong CA / wrong DNS identity test,
  GmSSL public TLS ABI callbacks and ShangMi provider KAT.
- Build and execute external C11/C++17 consumers against only the installed
  `SaltsCoreConfig.cmake` targets, without an installed old Salts package.
- Verify dylib and installed target files exist.

A failed macOS source/provider test must not be marked success on the
strength of the unrelated Linux job. Existing Linux qualification remains
unchanged. No production deployment or original Salts/ACE cutover is implied.

## Remaining after this job

Windows MSVC + IOCP provider-enabled build, CNet native cancellation/shard
close and DSO unload races, Linux ASan/UBSan and TSan, UDP/KCP/WebSocket
wire traffic, independently implemented TLS peer interoperability, Android
and iOS SDK/runtime, and explicit ABI/SONAME ownership handoff.
