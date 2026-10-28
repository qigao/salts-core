# Crypto native TLS 1.2 / TLS 1.3 server interoperability — M3.2.2

**Status: Draft / exact-head qualification. No production CNet, vcpkg-cache,
registry-pin or Crypto ABI release changes.**

## Independently observed failures

The initial opaque Crypto server could already complete a native GmSSL
client/server memory handshake. Real OpenSSL 3 `s_client` against a
Crypto TLS 1.2 server instead sent fatal `handshake_failure` (Alert
level 2, code 40). A comparison with the pinned original GmSSL CLI
server identified missing RFC 5746 initial-handshake secure
renegotiation signaling. The narrow Crypto private backend adapter now
enables that signaling for the **TLS 1.2 server** only. It does not
enable application-triggered renegotiation, weaken certificate checks
or skip Finished verification.

OpenSSL 3 TLS 1.3 clients that explicitly send an initial P-256
KeyShare complete TLS, authenticated HTTP and same-session exporter.
With the client default group selection, the GmSSL P-256-only server
sends a legitimate `HelloRetryRequest`. A separate OpenSSL 3 API
client showed one HRR and two ClientHello sends, but GmSSL's
`tls13_recv_client_hello_again` originally saw a TLS record
`type=20, legacy version=0x0303, payload length=1, byte 0x01`
and incorrectly tried to decode it as ClientHello2. This is the
RFC 8446 middlebox compatibility dummy ChangeCipherSpec. It is **not**
a client handshake message or an instruction to ignore validation.

## Native fix and ownership

The only protocol-format change is in the Crypto-owned
`providers/gmssl/hardening/tls13-hrr-compat-ccs.cmake` overlay,
applied strictly after the historical 40 source adaptations and
five separate Crypto undefined-behavior fixes. It adds a private
`TLS_CONNECT.crypto_hrr_ccs_seen` counter; the installed public
opaque Crypto handles, existing TLS v1/1.1/1.2 C11 ABI, three SM3
functions, the single GmSSL-bearing DSO and host loader remain
unchanged.

In **HRR/ClientHello2 state only**, the protocol fix:

1. Requires the configured `accept_change_cipher_spec` policy,
   full TLS record length of six bytes, legacy 0x0303 version,
   record type 20 and the exact single payload byte `0x01`.
2. Admits at most **one** validated CCS per private TLS Session,
   cleans the record and returns `TLS_ERROR_RECV_AGAIN` so the
   standard handshake state machine retries the real ClientHello2.
3. Rejects duplicate CCS, an invalid one-byte CCS payload and
   unexpected non-handshake records using TLS fatal Alerts. It does
   **not** add a generic `continue` loop, ignore unknown records
   or bypass transcript hashes, client certificates or Finished.
4. Leaves HRR/ClientHello2's strict consistency checks (legacy
   session id, cipher suites, supported extensions, key share, cookie,
   duplicate extension rejection and digest update) intact.

The source assembly receipt retains `git_patch_count=1`,
`source_adaptations=39`, `crypto_hardening_edits=5`, plus a separate
`crypto_protocol_fixes=1` field and SHA-256 of resulting TLS source
and private header. The upstream GmSSL SHA and original vcpkg source
objects have not changed.

## Real positive/negative gates

The installed C11 Crypto server is exercised against **independent
OpenSSL 3** in Linux/macOS:
- TLS 1.2 ECDHE-RSA + TLS 1.3 direct P-256, authenticated HTTP,
  verified certificates and actual same-session exporter.
- TLS 1.3 default-group OpenSSL HRR, independently confirmed from
  the *public HRR marker* and two ClientHello messages, then real
  completed HTTP and same-session exporter.
- An isolated loopback TLS-record mutation proxy duplicates an
  otherwise correct post-HRR CCS or corrupts its single payload byte.
  The proxy verifies that a genuine HRR occurred and its mutation
  was transmitted; the OpenSSL client and Crypto server must BOTH
  fail, and **neither may publish an exporter secret file**. No
  suppression, xfail, or treating an uninjected failure as a pass.
- Original installed C11 SDK, single-owner ELF/Mach-O/PE audit,
  native ASan/UBSan/LSan and TSan callback/DSO unload conformance
  remain required, including negative mutation under sanitizer.

Diagnostic builds can emit *only* state/handshake type/length metadata.
They are opt-in, not installed by release. Never log TLS records,
transcripts, certificate bytes, randoms or derived Exporter material.

## Outstanding production boundaries

These tests do not establish full CNet client/server capability or
native system CA path support, SM2/ShangMi/TLCP, arbitrary TLS groups,
client-auth policy, RFC5929 server-end-point, or the CNet NativeIO BIO
lifetime. [Crypto #18](https://github.com/qigao/crypto/issues/18)
and [salts-core #20](https://github.com/qigao/salts-core/issues/20)
remain independent production blockers. Do not merge or release
this Draft on the basis of a subset of green CI legs.
