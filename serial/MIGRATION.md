# Serial source migration and qualification

## Frozen source and native owner

The donor is [`qigao/salts-utils` at
`5b0be65de673261fc37b29485b0364ca38cc6cd6`](https://github.com/qigao/salts-utils/tree/5b0be65de673261fc37b29485b0364ca38cc6cd6/salts_serial).
All 20 original `salts_serial/` paths are present in the native owner under the
same relative names. The original public header is byte-identical and retains
all 30 explicitly declared functions. The frozen repository inventories have
not been changed.

| Original paths below `salts_serial/` | Treatment |
| --- | --- |
| `serial.h` | Exact original public ABI |
| `salts_serial.c` | Remove unused umbrella/CSTL includes and unwrap the private status conversion |
| `backend_posix.c` | Replace the private vector status type and constants with their existing Serial results |
| `backend_win32.c` | Same status adaptation; explicitly zero-initialize the temporary metadata vector |
| `serial_internal.h` | Preserve storage ownership operations; replace the reflected private vector with one typed owned array |
| `CMakeLists.txt`, `examples/CMakeLists.txt`, `test/CMakeLists.txt` | Native owner, install, dependency and test registration |
| `test/test_salts_serial_mock.c` | Retain all original scenarios and assertions through the existing backend/thread injection hooks |
| `test/serial_test.h`, `test/test_salts_serial.c`, `test/test_salts_serial_public_abi.c`, `test/test_fixes.c` | Exact original bytes |
| `examples/await_events.c`, `examples/handle_errors.c`, `examples/list_ports.c`, `examples/port_config.c`, `examples/port_info.c`, `examples/send_receive.c`, `examples/README` | Exact original bytes |

The eight adapted inputs have exact original copies under
`original/<relative-path>.original`. These records are never compiled.
`test/test_serial_provenance.py` checks each of the 20 original blobs and sizes
against the frozen manifest. It also constrains all three production `.c`
files to the named mechanical changes, checks the 30 declarations and
definitions, verifies the unchanged string ownership helpers, and rejects
source drift or removal of an original test. Source presence does not count as
completed migration evidence.

`Salts::Serial` is the sole native Serial owner and is installed as a shared
library. It links privately to `Salts::Core`, `Salts::Platform` and
`Salts::Concurrency`, plus Windows system libraries. Its public header does not
expose any private storage, native OS handle or dependency type. No generic
CSTL compatibility layer, reflection provider or TinyMock runtime is added.

## Storage adaptation and explicit correction

The old generic vector existed only to store the private
`salts_serial_port_info_storage_t`. The replacement is one module-private typed
array; it keeps the original append transaction and growth policy:

1. Allocate and deep-copy the prepared element before changing the array.
2. Allocate growth storage, starting at 8 entries and then doubling, with the
   original representable-count bound.
3. Move the existing owned entries and the prepared entry only after all
   fallible allocations/copies have succeeded.
4. Publish the new size after ownership has transferred.

The maximum count is `SIZE_MAX / sizeof(salts_serial_port_info_storage_t)`.
The complete C object size already includes its alignment padding, so this is
the old rounded stride for this concrete descriptor. There is no configured
port-count limit. Preparation, string-copy or growth failure returns the
original `SALTS_SERIAL_NO_MEMORY` result and preserves the previous vector
contents, count, capacity and allocation.

The six-string refresh, clone, copy, move and destroy operations remain
byte-identical. Partial copies destroy all successfully cloned strings.
Successful copies own distinct strings, moves clear the source, and repeated
destruction is safe. Optional `NULL` fields, allocated empty strings, UTF-8
bytes and scalar metadata keep their original meanings.

The Windows `win32_get_port_info` temporary vector is now declared with
`= {0}`. The previous initializer read the vector's initialized state even
though this stack variable was uninitialized. This is an explicit defect
correction required for defined initialization, not a claim of byte-for-byte
behavior equivalence. The POSIX and Windows I/O algorithms, error mapping,
UTF conversion, event implementation and cleanup paths otherwise retain their
original source under the constrained mechanical adaptation.

## Original test obligations

The three public TinyTest sources execute against the production shared owner:
five original ABI/buffering cases, three public type/metadata/event cases, and
the four previously unregistered review-fix cases. All six original examples
are build inputs when `SALTSCORE_BUILD_SERIAL_EXAMPLES=ON`.

The injected backend suite compiles the same production and platform source
with its original `SALTS_SERIAL_TESTING` hooks. The six portable scenarios are
retained: RX pumping, TX draining, a worker that runs before thread creation
returns, thread-creation rollback and retry, all-string copy/move/destroy
ownership, and temporary-copy rollback. The seventh original Windows-only
UTF-8 metadata scenario remains enabled on Windows. Function-call counts and
first-handle identity are recorded directly in test-private probes instead of
using reflected TinyMock descriptors. No original scenario is skipped or
replaced by a successful stub.

Added storage cases fail each of the six string clones, prepared allocation
and growth allocation, then validate successful growth and ownership after
freeing the source. They also cover `NULL` metadata and count overflow before
allocation. These tests exercise the same private transaction used in
production; leak detection checks partial-allocation cleanup.

The added POSIX test opens only a private PTY pair. It validates synchronous
and asynchronous transfers of NUL/non-UTF-8 bytes, queue capacity, original
timeout outcomes, invalid-state output counts while pumping, stop/join,
peer-close error readiness, and resource teardown. The fixture explicitly
sets its initial line discipline to raw mode; it does not invent a contract
that the original configure function resets every inherited termios flag.

The installed project uses only `<serial.h>`, standard headers and
`Salts::Serial`. C11 and C++17 consumers check all 30 function signatures and
actually call every symbol. They exercise metadata snapshot ownership,
buffer wraparound and backpressure, exact bytes, rejected-call output values,
and event/handle lifetime without a source-tree include path.

## Local evidence

The 2026-10-29 local validation used GNU C/C++ 13.3.0 and the candidate worktree
based on `6c8b5b91fda1adc09038964472cbeab2520b201e` with uncommitted migration
changes. These are working-tree results, not an exact-commit release
qualification or a Windows/macOS execution claim.

| Validation | Observed result |
| --- | --- |
| Complete configured Release graph, including all six original examples | Built successfully |
| `saltscore.serial_original_c11` | Passed all five original cases |
| `saltscore.serial_public_abi` | Passed all three original cases |
| `saltscore.serial_original_fixes` | Passed all four original cases |
| `saltscore.serial_backend_contract` | Passed all six portable original scenarios |
| `saltscore.serial_storage_ownership` | Passed all three added failure/ownership cases |
| `saltscore.serial_posix_pty` | Passed through the real POSIX backend |
| `saltscore.serial_source_provenance` | Passed all five source/negative checks |
| Independent `sdk.serial_c11`, `sdk.serial_cpp17` | Both passed; all 30 symbols called |
| ASan + UBSan + leak detection | All seven owner/source CTests and both installed consumers passed |
| TSan | All seven owner/source CTests and both installed consumers passed |
| Missing installed owner / original TinyTest dependency | Both configurations rejected at the intended guard |
| ELF symbol inspection | Exactly 30 original public Serial function exports |

The first leak-check run in the restricted sandbox failed because LSan could
not inspect `/proc` there. The unchanged tests passed with normal local OS
access and `detect_leaks=1`. No sanitizer check was disabled, no failure was
suppressed, and no production behavior was changed for a sanitizer profile.
TSan also covered the real async pump and its test injection cases.

## Observed native CI and Darwin compile correction

The [Serial workflow run at
`e48700d8e6219c2eed9dcc3d2ebb85b79465da20`](https://github.com/qigao/salts-core/actions/runs/40829937071)
passed its Linux Release, ASan/UBSan, TSan and Windows jobs, including their
installed consumers. Its macOS job failed while compiling `backend_posix.c`:
`B57600`, `B115200` and `B230400` were hidden by the private strict-POSIX feature
selection. The glibc `_DEFAULT_SOURCE` macro does not enable Darwin extensions.
Apple's [published termios header](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/sys/termios.h)
places those constants behind the `_POSIX_C_SOURCE` / `_DARWIN_C_SOURCE` gate.

The production target and the test-only target compiling the same backend now
also define `_DARWIN_C_SOURCE=1` privately on Apple platforms. No termios value
is redefined, no baud case or fallback is removed, and no production C source
or public header changes. A local compile probe using Apple's unmodified speed
definitions reproduced all three missing constants with the old flags and
passed with the Darwin feature macro. This probe is a feature-selection
regression, not a macOS backend execution. The corrected macOS graph still
requires its native CI run at the replacement candidate revision.

## Remaining admission and retained limits

`.github/workflows/serial-native.yml` defines exact-revision Linux Release,
ASan/UBSan/leak-check, TSan, Windows and macOS jobs, including installation and
independent consumers. The historical results above do not qualify a later
candidate revision. Its qualification record leaves physical-device coverage
and global migration completion pending.

Physical UART/USB serial data, actual baud/parity/flow-control behavior,
hardware disconnect and reattachment, the corrected macOS graph and mobile
profiles need their own real environment evidence. A PTY or injected backend
does not qualify hardware.

The migration preserves existing backend limitations. POSIX configuration
does not normalize every input flag and does not support every parity preset;
its baud fallback remains unchanged. A metadata lookup by name can create a
snapshot without proving that the named port exists. The original initial
metadata-allocation checks and `stop_async` join-result handling have not been
strengthened; the added private-vector rollback checks do not claim broader
failure guarantees for those paths.

The frozen 2,975-file and 610-public-function final gate remains strict. This
module does not accept another pending owner, remove deferred AC/Levenshtein
work, or change the decision to keep the existing Draft pull requests unmerged.
