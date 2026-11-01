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

Salts::Crypto is the one TLS and algorithms owner in the default Core graph.
The locked local `crypto/native/gmssl/` implementation and private libecc
objects are compiled directly into its single static archive. No installed
GmSSL prefix or imported provider archive is required. The independent
`crypto/` configure uses the same local sources and requires only the sibling
installed ASN1 package and native system dependencies.

Production CNet retains its original GmSSL API calls and TLS behavior, but
resolves them through the same local Salts::Crypto archive. It does not
compile another native TLS implementation. This is a source/build ownership
change; the static Crypto still contains the GmSSL-derived engine and is
NOT a third-party-free native TLS implementation. The additional
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
- Qualify the actual locked local TLS/TLCP implementation, including its
  third-party provenance, on every supported platform; local compilation
  alone does not prove a different engine or completion of a TLS rewrite.
- Preserve TLS1.2/1.3, ShangMi, TLCP, CA/SAN, mTLS, full binary ALPN,
  RFC5929/9266, alert/EOF and bounded NativeIO ownership semantics.
- Migrate all other active consumers, workflow inputs and release pins.
- Run exact-head Linux/macOS/Windows CI, sanitizers and installed C11/C++17
  tests, plus one physical owner across plugins and process DLL/DSOs.

Only then should the former qigao/crypto GitHub repository be ARCHIVED
read-only. Do not delete it or lose unmerged source; no release yet.

See qigao/crypto PR #43 and Issue #44, salts-core PR #74 and Issue #71.
