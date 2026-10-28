#!/usr/bin/env python3
"""SaltsCore-native source ownership, migrated from qigao/crypto PR #43."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "asn1/CMakeLists.txt", "asn1/src/reader.c", "asn1/src/value.c",
    "asn1/include/salts/asn1.h", "asn1/include/salts/asn1_value.h",
    "asn1/tests/CMakeLists.txt", "asn1/tests/installed/CMakeLists.txt",
    "crypto/CMakeLists.txt", "crypto/src/asn1/crypto.c",
    "crypto/include/crypto/asn1_crypto.h", "crypto/vcpkg.json",
    "crypto/cmake/CryptoConfig.cmake.in", "crypto/tools/audit_static_crypto.py",
    "crypto/include/crypto/x509_der.h", "crypto/src/crypto_x509_der.c",
    "crypto/include/crypto/tls_auth.h",
    "crypto/tests/x509/test_x509_der.c",
]
errors = []
for item in REQUIRED:
    if not (ROOT / item).is_file():
        errors.append("required sole owner missing: " + item)
# In SaltsCore, core/, vendor/, tests/, and cmake/ are legitimate
# separate module owners. The qigao/crypto-era repository-root bans cannot
# be copied verbatim; only the old duplications *inside ASN1/Crypto* are banned.
for subdir in ("asn1", "crypto"):
    if f"add_subdirectory({subdir})" not in (
            ROOT / "CMakeLists.txt").read_text(encoding="utf-8"):
        errors.append("missing local source assembly: " + subdir)
for item in ("crypto/src/asn1/reader.c", "crypto/src/asn1/value.c",
             "crypto/include/crypto/asn1.h", "crypto/include/crypto/asn1_value.h",
             "crypto/tests/asn1/reader.c", "crypto/tests/asn1/value.c"):
    if (ROOT / item).exists():
        errors.append("duplicated generic ASN.1 implementation: " + item)
a = (ROOT / "asn1/CMakeLists.txt").read_text(encoding="utf-8")
c = (ROOT / "crypto/CMakeLists.txt").read_text(encoding="utf-8")
if "add_library(salts_asn1 STATIC" not in a or "Salts::ASN1 ALIAS" not in a:
    errors.append("ASN1 has no sole static target")
if "add_library(salts_crypto STATIC" not in c or "Salts::Crypto ALIAS" not in c:
    errors.append("Crypto has no sole static target")
if "Salts::ASN1" not in c:
    errors.append("Crypto does not import canonical ASN1")
for pat in ("find_package(GmSSL", "find_package(Crypto", "Salts::Crypto",
            "Salts::Core", "target_link_libraries(salts_asn1",
            "find_package(ASN1", "src/asn1/crypto.c"):
    if pat in a:
        errors.append("provider dependency leaked into standalone ASN1: " + pat)
for f in (ROOT / "asn1/src").rglob("*.c"):
    if re.search(r"#\s*include\s*[<\"](?:gmssl|crypto|salts_core)/",
                 f.read_text(encoding="utf-8")):
        errors.append("ASN1 source depends on high-level provider: " + str(f))
for f in (ROOT / "crypto/src").rglob("*.c"):
    text = f.read_text(encoding="utf-8")
    if re.search(r"\b(?:crypto_asn1_reader_(?:init|next)|crypto_asn1_value_(?:create|free)|crypto_asn1_validate)\b", text):
        errors.append("retired generic Crypto ASN.1 ABI remains: " + str(f))
if errors:
    for error in errors:
        print("FAIL: " + error, file=sys.stderr)
    sys.exit(1)
print("PASS: sole source-owner modules Salts::ASN1 and Salts::Crypto")
