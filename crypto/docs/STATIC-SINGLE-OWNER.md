# Canonical two-module native source ownership

**The SaltsCore source tree contains two independently consumable C11
modules**, each with exactly one public static archive and canonical
`Salts::XXX` target:

| Module | CMake target | Native implementation | Link dependencies |
| --- | --- | --- | --- |
| `asn1/` | `Salts::ASN1` | BER/DER TLV, OID, strict validator, bounded owned Value Tree | C11 |
| `crypto/` | `Salts::Crypto` | Locked local TLS/algorithms, Ed448, TLS/CipherBIO, ECDSA DER and RFC 8410 formats | `Salts::ASN1` + native system libraries |

Plain SNMP and LDAP applications can link only `Salts::ASN1`; no GmSSL,
CMeta, Core or Crypto is pulled in. The Crypto archive does not compile or
reexport a second generic BER/DER implementation.

The GmSSL-derived native implementation is locked under
`crypto/gmssl-extensions/`; its source record also identifies the exact Core
extension inputs under `crypto/gmssl-extensions/`. GmSSL's external vcpkg-native and local Ed448
libecc OBJECT targets compile locally and enter `libsalts_crypto.a` / the
Windows equivalent exactly once. There is no imported GmSSL archive,
post-build archive merge, or separate installed native provider target.
`crypto/tools/check_local_native_source.py` verifies the raw input bytes.

Production `Salts::CNet` links this archive privately. A static archive can
still be copied into multiple final images, so an installed archive count
alone does not prove one physical runtime owner. The production CNet link-map
audit and the additional two-plugin qualification graph retain that separate
obligation; consumers must not add another Crypto implementation to the same
process and call it unified ownership.

Install the root project to produce two sibling packages:
`lib/cmake/ASN1/ASN1Config.cmake` and
`lib/cmake/Crypto/CryptoConfig.cmake`. Crypto declares
`find_dependency(ASN1)` and exports no private GmSSL target. The
`crypto/tools/audit_static_crypto.py` check confirms one archive per
module and rejects generic ASN.1 definitions inside the Crypto archive.
Standalone Crypto installations can pass `--asn1-prefix` to the auditor.

The source lock preserves the reviewed native input and patch provenance;
local ownership does not remove the GmSSL or libecc attribution. No fallback
provider or alternate namespace is introduced. Exact-current-source
C11/C++17, installed SDK, sanitizer, runtime owner and consumer wire tests
still gate production release.
