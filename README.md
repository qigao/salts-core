# SaltsCore

**Status: bootstrap / Draft qualification. Not a production replacement yet.**

SaltsCore is an independently built **C11 foundation SDK**. It does not depend
on the historical `qigao/salts`, `qigao/salts-utils`, CMeta, CFlow, or CSTL
packages. Development is tracked by [migration epic #1](https://github.com/qigao/salts-core/issues/1).

The original M0 bootstrap implements an **owned-by-caller, non-owning counted byte view**
in `Salts::Core`. The stacked M1/M2 migration candidate adds
`Salts::Platform` (clock, OS CSPRNG, thread/synchronization) and
`Salts::Concurrency` (SPSC, bounded deadline queue, RCU, Disruptor,
ThreadPool and Leader/Followers). The next stacked candidate adds
`Salts::Coroutine` (the original minicoro-backed lifecycle, pool and
bounded sharded executor). These are imported from exact Salts sources and
have no CMeta/CFlow dependencies. The next Core foundation slice extracts
`mem_buffer_t`, `mem_slice_t` and the bounded slab allocator and the original single-owner bipartite `ring_buffer`
from old `salts/utils`, with only the independent Platform synchronization API. A further stacked candidate moves the
original NativeIO backend and Sharded/IPC ownership to
`Salts::NativeIO` (IOCP, epoll, io_uring, kqueue); the ACE completion-token
adapter remains in Salts and is not installed by Core. They require exact-head Linux/Windows/macOS
CI and installed C11/C++17 qualification before this candidate is accepted. It is not yet a replacement for any
published `Salts::Core` binary, and no old APIs or CMake aliases have been
cut over.

## Source layout and original module ownership

The repository root has **no** catch-all `include/` or `src/`. Each
existing module owns its own source and header tree:

- The explicitly retained `core/include`, `core/src` hold the Core byte and
  pool foundation and the **only** native `base64_utils.h` / `tn_base64_*`
  implementation. `fs/include, fs/src` and `process/include, process/src`
  retain their selected module names.
- `platform/`, `concurrency/`, `coroutine/`, `native-io/`, `cnet/`
  and `uri/` keep their existing names and private/public layout.
  The generated URI grammar remains under `uri/parser/`, and SCTP retains
  its independent `sctp/` subtree.
- Already-owned `unicode/`, `idna/`, `cserde/` and `tinytest/`
  keep their own layouts. Migration does not create source-path shims,
  duplicate providers or old module aliases.

The **installed** C11/C++17 SDK maintains documented include spellings
(`<base64_utils.h>`, `<salts/random.h>`, `<cnet/cnet.h>`) and
`Salts::*` target identities; flat installed includes do **not** imply
flat root source ownership. `tools/check_module_layout.py` gates this
invariant on Linux, Windows and macOS.

## Planned modules

| Mandatory | Optional on supported platforms |
|---|---|
| Core byte views, Platform, Concurrency, Coroutine, NativeIO | URI (re2c build-time), CNet, Crypto, FS, Process, Unicode, IDNA, Serial, USB, Capture, Playback |

Unicode stays pinned to Unicode 17.0.0 with re2c as a **build-time** tool only;
IDNA depends only on the new Unicode module. FS and Process are native system
components; they **must not require** CFlow IO Actors or CMeta Reflection.
Advanced CMeta/CFlow/CSTL/Component/ACE functionality remains in `qigao/salts`
and consumes the new Core, never the other way round. The plain CNet transport
implementation will migrate to optional `Salts::CNet`; CMeta/ACE-specific
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

The standalone install exports `Salts::Core` through
`find_package(SaltsCore CONFIG REQUIRED)`. Optional targets will be exported
only when their independent implementations and platform gates are qualified.

## Native LLVM thread and scheduler contract

The core-contract workflow uses the shared vcpkg-cache compiler setup on
Linux/macOS x64 and ARM64 (runner-native Clang), and Windows x64 and ARM64
(llvm-mingw/UCRT 20261020). It builds the original Core graph with TinyTest,
runs its native tests, and installs the package. Existing public C11/C++17
assertions run inside that graph; the workflow no longer builds a second
installed-consumer project or runs compiler/source preflight probes.

Platform public declarations explicitly export/import their one shared DLL
owner, including native thread, clock, random, readiness and fast-key APIs.
The coroutine scheduler uses the same compiler-selected thread-local storage
contract as Platform/Executor. The existing coroutine test overlaps two live
OS threads and verifies that each sees only its own scheduler, then restores
an empty current-scheduler state on thread exit.

This bounded compiler-ABI repair does not qualify CNet/TLS, merge the migration
candidate, resolve original source/API inventories, or publish a Core SDK.
