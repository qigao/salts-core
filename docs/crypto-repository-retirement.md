# qigao/crypto repository retirement — native source ownership

The intended canonical source home, pending acceptance and merge, is
qigao/salts-core, with module-local
asn1/ and crypto/ directories. The separate qigao/crypto repository
becomes a historical donor only; it must not remain the active authority
for code, CI and future installed packages after migration completes.

Salts::ASN1 is the one independent BER/DER C11 codec, not a TLS engine.
The SaltsCore root always assembles and installs it; it is not controlled
by a build option. Its C/C++ module tests run in a standalone ASN1 configure
so C-only root and cross-compiler profiles do not require C++.

Salts::Crypto is the one TLS and algorithms owner. Its local CMake module
is assembled when the caller supplies `CRYPTO_GMSSL_ROOT`, the exact installed
pinned static GmSSL prefix currently needed by that implementation.
This is a required dependency path, not a source-owner build switch.
Without it, the root continues building its provider-neutral modules;
it neither creates a placeholder Crypto target nor silently picks another
TLS backend. The standalone crypto/ package still enforces its own
required dependency and is not replaced by another source repository.

Existing CNet still has a direct GmSSL implementation. Combining it with
the staged Salts::Crypto archive is disallowed until real production
cutover. The static Crypto also still contains GmSSL internally, so it
is NOT a third-party-free native TLS implementation yet.

RETIREMENT BLOCKERS:
- The 18 changed inputs of qigao/crypto Draft PR #43 have now
  been staged in this migration branch: 17 byte-exact Git blobs and one
  adapted module-ownership checker. They must still pass real TLS/mTLS
  native tests before retirement. Their original GmSSL-backed workflow
  is preserved as a historical input, not a production approval.
- Replace the actual third-party TLS engine inside Crypto (not merely
  the public link target) with a fully qualified native TLS/TLCP engine.
- Preserve TLS1.2/1.3, ShangMi, TLCP, CA/SAN, mTLS, full binary ALPN,
  RFC5929/9266, alert/EOF and bounded NativeIO ownership semantics.
- Migrate all other active consumers, workflow inputs and release pins.
- Run exact-head Linux/macOS/Windows CI, sanitizers and installed C11/C++17
  tests, plus one physical owner across plugins and process DLL/DSOs.

Only then should the former qigao/crypto GitHub repository be ARCHIVED
read-only. Do not delete it or lose unmerged source; no release yet.

See qigao/crypto PR #43 and Issue #44, salts-core PR #74 and Issue #71.
