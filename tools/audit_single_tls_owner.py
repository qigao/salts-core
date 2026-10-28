#!/usr/bin/env python3
"""Fail-closed single-TLS-owner SOURCE gate (not a feature-parity certification).

--strict is a release gate and currently returns RED on the historical tree.
--report describes blockers without claiming the TLS cutover is accepted.
The real TLS/alert/mTLS/NativeIO/profile matrix is separately required.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
TLS_HEADERS = (
    "crypto/include/crypto/tls.h",
    "crypto/include/crypto/tls_ext.h",
    "crypto/include/crypto/tls_peer.h",
    "crypto/include/crypto/tls_auth.h",
    "crypto/include/crypto/cipher_bio.h",
)
THIRD_PARTY_TLS = r"(?:gmssl|openssl|boringssl|mbedtls|wolfssl)"
SPLIT_TARGET = re.compile(
    r"\b(?:Salts::TLS(?:Host|CipherBIO)?|Salts::CryptoProvider|"
    r"Crypto::(?:TLS|TLSHost|TLSCipherBIO|Provider))\b")
PRIVATE_TLS_INCLUDE = re.compile(
    r"#\s*include\s*[<\"][ \t]*" + THIRD_PARTY_TLS + r"/", re.IGNORECASE)
DYNAMIC_PROVIDER = re.compile(
    r"(?im)^\s*add_library\(\s*(?:Salts::TLS|Crypto::TLS|Salts::CryptoProvider)\b")

def source_errors(root: Path) -> list[str]:
    failures: list[str] = []
    paths = (
        "CMakeLists.txt", "cnet/CMakeLists.txt", "cnet/src/cnet_tls.c",
        "asn1/CMakeLists.txt", "crypto/CMakeLists.txt",
    )
    for p in paths:
        if not (root / p).is_file():
            failures.append("MISSING_SOURCE " + p)
    if failures:
        return failures
    assembly = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    cmake = (root / "cnet/CMakeLists.txt").read_text(encoding="utf-8")
    tls = (root / "cnet/src/cnet_tls.c").read_text(encoding="utf-8")
    asn1 = (root / "asn1/CMakeLists.txt").read_text(encoding="utf-8")
    crypto = (root / "crypto/CMakeLists.txt").read_text(encoding="utf-8")
    if "add_subdirectory(asn1)" not in assembly:
        failures.append("ROOT_NOT_BUILDING_NATIVE_ASN1")
    if "add_subdirectory(crypto)" not in assembly:
        failures.append("ROOT_NOT_BUILDING_NATIVE_CRYPTO")
    if "SALTSCORE_BUILD_CNET AND SALTSCORE_BUILD_CRYPTO" in assembly:
        failures.append("ROOT_LEGACY_DUAL_TLS_BLOCKER_STILL_PRESENT")
    if not re.search(r"add_library\(\s*Salts::ASN1\s+ALIAS\s+salts_asn1\)", asn1):
        failures.append("ASN1_NOT_CANONICAL_FIRST_PARTY_OWNER")

    # A sole Crypto target is the ONLY allowed CNet TLS API and link.
    if re.search(r"find_package\(\s*Crypto\b", cmake):
        failures.append("CNET_USES_EXTERNAL_CRYPTO_PACKAGE")
    if "Salts::Crypto" not in cmake:
        failures.append("CNET_NOT_LINKING_SALTS_CRYPTO")
    if re.search(r"find_package\(\s*" + THIRD_PARTY_TLS + r"\b", cmake,
                 flags=re.IGNORECASE) or re.search(
                 r"(?:GmSSL::GmSSL|OpenSSL::SSL|mbedTLS::|wolfSSL::)", cmake):
        failures.append("CNET_DIRECT_THIRD_PARTY_TLS_LINK")
    if SPLIT_TARGET.search(cmake) or DYNAMIC_PROVIDER.search(cmake):
        failures.append("CNET_SPLIT_TLS_PROVIDER")
    if PRIVATE_TLS_INCLUDE.search(tls) or re.search(r"\bTLS_(?:CTX|CONNECT)\b", tls):
        failures.append("CNET_DIRECT_BACKEND_TLS_STATE")
    if not ("#include <crypto/tls.h>" in tls and
            "#include <crypto/cipher_bio.h>" in tls):
        failures.append("CNET_NOT_USING_OPAQUE_TLS_AND_BIO")

    # qigao/crypto, NOT CNet, is the only TLS implementation owner.
    if not re.search(r"add_library\(\s*salts_crypto\s+STATIC\b", crypto):
        failures.append("CRYPTO_NOT_SINGLE_STATIC_ARCHIVE")
    if not re.search(r"add_library\(\s*Salts::Crypto\s+ALIAS\s+salts_crypto\)",crypto):
        failures.append("CRYPTO_NOT_CANONICAL_OWNER")
    if re.search(r"find_package\(\s*" + THIRD_PARTY_TLS + r"\b", crypto,
                 flags=re.IGNORECASE) or re.search(r"\bGmSSL::GmSSL\b",crypto):
        failures.append("CRYPTO_THIRD_PARTY_TLS_PACKAGE")
    if "src/crypto_tls_gmssl.c" in crypto:
        failures.append("CRYPTO_GMSSL_BACKEND_STILL_COMPILED")
    if SPLIT_TARGET.search(crypto) or DYNAMIC_PROVIDER.search(crypto):
        failures.append("CRYPTO_SPLIT_TLS_PROVIDER")
    for p in TLS_HEADERS:
        if not (root / p).is_file():
            failures.append("CRYPTO_MISSING_VERSIONED_TLS_API " + p)

    # This scans *all* CNet source, including disabled #ifdef branches.
    cnet_dir = root / "cnet" / "src"
    for p in sorted(cnet_dir.rglob("*")):
        if p.is_file() and p.suffix in (".c", ".h", ".cpp"):
            content = p.read_text(encoding="utf-8")
            if PRIVATE_TLS_INCLUDE.search(content):
                failures.append("CNET_PRIVATE_THIRD_PARTY_HEADER " + str(
                    p.relative_to(root)))
    return failures

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", action="store_true")
    args = parser.parse_args()
    failures = source_errors(ROOT)
    if failures:
        print("CUTOVER BLOCKED: " + str(len(failures)) +
              " single-owner source gate(s) outstanding")
        for failure in failures:
            print("  " + failure)
        if args.report:
            print("NOT ACCEPTED: report mode is inventory only, not a green cutover")
            return 0
        return 1
    print("SOURCE OWNER ADMISSION: sole opaque Salts::Crypto and no third-party TLS")
    print("NOT A FEATURE ACCEPTANCE: runtime/profile/security CI still required")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
