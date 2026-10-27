# M3 — bounded text ALPN through sole static Salts::Crypto

**Draft qualification only, stacked on full Original Owner Draft #54.**
The production `src/cnet/cnet_tls.c`, its direct `GmSSL::GmSSL` link
and pinned GmSSL version are unchanged. No dynamic Crypto Provider,
legacy compatibility alias, or fallback is introduced.

## Exact admitted subset

The existing `cnet_tls_client_config` and `cnet_tls_server_config`
continue to be the public input contract. The candidate private
`cnet_crypto_owner_admission` only accepts ALPN exactly representable
by `crypto_tls_session_set_alpn_v1_1`: 1–8 NUL-terminated **printable
ASCII** protocol names, length 1–127 bytes each, combined ALPN wire-list
size at most 128 bytes (one length octet per name), no duplicates.
An absent list must have both pointer NULL and count zero.

Malformed pointer/count and empty names return an explicit invalid-input
error. Longer names, over-budget lists, binary/nonprintable names,
duplicate labels and more than eight names explicitly fail. All rejections
leave the caller's output descriptor unchanged and do not create a TLS
session. These are *deliberately stricter* than CNet's historical public
255-byte per-label ALPN policy: **this is not full production parity**.

Upon admission, the CNet reusable context deep-copies the names. A TLS
session sets ALPN exactly once before the handshake, then inspects the
authenticated negotiated name. The CNet state retains the selected label;
its pointer is borrowed only until state destruction. No-ALPN sessions
must still return `SALTS_ENOENT`, never a fabricated name.

## Executed acceptance requirements

- Real TLS 1.2 and TLS 1.3, each **with and without ALPN**, using the
  installed static Crypto backend, explicit CA and server key.
- Client offers `h2`, `http/1.1`; server offers `h2`. Both independently
  query exact negotiated `h2` with stable size.
- Full unchanged original CNet Owner and real epoll/kqueue/IOCP version
  and cipher metadata, user plaintext, RFC 9266 exporter, NativeIO
  retained buffers and close terminal under both ALPN variants.
- Negative-first policy matrix covers ALPN overlong name, duplicates,
  >8 entries, >128 cumulative bytes, nonprintable names, malformed
  pointer/count, and output unchanged on rejection.
- Repeat all three native OS CTests and linked binary import/symbol
  audits on one exact HEAD. Linux sanitizers and multi-module static
  Crypto owner proof still require separate follow-up evidence.

Out of scope: arbitrary binary ALPN and 255-byte CNet protocol labels,
system CA/CA path and IP SAN, optional/required mTLS, P-384/ECDSA,
ShangMi TLS1.3, TLCP1.1 dual certs, RFC5929 signature-aware binding,
close/cancel races and a single physical Crypto owner across multiple
application DSOs. **No production dependency may be removed yet.**
