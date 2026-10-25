# WebSocket frame parser — CNet private M4 prerequisite

## Migration scope

The RFC 6455 frame parser is pure C11 code from
`qigao/salts/tools/wsparser/{websocket_frame_parser.c,.h}`,
source branch `refactor/cnet-ipc-name-native` (Salts #1293), content blob
identities `e6f4854d45168ccda93392c181f2f9c69523ca15` (C) and
`f665d197723ee110044a6ac8b3dcc141cca911f6` (H).

It is now stored in `src/cnet/wsparser/` and compiled as an opt-in,
**non-exported** object target, `saltscore_wsparser`, when
`SALTSCORE_BUILD_WEBSOCKET_PARSER=ON`. It is deliberately **not** a public
CMake installed target. Later CNet will link the same object privately.

## Invariants

- No Salts, SaltsUtils, CMeta Reflection, CFlow, CSTL or crypto library
  is required for this frame parser.
- No fallback parser and no network/timer/owner is created.
- Exactly the original RFC 6455 header/frame contract: length 7/16/64,
  canonical form, overflow, reserved bits, control size and fragmentation,
  optional mask, header build, borrowed payload slices.
- Public CNet `<cnet/websocket.h>` is NOT migrated or published by this
  task. It remains in Salts until the actual CNet transport target qualifies.
- The parser's historical C symbols are intentionally kept for conformance.
  Never link old and new copies in the same process during migration.
- Test includes incomplete inputs, masked payload, invalid RSV/opcode,
  non-canonical lengths, control rejection, large 64-bit headers,
  failure-atomic outputs and framing round trips.

## Qualification scope

`SaltsCore WebSocket frame parser contract` runs opt-in build/CTest and
minimal installed C11/C++17 SDK consumers on Linux, Windows and macOS.
This only proves the parser, not CNet transport or TLS/CA/SAN.
CNet provider-resolved builds, real WebSocket over TCP/TLS, cancellation,
close-drain and ASan/TSan remain separately gated.
