#!/usr/bin/env python3
"""Fail-closed two-module SDK archive audit; one static ASN1 and one static Crypto."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import subprocess
import sys

CRYPTO_SYMBOLS = (
    "sm3_init", "sm3_update", "sm3_finish",
    "crypto_tls_session_handshake",
    "crypto_tls_auth_abi_version",
    "crypto_tls_session_create_auth_v1_3",
    "crypto_tls_session_client_authenticated_v1_3",
    "crypto_tls_session_export_v1_2",
    "crypto_tls_cipher_bio_create",
    "salts_crypto_sha256", "salts_crypto_ed448_sign",
    "salts_crypto_ed448_verify", "salts_crypto_random_bytes",
    "salts_crypto_libecc_get_random",
    "sha256_init", "sha256_update", "sha256_finish",
    "crypto_asn1_ecdsa_signature_encode",
    "crypto_asn1_ecdsa_signature_decode",
    "crypto_asn1_rfc8410_spki_encode",
    "crypto_asn1_rfc8410_pkcs8_encode",
    "crypto_x509_der_inspect", "crypto_x509_der_chain_next",
    "crypto_x509_der_common_name", "crypto_x509_der_get_public_key_kind",
)
ASN1_SYMBOLS = (
    "salts_asn1_reader_init", "salts_asn1_reader_next",
    "salts_asn1_validate", "salts_asn1_encode_tlv",
    "salts_asn1_value_create", "salts_asn1_value_encode",
    "salts_asn1_value_decode", "salts_asn1_value_free",
    "salts_asn1_value_get_oid",
)
OLD_SYMBOLS = (
    "get_unsafe_random",
    "salts_crypto_libecc_salts_crypto_libecc_get_random",
    "crypto_provider_sm3", "crypto_provider_runtime_abi",
    "crypto_provider_abi_0_1_required",
)

def _defined_symbols(archive: Path) -> Counter[str]:
    command = ["nm", "-gU" if sys.platform == "darwin" else "-g", str(archive)]
    p = subprocess.run(command, capture_output=True, text=True, check=False)
    if p.returncode != 0:
        raise ValueError("native archive symbol reader failed: " + p.stderr[-3000:])
    result: Counter[str] = Counter()
    for line in p.stdout.splitlines():
        # nm prefixes archive object boundaries; only count defined exports.
        m = re.match(r"^\s*(?:[0-9a-fA-F]+\s+)?([A-TV-Z])\s+(\S+)\s*$", line)
        if not m or m.group(1) not in ("T", "D", "B", "R", "S", "C"):
            continue
        symbol = m.group(2)
        if sys.platform == "darwin":
            symbol = symbol.removeprefix("_")
        result[symbol] += 1
    return result

def _check_archive_inventory(prefix: Path, allowed: list[Path]) -> None:
    archives = sorted(
        path for path in prefix.rglob("*")
        if path.is_file() and path.suffix.lower() in
        (".a", ".lib", ".dll", ".dylib", ".so")
    )
    if archives != sorted(allowed):
        raise ValueError(
            "unexpected native binary in installation: " +
            ", ".join(str(path.relative_to(prefix)) for path in archives)
        )
    for path in prefix.rglob("*"):
        if path.is_file() and re.search(r"\.so\.[0-9]+", path.name):
            raise ValueError("versioned shared library forbidden: " + str(path))

def inspect(prefix: Path, asn1_prefix: Path | None = None) -> dict:
    prefix = prefix.resolve(strict=True)
    asn1_prefix = (asn1_prefix or prefix).resolve(strict=True)
    ext = ".lib" if sys.platform == "win32" else ".a"
    crypto_name = "salts_crypto.lib" if ext == ".lib" else "libsalts_crypto.a"
    asn1_name = "salts_asn1.lib" if ext == ".lib" else "libsalts_asn1.a"
    crypto_archive = prefix / "lib" / crypto_name
    asn1_archive = asn1_prefix / "lib" / asn1_name
    if not crypto_archive.is_file() or not asn1_archive.is_file():
        raise ValueError("missing one of the required independent native static archives")
    _check_archive_inventory(prefix, [
        crypto_archive, *([asn1_archive] if asn1_prefix == prefix else [])
    ])
    if asn1_prefix != prefix:
        _check_archive_inventory(asn1_prefix, [asn1_archive])
    crypto_inc = prefix / "include" / "crypto"
    asn1_inc = asn1_prefix / "include" / "salts"
    for name in ("status.h", "sha256.h", "ed448.h", "asn1_crypto.h",
                 "tls_auth.h"):
        if not (crypto_inc / name).is_file():
            raise ValueError("missing canonical Crypto header: " + name)
    for name in ("asn1.h", "asn1_value.h"):
        if not (asn1_inc / name).is_file():
            raise ValueError("missing canonical ASN1 header: " + name)
        if (prefix / "include" / "crypto" / name).exists():
            raise ValueError("duplicated old Crypto-owned ASN1 header: " + name)
    if (prefix / "include" / "salts" / "crypto.h").exists():
        raise ValueError("legacy salts/crypto.h found")
    if (prefix / "include" / "crypto" / "provider.h").exists():
        raise ValueError("provider compatibility header found")
    if not (prefix / "include" / "gmssl" / "sm3.h").is_file():
        raise ValueError("canonical installed GmSSL sm3.h missing")

    targets = prefix / "lib" / "cmake" / "Crypto" / "CryptoTargets.cmake"
    asn1targets = asn1_prefix / "lib" / "cmake" / "ASN1" / "ASN1Targets.cmake"
    cfg = prefix / "lib" / "cmake" / "Crypto" / "CryptoConfig.cmake"
    if not targets.is_file() or not asn1targets.is_file() or not cfg.is_file():
        raise ValueError("missing exported native module package")
    crypto_value = targets.read_text(encoding="utf-8")
    asn1_value = asn1targets.read_text(encoding="utf-8")
    if "add_library(Salts::Crypto STATIC IMPORTED)" not in crypto_value:
        raise ValueError("Crypto must export one imported static target")
    if "add_library(Salts::ASN1 STATIC IMPORTED)" not in asn1_value:
        raise ValueError("ASN1 must export one imported static target")
    if "Salts::ASN1" not in crypto_value or "find_dependency(ASN1" not in cfg.read_text(encoding="utf-8"):
        raise ValueError("Crypto must explicitly depend on exactly one ASN1 package")
    if "GmSSL::" in crypto_value or "PrivateGmSSL" in crypto_value:
        raise ValueError("Crypto must not export a GmSSL/Provider target")
    report = {
        "ok": True,
        "crypto_archive": str(crypto_archive),
        "asn1_archive": str(asn1_archive),
        "native_modules": ["Salts::ASN1", "Salts::Crypto"],
        "symbols_checked": False,
    }
    if sys.platform == "win32":
        # COFF uniqueness still requires its own exact-tool qualification
        # (tracked independently); C11/C++17 consumers prove real linkage.
        return report

    crypto_symbols = _defined_symbols(crypto_archive)
    asn1_symbols = _defined_symbols(asn1_archive)
    for label, table, required in (
        ("Crypto", crypto_symbols, CRYPTO_SYMBOLS),
        ("ASN1", asn1_symbols, ASN1_SYMBOLS),
    ):
        invalid = {s: table[s] for s in required if table[s] != 1}
        if invalid:
            raise ValueError(label + " missing or nonunique exported symbols: " + str(invalid))
    duplicates = [s for s in ASN1_SYMBOLS if crypto_symbols[s]]
    duplicates += [s for s in CRYPTO_SYMBOLS if asn1_symbols[s]]
    if duplicates:
        raise ValueError("cross-module duplicate native definitions: " + str(duplicates))
    forbidden = [s for s in crypto_symbols if
                 s in OLD_SYMBOLS or
                 s.startswith(("salts_asn1_", "crypto_asn1_reader_", "crypto_asn1_value_")) or
                 s in ("crypto_asn1_validate", "crypto_asn1_encode_tlv")]
    if forbidden:
        raise ValueError("Crypto contains an obsolete or duplicate native owner: " + str(forbidden))
    report["symbols_checked"] = True
    report["crypto_symbol_count"] = len(crypto_symbols)
    report["asn1_symbol_count"] = len(asn1_symbols)
    return report

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--asn1-prefix", type=Path,
                        help="use only if ASN1 is installed at a separate prefix")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = inspect(args.prefix, args.asn1_prefix)
    except (OSError, ValueError) as exc:
        result = {"ok": False, "reason": str(exc)}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("Two static module ownership: " + ("PASS" if result["ok"] else "FAIL " + result["reason"]))
    return 0 if result["ok"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
