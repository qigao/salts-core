# SaltsCore 0.1.002 release

The immutable tag `v0.1.002` maps to CMake and NuGet version `0.1.2` and the
package identity `SaltsCore.Native`.

The existing LLVM CI is the only release producer. Its tag run configures and
builds the complete graph, runs the formal native CTest suites, installs the
eight supported SDKs, and publishes only after every matrix row succeeds:

- Linux x64 and arm64
- Windows x64 and arm64 using llvm-mingw/UCRT
- macOS x64 and arm64
- Android x64 and arm64

Sanitizer rows qualify the same source but are not packaged. Android rows are
cross-build evidence and do not claim device execution. Compiler obfuscation
remains disabled through the pinned `vcpkg-cache` setup actions.

Publication is authorized only by pushing `v0.1.002` at the reviewed master
commit. The release job refuses an existing GitHub release or normalized NuGet
version, packages the eight install prefixes without a consumer/installer CI
project, and uploads `SaltsCore.Native.0.1.2.nupkg` plus `SHA256SUMS`.

Do not move the tag, overwrite an existing package, skip failed matrix rows, or
publish from a locally assembled SDK.
