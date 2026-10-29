# SaltsCore native CI tool provenance

Scope: the original 13 GitHub Actions workflows at the `ci/fifo-master-runner-serialization` admission epoch. New TLS research workflows must adopt the same contract on rebase; no test may be removed to obtain green results.

## Native compiler cache

`.github/actions/setup-ccache/action.yml` is adapted from the reviewed [qigao/stun setup-ccache action](https://github.com/qigao/stun/blob/811a93eca9858971d54e52245e27bb18b053c4bf/.github/actions/setup-ccache/action.yml), original source blob `e0ae6b8439ebdb9715ae2f84bd1e9c2f3e577434`. It downloads the official **ccache 4.14.2** release, validates each SHA-256 digest, and uses a per-job/per-matrix/per-OS cache key. SaltsCore extends the verified archive switch to Linux ARM64, macOS x64 (official universal Darwin archive), and Windows ARM64, without an unverified download fallback.

The action exports `CMAKE_C_COMPILER_LAUNCHER=ccache`, `CMAKE_CXX_COMPILER_LAUNCHER=ccache`, and `CMAKE_GENERATOR=Ninja`. Windows CMake builds explicitly activate native MSVC before Ninja configuration, so a compiler launcher is actually used; a Visual Studio CMake generator does **not** qualify as an effective ccache build. Direct Clang commands in the portable ABI workflow explicitly invoke `ccache clang`. TinyCC is a separately validated compiler and cannot be passed through ccache as if it were GCC/Clang. The Windows ARM64 runner only **runs** previously built binary artifacts and therefore does not need a compiler cache.

Every ccache-enabled job emits `ccache --show-stats`; a first/cold run can have zero hits without constituting a failure. Cross-commit warm-hit claims require actual compilation stats and cannot be inferred from an action succeeding.

## Host re2c and native vcpkg packages

The single pinned source of re2c and canonical vcpkg ABI packages is `qigao/vcpkg-cache` commit `7bb5b556a001bc82d2922a04a718e986a7f47f54`. Workflows that need re2c use its `setup-re2c-tools` composite action and a `packages: read` permission. No apt/homebrew re2c, Windows source rebuild, or alternate tool is used.

CNet, the CNet sanitizer, and the original M5 NativeIO/required-mTLS jobs use `setup-vcpkg-curl-cache` with `mode: read`, exact target+host triplets and `features: cnet`. It downloads GitHub Packages NuGet V3 native ABI ZIPs using authenticated **curl**, verifies package identity and ABI, and restores local vcpkg binary caches. It does not execute Mono, `nuget.exe`, `dotnet restore`, or an external NuGet provider. Missing/corrupt immutable ABI packages **fail closed**; there is no downloaded-source substitution for the normal CNet feature graph.

### Explicit research exception still requiring architectural retirement

The *historical* `cnet-m5-python-mtls.yml` test also imports `qigao/crypto` donor revision `0b4ee094ce8002e6169ce231028107eba63588c6` and privately patches original **GmSSL #14** port source from `qigao/vcpkg-cache` commit `65c12e71d02b9b7dacfafc3d188a3bda18184296`. Its separate donor `vcpkg install --overlay-ports=...` compiles an audit-specific GmSSL archive. This is **not** an immutable cached ABI package, and CI must not describe it as a cache hit or production sole-Crypto acceptance. Keeping this exact behavior avoids silently dropping the existing real mTLS test. Retire it only when the imported donor/overlay is genuinely replaced by the module-local single Crypto provider with equivalent Python TLS1.2/1.3, TLCP, SAN/CA and NativeIO owner tests. Never add an OpenSSL/GmSSL fallback or silently reuse a different port ABI.

## CI admission

`tools/check_ci_native_cache.py` and `tests/test_ci_native_cache.py` are mandatory in the original Core SDK gate. They reject missing tool actions, unpinned package sources, ccache omission, MSVC Visual Studio generator bypass, Mono/NuGet bootstrap, and ordinary CI consumer binary-cache writes. Every existing workflow, original OS matrix, test command and FIFO gate remains mandatory. Source changes should be committed in one atomic tree update to avoid flooding Actions runners.
