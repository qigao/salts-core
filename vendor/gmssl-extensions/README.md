# GmSSL extension source ownership

This directory is the single repository home for the additional C11
implementations used to build the vcpkg GmSSL port: MD5 (legacy protocol
interoperability only), DES-CBC (legacy protocol interoperability only),
AES Key Wrap (RFC 3394), bounded RSA encodings/private operations and
P-384 signature verification. The private TLS1.3 handshake reader is a
source-inclusion header, not an installed public interface.

The exact 13 original Git blobs were moved from
`qigao/vcpkg-cache/ports/gmssl`, preserving their original bytes. All
sources are Apache-2.0 as previously distributed with the GmSSL port.
The original upstream of the P-384 verifier is
`guanzhi/GmSSL@24ae482701a7b124826c382fffc55c19f76d475d`.

**Integration boundary:** the vcpkg-cache GmSSL port fetches an exact
pinned qigao/crypto commit using `vcpkg_from_git` during source
assembly and stages the source files into its separately pinned
`guanzhi/GmSSL` checkout. The vcpkg port compiles them into one static
GmSSL package; downstream `Salts::Crypto` merges that installed static
package's objects into its **sole** public `Salts::Crypto` archive.
These original sources MUST NOT also be compiled directly by Crypto's
CMake target, which would introduce duplicate definitions.

No generic Provider/DSO, second GmSSL library, public Crypto target
alias, or `providers/gmssl/` is added. This directory is an immutable
source input for GmSSL port assembly, not a separate build system.

GmSSL-specific packaging rules, the 4 ordered source patch phases,
port revision and conformance suite continue to belong to
`qigao/vcpkg-cache`; they contain no duplicate added C implementation
files. The original 3.2.0#14 port remains available in the version
registry. Its follow-up 3.2.0#15 source-ownership refactor must pass
the original contract tests, source sanitizer gates and TLS/CA/SAN
interoperability before acceptance.
