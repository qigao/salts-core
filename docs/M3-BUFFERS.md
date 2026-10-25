# Core retained-buffer extraction

Status: Draft qualification; not an ABI cutover.

Imported `utils/include/cmeta_api.h`, `utils/include/cmeta_buffer.h` and `utils/src/cmeta_buffer.c` from qigao/salts at `d6d936a86087a7f9a982126157d154e905580f30`. Header and source dependencies are redirected from old aggregate `platform.h` and `cmeta_thread.h` to the new local `cmeta_api.h` and `<salts/thread.h>`.

The implementation owns its slab allocations, bounded recycled-buffer caches, reference counted `mem_buffer_t` and `mem_slice_t` views. Neither CMeta Reflection nor CFlow is involved, despite the legacy file name.

The standalone `SaltsCore::Core` target now consumes `SaltsCore::Platform` for thread/once/mutex support. It does not depend on NativeIO/CNet, GmSSL, CMeta, CFlow, or the old Salts SDK.

Before production migration: run original extensive buffer/cache/retention conformance, allocation fault tests, sanitizer and TSan, API/symbol audit, clean-prefix installed SDK on all target platforms. Do not link old `Salts::Core` together with this candidate because the `mem_*` symbols have two owners until coordinated cutover.