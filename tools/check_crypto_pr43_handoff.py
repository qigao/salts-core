#!/usr/bin/env python3
"""Immutable 18-file handoff of qigao/crypto PR #43 into salts-core.

Passing this guards source preservation only. Actual TLS/TLCP and mTLS
behavior, third-party-free engine and downstream CNet cutover are separate
mandatory acceptance gates. Source edits require a new explicit provenance
record, never quietly deleting historical test cases.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tools/crypto_pr43_retirement_manifest.json"
DONOR_HEAD = "0b4ee094ce8002e6169ce231028107eba63588c6"
DONOR_BASE = "57e03d9ebefc3b9348764edaddb834862e669a45"
ORIGINAL_COUNT = 18

# Stage-1 live build obligations are distinct from the immutable donor SHA.
# The archived original CMake must NEVER be substituted for proof that the
# active one still builds its original TLS/crypto sources and regression tests.
STAGE1_LIVE_CMAKE_REQUIRED = (
    "add_library(salts_crypto STATIC",
    "add_library(Salts::Crypto ALIAS salts_crypto)",
    "src/crypto_tls_gmssl.c",
    "src/crypto_tls_cipher_bio.c",
    "src/crypto_tls_profile.c",
    "src/crypto_tls_profile_session.c",
    "src/crypto_sha256.c",
    "src/crypto_ed448.c",
    "src/crypto_random.c",
    "src/crypto_x509_der.c",
    "src/asn1/crypto.c",
    "target_link_libraries(salts_crypto PUBLIC Threads::Threads Salts::ASN1)",
    "install(TARGETS salts_crypto EXPORT SaltsCryptoTargets",
    "add_subdirectory(tests/algorithms)",
    "add_subdirectory(tests/asn1)",
    "add_subdirectory(tests/x509)",
)

def check_live_cmake(content: str) -> list[str]:
    failures = ["Active Crypto CMake lost mandatory Stage 1 owner: " + name
                for name in STAGE1_LIVE_CMAKE_REQUIRED
                if name not in content]
    if content.count("add_library(salts_crypto STATIC") != 1:
        failures.append("Active Crypto must declare exactly one canonical STATIC target")
    # Do not stage a second handshake/record TLS implementation next to
    # the existing GmSSL engine during the Stage 1 migration.
    for marker in ("src/crypto_tls_native.c", "src/crypto_tls_openssl.c",
                   "src/crypto_tls_boringssl.c", "src/crypto_tls_mbedtls.c"):
        if marker in content:
            failures.append("Active Crypto contains second TLS engine: " + marker)
    return failures


def blob_sha(p: Path) -> str:
    raw = p.read_bytes().replace(b"\r\n", b"\n")
    header = b"blob " + str(len(raw)).encode("ascii") + b"\0"
    return hashlib.sha1(header + raw).hexdigest()

def check(root: Path, source: dict) -> list[str]:
    errors: list[str] = []
    baseline = source.get("baseline", {})
    if (baseline.get("repository") != "qigao/crypto"
            or baseline.get("head") != DONOR_HEAD
            or baseline.get("base_master") != DONOR_BASE
            or baseline.get("pull_request") != 43):
        errors.append("Crypto PR #43 historical exact source ref changed")
    files = source.get("files", [])
    if len(files) != ORIGINAL_COUNT or baseline.get("original_file_count") != 18:
        errors.append("Must migrate all 18 Crypto PR #43 files")
    if len({x.get("original") for x in files}) != len(files):
        errors.append("Crypto PR #43 source path duplicated")
    active_cmake = root / "crypto/CMakeLists.txt"
    if active_cmake.is_file():
        errors.extend(check_live_cmake(
            active_cmake.read_text(encoding="utf-8")))
    else:
        errors.append("Missing active Crypto CMake owner")
    adapted = 0
    for entry in files:
        relative = entry.get("destination", "")
        old = entry.get("original", "")
        if (not relative or not old
                or ".." in Path(relative).parts or Path(relative).is_absolute()):
            errors.append("invalid source or destination entry: " + old)
            continue
        if old == "crypto/CMakeLists.txt":
            # The original byte-exact donor remains a REQUIRED immutable
            # archive; the active, module-owned CMake can now evolve without
            # rewriting historical PR #43 evidence or waiving its hash.
            if (relative != "crypto/provenance/pr43/CMakeLists.txt"
                    or entry.get("source_blob_sha") !=
                       "230dacdd80e5cb5f336a2ab0627728ba31135864"
                    or entry.get("destination_blob_sha") !=
                       "230dacdd80e5cb5f336a2ab0627728ba31135864"
                    or entry.get("kind") != "byte_exact"):
                errors.append("Historical Crypto PR43 CMake provenance cannot be rewritten")
            if not (root / "crypto/CMakeLists.txt").is_file():
                errors.append("Active module-owned Crypto/CMakeLists.txt missing")
        file = root / relative
        if not file.is_file():
            errors.append("Missing migrated original file: " + relative)
            continue
        expected = entry.get("destination_blob_sha", "")
        if len(expected) != 40 or blob_sha(file) != expected:
            errors.append("Changed/lost migrated source: " + relative)
        if entry.get("kind") == "byte_exact":
            if entry.get("source_blob_sha") != expected:
                errors.append("Original source was not copied byte-for-byte: " + old)
        elif entry.get("kind") == "historical_workflow_byte_exact":
            if entry.get("source_blob_sha") != expected:
                errors.append("Historical mTLS workflow source was modified")
        elif entry.get("kind") == "module_root_adapted":
            adapted += 1
            if old != "tools/check_module_ownership.py":
                errors.append("Unexpected adapted source path: " + old)
        else:
            errors.append("Undocumented migration of source file: " + old)
    if adapted != 1:
        errors.append("Exactly one qigao/crypto root module-checker adaptation is permitted")
    return errors

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    file = args.root / "tools/crypto_pr43_retirement_manifest.json"
    data = json.loads(file.read_text(encoding="utf-8"))
    errors = check(args.root, data)
    for error in errors:
        print("FAIL:", error)
    if errors:
        return 1
    print("PASS: 18/18 original Crypto PR #43 inputs preserved: 17 byte exact, "
          "one root module checker adapted")
    print("NOT A TLS ACCEPTANCE: PR #43 backend still includes third-party GmSSL")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
