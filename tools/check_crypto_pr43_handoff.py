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
    adapted = 0
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
