# M1: CNet production uses sole local Salts::Crypto

Tracking: [salts-core #83](https://github.com/qigao/salts-core/issues/83).
Parent [#76](https://github.com/qigao/salts-core/issues/76).
Stage 2 final native-TLS audit remains [Draft #74](https://github.com/qigao/salts-core/pull/74).

## Two non-interchangeable acceptance gates

**M1 (this gate):** production `Salts::CNet` must use the one locally
compiled `Salts::Crypto` static owner, through opaque TLS/CipherBIO, not
`GmSSL::GmSSL` or a test-only qualification target. Local first-party
`Salts::ASN1` owns ASN.1. Exactly ONE private GmSSL TLS engine is permitted
*inside* Crypto while this first migration is in progress. Never compile a
native engine next to the GmSSL engine, keep a fallback, dynamically select a
provider or change the original public CNet ABI.

**M2 (strict final #74):** the GmSSL TLS engine, its headers, objects and
link inputs must also disappear from `Salts::Crypto`. Replace handshake,
record protection, key schedule and cipher-suite logic with native C11
TLS1.2/TLS1.3/ShangMi/TLCP using the existing first-party ASN1/X.509 and
cryptographic algorithms. No second TLS engine even during transition to a
production image. All M1 tests remain mandatory.

## Source gate vs functional acceptance

Run `python3 -m unittest discover -s tests -p test_cnet_crypto_stage1.py -v`,
then `python3 tools/audit_cnet_crypto_stage1.py` (STRICT and expected RED
until production source actually changes). `--report` exits zero to print
blockers only and may never be treated as an acceptance check. Source shape
is necessary, **never sufficient** for claiming M1 complete.

Only close M1 after one **exact HEAD** also passes all previously required
original CNet public tests and installed C11/C++17 SDK/ABI cases; TLS1.2/1.3
default and explicit versions; ShangMi TLS1.3/TLCP1.1, dual SM2 identities;
CA file/directory/system roots; DNS and IP SAN, SNI distinct from verified
identity, REQUIRED server-side mTLS/client EKU; binary ALPN; fingerprint
and peer DER-chain lifetime; RFC 5929 and 9266 bindings; close_notify and
fatal alert vs unauthenticated EOF; reject tests (no trust downgrade);
and true NativeIO epoll/kqueue/IOCP cancellation/teardown, plugin/DSO physical
owner uniqueness and sanitizers. Independent Python TLS peers and genuinely
independent TLCP/ShangMi interop are required. Existing mobile runtime gates
remain in force. Check actual compiler/link/archive graph, not just strings.

Stage 1 can be approved while GmSSL remains *private* in Crypto, but must
never be called a first-party native TLS/TLCP completion. No releases or
merges based only on this source test; no fallback, alternative TLS engine
or suppressed original test failures.

## Current baseline

Source branch: [Draft #80](https://github.com/qigao/salts-core/pull/80)
at `9e948e737e772706de2d68ec561a983f75841d15`.
Actual CNet is still GmSSL-backed and the original CNet public interface
has not been proven on opaque Crypto. The separate strict native engine
admission remains blocked for intentional, factual reasons.
