# Capture native ownership migration

This module retains all **39 frozen Capture inputs** from SaltsUtils commit
`5b0be65de673261fc37b29485b0364ca38cc6cd6`. All production C/Objective-C files,
public/private headers, original tests, shell/Python regression harnesses and
original README bytes are unchanged. The three adapted CMake files have exact
original copies under `original/`, including the standalone native TSan graph.
The frozen repository inventory remains the reference for the provenance test.
File retention is not completion of the original public-function qualification.

## Current build and installed contract

Enable `SALTSCORE_BUILD_CAPTURE=ON`. Test builds also require
`SALTSCORE_BUILD_TINYTEST=ON`. The production target is a single shared
`Salts::Capture`, exported through `SaltsCoreTargets` with the Core `0.1` ABI
epoch. Its internal CMake target remains `salts_capture`; the installed binary
is `saltscore_capture` with the platform's versioned DLL/DSO naming.

```cmake
find_package(SaltsCore 0.1 CONFIG REQUIRED)
target_link_libraries(application PRIVATE Salts::Capture)
```

Core and Platform provide the existing native logging/thread primitives. No
CMeta reflection runtime, CFlow runtime, or alternative Capture provider is
introduced. The Android implementation's `cmeta_mutex_*` and `cmeta_cond_*`
spellings are the retained native functions in `<salts/thread.h>`, owned by
`Salts::Platform`; their historical names are not a CMeta runtime dependency.

The original 26 common public symbols and three Android extension symbols keep
their names and signatures. Original error codes, native-mode identities,
frame-rate arithmetic, device ID strings, byte/UTF-8 handling, callback payload
lifetimes, thread admission, stop/join barriers and resource ownership are
unchanged. Platform-specific differences are retained too: for example,
unsupported control reads zero the supplied output on the original non-Windows
backends, while Windows null-owner failure leaves it untouched. This migration
does not silently rewrite that existing behavior to match a broader header
comment.

The explicit export lists contain the public C functions. Private miniaudio and
backend symbols do not become a second public runtime when Capture and Playback
are loaded together. The public headers stay byte-for-byte original; visibility
is configured at the source/link level. No third-party implementation header is
added to the common public interface. The original Android extension continues
to use the platform's JNI types.

## Complete native dependency selection

| Platform | Actual compiled backend | Private dependencies |
| --- | --- | --- |
| Windows | miniaudio, Media Foundation/DirectShow video, D3D11/DXGI screen | Pinned miniaudio and libyuv/JPEG; Windows SDK libraries |
| Linux | miniaudio, V4L2 video, PipeWire and X11/XShm screen | Pinned miniaudio and libyuv/JPEG; PipeWire 0.3 API, X11/Xext, threads, dl and libm |
| macOS | miniaudio/CoreAudio, AVFoundation video, ScreenCaptureKit | System Apple frameworks plus pinned miniaudio |
| Android | OpenSL ES/miniaudio support, Camera2/Media NDK, AImageReader/MediaProjection | NDK, pinned miniaudio and libyuv/JPEG; native Platform synchronization |
| iOS | AVAudioEngine, AVFoundation and ReplayKit source set | System Apple frameworks; the public-symbol gaps below remain unresolved |

Linux always enables and links **both** original PipeWire and X11 paths. Missing
SDK dependencies fail configuration. The build is not reduced to the platform
dispatcher or the pure video-mode helper, and does not select a stub backend.
Android requires API **26**, as the retained MediaProjection interface uses
`ANativeWindow_toSurface`. Building only the older Core API-24 profile does not
qualify Capture.

The API floor is checked by compiling against the actual Android compiler's
`__ANDROID_API__`. The NDK's legacy toolchain sets `CMAKE_SYSTEM_VERSION=1` as
an integration sentinel, even when its compiler targets API 26; that sentinel
is not treated as the effective API. The dedicated NDK configure regression
accepts API 26 and requires API 21 to fail the same production guard. It does
not claim execution on an Android device.

The Capture-only iOS feature has no private vcpkg packages: the original iOS
backend uses Apple frameworks. Its workflow therefore configures the full
iPhoneOS/arm64 source and installed-consumer graphs through CMake's real Apple
SDK support without invoking an empty private-provider restore. All common
public-symbol references remain required, including the six unresolved iOS
implementations below. The unified Core SDK's iOS restore is unchanged because
its other enabled modules have real private dependencies.

The frozen vcpkg baseline is
`b1b19307e2d2ec1eefbdb7ea069de7d4bcd31f01`: miniaudio is `0.11.22`, and libyuv
uses upstream commit `a37e6bc81b52d39cdcfd0f1428f5d6c2b2bc9861` with the baseline's
CMake patch. The private usage requirements and actual link artifacts are
consumed during the producer build. Third-party imported target names are not
made requirements of the installed `Salts::Capture` CMake target.

