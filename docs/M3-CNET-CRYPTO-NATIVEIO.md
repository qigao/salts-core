# M3.4 — Real NativeIO + Crypto TLS Cipher BIO acceptance

**Candidate test-only integration. Not a production CNet TLS backend switch.**

This branch keeps the original `saltscore_cnet` target, `src/cnet/cnet_tls.c`,
`GmSSL::GmSSL` target and `vcpkg-cache gmssl 3.2.0#13` pinned
production source **unchanged**.

Instead, `tests/crypto-nativeio/` is an independently configured C11
consumer of TWO *actually installed* packages:

- `SaltsCore::Core` and `SaltsCore::NativeIO` from the independent
  `qigao/salts-core` SDK, with production CNet **disabled**.
- `Crypto::TLS` and the host-only `Crypto::TLSCipherBIO` from the exact
  immutable Crypto Draft #26 commit
  `369b4dd7b2863cca2be8f939a6c5a834ecf5663d`.
  Private GmSSL is rebuilt from the pinned original
  `7c9f02904ef33e59c87b4f16621cc8fd434e7579`, with the
  original historical patches and separate Crypto safety/HRR fixes.
  The installed Crypto shared library is still the **only GmSSL owner**.

## Actual NativeIO ownership handoff

The test uses two real connected **IPv4 loopback TCP sockets**, attached
to one platform-native `native_io_backend`: epoll on Linux and kqueue on
macOS. No fake completion queue, loopback BIO shortcut or manually
fabricated NativeIO completion is accepted.

Each endpoint has exactly one opaque Crypto TLS session and one bounded
Crypto ciphertext BIO. A single Owner drives both TLS and NativeIO
`submit/observe` calls:

1. TLS writes ciphertext into its own bounded Crypto output ring.
2. The Owner copies at most 193 bytes into a real `mem_buffer_t`
   allocated by the **shared SaltsCore Core runtime**.
3. NativeIO receives that buffer as a borrowed `TCP_SEND` backing:
   the Owner neither changes nor releases the bytes while a request
   exists; only a matching terminal completion allows release.
   A partial send advances the accepted prefix and resubmits the still
   retained remainder on the same immutable buffer.
4. The peer's real `TCP_RECV` completion delivers at most 331 bytes;
   the Owner makes an **all-or-nothing** BIO ciphertext feed and then
   drives the peer's opaque TLS session. The receive backing remains
   writable by NativeIO until its terminal completion, never earlier.
5. After real TLS 1.2 and 1.3 handshakes the test proves exact
   application plaintext, then runs bounded `close_notify`.
6. Shutdown closes admission and cancels all retained NativeIO
   requests, **observes every terminal completion**, then destroys
   Crypto sessions, releases their borrowed BIO callbacks and frees
   BIO buffers. A successful `native_io_backend_cancel()` by itself
   does NOT authorize freeing any backing memory.
7. Before release, `crypto_tls_cipher_bio_destroy()` must return
   `CRYPTO_TLS_BUSY`. Core foundation buffer ownership and TLS
   state never escape via a raw GmSSL pointer.

The final link audit requires `crypto_tls` and
`saltscore_native_io`, and forbids a second GmSSL or
`crypto_provider` dynamic dependency.

## What this first gate does NOT claim

- It is a **test-only CNet-style NativeIO adapter**, not an integration
  of the production `cnet_tls_state` into the new Crypto ABI.
- It does not replace CNet's TLS X.509 trust-directory/system-store,
  ShangMi/TLCP, P-384/ECDSA, mTLS client authentication, RFC 5929
  server-end-point, plaintext-pending/probe-close APIs or full
  protocol policy. Those capabilities require explicit admission.
- Windows IOCP with overlapped sockets is a **separate mandatory**
  platform gate; POSIX sockets or readiness emulation are not an
  acceptable surrogate.
- Nonzero partial-send handling is implemented, but a forced *OS-level*
  short `TCP_SEND` completion must be separately injected/verified
  before marking the corresponding failure mode accepted.
- NativeIO close/cancel **simultaneously** with TLS callback drives
  remains a later real concurrency/TSan gate; this initial test is
  one-owner serialized. Crypto CipherBIO's independent cross-thread
  cancellation/TSan gate already exists in Draft #26.
- The full SaltsCore foundation owner and CNet plugin acceptance remains
  tracked in [salts-core #20](https://github.com/qigao/salts-core/issues/20);
  production transition additionally requires [Crypto #18](https://github.com/qigao/crypto/issues/18).

CI uses a fresh installed prefix; it neither updates the production
CNet target nor mutates its registry baseline. Do not merge any Draft
or switch production until all explicit unmet gates have been qualified.
