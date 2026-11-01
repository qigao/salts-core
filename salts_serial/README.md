# Native Serial owner

`Salts::Serial` provides the original 30-function Serial C ABI through
`<salts_serial.h>`. It owns port handles, metadata snapshots, event sets and the
per-handle receive/transmit queues. The implementation uses the POSIX or Windows
backend selected at build time, plus the native Core, Platform and Concurrency
owners. It does not require CMeta, CSTL, CFlow, TinyMock or libserialport.

## Build and consume

From the SaltsCore repository root:

```sh
cmake -S . -B build-serial \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DSALTSCORE_BUILD_SERIAL=ON \
  -DSALTSCORE_BUILD_TINYTEST=ON \
  -DSALTSCORE_BUILD_SERIAL_EXAMPLES=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/serial-sdk"
cmake --build build-serial --config Release
ctest --test-dir build-serial -C Release --output-on-failure \
  --no-tests=error -R '^saltscore[.]serial_'
cmake --install build-serial --config Release
```

The examples option builds all six original example programs. Source
conformance requires TinyTest; an enabled test build with TinyTest disabled
fails configuration. A production build can use `BUILD_TESTING=OFF`.

An installed application needs only the public target:

```cmake
find_package(SaltsCore 0.1 CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE Salts::Serial)
```

`Salts::Serial` is a shared owner. Its native dependencies remain private.
Windows additionally uses `setupapi` and `cfgmgr32`; Unix uses the native
Platform thread implementation. The public header contains only its own types
and standard C declarations, and retains its original C++ linkage and export
macro. Do not define `SALTS_SERIAL_EXPORTS` in an application.

The independent installed-consumer project can be configured with
`cmake -S salts_serial/test/installed -B build-serial-installed
-DCMAKE_PREFIX_PATH=<installed-sdk>`. It compiles both C11 and C++17 clients,
checks all 30 signatures and actually calls every symbol. The shared libraries
must be available to the loader: use the SDK's `bin` directory in `PATH` on
Windows, or its `lib` directory in the appropriate Unix loader path. A package
without `Salts::Serial` is a configuration failure.

## Ownership and byte behavior

Create a handle with `salts_serial_create`, open/configure it with
`salts_serial_open`, and release it with `salts_serial_destroy`. Destruction
stops the async pump, closes the port and releases the handle-owned queues.
`salts_serial_close` permits a later reopen and is idempotent while closed.

The receive and transmit paths carry bytes, including NUL and bytes that are
not valid UTF-8. A configured power-of-two queue size reserves one sentinel
slot, so size 8 has capacity 7. Successful buffered operations may accept only
part of a request; callers must inspect the returned byte count. An empty read
or full write reports `SALTS_SERIAL_WOULD_BLOCK` with a zero count.

Async operation has one pump thread, one application receive consumer and one
application transmit producer. Applications must synchronize additional
readers or writers. Blocking port I/O cannot run while the pump is active.
The adapter helpers `salts_serial_buffer_rx` and
`salts_serial_drain_tx_buffer` also require a stopped pump.

Port metadata belongs to its `salts_serial_port_list_t` snapshot. All six
optional strings are owned copies; `NULL` metadata remains distinct from an
allocated empty string. Views expire when the snapshot is destroyed. Windows
keeps the original UTF-16/UTF-8 conversion path; serial payloads do not pass
through that conversion.

An event set borrows opened port handles. Destroy the event set before closing
or destroying those handles. Event waits report the first ready port's mask;
a timeout returns `SALTS_SERIAL_WOULD_BLOCK`. The original POSIX blocking-read
timeout instead returns `SALTS_SERIAL_OK` with a zero byte count.

## Validation scope

The original source and test inventory, the private storage adaptation, the
explicit Windows initialization fix and local evidence are recorded in
[MIGRATION.md](MIGRATION.md). The dedicated workflow covers Linux Release,
ASan/UBSan/leak checks, TSan, Windows and macOS, but a workflow definition is not
a completed run.

The native POSIX test uses a private pseudo-terminal pair. It exercises real
backend I/O, readiness, timeout, async startup/teardown, byte preservation and
peer-close errors without opening a physical serial device. Physical UART/USB
serial configuration, flow control and disconnect behavior require hardware
fixtures and remain pending. This owner does not close the repository's frozen
full-migration gate.
