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
- CNet's existing real TCP/TLS CA/SAN, GmSSL/ShangMi and SDK tests remain
  enabled on Linux, macOS and Windows. The minimal SDK workflow also runs
  installed C11/C++17 consumers after adding the shared-runtime loader path.
- Windows test binaries and DLLs share one build-tree runtime output
  directory; installed binaries must resolve from the explicit install prefix.

The new checks are designed to fail with independent STATIC copies; that
hypothesis must be confirmed against the original graph before claiming an
observed before/after failure.

## Still unqualified

Final symbol/loaded-image audits on ELF/Mach-O/PE, two concurrently loaded
CNet plugins, dynamic unload and retained-buffer races, ASan/UBSan/TSan,
independent TLS peer interoperability, Android/iOS ABI, old Salts/ACE cutover,
and a documented stable new ABI epoch are all **separate gates**. Green on
one or more desktop CI jobs does not waive these.
