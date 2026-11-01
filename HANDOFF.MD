# Handoff

## 2026-11-01 Windows LLVM release identity

- CMake's MinGW platform defaults add a `lib` prefix to Windows DLLs. Keep
  `CMAKE_SHARED_LIBRARY_PREFIX` empty on Windows so MSVC and llvm-mingw publish
  the same versioned `saltscore_<module>-0.1.dll` ABI identity. Do not loosen
  the installed-owner auditor to accept compiler-dependent DLL names.

## 2026-11-01 LLVM preset and CI convergence

- `vcpkg-cache` compiler actions reject protected Release setup when
  `build-type: Release` is combined with `obfuscator: false`. SaltsCore does
  not enable obfuscation, so CI requests `build-type: toolchain-only`; the
  checked-in CMake preset independently selects the Release configuration.
- The outer CMake toolchain is the vcpkg manifest toolchain. Native LLVM or
  the Android NDK is supplied only through `VCPKG_CHAINLOAD_TOOLCHAIN_FILE`.
  Reversing these two toolchains bypasses manifest-mode dependency ownership.
- CI path filters intentionally contain only source/public-header/data
  directories. CMake, workflow, test, tool, documentation and packaging files
  must not trigger product CI; use `workflow_dispatch` to qualify CI-only
  changes.
- Do not restore installed-consumer/smoke CMake projects. Configure owns the
  complete graph, build runs it without `--target`, and formal tests run only
  through CTest.
- When removing an installer/consumer source tree, remove its native-graph
  registration in the same change. A stale `add_executable` is detected only
  at CMake generation, even when configure has otherwise found all packages.
- `setup-vcpkg-curl-cache` is a binary-only admission gate and fails on the
  first absent immutable ABI package. After the package registry is cleared,
  product CI must use `setup-vcpkg-cache` in read mode so vcpkg can compile a
  cache miss from the pinned source graph without publishing from the product
  repository.

## 2026-11-01 CNet numeric-host endpoint

- A c-ares numeric-host result cannot be trusted to preserve the service port
  byte order on every platform. `cnet_resolver_submit()` retains the caller's
  host-order port and the callback writes `htons(requested_port)` into the
  validated IPv4/IPv6 sockaddr before transport admission. Do not add retry,
  provider fallback, or timeout relaxation for this failure.
- Local configure/build/test requires the tracked root
  `CMakeUserPresets.json`; do not qualify an older checkout that lacks the
  versioned root presets.
