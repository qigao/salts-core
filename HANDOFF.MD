# Handoff

## 2026-11-01 Windows LLVM release identity

- CMake's MinGW platform defaults add a `lib` prefix to Windows DLLs. Keep
  `CMAKE_SHARED_LIBRARY_PREFIX` empty on Windows so MSVC and llvm-mingw publish
  the same versioned `saltscore_<module>-0.1.dll` ABI identity. Do not loosen
  the installed-owner auditor to accept compiler-dependent DLL names.

## 2026-11-01 CNet numeric-host endpoint

- A c-ares numeric-host result cannot be trusted to preserve the service port
  byte order on every platform. `cnet_resolver_submit()` retains the caller's
  host-order port and the callback writes `htons(requested_port)` into the
  validated IPv4/IPv6 sockaddr before transport admission. Do not add retry,
  provider fallback, or timeout relaxation for this failure.
- The repository currently has no tracked root `CMakeUserPresets.json`; under
  the Salts build contract this blocks local configure/build/test until the CI
  convergence change restores versioned root presets.
