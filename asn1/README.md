# ASN.1 C11 module

The canonical ASN.1 protocol module belongs to `qigao/crypto/asn1/` in
the same repository as `crypto/`. It is independently buildable and does
**not** include or link GmSSL, SaltsCore, CMeta, CFlow or Crypto.

`Salts::ASN1` is one **static** C11 library, exporting
`<asn1.h>` and `<asn1_value.h>` with `salts_asn1_*` APIs:
bounded BER/DER Reader, Validator, Encoder, borrowed TLV views, OID and
opaque owned Value Tree. On successful child admission, the parent owns
the child; on error it remains with the caller.

This module is used by both `Salts::Crypto` (signature/key formats) and
`SaltsNet` (SNMP, LDAP). ASN.1 MIB schema-language compilation is
currently a separate protocol tooling concern, not a duplicate binary BER
parser. Certificate trust verification stays with its crypto/TLS owner.

To build this module alone:

```sh
cmake -S asn1 -B build-asn1 -DBUILD_TESTING=ON
cmake --build build-asn1 --parallel
ctest --test-dir build-asn1 --output-on-failure
cmake --install build-asn1 --prefix "$PWD/asn1-sdk"
```

No separate installed-consumer project is maintained. Public C11/C++17
headers and behavior are covered by the module's formal source-tree tests.
No old `crypto_asn1_reader_*`, `SaltsCore::ASN1` or dynamically exported
`Salts::ASN1` compatibility facade is retained. The original strict
parser implementation is transferred from the qualified independent
export candidate, without semantic or ownership edits.
