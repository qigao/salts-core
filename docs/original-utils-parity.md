# Original Salts utils/Core migration parity — work in progress

**Blocking issue:** [#71](https://github.com/qigao/salts-core/issues/71).
**Historical baseline:** `qigao/salts@codex/finish-1181-session`, `utils/` Git tree
`9c5b53d7194d142a89be576c3314ee4bfd569a37`.
The original `utils/include` has **35 headers**, `utils/src` has **30 C files plus one
internal header**, and `utils/tests` has **69 entries including subdirectories**.

This document tracks **functional equivalence**, not just file presence. Nothing
is considered fully migrated until production source, public APIs, ownership,
negative and positive tests, and three-OS CI are verified. Do **not** remove a
function because its old implementation included CMeta reflection.

## Stage A: native string/allocator recovery (candidate, not accepted yet)

| Original source | Destination | Status |
| --- | --- | --- |
| `utils/include/tstr.h`, `utils/src/tstr.c` | `core/include/tstr.h`, `core/src/tstr.c` | C11 port, CI pending |
| `utils/include/vstr.h`, `utils/src/vstr.c` | `core/include/vstr.h`, `core/src/vstr.c` | Original view operations restored; no CMeta reflection, CI pending |
| `utils/include/str.h` | `core/include/str.h` | Same original source, CI pending |
| `utils/include/memory_pool.h`, `utils/src/memory_pool.c` | `core/include/memory_pool.h`, `core/src/memory_pool.c` | Plain C11, CI pending |
| `vendor/sds/{sds.c,sds.h,sdsalloc.h,LICENSE}` | `core/src/sds/` | Original bytes, private Core-owned storage; BSD license retained |
| `utils/tests/test_tstr_v2_contract.c` | `tests/test_tstr_v2_contract.c` | Exact original TinyTest case, optional profile |

The old SIMD-only vector operations are replaced with *bounded C11 byte scans*
for independent baseline Core builds; exported behavior is retained but
performance parity is **not yet claimed**. New real C11 and installed C11/C++17
consumers are required. `vstr_from_buf(NULL, n)` preserves the current strict
native invalid-range contract rather than reintroducing old silent truncation.

| `utils/include/object_pool.h`, `utils/src/object_pool.c` | `core/include/object_pool.h`, `core/src/object_pool.c` | Original object pool source and tests, CI pending |

## Still missing: full original utils functionality

**Not restored, do not mark migration complete or release:**

- Logging/format: `tlog.h`, `tlog_helper.h`, `tlog.c`, `fmt.h`,
  `fmt.c`, log-pattern and formatting lexers; sync/async sinks and flush
  parity, fatal logging, bounded queue and source capture all remain pending.
- Containers and algorithms: `ac_automaton`, `levenshtein_automaton`,
  `bucket_priority_queue`, `bucket_priority_queue_mpmc`,
  `bucket_priority_queue_spsc`, `ring_buffer_spsc`,
  `re`, `enum_utils`.
- mmap/UUID/zstd/SIMD/error: original `cmeta_mmap`, `cmeta_uuid`,
  `cmeta_zstd`, `cmeta_simd_scan`, `cmeta_error`.
- Requires a per-symbol difference audit, **not name matching**:
  `base64_utils`, `cmeta_buffer`, `cmeta_bytes`, `cmeta_fs*`,
  `cmeta_process`, `cmeta_thread`, `platform`, crypto ownership,
  `cmeta_cmeta_data`, `cmeta_mdns`.

**Permanent build contract:** Only `Salts::Core` owns these Core functions.
No additional provider, no CMeta runtime dependency, no legacy fallbacks.
Remaining functionality must be implemented in the appropriate source module
and verified with the original tests, ASan/UBSan/TSan, Linux/macOS/Windows, and
installed C11/C++17 consumers. Stage A PR stays Draft until its own exact HEAD
CI is green, and Issue #71 stays Open until every original function is mapped.
