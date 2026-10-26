# M4 — Production CNet Owner: opaque Crypto candidate (qualification only)

Stacked on Draft SaltsCore #28, after the successful installed Windows IOCP
handshake, real TCP send terminal, 64-cycle wake/cancel and PE owner graph.
**Draft / DO NOT MERGE / no CNet production cutover.**

## What this branch actually qualifies

`tests/cnet-crypto-owner/policy.c` compiles directly against the real
current `include/cnet/cnet.h` public TLS configuration structures and
the independently installed opaque `crypto/tls.h`. It creates no socket,
no TLS session, no GmSSL state, and has no new public ABI. Its *only*
purpose is a negative-first, fail-closed admission gate before a
future CNet Owner adapter can receive a policy.

Permitted **candidate subset**, pending real CNet application parity:

- Explicit TLS 1.2 **or** TLS 1.3; no negotiated range/default hidden fallback
- Client: required explicit CA file and effective verified DNS hostname;
  matching explicit SNI override, if any; mandatory certificate/SAN
  verification remains Crypto's responsibility
- Server: explicit PEM certificate and private key; no client auth
- Provider handles and `crypto_tls_session_options` contain no direct
  GmSSL types or paths into the incumbent `GmSSL::GmSSL` library

Denied with `SALTS_ENOTSUP` before any TLS state allocation: implicit
platform/system trust, hashed CA directories, client mTLS certificates,
required server-side client auth, ALPN, ShangMi/TLCP, CNet TLS version
range/default and unknown protocol or security profile values. Invalid
sizes, missing identities and missing certificate/key material are rejected
with `SALTS_EINVAL`. On *every* failure, the output descriptor is fully
zeroed, not partly admitted. Strings remain borrowed only for immediate
`crypto_tls_session_create()` use; no worker may persist them.

The new installed C11 test executable links exactly
`SaltsCore::Core` and `Crypto::TLS`. It does **not** link
`SaltsCore::CNet`, GmSSL, or a legacy Salts DSO. This avoids accidentally
loading TWO independently stateful GmSSL implementations into one
candidate process. The existing full production CNet continues to use
`GmSSL::GmSSL`; production `vcpkg-cache gmssl 3.2.0#13` remains pinned.

## Next source-level test branch gates

1. Compile a **second, test-only CNet DSO variant** using
   `cnet_tls_crypto.c` instead of the incumbent `cnet_tls.c`.
   The variant must link `Crypto::TLS` plus host `Crypto::TLSCipherBIO`
   and must **not link `GmSSL::GmSSL`**, statically or transitively.
   The production target and its installed ABI remain untouched.
2. Map the existing `cnet_owner.c` TLS state machine to one serialized
   Crypto session and one retained NativeIO request generation. Preserve
   `mem_buffer_t`/`mem_slice_t` until actual completion even on
   cancellation, callback `AGAIN`, close, peer EOF, and DSO drain.
3. Run two process-isolated application programs against the **same**
   loopback TLS fixtures, input sequence and failure injection: one
   incumbent CNet Owner binary, one test-only Crypto-backed CNet Owner
   binary. Compare error/status/ordering/shutdown and resource disposal;
   do not import both backends into the same process.
4. Reject or explicitly implement remaining CNet capabilities:
   CA directories, OS trust, mTLS, ALPN, RFC5929 binding, peer chain,
   negotiated metadata, plaintext pending, TLCP/ShangMi, and
   feature-specific identity semantics. Returning fabricated success
   or substituting TLS 1.2 for requested TLS 1.3 is forbidden.
5. Prove Windows true IOCP plus Linux epoll/macOS kqueue, ASan/UBSan/TSan,
   Installed SDK C11/C++17, one physical DSO owner and lifetime barriers
   on the **same exact candidate HEAD**.

The admission-test CI turning green does not check off any of items 1–5.
Do not merge any migration PR or modify the production GmSSL pin.
