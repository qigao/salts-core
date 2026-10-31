# Native USB

`Salts::USB` owns the original bounded libusb context, device, transfer and
hotplug state machine. It is an optional shared library in SaltsCore, with
one private libusb provider and the existing native `Salts::Platform` thread
owner. Enable it explicitly:

```sh
cmake -S . -B build-usb \
  -DSALTSCORE_BUILD_USB=ON \
  -DSALTSCORE_BUILD_TINYTEST=ON \
  -DBUILD_TESTING=ON \
  -DVCPKG_MANIFEST_FEATURES=usb
cmake --build build-usb --config Release --parallel 2
ctest --test-dir build-usb -C Release --output-on-failure --no-tests=error \
  -R '^saltscore[.]usb_'
cmake --install build-usb --config Release
```

Use the repository's pinned vcpkg toolchain for this profile. The `usb`
manifest feature provides libusb and host pkgconf. An existing system libusb
development package can also be discovered with pkg-config. An explicit
shared provider can be selected by setting both
`SALTSCORE_USB_LIBUSB_INCLUDE_DIR` and `SALTSCORE_USB_LIBUSB_LIBRARY`.
Missing provider inputs are configuration errors. The SDK does not vendor or
redistribute an arbitrary system libusb runtime.

The installed consumer uses `find_package(SaltsCore 0.1 CONFIG REQUIRED)`,
links only `Salts::USB`, and includes the unchanged public header
`<cflow/usb.h>`. Its thirteen `cflow_usb_*` function names remain the original
C ABI. This historical header name does not require a CFlow runtime.
libusb headers and native handles stay private. There is no second USB
target alias or implementation.

## Ownership and errors

The context owns one event thread, fixed transfer slots, bounded control
storage, and a bounded hotplug queue. User callbacks run only when one caller
drives `cflow_usb_run_ready()`. Control-plane calls are externally serialized;
destroy cannot race another call or callback.

Bulk and interrupt buffers remain borrowed through their terminal callback.
Control OUT data is copied on admission; successful control IN data is copied
back before callback delivery. A rejected submit accepts no buffer and clears
the output transfer ID. Cancellation requests are asynchronous; the terminal
callback releases the borrow.

Enumeration capacity overflow returns `SALTS_ENOBUFS`, reports the required
count, and commits no partial snapshot. A bus/address/VID/PID tuple is an
observed identity. Reconnection requires a fresh enumeration and explicit
open. A lost hotplug detail still marks the affected handle lost and requests
cancellation.

Hotplug queue loss produces `CFLOW_USB_HOTPLUG_RESCAN_REQUIRED`. If another
event is suppressed after delivery of that marker, acknowledgement queues a
new marker. The caller repeats enumeration and acknowledgement until the
marker queue stays empty.

Before destroying a context, drain terminal callbacks and hotplug events,
release interfaces and close devices. Busy destroy leaves the owner live.
A thread-join failure also retains the owner for retry. All original mapped
error results and resource transitions are preserved in the unchanged
production source.

## Validation boundaries

The three original test sources and their fake libusb provider are retained
byte for byte. The real-provider test initializes, enumerates and destroys
a context without opening a physical device. Deterministic state tests
execute the same production source against the original fake, including
cancellation, control IN copying and rescan races. Added lifecycle checks
cover busy ownership, callback reentry, slot reuse, lost-device cancellation
during overflow, and native-submit rejection rollback.

Independent C11 and C++17 clients under `tests/installed` use only the
installed SDK, verify all thirteen signatures, actually call every public
function, and create/destroy real contexts. Neither client includes libusb
headers or a source-tree header.

These tests do not qualify physical-device transfers or real hotplug.
Hardware fixtures, Windows/macOS execution, mobile support, and other
unexecuted profiles remain pending until their actual results exist. See
[MIGRATION.md](MIGRATION.md) for the frozen-source mapping and local evidence.
Isochronous transfers and automatic kernel-driver detach remain outside the
original public API.
