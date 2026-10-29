#!/usr/bin/env python3
"""Verify local Crypto inputs; optionally reproduce them from the pinned source.

Normal builds never download, patch or import a provider. --upstream is a
maintainer-only check against an already obtained pristine GmSSL checkout.
It applies the recorded derivation in a temporary directory, never to the
supplied checkout or to the checked-in local implementation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "crypto/native/gmssl"
REVIEWED_NATIVE_CMAKE_BLOB = "0498c615298938e36ac222637d94a2960202a731"


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def relative_file(root: Path, relative: str) -> Path:
    path = root / relative
    if Path(relative).is_absolute() or ".." in Path(relative).parts:
        raise ValueError("nonlocal native source path: " + relative)
    return path


def verify_build_recipe(lock: dict) -> None:
    recipe = lock["build_recipe"]
    if recipe["path"] != "CMakeLists.txt" or recipe["git_blob"] != REVIEWED_NATIVE_CMAKE_BLOB:
        raise ValueError("native build recipe has no reviewed identity")
    data = (NATIVE / "CMakeLists.txt").read_bytes()
    if git_blob(data) != REVIEWED_NATIVE_CMAKE_BLOB:
        raise ValueError("changed reviewed native OBJECT build recipe")
    text = data.decode("utf-8")
    common = re.search(r"add_library\(salts_crypto_native_objects OBJECT\s+(.*?)\)", text, re.S)
    definitions = re.search(
        r"target_compile_definitions\(salts_crypto_native_objects PRIVATE\s+(ENABLE_.*?)\)",
        text, re.S)
    if common is None or common.group(1).split() != lock["common_sources"]:
        raise ValueError("native common source profile differs from the real OBJECT recipe")
    if definitions is None or definitions.group(1).split() != lock["compile_definitions"]:
        raise ValueError("native feature profile differs from the real OBJECT recipe")
    platforms = [match.split() for match in re.findall(
        r"target_sources\(salts_crypto_native_objects PRIVATE\s+(.*?)\)", text)]
    expected = [lock["platform_sources"][name]
                for name in ("windows", "apple", "getentropy", "portable")]
    if platforms != expected:
        raise ValueError("native platform source selection differs from the real OBJECT recipe")


def verify(upstream: Path | None = None) -> int:
    lock = json.loads((NATIVE / "source-lock.json").read_text(encoding="utf-8"))
    if lock["owner"] != "Salts::Crypto" or lock["upstream"]["commit"] != (
            "7c9f02904ef33e59c87b4f16621cc8fd434e7579"):
        raise ValueError("unexpected local native source identity")
    verify_build_recipe(lock)
    entries = lock["files"]
    names = [entry["path"] for entry in entries]
    if len(names) != len(set(names)):
        raise ValueError("duplicate native input")
    actual = {path.relative_to(NATIVE).as_posix()
              for directory in (NATIVE / "src", NATIVE / "include")
              for path in directory.rglob("*") if path.is_file()}
    if actual != set(names) - {"LICENSE"}:
        raise ValueError("untracked or missing local native source/header")
    for entry in entries:
        path = relative_file(NATIVE, entry["path"])
        if git_blob(path.read_bytes()) != entry["local_git_blob"]:
            raise ValueError("changed local native input: " + entry["path"])
    for entry in lock["core_adaptations"]:
        path = relative_file(ROOT, entry["path"])
        if git_blob(path.read_bytes()) != entry["git_blob"]:
            raise ValueError("changed original Core adaptation: " + entry["path"])
    derivation = lock["derivation_patch"]
    patch = relative_file(NATIVE, derivation["path"])
    if git_blob(patch.read_bytes()) != derivation["git_blob"]:
        raise ValueError("changed native source derivation patch")
    source_sets = [lock["common_sources"], *lock["platform_sources"].values()]
    for sources in source_sets:
        if len(sources) != len(set(sources)) or not set(sources).issubset(names):
            raise ValueError("invalid or duplicated native translation unit")
    if set(lock["common_sources"]) & set().union(*(
            set(items) for items in lock["platform_sources"].values())):
        raise ValueError("platform source compiled twice")
    if upstream is not None:
        upstream = upstream.resolve(strict=True)
        with tempfile.TemporaryDirectory(prefix="saltscore-native-reproduce-") as temporary:
            staged = Path(temporary)
            for entry in entries:
                expected = entry["upstream_git_blob"]
                if expected is None:
                    continue
                data = relative_file(upstream, entry["path"]).read_bytes()
                if git_blob(data) != expected:
                    raise ValueError("upstream source identity mismatch: " + entry["path"])
                target = relative_file(staged, entry["path"])
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            for options in (("--check",), ()):
                subprocess.run(["git", "apply", *options, str(patch)], cwd=staged,
                               capture_output=True, text=True, check=True)
            for entry in entries:
                data = relative_file(staged, entry["path"]).read_bytes()
                if git_blob(data) != entry["local_git_blob"]:
                    raise ValueError("source derivation mismatch: " + entry["path"])
    return len(entries)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path,
                        help="verify derivation from the pristine pinned upstream source")
    args = parser.parse_args()
    try:
        count = verify(args.upstream)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print("FAIL: local Crypto source: " + str(error))
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr)
        return 1
    print(f"PASS: {count} pinned local Crypto source/header/license inputs" +
          ("; upstream derivation reproduced byte-for-byte" if args.upstream else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
