# Canonical two-module native source ownership

**The single qigao/crypto repository contains two separately built C11
modules**, each with exactly one public static archive and canonical
`Salts::XXX` target:

| Module | CMake target | Native implementation | Link dependencies |
| --- | --- | --- | --- |
| `asn1/` | `Salts::ASN1` | BER/DER TLV, OID, strict validator, bounded owned Value Tree | C11 |
| `crypto/` | `Salts::Crypto` | SHA256, Ed448, TLS/CipherBIO, ECDSA DER and RFC 8410 formats | `Salts::ASN1` + GmSSL |

Plain SNMP and LDAP applications can link only `Salts::ASN1`; no GmSSL,
CMeta, Core or Crypto is pulled in. The Crypto archive does not compile or
reexport a second generic BER/DER implementation.

The one GmSSL provider itself is built by the exact
`qigao/vcpkg-cache/ports/gmssl` pin. Additional extension input resides
at the externally pinned `qigao/crypto/vendor/gmssl-extensions/` and is
compiled by vcpkg-cache only once. The Crypto module builds only its vetted
native facade and its private Ed448 libecc objects in
`crypto/vendor/libecc/`; it imports the exact GmSSL static input at
configuration and combines its native objects once into
`libsalts_crypto.a`/the Windows equivalent.

Install the root project to produce two sibling packages:
`lib/cmake/ASN1/ASN1Config.cmake` and
`lib/cmake/Crypto/CryptoConfig.cmake`. Crypto declares
`find_dependency(ASN1)` and exports no private GmSSL target. The
`crypto/tools/audit_static_crypto.py` check confirms one archive per
module and rejects generic ASN.1 definitions inside the Crypto archive.
Standalone Crypto installations can pass `--asn1-prefix` to the auditor.

Historical GmSSL production pins remain unchanged; no transitive ABI
compatibility bridge, fallback provider, SaltsCore dependency or alternate
namespace is introduced. The exact module source C11/C++17, installed
SDK, sanitizer and consumer wire tests gate production release.
