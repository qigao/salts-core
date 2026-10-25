#!/usr/bin/env python3
"""Fail-closed ABI ownership audit of the *installed* SaltsCore runtime images.

Validates exported definitions plus direct binary imports in a clean-prefix
installed executable and a separately built plugin. A successful link/CTest
alone cannot prove the absence of duplicate process-global owners.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

PARTS = ("core", "platform", "concurrency", "coroutine", "native_io", "cnet")
OWNERS = {
    "mem_global": "core",
    "mem_buffer_release": "core",
    "mem_slice_release": "core",
    "cmeta_thread_current_token": "platform",
    "cmeta_deadline_queue_init": "concurrency",
    "coro_executor_current": "coroutine",
    "native_io_backend_init": "native_io",
    "cnet_client_init": "cnet",
}
PUBLIC_FAMILIES = re.compile(r"^(?:mem_|cmeta_|coro_|native_io_|cnet_|salts_core_|ring_)")
# 'cmeta_threadpool_current' is a file-local TLS variable, never a public API.
PRIVATE_STATE = ("cmeta_threadpool_current",)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def command(args: list[str]) -> str:
    result = subprocess.run(args, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"{args[0]} exited {result.returncode}: {result.stderr[-2000:]}"
        )
    return result.stdout


def dumpbin_path(explicit: str | None) -> str:
    if explicit:
        path = Path(explicit)
        require(path.is_file(), f"dumpbin does not exist: {path}")
        return str(path)
    found = shutil.which("dumpbin")
    if found:
        return found
    choices: list[Path] = []
    for key in ("VCToolsInstallDir", "VSINSTALLDIR"):
        if os.environ.get(key):
            root = Path(os.environ[key])
            choices += list(root.glob("bin/Hostx64/x64/dumpbin.exe"))
            choices += list(root.glob("VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"))
    for key in ("ProgramFiles", "ProgramFiles(x86)"):
        if os.environ.get(key):
            root = Path(os.environ[key]) / "Microsoft Visual Studio"
            choices += list(root.glob("*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"))
    require(bool(choices), "dumpbin.exe not found; cannot certify PE exports")
    return str(sorted(choices)[-1])


def exports_from_output(output: str, system: str) -> set[str]:
    exports: set[str] = set()
    for line in output.splitlines():
        if system == "windows":
            match = re.match(
                r"^\s*\d+\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+([A-Za-z_][A-Za-z0-9_@$?]*)",
                line,
            )
            if match:
                exports.add(match.group(1))
            continue
        fields = line.strip().split()
        if len(fields) < 3 or len(fields[-2]) != 1 or not fields[-2].isalpha():
            continue
        name = fields[-1].split("@@", 1)[0]
        if system == "darwin" and name.startswith("_"):
            name = name[1:]
        exports.add(name)
    return exports


def image_exports(path: Path, system: str, dumpbin: str | None) -> set[str]:
    if system == "windows":
        return exports_from_output(command([str(dumpbin), "/nologo", "/exports", str(path)]), system)
    if system == "darwin":
        return exports_from_output(command(["nm", "-gU", str(path)]), system)
    return exports_from_output(command(["nm", "-D", "--defined-only", str(path)]), system)


def parse_dependencies(output: str, system: str, is_dylib: bool = False) -> set[str]:
    if system == "darwin":
        # otool begins with a filename header. For a dylib *only*, the next
        # line is its install-name/self-ID; for an executable the next line
        # is its first imported library and must never be skipped.
        lines = output.splitlines()
        output = "\n".join(lines[2 if is_dylib else 1:])
    return set(re.findall(r"(?:lib)?saltscore_(core|platform|concurrency|coroutine|native_io|cnet)(?=[.\s/\\-])", output, re.I))


def binary_dependencies(path: Path, system: str, dumpbin: str | None) -> set[str]:
    if system == "windows":
        output = command([str(dumpbin), "/nologo", "/dependents", str(path)])
    elif system == "darwin":
        output = command(["otool", "-L", str(path)])
    else:
        output = command(["readelf", "-d", str(path)])
    return parse_dependencies(output, system, path.suffix == ".dylib")


def is_library(name: str, part: str, system: str) -> bool:
    if system == "windows":
        return name.lower() == f"saltscore_{part}.dll"
    if system == "darwin":
        return re.fullmatch(rf"libsaltscore_{part}(?:\.[0-9]+)*\.dylib", name) is not None
    return re.fullmatch(rf"libsaltscore_{part}\.so(?:\.[0-9]+)*", name) is not None


def find_library(prefix: Path, part: str, system: str) -> Path:
    folder = prefix / ("bin" if system == "windows" else "lib")
    require(folder.is_dir(), f"installed runtime directory missing: {folder}")
    matches = {p.resolve() for p in folder.iterdir() if is_library(p.name, part, system)}
    require(len(matches) == 1, f"{part}: expected one installed binary identity; found {sorted(map(str, matches))}")
    return next(iter(matches))


def find_build_image(build: Path, name: str, system: str, executable: bool) -> Path:
    if executable:
        patterns = [f"{name}.exe"] if system == "windows" else [name]
    elif system == "windows":
        patterns = [f"{name}.dll"]
    elif system == "darwin":
        patterns = [f"lib{name}.dylib"]
    else:
        patterns = [f"lib{name}.so"]
    paths = {p.resolve() for pattern in patterns for p in build.rglob(pattern)}
    require(len(paths) == 1, f"{name}: expected exactly one clean-prefix consumer image: {sorted(map(str, paths))}")
    return next(iter(paths))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect(prefix: Path, build: Path, system: str, dumpbin: str | None) -> dict:
    require(prefix.is_dir(), f"installed SDK missing: {prefix}")
    require(build.is_dir(), f"installed consumer build missing: {build}")
    runtime = {part: find_library(prefix, part, system) for part in PARTS}
    consumer = find_build_image(build, "abi_owner_consumer", system, True)
    probe = find_build_image(build, "owner_probe_installed", system, False)
    symbols = {part: image_exports(path, system, dumpbin) for part, path in runtime.items()}
    extras = {
        "abi_owner_consumer": image_exports(consumer, system, dumpbin),
        "owner_probe_installed": image_exports(probe, system, dumpbin),
    }
    deps = {
        part: binary_dependencies(path, system, dumpbin)
        for part, path in runtime.items()
    }
    deps.update({
        "abi_owner_consumer": binary_dependencies(consumer, system, dumpbin),
        "owner_probe_installed": binary_dependencies(probe, system, dumpbin),
    })
    failures: list[str] = []
    for symbol, expected in OWNERS.items():
        defined = [part for part, exports in symbols.items() if symbol in exports]
        if defined != [expected]:
            failures.append(f"{symbol}: expected sole runtime owner {expected}, defined in {defined}")
        for image, exported in extras.items():
            if symbol in exported:
                failures.append(f"{symbol}: unexpectedly defined by clean-prefix {image}")
    public_definitions: dict[str, list[str]] = {}
    for part, exported in symbols.items():
        for symbol in exported:
            if PUBLIC_FAMILIES.match(symbol):
                public_definitions.setdefault(symbol, []).append(part)
        for private_name in PRIVATE_STATE:
            if private_name in exported:
                failures.append(f"{part} exports private TLS state {private_name}")
    duplicates = {
        name: parts for name, parts in public_definitions.items() if len(parts) != 1
    }
    if duplicates:
        failures.append(f"duplicate public C symbols across shared owners: {duplicates}")
    for name, needed in {
        "abi_owner_consumer": {"core", "platform", "cnet"},
        "owner_probe_installed": {"core", "platform"},
        "cnet": {"core", "native_io"},
        "core": {"platform"},
    }.items():
        missing = needed - deps[name]
        if missing:
            failures.append(f"{name}: missing direct dynamic imports {sorted(missing)}; actual {sorted(deps[name])}")
    artifacts = {part: path for part, path in runtime.items()}
    artifacts.update({"abi_owner_consumer": consumer, "owner_probe_installed": probe})
    return {
        "schema_version": 1,
        "platform": system,
        "artifacts": {
            name: {
                "path": str(path),
                "sha256": sha256(path),
                "imports": sorted(deps[name]),
            } for name, path in artifacts.items()
        },
        "owned_symbols": {key: owner for key, owner in OWNERS.items()},
        "public_symbols_checked": len(public_definitions),
        "duplicate_public_symbols": duplicates,
        "failures": failures,
        "ok": not failures,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True, help="clean installed SaltsCore SDK root")
    parser.add_argument("--build-dir", type=Path, required=True, help="independent SDK consumer build root")
    parser.add_argument("--report", type=Path, required=True, help="JSON evidence artifact")
    parser.add_argument("--dumpbin", type=str, help="exact dumpbin.exe for Windows export/import audits")
    args = parser.parse_args()
    system = "windows" if os.name == "nt" else ("darwin" if sys.platform == "darwin" else "linux")
    try:
        report = inspect(
            args.prefix.resolve(), args.build_dir.resolve(), system,
            dumpbin_path(args.dumpbin) if system == "windows" else None,
        )
    except (OSError, ValueError, RuntimeError) as exc:
        report = {"schema_version": 1, "platform": system, "ok": False, "failures": [str(exc)]}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"SaltsCore installed ABI audit: {'PASS' if report['ok'] else 'FAIL'}; report={args.report}")
    for issue in report.get("failures", []):
        print(f"ERROR: {issue}", file=sys.stderr)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
