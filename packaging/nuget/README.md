# SaltsCore.Native SDK candidate

This package carries native CMake install trees. It has no managed assemblies or
managed dependency graph. A candidate archive is not a release and does not
certify completion of the original-source migration.

## Version identity

| Field | Value |
| --- | --- |
| User-facing release version | `0.1.000` |
| Explicit Git tag | `v0.1.000` |
| NuGet package ID | `SaltsCore.Native` |
| Normalized NuGet/CMake version | `0.1.0` |
| Native ABI epoch | `0.1` |

NuGet removes leading zeroes when normalizing versions. `0.1.000` and `0.1.0`
cannot be published as distinct package identities. The release tag and SDK
manifests preserve the requested display version; native package lookup uses
`0.1.0`. See [NuGet version normalization](https://learn.microsoft.com/en-us/nuget/concepts/package-versioning#normalized-version-numbers).

The complete package layout is `sdk/<rid>/include`, `sdk/<rid>/lib`, optional
`sdk/<rid>/bin`, and `sdk/<rid>/saltscore-sdk-manifest.json`. Its exact RID set is
`linux-x64`, `linux-arm64`, `windows-x64`, `macos-arm64`,
`android-arm64-v8a`, and `ios-arm64`. An individual ZIP carries one candidate RID;
the NuGet publication gate requires all six real SDKs.

Each manifest records display/package/ABI versions, source commit and tree when
known, exported target kinds, imported binary paths, payload SHA-256 values,
selected native I/O backend, and the consumer execution category. Archive
validation checks structure and bytes. It does not convert a missing source
identity, unsupported backend, cross compilation, or unqualified API into a
completed migration.

Manifest schema 2 distinguishes regular files from symlinks. The previous PR-time
six-RID archive/installed-consumer harness has been removed from CI; publication
packaging is deliberately deferred until the migration gate is complete. Release
tooling must preserve relative SONAME aliases and reject escaping, cyclic,
dangling or cross-RID links when it is reintroduced. Ordinary PR CI does not
assemble or unpack NuGet packages.

## CMake consumption

Extract the chosen RID from an exact package version, verify its SHA-256 and
manifest, and put that SDK root in `CMAKE_PREFIX_PATH`. The canonical entry is:

```cmake
find_package(SaltsCore 0.1 CONFIG REQUIRED)
target_link_libraries(my_native_client PRIVATE Salts::Core Salts::SIMD)
```

`Salts::SIMD` is a static native module with no public SIMDe headers and no CMeta
runtime. Shared native foundation libraries retain their single runtime owners.
Install the runtime DLLs/DSOs alongside the application using the platform's
normal loader rules. The SDK is not an implicit replacement for Salts 2.3:
upper-layer Salts 3 consumers must use their separate installed package.

The current candidate profile assembles the implemented foundation plus SIMD and
TinyTest, including unconditional modules selected by the source CMake graph.
It is deliberately smaller than the required publication scope. Refer to the
repository's `docs/RELEASING.md` for missing release qualifications, exact source
gates, six-RID testing, and downstream acceptance.
