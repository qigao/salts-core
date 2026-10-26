# Salts::Platform portable Atomic / Thread runtime (Draft P0)

Owner: **qigao/salts-core**, with dependency direction
\`Salts::Platform (Atomic + OS Threads) -> Salts::Concurrency (queues, RCU, pool)\`.
Consumers link only \`Salts::Platform\`; CMeta, CFlow, ACE, SaltsUtils,
Crypto, GmSSL and C++ are not dependencies.

## Compiler portability

MSVC, llvm-mingw/Clang and TinyCC use **one exported C ABI** for atomic
and thread operations. The provider uses Win32 Interlocked and
\`_beginthreadex\` / SRWLOCK / CONDITION_VARIABLE on Windows, and GNU
atomic compiler intrinsics plus pthreads on Unix, with no \`_Atomic\` source
requirement in *any client*. The provider may be built with MSVC or Clang;
TinyCC clients must use the same version-checked DLL/DSO C ABI, never
reinterpret another compiler's \`_Atomic(T)\` or C++ \`std::atomic\`.

This is a **source-independent runtime interface**, not an ISO C11
\`<stdatomic.h>\` or \`<threads.h>\` implementation yet. The C11 compiler
keyword \`_Atomic\` cannot be implemented by a library alone. The
standard-header adapter and compiler lowering work remain separate
tasks, and existing \`__STDC_NO_ATOMICS__\` / \`__STDC_NO_THREADS__\`
declarations are never silently undefined to forge conformance.

## Atomic storage, memory semantics and ABI

- Embedded slot widths 8/16/32/64 and native pointer, with explicit
  eight-byte alignment on 64-bit slots even on Windows i686.
- All operations including CAS and publication currently execute with
  **at least sequential consistency**, even when a weaker valid order
  was requested. Load and store order constraints and CAS success/failure
  order pairs are checked, invalid combinations fail without mutation.
- No unsupported backend, volatile RMW, adjacent-byte CAS, or generic
  lock-based pretence of lock-free conformance. Windows uses intrinsic
  Interlocked CAS; GCC/Clang uses \`__atomic\`.
- All slots must be aligned, remain address-stable and stay alive through
  all concurrent accesses. They must not alias regular native atomics.
- \`salts_platform_portable_abi_verify\` fails closed on a different
  ABI version, atomic64 storage/alignment or pointer-slot size. C exports
  have explicit Windows cdecl; no cross-module malloc/free or native
  Windows/POSIX OS handles in the public API.
- APIs return Salts status codes; compare-exchange returns an explicit
  exchanged output plus failed-CAS expected writeback.

## Thread ownership

- Provider-owned opaque thread, mutex and condition allocations;
  join/detach consumes the owning handle. Worker + owner references
  protect a detached worker's storage from early release.
- Thread entry callback is C cdecl on Windows i686 and remains
  owned by its defining module; **do not unload that module before
  callbacks have quiesced**. Detach does not cancel or drain callbacks.
- Mutex/condition destruction is permitted only after all entrants and
  waiters have quiesced. POSIX mutex destroy detects an active lock;
  Win32 SRWLOCK cannot enforce safety against a racing entrant.
- Condition waits release/reacquire their mutex and must always
  recheck predicates in a loop. Once callbacks must not recursively
  initialize the same flag.
- P1: timed mutex/condition, thread-specific keys/destructors,
  creation fault injection, module unload races, full \`threads.h\`
  conformance and extension over standard API as needed.

## Required acceptance

- Native MSVC, Windows llvm-mingw x86/x64/ARM64, Linux GCC/Clang,
  native macOS Clang and platform install/consumer tests.
- Actual Windows TinyCC x86/x64/ARM64 caller -> versioned exported DLL
  (use function pointers/LoadLibrary if TinyCC cannot link COFF import libs).
  Linux/macOS TinyCC -> same shared ABI. No fake compiler-native atomics.
- CAS failure writeback, overflow and subword sentinels, pointer CAS,
  invalid memory orders, 64-bit alignment, 6 worker contention,
  join/detach, once, mutex/condition wakeup and failed busy destroy.
- GCC/Clang source TSan, ASan/UBSan and actual Windows ARM64 execution.
  Native ARM64 hardware evidence is not replaced by cross compilation.
- Full downstream migration of older \`_Atomic\` fields (Core buffers,
  Concurrency, CNet) is **not yet performed**. The existing ABI stays
  unchanged until separately qualified. No production compiler switch
  or non-Draft merge based solely on these platform tests.

There is no legacy runtime fallback; old historical \`cmeta_\` entrypoints
are outside this new C ABI and are not accepted as substitutes.


## M1-A: Monotonic relative timed wait / thread sleep

\`salts_platform_condition_timedwait_ns\` and \`salts_platform_thread_sleep_ns\`
accept relative nanoseconds, with a **uniform cross-compiler maximum**
\`SALTS_PLATFORM_MAX_RELATIVE_WAIT_NS\` (the largest non-INFINITE Win32
millisecond duration). Larger durations fail with \`SALTS_EINVAL\`, rather
than silently converting to an infinite sleep. Zero-duration condition
waits return \`SALTS_ETIMEDOUT\` when no signal was admitted.

- Win32 condition waits use \`SleepConditionVariableSRW\`; positive
  sub-millisecond timeouts round upward to a millisecond.
- Linux / compatible POSIX use \`CLOCK_MONOTONIC\` condition attributes
  and absolute monotonic deadlines with overflow checks; unsupported
  platform clock support refuses \`SALTS_ENOTSUP\` rather than
  falling back to real-time waits.
- Apple Darwin uses \`pthread_cond_timedwait_relative_np\` with a relative
  deadline. POSIX sleep retries a signal-interrupted \`nanosleep\`.
- All condition waits leave the caller holding its mutex; wakeups
  may be spurious, so callers must repeat their predicate checks.
- This is a portable Platform contract, not yet C11 \`cnd_timedwait\`.


## M1-B: Portable thread-specific storage (TSS)

Provider-owned opaque \`salts_platform_tss\` keys use \`pthread_key_create\`
on POSIX and \`FlsAlloc\` on Windows. Each active thread receives a
private heap-owned slot wrapper; the pointed-to user value remains borrowed.
On thread exit the provider invokes the user destructor (C cdecl, **not**
the x86 \`WINAPI\` FLS calling convention), and only then retires the active
slot count. Thus a key cannot be destroyed while any live non-NULL value
or in-progress destructor remains; destruction returns \`SALTS_EBUSY\`.
\`tss_set(NULL)\` removes the current thread's wrapper without calling the
user destructor. A deterministic six-worker test holds real slots live,
proves busy-destroy denial, then joins threads and requires exactly one
destructor per worker.

Current ownership contract: caller serializes key destruction against
new set/get activity, and keeps the code module implementing destructor
callbacks loaded until all relevant threads have fully exited. Callback
re-registration on the *same* key, explicit thread cancellation and
adversarial simultaneous key-destroy/set are **not** claimed qualified;
these require additional native fault tests before exposing full
\`<threads.h>\` semantics.


## M1-C: monotonic timed mutex (additive)

\`salts_platform_mutex_timedlock_ns\` uses the same **relative** nanosecond
API and maximum timeout as the earlier timed condition/sleep primitives.
A held mutex with duration 0 returns \`SALTS_ETIMEDOUT\` after one try-lock.
Invalid/null inputs and a duration exceeding
\`SALTS_PLATFORM_MAX_RELATIVE_WAIT_NS\` return \`SALTS_EINVAL\`.
On success the caller owns the mutex until an explicit unlock.

The Linux/macOS backend retries \`pthread_mutex_trylock\` with a bounded
1 millisecond \`nanosleep\` and a \`CLOCK_MONOTONIC\` elapsed-time check;
it intentionally does **not** call \`pthread_mutex_timedlock\` (which
commonly uses CLOCK_REALTIME). The Windows backend retries the native
SRWLOCK \`TryAcquireSRWLockExclusive\` against \`GetTickCount64\`,
rounding positive durations up to a millisecond and yielding to the OS
rather than spinning with raw volatile reads.

This is an additive portable C ABI, not a claim of C11 \`mtx_timedlock\`
(which specifies an absolute UTC deadline). Recursive locking remains
invalid. Pointer lifetime, shutdown and destruction races still require
external quiescence, as with the base mutex API. See \`Issue #42\`
for complete \`threads.h\` admission and owner-drain handling.
