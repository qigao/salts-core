# Crypto-specific ASN.1 formats

This module tests **only** X9.62 ECDSA DER signature encoding/decoding and
RFC 8410 X25519/Ed25519 SPKI/PKCS#8 structures.

General-purpose ASN.1 Reader/Validator/Encoder, borrowed TLVs, OID and owned
Value Tree live **in the same repository** under `asn1/`, not `crypto/`.
Crypto imports its one static `Salts::ASN1` target and maintains no local
generic BER/DER source, shared legacy ASN.1 facade or second parser.

The root CMake project builds both modules in dependency order. A standalone
`cmake -S crypto` build requires an independently installed
`Salts::ASN1` CMake package. Its `Salts::Crypto` static archive only
contains key-format and cryptographic/TLS code; the ASN1 sibling is linked
and exported as a package dependency. The installed C11/C++17 consumers
must use the real `CryptoConfig`/ `ASN1Config` imports.
