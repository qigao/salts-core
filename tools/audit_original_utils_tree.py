#!/usr/bin/env python3
"""Full, immutable 175-file baseline inventory for the eight original utils directories.

Baseline ownership and source blobs are from qigao/salts, branch
codex/finish-1181-session, Git tree 9c5b53d7194d142a89be576c3314ee4bfd569a37.
Passing inventory admission DOES NOT prove feature parity: --full must fail
until every file has independently verified ownership, implementation,
tests and exact-head cross-platform evidence.
"""
from __future__ import annotations
import argparse
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = Path("tools/original_utils_full_inventory.json")
SOURCE_TREE = "9c5b53d7194d142a89be576c3314ee4bfd569a37"
DIRECTORIES = {
    ".": 2,
    "benchmarks": 11, "examples": 5, "fuzz": 7,
    "include": 35, "parser": 5, "src": 31,
    "tests": 68, "verification": 11,
}
UNVERIFIED = {"unmapped", "name_match_unverified", "byte_identical_unverified"}
CHECKSUM = "22b99d8abed1cac2"


def source_digest(records: list[dict]) -> str:
    """FNV-1a 64-bit over sorted canonical original path<TAB>Git blob SHA pairs."""
    data = "".join(
        record["original"] + "\t" + record["source_blob_sha"] + "\n"
        for record in sorted(records, key=lambda r: r["original"])
    ).encode("utf-8")
    result = 0xcbf29ce484222325
    for value in data:
        result = ((result ^ value) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{result:016x}"


def check_inventory(root: Path, data: dict, *, full: bool = False) -> list[str]:
    errors = []
    original = data.get("historical_baseline", {})
    if original.get("utils_tree_sha") != SOURCE_TREE:
        errors.append("immutable original utils Git tree changed")
    if original.get("file_count") != 175:
        errors.append("original baseline total must be exactly 175 files")
    if set(original.get("directories", [])) != set(DIRECTORIES) - {"."}:
        errors.append("all eight original top-level directories must be recorded")
    rows = data.get("files", [])
    if len(rows) != 175 or source_digest(rows) != CHECKSUM:
        errors.append("175 original source paths/blob SHAs have been omitted or altered")
    names = [x.get("original") for x in rows]
    if len(set(names)) != len(names):
        errors.append("duplicate original file paths")
    observed = Counter()
    for row in rows:
        name = row.get("original", "")
        group = name.split("/", 1)[0] if "/" in name else "."
        observed[group] += 1
        if group not in DIRECTORIES:
            errors.append(f"unrecognized source directory: {name}")
        status = row.get("status")
        if status not in UNVERIFIED | {"verified"}:
            errors.append(f"unknown parity status for {name}: {status}")
        if status == "verified":
            evidence = row.get("acceptance_evidence", {})
            if not (evidence.get("destination")
                    and evidence.get("tests")
                    and evidence.get("ci_head_sha")):
                errors.append(f"cannot mark original file verified without destination, "
                              f"tests and exact CI head: {name}")
        for candidate in row.get("destination_candidates", []):
            path = candidate.get("path", "")
            if (not path or Path(path).is_absolute()
                    or ".." in Path(path).parts):
                errors.append(f"invalid candidate path for {name}: {path}")
            elif not (root / path).is_file():
                errors.append(f"missing recorded original inventory candidate: {path}")
            if not candidate.get("blob_sha"):
                errors.append(f"missing candidate SHA for {name}: {path}")
    if dict(observed) != DIRECTORIES:
        errors.append(f"historical directory file counts changed: {dict(observed)}")
    if full:
        unresolved = [r["original"] for r in rows if r.get("status") != "verified"]
        if unresolved:
            errors.append(f"FULL MIGRATION NOT ACCEPTED: {len(unresolved)}/175 files "
                          "lack verified destination, tests and exact-head CI")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--full", action="store_true")
    args = parser.parse_args()
    data = json.loads((args.root / INVENTORY).read_text(encoding="utf-8"))
    errors = check_inventory(args.root, data, full=args.full)
    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1
    status = Counter(r["status"] for r in data["files"])
    print(f"PASS: all 175 original utils files in 8 directories recorded, source tree {SOURCE_TREE}")
    print("NOT A PARITY PASS: " + ", ".join(
        f"{key}={value}" for key, value in sorted(status.items())))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
