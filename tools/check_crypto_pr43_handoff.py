#!/usr/bin/env python3
"""Immutable 18-file handoff of qigao/crypto PR #43 into salts-core.

Passing this guards source preservation only. Actual TLS/TLCP and mTLS
behavior, third-party-free engine and downstream CNet cutover are separate
mandatory acceptance gates. Source edits require a new explicit provenance
record, never quietly deleting historical test cases.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tools/crypto_pr43_retirement_manifest.json"
DONOR_HEAD = "0b4ee094ce8002e6169ce231028107eba63588c6"
DONOR_BASE = "57e03d9ebefc3b9348764edaddb834862e669a45"
ORIGINAL_COUNT = 18
BUILD_SOURCE = "crypto/CMakeLists.txt"
BUILD_SOURCE_BLOB = "230dacdd80e5cb5f336a2ab0627728ba31135864"
BUILD_SNAPSHOT = "crypto/tests/migration/CMakeLists.original.txt"
BUILD_DIFF = "crypto/tests/migration/CMakeLists.local-native.patch"
# This reviewed patch preserves the public source/test graph while replacing
# the external GmSSL archive merge with the source-locked local OBJECT owner.
# Changing only manifest destination hashes cannot approve another adaptation.
BUILD_DIFF_BLOB = "d38ec05d4db650789977e74c88cdeedecaa0aa90"

def blob_sha(p: Path) -> str:
    raw = p.read_bytes().replace(b"\r\n", b"\n")
    header = b"blob " + str(len(raw)).encode("ascii") + b"\0"
    return hashlib.sha1(header + raw).hexdigest()

def build_diff(original: Path, current: Path) -> str:
    return "".join(difflib.unified_diff(
        original.read_text(encoding="utf-8").splitlines(keepends=True),
        current.read_text(encoding="utf-8").splitlines(keepends=True),
        fromfile="a/" + BUILD_SOURCE, tofile="b/" + BUILD_SOURCE, n=3))

def check_build_adaptation(root: Path, entry: dict) -> list[str]:
    errors = []
    if (entry.get("original") != BUILD_SOURCE
            or entry.get("destination") != BUILD_SOURCE
            or entry.get("source_blob_sha") != BUILD_SOURCE_BLOB):
        return ["Local build adaptation must retain the original Crypto CMake source identity"]
    adaptation = entry.get("adaptation", {})
    expected = {
        "original_snapshot": BUILD_SNAPSHOT,
        "original_snapshot_blob_sha": BUILD_SOURCE_BLOB,
        "reviewed_diff": BUILD_DIFF,
        "reviewed_diff_blob_sha": BUILD_DIFF_BLOB,
    }
    if any(adaptation.get(key) != value for key, value in expected.items()):
        errors.append("Local build adaptation must name the fixed original snapshot and reviewed diff")
    snapshot, patch = root / BUILD_SNAPSHOT, root / BUILD_DIFF
    if not snapshot.is_file() or blob_sha(snapshot) != BUILD_SOURCE_BLOB:
        errors.append("Original Crypto CMake snapshot changed or disappeared")
    if not patch.is_file() or blob_sha(patch) != BUILD_DIFF_BLOB:
        errors.append("Reviewed Crypto CMake adaptation diff changed or disappeared")
    if not errors and build_diff(snapshot, root / BUILD_SOURCE) != patch.read_text(encoding="utf-8"):
        errors.append("Active Crypto CMake differs from its reviewed local build adaptation")
    return errors

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
    adapted = 0
    build_adapted = 0
    for entry in files:
        relative = entry.get("destination", "")
        old = entry.get("original", "")
        if (not relative or not old
                or ".." in Path(relative).parts or Path(relative).is_absolute()):
            errors.append("invalid source or destination entry: " + old)
            continue
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
        elif entry.get("kind") == "local_build_adapted":
            build_adapted += 1
            errors.extend(check_build_adaptation(root, entry))
        else:
            errors.append("Undocumented migration of source file: " + old)
    if adapted != 1:
        errors.append("Exactly one qigao/crypto root module-checker adaptation is permitted")
    if build_adapted != 1:
        errors.append("Exactly one reviewed local Crypto CMake adaptation is required")
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
    print("PASS: 18/18 original Crypto PR #43 inputs preserved: 16 byte exact, "
          "one root module checker adapted, one reviewed local build adaptation")
    print("NOT A TLS ACCEPTANCE: source preservation does not qualify production native TLS")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
