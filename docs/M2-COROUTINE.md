# M2: Coroutine extraction candidate

Source: qigao/salts master `d6d936a86087a7f9a982126157d154e905580f30`.

## Ownership
- Native `coro_create`/`coro_yield`/`coro_resume` and scheduler, pool and sharded
  executor move into the independent `Salts::Coroutine` target.
- Files copied unchanged: `coroutine/src/{coro.c,coro_pool.c,coro_executor.c,coro_executor_internal.h}`,
  `coroutine/include/{coro.h,coro_pool.h,coro_executor.h}`.
- Private vendor: `vendor/minicoro/minicoro.h` and `LICENSE`. Vendor header is NOT public.
- `coroutine/include/cmeta/coroutine.h` stays in qigao/salts as the
  CMeta coroutine facade, never included from SaltsCore.
- ABI and symbol names are candidates only. No alias `Salts::Coroutine`,
  no dual installation or link with original Salts until migration cutover.

## Qualification
- C11 standalone build and installed C++17/C consumers, Linux/Windows/macOS
  native execution.
- Manual yield/resume and bounded executor success/finalize smoke tests.
- Future full original TinyTest suite: await/timeout race, cancellation,
  cross-thread completion, queue exhaustion, worker/shard affinity, close/
  destroy quiescence, TSan and Android/iOS installed SDK cross-build.
- Core -> Platform -> Concurrency -> Coroutine -> NativeIO is the only
  allowed lower-layer dependency direction.
