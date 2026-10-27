# M4 — static Crypto/CNet real CA and DNS SAN negative handshake

Draft, test-only, stacked on M3 #56, original real CNet Owner M2 #54
and static TLS M0/M1 #53. Historical production CNet/GmSSL unchanged.

## Wrong CA — CNet client authentication rejection

The private CNet TLS adapter drives real TLS 1.2 and TLS 1.3 encrypted
client/server handshake records: the server certificate and requested
SNI localhost are correct, but the client trusts an unrelated valid
test PEM root (tests/fixtures/p384-test-wrong-root-cert.pem). Both
contexts and real TLS sessions must construct. Require ciphertext to
flow and CLIENT handshake rejection (not timeout/server-only failure),
no authenticated client state, and no surviving provider sessions
after cleanup.

## Wrong DNS SAN — independent SNI-neutral peer

Pinned GmSSL performs SAN-matching server certificate selection based
on requested SNI before sending the certificate. A local GmSSL server
therefore rejects wrong.example *server-side* before client verification.
Such an event is NOT evidence of client SAN verification.

Instead, the test harness external_san_peer.py runs an independent
Python ssl/OpenSSL server in a separate process, presenting the exact
valid localhost certificate unconditionally. It records client SNI but
never rejects/selects a certificate on that basis. The native original
CNet/NativeIO executable in another process links exactly one STATIC
Salts::Crypto; it never loads an OpenSSL library itself.

For each TLS version, require success when connecting to localhost
under the same CA and cipher configuration. Then request wrong.example
with that SAME trusted CA and certificate. The client must observe
CNET_CONNECTION_FAILED and never CONNECTED. The server must accept TCP
and record the wrong.example SNI. This positive control prevents an
unrelated cipher, trust, transport or protocol failure being counted as
SAN rejection.

## Remaining hard blockers

These test-only positive/negative cases must run on Linux epoll,
macOS kqueue and genuine Windows IOCP, repeated 3x with static archive
symbol checks and final ELF/Mach-O/PE imports.

System CA, hashed CA paths, IP SAN, client/server mTLS, P-384/ECDSA,
TLCP/ShangMi, RFC5929 signature-aware binding, arbitrary binary ALPN,
source sanitizer and multiple DSO physical owner have NOT been proven.
Do not delete existing GmSSL production dependency or cut over CNet.
