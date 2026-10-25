# M4.6 — Windows MSVC/IOCP CNet provider qualification (Draft)

Stacked on SaltsCore PR #18 (macOS Provider) and PR #17 (CNet public CA/SAN
negative test). The goal is actual Windows socket/TLS runtime and an external
installed SDK, not just a header or package build.

## Pinned dependency mechanics

The scoped Windows provider job selects `windows-2025` and a
`Visual Studio 18 2026` x64 CMake generator, with `x64-windows` vcpkg
triplet. Microsoft vcpkg scripts are fetched at exact SHA
`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`; the builtin
dependency baseline is `b1b19307e2d2ec1eefbdb7ea069de7d4bcd31f01`.

`vcpkg-configuration.json` retains qigao GmSSL 3.2.0#13 and other
registry locks, with explicit manifest `cnet` feature. The binary cache
is files-only, and is versioned independently of Linux/macOS. There is
no Mono and no NuGet executable.

Windows re2c build-time generator comes from the original Salts audited
`cmake/ci/github-packages.py` transport, imported byte-identically from
Salts `bbeecfff4615d1385787b05369879aa08ed7249a`. The script uses
`curl` with host allowlisting, GitHub package API authentication and ZIP
path/symlink checks. It emits the resolved re2c package version to the
GitHub Actions environment and checks runtime host binary/tool assets.
**Current re2c selection is the latest stable hosted package, not an exact
version pin**; audit/pin before production release.

## Runtime tests

- Complete Provider-backed `SaltsCore::CNet` DLL build with its pinned
  GmSSL/KCP/c-ares/SIMDe libraries, and NativeIO IOCP support.
- All source CTests, including explicit real loopback TCP
  retained-buffer/send/receive/close and real TLS positive/negative CA/SAN
  paths, GmSSL external I/O ABI and ShangMi provider-side crypto KATs.
- Install `SaltsCore::CNet`, then compile/link/run external C11/C++17
  consumers against only the installed SDK and resolved provider DLLs.
- Check CNet public headers, DLL and imported target actually exist.

This is a Draft CI candidate. Windows Provider run must be green at its
exact head before the platform is counted. Linux/macOS verified tests
remain separate evidence. Third-party TLS peer interoperability, UDP/KCP
protocol tests, TSan/ASan/UBSan, ABI export symbol audits and Salts ACE
consumer cutover remain separately gated in salts-core#3.
