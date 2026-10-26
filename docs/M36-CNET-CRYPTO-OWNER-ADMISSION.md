# M3.6 — Test-only CNet Owner → Crypto TLS policy admission

**Draft / source-level qualification only. Do not merge or switch production.**
This branch is stacked on SaltsCore [#28](https://github.com/qigao/salts-core/pull/28)
after its exact-head Windows IOCP / Linux epoll / macOS kqueue real
NativeIO acceptance, and tracks [#27](https://github.com/qigao/salts-core/issues/27),
[Core #20](https://github.com/qigao/salts-core/issues/20),
[Crypto #18](https://github.com/qigao/crypto/issues/18).

## Precisely what is implemented

- The new **private** `src/cnet/cnet_crypto_owner_admission.{h,c}`
  consumes the actual public CNet `cnet_tls_client_config` and
  `cnet_tls_server_config`, not test-invented configuration types.
  It maps an **explicit admitted subset** to installed opaque
  `crypto_tls_session_options`. No raw `TLS_CTX` or `TLS_CONNECT`
  crosses the DLL boundary.
- `CNET_CRYPTO_OWNER_PRODUCTION` is unconditionally rejected with
  `SALTS_ENOTSUP`. Only `CNET_CRYPTO_OWNER_TEST_ONLY` with fixed,
  exact TLS 1.2 or TLS 1.3 is eligible. No implicit protocol fallback.
- Client admission requires a bounded ASCII DNS identity and explicit
  nonempty PEM CA file. Missing file CA (the current CNet OS-trust
  default), CA directory, client mTLS certificate, ALPN, nonstandard
  profile, numeric IP literal, inconsistent SNI, or out-of-range BIO
  capacity is *rejected*, never silently ignored.
- Server admission requires explicit cert+key, no client authentication,
  no CA path, ALPN, TLCP/ShangMi, or unqualified key-password behavior.
  Both directions initialize the opaque options transactionally: any
  refusal leaves caller output unchanged.
- In `tests/crypto-nativeio/owner.c`, **real** TLS 1.2/1.3 + NativeIO
  loopback sessions now pass through this CNet policy gate *before*
  `crypto_tls_session_create`. The existing observed-terminal
  `mem_buffer_t` / TLS callback-lease / cancel-first barriers remain
  required. This is a useful CNet-facing qualification, but is NOT
  equivalent to linking or replacing the production `cnet_owner.c`.
- A distinct `tests/cnet-crypto-owner` consumer compiles the private
  admission source with the exact CNet public TLS structs staged from
  this HEAD. Core and Crypto are **separately installed** binary
  dependencies; `SALTSCORE_BUILD_CNET=OFF` and
  `SaltsCore::CNet` must be absent. Strict negative CTests explicitly
  reject production, system-store CA, CA directory, client mTLS, ALPN,
  TLCP/ShangMi, unverified IP SAN, unsupported versions and undersized
  buffers on Linux/macOS/Windows. The tested Crypto shared DSO is the
  single owner of the pinned private GmSSL implementation.
- Existing Linux ASan/UBSan/LSan and TSan, macOS/Linux real NativeIO,
  Windows true IOCP/Overlapped Socket, installed C11/C++17 and PE
  audit gates remain unchanged and mandatory.

## Remaining source-level CNet Owner integration

This branch intentionally **does not** compile an alternate full CNet
transport DSO, adjust `SaltsCoreCNet.cmake`, or modify
`src/cnet/cnet_tls.c` / `src/cnet/cnet_owner.c`. A complete CNet Owner
capability-gated test adapter still requires:

1. A mutually exclusive **test-only** `cnet_tls_state` implementation
   with correct `cnet_tls_handshake/write/read/take_cipher/feed_cipher`
   semantics, and an explicit *rejection*, not fabricated success, for
   every unsupported state query. Build it without linking the old
   GmSSL-bearing CNet DSO alongside Crypto.
2. The original owner state machine + real connection commands/events,
   retained `mem_slice_t` sends, receive events and generation-checked
   request IDs; race cancellation against TLS callback `AGAIN`,
   simultaneous close and NativeIO terminal observation.
3. Exact Client/Server ALPN, explicit CA paths and system trust,
   P-384/ECDSA, client mTLS, TLCP and ShangMi policy, pending plaintext,
   peer close-notify/EOF, negotiated information, peer-chain retrieval,
   RFC5929 signature-aware server-end-point binding, and all CNet
   error stages. Existing Crypto APIs are **not** claimed to provide
   features that are absent.
4. Source sanitizers and independent real Windows IOCP, Linux epoll,
   macOS kqueue, DSO unload/one shared runtime owner, installed SDK
   and C++17 graph. Linux/macOS success never substitutes for Windows.
5. A separate security/ABI review before discussing a coordinated
   production cutover. **All migration PRs remain Draft**, no merge or
   release, and `vcpkg-cache gmssl 3.2.0#13` / production
   `GmSSL::GmSSL` linkage stay unchanged.

## Evidence policy

Never tick the CNet Owner end-to-end or production-parity gates merely
because the stand-alone policy unit and NativeIO TLS owner are green.
These are the first fail-closed *source contract* checks, not a hidden
production replacement. Cite exact HEAD and real CI run IDs separately.
