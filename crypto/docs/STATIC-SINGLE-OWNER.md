# ASN1 / Crypto static library boundaries

`Salts::ASN1` is the single first-party BER/DER implementation and can be
used independently without bringing in TLS or the Crypto facade.

`Salts::Crypto` is the single first-party C11 TLS/X.509/CipherBIO/hash facade.
It is a STATIC target composed of first-party sources and original Ed448
libecc objects. **GmSSL is a separate pinned vcpkg STATIC archive**,
imported as `GmSSL::GmSSL`. Its native C and H sources are owned by the
vcpkg port, not mirrored in this repository.

The original extension C/H/patch blobs and their cryptographic provenance
are pinned under `crypto/gmssl-extensions/source-lock.json`. The custom
registry supplies `gmssl 3.2.0#16`; its public CMake import is required
by installed `CryptoConfig.cmake` and consequently by SaltsCore's
installed package.

`Monocypher::Monocypher` and `Reed::Reed` are independent, hidden PIC
vendor libraries; Monocypher remains the existing Secure-KCP algorithm
dependency, Reed is FEC (not cryptography). No additional TLS engine or
Monocypher copy is compiled into `Salts::Crypto`.

A source-local target uniqueness check does not establish process-wide
physical uniqueness. The CNet native linker-map audit must prove the
imported GmSSL native TLS symbols originate once, the Crypto facade is
separate and defined once, and plugin/DSO peers do not import or copy
another static provider.

Use `crypto/tools/audit_static_crypto.py` with the first-party installed
prefix and an explicit `--gmssl-prefix` pointing at the imported vcpkg
target triplet. Preserve real TLS, ASN1, plugin, sanitizer and platform
qualification; an SDK configure or empty CTest result alone is insufficient.
