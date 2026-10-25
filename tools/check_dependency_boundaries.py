#!/usr/bin/env python3
"""Reject dependencies from SaltsCore back to legacy Salts/CMeta/CFlow/CSTL.

A historical `cmeta_` API spelling is *not* a CMeta runtime dependency; only
specific target, include and runtime reflection/actor types are forbidden.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re

SCANNED_EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp", ".m", ".mm", ".cmake"}
IGNORED_DIRECTORIES = {".git", ".github", "vendor", "third_party", "build", "out", "tools", "docs", "_build", "_installed", "_sdk", "CMakeFiles"}
PATTERNS = {
    "legacy CMake target": re.compile(r"\bSalts::[A-Za-z_0-9]+"),
    "legacy package lookup": re.compile(r"\b(?:find_package|find_dependency)\s*\(\s*Salts(?:Utils)?(?:\s|\))", re.IGNORECASE),
    "CMeta/CFlow/CSTL include": re.compile(r'^\s*#\s*include\s*[<"](?:cmeta/|cflow/|cstl(?:/|\.h))'),
    "CMeta/CFlow runtime object": re.compile(r"\b(?:CMETA_STRUCT|CMETA_IMPLEMENTS|cmeta_type_desc|cflow_executor|cflow_io_actor|cflow_waker|cflow_publisher)\b"),
}


def check_repository(root: Path) -> list[str]:
    violations: list[str] = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if any(part in IGNORED_DIRECTORIES or part.startswith(".build") for part in relative.parts[:-1]):
            continue
        if path.suffix not in SCANNED_EXTENSIONS and path.name != "CMakeLists.txt":
            continue
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            # Build and source files must not refer to any legacy module even in
            # disabled branches; migration is not allowed to add hidden fallback.
            for rule, regex in PATTERNS.items():
                if regex.search(line):
                    violations.append(f"{relative}:{lineno}: {rule}: {line.strip()}")
    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    violations = check_repository(args.root)
    for violation in violations:
        print(violation)
    if violations:
        print(f"FAILED: {len(violations)} forbidden dependency reference(s)")
        return 1
    print("PASS: no legacy Salts/CMeta/CFlow/CSTL dependencies")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