That separation does not remove runtime requirements. Linux still needs its
actual PipeWire/X11 system libraries. The Linux development verification uses
official Ubuntu archives, checked against the Release-index SHA-256 values and
extracted into a temporary dependency prefix; that prefix is not a published
SDK closure. Native dependency packaging remains a separate release blocker.
Windows installs the real yuv/JPEG DLLs when those providers are shared;
static provider archives are not mislabeled as runtime DLLs. miniaudio/libyuv
notices and the Windows JPEG notice are installed with the SDK.

## Qualification and original test coverage

The original `capture_posix_test`, `capture_windows_test`,
`capture_exposure_windows_test`, and `capture_android_test` sources and test
names are retained. CMake now registers them directly instead of requiring the
SaltsUtils build macros. The Windows fake-COM exposure test still compiles the
original backend body as its isolated regression, without replacing production
functions in the SDK.

`capture/tests/installed` is an independent C11/C++17 project. It uses only
installed headers and `Salts::Capture`, with typed volatile references to **all
26 common APIs** and, on Android, **all three extension APIs**. These references
must resolve even for operations that a test without device access must not
start. The executed checks cover C/C++ layouts, enum values, rational frame-rate
boundaries, unchanged byte buffers on invalid list operations, output reset on
failed creation, null lifecycle behavior and the original platform-specific
control error behavior. No camera, screen, or microphone is started by those
consumers.

The Capture workflow builds the complete configured Core graph for Linux x64,
Linux ARM64, Windows x64, macOS ARM64, Android ARM64 and iOS ARM64. It adds an
ASan/UBSan/LSan native Linux row. Native rows run their original tests and both
installed consumers; mobile rows compile/link their original test graph and
installed clients, without asserting device execution. No disabled test or
unsupported platform is accepted as passing capability evidence.

The original standalone source-TSan regression now imports an installed
`SaltsCore` foundation. Its actual Capture/Linux/miniaudio source and private
PulseAudio harness remain original. The separate workflow job builds an
instrumented native Core owner and the original Capture regression, then runs
the retained 12 repetitions and six exact markers. The harness permits only a
CI-owned software null monitor, zero physical audio cards and digital-zero PCM.
This is callback/stop/restart/join evidence when executed successfully, not
physical microphone or six-platform device acceptance. The historical
`tests/native_tsan/README.md` is retained as original provenance; its old
Salts.Native package instructions are superseded by this Core integration.

## Explicit unresolved release blockers

The machine-readable list is `capture/release-blockers.json`. The **original
iOS source set has no definitions** for six functions declared in the common
public header:

- `salts_capture_list_gpu_devices`
- `salts_video_capture_get_control_range`
- `salts_video_capture_set_control`
- `salts_video_capture_get_control`
- `salts_video_capture_set_crop`
- `salts_video_capture_get_crop`

These are source-inspection findings, not a claimed iOS execution result. The
explicit export list and real installed consumer references expose the missing
definitions at link time. The iOS row remains mandatory and failing until a
separately reviewed implementation resolves the gap. There are no fabricated
success returns, removal of public declarations, or skipped consumer references.

The Linux original POSIX test also exposes a resource leak in the fixed
**miniaudio 0.11.22 provider** when PulseAudio initialization cannot connect.
With ASan/UBSan and `detect_leaks=1`, the unmodified Capture test reported
**41,332 bytes in 28 allocations**. The call chain is
`pa_context_new_with_proplist` →
`ma_init_pa_mainloop_and_pa_context__pulse` → `ma_context_init__pulse` →
the original Capture audio enumeration/unknown-device creation paths. A
miniaudio-only reproducer with an explicitly absent private PulseAudio socket
also leaks; it does not link Core or Capture or start any audio device.

The provider's two connection-failure branches free the main loop without
unreferencing the already-created Pulse context. Capture's observed paths
already destroy every successfully initialized miniaudio context. Patching
Capture to destroy an unsuccessfully initialized context would violate the
provider lifecycle and would not recover its lost private pointer. A minimal
provider correction must therefore be reviewed with explicit dependency
provenance. The shared original header and frozen pin are unchanged here.
PulseAudio is not disabled, LSan is not suppressed, and the failing original
sanitizer test remains a release blocker. Passing installed null/error/ABI
consumers cannot override this failure.

The isolated reproducer, exact header/log identities, and an **unapplied**
two-line provider cleanup proposal are recorded in
[`provider-proposals/README.md`](provider-proposals/README.md). The temporary
copy preserves error `-203` and exits cleanly under the same sanitizers in the
absent-server scenario. Only a reviewed dependency-owner revision and renewed
platform qualification can resolve this blocker; no production build applies
that proposal.

Camera acquisition, real microphone capture, screen acquisition, permission
denials, device unplug/reconnect, and mobile lifecycle qualification remain
pending wherever the required device/OS environment has not been exercised.
Absent-device enumeration and successful ABI tests cannot automatically satisfy
those requirements. The complete original-source/public-function release gate
and the dependency-closure gate continue to apply independently.
