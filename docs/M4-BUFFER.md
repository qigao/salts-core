# Core buffer ABI extraction — CNet dependency preparation

Track [#3](https://github.com/qigao/salts-core/issues/3).

Source: qigao/salts `d6d936a86087a7f9a982126157d154e905580f30`;
original `utils/include/cmeta_api.h`, `utils/include/cmeta_buffer.h`,
`utils/src/cmeta_buffer.c`.

## Scope

Copy original slab, refcounted memory buffer, borrowed slice and pool internals.
The only source substitutions are direct header dependencies:
- `cmeta_buffer.h`: replace the former broad `platform.h` and
  `cmeta_thread.h` with `cmeta_api.h` and `<salts/thread.h>`.
- `cmeta_buffer.c`: use `<salts/thread.h>` instead of `cmeta_thread.h`.
- Documentation names the new target `SaltsCore::Core` rather than old
  `Salts::Core`.

No Reflection, CMeta type descriptor, CSTL or CFlow is needed. The public ABI
continues to expose the original struct sizes and `mem_*` symbols, but its
atomic C/C++ representation and global cache must be independently audited.

## Acceptance

- Plain `SaltsCore::Core` may link `SaltsCore::Platform` for OS mutex/once
  and TLS; must not link GmSSL, CNet, CMeta or CFlow.
- Validate allocation-size overflow, failed allocation rollback,
  slice-retain/release, refcounts, global pool lifecycle, cross-thread races
  and C++ installed header ABI.
- **No dual linking** with the original Salts::Core `mem_*` implementation.
  This is a new single-owner qualification candidate, not a parallel provider.
- Real CNet source extraction requires separately qualifying the URI parser,
  DNS/TLS dependencies and borrowed-buffer wire/lifecycle contracts.
