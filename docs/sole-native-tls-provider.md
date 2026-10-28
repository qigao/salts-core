# Sole native TLS Provider cutover — blocked until feature parity

**Authoritative source:** [qigao/crypto](https://github.com/qigao/crypto)
(master 57e03d9ebefc3b9348764edaddb834862e669a45).
**CNet baseline:** salts-core/master 012fa1a5effc956ba764509034b3c265d9b9aa79.
**Upstream blocking issue:** [Crypto #44](https://github.com/qigao/crypto/issues/44)
(no third-party TLS engine at all); [Crypto #18](https://github.com/qigao/crypto/issues/18)
(CNet one-provider cutover); [Crypto PR #43](https://github.com/qigao/crypto/pull/43)
(Draft versioned REQUIRED mTLS authentication).

## Two different, both mandatory boundaries

1. **Public/physical owner:** exactly one STATIC Salts::Crypto, with
   independent first-party Salts::ASN1 for ASN.1 BER/DER. CNet imports
   *that one target* and must not compile gmssl headers, link
   GmSSL::GmSSL directly, load an alternate provider, or duplicate a static
   Crypto inside multiple plugin/consumer DSOs. Salts::CNet is a single
   shared owner that embeds the one Crypto archive.
2. **Underlying implementation:** Crypto's current
   crypto/src/crypto_tls_gmssl.c and GmSSL 3.2.0#14 still ARE a third-party
   TLS engine, even when merged inside the STATIC Salts::Crypto archive.
   To satisfy **no third party**, replace this with an implemented,
   independently qualified native TLS/TLCP engine. Rename or link changes
   do not meet this requirement. Do not introduce a no-op backend.

Neither boundary is complete on the cited master. The copied
asn1/ and crypto/ modules are first-party source from qigao/crypto, but the
SaltsCore root CMake does not build them. The Crypto master copy still
lacks crypto/include/crypto/tls_auth.h (present only in Draft Crypto #43).
The current CNet product target still includes gmssl and directly links
GmSSL::GmSSL. An explicit test-only crypto_tls_qualification.c exercises
a subset of standard TLS, not the production replacement.

## Feature-by-feature production requirements (nothing dropped)

| Existing CNet feature | Sole native Crypto acceptance |
| --- | --- |
| TLS 1.2, TLS 1.3 and default negotiated range | Pending full parity; test-only fixed version is insufficient |
| ShangMi TLS 1.3 and TLCP 1.1 (dual sign/encrypt identities) | Missing Crypto opaque profile, **BLOCK** |
| RSA/ECDSA/P-384/SM2, SM3/SM4 and key schedule | Require native wire and certificate parity |
| Explicit CA file/path, platform system anchors and verify depth | CA file subset only; path/system trust **BLOCK** |
| DNS/IP SAN, independent SNI vs identity, strict trust rejection | DNS SAN subset only; IP/identity **BLOCK** |
| Client cert+key, REQUIRED server-side mTLS and client EKU | Crypto #43 Draft, CNet full Owner proof incomplete |
| Full binary ALPN including negotiation | Text ALPN subset only, **BLOCK** |
| Peer chain/fingerprint, RFC 5929 server-end-point, RFC 9266 exporter | Incomplete exact semantic parity, **BLOCK** |
| Cancellation, NativeIO buffer retire, close_notify vs raw TCP EOF, TLS fatal alert | Need real epoll/kqueue/IOCP cross-owner race/alert evidence |
| One physical TLS Crypto owner across DLL/DSO/plugins | Test candidate evidence is necessary but not release proof |
| No GmSSL/other third-party TLS engine in Crypto runtime | **BLOCK** — current Crypto CMake includes it |

Original CNet public API and behavior are non-negotiable. Unsupported
profiles must NOT silently switch to plaintext, retry with another engine,
disable authentication or discard an API. Keep the prior production source
intact **in master only until** the full native implementation is validated.
The final production cutover must remove the direct third-party TLS path
without a runtime fallback.

## Machine-verifiable source gate

Run:

    python3 -m unittest discover -s tests -p 'test_single_tls_owner.py' -v
    python3 tools/audit_single_tls_owner.py --report

The REPORT is intentionally **NOT a cutover PASS**. The release gate is:

    python3 tools/audit_single_tls_owner.py

and MUST return nonzero today. It demands one Salts::Crypto link in CNet,
no GmSSL/other TLS backend in production source or CMake, a native static
Crypto source owner and versioned session/auth/peer/BIO interfaces.

Even after the SOURCE gate passes, release additionally requires the
independent Python TLS peer, true TLCP/ShangMi interop, full TLS/mTLS
security-negative matrix, original CNet API/ABI and 3-OS/native/mobile
ownership tests, sanitizers, exact-head archive and multi-DSO identity.
No publication or merge based on source scanning alone.
