# Core buffer foundation extraction

Tracked by [salts-core #3](https://github.com/qigao/salts-core/issues/3).
This is a stacked Draft qualification branch, not a production ABI cutover.

Original source commit: `qigao/salts@d6d936a86087a7f9a982126157d154e905580f30`.
- `salts/utils/src/cmeta_buffer.c` -> `core/src/cmeta_buffer.c`
- `salts/utils/include/cmeta_buffer.h` -> `core/include/cmeta_buffer.h`
- `salts/utils/include/cmeta_api.h` -> `core/include/cmeta_api.h`

Only includes and old target-owner text changed. Existing pool/buffer allocation,
reference counts, zero-copy slices, external buffer callback/retention and
bounded recycler remain unchanged.

`Salts::Core` now links `Salts::Platform` for mutex/once/TLS.
No CMeta reflection descriptors or CFlow actors are required, despite legacy
`cmeta_` API naming. `CNet`'s publicly exported `mem_buffer_t` and
`mem_slice_t` can therefore use these types without a dependency on old Salts.

Gates: cross-platform C11/C++17 clean-prefix consumer, ABI size/alignment and
source-signature comparison with old `cmeta_buffer.h`, invalid ranges,
pool-reset/destroy quiescence, TSAN, ASAN and allocator-fault tests. Do not
publish or dual-link both old and new `mem_*` implementations.
