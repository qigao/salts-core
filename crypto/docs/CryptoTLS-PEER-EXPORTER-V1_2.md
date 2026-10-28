# Crypto M3.2 — TLS peer DER, SHA-256 and exporter (Draft)

**Qualification-only C11 API extension, not a production CNet cutover.**
This branch builds on [M3.1 ALPN v1.1](https://github.com/qigao/crypto/pull/20) and
the [single physical SM3/TLS owner](https://github.com/qigao/crypto/pull/19).

## Additive C11 epoch

The installed `<crypto/tls_peer.h>` publishes an independent
`CRYPTO_TLS_PEER_ABI_VERSION=0x00010002` admission marker. It neither
changes `<crypto/tls.h>` v1.0 nor the `<crypto/tls_ext.h>` v1.1 ABI.

Five additional functions appear in the **same** shared `crypto_tls`
image: version, feature flags, copied DER chain, first peer certificate
SHA-256, RFC 5705/8446 keying material. The native export list is
**23 exact Crypto names** (3 SM3 + 11 TLS v1 + 4 TLS v1.1 + 5 v1.2).
`Salts::TLSHost` validates all 20 TLS functions and every extension
marker before publishing a dynamic library lease.

## Certificate ownership

`crypto_tls_session_copy_peer_chain_v1_2()` returns GmSSL's
concatenated DER certificate representation **by value**, never a
pointer to `TLS_CONNECT`. A size-only query passes `out=NULL,
capacity=0`. A too-small buffer returns
`CRYPTO_TLS_PEER_BUFFER_TOO_SMALL`, reports required capacity, and
leaves the buffer bytes untouched. Other failures leave the buffer
and size result untouched. The accepted chain length is at most 4096
bytes (the real GmSSL per-session certificate storage limit).
The caller must independently parse and validate certificate data
if it needs individual certificates or policy decisions.

`crypto_tls_session_peer_sha256_v1_2()` returns a **32-byte raw**
SHA-256 of the first DER peer certificate. It is **not** RFC 5929
`tls-server-end-point`, which requires choosing a digest based on the
certificate's signature algorithm. Do not conflate the two in CNet.
Missing peer certificates (e.g. default server without client auth)
return a distinct `CRYPTO_TLS_PEER_CERT_ABSENT` status.

## Exporter and bounded memory

`crypto_tls_session_export_v1_2()` uses the exact private GmSSL 3.2
TLS 1.2 or 1.3 key schedule. No OpenSSL production fallback.
Both the label and output are bounded: printable ASCII label
1–127 bytes; context up to 1024 bytes; output 1–64 bytes.
Only `use_context=0` and `1` are accepted. Key material is
derived into a temporary private stack buffer, copied to the caller
only on success, then erased. All failures leave caller output
unmodified.

CNet's RFC 9266 channel binding call is represented by:

`label="EXPORTER-Channel-Binding", context=NULL, context_len=0,
use_context=1, output_len=32`.

Client and server running the same true TLS session must derive
identical results; a different label/context must change them. For
TLS 1.2, the omitted context and present-but-empty context produce
different exporter values. TLS 1.3 follows RFC 8446 empty context
hash behavior and makes these two representations equivalent.

An exporter binds to a cryptographic TLS session; it does **not**
implicitly establish client-certificate trust or an application user
identity. GmSSL peer trust and SAN validation are independent.

## Acceptance and release boundary

- Independent installed C11 TLS 1.2/1.3 client and server tests:
  full DER copy and too-small-buffer nonmutation, known frozen RSA
  server certificate SHA-256 oracle, absence of client cert,
  pre-handshake refusal, caller-owned exporter output and rejection
  of invalid parameters. Actual 32-byte exporter values must match
  between the two real peers and be nonzero.
- Independent OpenSSL 3 true TCP TLS 1.2/1.3 peers: the public
  Crypto facade compares exact leaf DER and SHA-256 to the OpenSSL
  X.509 fixture oracle, derives channel-binding material, negotiates
  ALPN `h2`, delivers authenticated HTTP and still refuses bad
  CA/SAN. Private keys and exporter material are never uploaded or
  logged.
- Linux/macOS/Windows final installed and *physically relocated*
  C11/C++17/two-MODULE tests must prove exactly **23** public symbols
  and one GmSSL-bearing shared image; native Host loader rejects
  incomplete or wrong-version dynamic ABI.
- Full original source, native ASan/UBSan/LSan and callback-in-flight
  TSan must remain enabled with no suppressions or lowered test counts.

**Still missing for complete CNet parity:** native CA paths/system
stores, P-384/ECDSA, SM2/ShangMi/TLCP, client cert verification and
mTLS policy, RFC 5929 server-end-point signature-aware digest, full
CNet/NativeIO cipher BIO and completion/cancellation barriers,
full independent external mTLS peer, and cross-repository runtime
single-owner verification. [Crypto #18](https://github.com/qigao/crypto/issues/18)
remains OPEN. Neither production CNet nor vcpkg-cache
`gmssl 3.2.0#13` pin is changed.
