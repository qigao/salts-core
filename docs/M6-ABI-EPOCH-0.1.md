# SaltsCore pre-stable native ABI 0.1 (Draft)

SaltsCore 0.1.0 has an independent **native ABI epoch 0.1**. Shared
Core, Platform, Concurrency, Coroutine, NativeIO and optional CNet
use SONAME 0.1 (not 0) so an incompatible future 0.2 cannot silently
replace an installed 0.1 binary. The CMake package version file uses
SameMinorVersion rather than SameMajorVersion during 0.x development.

Consumers admitted for this epoch must resolve
salts_core_runtime_abi_0_1_required() from the real installed Core DSO,
check the runtime version, and compare public mem_pool_t,
mem_buffer_t and mem_slice_t sizes against the header. This detects
a mismatched DSO/header, but is not evidence of runtime pointer/CRT
ownership by itself. Continue the exact-head cross-DSO retained-buffer,
Linux sanitizer/TSan, and final ELF/Mach-O/PE symbol audits.

Old Salts 2.3 still exports overlapping historical C names. The new
SONAME/CMake target is NOT a fallback alias or permission to load
both old and new owners. Full CMeta/CFlow/ServiceHost/ACE binary
handoff remains a separate unmerged migration. The isolated original
CMeta/Configurator downstream proof is in Salts Draft PR #1304.
Tracking: salts-core#20, salts-core#3, salts#1287.

## Bidirectional version compatibility proof

CI mutates the **real installed** SaltsCoreConfigVersion.cmake output
while retaining its compatibility logic. A 0.1.0 consumer must accept
0.1.0 and 0.1.9, but reject a future 0.2.0 provider. Conversely,
0.1.0 must reject a 0.2.0 consumer request. This detects an accidental
SameMajorVersion regression, which would otherwise admit 0.2.0 to
a consumer built against the incompatible 0.1 ABI. No native 0.2
binary is built or considered qualified.
