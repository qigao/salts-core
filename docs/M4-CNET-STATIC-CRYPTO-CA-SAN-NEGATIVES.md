# M4 — actual CA and DNS SAN authentication failure on sole static Crypto

This is a **test-only, standalone negative-first gate** stacked on
bounded-ALPN M3 Draft #56, original real Owner M2 Draft #54 and static
CNet TLS state M0/M1 Draft #53. No production `cnet_tls.c`, pin,
provider fallback, ABI or public CNet security policy is changed.

For **both TLS 1.2 and TLS 1.3**, drive two independent real CNet private
opaque Crypto client/server handshake pairs with bounded encrypted BIO.
The client policy is accepted and both actual TLS sessions must be
constructed; client network records must actually move. Refuse:
- A valid but unrelated independent trust root
  (`tests/fixtures/p384-test-wrong-root-cert.pem`) while connecting to
  the otherwise valid `localhost` server certificate.
- A valid installed trust root while demanding `wrong.example` as the
  fully verified DNS SAN identity (same effective source and SNI policy).

In neither case may the client reach `handshake_complete` or obtain a
successful authenticated connection. A mere configuration refusal, zero
ciphertext, timeout, or server-only failure does **not** qualify.
The original positive (valid CA + verified localhost) with and without
ALPN, and the full real epoll/kqueue/IOCP CNet Owner regression, continue
in the same native 3OS workflow. Repeat each static state CTest three
times with fixed original Crypto SHA, verified GmSSL 3.2 port, no alternate
native provider and exact-head import audits.

These tests do NOT qualify OS system roots, hashed CA path, IP SAN, client
or server mTLS, TLCP/ShangMi, revocation, binary ALPN, RFC5929 or multiple
DSO process-level Crypto owner. Those are still hard production blockers.
