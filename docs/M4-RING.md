# M4: Core single-owner bipartite ring

Source: `qigao/salts/utils/src/ring_buffer.c` and
`qigao/salts/utils/include/ring_buffer.h` from immutable Salts head
`d6d936a86087a7f9a982126157d154e905580f30`.

These C11 functions are the single-owner borrow/acquire/release byte ring
already used by CNet's bounded inbound handoff. The algorithm was copied
byte-for-byte; only the installed header swaps old `platform.h` for the
independent `cmeta_api.h` and standard `stddef.h`.

- Installs `<ring_buffer.h>` with `SaltsCore::Core`, no CMeta/CSTL/CFlow.
- Caller owns fixed storage, supplies no allocator and must not use across
  concurrent producers/consumers (use SPSC ring instead).
- New native test proves FIFO, tail invalidate/wrap, empty transition and
  bounded capacity rejection; installed C11/C++17 consumers link `Core`.
- No old/new simultaneous linking: `ring_*` C symbols are identical.
- Additional parity tests and ABI checks are required before retiring the
  old Salts library. This is a Draft migration step, not a production switch.

Next: CNet transport refactor needs removal of reflected CSTL map and deque,
and `tstr` IPC name helper (tracked in original Salts Draft PRs). Only
after their exact-head CI passes should CNet code be copied to Core.
