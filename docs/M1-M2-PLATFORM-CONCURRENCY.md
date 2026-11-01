# M1/M2: Platform and Concurrency extraction candidate

**Status: Draft; no production cutover.** Parent: [#1](https://github.com/qigao/salts-core/issues/1);
runtime milestone [#3](https://github.com/qigao/salts-core/issues/3).

## Source provenance

Original `qigao/salts` master commit:
`d6d936a86087a7f9a982126157d154e905580f30`.

Copied unchanged from `salts/platform`:
- `platform/include/{clock.h,random.h,thread.h,error_codes.h}`
- `platform/src/{clock.c,random.c,thread_posix.c,thread_win32.c}`

Copied unchanged from `salts/concurrency`:
- public `{deadline_queue.h,disruptor.h,rcu.h,spsc_ring.h,thread_pool.h,atomic.h}`
- sources `{deadline_queue.c,rcu.c,spsc_ring.c,thread_pool.c,thread_pool_lf.c,thread_pool_lf.h}`.

`disruptor.c` has **include-only** changes: it now includes
`<disruptor.h>` and `<thread.h>` rather than legacy
`"disruptor.h"` and `"cmeta_thread.h"` from the old Core aggregate.
It does not import CMeta types or scheduling objects.

## Contracts

- Public `Salts::Core` remains dependency-free. `Salts::Platform`
  links only system Threads / Win32 BCrypt. `Salts::Concurrency` links
  `Salts::Platform` and has no Salts/CMeta/CFlow/CSTL dependency.
- Public `<*.h>` and historical `cmeta_*` symbols are **migration**
  compatibility spellings. Do **not** link new and old copies of these symbols
  into the same image during qualification. No `Salts::` alias is exported.
- Installed `find_package(SaltsCore CONFIG REQUIRED)` exports three targets.
  C11 and C++17 installed consumers must compile and run without Salts present.
- OS CSPRNG is fail-closed. Randomness has no provider fallback.
- Deadline queue, SPSC, RCU, Disruptor and ThreadPool follow original bounded
  ownership contracts. One accepted task has one run/cancel terminal and one
  finalize; queue rejection invokes none. SPSC is strictly one producer/consumer.
- This PR does **not** qualify Coroutine, NativeIO, CNet, full readiness, Android
  or iOS. No preexisting platform CNet/ACE ownership should be re-exported.

## Remaining test work

- Run full original Concurrency conformance (TinyTest), including stress,
  cancellation/queue saturation, RCU guard transfer and TSan where supported.
- Audit 32/64-bit and C/C++ public atomic layout across supported SDK ABIs.
- Validate Linux/macOS/Windows installed SDK, Debug/Release, symbol ownership;
  Android/iOS compile with appropriate toolchains.
- Separate platform monotonic-clock/cache and Android CSPRNG contract review.
