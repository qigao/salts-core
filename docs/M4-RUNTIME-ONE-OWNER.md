# M4.7 — One shared foundation owner across CNet and installed consumers

Tracks #20. This is a **stacked Draft** on Windows Provider #19. The three
desktop CNet Provider jobs had passed at #19 head before this change, but
they did not establish a single process-wide allocator and thread identity.

## Architectural fix

Previously, Core, Platform, Concurrency, Coroutine, and NativeIO were STATIC
archives. Linking them both into CNet SHARED and a host executable or plugin
can create multiple distinct copies of global pool caches, thread-local
ownership markers, and coroutine state. On ELF, symbol preemption may hide
the problem. On Windows it is especially visible across DLL boundaries.

All five stateful foundation targets are now **SHARED** with one installed
runtime identity per module, including Windows import libraries. In dependency
order: Platform → Core / Concurrency → Coroutine → NativeIO; CNet imports
these modules rather than copying their static archive objects. CNet remains
an optional SHARED provider and must fail at configure time if a foundation
owner is not shared. URI is stateless and may remain an archive; the CNet
private Monocypher/Reed helpers remain internal archives.

Only one version of these five binaries may exist in a process. The old Salts
libraries still own historical C symbols and **must not be linked or loaded
alongside this candidate**. The separate coordinated ABI/SONAME epoch,
consumer cutover and symbol ownership audit remain mandatory before release.

## Failing-first contracts (must be executed by exact-head CI)

- Source CNet TCP test receives a real retained slice from the CNet DSO,
  requires `mem_buffer_pool(slice.buffer) == mem_global()` in the executable,
  compares pool/TLS identities to a separate loaded test module, then destroys
  CNet and releases the retained slice **from that module**.
- The installed SDK test links a clean-prefix executable and independently
  compiled test SHARED library to Core/Platform plus CNet, checks same-thread
  `cmeta_thread_current_token()` identity and global pool pointer equality,
  and transfers retained slices both directions. It releases a slice after
  CNet teardown.
- A CNet-free host dynamically loads two independent shared consumer plugins,
  both explicitly calling CNet; after unloading plugin A, plugin B releases a
  retained slice allocated by A. Eight unload/reload cycles compare the same
  shared pool/TLS identity; no duplicate CNet copy or fallback is linked into
  the executable. Exact-head CI must qualify this new test on all three OSes.
- CNet's existing real TCP/TLS CA/SAN, GmSSL/ShangMi and SDK tests remain
  enabled on Linux, macOS and Windows. The minimal SDK workflow also runs
  installed C11/C++17 consumers after adding the shared-runtime loader path.
- Windows test binaries and DLLs share one build-tree runtime output
  directory; installed binaries must resolve from the explicit install prefix.

The new checks are designed to fail with independent STATIC copies; that
hypothesis must be confirmed against the original graph before claiming an
observed before/after failure.

## Still unqualified

Final symbol/loaded-image audits on ELF/Mach-O/PE, threaded unload/retain races across concurrently loaded
CNet plugins, ASan/UBSan/TSan,
independent TLS peer interoperability, Android/iOS ABI, old Salts/ACE cutover,
and a documented stable new ABI epoch are all **separate gates**. Green on
one or more desktop CI jobs does not waive these.

## M4.8 — independent installed binary ownership audit (candidate)

The provider workflow now validates **installed**, not build-tree, Core,
Platform, Concurrency, Coroutine, NativeIO and CNet shared images via
`tools/audit_runtime_owner.py`. It checks:

- Exactly one installed real image per owner (versioned symlinks resolve to
  the same binary), including each binary's SHA-256 in a portable JSON report.
- Unique exported definitions of `mem_global`, `mem_buffer_release`,
  `mem_slice_release`, `cmeta_thread_current_token`,
  `cmeta_deadline_queue_init`, `coro_executor_current`,
  `native_io_backend_init` and `cnet_client_init`. Duplicate public
  symbols across the six libraries fail closed; private ThreadPool TLS is
  explicitly forbidden as a public export.
- Clean-prefix installed executable and independently built shared probe
  must not **define** global-owner functions. Binary link dependencies are
  checked using ELF readelf, Mach-O otool, or PE dumpbin: the executable
  must import Core/Platform/CNet; the independent module Core/Platform,
  and CNet Core/NativeIO.
