# M4.4 — Verified TCP TLS handshake and CA/SAN rejection (Draft)

Stacked on SaltsCore PR #16 real TCP loopback and PR #15 provider-backed
`Salts::CNet`. This adds a genuine TLS-over-NativeIO network test. It
uses the existing pinned GmSSL provider and the **unchanged** fixture material
from the original Salts CNet conformance suite. No fixtures are regenerated.

## Fixture provenance (test-only)

Files copied byte-for-byte from qigao/salts:
- `cnet/tests/fixtures/p384-test-root-cert.pem`
- `cnet/tests/fixtures/p384-test-wrong-root-cert.pem`
- `cnet/tests/fixtures/p384-test-server-cert.pem`
- `cnet/tests/fixtures/p384-test-server-key.pem`

Original Salts source suite: `cnet/tests/cnet_tls_test.c`, especially the
public CNet/NativeIO P-384 issuer / P-256 ECDSA-SHA384 tests.

Test-only PEM private key is a *public repository fixture*, not a production
secret. Fixtures are source test inputs and never installed as SDK credentials.

## Run logic

`test_cnet_tls_ca_san.c` initializes actual CNet TCP client and listener
owners, two private NativeIO progress paths, a real TLS server context, and
the public `tls://127.0.0.1:<ephemeral>` client connection.

Three independent scenarios are executed with explicit bounded deadlines:

1. **Correct CA + DNS SAN localhost:** both CNet Owner peers must publish
   `CONNECTED`, report negotiated TLS version/cipher, expose a verified
   SHA-256 peer-certificate fingerprint, and exchange one authenticated
   application byte using a retained send buffer. Normal shutdown must settle
   once without failure.
2. **Wrong root CA:** the public CNet client must reach terminal failed
   state at the handshake stage, without emitting `CONNECTED` or plaintext.
3. **Wrong DNS identity:** a trusted chain with an incorrect `server_name`
   must be rejected just as strictly, without a plaintext downgrade or send.

All server/client/listener/TLS-context lifetimes are cleaned up on either
success or failure. A versioned TLS-profile mismatch and old Salts binary are
never substituted.

## Status and limits

Only a green **exact-head CNet provider-enabled CI** run qualifies these three
scenarios, not a default Core SDK build. The CI explicitly selects
`saltscore.cnet_tls_ca_san`, on top of other CNet and GmSSL tests.

This proves loopback TLS/CA/SAN behavior using the same GmSSL on both peers.
It does **not** yet prove independent third-party peer interoperability,
TLS 1.2/1.3 negative matrix under external peer implementations,
Windows/macOS enabled-provider builds, KCP/UDP/WebSocket or production ABI
cutover. Keep all PRs Draft pending separate conformance and sanitizers.
