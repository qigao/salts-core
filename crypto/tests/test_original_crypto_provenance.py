#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

DONORS = {
    "crypto/original/cmeta_crypto.h.original":
        "65ce9039ac2d6d83a258257b3d769d031d9fa5cb",
    "crypto/src/cmeta_crypto.c":
        "fb82a80178fdde77f174692437cf6d8a8b7038b2",
    "crypto/vendor/monocypher/UPSTREAM.md":
        "9c3c2bdeedfd8de5b3d7876ac6cc297573e12f19",
    "crypto/vendor/monocypher/core_monocypher.c":
        "184918488d0ddf69b8d8d44f8b195b94aba3cf5e",
    "crypto/vendor/monocypher/core_monocypher_ed25519.c":
        "296769c4e8da3debda6a0fb8b23714bc06a5dde6",
    "crypto/vendor/monocypher/core_monocypher_namespace.h":
        "130aae34d2dc8be2d865c1933cef76e9bcb36a6e",
    "crypto/vendor/monocypher/monocypher.c":
        "9582f857ad320046d46824513f61225f2182ea3b",
    "crypto/vendor/monocypher/monocypher.h":
        "cf635e88e4f5851e1f58a9e2b3e85b8029a73861",
    "crypto/vendor/monocypher/monocypher-ed25519.c":
        "4fdd07641e1c4d54a348e95d0ab05c28f84654a6",
    "crypto/vendor/monocypher/monocypher-ed25519.h":
        "d7aa004123e1d38b891fdabd4171fbf9957242ae",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode("ascii") + b"\0" + data
    ).hexdigest()


def public_declarations(text: str) -> list[str]:
    # Strip comments first so documentation mentions do not count as API.
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*?$", "", text, flags=re.M)
    return sorted(set(re.findall(
        r"\b(cmeta_[A-Za-z0-9_]+)\s*\(", text)))


def definitions(text: str) -> list[str]:
    return sorted(set(re.findall(
        r"(?m)^\s*(?:[A-Za-z_][A-Za-z0-9_]*\s+|\*\s*)+"
        r"(cmeta_[A-Za-z0-9_]+)\s*\([^;{}]*\)\s*\{", text)))


def main() -> int:
    for relative, expected in DONORS.items():
        path = ROOT / relative
        require(path.is_file(), "missing frozen Crypto donor: " + relative)
        require(git_blob(path.read_bytes()) == expected,
                "Crypto donor bytes changed: " + relative)

    inventory = json.loads(
        (ROOT / "tools/migration/original-public-functions.json").read_text())
    row = next(
        item for item in inventory["headers"]
        if item["path"] == "utils/include/cmeta_crypto.h")
    expected_api = sorted(row["explicit_functions"])
    require(len(expected_api) == 37,
            "frozen Crypto public-function denominator changed")

    header = (ROOT / "crypto/include/cmeta_crypto.h").read_text()
    source = (ROOT / "crypto/src/cmeta_crypto.c").read_text()
    cmake = (ROOT / "crypto/CMakeLists.txt").read_text()
    installed = (ROOT / "crypto/tests/original-installed/CMakeLists.txt").read_text()
    surface = (ROOT / "crypto/tests/original-installed/surface_smoke.c").read_text()

    require(public_declarations(header) == expected_api,
            "restored Crypto public declarations differ from frozen 37-function set")
    require(set(expected_api).issubset(definitions(source)),
            "original cmeta_crypto.c no longer defines all 37 public functions")
    require("SALTS_C_API" not in header,
            "static Crypto public surface reintroduced SALTS_C_API")
    require("#include <salts/error_codes.h>" in header,
            "Crypto surface lost the original SALTS_* result-code ABI")

    # One and only one installed Crypto archive. Monocypher is private source
    # folded directly into that archive, never a second target/provider.
    require("add_library(salts_crypto STATIC" in cmake,
            "canonical static Salts::Crypto owner is missing")
    require("src/cmeta_crypto.c" in cmake,
            "original Crypto implementation is not compiled")
    require("vendor/monocypher/core_monocypher.c" in cmake and
            "vendor/monocypher/core_monocypher_ed25519.c" in cmake,
            "private 25519 implementation is not folded into Salts::Crypto")
    for forbidden in (
        "add_library(monocypher ",
        "add_library(core_monocypher ",
        "CRYPTO_GMSSL_ROOT=",
        "find_package(GmSSL",
    ):
        require(forbidden not in cmake,
                "second/external Crypto provider path returned: " + forbidden)

    require("find_package(Crypto 0.1 CONFIG REQUIRED)" in installed,
            "installed consumer does not resolve the standalone Crypto package")
    require("Salts::Crypto" in installed and "Salts::Core" not in installed,
            "installed original Crypto consumer uses an indirect Core owner")

    referenced = set(re.findall(r"\b(cmeta_[A-Za-z0-9_]+)\s*\(", surface))
    require(set(expected_api).issubset(referenced),
            "installed C surface smoke does not reference all 37 original APIs")

    print("PASS: original 37-function Crypto surface and private provider sources retained")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
