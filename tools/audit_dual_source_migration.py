#!/usr/bin/env python3
"""Immutable dual-origin SaltsCore migration provenance and release admission.

PATH/BLOB inventory is never claimed to prove compiled functions, installed ABI,
owner lifetime or original behavioral tests. The --full gate is fail-closed and
remains RED while any original Core-owned code/API is absent or unqualified.
Upstream CMeta/CFlow/SaltsUtils code is accounted for, NOT imported into Core.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MANIFESTS = {
    "salts": {
        "path": "migration/original_salts_files.json",
        "blob": "74ba8bd8f1dda2b8c719d6deb272be140a9bbb2f",
        "source": "qigao/salts",
        "commit": "386f81328c656f416d61a6d2ef662f05443e1b4a",
        "tree": "e0c0881d202a8c9620fd7d3d2d4611a03a8aa8df",
        "count": 1554,
        "native": 745,
    },
    "salts-utils": {
        "path": "migration/original_salts_utils_files.json",
        "blob": "45707202e08562d46559579e992437b857e684b9",
        "source": "qigao/salts-utils",
        "commit": "5b0be65de673261fc37b29485b0364ca38cc6cd6",
        "tree": "dfd9db4622f137595a4ca5b7b5abcb60eca95894",
        "count": 1421,
        "native": 152,
    },
}
API_FILE = "migration/original_public_apis.json"
API_BLOB = "1093eecf2f3518e75c3d327374332e39c0c030c1"
ORIGINAL_UTILS_API_BLOB = "922dd393f57a0f8cd68b1dc97771c47c427ccf65"
EXPECTED_PUBLIC_HEADERS = 96
EXPECTED_API_CANDIDATES = 1191
REQUIRED_SOURCES = {
    "salts-utils": {
        "capture/include/salts_capture.h": "salts-core",
        "playback/include/salts_playback.h": "salts-core",
        "salts_serial/salts_serial.h": "salts-core",
        "cflow-usb/include/cflow/usb.h": "salts-core",
        "cflow-fs/include/salts/fs_watch.h": "salts-core",
        "cflow-fs/include/salts/fs_watch_publisher.h": "salts-utils",
        "cflow-process/include/salts/process_retainer.h": "split_review",
        "parser/csv_parser/include/csv_parser.h": "salts-utils",
        "jinja/CMakeLists.txt": "salts-utils",
    },
    "salts": {
        "utils/include/tstr.h": "salts-core",
        "utils/include/tlog.h": "salts-core",
        "utils/include/re.h": "salts-core",
        "utils/src/re.c": "salts-core",
        "utils/src/cmeta_mmap.c": "salts-core",
        "utils/src/ac_automaton.c": "salts-core",
        "utils/src/levenshtein_automaton.c": "salts-core",
        "cnet/sctp/src/sctp_gmssl_mac.c": "salts-core",
        "native-io/include/salts/native_io_ace_token.h": "salts",
        "coroutine/include/cmeta/coroutine.h": "salts",
        "cmeta/CMakeLists.txt": "salts",
    },
}
SAFE_PATH = re.compile(r"[A-Za-z0-9_+.@/ -]+\Z")
SHA40 = re.compile(r"[0-9a-f]{40}\Z")
NAMES = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
OWNERS = {
    "salts-core", "salts", "salts-utils", "split_review",
    "infrastructure", "third_party_review",
}
CORE_SCOPES = {"native_migration"}
BASELINE_STATES = {
    "same_path_same_blob", "same_path_changed",
    "same_blob_elsewhere", "unaccounted_in_core",
}
SNAPSHOT = {
    "repo": "qigao/salts-core",
    "commit": "012fa1a5effc956ba764509034b3c265d9b9aa79",
    "tree": "63aa91c5403e08dbfeeb44e9c57cdd36e95758e4",
}


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode("ascii") + b"\0" + data
    ).hexdigest()


def verified_json(root: Path, path: str, expected_sha: str) -> dict:
    file = root / path
    if not file.is_file() or file.is_symlink():
        raise ValueError(f"missing immutable migration source inventory: {path}")
    data = file.read_bytes().replace(b"\r\n", b"\n")
    if git_blob(data) != expected_sha:
        raise ValueError(f"migration provenance Git blob changed: {path}")
    parsed = json.loads(data)
    if not isinstance(parsed, dict):
        raise ValueError(f"non-object source inventory {path}")
    return parsed


def read_all(root: Path = ROOT) -> tuple[dict[str, dict], dict]:
    documents = {}
    for key, contract in MANIFESTS.items():
        data = verified_json(root, contract["path"], contract["blob"])
        source = data.get("source", {})
        if (source.get("repo") != contract["source"]
                or source.get("commit") != contract["commit"]
                or source.get("tree") != contract["tree"]
                or source.get("count") != contract["count"]
                or data.get("snapshot") != SNAPSHOT
                or data.get("schema_version") != 1):
            raise ValueError(f"wrong original Git Tree or source owner: {key}")
        entries = data.get("files")
        if not isinstance(entries, list) or len(entries) != contract["count"]:
            raise ValueError(f"original source files were dropped: {key}")
        visited = {}
        native_count = 0
        for entry in entries:
            path = entry.get("path")
            if (not isinstance(path, str) or not SAFE_PATH.fullmatch(path)
                    or path.startswith("/") or "\\" in path
                    or any(q in ("", ".", "..") for q in path.split("/"))
                    or path in visited):
                raise ValueError(f"unsafe/duplicate source path in {key}: {path}")
            sha = entry.get("blob")
            if not isinstance(sha, str) or not SHA40.fullmatch(sha):
                raise ValueError(f"bad source Git blob: {key}/{path}")
            owner = entry.get("owner")
            if owner not in OWNERS or entry.get("status") not in BASELINE_STATES:
                raise ValueError(f"unknown native ownership/status: {key}/{path}")
            if owner == "salts-core":
                native_count += 1
                if entry.get("scope") != "native_migration":
                    raise ValueError(f"native source not assigned to Core: {key}/{path}")
                expected = entry.get("expected")
                if (not isinstance(expected, str)
                        or not SAFE_PATH.fullmatch(expected)
                        or any(q in ("", ".", "..") for q in expected.split("/"))):
                    raise ValueError(f"Core-owned source lacks destination: {key}/{path}")
            if not isinstance(entry.get("size"), int) or entry["size"] < 0:
                raise ValueError(f"unbounded/invalid source size: {key}/{path}")
            visited[path] = entry
        if native_count != contract["native"]:
            raise ValueError(f"Core native ownership inventory changed: {key} "
                             f"{native_count} != {contract['native']}")
        for mandatory, owner in REQUIRED_SOURCES[key].items():
            if mandatory not in visited or visited[mandatory]["owner"] != owner:
                raise ValueError(f"missing or reassigned critical public capability: "
                                 f"{key}/{mandatory}")
        documents[key] = {"raw": data, "index": visited}
    api = verified_json(root, API_FILE, API_BLOB)
    if api.get("schema_version") != 1 or len(api.get("headers", [])) != EXPECTED_PUBLIC_HEADERS:
        raise ValueError("original public C declarations omitted/rewritten")
    if api.get("sources") != {
        k: {"commit": v["commit"], "tree": v["tree"]}
        for k, v in MANIFESTS.items()
    }:
        raise ValueError("public functions refer to different source revisions")
    total, seen = 0, set()
    for entry in api["headers"]:
        source = entry.get("source")
        path = entry.get("header")
        if source not in documents or path not in documents[source]["index"]:
            raise ValueError(f"unknown public source header: {source}/{path}")
        marker = (source, path)
        if marker in seen:
            raise ValueError(f"duplicated public source header: {marker}")
        seen.add(marker)
        if entry.get("source_blob") != documents[source]["index"][path]["blob"]:
            raise ValueError(f"public API source SHA drift: {source}/{path}")
        if not isinstance(entry.get("owner"), str) or not entry["owner"].strip():
            raise ValueError(f"unowned public API declaration: {source}/{path}")
        functions = entry.get("functions")
        if (not isinstance(functions, list)
                or len(set(functions)) != len(functions)
                or any(not isinstance(n, str) or not NAMES.fullmatch(n)
                       for n in functions)):
            raise ValueError(f"invalid/duplicated C function names: {source}/{path}")
        total += len(functions)
    if total != EXPECTED_API_CANDIDATES:
        raise ValueError(f"original declared public API inventory changed: "
                         f"{total} != {EXPECTED_API_CANDIDATES}")
    return documents, api


def gap_summary(documents: dict[str, dict], api: dict) -> dict:
    native = []
    for source, d in documents.items():
        for row in d["raw"]["files"]:
            if row["owner"] == "salts-core":
                native.append((source, row))
    missing = [(name, p["path"]) for name, p in native
               if p["status"] == "unaccounted_in_core"]
    needs_macro = [
        (x["source"], x["header"]) for x in api["headers"]
        if not x["functions"]
    ]
    return {
        "original_source_blobs": sum(v["source"]["count"]
                                     for v in (d["raw"] for d in documents.values())),
        "native_core_file_candidates": len(native),
        "original_unaccounted_core_baseline": len(missing),
        "public_header_candidates": len(api["headers"]),
        "explicit_declaration_candidates": sum(len(x["functions"])
                                                for x in api["headers"]),
        "macro_inline_review_headers": len(needs_macro),
        "missing_example": missing[:12],
        "macro_inline_review_example": needs_macro[:8],
    }


def enforce_full(root: Path, docs: dict[str, dict], api: dict) -> None:
    """Fail closed; do not turn a copied source or header into ABI acceptance.

    A complete cutover must supply separate installed C11/C++17, cross-platform
    source/test and original-function evidence for every Core-owned entry.
    These authoritative attestations have NOT yet been completed.
    """
    missing = []
    for repo, document in docs.items():
        for row in document["raw"]["files"]:
            if row["owner"] != "salts-core":
                continue
            target = root / row["expected"]
            if not target.is_file() or target.is_symlink():
                missing.append(f"{repo}:{row['path']} -> {row['expected']}")
    if missing:
        raise ValueError(f"FULL MIGRATION NOT ACCEPTED: {len(missing)} "
                         f"original Core-owned files missing at assigned paths. "
                         f"First: {', '.join(missing[:8])}")
    # Exact-HEAD functional acceptance must be supplied separately from the
    # immutable historical inventories. No future reviewer may turn a
    # successful source copy into a claim about exported symbols or behavior.
    proof = root / "migration/native-functional-qualification.json"
    if not proof.is_file() or proof.is_symlink():
        raise ValueError(
            "FULL MIGRATION NOT ACCEPTED: missing per-file and per-function "
            "installed ABI/behavior/CI qualification ledger"
        )
    evidence = json.loads(proof.read_text(encoding="utf-8"))
    if evidence.get("schema_version") != 1:
        raise ValueError("FULL MIGRATION NOT ACCEPTED: wrong qualification schema")
    try:
        import subprocess
        exact_head = subprocess.check_output(
            ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
        ).strip()
    except Exception as exc:
        raise ValueError("FULL MIGRATION NOT ACCEPTED: no exact Git HEAD") from exc
    if evidence.get("head") != exact_head or not SHA40.fullmatch(exact_head):
        raise ValueError("FULL MIGRATION NOT ACCEPTED: no exact HEAD CI identity")
    sources = evidence.get("files", {})
    functions = evidence.get("functions", {})
    if not isinstance(sources, dict) or not isinstance(functions, dict):
        raise ValueError("FULL MIGRATION NOT ACCEPTED: invalid proof collections")
    native_files = {
        f"{repo}:{row['path']}": row
        for repo, doc in docs.items()
        for row in doc["raw"]["files"]
        if row["owner"] == "salts-core"
    }
    if set(sources) != set(native_files):
        raise ValueError(
            "FULL MIGRATION NOT ACCEPTED: original file proof incomplete/extra: "
            f"{len(native_files)} required vs {len(sources)} provided"
        )
    for key, row in native_files.items():
        proof_entry = sources[key]
        if (not isinstance(proof_entry, dict)
                or proof_entry.get("destination") != row["expected"]
                or proof_entry.get("source_blob") != row["blob"]
                or not proof_entry.get("native_target")
                or len(proof_entry.get("tests", [])) < 2):
            raise ValueError(f"FULL MIGRATION NOT ACCEPTED: unverified source {key}")
    native_functions = {
        f"{h['source']}:{h['header']}:{name}": h
        for h in api["headers"]
        if h["owner"] == "salts-core" or h["owner"].startswith("salts-core/")
        for name in h["functions"]
    }
    if set(functions) != set(native_functions):
        raise ValueError(
            "FULL MIGRATION NOT ACCEPTED: public function proof incomplete/extra: "
            f"{len(native_functions)} required vs {len(functions)} provided"
        )
    for key, h in native_functions.items():
        f = functions[key]
        if (not isinstance(f, dict)
                or f.get("source_header_blob") != h["source_blob"]
                or not f.get("compiled_export")
                or not f.get("installed_c11")
                or not f.get("installed_cpp17")
                or not f.get("original_behavior_test")):
            raise ValueError(f"FULL MIGRATION NOT ACCEPTED: missing ABI/behavior {key}")
    # Generated macro/inline API families are not counted by conservative
    # prototypes. They require an additional explicit reviewed ledger.
    review = {
        f"{h['source']}:{h['header']}" for h in api["headers"]
        if len(h["functions"]) == 0
    }
    if set(evidence.get("macro_inline_review", {})) != review:
        raise ValueError(
            "FULL MIGRATION NOT ACCEPTED: unreviewed macro/inline/generated public ABI"
        )
    if any(not value.get("reviewed_behavior_tests")
           for value in evidence["macro_inline_review"].values()):
        raise ValueError("FULL MIGRATION NOT ACCEPTED: untested macro/inline behavior")
    if evidence.get("split_owner_review_complete") is not True:
        raise ValueError("FULL MIGRATION NOT ACCEPTED: unresolved Core vs CMeta/CFlow ownership")
    print("PASS: exact-head complete native source, exported ABI and original behavior proofs")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--full", action="store_true",
                        help="strict incomplete cutover is RED, not waived")
    args = parser.parse_args()
    docs, api = read_all()
    summary = gap_summary(docs, api)
    for key, value in summary.items():
        print(f"{key}={value}")
    if args.full:
        enforce_full(ROOT, docs, api)
    print("PASS: immutable complete 2-source inventory (provenance ONLY)")
    print("No source/function/test parity claim: --full remains fail-closed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
