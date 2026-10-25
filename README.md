# SaltsCore

**Status: bootstrap / Draft qualification. Not a production replacement yet.**

SaltsCore is an independently built **C11 foundation SDK**. It does not depend
on the historical `qigao/salts`, `qigao/salts-utils`, CMeta, CFlow, or CSTL
packages. Development is tracked by [migration epic #1](https://github.com/qigao/salts-core/issues/1).

The M0 bootstrap currently implements only an **owned-by-caller, non-owning
counted byte view** in `SaltsCore::Core`. It tests C11 and C++17 installed
consumers on Linux, Windows and macOS. It is not yet a replacement for any
published `Salts::Core` binary, and no old APIs or CMake aliases have been
cut over.

## Planned modules

| Mandatory | Optional on supported platforms |
|---|---|
| Base / Core, Platform, error, thread, byte view | Crypto, FS, Process, Unicode, IDNA, Serial, USB, Capture, Playback |

Unicode stays pinned to Unicode 17.0.0 with re2c as a **build-time** tool only;
IDNA depends only on the new Unicode module. FS and Process are native system
components; they **must not require** CFlow IO Actors or CMeta Reflection.
Advanced CMeta/CFlow/CSTL/Component/CNet functionality remains in `qigao/salts`
and consumes the new Core, never the other way round.

See [architecture](docs/ARCHITECTURE.md) for ownership and migration gates.

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
