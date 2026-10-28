# qigao/crypto repository retirement — native source ownership

The canonical source home is now qigao/salts-core, with module-local
asn1/ and crypto/ directories. The separate qigao/crypto repository
becomes a historical donor only; it must not remain the active authority
for code, CI and future installed packages after migration completes.

Salts::ASN1 is the one independent BER/DER C11 codec, not a TLS engine.
Salts::Crypto is the one TLS and algorithms owner. SaltsCore's CMake
may assemble both locally when explicitly selected. It must not import
a second Crypto build, duplicate archives or silently retry other backends.

Source-assembly options:
  SALTSCORE_BUILD_ASN1=ON   build local codec without third-party TLS
  SALTSCORE_BUILD_CRYPTO=ON build local Crypto + ASN1 sources

Existing CNet still has a direct GmSSL implementation. Combining it with
the staged Salts::Crypto archive is disallowed until real production
cutover. The static Crypto also still contains GmSSL internally, so it
is NOT a third-party-free native TLS implementation yet.

RETIREMENT BLOCKERS:
- Transfer the 18 changed files of qigao/crypto Draft PR #43, including
  the real TLS auth source, Python mTLS fixtures, tests and patch inputs.
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
