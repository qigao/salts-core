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
    "forbidden high-level Salts target": re.compile(r"\bSalts::(?!Core\b|Platform\b|Concurrency\b|Coroutine\b|NativeIO\b|URI\b|CNet\b|Unicode\b|IDNA\b|CSerde\b|TinyTest\b|SCTP\b|SCTPWire\b|FS\b|Process\b|Crypto\b)[A-Za-z_0-9]+"),
    "forbidden alternate target namespace": re.compile(r"\b(?:SaltsCore|SaltsCrypto|Crypto)::[A-Za-z_0-9]+"),
    "legacy package lookup": re.compile(r"\b(?:find_package|find_dependency)\s*\(\s*Salts(?:Utils)?(?:\s|\))", re.IGNORECASE),
    "CMeta/CFlow/CSTL include": re.compile(r'^\s*#\s*include\s*[<"](?:cmeta/|cflow/|cstl(?:/|\.h))'),
    "CMeta/CFlow runtime object": re.compile(r"\b(?:CMETA_STRUCT|CMETA_IMPLEMENTS|cmeta_type_desc|cflow_executor|cflow_io_actor|cflow_waker|cflow_publisher)\b"),
}


# The test-local sole-static candidate rejects split providers at configure
# time. Two exact guard lines are non-dependency references; no other CMake
# lines or source paths are exempt.
STATIC_CRYPTO_GUARD_FILE = Path("tests/cnet-crypto-static/CMakeLists.txt")
STATIC_CRYPTO_DENIED_TARGET_GUARD = (
    "foreach(_forbidden IN ITEMS",
    "Salts::TLS Salts::TLSCipherBIO Salts::CryptoProvider",
    "Crypto::TLS Crypto::TLSCipherBIO Crypto::Provider)",
)


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
        denied_guard_indices: set[int] = set()
        if relative == STATIC_CRYPTO_GUARD_FILE:
            # Only a complete, contiguous CMake foreach(_forbidden...)
            # definition can be treated as a negative assertion; never
            # exclude target_link_libraries or unrelated lines in this file.
            for offset in range(len(lines) - 2):
                if tuple(line.strip() for line in lines[offset:offset + 3]) == (
                        STATIC_CRYPTO_DENIED_TARGET_GUARD):
                    denied_guard_indices.update((offset + 1, offset + 2))
        for index, line in enumerate(lines):
            if index in denied_guard_indices:
                continue
            lineno = index + 1
            # Salts::Foo is a C++ type namespace inside .cpp/.hpp, not
            # a CMake imported target. Target name restrictions apply ONLY
            # to CMake source; includes/runtime forbidden patterns are still
            # applied to C and C++ source without exceptions.
            is_cmake = relative.name == "CMakeLists.txt" or relative.suffix == ".cmake"
            for rule, regex in PATTERNS.items():
                if rule in ("forbidden high-level Salts target",
                            "forbidden alternate target namespace") and not is_cmake:
                    continue
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
