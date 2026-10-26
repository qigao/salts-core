# M3.7 — Real CNet-private TLS state signatures on independent Crypto TLS

**TEST-ONLY qualification; stacked on SaltsCore Draft #30. DO NOT MERGE or switch CNet.**

The code in `src/cnet/cnet_tls_crypto_qualification.{h,c}` is a
mutually-exclusive candidate implementation of the **original private
`cnet_tls_state` interface**, not a second GmSSL library and not a
production provider selection. The original `src/cnet/cnet_tls.c` and
`src/cnet/cnet_owner.c` remain untouched and are still built with
`GmSSL::GmSSL` when `SALTSCORE_BUILD_CNET=ON`. This candidate is
compiled only in an independently configured **installed SDK test**
with production CNet disabled.

## Genuine supported slice

- A private, explicit, qualified context factory consumes the real
  public CNet client/server TLS policies from M3.6, an exact
  TLS1.2 **or** TLS1.3 protocol and a borrowed single Crypto provider.
  It deep-copies the admitted hostname/trust/cert/key paths before
  session creation; failed admission never transfers ownership.
  The factory is a test-only entry point, not a public ABI.
- The actual CNet `cnet_tls_state_init/destroy`, `handshake`,
  `feed_cipher/take_cipher`, `write/read`,
  `probe_peer_close`, `shutdown`, `read_buffer/write_buffer`,
  `cipher_input_capacity` signatures now have a bounded Crypto
  implementation. NativeIO buffers remain separate from TLS scratch
  and private plaintext; no kernel borrow aliases Crypto's internal
  bounded ciphertext rings. The caller must observe every NativeIO
  terminal before invoking the void `cnet_tls_state_destroy`.
- The engine has one opaque Crypto TLS session, a host-only static
  Crypto CipherBIO and a bounded independent plaintext buffer.
  `probe_peer_close` may prefetch decrypted plaintext but never
  publishes or discards it: `cnet_tls_read` consumes it in exact
  prefixes. Partial application writes retain the exact source and
  offset until the accepted logical write settles.
- The candidate maps Crypto TLS1.1 negotiated version/cipher and
  TLS1.2 peer SHA-256 and RFC9266 channel binding to the real CNet
  private query signatures. The exporter is computed by the *one real*
  Crypto/GmSSL engine, not by a fake KDF.
- No ALPN is admitted, so negotiated ALPN reports
  `SALTS_ENOENT` (never fabricated). Incorrect or unspecified TLS
  ranges return `SALTS_ENOTSUP`; exact configured version can be
  confirmed before handshake, then range changes are refused as
  `SALTS_EBUSY`.

## Fail-closed non-parity (must remain OPEN)

- An opaque Crypto `CLOSED` result does **not** prove authenticated
  TLS `close_notify`; the candidate treats it as a connection failure
  rather than setting CNet's `peer_close_notify` Boolean. True
  close-notify introspection needs an upstream Crypto ABI extension.
- RFC5929 `tls-server-end-point` needs the certificate signature-
  algorithm-specific digest. Existing `peer_sha256` is **not**
  interchangeable and is never substituted. RFC5929 and the
  CNet zero-copy peer-chain view explicitly return `SALTS_ENOTSUP`
  until matching data ownership and parsing are qualified.
- Missing OS trust/hashed CA dirs, P384/ECDSA parity,
  client mTLS, ShangMi/TLCP, binary ALPN policy, full CNet
  configurable TLS1.2-to-1.3 negotiation, and exact EOF/error stages
  are still explicit blockers. The CNet production default range
  must **never** be converted to silently enforced TLS1.3 or a
  fallback TLS1.2 session.
- The **original** `cnet_owner.c` command/event/session scheduler,
  original retained `mem_slice_t` transfers and NativeIO completion
  lifecycles, TLS callback AGAIN vs owner close/cancel and DSO unload
  remain a separate later integrated test: this candidate does not
  claim a complete alternate full CNet Owner just because individual
  CNet private functions can now be linked.

## CI acceptance

New `tests/cnet-crypto-owner/state.c` does real Crypto TLS1.2 **and**
TLS1.3 handshakes through the canonical `cnet_tls_state` signatures,
the original CNet policy structs, exact encrypted record exchange,
plaintext prefetch/partial read retention, one-owner quiescence,
negotiated strings, peer fingerprint and equal end-to-end session
Exporter bytes, plus negative feature and range tests.

The same independently installed Crypto / Core graph now builds these
tests on Linux epoll, macOS kqueue and Windows MSVC alongside existing
real NativeIO tests. The source-instrumented Linux GmSSL, Crypto,
Core, and candidate CNet state all run under ASan/UBSan/LSan and
TSan. All tests are repeated 3x. Exact final SHA and run IDs are
required **before** marking M3.7 accepted.

**Production remains: original CNet + GmSSL::GmSSL;**
**vcpkg-cache gmssl 3.2.0#13 pin unchanged;** all migration PRs Draft.
