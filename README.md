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

## Default and optional modules

| Mandatory | Optional on supported platforms |
|---|---|
| Core byte views, Platform, Concurrency, FS, Coroutine, NativeIO, ASN1, Crypto, URI, CNet | Process, Unicode, IDNA, SIMD, Serial, USB, Capture, Playback |

Unicode stays pinned to Unicode 17.0.0 with re2c as a **build-time** tool only;
IDNA depends only on the new Unicode module. FS and Process are native system
components; they **must not require** CFlow IO Actors or CMeta Reflection.
Advanced CMeta/CFlow/CSTL/Component/ACE functionality remains in `qigao/salts`
and consumes the new Core, never the other way round. The plain CNet transport
implementation is part of the default Core candidate; CMeta/ACE-specific
CNet integrations stay in Salts. Enabling a module does not establish its
cross-platform or release qualification.

See [architecture](docs/ARCHITECTURE.md) for ownership and migration gates.

## Default local Crypto and CNet build (Draft)

The default graph builds the whole CNet transport source tree, including the
original IPC FIFO, peer index and native-path changes. Production CNet links
the sole source-built static `Salts::Crypto` archive. Its private native TLS
and algorithm sources are locked under `crypto/native/gmssl/`; Crypto's
Ed448 objects remain under `crypto/vendor/libecc/`. No separately installed
GmSSL archive or external Crypto prefix is consumed.

Use CMake 3.21 or newer, a C11 compiler, and a C++17 compiler when testing.
Provide the canonical pinned c-ares, KCP and SIMDe dependencies and an installed
host re2c 4.6 with the pinned Unicode 17 data. The checked-in manifest makes
these base dependencies mandatory. The `cnet` feature remains an empty
compatibility feature and does not control the production target:

```sh
python3 crypto/tools/check_local_native_source.py
cmake -S . -B build-cnet -DBUILD_TESTING=ON \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/verified-vcpkg-toolchain.cmake
cmake --build build-cnet --parallel
ctest --test-dir build-cnet --output-on-failure
```

The active workflows share `.github/actions/setup-core-native`, which restores
the exact non-crypto dependencies and re2c host tool and checks the local
Crypto source bytes. `.github/workflows/cnet-provider.yml` retains real
Linux/macOS/Windows transport, TLS, installed SDK, relocation and owner checks.
The separate `SALTSCORE_TEST_CNET_CRYPTO_QUALIFICATION` option enables the
additional M2/M5/M6 qualification graph; it defaults to `OFF` and does not
change production CNet selection or ownership.

An unavailable dependency fails configuration, without substituting another
TLS provider or an implicit transport fallback. Local source ownership does
not make the retained GmSSL-derived implementation third-party-free. Exact
current-source platform CI, TLS/CA/SAN and independent peer tests, sanitizers,
installed consumers, and physical runtime ownership still gate acceptance.
No downstream cutover or production release is implied by this candidate.

## Build and consume the candidate SDK

```sh
cmake -S . -B build -DBUILD_TESTING=ON \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/verified-vcpkg-toolchain.cmake
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /tmp/saltscore-sdk
cmake -S tests/installed -B build-installed -DCMAKE_PREFIX_PATH=/tmp/saltscore-sdk
cmake --build build-installed
ctest --test-dir build-installed --output-on-failure
python3 tools/check_dependency_boundaries.py
```

The standalone install exports the default graph through
`find_package(SaltsCore CONFIG REQUIRED)`, including the sibling ASN1 and
Crypto packages. Optional module targets are exported when configured; their
presence is not a substitute for the source/API, platform and release gates.
