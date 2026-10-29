# Playback native source ownership

`Salts::Playback` is the single shared native PCM owner in SaltsCore. Its
17 original public functions, headers, four implementation/header source
files, Apple export list and both original test sources are unchanged from
`qigao/salts-utils@5b0be65de673261fc37b29485b0364ca38cc6cd6`.
The active CMake files assemble the existing Core Platform/Concurrency owners
and export through SaltsCore. Both original build files remain under
`original/`. `tests/test_source_provenance.py` checks all 11 original Git blobs.

Miniaudio 0.11.22 comes from the same frozen vcpkg baseline as the original
module. It is a private implementation input, and its license is installed
with the SDK. The public header and installed link interface do not require
miniaudio headers or a second Salts package.

The original five PCM-buffer and eight lifecycle tests run unchanged. Their
deterministic null device remains confined to `playback_null_test_impl`; that
archive is neither installed nor linked into the production shared library.
The production library uses the original native device-selection path.
Installed C11 and C++17 consumers link only the exported `Salts::Playback`
target and execute all 17 public entry points' invalid-input contracts,
including output clearing and safe null destruction. Those tests establish
the production ABI and failure semantics; they are not physical audio playback
or mobile device qualification.

The original frame alignment, retained PCM byte order, bounded partial writes,
single producer/control-owner boundaries, callback behavior, stop/error state,
volume, drain timeout and quiescent destruction semantics remain intact.
The dedicated native workflow requires Linux Release, ASan/UBSan/LSan, TSan,
Windows and macOS evidence at the candidate head. Android/iOS build and real
audio-device behavior remain separate open obligations until actually run.
This migration does not change or waive the frozen 2,975-file / 610-function
final gate.
