# Local native Crypto implementation

This directory is compiled from source into the private
`salts_crypto_native_objects` OBJECT target. Its objects are archived once in
`Salts::Crypto`. It does not build, import, export or install a separate GmSSL
library. A normal Core or standalone Crypto build needs no GmSSL installation,
prefix, package registry, source download or post-build archive merge.

The implementation retains its GmSSL ancestry and Apache-2.0 license. This is a
source ownership and build change, not a new cryptographic implementation.
The native source profile is the same one previously used by Core's pinned
Crypto qualification: GmSSL commit
`7c9f02904ef33e59c87b4f16621cc8fd434e7579`, the `gmssl 3.2.0#14` recipe at
`qigao/vcpkg-cache@65c12e71d02b9b7dacfafc3d188a3bda18184296`, and the existing
Core TLS 1.2 empty-authority, TLS 1.3 ticket-bound and strict handshake-reader
adaptations. The recipe includes its original ordered TLS, X.509, RSA,
portability, P-384 verification and undefined-behavior corrections. The P-384
backport is verification-only; its original provenance remains in
`vendor/gmssl-extensions`.

The former production CNet registry at
`2e214ae44a3e3244eded5db6361194576356fc9e` selected port `3.2.0#13`.
Unifying the native owner also gives CNet the existing Crypto #14 hardening
and Core mTLS/record corrections. The production CNet source and native
feature profile are retained; the backend is not described as a byte-exact
copy of the former #13 package. Its runtime contracts require separate
qualification, including TLS versions, certificate verification, ALPN,
external I/O, error propagation and ownership.

`source-lock.json` records every original and resulting source blob, the
recipe input blobs, Core adaptation blobs, compile definitions and platform
source selection. `upstream-to-local.patch` records the complete resulting
source transformation. The 82 common translation units plus exactly one
platform RNG and HTTP pair reproduce the previous 84-unit profile. Unused
upstream algorithms, command-line tools and upstream build/install logic are
not added to the build.

The Core-owned OBJECT build recipe has a separate reviewed identity. Source
integrity also checks its actual common/platform source lists and feature
definitions, so retaining an uncompiled file cannot satisfy this gate.

One additional local correction is recorded explicitly: the fully instrumented
ShangMi known-answer test exposed signed integer promotion before a 24-bit
shift in SM4's S-box substitution. Converting the byte to `uint32_t` before
shifting preserves the mathematical substitution and output bytes while
removing undefined signed overflow. The original and corrected source blobs
are recorded in `local_hardening`; the complete derivation patch includes it.

Check the local source inventory with:

```sh
python3 -B crypto/tools/check_local_native_source.py
```

To reproduce the checked-in source from an already obtained pristine copy of
the fixed upstream commit, use:

```sh
python3 -B crypto/tools/check_local_native_source.py --upstream /path/to/pinned/GmSSL
```

The reproduction check only writes a temporary directory. It verifies the
original input hashes, applies the recorded patch, and compares every result
byte-for-byte. It does not replace any source files or update the lock.

Production CNet keeps its complete existing TLS implementation and privately
uses these native headers. Its native calls resolve from `Salts::Crypto`.
Native symbols remain private to the containing runtime. Only the existing
`gmssl/sm3.h` public API is installed alongside the Crypto public headers.
The narrower Crypto facade qualification adapter remains test-only; its
passing tests do not establish complete production CNet feature equivalence.

These new local source records are separate from the original migration
baseline. They do not credit any unfinished item in the 2,975-file / 610-public-
function final completeness gate, including the excluded AC/Levenshtein work.
