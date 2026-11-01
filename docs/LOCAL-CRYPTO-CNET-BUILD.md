# Crypto / CNet build — vcpkg GmSSL provider admission

**Current migration design (Draft).** The historical source-owned results in
[commit 720abc55](https://github.com/qigao/salts-core/blob/720abc55eca41cc7f676d0b5dbb38e8605d091b9/docs/LOCAL-CRYPTO-CNET-BUILD.md)
are evidence for that **older** native-source HEAD only. They are not
evidence that a later vcpkg-linked HEAD has already passed.

## Single dependency source

The root `vcpkg.json` requires GmSSL 3.2.0#16, sourced from the
exact `qigao/vcpkg-cache` custom registry commit pinned in
`vcpkg-configuration.json`. The port owns the unmodified upstream GmSSL
checkout and the ordered four overlay phases. Its 15 original additional
extension sources and patches are supplied from the immutable
`crypto/gmssl-extensions/` checkout, not from a second implementation mirror.
`crypto/tools/check_local_native_source.py` checks the original Git blob
identities, the immutable package/version selection, and the absence of a
locally compiled GmSSL source copy.

`Salts::Crypto` compiles C11 TLS facade/CipherBIO/Ed448/SHA256/X.509 code and
links `GmSSL::GmSSL` as an imported external static package dependency.
`Salts::ASN1` remains the independent BER/DER codec. `Salts::CNet` uses
the same package for its existing native GmSSL TLS implementation until
the opaque API has reached full parity; no second backend or fake fallback
is introduced. The independent Monocypher and Reed/GF256 libraries stay
in `vendor/monocypher` and `vendor/reed`.

## Real build

Use the checked-in native LLVM presets and the same canonical compiler and
vcpkg-cache environment already used by the project's unified CI:

```sh
python3 -B crypto/tools/check_local_native_source.py
cmake --preset ci-native
cmake --build --preset ci-native
ctest --preset ci-native
```

No new installer/consumer test project or probe CMake/CI is needed.

## Independent qualification blockers

- Resolve and compile the immutable GmSSL package (including Linux/macOS,
  Windows LLVM-MinGW, Android) rather than a pre-existing local source tree.
- Verify the exact native GmSSL and Crypto archive definitions and
  CNet/DSO ownership maps; the imported static GmSSL archive is distinct
  from the first-party `salts_crypto` archive.
- Run original TLS 1.2/1.3, mTLS, CA/SAN negative tests, ShangMi/TLCP, ALPN,
  close_notify/EOF/cancel and NativeIO ownership; never count a synthetic or
  disabled test as success.
- Preserve the exact installed `find_dependency(GmSSL CONFIG REQUIRED)`
  SDK contract; release packaging must not claim a self-contained GmSSL
  payload without explicitly installing/publishing that dependency.
- Only report CI from the **exact** source HEAD, not the historical native
  blob-mirror passes. Do not merge or publish while any gate is missing.
