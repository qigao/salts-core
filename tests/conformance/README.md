# Test-only original runtime conformance

Source SHA: qigao/salts `d6d936a86087a7f9a982126157d154e905580f30`.

The isolated `tests/conformance/vendor/tinytest` tree is a **test-only** copy of
qigao/salts `tinytest/include` (selected C headers) and `tinytest/src/tinytest.c`;
it is neither installed nor linked by any SaltsCore production target.

All 9 conformance sources are copied from original Salts without changes,
except for historical `#include "cmeta_thread.h"` (replaced by
`<salts/thread.h>` + `<salts/thread_pool.h>`) and old
`#include "disruptor.h"` (replaced by `<salts/disruptor.h>`).

Includes the original DeadlineQueue, RCU, default and LF ThreadPool,
Disruptor, Coroutine Executor, manual Coroutine completion, NativeIO
Sharded host lease and IPC tests.

**Scope limit**: Not yet the entire NativeIO suite, Linux io_uring stress,
Windows IOCP socket acceptance, full NativeIO sharded workload or sanitizer
TSan/ASan/UBSan qualification. Treat passing tests as source-conformance
evidence only; don't change the old Salts production owners.
