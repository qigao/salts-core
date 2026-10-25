# M4.7 — Independent OpenSSL 3 peer interoperability (Draft)

Goal: prove SaltsCore CNet/GmSSL can exchange real TLS handshake and
application records with an **independently implemented OpenSSL 3** peer.
This is not GmSSL-to-GmSSL loopback and uses no OpenSSL library inside
`SaltsCore::CNet`. Stacks on Draft PR #19.

## Exact test boundary

- `tests/test_cnet_openssl_client.c` builds with **only**
  `SaltsCore::CNet` and its already-pinned private GmSSL provider.
- `tests/interop/verify_openssl_server.py` verifies the OpenSSL 3 executable
  identity, launches an external `openssl s_server -www` on native
  `127.0.0.1` sockets and passes explicit trust/hostname conditions to
  the CNet client. It cleans up every server process, checks deadlines and
  reports the external peer's logs on failure.
- TLSv1.2 and TLSv1.3 are each exercised under three conditions:
  **verified CA + correct localhost SAN** (accept and fetch real HTTP
  application data), **wrong root CA** (reject without authenticated state),
  and **correct root + wrong DNS name** (reject without authenticated state).
- The original Salts CNet P-384 issuer / P-256 ECDSA-SHA384 leaf certificate
  fixture and deliberately incorrect root CA are reused byte-for-byte.
- For each verified connection, the CNet public owner checks the negotiated
  version/cipher, verified leaf SHA-256, a retained send buffer with immediate
  caller release, one on_send callback, and a real `HTTP/` response from the
  OpenSSL server. The failure paths require a handshake-stage terminal,
  never an authenticated CONNECTED callback, send or plaintext.

## CI

The Linux pinned-Provider workflow adds `openssl` to the explicit host
packages and sets `SALTSCORE_ENABLE_OPENSSL_INTEROP=ON`. It must both
pass the whole CTest suite and run `saltscore.cnet_openssl_interop`
explicitly (no-tests=error). The CMake test is opt-in and fails to configure
if the required independent OpenSSL/Python3 programs or actual
`SaltsCore::CNet` target are missing. Windows/macOS can opt in once an
independent peer executable is grounded for those images; their current
Provider jobs are unchanged.

## Remaining gates

The OpenSSL 3 command-line executable is a distinct TLS implementation,
but this test shares localhost and source-controlled certificate fixtures.
It does not qualify internet/public PKI, production certificate issuance,
third-party SCTP/libwebrtc, browser WebSocket, or full security scrutiny.
The OpenSSL version must be recorded from the exact CI run.
Linux TSan/ASan/UBSan, IOCP/kqueue cancellation/shard races, actual UDP/KCP
wire traffic, Android/iOS, one-DSO symbol ownership and downstream Salts
ACE remain independent gates. No production cutover or merge.
