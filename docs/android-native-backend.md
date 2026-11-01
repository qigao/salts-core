# Android native backend selection

Android builds select the existing Linux implementation of `Salts::NativeIO`.
The shared module compiles these source files:

```
native_io.c
native_io_accept_escrow.c
native_io_sharded.c
native_ipc.c
native_io_linux.c
native_io_epoll.c
native_io_io_uring.c
native_io_readiness.c
native_ipc_posix.c
```

`Salts::Platform` also accepts `SALTSCORE_ENABLE_EPOLL_READINESS=ON` on Android
and compiles its existing `readiness_epoll.c` implementation. The option remains
explicit. The native runtime code, provider ownership, output handling, and
resource lifetime rules are unchanged by this CMake selection change.

The old configuration selected `native_io_unsupported.c` for Android and
rejected an explicit Platform epoll request. An actual Android configure with
that request reproduced the rejection. The corrected configuration compiles
the epoll/eventfd and io_uring syscall code provided by the NDK headers.

This establishes build availability. io_uring initialization still reports
kernel or policy failures; selecting a compiled backend does not promise that
every device permits it. There is no success stub or automatic conversion of
an unavailable backend into an accepted runtime test. The packaging check still
rejects a compile database containing `native_io_unsupported.c`.

## Reproducible cross-build

Local compilation used the official NDK r27d, revision `27.3.13750724`, with
`arm64-v8a` and API 26. The Linux archive was checked against the published
663,956,036-byte size and SHA1
`22105e410cf29afcf163760cc95522b9fb981121` before extraction. This records the
toolchain actually tested, rather than requiring release automation to use an
older NDK. The archive and checksum are listed in the Android project's
[NDK release inventory](https://github.com/android/ndk/wiki/Unsupported-Downloads#r27d).

```sh
cmake -S . -B BUILD \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DSALTSCORE_ENABLE_EPOLL_READINESS=ON \
  -DSALTSCORE_BUILD_SERIAL=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_INSTALL_PREFIX="$SDK"
cmake --build BUILD --parallel 2
cmake --install BUILD
cmake -S tests/installed -B INSTALLED_BUILD \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release \
  -DSaltsCore_DIR="$SDK/lib/cmake/SaltsCore"
cmake --build INSTALLED_BUILD --parallel 2
```

The explicit `SaltsCore_DIR` is needed for the tested NDK find-root behavior;
supplying only `CMAKE_PREFIX_PATH` did not locate the target SDK. This uses the
target installation without allowing arbitrary host package discovery.

Nested ASN1/Crypto discovery also needs an exact SDK owner. Four actual NDK
configuration probes used the installed ASN1 archive: the original direct
include followed by `find_dependency(ASN1)` failed to find its package; adding
only `PATHS`, `NO_DEFAULT_PATH`, and `NO_CMAKE_FIND_ROOT_PATH` still selected a
foreign cached `ASN1_DIR`; setting the normal `ASN1_DIR` variable to the real
sibling package first selected the intended owner and allowed nested discovery;
and an already loaded foreign owner remained rejected. The Core package applies
this exact-directory rule to ASN1 and Crypto without forcing global cache
values. These probes validate CMake package lookup and owner rejection, not a
successful build or runtime qualification of the currently blocked Crypto owner.

The complete configured foundation and Serial graphs compiled and installed.
The complete foundation installed graph compiled 40 registered test executables,
including the new C11/C++17 error clients; their ELF machine type was checked
as AArch64. Both independent Serial clients also compiled and linked. The
compile database records API 26 and the genuine NativeIO/Platform sources.

These results are **cross-compiled consumers**, with no Android device or
emulator execution. They do not cover optional modules absent from that local
configuration. The release candidate's broader configured graph and actual
runtime acceptance remain separate obligations. Frozen source/API migration
gates and the deferred AC/Levenshtein obligations are unchanged.
