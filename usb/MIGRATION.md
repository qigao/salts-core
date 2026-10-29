# USB source migration and qualification

## Source and owner

The donor is [`qigao/salts-utils` at
`5b0be65de673261fc37b29485b0364ca38cc6cd6`](https://github.com/qigao/salts-utils/tree/5b0be65de673261fc37b29485b0364ca38cc6cd6/cflow-usb).
Its complete `cflow-usb/` group contains 10 files and 13 explicitly declared
public functions. The frozen repository inventories remain unchanged.

The native owner is now `usb/`, exported as `Salts::USB`. The production
implementation has always used only its own plain C header, libusb, and native
Salts error/thread APIs. The old `Salts::CFlow` link was unnecessary; removing
that link does not remove a scheduler, publisher or reflected adapter from
this source group. Other CFlow/reactive groups remain separate migration debt.

The source mapping preserves the module-relative paths:

| Original path below `cflow-usb/` | Native owner path below `usb/` | Treatment |
| --- | --- | --- |
| `include/cflow/usb.h` | same | Exact original bytes; all 13 declarations |
| `src/usb.c` | same | Exact original bytes; all 13 implementations |
| `tests/cflow_usb_test.c` | same | Exact original bytes; real-provider tests |
| `tests/cflow_usb_state_test.c` | same | Exact original bytes; deterministic state tests |
| `tests/cflow_usb_header_cpp_test.cpp` | same | Exact original bytes; C++17 header test |
| `tests/fake/fake_libusb.c` | same | Exact original bytes; test-only provider |
| `tests/fake/libusb.h` | same | Exact original bytes; test-only provider header |
| `CMakeLists.txt` | same; original at `original/CMakeLists.txt.original` | Native target/dependency/install adaptation |
| `tests/CMakeLists.txt` | same; original at `original/tests/CMakeLists.txt.original` | Register all original tests and added ownership checks |
| `README.md` | same; original at `original/README.md.original` | Current owner/build instructions |

The `.original` files are provenance records, never build inputs.
`tests/test_usb_provenance.py` checks every retained original blob against the
frozen Git blob/size, verifies the original public declarations and definitions,
and demonstrates that source drift or a missing public header fails admission.
No source-availability check marks the global migration complete.

## Preserved behavior

The byte-identical production source preserves all mapped libusb errors,
hard capacity checks, exact device identity, interface claims, buffer borrowing,
control-copy timing, asynchronous cancellation, callback ordering, hotplug
overflow/rescan acknowledgement, and quiescent-only teardown. Thread-join
failure retains the context for retry. The migrated code does not add an
isochronous path, automatic kernel-driver detach, a compatibility alias, a
new CMeta/CFlow dependency, or a substitute USB implementation.

The added `usb_lifecycle_test.c` closes specific ownership gaps in the original
tests: admission failure retains the owner and zero output handle; interfaces
and accepted transfers prevent early close; control IN bytes are visible
before the callback; a callback can reuse its released slot while recursive
callback driving and destruction remain busy; lost-device cancellation still
occurs when detailed hotplug delivery overflows; native submit rejection rolls
back the reserved slot and counters and does not accept the caller buffer.
These tests execute the unchanged `src/usb.c` against the unchanged original
fake provider. They do not replace real-provider tests.

## Dependencies and installed ABI

The optional owner requires `SALTSCORE_BUILD_USB=ON`. Source tests also require
`BUILD_TESTING=ON` and `SALTSCORE_BUILD_TINYTEST=ON`; missing TinyTest is a
configuration error. An installed consumer must find `Salts::USB`; a package
without that target is rejected explicitly.

libusb stays private to the shared owner. pkg-config discovery requests its
declared private system link dependencies, which are needed when the provider
is a static archive. Explicit include/library paths select the same libusb
provider, and a manually supplied `.a` without dependency metadata is rejected.
The repository's vcpkg baseline `b1b19307e2d2ec1eefbdb7ea069de7d4bcd31f01`
selects libusb 1.0.29 port revision 1. Its Linux udev feature requires the
system libudev development package. A local system-runtime link is useful
validation evidence and is not a complete distributable dependency package.

The pinned Windows triplet selects shared libusb. Its installation now includes
the selected import library's matching DLL in the SDK `bin` directory and the
provider's `share/libusb/copyright` under
`share/licenses/SaltsCore/libusb/copyright`. The resolver takes the actual link
and include paths, requires a common canonical provider prefix, matches a
debug import library to its debug runtime, and rejects a missing DLL, missing
license, mixed header/library origins or an ambiguous selected library. It
does not switch to a static provider. Windows installed-consumer CI uses the
SDK `bin` directory without adding the provider's `bin` directory to `PATH`.
Three packaging-path tests cover release/debug selection and negative
admission with temporary path fixtures; they do not claim real Windows PE
execution. Other platforms' shared-provider packaging and clean-runtime
closure remain pending.

The independent `tests/installed` project compiles C11 and C++17 clients using
only installed SDK headers and `Salts::USB`. Both clients type-check and
actually call all 13 functions. They verify rejected-call outputs, opaque
carrier layout, unchanged C linkage, and real context teardown. No private
libusb header or source-tree include path is available to those clients.

## Local evidence and remaining admission

The 2026-10-29 local validation used the candidate worktree based on
`6c8b5b91fda1adc09038964472cbeab2520b201e`, with uncommitted migration changes,
GNU C/C++ 13.3.0 and system libusb 1.0.27 (`2:1.0.27-1`). Its matching official
header was staged outside the repository. These are working-tree results;
they are not an exact-commit release qualification or a vcpkg-package result.

| Validation | Observed local result |
| --- | --- |
| Complete configured Release build graph | Built successfully |
| `saltscore.usb_original_c11` | Passed with real libusb initialization/enumeration |
| `saltscore.usb_original_cpp17` | Passed |
| `saltscore.usb_original_state` | Passed against the original deterministic provider |
| `saltscore.usb_lifecycle` | Passed all four ownership cases |
| `saltscore.usb_source_provenance` | Passed four source/negative checks and three Windows packaging-path checks |
| Independent `sdk.usb_c11`, `sdk.usb_cpp17` | Both passed; all 13 symbols called |
| ASan + UBSan + leak detection, owner and installed consumers | All 5 owner/source CTests and both installed consumers passed |
| Missing installed USB owner / disabled original tests | Both configurations rejected as required |
| ELF exports and dependencies | Exactly 13 original USB function exports; native Platform, libusb and C runtime only |

The initial real-provider test in the restricted sandbox failed because its
udev monitor could not be created. The unchanged test passed when run with
normal local OS access. No test was skipped and no provider or code was changed
to hide that failure.

The dedicated `usb-native.yml` workflow defines Linux Release/sanitizer,
Windows and macOS execution with the pinned vcpkg provider and installed
consumers. Adding that workflow is not evidence that those jobs passed.
Physical control/bulk/interrupt transfers, physical cancellation/unplug,
real hotplug behavior, Windows/macOS execution, and mobile profiles still need
their actual environment/fixture evidence. The workflow records its exact
revision and leaves physical-device and global-completion fields pending.

The 2,975 original files and 610 explicitly public functions retain their
strict final gate. This module does not clear other migration debt or the
deferred AC/Levenshtein work. Draft pull requests remain unmerged.
