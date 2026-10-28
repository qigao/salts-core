#!/usr/bin/env python3
"""Stage 1: fail-closed source-ownership gate for production CNet TLS.

This permits exactly ONE PRIVATE GmSSL TLS engine inside Salts::Crypto until
the native Stage 2 replacement. It NEVER permits direct CNet GmSSL,
two engines, test-only substitutions, dynamic provider selection or fallback.
Stage 1 source evidence does not replace ABI, runtime and link-image tests.
The distinct Stage 2 strict no-third-party gate is NOT weakened here.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HEADERS = ("tls.h", "tls_ext.h", "tls_peer.h", "tls_auth.h", "cipher_bio.h")
BACKENDS = ("src/crypto_tls_gmssl.c", "src/crypto_tls_native.c")
THIRD_PARTY = r"(?:gmssl|openssl|boringssl|mbedtls|wolfssl)"
INCLUDE = re.compile(r'#\s*include\s*[<"]\s*' + THIRD_PARTY + r'/', re.I)
ALTERNATE = re.compile(r"\b(?:Salts::TLS(?:Host|CipherBIO)?|Salts::CryptoProvider|Crypto::(?:TLS|TLSHost|TLSCipherBIO|Provider))\b")
OTHER_ENGINE = re.compile(r"\b(?:openssl|boringssl|mbedtls|wolfssl)(?:::|/)", re.I)


def cmake_code(source: str) -> str:
    # A positive assertion cannot be satisfied by a commented-out link.
    return "\n".join(line.split("#", 1)[0] for line in source.splitlines())


def source_errors(root: Path) -> list[str]:
    failures: list[str] = []
    required = ("CMakeLists.txt", "asn1/CMakeLists.txt", "crypto/CMakeLists.txt",
                "cnet/CMakeLists.txt", "cnet/src/cnet_tls.c")
    for name in required:
        if not (root / name).is_file():
            failures.append("MISSING " + name)
    if failures:
        return failures

    root_cmake = cmake_code((root / "CMakeLists.txt").read_text(encoding="utf-8"))
    asn1 = cmake_code((root / "asn1/CMakeLists.txt").read_text(encoding="utf-8"))
    crypto = cmake_code((root / "crypto/CMakeLists.txt").read_text(encoding="utf-8"))
    cnet = cmake_code((root / "cnet/CMakeLists.txt").read_text(encoding="utf-8"))
    tls = (root / "cnet/src/cnet_tls.c").read_text(encoding="utf-8")

    if "add_subdirectory(asn1)" not in root_cmake or not re.search(
            r"add_library\(\s*Salts::ASN1\s+ALIAS\s+salts_asn1\s*\)", asn1):
        failures.append("MISSING_SINGLE_LOCAL_ASN1")
    if "add_subdirectory(crypto)" not in root_cmake:
        failures.append("MISSING_LOCAL_CRYPTO_ASSEMBLY")
    if "SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST" in root_cmake:
        failures.append("ROOT_STILL_TEST_ONLY_DUAL_OWNER")
    if not re.search(r"add_library\(\s*salts_crypto\s+STATIC\b", crypto) or not re.search(
            r"add_library\(\s*Salts::Crypto\s+ALIAS\s+salts_crypto\s*\)", crypto):
        failures.append("CRYPTO_NOT_ONE_LOCAL_STATIC_OWNER")

    libs = re.findall(r"target_link_libraries\(\s*saltscore_cnet\b([^)]*)\)", cnet,
                      flags=re.I | re.S)
    if not libs or not any(re.search(r"\bSalts::Crypto\b", part) for part in libs):
        failures.append("PRODUCTION_CNET_NOT_LINKING_CRYPTO")
    if re.search(r"find_package\(\s*Crypto\b", cnet, re.I):
        failures.append("PRODUCTION_CNET_EXTERNAL_CRYPTO_PACKAGE")
    if re.search(r"find_package\(\s*" + THIRD_PARTY + r"\b", cnet, re.I) or re.search(
            r"\b(?:GmSSL::GmSSL|OpenSSL::SSL|OpenSSL::Crypto|mbedTLS::|wolfSSL::)\b", cnet):
        failures.append("PRODUCTION_CNET_DIRECT_THIRD_PARTY_TLS")
    if ALTERNATE.search(cnet):
        failures.append("PRODUCTION_CNET_SPLIT_TLS_TARGET")

    # Require the actual CNet production source, not a test-only qualification.
    if not re.search(r"\bcnet/src/cnet_tls\.c\b", cnet):
        failures.append("PRODUCTION_CNET_MISSING_TLS_SOURCE")
    if not ("#include <crypto/tls.h>" in tls and "#include <crypto/cipher_bio.h>" in tls):
        failures.append("PRODUCTION_CNET_NOT_USING_OPAQUE_CRYPTO")
    if re.search(r"\bTLS_(?:CTX|CONNECT)\b", tls):
        failures.append("PRODUCTION_CNET_OWNS_FOREIGN_TLS_STATE")

    # One and only one actual TLS engine is compiled into Crypto. Stage 1
    # expressly allows the ORIGINAL GmSSL one, not GmSSL plus a native engine.
    compiled = [name for name in BACKENDS if name in crypto]
    if len(compiled) != 1:
        failures.append("CRYPTO_NOT_EXACTLY_ONE_TLS_ENGINE")
    # Treat any extra compiled crypto_tls_* implementation as unaudited:
    # merely giving a second engine another filename must not bypass the gate.
    tls_units = set(re.findall(r"\\bsrc/crypto_tls_([a-z0-9_]+)\\.c\\b", crypto))
    if tls_units - {"gmssl", "native", "cipher_bio"}:
        failures.append("CRYPTO_UNAUDITED_TLS_ENGINE_SOURCE")

    if OTHER_ENGINE.search(crypto):
        failures.append("CRYPTO_ALTERNATE_THIRD_PARTY_TLS_ENGINE")
    if ALTERNATE.search(crypto) or re.search(
            r"add_library\(\s*salts_crypto\s+SHARED\b", crypto):
        failures.append("CRYPTO_SPLIT_OR_DYNAMIC_TLS_OWNER")
    for name in HEADERS:
        if not (root / "crypto/include/crypto" / name).is_file():
            failures.append("CRYPTO_MISSING_PUBLIC_API " + name)

    # Source-level policy rejects disabled includes too, so no #ifdef fallback
    # can be reactivated without an admission failure.
    for path in sorted((root / "cnet/src").rglob("*")):
        if path.is_file() and path.suffix in (".c", ".h", ".cpp"):
            content = path.read_text(encoding="utf-8")
            if INCLUDE.search(content):
                failures.append("CNET_FOREIGN_TLS_HEADER " + str(path.relative_to(root)))
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", action="store_true",
                        help="inventory only; 0 never means acceptance")
    args = parser.parse_args()
    failures = source_errors(ROOT)
    if failures:
        print(f"STAGE 1 BLOCKED: {len(failures)} source ownership errors")
        for failure in failures:
            print("  " + failure)
        if args.report:
            print("INVENTORY ONLY — neither Stage 1 nor Stage 2 is accepted")
            return 0
        return 1
    print("STAGE 1 SOURCE SHAPE PASS — one production CNet -> one local Salts::Crypto")
    print("NOT RELEASE ACCEPTANCE: runtime, API parity, ABI, link-image and CI required")
    print("STAGE 2 THIRD-PARTY-FREE GATE REMAINS SEPARATE AND MANDATORY")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
