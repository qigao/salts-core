#!/usr/bin/env python3
"""Strict original-source migration audit (Salts + salts-utils).

This is a provenance and function-ownership qualification, NOT a claim that
unchanged filenames have equivalent C11 behavior. The default report accepts
known debt while freezing ALL upstream Git blob paths and their owners.
--require-complete is the final fail-closed gate and must remain RED until
every Core candidate has explicit function, lifetime and test qualification.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ORIGINS = {
    "salts": {
        "file": "tools/migration/original-salts-files.json",
        "blob": "05bc04582efc1e000b35f76288ee3b35fbe45e06",
        "repo": "qigao/salts",
        "commit": "386f81328c656f416d61a6d2ef662f05443e1b4a",
        "tree": "e0c0881d202a8c9620fd7d3d2d4611a03a8aa8df",
        "count": 1554,
        "owners": {"salts-core-candidate": 759,
                   "salts-upstream-or-build-support": 795},
    },
    "salts-utils": {
        "file": "tools/migration/original-salts-utils-files.json",
        "blob": "f70913c46ba4596b40d336d171024d4c4762cf3e",
        "repo": "qigao/salts-utils",
        "commit": "5b0be65de673261fc37b29485b0364ca38cc6cd6",
        "tree": "dfd9db4622f137595a4ca5b7b5abcb60eca95894",
        "count": 1421,
        "owners": {"salts-core-candidate": 126,
                   "split-native-core-and-cflow-adapter": 39,
                   "salts-utils-upstream-or-build-support": 1256},
    },
}
STATES = frozenset({
    "upstream-retained-verify-ownership",
    "unaccounted-source-needs-owner",
    "changed-at-expected-path-needs-api-and-behavior-review",
    "byte-identical-at-expected-path",
    "byte-identical-relocated",
})
CORE_PENDING = frozenset({
    "unaccounted-source-needs-owner",
    "changed-at-expected-path-needs-api-and-behavior-review",
})
CRITICAL_SHA = {
    ("salts", "utils/include/cmeta_mmap.h"):
        "ba7aecd588492814ebca196d66023dfc9f24801c",
    ("salts", "utils/src/cmeta_mmap.c"):
        "04e87de7bc565d77261e2de66c84380b85a0ac15",
    ("salts", "utils/include/re.h"):
        "2b99ec662352677b21aceddfc13dea6d624df15f",
    ("salts", "utils/src/re.c"):
        "33468540fe02a2adcc7fd7a5303eb990d1521a31",
    ("salts", "utils/include/tstr.h"):
        "c67b3f9b108d3c0be2c468494d1e818fad7ad970",
    ("salts", "utils/include/tlog.h"):
        "8dff585fbb60c50a7474f88263d0dfb0ba808057",
    ("salts", "simd/include/salts/simd.h"):
        "304f01ed618a6a96e113a0362d6bb6e1c198999f",
    ("salts-utils", "capture/include/salts_capture.h"):
        "c206fb0940f4fb8162c469ecd823c377f4890cc1",
    ("salts-utils", "playback/include/salts_playback.h"):
        "44c7e605d714e62cf831147b6d1948edb5526ac1",
    ("salts-utils", "salts_serial/salts_serial.h"):
        "4cbc484baaad2f47564e50c65207405912dd2d29",
    ("salts-utils", "salts_serial/salts_serial.c"):
        "45aca30915068504deede7a59416ff26089efd7e",
    ("salts-utils", "cflow-fs/include/salts/fs_watch.h"):
        "f70748b71d1d0810bb6430c0eb4335a9b351b1ac",
    ("salts-utils", "cflow-usb/include/cflow/usb.h"):
        "60321840d9755ba65d26d3f98504f06e8d618746",
}
MMAP_EXPORTS = (
    "cmeta_mmap_init", "cmeta_mmap_open", "cmeta_mmap_open_range",
    "cmeta_mmap_from_fd", "cmeta_mmap_sync", "cmeta_mmap_sync_range",
    "cmeta_mmap_unmap", "cmeta_mmap_close", "cmeta_mmap_page_size",
    "cmeta_mmap_page_count", "cmeta_mmap_pages", "cmeta_mmap_advise",
    "cmeta_mmap_lock", "cmeta_mmap_unlock", "cmeta_mmap_group_init",
    "cmeta_mmap_group_open", "cmeta_mmap_group_close",
    "cmeta_mmap_strerror",
)


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode("ascii") + b"\0" + data
    ).hexdigest()


def verify_origin(root: Path, key: str) -> tuple[dict, Counter]:
    cfg = ORIGINS[key]
    path = root / cfg["file"]
    raw = path.read_bytes()
    if git_blob(raw) != cfg["blob"]:
        raise ValueError(f"{key}: frozen 1,554/1,421 source manifest was modified")
    data = json.loads(raw)
    if data.get("schema") != 1 or data.get("source") != {
        "repo": cfg["repo"], "commit": cfg["commit"],
        "tree": cfg["tree"], "files": cfg["count"],
    }:
        raise ValueError(f"{key}: source commit/tree/metadata differs")
    files = data.get("files")
    if not isinstance(files, list) or len(files) != cfg["count"]:
        raise ValueError(f"{key}: source Git blob inventory truncated")
    paths = set()
    owners: Counter = Counter()
    statuses: Counter = Counter()
    critical = {p: sha for (name, p), sha in CRITICAL_SHA.items()
                if name == key}
    for row in files:
        p = row.get("path", "")
        sha = row.get("git_blob", "")
        owner = row.get("owner", "")
        status = row.get("status", "")
        if (not isinstance(p, str) or not p
                or p.startswith("/") or "\\" in p
                or any(x in ("", ".", "..") for x in p.split("/"))
                or p in paths or not re.fullmatch(r"[0-9a-f]{40}", sha)
                or not isinstance(row.get("size"), int)
                or row["size"] < 0):
            raise ValueError(f"{key}: invalid/duplicate source path or blob {p!r}")
        paths.add(p)
        if owner not in cfg["owners"] or status not in STATES:
            raise ValueError(f"{key}: missing owner/classification: {p}")
        if owner.endswith("upstream-or-build-support"):
            if status != "upstream-retained-verify-ownership":
                raise ValueError(f"{key}: upstream provenance silently recategorized: {p}")
        elif status == "upstream-retained-verify-ownership":
            raise ValueError(f"{key}: native candidate incorrectly treated as retained: {p}")
        target = row.get("target_path")
        if status in ("byte-identical-at-expected-path",
                      "byte-identical-relocated",
                      "changed-at-expected-path-needs-api-and-behavior-review"):
            if not isinstance(target, str) or not target or ".." in target.split("/"):
                raise ValueError(f"{key}: source link missing real destination path: {p}")
        elif target is not None and not owner.endswith("upstream-or-build-support"):
            # An upstream-retained file may have an identical Git blob in
            # Core for completely unrelated/shared build infrastructure.
            # That does NOT qualify the upstream function for migration.
            # Native candidates still must never claim a target while
            # classified as unaccounted.
            raise ValueError(f"{key}: unaccounted native source claims a destination: {p}")
        if p in critical and sha != critical[p]:
            raise ValueError(f"{key}: original critical algorithm or public ABI altered: {p}")
        owners[owner] += 1
        statuses[(owner, status)] += 1
    if dict(owners) != cfg["owners"] or set(critical) - paths:
        raise ValueError(f"{key}: incomplete module/function inventory")
    return data, statuses


def strip_comments(text: str) -> str:
    return re.sub(r"//[^\n]*|/\*[\s\S]*?\*/", "", text)


def verify_restored_mmap(root: Path) -> None:
    # This header/source preserves ALL 18 original native memory-map symbols.
    # A noninstalled source-only fixture or empty stub cannot satisfy these.
    header = root / "core/include/cmeta_mmap.h"
    source = root / "core/src/cmeta_mmap.c"
    installed = root / "tests/installed/CMakeLists.txt"
    test_cmake = root / "tests/CMakeLists.txt"
    core_cmake = root / "core/CMakeLists.txt"
    for file in (header, source, installed, test_cmake, core_cmake,
                 root / "tests/test_original_native_mmap.c",
                 root / "tests/installed/original_mmap_consumer.c",
                 root / "tests/installed/original_mmap_consumer.cpp"):
        if not file.is_file():
            raise ValueError("original native mmap file/test/installed owner missing: " + str(file))
    head = strip_comments(header.read_text(encoding="utf-8"))
    body = strip_comments(source.read_text(encoding="utf-8"))
    for name in MMAP_EXPORTS:
        if not re.search(r"\bSALTS_C_API\s+[\s\S]{0,135}?\b"
                         + name + r"\s*\(", head):
            raise ValueError("original mmap public function missing from installed ABI: " + name)
        if not re.search(r"\b" + name
                         + r"\s*\([^;{}]*\)\s*\{", body):
            raise ValueError("original mmap native implementation missing: " + name)
    if re.search(r'#\s*include\s*[<"](?:cmeta/|cflow/|cstl)', head + body):
        raise ValueError("mmap reintroduced a CMeta/CFlow/CSTL provider")
    if "src/cmeta_mmap.c" not in core_cmake.read_text(encoding="utf-8"):
        raise ValueError("mmap not compiled by actual Salts::Core owner")
    if "include/cmeta_mmap.h" not in core_cmake.read_text(encoding="utf-8"):
        raise ValueError("mmap original public ABI missing from installed Core")
    if "test_original_native_mmap.c" not in test_cmake.read_text(encoding="utf-8"):
        raise ValueError("original mapped file CTest not registered")
    if not all(s in installed.read_text(encoding="utf-8") for s in
               ("original_mmap_consumer.c", "original_mmap_consumer.cpp")):
        raise ValueError("installed C11/C++17 mmap client absent")


def audit(root: Path = ROOT, require_complete: bool = False) -> dict:
    output = {}
    core_debt = 0
    review_debt = 0
    for key in ORIGINS:
        data, rows = verify_origin(root, key)
        outstanding = sum(count for (owner, status), count in rows.items()
                          if owner != "salts-upstream-or-build-support"
                          and owner != "salts-utils-upstream-or-build-support"
                          and status in CORE_PENDING)
        # Source files are not equivalent simply because paths/hashes match.
        unqualified = sum(count for (owner, status), count in rows.items()
                          if owner not in ("salts-upstream-or-build-support",
                                           "salts-utils-upstream-or-build-support")
                          and status not in CORE_PENDING)
        core_debt += outstanding
        review_debt += unqualified
        output[key] = {"frozen_total": len(data["files"]),
                       "unaccounted_or_changed": outstanding,
                       "present_but_behavior_unqualified": unqualified}
    verify_restored_mmap(root)
    if require_complete and (core_debt or review_debt):
        raise ValueError(
            f"FINAL MIGRATION BLOCKED: {core_debt} missing/changed original "
            f"native files and {review_debt} present-but-unqualified file cases; "
            "function/API/tests/installed-runtime evidence must be approved")
    return output


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--require-complete", action="store_true")
    args = p.parse_args()
    try:
        proof = audit(require_complete=args.require_complete)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        print("FAIL:", exc)
        return 1
    for name, counts in proof.items():
        print(f"{name}: frozen={counts['frozen_total']}, "
              f"unaccounted_or_changed={counts['unaccounted_or_changed']}, "
              f"present_but_behavior_unqualified={counts['present_but_behavior_unqualified']}")
    print("PASS: all 2,975 pinned original Git blobs accounted for by owner; "
          "native mmap 18/18 sources, exported symbols and tests registered; "
          "this is NOT final functional equivalence")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
