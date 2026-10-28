# M3.2.1 — Independent OpenSSL 3 same-session TLS exporter oracle

**Draft qualification only. Production Crypto and CNet do not link
OpenSSL, and no native shared-provider ownership model is changed.**

## Provenance of the oracle

The original TLS 1.2/1.3 independent OpenSSL3 peer contracts already
prove that an **installed Crypto C11 client** can complete a real
authenticated handshake, server SAN/trust validation, `h2` ALPN,
the exact peer DER/SHA-256 oracle, and HTTP application data against
an independently implemented OpenSSL 3 server.

This branch adds a TEST-ONLY reference server compiled from
`tests/sdk/openssl-exporter-oracle.c`, which links only
`OpenSSL::SSL` / `OpenSSL::Crypto`, **never Salts::TLS or GmSSL**.
The Crypto C11 client still links only the single imported native
`Salts::TLS` shared image, **never the OpenSSL library**.

The reference server uses the real OpenSSL 3
`SSL_export_keying_material()` implementation after `SSL_accept()`.
The Crypto client uses `crypto_tls_session_export_v1_2()` after its
real authenticated handshake. They use exactly the **same TCP/TLS
connection** (one server accepts one client), the same label
`EXPORTER-Channel-Binding` and 32-byte output. Both implementations
independently derive TWO context forms **from the same TLS session**:
an omitted context (`use_context=0`) and the CNet RFC 9266
explicitly-present empty context (`use_context=1, context_len=0`).
OpenSSL 3 `SSL_export_keying_material()` provides the independent
oracle for BOTH forms; the Crypto facade provides them through
`crypto_tls_session_export_v1_2()`. TLS 1.2 must produce distinct
values for these two forms, while TLS 1.3 produces equal values in
accordance with RFC 8446. Never conflate the TLS 1.2 semantics.

The test server advertises the existing RSA/ECDHE GCM TLS 1.2 or
RSA-PSS/AES-GCM TLS 1.3 profile and selects `h2` from the client ALPN
offered list, then replies to an actual HTTP request.
Both producer processes independently write their 32 derived bytes
as exactly 64 uppercase hex characters into private, exclusive
0600 `.secret` files, **only after valid TLS authentication**.
The script requires strict **byte-for-byte** matches for BOTH
context forms and a completed TLS/HTTP exchange for BOTH protocols. Never print, export,
artifact-upload, reuse or snapshot raw exporter material.
Every failure exits nonzero; no skip, xfail, suppression or retry-to-pass.

## Reverse-direction finding remains separate

The earlier experimental `tests/sdk/openssl-server.c` Crypto-server
vs `openssl s_client -keymatexport` direction failed **before**
the Crypto server completed a TLS 1.2 handshake, inside the existing
GmSSL TLS 1.2 ClientKeyExchange/Finished parsing path. OpenSSL
certificate verification and local exporter availability at the
client do not establish that the server authenticated/finished.

This reverse-direction incompatibility is **not considered fixed**
by the reference test above. The experimental test source remains
available and is explicitly tracked in [Crypto #23](https://github.com/qigao/crypto/issues/23)
for a real provider/server-side fix and independent fail-closed
qualification. It cannot be used
as evidence for production CNet server parity.

## CI / release boundary

- Linux/macOS independent installed Crypto C11 client vs OpenSSL 3
  `SSL_export_keying_material` test server.
- Linux real GmSSL and shared Crypto source, installed facade and
  test-only OpenSSL reference executable under ASan/UBSan/LSan.
- Existing three-OS SDK, original 29 test matrix, single native owner,
  host DSO cancellation/TSan, ALPN/CA/SAN and peer DER/SHA-256 tests
  remain required.
- No production OpenSSL fallback, no source provenance mutation,
  no changes to `gmssl 3.2.0#13` vcpkg pin or CNet linkage.
