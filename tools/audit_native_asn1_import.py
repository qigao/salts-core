#!/usr/bin/env python3
"""Fail closed if a migrated ASN1 implementation/test differs from its original blob."""
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
  "asn1/include/salts/asn1.h": "8f153e38c2a2e9bc0abb8ce94951640268ae26ce",
  "asn1/include/salts/asn1_crypto.h": "cd9deb96ea1d0b3a152d767410d83e82ddc1476c",
  "asn1/include/salts/asn1_value.h": "f58958a23275bf0dec209e86cfba5e60d24cffb3",
  "asn1/src/asn1.c": "b91d65eff46500e6f7caf17cee7e3c521a55b6ed",
  "asn1/src/crypto.c": "5cfea06efd212d0c27a121092b40c4075fc1c699",
  "asn1/src/value.c": "7b5d9abcdbd200dba910270bab9e1bde5c214274",
  "asn1/tests/asn1_crypto_test.c": "c04f7ec853b022874244b4c4eeb0d62f082d97ba",
  "asn1/tests/asn1_header_cpp_test.cpp": "6b96593725a041d6b4516bc182a068f7b8051395",
  "asn1/tests/asn1_reader_test.c": "8de2931559e19f1ee329244019983d1a6ee0f1fb",
  "asn1/tests/asn1_value_test.c": "5f2abf5c3d5368b17be02c6b254a888d2f9058e4",
  "asn1/tests/installed/asn1_installed_c11.c": "ce5cebf5ce354adb85114c1498545c7809a4ee3a",
  "asn1/tests/installed/asn1_installed_cpp17.cpp": "65068b6a94b4be0f58b5675f8b1ba626f5d41c3c",
}
for path, sha in sorted(EXPECTED.items()):
    if not (ROOT / path).is_file():
        raise SystemExit(f"Missing Core-owned ASN1 file: {path}")
    found = subprocess.check_output(["git", "hash-object", "--", path],
                                    cwd=ROOT, text=True).strip()
    if found != sha:
        raise SystemExit(f"Original ASN1 blob differs: {path}")
print(f"PASS: {len(EXPECTED)} byte-identical ASN1 source/test Git blobs")
