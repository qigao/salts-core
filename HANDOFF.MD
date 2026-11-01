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
