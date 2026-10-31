#!/usr/bin/env python3
"""Require CNet's actual CMake dependency closure to trigger 3-OS TLS CI.

Keep source/include coverage keyed by original module names. This gate runs
from the unconditional Core SDK workflow on every pull request; no PyYAML or
third-party pathspec is needed for the intentionally exact directory patterns.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CNET_CMAKE = ROOT / "cnet" / "CMakeLists.txt"
PROVIDER_WORKFLOW = ROOT / ".github" / "workflows" / "cnet-provider.yml"

# New Salts:: dependencies must be explicitly reviewed and mapped here.
TARGET_MODULE = {
    "Core": "core",
    "Platform": "platform",
    "NativeIO": "native-io",
    "Concurrency": "concurrency",
    "URI": "uri",
    "Crypto": "crypto",
}
REQUIRED_BASE = frozenset({
    "cnet/CMakeLists.txt",
    "cnet/src/**",
    "cnet/include/**",
    "uri/parser/**",  # re2c input is outside uri/src
    "vendor/cnet/**",
    # The local Crypto owner compiles its native backend outside src/include,
    # links ASN1 and retains these original hardening inputs.
    "crypto/native/**",
    "asn1/**",
    "vendor/gmssl-extensions/**",
    "vendor/CMakeLists.txt",
    "cmake/**",
    "CMakeLists.txt",
    "vcpkg.json",
    "vcpkg-configuration.json",
    ".github/workflows/cnet-provider.yml",
})


def linked_modules(cmake_text: str) -> frozenset[str]:
    match = re.search(
        r"\btarget_link_libraries\s*\(\s*saltscore_cnet\s+(.*?)\)",
        cmake_text, re.DOTALL,
    )
    if match is None:
        raise ValueError("missing saltscore_cnet link contract")
    names = set(re.findall(r"\bSalts::([A-Za-z][A-Za-z0-9_]*)\b", match.group(1)))
    if not names:
        raise ValueError("no Salts::* dependencies in saltscore_cnet link contract")
    unknown = names - TARGET_MODULE.keys()
    if unknown:
        raise ValueError(f"unmapped CNet linked owner(s): {', '.join(sorted(unknown))}")
    return frozenset(TARGET_MODULE[name] for name in names)


def pull_request_paths(yaml_text: str) -> frozenset[str]:
    """Read the strict on/pull_request/paths string-list contract in this workflow.

    This avoids YAML's 'on' scalar ambiguity and rejects unsupported negated
    globs rather than accidentally treating an excluded file as covered.
    """
    state = "on"
    found = []
    for line in yaml_text.splitlines():
        if state == "on":
            if line == "on:":
                state = "pull_request"
        elif state == "pull_request":
            if line and not line.startswith((" ", "#")):
                break
            if line == "  pull_request:":
                state = "paths"
        elif state == "paths":
            if line.strip() and not line.startswith("    "):
                break
            if line == "    paths:":
                state = "entries"
        else:
            if line.strip() and not line.startswith("      "):
                break
            item = re.fullmatch(r"\s{6}-\s+(.+?)\s*", line)
            if item:
                pattern = item.group(1).strip()
                if len(pattern) >= 2 and pattern[0] == pattern[-1] and pattern[0] in "'\"":
                    pattern = pattern[1:-1]
                if pattern.startswith("!"):
                    raise ValueError(f"negated CNet trigger requires explicit path-order review: {pattern}")
                found.append(pattern)
    if not found:
        raise ValueError("CNet workflow missing on.pull_request.paths")
    return frozenset(found)


def required_filters(cmake_text: str) -> frozenset[str]:
    owner_patterns = {
        f"{module}/{kind}/**"
        for module in linked_modules(cmake_text)
        for kind in ("src", "include")
    }
    owner_cmake = {f"{module}/CMakeLists.txt" for module in linked_modules(cmake_text)}
    return REQUIRED_BASE | owner_patterns | owner_cmake


def validate(workflow_text: str, cmake_text: str) -> frozenset[str]:
    actual = pull_request_paths(workflow_text)
    expected = required_filters(cmake_text)
    missing = expected - actual
    if missing:
        raise ValueError("CNet pinned-provider PR triggers missing: " + ", ".join(sorted(missing)))
    return expected


def main() -> None:
    expected = validate(
        PROVIDER_WORKFLOW.read_text(encoding="utf-8"),
        CNET_CMAKE.read_text(encoding="utf-8"),
    )
    print(f"PASS: CNet pinned-provider TLS CI covers {len(expected)} required owner/build globs")


if __name__ == "__main__":
    main()
