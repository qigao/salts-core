# Crypto TLS Cipher BIO — CNet/NativeIO bounded handoff (qualification)

**Draft only. Not a production CNet replacement.**

This stage adds a host-only C11 `Salts::TLSCipherBIO` static helper
(`<crypto/cipher_bio.h>`) alongside the existing single native shared
`Salts::TLS` DSO. The helper embeds **no GmSSL implementation**, does not
load a second crypto DSO, and has no native sockets, persistence, Salts,
CMeta, CFlow, NativeIO or C++ dependency.

## Contract mapping from real SaltsCore CNet

CNet's original `src/cnet/cnet_tls.c` uses a fixed-capacity input/output
cipher ring (`cnet_tls_feed_cipher`, `cnet_tls_take_cipher`) and one
TLS session owner on the CNet event thread. NativeIO owns completion
records and retains its send-buffer backing until terminal completion.
The new helper replicates only the **ciphertext transport half**, not
the old backend's certificates, protocol policy or application routing.

| Existing CNet step | Crypto C11 transport contract |
| --- | --- |
| Allocate one inbound/outbound capacity per connection | `crypto_tls_cipher_bio_create(capacity)`, 17 KiB..1 MiB per direction |
| Bind native SSL IO | `crypto_tls_cipher_bio_acquire_io()` gives borrowed `crypto_tls_io` for exactly one opaque TLS session |
| NativeIO receive completion delivers ciphertext | `crypto_tls_cipher_bio_feed_cipher(data,size)`: entire batch copied or `CRYPTO_TLS_BIO_CAPACITY`, zero partial admission |
| Send pending ciphertext through NativeIO | `crypto_tls_cipher_bio_take_cipher()`: copies bounded FIFO bytes into caller-owned send backing; `AGAIN` when empty |
| NativeIO reports transport EOF | `crypto_tls_cipher_bio_end_input()`: buffered data drains before TLS read callback returns 0 |
| Cancellation / terminal close | `crypto_tls_cipher_bio_cancel()` atomically interrupts subsequent callbacks but never destroys retained buffers or TLS state |
| Destroy after all backend work and NativeIO completion | Destroy Crypto TLS session; release IO descriptor; destroy BIO. Before release, BIO destroy **must return BUSY** |

The callback is nonblocking, returns positive short counts, `IO_AGAIN`
on empty inbound/full outbound and `IO_ERROR` after cancellation.
Buffers are copied, not borrowed after call return. Two fixed rings
are allocated once; no unbounded growth, replacement, fallback or
NativeIO send-buffer aliasing.

**Concurrency:** the TLS Owner thread serializes `feed`, `take`,
`end_input`, `acquire_io`, `release_io` and all TLS session drives.
Only `cancel` is atomic and allowed from a different thread.
`destroy` requires host-level quiescence of all possible callback,
cancel and NativeIO completion entrants. A borrowed descriptor is
released **only after** the exact TLS session that uses it is destroyed;
a callback-active check alone cannot authorize early release.
Neither the helper nor a naked pointer protects callers that enter
after its object was freed.

**Security boundary:** transport EOF is NOT TLS `close_notify`.
Closing the TCP receive half after a partial TLS record must not be
misreported as authenticated TLS shutdown or application data.
CNet's close-notify detection, decrypted-plaintext demand, write-source
retention across `AGAIN`, and independent NativeIO cancel/close
completion barrier remain additional CNet integration responsibilities.

## Qualification

The independent installed SDK test `crypto.installed.cipher_bio_tls12_tls13`
runs both a pure ring conformance and **real GmSSL-backed opaque C11
TLS 1.2 and TLS 1.3 sessions**:

- fixed-capacity input all-or-nothing, no partial copy on overflow;
- saturated output callback returns AGAIN, FIFO wraparound correct,
  no duplicate or stale data on bounded `take`;
- queued cipher survives transport EOF and is then followed by actual
  read callback EOF (never fabricated close_notify);
- cancel rejects later send/receive/feed, while outstanding IO lease
  blocks destruction;
- real server/client authenticated handshake via TWO cipher BIOs,
  small bounded movement chunks, exact TLS plaintext delivery and
  orderly close_notify;
- TLS session destruction precedes BIO lease release and terminal free.

CI runs the test on Linux/macOS/Windows with the one imported
`Salts::TLS` DSO and also under fully source-instrumented
ASan/UBSan/LSan and repeated TSan. The target is installed as
`Salts::TLSCipherBIO` and does not alter the **23-symbol** public
Crypto TLS+SM3 shared ABI or its SONAME.

## Explicit remaining gates before CNet cutover

- Full CNet TLS client/server **capability admission** is not complete:
  CA directory/system roots, P-384/ECDSA, certificate client-auth,
  ShangMi/TLCP, wider ALPN, RFC5929 binding and various failure-stage
  semantics remain in [Crypto #18](https://github.com/qigao/crypto/issues/18).
- The helper has no plaintext-pending query or low-level
  `tls_get_peer_close_notify` facade; don't fake these values.
- CNet and NativeIO still have to integrate this via a **separate test
  branch**, map exact errors and prove retained slice ownership,
  zero-copy backing lifetimes, simultaneous cancel/close/receive,
  and installed C11/C++17 final linkage on all target platforms.
- SaltsCore shared foundation owner [#20](https://github.com/qigao/salts-core/issues/20)
  and canonical Crypto master integration [Draft #25](https://github.com/qigao/crypto/pull/25)
  remain open. Production `GmSSL::GmSSL`, vcpkg-cache pin
  `3.2.0#13` and consumers are unchanged.
