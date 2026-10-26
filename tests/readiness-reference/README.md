# Source-identical Readiness tests against installed SaltsCore Platform

This independent C11 harness checks out qigao/salts at exact revision
`df4c697d2f1facb6b54a3568520dd743c628aa81`, then compiles
its **original** Readiness test translations and test-only TinyTest.
No old Salts Core/Platform/NativeIO binaries are built or linked;
all Readiness state, callbacks, backend ops and resource owners resolve
from installed **shared** `Salts::Platform`.

The independent source conformance checks original state-model and
complete fake-backend contract, including failed register/arm/unarm,
close/shutdown, stale and duplicate generation events, terminal callbacks,
concurrent callbacks and bounded slot reuse. Native tests exercise poll
on Linux/macOS, kqueue on macOS, Linux explicit epoll as a **separate
opt-in** configuration and Windows' no-fallback ENOTSUP contract.

All test sources stay in the original repo, pinned by commit; no
redundant permanent test/third-party source fork is added to Core.
The original private Readiness header is byte-identical at the selected
revision, while the final implementation is always imported from the
single installed SaltsCore Platform owner.

This is a Draft qualification path, not final coexistence with legacy
Salts 2.3 symbols, production switch, or release acceptance.

## Instrumented source parity

The separate Linux `readiness-reference-sanitizers.yml` compiles the
actual standalone Core shared Platform and all original pinned Salts
Readiness test translation units using identical ASan/UBSan or TSan
instrumentation. Explicit epoll and poll are both exercised. Each
original state, fault-backend, poll and epoll test is repeated three
times with LeakSanitizer or ThreadSanitizer enabled and fail-on-report.

The SDK workflow statically validates this workflow's anchored,
shell-quoted `ctest -R` selection, preventing build-only false greens.
