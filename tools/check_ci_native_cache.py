#!/usr/bin/env python3
"""Strict CI cache/toolchain admission. No silent local provider fallback.

The original CI tests, cross-platform matrices, and exact-head FIFO queue
remain mandatory. Only tool provenance and compiler-launcher wiring may change.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github" / "workflows"
CACHE_SHA = "7bb5b556a001bc82d2922a04a718e986a7f47f54"
MSVC_DEV_SHA = "0b201ec74fa43914dc39ae48a89fd1d8cb592756"

REQUIRED = {
    "cnet-m5-python-mtls.yml": (2, 2, 2),
    "cnet-provider.yml": (3, 3, 3),
    "cnet-sanitizers.yml": (1, 1, 1),
    "cnet-sctp-native.yml": (1, 0, 0),
    "core-contract.yml": (1, 0, 0),
    "native-fs.yml": (1, 0, 0),
    "native-modules.yml": (1, 1, 0),
    "native-process.yml": (1, 0, 0),
    "portable-compiler-abi.yml": (4, 0, 0),
    "readiness-reference-conformance.yml": (1, 0, 0),
    "readiness-reference-sanitizers.yml": (1, 0, 0),
    "uri-contract.yml": (1, 1, 0),
    "wsparser-contract.yml": (1, 0, 0),
}
# Only an ARM64 device runtime job: downloads already compiled exact PE files.
# It does not execute a C/C++ compiler, hence no compiler cache is installed.
RUN_ONLY_JOB = ("portable-compiler-abi.yml", "win-arm64-native-tinycc")

def jobs(source: str) -> dict[str, str]:
    match = re.search(r"(?m)^jobs:\s*$", source)
    if not match:
        return {}
    tail = source[match.end():]
    starts = list(re.finditer(r"(?m)^  ([a-z0-9_-]+):\s*$", tail))
    return {
        match.group(1): tail[match.start():starts[i + 1].start()
                              if i + 1 < len(starts) else len(tail)]
        for i, match in enumerate(starts)
    }

def validate_workflow(name: str, source: str) -> list[str]:
    errors = []
    if name not in REQUIRED:
        return [f"{name}: unknown workflow not accounted for in cache contract"]
    expected_cache, expected_re2c, expected_vcpkg = REQUIRED[name]
    expected_jobs = expected_cache + (1 if name == RUN_ONLY_JOB[0] else 0)
    sections = jobs(source)
    if len(sections) != expected_jobs:
        errors.append(f"{name}: removed or added native jobs")
    if source.count("id: ccache") != expected_cache:
        errors.append(f"{name}: missing per-job ccache")
    if source.count("ccache --show-stats") != expected_cache:
        errors.append(f"{name}: missing independent ccache statistics")
    if source.count("setup-re2c-tools@" + CACHE_SHA) != expected_re2c:
        errors.append(f"{name}: wrong/missing re2c cache package")
    if source.count("setup-vcpkg-curl-cache@" + CACHE_SHA) != expected_vcpkg:
        errors.append(f"{name}: wrong/missing vcpkg curl-only binary cache")
    # Exact values, not substring matching: "mode: readwrite" is NOT read-only.
    modes = re.findall(r"(?m)^ +mode: *([A-Za-z-]+) *(?:#.*)?$", source)
    if modes != ["read"] * expected_vcpkg:
        errors.append(f"{name}: unqualified binary cache write/fallback")
    if expected_re2c and not re.search(
            r"(?m)^  packages: read\s*$", source):
        errors.append(f"{name}: missing GitHub Packages read permission")
    re2c_pins = re.findall(r"setup-re2c-tools@([A-Za-z0-9_.-]+)", source)
    vcpkg_pins = re.findall(r"setup-vcpkg-curl-cache@([A-Za-z0-9_.-]+)", source)
    if (re2c_pins != [CACHE_SHA] * expected_re2c
            or vcpkg_pins != [CACHE_SHA] * expected_vcpkg):
        errors.append(f"{name}: mutable/unverified/duplicate package action")
    if "Visual Studio 18 2026" in source:
        errors.append(f"{name}: VS generator bypasses CMake ccache launcher")
    if re.search(r"(?m)^\s*.*(?:apt-get install|brew install).*\bre2c\b", source):
        errors.append(f"{name}: host re2c must come from pinned vcpkg-cache")
    if "re2c-4.6.tar.xz" in source or "cmake/ci/github-packages.py re2c" in source:
        errors.append(f"{name}: redundant/unpinned re2c source/provider")
    if "setup-vcpkg-cache@" in source or re.search(
            r"(?mi)^.*\bmono-complete\b|^\s*.*\bnuget\.exe\b", source):
        errors.append(f"{name}: forbidden Mono/NuGet vcpkg bootstrap")
    if re.search(r"(?m)^\s*.*VCPKG_BINARY_SOURCES=.*readwrite", source):
        errors.append(f"{name}: binary package producer inside CI consumer")
    if re.search(r"(?m)^      - uses: actions/checkout@v4\s*$", source):
        # Shared local action changes must retrigger every path-scoped PR CI.
        if (re.search(r"(?m)^  pull_request:\s*\n    paths:", source)
                and '.github/actions/setup-ccache/**' not in source):
            errors.append(f"{name}: local ccache action changes not observed")
    for job, fragment in sections.items():
        if (name, job) == RUN_ONLY_JOB:
            if re.search(r"\bcmake --build\b|\bgcc -c\b|\bclang -c\b", fragment):
                errors.append(f"{name}: ARM64 run-only exemption now compiles")
            continue
        if fragment.count("id: ccache") != 1 or fragment.count(
                "uses: ./.github/actions/setup-ccache") != 1:
            errors.append(f"{name}/{job}: missing sole verified local ccache")
        if "source_sha: ${{ github.event.pull_request.head.sha || github.sha }}" not in fragment:
            errors.append(f"{name}/{job}: not keyed to exact source HEAD")
        if fragment.count("ccache --show-stats") != 1:
            errors.append(f"{name}/{job}: no cache evidence")
        if "Ninja" in fragment and "Visual Studio 18 2026" in fragment:
            errors.append(f"{name}/{job}: contradictory generator")
    return errors

def validate_action(source: str) -> list[str]:
    errors = []
    required = [
        "version=4.14.2",
        "b1f7fbb60ee24b80a6abae09ec4fcc6554fbf98c933c9bd25a3f53ab685fc00b",
        "bc537a1f29232be50be3219ffe80c7c1955bc80a99c75311d7fb839828c80dd0",
        "cf240009630b605b775c7c5c7b7105a3b196c79f31af322c1557969aa28003fc",
        "6993ce645e53192cec592ee23186200b574ac4c59097ecde8d9cfd681b47ae84",
        "47ea5a0ed661bea53962d8fb8ee9a1769092d979a6f7c73d3081499b3305b2ab",
        "macOS/X64|macOS/ARM64",
        "CMAKE_C_COMPILER_LAUNCHER=ccache",
        "CMAKE_CXX_COMPILER_LAUNCHER=ccache",
        "CMAKE_GENERATOR=Ninja",
        "CCACHE_COMPILERCHECK=content",
        "CCACHE_DIR=",
        "actions/cache@v4",
    ]
    for marker in required:
        if marker not in source:
            errors.append("ccache action missing verified requirement: " + marker)
    if "stun-ccache/" in source:
        errors.append("ccache wrongly reuses Stun's cache directory")
    return errors

def main() -> int:
    files = list(WORKFLOWS.glob("*.yml"))
    found = {f.name for f in files}
    errors = []
    if found != REQUIRED.keys():
        errors.append(f"workflow inventory changed: expected {sorted(REQUIRED)}, got {sorted(found)}")
    for f in files:
        if f.name in REQUIRED:
            errors.extend(validate_workflow(f.name, f.read_text(encoding="utf-8")))
    action = ROOT / ".github/actions/setup-ccache/action.yml"
    if not action.is_file():
        errors.append("single verified ccache action missing")
    else:
        errors.extend(validate_action(action.read_text(encoding="utf-8")))
    for error in errors:
        print("FAIL:", error)
    if errors:
        return 1
    print("PASS: all 13 original workflows and native jobs use exact ccache/"
          "re2c/vcpkg-cache owners; no weakened CI or compiler launcher bypass")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
