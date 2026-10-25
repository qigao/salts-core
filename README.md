# SaltsCore

**Status: bootstrap / Draft qualification. Not a production replacement yet.**

SaltsCore is an independently built **C11 foundation SDK**. It does not depend
on the historical `qigao/salts`, `qigao/salts-utils`, CMeta, CFlow, or CSTL
packages. Development is tracked by [migration epic #1](https://github.com/qigao/salts-core/issues/1).

The original M0 bootstrap implements an **owned-by-caller, non-owning counted byte view**
in `SaltsCore::Core`. The stacked M1/M2 migration candidate adds
`SaltsCore::Platform` (clock, OS CSPRNG, thread/synchronization) and
`SaltsCore::Concurrency` (SPSC, bounded deadline queue, RCU, Disruptor,
ThreadPool and Leader/Followers). The next stacked candidate adds
`SaltsCore::Coroutine` (the original minicoro-backed lifecycle, pool and
bounded sharded executor). These are imported from exact Salts sources and
have no CMeta/CFlow dependencies. The next Core foundation slice extracts
`mem_buffer_t`, `mem_slice_t` and the bounded slab allocator and the original single-owner bipartite `ring_buffer`
from old `salts/utils`, with only the independent Platform synchronization API. A further stacked candidate moves the
original NativeIO backend and Sharded/IPC ownership to
`SaltsCore::NativeIO` (IOCP, epoll, io_uring, kqueue); the ACE completion-token
adapter remains in Salts and is not installed by Core. They require exact-head Linux/Windows/macOS
CI and installed C11/C++17 qualification before this candidate is accepted. It is not yet a replacement for any
published `Salts::Core` binary, and no old APIs or CMake aliases have been
cut over.

## Planned modules

| Mandatory | Optional on supported platforms |
|---|---|
| Core byte views, Platform, Concurrency, Coroutine, NativeIO | URI (re2c build-time), CNet, Crypto, FS, Process, Unicode, IDNA, Serial, USB, Capture, Playback |

Unicode stays pinned to Unicode 17.0.0 with re2c as a **build-time** tool only;
IDNA depends only on the new Unicode module. FS and Process are native system
components; they **must not require** CFlow IO Actors or CMeta Reflection.
Advanced CMeta/CFlow/CSTL/Component/ACE functionality remains in `qigao/salts`
and consumes the new Core, never the other way round. The plain CNet transport
implementation will migrate to optional `SaltsCore::CNet`; CMeta/ACE-specific
CNet integrations stay in Salts.

See [architecture](docs/ARCHITECTURE.md) for ownership and migration gates.

## CNet source migration (Draft, opt-in only)

The next stacked Draft candidate stages the whole CNet transport source tree,
after the original Salts CNet IPC FIFO, peer index and native-path changes.
Provider-free CNet destination/owner/recovery policies are tested on all three
desktop platforms, even with the transport disabled.

To request a **real CNet provider build** explicitly, provide a verified
GmSSL (project overlay), c-ares, KCP and SIMDe SDK and an installed host re2c:

```sh
cmake -S . -B build-cnet -DBUILD_TESTING=ON \
  -DSALTSCORE_BUILD_URI=ON -DSALTSCORE_BUILD_CNET=ON \
  -DVCPKG_MANIFEST_FEATURES=cnet \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/verified-vcpkg-toolchain.cmake
cmake --build build-cnet --parallel
ctest --test-dir build-cnet --output-on-failure
```

The checked-in vcpkg manifest keeps CNet under an explicit feature; the
pinned registry snapshot uses GmSSL 3.2.0#13. The scoped pinned-provider
Linux workflow is in `.github/workflows/cnet-provider.yml`.

An unavailable dependency fails configuration, without substituting another
TLS provider or an implicit transport fallback. **No full CNet provider build,
TLS/CA/SAN, real network interop, sanitizer or ABI qualification has yet been
established by the source-extraction change.** The original Salts CNet remains
the production owner until the consumers cut over together.

## Local M0 build

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /tmp/saltscore-sdk
cmake -S tests/installed -B build-installed -DCMAKE_PREFIX_PATH=/tmp/saltscore-sdk
cmake --build build-installed
ctest --test-dir build-installed --output-on-failure
python3 tools/check_dependency_boundaries.py
```

The standalone install exports `SaltsCore::Core` through
`find_package(SaltsCore CONFIG REQUIRED)`. Optional targets will be exported
only when their independent implementations and platform gates are qualified.
