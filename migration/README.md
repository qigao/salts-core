# Salts and SaltsUtils complete migration ledger

This is provenance, NOT a declaration of functional completion. Never remove original behavior because the original implementation used CMeta, CSTL or a third-party provider.

## Exact original Git trees

- qigao/salts branch codex/finish-1181-session commit 386f81328c656f416d61a6d2ef662f05443e1b4a: **1554 nontruncated Git blobs**
- qigao/salts-utils master commit 5b0be65de673261fc37b29485b0364ca38cc6cd6: **1421 nontruncated Git blobs**
- Target qigao/salts-core master baseline 012fa1a5effc956ba764509034b3c265d9b9aa79, original tree 63aa91c5403e08dbfeeb44e9c57cdd36e95758e4

The two original file manifests preserve every original file path, byte size, Git blob SHA1, proposed owner, canonical destination and target-baseline path/blob status. Across both trees 897 files have provisional SaltsCore ownership (745 Salts, 152 SaltsUtils). There are 96 original public headers and **at least 1191 conservative C function declarations/candidates**: 589 original Salts native headers + 465 original Salts utils explicit SALTS_C_API declarations (separate #71/#72 pinned manifest) + 137 original SaltsUtils native/CFlow-split headers.

A filename or identical blob is NOT an exported ABI/behavioral parity proof. Generated macros, static inline and reflection interfaces can have additional callable operations; they are a separate mandatory review category, not implicitly zero.

## Function ownership boundary

- SaltsCore lower-level native: Core strings/tstr/tlog/fmt/regex/mmap/errors, Platform, Concurrency, Coroutine primitives, NativeIO/IPC, CNet, SCTP, URI, ASN1, sole Crypto, CSerde, Unicode/IDNA, FS, Process, TinyTest; plus SaltsUtils Capture, Playback, Serial, native USB and native filesystem watchers.
- Salts upper layer: CMeta/Reflection, CFlow typed execution/Actor/DAG, reflected CSTL, ACE, Configurator and ServiceHost. Core can invoke an upper callback defined in its own plain C11 contract but must never link directly to CMeta/CFlow.
- SaltsUtils upper applications: Parser, Jinja, Mustache, DataBind, Bindings, Cron, BehaviorTree and app-specific integrations.
- Mixed cases require explicit per-function review: cflow-fs native file watcher goes to Core FS, but the fs_watch_publisher that includes cflow/reactive.h stays above; native cflow-usb implementation must not be lost; cflow-process retainer requires an explicitly verified owner.

## High-priority original functionality still lacking qualification

- Salts utils: bounded byte regex re.c and re_scan.h, AC/Levenshtein automata, mmap, errors, tstr, tlog, fmt, memory/object pools and their original tests; existing [Issue #71](https://github.com/qigao/salts-core/issues/71), [Draft #72](https://github.com/qigao/salts-core/pull/72).
- Salts SCTP: SM3-HMAC Cookie authentication, key epochs/rotation, entropy failure and constant-time verification, plus original CNet/NativeIO/Platform/Concurrency regression suites and benchmarks; [Issue #89](https://github.com/qigao/salts-core/issues/89).
- SaltsUtils native: Capture (39 original files), Playback (11), Serial (20), CFlow-USB native (10). All currently lack complete source, installed exported function and lifecycle evidence in SaltsCore master. Missing code must be restored as real C11, not stubbed or labelled upper CFlow for convenience.

## Two independent admission gates

The fast audit:
    python3 tools/audit_dual_source_migration.py
    python3 -m unittest discover -s tests -p test_dual_source_migration.py -v

This is added to the existing SDK CI only; it verifies immutable manifests and exact original Git blob provenance with 6 positive/negative tests and never claims that migration is complete.

Strict final cutover:
    python3 tools/audit_dual_source_migration.py --full

This deliberately fails until **all** original lower-level files exist in their registered module paths and one separate native-functional-qualification.json has original SHA-bound per-file proof and per-function compiled-export, C11/C++17 installed consumer, original behavior test and exact-head CI evidence. Macro/inline/generated API families also need explicit review. No mock implementation, switched backend or missing original tests may satisfy the full gate.

Final required acceptance remains Linux native + ASan/UBSan/TSan, Windows IOCP/MSVC, macOS kqueue, C11/C++17 installed ABI and applicable Android/iOS cross builds. No unrequested merge/release, no CMeta dependency in Core, no fallback, and keep Github Actions FIFO runner budget.
