# qigao/crypto repository retirement — native source ownership

The intended canonical source home, pending acceptance and merge, is
qigao/salts-core, with module-local
asn1/ and crypto/ directories. The separate qigao/crypto repository
becomes a historical donor only; it must not remain the active authority
for code, CI and future installed packages after migration completes.

Salts::ASN1 is the one independent BER/DER C11 codec, not a TLS engine.
The SaltsCore root always assembles and installs it; it is not controlled
by a build option. Its independent C/C++ module tests also run in a standalone
ASN1 configure, separately from the default Core graph.

Salts::Crypto is the canonical TLS/X.509 service facade in the default Core
graph. Its static archive contains its own C11 wrapper/Ed448 sources and links
the versioned `GmSSL::GmSSL` static archive delivered by vcpkg, rather than
duplicating a checked-in GmSSL native source build. The original pinned
extension C/H inputs live under `crypto/gmssl-extensions` and are staged
by the vcpkg-cache port with a hash-verified immutable source checkout.

Production CNet retains its original GmSSL TLS API use temporarily; it now
links the same imported vcpkg GmSSL archive along with Salts::Crypto. This
single package source does not, by itself, prove one physical process-wide
provider across multiple independently linked DSO images. The original
native/TLS/DSO negative checks remain mandatory. Removing direct private
GmSSL CNet calls is a separate semantic gate; no additional provider fallback
is added. The additional
`SALTSCORE_TEST_CNET_CRYPTO_QUALIFICATION` graph is test-only and cannot
substitute for real production CNet, installed SDK or runtime owner evidence.

RETIREMENT BLOCKERS:
- The 18 changed inputs of qigao/crypto Draft PR #43 have now
  been staged in this migration branch: 16 byte-exact active inputs, one
  adapted module-ownership checker, and one reviewed local-build adaptation.
  The original Crypto CMake input remains byte-exact in a snapshot, and a
  pinned patch must reconstruct the active CMake file. They must still pass real TLS/mTLS
  native tests before retirement. Their original GmSSL-backed workflow
  is preserved as a historical input, not a production approval.
- Qualify the pinned vcpkg TLS/TLCP implementation and its exact port/source
  provenance on every supported platform. An imported package alone does not
  prove an independent engine or completed TLS rewrite.
- Preserve TLS1.2/1.3, ShangMi, TLCP, CA/SAN, mTLS, full binary ALPN,
  RFC5929/9266, alert/EOF and bounded NativeIO ownership semantics.
- Migrate all other active consumers, workflow inputs and release pins.
- Run exact-head Linux/macOS/Windows CI, sanitizers and installed C11/C++17
  tests, plus one physical owner across plugins and process DLL/DSOs.

Only then should the former qigao/crypto GitHub repository be ARCHIVED
read-only. Do not delete it or lose unmerged source; no release yet.

See qigao/crypto PR #43 and Issue #44, salts-core PR #74 and Issue #71.
