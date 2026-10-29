# Whole-tree Salts / Salts-utils migration completeness admission

## Immutable source identities

- `qigao/salts@386f81328c656f416d61a6d2ef662f05443e1b4a`, tree `e0c0881d202a8c9620fd7d3d2d4611a03a8aa8df`: **1,554 exact original Git blobs**.
- `qigao/salts-utils@5b0be65de673261fc37b29485b0364ca38cc6cd6`, tree `dfd9db4622f137595a4ca5b7b5abcb60eca95894`: **1,421 exact original Git blobs**.
- Destination comparison baseline: `salts-core@004905642dce8861e39907368e53fa35b37c3cc3` (original Core parity [Draft PR #72](https://github.com/qigao/salts-core/pull/72)), tree `925b15b3f8f88de08164fbaebece42787b2fc622`.

The complete per-file path, byte count, original Git Blob SHA, owner assignment and audit status are frozen in `tools/migration/original-salts-files.json` and `tools/migration/original-salts-utils-files.json`. These inventories are **raw historical snapshots**: they do not get overwritten when a source is subsequently ported. A separate source/function/test evidence record is required to close each item. Git Blob identity is checked byte-for-byte by `tools/check_full_source_migration.py`; deleting, shrinking or rewriting the ledger fails the source gate.

## What the snapshot actually shows

| Source | Core native candidate | Split native / upper-layer bridge | Retained upper repository / support | Directly absent/changed from candidate snapshot | Present but NOT yet behavior-qualified |
| --- | ---: | ---: | ---: | ---: | ---: |
| Salts | 759 | — | 795 | 379 absent + 77 changed | 303 identical blobs |
| Salts-utils | 126 | 39 | 1,256 | 71 absent + 7 changed + 39 split | 48 identical blobs |
| **Total** | **885** | **39** | **2,051** | **573 missing/changed cases** | **351 present-but-unqualified** |

**Presence is not API parity.** A changed file with the same name requires explicit semantic review. A byte-identical source file still requires CMake build ownership, public header, installed C11/C++17 consumer tests, lifecycle/error semantics, sanitizer and relevant platform evidence. No missing cases are silently waived. The future `--require-complete` gate remains intentionally **RED** until **all 924 native candidate/split cases** have real acceptance evidence.

## Separate public-function/source ABI ledger

The new `tools/migration/original-public-functions.json` additionally freezes **54 original headers** and **610 distinct explicitly declared public functions**:

- Salts `utils/include/` + `simd/include/`: **36 headers, 476 functions**.
- Salts-utils native Capture/Playback/Serial and CFlow FS/Process/USB headers: **18 headers, 134 functions**, including 30 declared Serial APIs, 26 Capture main-header APIs, 17 Playback APIs, and real CFlow USB/FS native functions.
- Nine nonfunction CMeta metadata declarations (`extern const` data/type descriptors) in `utils/include/cmeta_cmeta_data.h` remain explicitly **unparsed / awaiting upper-layer ownership review**. Macros and inline APIs still require separate manual inventory.
- Each API points to its original header Git Blob and source commit, with `requires-real-implementation-and-behavior-test` status. A matching function name/header alone **does not** close implementation, linkage, lifetime, failure, or testing obligations.
- This ledger is immutable Git Blob `19db69a508c6049afcfe6922e19448f5f9c5d875`; the same `tools/check_full_source_migration.py` checks all 54 original header/source associations and 610 names, catches silent function deletion/renaming, and keeps `--require-complete` **RED** while the function-level claims remain unqualified. The full `CNet/Platform/NativeIO` public API inventory is additional scope and is not falsely represented by these 610 initial functions.

## Current iteration scope

The SIMD source recovery adds a separate
`tools/migration/original-simd-inventory.json` supplement for all **7 original
SIMD files and 32 public execution functions**. The frozen 610-function
inventory lists `simd/include/salts/simd.h` but extracted zero functions from
its unannotated `bool`/`void` declarations. That gap is recorded explicitly;
the original inventories are not rewritten and the 610 count is not treated
as complete SIMD coverage. Source parity, installed/native execution,
CMeta reflection mapping and downstream descriptor ABI admission remain
separate obligations; see `simd/README.md`.

At the user's direction, `Salts::SIMD` is a static module with plain C
declarations and no DLL import/export annotations. Static linkage does not
qualify any frozen source or function record, change numeric behavior, or
close the pending upper-layer reflection and descriptor mapping acceptance.

AC and Levenshtein development and acceptance are deferred by the user. The
working plan lists `ac_automaton` and `levenshtein_automaton` under
`pending_groups.deferred_by_user`, which remains part of the pending total.
The frozen ledgers retain both original header/source pairs and their shared
`utils/tests/test_string_automata.c`: **5 unresolved files**. The original
**16 AC and 10 Levenshtein functions** retain
`requires-real-implementation-and-behavior-test` status. None is marked
migrated, verified or waived, and the **2,975-file / 610-function** baseline
is unchanged. Both `audit_original_utils_parity.py --full` and
`check_full_source_migration.py --require-complete` continue to reject
incomplete migration.

This scope change leaves existing byte and UTF-8 behavior, regex error
handling and resource ownership unchanged. [PR #72](https://github.com/qigao/salts-core/pull/72)
and [PR #94](https://github.com/qigao/salts-core/pull/94) remain **Draft and
must not be merged**.

## Layer ownership contract

- `salts-core`: plain C11 Core, mmap, strings, logging, memory pools, regex, byte scanners; native Platform, Concurrency, NativeIO, SCTP/CNet, Crypto, ASN1, FS, Process, Capture, Playback, Serial, low-level FS watch and native USB transport. Preserve original public behavior, independent lifetime and error paths.
- `salts`: CMeta, CFlow, CSTL, ACE and only genuinely runtime/reflection-specific integration; upper layers may depend on Core, never the reverse.
- `salts-utils` upstream: original parser, JSON/XML/YAML/CSV, Jinja, Mustache, DataBind, bindings and higher-level services remain owned until a separately specified migration; **retained upstream is not deletion**.
- `cflow-fs`, `cflow-process`, `cflow-usb`: split native source/polling/USB/FS owner (Core) from typed reactive publisher/retainer/actor adapters (upper layer). Splitting MUST NOT delete the underlying bounded event, error or teardown semantics.
- `salts_serial`: original port discovery, POSIX termios, Windows COM/SetupAPI, async and SPSC transfer must be reproduced, independent of CMeta type traits and CSTL Vec, with real owner lifetime tests. Preserve all original public functions, not just smoke tests.
- `capture` and `playback`: original audio/video/screen platform behavior, miniaudio and Win/macOS/iOS/Android backends must have explicit owner mapping. No file is lost because a dependency is being removed.

## Restored original native mmap (first candidate)

`core/include/cmeta_mmap.h` and `core/src/cmeta_mmap.c` are derived mechanically from original `utils/include/cmeta_mmap.h` Git Blob `ba7aecd588492814ebca196d66023dfc9f24801c` and original `utils/src/cmeta_mmap.c` Git Blob `04e87de7bc565d77261e2de66c84380b85a0ac15`. The only production integration changes are replacing the obsolete umbrella `platform.h` include with the independent C11 linkage contract and platform-native Win32 includes, plus linking the source in existing `Salts::Core` (never a new DSO/legacy provider).

All **18 explicit** original mmap public entrypoints are present in the C11 header and implementation. A native real-file CTest verifies read/write/mapped range, dirty flush, page accounting, owner lifetime and multi-file POSIX group. Windows single-file API and independent C11/C++17 installed consumer ABI are required on every target platform. The historical Windows multi-view group-map reservation still requires a separately explicit real Windows conformance test; no missing Windows proof can be described as complete.

## Final gates

- Structural: run `python3 tools/check_full_source_migration.py`, plus `python3 -m unittest discover -s tests -p test_full_source_migration.py`. These **accept a truthful incomplete ledger**, and reject tampering, loss of source file/function evidence or unlinked source implementations.
- Functional: run `python3 tools/check_full_source_migration.py --require-complete`, which remains RED until missing/changed and unqualified cases are resolved with real installed/native tests. This is intentionally not substituted for the structural gate.
- No second provider, no CMeta/CFlow direct Core links, no symbol-only compatibility shim, no test-only owner offered as production.
- Use the canonical Stun-derived verified ccache and pinned re2c/vcpkg-cache from [Draft CI #90](https://github.com/qigao/salts-core/pull/90) when its independent prerequisite [FIFO #88](https://github.com/qigao/salts-core/pull/88) is merged. No per-file CI trigger bursts.

Tracking: [Issue #71](https://github.com/qigao/salts-core/issues/71) (original 175 utils files / 465 explicit public prototypes), [Issue #89](https://github.com/qigao/salts-core/issues/89) (other Salts modules and old tests), and recovery [Draft PR #72](https://github.com/qigao/salts-core/pull/72).
