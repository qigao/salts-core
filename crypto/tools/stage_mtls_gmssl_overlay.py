#!/usr/bin/env python3
"""Stage Crypto-owned, test-only GmSSL TLS1.2 mTLS source correction.

The pinned vcpkg-cache port 3.2.0#14 is copied into an ephemeral per-run
overlay. This script never mutates the registry, installed stable GmSSL,
production CNet or any checked-in upstream tree.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import sys

HERE = Path(__file__).resolve()
REPOSITORY = HERE.parents[2]
PATCH_SOURCES = tuple(
    REPOSITORY / "vendor" / "gmssl-extensions" / "patches" / name
    for name in ("tls12-empty-authorities.cmake",
                 "tls13-session-ticket-bounds.cmake")
)
HANDSHAKE_SOURCE = (
    REPOSITORY / "vendor" / "gmssl-extensions" / "src" /
    "tls13-handshake-reader.h"
)
# Byte-exact original #14 port header; fail on unexpected port contents,
# never silently overwrite a different TLS1.3 implementation.
PINNED_TLS13_HANDSHAKE_GIT_BLOB = "a93b8f930238b5eae0034f84bc022fee6b73d587"
ANCHOR = 'include("${CMAKE_CURRENT_LIST_DIR}/tls12-rsa-mtls.cmake")'
HRR_SOURCE = 'include("${CMAKE_CURRENT_LIST_DIR}/hardening/tls13-hrr-compat-ccs.cmake")'
EXTRAS = tuple(
    'include("${CMAKE_CURRENT_LIST_DIR}/' + patch.name + '")'
    for patch in PATCH_SOURCES
)


def stage(port: Path) -> None:
    portfile = port / "portfile.cmake"
    manifest = port / "vcpkg.json"
    if not (portfile.is_file() and manifest.is_file()
            and HANDSHAKE_SOURCE.is_file()
            and all(patch.is_file() for patch in PATCH_SOURCES)):
        raise ValueError("missing pinned GmSSL port or Crypto source adaptation")
    metadata = json.loads(manifest.read_text(encoding="utf-8"))
    if metadata.get("name") != "gmssl" or metadata.get("version") != "3.2.0" or (
        metadata.get("port-version") != 14
    ):
        raise ValueError("expected only unmodified pinned GmSSL 3.2.0#14")
    original = portfile.read_text(encoding="utf-8")
    if original.count(ANCHOR) != 1 or original.count(HRR_SOURCE) != 1 or (
        any(extra in original for extra in EXTRAS)
    ):
        raise ValueError("GmSSL #14 mTLS/HRR source anchors must occur exactly once")
    if any((port / patch.name).exists() for patch in PATCH_SOURCES):
        raise ValueError("refusing to overwrite an existing GmSSL source patch")
    frozen = port / "tls13-handshake-reader.h"
    if not frozen.is_file():
        raise ValueError("missing original pinned TLS13 handshake reader")
    frozen_bytes = frozen.read_bytes()
    actual_blob = hashlib.sha1(
        b"blob " + str(len(frozen_bytes)).encode("ascii") + b"\0" + frozen_bytes
    ).hexdigest()
    if actual_blob != PINNED_TLS13_HANDSHAKE_GIT_BLOB:
        raise ValueError("unrecognized TLS13 reader: refusal to replace patched source")
    for patch in PATCH_SOURCES:
        shutil.copyfile(patch, port / patch.name)
    shutil.copyfile(HANDSHAKE_SOURCE, frozen)
    portfile.write_text(original.replace(ANCHOR, ANCHOR + "\n" + "\n".join(EXTRAS)),
                        encoding="utf-8")
    print("PASS: Crypto-owned TLS12 CA-name patch and single strict TLS13 reader staged")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: stage_mtls_gmssl_overlay.py <private overlay gmssl port>")
    try:
        stage(Path(sys.argv[1]).resolve(strict=True))
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        raise SystemExit("FAIL: M5 GmSSL source staging: " + str(exc)) from None
