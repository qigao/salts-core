#!/usr/bin/env python3
"""Fail-closed original Salts utils *stage A* source/API inventory gate.

Passing this never means tlog/fmt or the full original utils migration is complete.
The complete gate (--full) intentionally remains red until all pending groups
are migrated and proven on their exact module owners.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tools/original_utils_parity.json"


def check_stage_a(root: Path, manifest: dict) -> list[str]:
    errors = []
    core = (root / "core/CMakeLists.txt")
    core_cmake = core.read_text(encoding="utf-8") if core.is_file() else ""
    for entry in manifest["stage_a"]:
        owner = entry.get("owner", "core")
        owner_path = root / owner / "CMakeLists.txt"
        cmake = owner_path.read_text(encoding="utf-8") if owner_path.is_file() else ""
        header = root / entry["header"]
        source = root / entry["source"]
        if not header.is_file() or not source.is_file():
            errors.append(f"Missing {owner} source/header owner for {entry['module']}")
            continue
        h = header.read_text(encoding="utf-8")
        s = source.read_text(encoding="utf-8")
        for fn in entry["functions"]:
            # The public declaration must stay linkable: a vanished original
            # symbol is never accepted just because source includes compile.
            declared = re.search(r"\b" + re.escape(fn) + r"\s*\(", h)
            implemented = re.search(r"(?m)^\s*(?:static\s+)?[\w\s*]+\b"
                                    + re.escape(fn) + r"\s*\(", s)
            if not declared or not implemented:
                errors.append(f"{entry['module']}: missing public/source function {fn}")
        if entry["source"].split("/")[-1] not in cmake:
            errors.append(f"{entry['module']}: source not compiled by its sole {owner} module target")
        if "<cmeta/" in h or "CMETA_STRUCT(" in h:
            errors.append(f"{entry['module']}: forbidden CMeta reflection in public header")
    for file in manifest["stage_a_support_files"]:
        if not (root / file).is_file():
            errors.append(f"Missing original string/allocator support file: {file}")
    if "src/sds/sds.c" not in core_cmake or "include/tstr.h" not in core_cmake:
        errors.append("SDS or tstr not owned/installed by the canonical Core target")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--full", action="store_true")
    args = parser.parse_args()
    manifest = json.loads((args.root / "tools/original_utils_parity.json").read_text())
    errors = check_stage_a(args.root, manifest)
    pending = sum(len(group) for group in manifest["pending_groups"].values())
    if args.full and pending:
        errors.append(f"FULL RESTORATION NOT ACCEPTED: {pending} pending module/function-owner investigations")
    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1
    count = sum(len(m["functions"]) for m in manifest["stage_a"])
    print(f"PASS: restored {count} original public functions in canonical module owners")
    print(f"INCOMPLETE: {pending} remaining restoration/semantic-parity investigations (Issue #71)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
