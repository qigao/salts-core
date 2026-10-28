# Crypto native module

`Salts::Crypto` is the cryptographic module of the **same**
[qigao/crypto repository](../README.md). It produces exactly **one
cryptographic static archive**; do not compile or install another GmSSL or
generic ASN.1 reader within this module.

`crypto/src/asn1/crypto.c` contains ECDSA DER SignatureValue and RFC 8410
SPKI/PKCS8 format logic. The canonical general-purpose BER/DER, OID, TLV,
reader and owned Value Tree are under [../asn1/](../asn1/), exported as
`Salts::ASN1` and consumed directly. All public APIs are `Salts::XXX`
CMake targets, C11 source and C++17-usable C headers.

Use the repo root for a combined build or `cmake -S crypto` to compile
just Crypto against a previously installed ASN1 module. GmSSL is taken
only from the exact installed vcpkg-cache port; the root
`vendor/gmssl-extensions/` input remains where the pinned port expects it.
Private Ed448 libecc objects live under `vendor/libecc/` in this module.
No backwards-compatibility alias, fallback or duplicate BER/DER owner.

Installed consumers require `find_package(Crypto 0.1 CONFIG REQUIRED)`
and link `Salts::Crypto`. This imports the sibling `Salts::ASN1` via
`find_dependency(ASN1)`. Direct protocol consumers should instead use only
`find_package(ASN1 0.1 CONFIG REQUIRED)` and `Salts::ASN1`.

## X.509 certificate inspection for TLS callers

The Crypto module also exposes read-only `<crypto/x509_der.h>`, whose
`crypto_x509_der_inspect` and `crypto_x509_der_chain_next` validate and
walk DER certificates through the one sibling `Salts::ASN1` codec. The
existing `crypto_tls_session_copy_peer_chain_v1_2` returns a bounded
caller-owned concatenated DER chain that can be consumed by this inspector
without changing GmSSL's authenticated TLS handshake or trust decisions.
See [X.509 DER scope](docs/X509-DER-ASN1-INSPECTION.md).
