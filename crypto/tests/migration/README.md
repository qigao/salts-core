# Original qigao/crypto PR #43 mTLS source handoff

The original workflow has been preserved byte-for-byte as
`mtls-qualification-original.yml`, source blob e0ca7d6b66b0d286d6d72c3f3e7416d179c9a62a.
It is a HISTORICAL TEST INPUT, not an active TLS provider.

The historical workflow and source still use the GmSSL backend and
pinned patch overlay. They are kept to prevent loss of original TLS1.2/1.3
mTLS tests while the SaltsCore native third-party-free TLS/TLCP engine
is implemented and independently verified.

Do not copy this workflow verbatim to .github/workflows or treat its
success as native TLS provider acceptance. Real installed C11/C++17,
Python-only TLS peers, real IOCP, epoll/kqueue, and negative security
tests must be re-qualified in the new SaltsCore source-owner graph.

Source repository: qigao/crypto, PR #43, head 0b4ee094ce8002e6169ce231028107eba63588c6.
