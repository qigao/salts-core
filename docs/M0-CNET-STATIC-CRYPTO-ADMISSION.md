# CNet static Salts::Crypto — M0 isolated admission

**Qualification only; not a production TLS migration.** Source baseline:
SaltsCore master `cfc1ed0d24364689b10ef7a85cff28d8776eb7db`;
Crypto static master `b09a981c1922db01070509c98511d291c98ee81e`.
Existing independent CNet/NativeIO test branches #26–#36 used an older shared
`Crypto::TLS` + `Crypto::TLSCipherBIO` arrangement. Their application
behavior evidence is useful, but neither their installed CMake target checks
nor their ELF/Mach-O/PE shared library imports qualify today's canonical
`Salts::Crypto` STATIC archive.

## M0: separate static link / strict admission

Set `SALTSCORE_CNET_STATIC_CRYPTO_ADMISSION=ON` with `BUILD_TESTING=ON`,
`SALTSCORE_BUILD_CNET=OFF`, and an independently installed Crypto 0.1 prefix
in `CMAKE_PREFIX_PATH`. The CMake test *requires* exactly one imported STATIC
`Salts::Crypto`, rejects split Crypto/GmSSL imported target aliases and
links only `Salts::Core` and `Salts::Crypto`.

The exact existing public CNet client/server policy structures feed an
internal, test-only admission function. TLS1.2/1.3 DNS+explicit CA/server
cert+key policy may be accepted; production mode and unsupported/default
policy are refused **without modifying the caller output**. Negative cases
cover system CA, CA directory, IP literals, client mTLS, server client-auth,
ALPN, ShangMi TLS1.3, TLCP1.1, protocol range and undersized rings.
The C11 executable instantiates one real static Crypto TLS provider and
CipherBIO, then destroys both while quiescent. The C++17 consumer loads the
same canonical static ABI. **Neither test performs a network handshake.**

## Mandatory gates before changing production CNet

| Gate | Minimum evidence | Current status |
| --- | --- | --- |
| Native static object ownership | One Crypto archive physically containing the exact GmSSL object set, no duplicate global TLS/SM3/ASN.1 symbols; final CNet link map and PE/ELF/Mach-O checks | New M0 candidate requires CI |
| TLS 1.2/1.3 | Correct client/server negotiation, protocol range, ECDSA/P-384, nonblocking AGAIN and partial record handling | Partial older shared qualification; static full Owner **not done** |
| Trust & identity | explicit file/directory CA, OS trust, DNS SAN, IP SAN, wrong root/name rejection and no fallback | Not complete in static facade |
| ALPN | original CNet 255-byte binary protocols, negotiation and exact introspection | Crypto text-only 127-byte subset; **not parity** |
| mTLS | optional client key/cert, required server client-auth, absence, wrong-issuer and revocation policy as applicable | **not parity** |
| ShangMi/TLCP | RFC8998 SM2/SM4/SM3 TLS1.3 and TLCP1.1 dual certificates, negative protocol/cert tests | **not parity** |
| TLS inspection | version, cipher, verified chain/fingerprint, signature-aware RFC5929 binding, RFC9266 exporter, close_notify and buffered plaintext/EOF | **not parity** |
| NativeIO | real epoll/kqueue/IOCP, retained send slice through matching terminal, cancel/close/timeout races, no double-free/early unload | Previous shared isolated tests are not static proof |
| Consumers/platforms | Linux/Windows/macOS C11/C++17, Linux ASan/UBSan/LSan & TSan; Android/iOS where supported; exact HEAD and relocated SDK | **not complete** |

Next steps are (1) expand the Crypto-owned C11 opaque TLS options and capabilities
to cover the missing features with direct tests; (2) rerun the original
CNet Owner/NativeIO application using a **single** static Crypto-backed
candidate, independent of the original GmSSL CNet process; (3) perform
source-sanitized, cross-platform and installed consumer/link-map gates;
(4) only then replace the production `src/cnet/cnet_tls.c` and
`GmSSL::GmSSL` linkage atomically.

Never install an alternate CNet provider, create a backward-compatibility
alias, silently downgrade security profiles, or remove the historical GmSSL
production pin before these gates pass.

## M1: real CNet TLS private state on one static Crypto archive

With `SALTSCORE_CNET_STATIC_CRYPTO_TLS_STATE_TESTS=ON` and
`SALTSCORE_CRYPTO_TEST_FIXTURES` referencing the exact Crypto source
`tests/fixtures`, build and run
`saltscore.cnet_crypto_static_state_tls12_tls13`.
The CNet private API signatures and an independently qualified opaque
Crypto/CipherBIO engine are exercised with **actual TLS 1.2 and 1.3**
client/server sessions, the pinned explicit CA and server leaf/key.
It checks wrong identity/unsupported range, negotiated version and cipher,
authenticated peer fingerprint, matched exporter, bounded encrypted record
movement, deferred plaintext reads, repeated close_notify suppression,
real EOF vs close_notify, and cleanup.

This is a **test-only CNet TLS state**: no GmSSL include in the TLS candidate,
no CNet production implementation, no direct imported GmSSL target, and no
alternative Crypto shared provider. It does not claim system CA, IP SAN,
client mTLS, ALPN binary names, ShangMi/TLCP or full NativeIO owner parity.
