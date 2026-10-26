# M6 — strict bounded UTF-8 primitive for PluginRegistry (Draft)

`salts_core_utf8_valid(data,len)` is now a dependency-free Core C11
primitive. Its semantics match legacy `vstr_utf8_valid` for a counted
byte sequence: accept all valid Unicode scalar UTF-8 encodings, reject
overlong sequences, UTF-16 surrogate code points, out-of-range scalars,
truncation and malformed continuation. NUL in a counted byte span is
a valid scalar, not a filename validator. `NULL,0` is accepted; Plugin
caller separately rejects empty paths, enforces a terminator within
CMETA_PLUGIN_PATH_MAX, then uses this function.

This eliminates PluginRegistry's **only direct vstr call** without
duplicating the Unicode decoder in ACE. The original Salts vstr path
continues to serve the old production binary until a coordinated
downstream owner cutover; no runtime fallback or CMeta dependency is
introduced to Core.

Qualification requires synthetic edge vectors, C11/C++17 installed SDK
consumers, final-image symbol ownership, and real PluginRegistry path
and ServiceHost tests on Linux/macOS/Windows. Old Salts::Core must
not link alongside the new owner. See salts#1287, salts#1304 and
salts-core#20/#23.
