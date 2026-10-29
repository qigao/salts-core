# X.509 read-only structural inspection using the canonical ASN.1 codec

`crypto_x509_der_inspect()` uses the provider-neutral sibling
`Salts::ASN1` to validate a **single strict DER** Certificate and return
borrowed views of its tbsCertificate, positive serial, issuer, validity,
subject, SPKI, ECDSA/RSA signature algorithm OID, signature bytes and
optional X.509 extensions.

`crypto_x509_der_chain_next()` iterates **concatenated DER certificates**.
It directly accepts the caller-owned bytes copied through the existing
`crypto_tls_session_copy_peer_chain_v1_2()` API after a completed TLS
handshake; it never borrows GmSSL's internal TLS/session storage. No
second BER/DER parser is compiled into Crypto.

This module **does not** decide trust, issuer chain path, hostname/SAN,
certificate time validity, signature verification, revocation or key usage
authorization. GmSSL remains the certificate-chain security authority in
TLS and all handshake and exporter state machines remain untouched.

Negative source-only tests reject incomplete DER, trailing bytes, malformed
algorithm identifiers, invalid serials, malformed DER dates, BIT STRING
unused-bit errors and invalid certificate versions without modifying caller
outputs. They use the repository's real P-256 and P-384 SHA-384 PEM fixture
certificates decoded **inside the test program only**, so this adds no PEM,
base64 or OpenSSL parsing API to production.

Standalone source CI can compile `crypto/src/crypto_x509_der.c` with
`Salts::ASN1` while leaving GmSSL unavailable; the full static Crypto SDK
and TLS peer interoperability must still be accepted separately.

## Removed SaltsNet ASN.1-side X.509 API

The former `qigao/salts-net/snmp/asn1/include/asn1/x509_cert.h` and
`src/x509_cert.c` were NOT retained as duplicate ASN.1/X.509 implementations.
The reviewed 951-byte original self-issued RSA certificate DER test fixture
is now checked in here, and Crypto exposes audited read-only metadata:
`crypto_x509_der_common_name` (length-bounded borrowed issuer/subject CN)
and `crypto_x509_der_get_public_key_kind` (RSA/EC/Ed25519 OID classification).
The old owned `x509_cert_t` allocation tree, `x509_cert_is_self_signed`
(which only compared names without verifying a signature), locale-dependent
`mktime` validity decisions, and old `SALTSNET_ASN1_C_API` are retired,
not emulated. This is an intentional clean API break, not a compatibility
wrapper. TLS authentication and certificate trust remain solely GmSSL-owned.
