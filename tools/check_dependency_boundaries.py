#!/usr/bin/env python3
"""Reject high-level dependencies and enforce canonical Salts:: targets in SaltsCore.

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
    "forbidden high-level Salts target": re.compile(r"\bSalts::(?!Core\b|Crypto\b|Platform\b|Concurrency\b|Coroutine\b|NativeIO\b|URI\b|CNet\b|Unicode\b|IDNA\b|CSerde\b|TinyTest\b|SCTP\b|SCTPWire\b|FS\b|Process\b)[A-Za-z_0-9]+"),
    "forbidden alternate target namespace": re.compile(r"\b(?:SaltsCore|SaltsCrypto|Crypto)::[A-Za-z_0-9]+"),
    "legacy package lookup": re.compile(r"\b(?:find_package|find_dependency)\s*\(\s*Salts(?:Utils)?(?:\s|\))", re.IGNORECASE),
    "CMeta/CFlow/CSTL include": re.compile(r'^\s*#\s*include\s*[<"](?:cmeta/|cflow/|cstl(?:/|\.h))'),
    "CMeta/CFlow runtime object": re.compile(r"\b(?:CMETA_STRUCT|CMETA_IMPLEMENTS|cmeta_type_desc|cflow_executor|cflow_io_actor|cflow_waker|cflow_publisher)\b"),
}



def verified_rejection_guard_lines(lines: list[str]) -> set[int]:
    """Skip only proven fail-closed CMake checks, never actual dependency use.

    Forbidden targets in an explicit TARGET guard followed by FATAL_ERROR
    are negative assertions, not dependencies. No entire conditional branch,
    file or directory is ignored. Actual links in the guard remain scanned.
    """
    safe: set[int] = set()
    for pos, raw in enumerate(lines):
        stripped = raw.strip()
        if stripped.startswith("foreach(_forbidden IN ITEMS"):
            end = pos
            while end < len(lines) and ")" not in lines[end]:
                end += 1
            if end >= len(lines):
                continue
            following = "\n".join(lines[end + 1:end + 5])
            if (re.search(r"if\s*\(\s*TARGET\s+\$\{_forbidden\}\s*\)",
                          following) and
                    re.search(r"message\s*\(\s*FATAL_ERROR\b", following)):
                safe.update(range(pos, end + 1))
        if re.match(r'if\s*\(\s*_crypto_links\s+MATCHES\s+"', stripped):
            following = "\n".join(lines[pos + 1:pos + 4])
            if re.search(r"message\s*\(\s*FATAL_ERROR\b", following):
                safe.add(pos)
    return safe


def comment_only(line: str, is_cmake: bool, in_block: bool) -> tuple[bool, bool]:
    """Distinguish comments from real source and preprocessor directives.

    Never exempt C #include/#define, a dereferenced expression, or actual
    CMake commands. Handle multi-line C comments without skipping code after
    an inline closing delimiter. A mixed comment/code line remains scanned.
    """
    stripped = line.lstrip()
    if in_block:
        if "*/" not in stripped:
            return True, True
        tail = stripped.split("*/", 1)[1]
        return not bool(tail.strip()), False
    if is_cmake and stripped.startswith("#"):
        return True, False
    if stripped.startswith("//"):
        return True, False
    if stripped.startswith("/*"):
        if "*/" in stripped:
            tail = stripped.split("*/", 1)[1]
            return not bool(tail.strip()), False
        return True, True
    return False, False


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
        lines = path.read_text(encoding="utf-8").splitlines()
        is_cmake = path.suffix == ".cmake" or path.name == "CMakeLists.txt"
        guarded = verified_rejection_guard_lines(lines) if is_cmake else set()
        in_block = False
        for lineno, line in enumerate(lines, 1):
            # Real dependencies are forbidden even in inactive code paths.
            # Comments and proven fatal-error guard conditions are not links.
            is_comment, in_block = comment_only(line, is_cmake, in_block)
            if (lineno - 1) in guarded or is_comment:
                continue
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
