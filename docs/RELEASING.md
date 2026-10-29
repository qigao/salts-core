# SaltsCore 0.1.000 release qualification

The candidate packaging and guarded publication workflow are prepared for
review. Production publication admission remains unimplemented and disabled:
the verifier still needs to bind trusted CI build/test receipts to each RID's
actual payload and prove the clean-runtime dependency closure. Clearing the
editable debt list or declaring `execution=native-ctest` cannot remove these
implementation blockers. This document does not assert
that `0.1.000` has been released or that the native migration is complete.
Original 2,975-file and 610-public-function qualification remains mandatory.
An inventory entry, copied file, target declaration, or successful partial
profile is insufficient evidence to mark an original function migrated.
The user's AC/Levenshtein exclusion remains an explicit scope decision; these
algorithms are not silently implemented or credited by the SDK tooling.

## Version and source identity

`packaging/release-profile.json` records all version identities separately:

| Contract | Value |
| --- | --- |
| Display version | `0.1.000` |
| Explicit tag | `v0.1.000` |
| NuGet package and normalized version | `SaltsCore.Native` / `0.1.0` |
| CMake package version | `0.1.0` |
| DSO/DLL ABI epoch | `0.1` |

The existing CMake numeric version remains `0.1.0`; padding does not create a
second ABI. [NuGet normalizes leading zeroes](https://learn.microsoft.com/en-us/nuget/concepts/package-versioning#normalized-version-numbers),
so an existing equivalent `0.1.000`, `0.1.0`, or zero-revision version blocks a
second publication. No duplicate skipping or asset overwriting is permitted.

CI checks out the exact PR head or tag commit, with no credentials persisted in
the checkout. Builds, installations, staging, NuGet intermediates, and logs stay
outside the source tree. The build driver records a clean commit/tree before
configuration and verifies the same identity after consumer execution. Every
RID must match the same source identity. The publisher checks the exact local
tag resolution against that commit again.

The manifest's source field may be `null` for a locally inspected older SDK.
That is useful structural evidence but always blocks publication. Supplying a
current checkout identity for an unrelated pre-existing SDK would be false
provenance and is not an acceptable use of the staging command.

## Candidate builds and six-RID capability

`.github/workflows/core-sdk-release.yml` runs on PRs and manual candidate
requests. It also has a separate, strictly guarded publication path for a push
of the exact `v0.1.000` tag. All active Core producer workflows restore the
same required base dependency graph and compile the local Crypto source.

| RID | Runner / target | Required candidate evidence |
| --- | --- | --- |
| `linux-x64` | Ubuntu 24.04 / x64 Linux | Full configured build and CTest; relocated installed C11/C++17 consumer execution |
| `linux-arm64` | Ubuntu 24.04 ARM / arm64 Linux | Native ARM64 build, CTest, and relocated consumer execution |
| `windows-x64` | Windows 2025 / pinned MSVC v145 triplet | Actual DLL/import-library install and native consumer execution |
| `macos-arm64` | macOS 15 / arm64 macOS | Actual dylib install and native consumer execution |
| `android-arm64-v8a` | Ubuntu / NDK, API 26, arm64-v8a | Supported native backend selection, full cross build, installed consumer compile/link |
| `ios-arm64` | macOS 15 / iPhoneOS device arm64 | Supported native backend selection, full cross build, installed consumer compile/link |

An additional native Linux row instruments the complete configured producer
and installed consumer graphs with ASan and UBSan; LSan remains enabled.
The four native host rows execute tests. Android and iOS record
`cross-compiled-consumer`, which makes no assertion that device tests ran.
Future device qualification cannot be inferred from those cross builds.

The `native-migration` candidate enables SIMD, TinyTest, URI, Unicode, IDNA,
CSerde, Process, SCTP, the private WebSocket parser, USB, Playback, Serial,
Capture and the original Core Zstd facade. The default graph also includes
ASN1, source-built static Crypto and production CNet, with no external GmSSL
archive. The original six Serial examples also compile. Current-source
production TLS, native algorithm, source-byte, physical owner and installed
consumer qualification is mandatory; enabling the additional M5 test graph
does not replace any production test or establish release admission.

Each configured graph builds without a selected-target shortcut. Native rows
enumerate all CTests, require the original/runtime and source-gate entries,
reject disabled/skippable tests, and execute the complete suite with
`--no-tests=error`. Each SDK is archived and re-extracted before separate
foundation, FS, Process, SCTP, USB, Playback, Serial and Capture consumer graphs
are configured. The final NuGet archive is also extracted and consumed on Linux.

The fixed Stun ccache action has no Linux ARM64 binary, so that row builds
without the compiler cache. Other rows use the same pinned cache action.
Private c-ares, KCP, SIMDe, miniaudio, libusb, libyuv and Zstd come through the fixed
vcpkg/curl action and the repository's pinned registries. Linux Capture uses
the actual PipeWire/X11 system SDKs, and libusb requires the udev development
package. The exact re2c host tool and Unicode 17 data remain mandatory. Cache
availability never substitutes for a build or platform gate. Missing exact
binary packages remain failures, with no provider/version fallback. Android API 26
preserves the original Capture MediaProjection requirement.

The native backend check inspects the actual compile database. In particular,
selecting `native_io_unsupported.c` rejects the Android candidate. It is not a
successful platform SDK and must not be made green by disabling NativeIO or
omitting Android. A missing macOS result or a failed sanitizer row likewise
blocks the aggregate package. No platform is optional and no automatic retry
changes a failed capability result into release evidence.

## Independent publication gates

The publication target floor is defined independently in the validator, then
checked against the editable profile. It includes real library targets for:

`Core`, `Platform`, `Concurrency`, `Coroutine`, `NativeIO`, `FS`, `Process`,
`URI`, `Unicode`, `IDNA`, `CSerde`, `TinyTest`, `SIMD`, `ASN1`, `Crypto`, `CNet`,
`SCTP`, `Serial`, `USB`, `Capture`, and `Playback`, all in the `Salts::`
namespace. Missing or interface-only native modules block release. A binary
target must name an actual installed library. SIMD must remain static; the
native foundation must retain its shared owners. Core must not export upper
layer `CMeta`, `CFlow`, or `CSTL` runtimes.

This floor is not derived from whichever modules happen to be present. Removing
Capture, Playback, Serial, USB, or a required RID from the profile is rejected.
The frozen full-source gate is independent of this target check and is the
authority for remaining source/API migration debt.

Before publication, all of these conditions must hold:

1. The final source gate succeeds with
   `python tools/check_full_source_migration.py --require-complete`. Its ordinary
   inventory mode is not accepted in place of this final gate.
2. Every explicit `release_deferred` entry has been resolved with reviewable
   evidence. Clearing the list does not override any other gate.
3. All six actual RID manifests identify the exact clean source commit/tree,
   supported backend selection, correct native/cross consumer category, full
   native target set, and exact payload hashes. Missing or changed libraries
   and unsafe archive paths are rejected.
4. All seven build rows and the package consumer pass. The aggregate NuGet
   package has one correct normalized nuspec and exactly six SDK roots, with
   no managed dependency graph.
5. The tag resolves to the same commit. The publisher re-runs the strict source
   gate and the complete package checks immediately before upload.
6. Neither the GitHub release nor an equivalent normalized NuGet version already
   exists. Authentication, transport, or pagination errors fail this check.

Archive integrity and target names alone cannot establish SDK qualification.
Review reproduced a structurally valid archive of non-executable fixture files
that claimed all required targets, selected backend names and completed tests.
`RELEASE_IMPLEMENTATION_BLOCKERS` therefore keeps publication disabled until
real receipt admission and clean-runtime closure verification replace those
explicit gaps. These checks are independent of `release_deferred`, and are not
manual acceptance flags.

`release-check` itself is read-only. It returns `published: false`, including
when all gates pass. Only the explicit tag's final Actions job has write
permissions and invokes the NuGet/GitHub upload commands. Manual workflow runs
remain candidate-only, even when a tag is selected in the UI.

The initial profile keeps explicit deferred entries for full migration
qualification, six-RID native module capabilities, and dependency/runtime
closure. Candidate success cannot clear them automatically. The current
candidate build options and dependency selection must be expanded and
qualified as missing modules become available. The full release floor remains
in force throughout that work. Native dependency releases and their runtime
libraries must be qualified before removing the closure blocker; source-local
providers are not automatically a redistributable SDK.

## Local, independently runnable candidate tooling

The tools use Python's standard library. To inspect an existing installation,
choose fresh output paths and intentionally omit source identity if that
installation's exact build provenance is unavailable:

```sh
python cmake/ci/core_sdk.py stage \
  --prefix /tmp/core-install \
  --output /tmp/core-sdk-review \
  --rid linux-x64
python cmake/ci/core_sdk.py archive \
  --prefix /tmp/core-sdk-review \
  --output /tmp/SaltsCore.Native.0.1.000.linux-x64.zip
python cmake/ci/core_sdk.py verify-archive \
  --archive /tmp/SaltsCore.Native.0.1.000.linux-x64.zip
python cmake/ci/core_sdk.py extract \
  --archive /tmp/SaltsCore.Native.0.1.000.linux-x64.zip \
  --output /tmp/core-sdk-relocated
python -m unittest discover -s packaging/tests -p 'test_*.py' -v
```

Manifest **schema 2** includes each entry's kind, digest and size, target kinds,
and actual imported library paths. A regular file is recorded as `kind=file`.
A symlink is recorded as `kind=symlink` with its exact relative `target`; its
size and SHA-256 cover the UTF-8 link text, not the dereferenced library. The ZIP
entry must also carry the Unix symlink type and that same link text. A regular
file with equal library bytes cannot satisfy a symlink record. Earlier schema-1
candidates are historical artifacts, not implicitly reinterpreted as preserving
link identity.

The original `libname.so -> libname.so.0.1 -> libname.so.0.1.0` chain remains a
chain through staging, archive creation and extraction. All names resolve to one
actual file, preserving the existing downstream runtime-owner audit. Before
creating an extraction directory, the verifier checks the complete link graph
within each individual SDK root. Absolute/escaping targets, cross-RID links,
cycles, dangling or directory links, links used as parent directories, conflicting
entry types, path traversal, case collisions, reserved Windows names, duplicate
JSON keys and undeclared payload files are rejected. The manifest itself must
be regular; package metadata outside the SDK roots cannot be symlinks.

Use `core_sdk.py extract` to materialize this contract. It writes validated
regular files before creating the validated relative links and verifies the
result. It never substitutes a copy when symlink creation is denied. Native
Windows DLL SDKs with only regular files remain supported, including ordinary
DOS ZIP file attributes. Materializing a Unix SDK on a host without symlink
support or permission fails explicitly; it does not qualify that host or SDK.
ZIP entries have fixed timestamps for reproducible archives of the same staged
SDK. Native compiler outputs are not claimed reproducible across machines.

The current `dotnet pack` glob and NuGet restore path are **not yet qualified**
to retain this Unix link contract. If a packer follows a link or writes it as a
regular file while retaining the manifest, `verify-archive --nuget` rejects the
package. No post-pack relabeling, equal-hash owner deduplication, or silent link
reconstruction changes a mismatched package into accepted evidence. A genuine
NuGet pack/restore round trip and compatible downstream download clients remain
publication requirements. Existing clients that reject all archive symlinks
must be qualified separately before consuming this format; the old Salts 2.3
restore path has not been converted by this packaging fix.

For a new exact-source build, `cmake/ci/build_core_sdk.py` drives configuration,
complete configured builds, tests, installation, relocation and consumer
qualification. It requires a clean checkout, exact source SHA, toolchain and
triplets. It refuses output beneath the source checkout. Its manifest records
execution only after the actual consumer step completes. These records are CI
evidence within that controlled workflow, not a cryptographic attestation that
can independently validate arbitrary third-party assertions.

## Publication and downstream acceptance order

First complete Core's migration and all platform/dependency qualifications.
Review the exact source commit and six-RID package. Only then create the
explicit release tag through the normal authorized process. The tag workflow
rebuilds and rechecks that source before publishing the immutable normalized
NuGet package and the `v0.1.000` GitHub release asset plus `SHA256SUMS`.

NuGet and GitHub Release uploads are separate operations. If one succeeds and
the other fails, the workflow does not overwrite, skip duplicates, or silently
resume. Retain its artifacts and inspect the source/digests before an explicit
recovery decision. Existing published bytes must remain immutable.

After publication, download the actual exact `SaltsCore.Native` version
`0.1.0` and verify the release asset hash, all six manifests, source commit and
tree. Configure independent CMake consumers from that downloaded installation.
Then qualify Salts 3 against the published Core SDK: the 14 reflected SIMD
shapes, same-value/different-address descriptors, failure paths, and real
execution consumers must pass. Finally build the full TurboWasm configured
graph against the two installed prefixes, execute its 12 SIMD/relaxed-SIMD/JIT
cases plus the three HostFS cases, and run its installed consumer.

Local integration SDKs and source-build workflows are useful development
evidence, but they do not prove consumption of a published release. TurboWasm's
existing Salts 2.3 restore path must not be relabeled as a Core/Salts 3 package
consumer; its production transition remains a separate, explicit task.