- No silent success when a binary/tool is absent, symbols are missing, or
  imports are wrong. Reports from each platform are uploaded by CI.

This distinguishes symbol ownership from the already-passed live pointer
identity and basic plugin unload tests. The audit does not yet prove an
old-Salts binary can coexist, threaded unload under TSan, or a final new
ABI/SONAME epoch; these gates remain **open**.

## M4.9 — bounded cross-thread release after plugin unload

The two-plugin runtime test additionally creates four caller-owned retained
buffers from plugin A, unloads A only after its callbacks have returned, then
runs four `cmeta_thread_create` workers concurrently using plugin B's
release entry point. Each worker checks the one shared pool and same-thread
Platform token identity; the parent joins all workers **before** unloading B.
Failure cleanup also joins all admitted workers before any module teardown.

This is a bounded quiescent-unload/parallel-release contract, **not** a license
to `dlclose` a library while any of its code is executing. TSan/ASan and
real adversarial close/cancel races remain qualification gates.

### macOS audit parser correction

The first installed Mach-O audit surfaced a tooling bug: `otool -L`
includes a dylib's own install-name as the first entry, but an executable's
first entry is an actual dependency. The parser previously skipped that first
dependency for executables and falsely reported missing CNet. Regression tests
now distinguish the two formats. This change does **not** relax exported-owner
uniqueness, required imports, or failure on missing tools/artifacts.

## M4.10 — scoped Linux ASan/UBSan and TSan provider gate

The independent `.github/workflows/cnet-sanitizers.yml` builds the **real**
GmSSL-backed CNet using the canonical pinned vcpkg scripts and the files-only
provider cache. The scoped PR/dispatch matrix runs instrumented runtime code
in two mutually exclusive configurations: AddressSanitizer+UndefinedBehavior-
Sanitizer (with leak detection), and ThreadSanitizer (with fail-on-report).
It repeats the real TCP, buffer/NativeIO, and two-plugin cross-thread release
CTest group three times. It does **not** install Mono/NuGet or switch TLS
providers, and is deliberately not triggered by unrelated documentation PRs.

Sanitizer evidence must reference the exact branch commit; adding this job
does not constitute a passing TSan/ASan claim until both jobs succeed. The
original Salts module transition and multi-provider TLS interoperability
remain separate gates.

## M4.11 — address LeakSanitizer's per-DSO global mutex leak

The new sanitized two-plugin test found an actual **320-byte heap leak**
(8 allocations × 40 bytes) at `cmeta_mutex_init()` via the CNet module's
once-initialized global provider lock. Each `dlclose` left the allocated
mutex behind; `cnet_module_shutdown()` could not safely destroy that mutex
because a concurrent `cnet_module_init()` might already be waiting on it.

The CNet module's owner-state lock is now a **statically initialized native
mutex** (`PTHREAD_MUTEX_INITIALIZER` on POSIX, `SRWLOCK_INIT` on Windows).
No lock allocation or destructor is needed, avoiding both the leak and
destroy-under-waiter race. The provider's `references`/`resolvers` admission
and cleanup semantics are unchanged; process clients must still quiesce before
unloading CNet. LeakSanitizer remains **enabled**, not suppressed.

This is a candidate source fix until ASan/UBSan exact-head tests confirm it.

## M4.12 — installed C11/C++17 SDK relocation acceptance

After each provider platform's first clean-prefix ABI audit, CI **moves** the
entire installed SaltsCore SDK out of its original prefix. The original path
is explicitly checked absent. It then runs `find_package(SaltsCore CONFIG
REQUIRED)` from an unrelated second build directory using only the relocated
prefix, builds and executes all seven C11/C++17 CNet consumers, and repeats
the installed binary-owner/export/import audit. The pinned GmSSL/KCP/c-ares
provider DLLs stay on their legitimate external vcpkg dependency path, not
in the old SaltsCore prefix.

This tests actual package relocatability and cross-DSO buffer ownership after
relocation, not just installed header presence. A JSON audit report for the
relocated binaries is uploaded per OS. The gate must pass at its exact HEAD
before calling SDK relocation qualified.
