# Vendored libecc

- Source: https://github.com/libecc/libecc
- Commit: `6e8f214f41f65d5f30b04da75472f9c24f2100db`
- Commit date: 2026-07-17
- License: dual BSD/GPLv2; Crypto redistributes it under the BSD terms in
  `LICENSE`.

The selected upstream include and source files in this directory are
unmodified files from a snapshot produced with `git archive`; this
`UPSTREAM.md` records local provenance.

`CMakeLists.txt` owns the private Ed448-only OBJECT target; the repository
root folds its objects into the single installed `Salts::Crypto` static archive.
The explicit source list and compile definitions retain only the reviewed
WEI448 curve, SHAKE256 hash, and EDDSA448 signature configuration.
