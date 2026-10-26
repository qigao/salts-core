# M3.8 — Original CNet Owner ↔ test-only opaque Crypto TLS object ABI

This is a **failing-first, test-only source/object qualification** stacked on
[SaltsCore Draft #31](https://github.com/qigao/salts-core/pull/31).
It DOES NOT merge, publish or replace production `SaltsCore::CNet`.

## Verified source boundary

The CNet Owner is the **original, unmodified**
`src/cnet/cnet_owner.c`, not a copied or simplified test owner. The private
TLS substitute comes from `src/cnet/cnet_tls_crypto_qualification.c`
which has been separately qualified on genuine TLS 1.2/1.3 sessions
against one physically shared `Crypto::TLS` native image and the
host-only, bounded `Crypto::TLSCipherBIO` library.

The new independently configured `tests/cnet-crypto-owner/CMakeLists.txt`
compiles BOTH source files into real native C11 object artifacts, with
the originally installed SaltsCore::Core / NativeIO / Concurrency / Platform
targets and exactly one Crypto TLS import. There is **no** direct private
GmSSL C header, source file or second shared provider in either object.

The test-only `audit_original_owner_abi.py` additionally requires:

1. The complete fixed set of **21** currently invoked original CNet Owner
   `cnet_tls_*` APIs remains unchanged. New or missing calls fail closed.
2. All those exact function names have genuine definitions in the
   alternative opaque Crypto TLS source and declarations in the current
   original private CNet header. A mere renamed stub or undocumented
   capability bypass fails admission.
3. Both compiled native object files exist and are nonempty.
4. The canonical production `cmake/SaltsCoreCNet.cmake` continues to use
   the incumbent `src/cnet/cnet_tls.c` and `GmSSL::GmSSL`. The private
   replacement has not accidentally entered that production graph.

All three desktop platforms run this against **independently installed**
native Core and Crypto. Linux runs the same source and object validation
under real source-instrumented ASan/UBSan/LSan and TSan along with the
prior TLS/NativeIO tests.

## Critical limits

**Compile + ABI surface alignment is not linkage or runtime parity.**
The original CNet Owner also depends on command queues, event buses,
resolver, transport, URI parsing, pending socket operations, retained
`mem_slice_t` and lifetime/cancellation logic, which are not linked by
this standalone object qualification. An exact defined `cnet_tls_*`
symbol does not prove its original runtime return codes, metadata or
borrowed-pointer semantics are behaviorally identical.

The next separate, high-priority milestone must compile a **mutually
exclusive test CNet DSO**, using all actual original Owner dependencies
but replacing `src/cnet/cnet_tls.c` with the qualified Crypto source.
It must NEVER load the old CNet/GmSSL binary alongside the new Crypto
DSO in one process; compare both applications as isolated processes.
Require real NativeIO command/event/retained-buffer differential runs,
CA/SAN failure handling, TLS AGAIN, EOF/close_notify, cancellation and
DSO unload. The current Crypto provider does **not** yet support all
CNet system CA stores, P-384/ECDSA, client mTLS, ShangMi/TLCP and
RFC5929 binding semantics, so these remain explicit fail-closed blockers.

Original [SaltsCore #27](https://github.com/qigao/salts-core/issues/27),
[Crypto #18](https://github.com/qigao/crypto/issues/18) and the shared
Foundation runtime [SaltsCore #20](https://github.com/qigao/salts-core/issues/20)
must remain OPEN. No production CNet or `vcpkg-cache gmssl 3.2.0#13`
changes are made here.
