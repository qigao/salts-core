# M2 — real original CNet Owner with sole static Salts::Crypto

This is a separate Draft test-only qualification stacked on PR #53. It
compiles the complete original native CNet Owner, resolver, URI, WebSocket,
events and transport into ONE executable, replacing only the private TLS
source with the reviewed opaque Crypto/CipherBIO adapter. The candidate
links exactly one independently installed static Salts::Crypto archive and
one native Core/NativeIO foundation. It does not link or load production
Salts::CNet or a separate GmSSL target, even if those build as siblings.

Explicit build options: BUILD_TESTING=ON, SALTSCORE_BUILD_URI=ON,
SALTSCORE_BUILD_CNET=ON, and
SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST=ON. Pass
SALTSCORE_CRYPTO_TEST_FIXTURES with the exact pinned Crypto source fixtures,
and CMAKE_PREFIX_PATH pointing to its installed static SDK.

The real application test runs TLS 1.2 and 1.3 with platform native epoll,
kqueue or IOCP, explicit CA and DNS SAN, fingerprint/exporter checks,
retained send buffer through NativeIO completion, exact received plaintext,
retained received Core slice across both client destructions, and orderly
terminal close. No fallbacks, compatibility aliases or mock NativeIO.

Full CA directory/system trust, IP SAN, binary ALPN, client/server mTLS,
P-384/ECDSA, RFC5929, ShangMi TLS1.3 and TLCP1.1 still block production
switch. This test is not a DSO-unload proof and does not remove historical
GmSSL from the incumbent CNet.
