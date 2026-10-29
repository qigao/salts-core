# Default local Crypto and CNet build

Date: 2026-10-29. Status: Draft source/build repair, not release admission.

## Change

Core now always assembles ASN1, Crypto, URI, the existing private CNet helpers,
and the complete production CNet target. `SALTSCORE_BUILD_CNET` and
`CRYPTO_GMSSL_ROOT` are retired inputs. Supplying either produces an explicit
configuration error, including an attempted `SALTSCORE_BUILD_CNET=OFF`.
There is no replacement external TLS prefix or provider selection flag.

`Salts::Crypto` is the single public static Crypto archive. It contains its
seven existing facade translation units, the existing 33-unit private
libecc Ed448 subset, and 84 locally compiled native units. The native source,
headers and license live in `crypto/native/gmssl`; its OBJECT target is not
exported or installed independently. No GmSSL package lookup, imported native
archive, post-build archive merge, provider DSO or duplicate public target is
part of this build.

Production `cnet/src/cnet_tls.c` remains unchanged. Its private native calls
link the local Crypto archive. It keeps the existing version/range handling,
standard and ShangMi profiles, CA/SAN checks, ALPN, certificate inspection,
close handling and original Owner/NativeIO lifetimes. This does not promote
the narrower `cnet_tls_crypto_qualification.c` adapter into production.
The renamed `SALTSCORE_TEST_CNET_CRYPTO_QUALIFICATION` option enables only the
additional isolated qualification tests and defaults to OFF.

The SDK exports Crypto and CNet by default and requires its own sibling ASN1
and Crypto packages. Crypto/ASN1/URI remain STATIC; CNet and the stateful
foundation modules remain SHARED. Installed C11 and C++17 Crypto, URI, CNet
and ABI-owner consumers cannot silently disappear when a target is missing.

## Inputs and behavior boundary

The native implementation retains its GmSSL ancestry and Apache-2.0 license.
The complete input and derivation record is in `source-lock.json` and
`upstream-to-local.patch`. The 166 source/header/license inputs reproduce
byte-for-byte from the fixed upstream source. The Core-owned native CMake
recipe has a separate reviewed blob identity; its actual common/platform
source lists and feature definitions must match the recorded profile.

The former production CNet registry selected `gmssl 3.2.0#13`. The shared
local owner uses the existing Crypto qualification's #14 hardening plus
Core's existing mTLS/record corrections. This difference is recorded
explicitly; the new backend is not claimed to be byte-identical to #13.

Full implementation instrumentation found one additional SM4 issue: an
S-box byte was promoted to signed `int` before shifting left by 24. The local
correction casts to `uint32_t` before the shift. It preserves the 32-bit
substitution and known-answer output while removing undefined behavior.
The original blob, corrected blob and reason are recorded separately as
`local_hardening`; the reproduction patch includes the actual change.

An additional optional-client-certificate probe exposed two TLS 1.2 failure
modes: the server requests a certificate but allows its omission, while the
client has no certificate and receives a nonempty CA-name list. The original
#13 implementation was rebuilt in an isolated temporary directory and tested
with the same unchanged external-I/O source and fixtures. Both before and
after implementations fail in the same issuer-certificate check. The other
six tested TLS 1.2/1.3 modes pass on both sides. The 16 raw executions remain
12 PASS / 4 FAIL / 0 skip; no `WILL_FAIL` or success reclassification was used.
These two specific behaviors are pre-existing. They are not silently fixed,
and this bounded comparison is not evidence of universal TLS equivalence.

## Build

Use CMake 3.21 or newer, a C11 compiler, and a C++17 compiler when testing.
The normal networking dependencies remain the canonical pinned static
c-ares and KCP libraries and SIMDe headers. URI uses the real host re2c tool.
The manifest now makes these networking dependencies part of the base SDK;
the empty `cnet` feature is retained only for existing manifest callers.

```sh
python3 -B crypto/tools/check_local_native_source.py
cmake -S . -B build -DBUILD_TESTING=ON \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/verified-vcpkg-toolchain.cmake
cmake --build build --parallel
ctest --test-dir build --output-on-failure --no-tests=error
cmake --install build --prefix /path/to/sdk
```

The active CI workflows share the pinned non-crypto prerequisite setup. All
23 workflows retain their existing 35 jobs and runner/matrix strategy,
native tests, sanitizer repetitions, installed/relocated consumers and
runtime ownership audits. Workflow syntax checking is not platform runtime
qualification. Windows/macOS require real current-commit CI results.

## Local verification

| Check | Observed result |
|---|---|
| Default Linux Core build with neither retired input | Complete build and install succeeded |
| Default SDK installed C11/C++17 consumers | 47/47 PASS |
| Full Linux Core source graph, including existing optional modules and M5/M6 | 164/164 PASS |
| Full SDK installed consumers | 53/53 PASS |
| Full SDK consumers after moving the installation and removing its original path | 53/53 PASS |
| Instrumented local native Crypto/CNet, original Owner and M5/M6 | 100/100 PASS with ASan, UBSan and leak detection; initial SM4 failure retained separately |
| Separate two-archive Crypto/ASN1 installation audit | PASS, unique static symbols and no provider export |
| Original installed Crypto C11/C++17 consumers | 2/2 PASS |
| Original installed Crypto ASN1 C11/C++17 consumers | 2/2 PASS |
| Installed TLS 1.2/1.3, auth, independent Python mTLS, cancellation and CipherBIO | 6/6 PASS |
| Original native #14 contract programs linked to local Crypto | 29/29 default contracts PASS; extra optional modes remain separately recorded as 33/35 PASS |
| Real production CNet archive/link-map/dynamic-import ownership | PASS; 12 audited native TLS entry points come from the local Crypto archive; both original plugins and loader contain no second Crypto archive |
| Native source derivation and compiled-profile admission | 166 inputs reproduced; five negative/positive admission tests PASS |
| Crypto PR43 handoff | 18/18 obligations preserved; eight negative/positive tests PASS |
| Core Python unit modules and negative source/owner gates | 119/119 PASS; the separate Zstd binary CLI is exercised by full CTest |
| SDK packaging Python checks | 29/29 PASS |
| Android arm64 API 24 default build/install and installed consumer linkage | 220 translation units, including 84 native Crypto units, and 47 real AArch64 ELF consumers; no device execution claimed |

The Linux installed consumers use an explicit SDK library search path.
Full-module consumers also use the verified existing non-crypto dependency
prefixes. These checks do not claim clean-machine runtime closure or trusted
release receipts. The Android check is cross compilation and linkage only.

The historical PR43 CMake input is preserved in
`crypto/tests/migration/CMakeLists.original.txt`. Its change to local assembly
has an independently reviewed diff. Historical workflow/input snapshots and
all original source inventories retain their original bytes.

## Remaining release gates

This repair does not mark the original 2,975 files or 610 explicitly public
functions as fully migrated. The strict final completeness command still
fails on unresolved source/installed-runtime evidence. AC/Levenshtein remain
out of implementation scope, with their original obligations still pending.
New source changes also require new current-commit platform qualification;
old SIMD receipts are not relabeled as evidence for this tree.

The existing platform/provider, clean-runtime closure, trusted receipt and
NuGet qualification blockers remain independent. No downstream repository was
read or tested for this repair. PRs #72 and #94 remain Draft and unmerged.
Version 0.1.000 remains unpublished because release admission is incomplete.
