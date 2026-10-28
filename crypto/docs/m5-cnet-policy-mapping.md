# Crypto #42 M5: CNet policy mapping audit (not a production switch)

This audit compares the **existing** CNet client/server TLS contract in
`qigao/salts-core/include/cnet/cnet.h` with the additive C11
`crypto_tls_session_create_auth_v1_3` and `tls_ext.h` contract. It is a
source/API audit, **not** a claim that the production CNet Owner uses Crypto.
CNet still links its original GmSSL implementation. No provider/fallback or
compatibility adapter is authorized by this document.

| Original CNet input/behavior | Crypto v1.3 status | Required admission |
|---|---|---|
| STANDARD TLS 1.2 or TLS 1.3 with explicit CA PEM | Candidate exists | Preserve the exact selected protocol, SAN, trust and cipher constraints; never downgrade |
| Client PEM cert + private key (+ password) | Candidate exists | Both required together, strictly bounded; fail create on mismatch/invalid key |
| Server PEM identity/key | Existing v1.0 owner | Reuse original server identity fields, no second credential owner |
| Server `client_auth=REQUIRED` + explicit `ca_file` | Candidate exists | Enable peer CertificateRequest, require verified client chain, clientAuth EKU and CertificateVerify before application I/O |
| Server `client_auth=NONE` | Existing v1.0 behavior | Do not silently map REQUIRED to NONE |
| CNet `ca_path` (hashed trust directory) | **Not supported** | Reject; no trust-dir-to-CA-file guessing |
| NULL CA selecting the platform trust store | **Not supported by v1.3** | Reject; do not fall back to another TLS provider |
| CNet IP SAN | **Not supported by Crypto v1.1 DNS SAN-only gate** | Reject IP identity requests until native exact IP SAN handling is qualified |
| CNet `server_name` identity/SNI override | **Partial** | v1.3 uses one hostname for both verified identity and SNI. Do not silently conflate independently configured identity requirements |
| CNet ordered ALPN | **Partial** | Crypto supports 1–8 printable ASCII names, max 127 bytes/name, total <=128 bytes. CNet permits names up to 255 bytes; never truncate/drop preferences |
| Negotiable standard TLS 1.2–1.3 version range | **Partial** | v1.3 constructor specifies one protocol per session; do not emulate a range by opportunistic retries |
| SHANGMI_TLS13 RFC 8998 | **Not supported** | Reject exact profile; no standard-TLS substitution |
| TLCP11 SM2 signing + encryption identities | **Not supported** | Reject; requires distinct dual-cert/key profile |
| `cnet_tls_client` / `cnet_tls_server` reusable context leases | **Not qualified** | Retain an immutable policy snapshot and one bounded physical static owner through all sessions |
| Original NativeIO epoll/kqueue/IOCP and CipherBIO buffers | **Not qualified end-to-end for mTLS** | Prove request/completion owner, borrowed callback lifetime and close/cancel overlap on the real CNet Owner |
| TLS fatal-alert delivery, EOF/close_notify and half-close | **Not qualified for every reject path** | A terminal EOF is fail-closed, but is not evidence of correct peer alert publication |
| CNet shared binary, plugins and multiple DSOs | **Not qualified** | Prove one **physical** `Salts::Crypto` static implementation, not just one installed archive on disk |

## Real acceptance boundaries

- `qigao/crypto` PR #43 is the **Draft** Crypto API and Python `ssl`
  handshake-candidate gate for TLS 1.2 and TLS 1.3. Python is an external
  peer, not a runtime TLS Provider inside the C library; no OpenSSL C
  peer, CLI or OpenSSL dev library is added by that gate.
- SaltsCore M0–M4 Draft PRs #53/#54/#56/#57 are existing original-Owner
  baselines, **not** evidence of REQUIRED mTLS policy parity.
- Repeat exact-head Linux, macOS, Windows, cancellation/CipherBIO
  lifecycle, required peer-auth negatives and alerts on the original Owner
  before switching CNet. Distinguish Windows compiled ABI from true IOCP
  mTLS handshake/runtime acceptance.
- Do not remove CNet direct GmSSL, publish, or merge any production
  migration until ALPN, CA/SAN, mTLS, TLCP, NativeIO and multi-DSO gates
  are independently satisfied. No fallback, no split Crypto Provider.
