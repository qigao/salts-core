# Crypto TLS v1.1 extension — bounded text ALPN and negotiated inspection

This is a **candidate-only C11 ABI extension** for the single shared
GmSSL-bearing Crypto image from Draft PR #19. It addresses the first
bounded slice of [Crypto #18](https://github.com/qigao/crypto/issues/18)
and is **not** a complete SaltsCore CNet replacement.

## Compatibility and admission

- The original `<crypto/tls.h>` / `CRYPTO_TLS_ABI_VERSION=0x00010000`
  / original 11 exported symbols and opaque handle layouts are untouched.
  Old compiled C11 consumers still pass the original version marker.
- New installed `<crypto/tls_ext.h>` defines an *independently checkable*
  `CRYPTO_TLS_EXTENSION_ABI_VERSION=0x00010001`, queryable through
  `crypto_tls_extension_abi_version()`. Four additional exports reside in
  the SAME shared library (15 TLS + 3 SM3 total = **18 exact exports**).
  The host-only `Salts::TLSHost` validates all 15 TLS exports and the
  extension version before exposing the v1.1 function table; invalid or
  partial DSO tables fail closed.
- `crypto_tls_get_features_v1_1()` reports explicitly supported options:
  TLS 1.2 ECDHE-RSA, TLS 1.3 RSA-PSS, explicit CA file, DNS SAN,
  printable ASCII ALPN and negotiated protocol/cipher readback.
  **No** automatic system trust, CA directory, arbitrary binary ALPN,
  certificate-chain/fingerprint, exporter/channel binding, mutual TLS
  policy, ShangMi/TLCP or DTLS capability is reported.

## Owned ALPN storage

`crypto_tls_session_set_alpn_v1_1(session, names, count)` is legal
exactly once **before** the first handshake attempt. It deep-copies 1–8
printable ASCII, NUL-terminated protocol labels of length 1–127 into
private, fixed-size session storage, including the pointer list borrowed
by the GmSSL `TLS_CTX`. The caller may release its input list and strings
immediately after configuration.

The total wire-name budget is **128 bytes**, intentionally stricter
than RFC 7301's theoretical 65535-byte list and a single 255-byte
name: the original GmSSL 3.2 ClientHello extension builder uses a
fixed 512-byte buffer shared by other TLS extensions. Before growing
the public ALPN budget for CNet, the upstream builder must provide
a separately audited capacity-aware serializer with exact failure tests.
An oversize/duplicate/empty/invalid name fails with no published
partial configuration. Server preference comes from the original GmSSL
source adaptation and is checked by actual TLS 1.2/1.3 handshake tests.

## Copy-only authenticated inspection

After handshake is successfully complete,
`crypto_tls_session_get_info_v1_1(session, &info)` returns a copied,
size-marked record of the negotiated IANA cipher ID, cipher name,
TLS 1.2/1.3 protocol and selected ALPN (zero when none selected).

Before handshake completion the call reports AGAIN and **does not
modify** the caller's output. Concurrent TLS drive returns BUSY;
cancelled sessions return CANCELLED. No native `TLS_CTX`,
`TLS_CONNECT` or pointers into session-owned GmSSL strings leave
the shared module, and the selected ALPN must match one of the
private configured labels.

## Evidence and limitations

- The independent installed C11 SDK repeats real TLS 1.2/1.3
  handshakes both with and without ALPN, with client and server
  preference orders intentionally reversed and bounded partial I/O.
  Tests verify protocol/cipher readback, selected `h2`, pre-handshake
  output preservation, invalid inputs and post-start mutation refusal.
- An independent OpenSSL 3 server advertises `h2,http/1.1` and
  completes TLS 1.2/1.3 with the installed Crypto facade selecting
  `h2`, preserving strict authenticated HTTP and wrong-CA/SAN refusal.
- The same real GmSSL source, shared facade, host DSO leases and
  C11/C++17 two-module owner tests remain under the installed,
  ASan/UBSan/LSan and TSan qualification workflows.

**Not yet complete:** CNet requires wider ALPN bounds, TLS version
ranges, CA paths/native system stores, client certificate policies,
ShangMi/TLCP, exporter/channel binding, peer cert chain/fingerprint,
and true CNet/NativeIO pull/push buffer semantics. No production
CNet or vcpkg registry pin change is made here.
