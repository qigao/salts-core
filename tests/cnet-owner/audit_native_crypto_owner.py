#!/usr/bin/env python3
"""Audit the real production CNet link, including hidden static TLS symbols.

The Crypto archive is allowed in the production CNet image only. The existing
two plugins and their loader must import that shared owner, never extract
another Crypto archive. Link maps are mandatory evidence on every platform.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


NATIVE_SYMBOLS = (
    "tls_ctx_init", "tls_ctx_cleanup", "tls_init", "tls_cleanup", "tls_set_io",
    "tls_do_handshake", "tls_send", "tls_recv", "tls_shutdown",
    "tls_get_peer_certificate", "tls_get_peer_certificate_sha256",
    "tls_export_keying_material",
)
ARCHIVE_SYMBOLS = NATIVE_SYMBOLS + (
    "crypto_tls_session_handshake", "crypto_tls_session_create_auth_v1_3",
    "crypto_tls_cipher_bio_create", "salts_crypto_sha256",
    "salts_crypto_ed448_sign",
)
CRYPTO_MEMBER = re.compile(
    r"(?:^|[/\\\s])(?:lib)?salts_crypto(?:[.](?:a|lib))?(?:\(|:)", re.I)
CRYPTO_ARCHIVE = re.compile(
    r"(?:^|[/\\\s])(?:lib)?salts_crypto(?:[.](?:a|lib))?(?:\(|:|[\s]|$)", re.I)
EXTERNAL_CRYPTO = re.compile(
    r"^(?:lib)?(?:gmssl|ssl|crypto|salts_crypto)(?:[-.]|$)", re.I)
EXTERNAL_ARCHIVE = re.compile(
    r"(?:^|[/\\\s])(?:lib)?(?:gmssl|ssl|crypto)[.](?:a|lib)(?:[\s(:]|$)",
    re.I | re.M)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def command(arguments):
    result = subprocess.run(arguments, text=True, capture_output=True)
    require(result.returncode == 0,
            f"symbol/dependency reader failed: {arguments[0]}: {result.stderr[-2000:]}")
    return result.stdout


def normalized(name, system):
    if system == "Darwin":
        name = name.removeprefix("_")
    return name.split("@@", 1)[0]


def archive_definitions(output, system):
    """Count strong definitions, retaining duplicate members in the archive."""
    result = Counter()
    for line in output.splitlines():
        if system == "Windows":
            if ("External" not in line or "WeakExternal" in line
                    or not re.search(r"\bSECT[0-9A-F]+\b", line) or "|" not in line):
                continue
            name = line.split("|", 1)[1].strip().split()[0]
        else:
            fields = line.split()
            if len(fields) < 3 or fields[-2] not in ("T", "D", "B", "R", "S", "G", "C"):
                continue
            name = normalized(fields[-1], system)
        # Compiler-generated COFF COMDAT constants/refptrs are not native C
        # definitions. Their coalescing is unrelated to duplicate TLS objects.
        if name.startswith((".", "__", "$")):
            continue
        result[name] += 1
    return result


def validate_archive(definitions):
    missing = {name: definitions[name] for name in ARCHIVE_SYMBOLS
               if definitions[name] != 1}
    require(not missing, "native/facade definitions missing or duplicated: " + str(missing))
    duplicates = {name: count for name, count in definitions.items() if count > 1}
    require(not duplicates, "duplicate strong Crypto archive definitions: " + str(duplicates))


def map_definitions(text, system):
    """Read the selected physical member for each required native TLS symbol."""
    result = defaultdict(list)
    if system == "Darwin":
        objects = {}
        for line in text.splitlines():
            match = re.match(r"^\[\s*(\d+)\]\s+(.+)$", line)
            if match:
                objects[match[1]] = match[2].strip()
            match = re.match(
                r"^\s*0x[0-9a-fA-F]+\s+0x[0-9a-fA-F]+\s+\[\s*(\d+)\]\s+(\S+)",
                line)
            if match:
                name = normalized(match[2], system)
                if name in NATIVE_SYMBOLS:
                    result[name].append(objects.get(match[1], "unidentified object"))
    elif system == "Windows":
        for line in text.splitlines():
            match = re.match(
                r"^\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+[0-9a-fA-F]+"
                r"(?:\s+[fi])*\s+(\S+)\s*$", line)
            if match and match[1] in NATIVE_SYMBOLS:
                result[match[1]].append(match[2])
    else:
        owner = ""
        object_pattern = re.compile(r"(\S+[.](?:a|lib)\([^)]+\)|\S+[.](?:o|obj))")
        for line in text.splitlines():
            obj = object_pattern.search(line)
            if obj:
                owner = obj[1]
            match = re.match(
                r"^\s*(?:(?:0x)?[0-9a-fA-F]+\s+)+([A-Za-z_][A-Za-z_0-9]*)\s*$",
                line)
            if match and match[1] in NATIVE_SYMBOLS:
                result[match[1]].append(owner)
    return result


def validate_runtime_map(text, system):
    require(not EXTERNAL_ARCHIVE.search(text),
            "production link selected an external Crypto/GmSSL archive")
    definitions = map_definitions(text, system)
    invalid = {name: definitions[name] for name in NATIVE_SYMBOLS
               if len(definitions[name]) != 1 or
               not CRYPTO_MEMBER.search(definitions[name][0])}
    require(not invalid, "production TLS is not supplied once by local Crypto: " + str(invalid))
    return dict(definitions)


def validate_peer_map(text):
    require(not CRYPTO_ARCHIVE.search(text),
            "consumer/plugin link contains a second static Crypto archive")
    require(not EXTERNAL_ARCHIVE.search(text),
            "consumer/plugin link contains an external Crypto/GmSSL archive")
    for symbol in NATIVE_SYMBOLS:
        require(re.search(r"(?<![A-Za-z_0-9])_?" + re.escape(symbol) +
                          r"(?![A-Za-z_0-9])", text) is None,
                "consumer/plugin defines or directly imports native TLS: " + symbol)


def dependencies(output, system):
    if system == "Windows":
        return re.findall(r"(?im)^\s+([^\s/\\]+[.]dll)\s*$", output)
    if system == "Darwin":
        return [line.strip().split(" ", 1)[0].replace("\\", "/").rsplit("/", 1)[-1]
                for line in output.splitlines()[1:] if line.strip()]
    return re.findall(r"\(NEEDED\)[^\n]*\[([^\]]+)\]", output)


def validate_dependencies(names):
    forbidden = [name for name in names if EXTERNAL_CRYPTO.match(name)]
    require(not forbidden, "independent Crypto/GmSSL runtime dependency: " + str(forbidden))


def inspect(args):
    files = [args.archive, args.runtime, args.map, *args.peer, *args.peer_map]
    require(all(path.is_file() for path in files), "required native artifact/link map missing")
    require(len(args.peer) == 3 and len(args.peer_map) == 3,
            "require both actual production plugins and their loader")
    if args.system == "Windows":
        reader = args.nm if "dumpbin" in Path(args.nm).name.lower() else shutil.which("dumpbin")
        require(reader, "dumpbin required for real COFF/native imports")
        symbols = command([reader, "/nologo", "/symbols", str(args.archive)])
    else:
        reader = args.nm or shutil.which("nm")
        require(reader, "native nm required")
        symbols = command([reader, "-g", str(args.archive)])
    table = archive_definitions(symbols, args.system)
    validate_archive(table)
    ownership = validate_runtime_map(args.map.read_text(errors="replace"), args.system)
    for path in args.peer_map:
        validate_peer_map(path.read_text(errors="replace"))
    imports = {}
    for path in [args.runtime, *args.peer]:
        if args.system == "Windows":
            output = command([reader, "/nologo", "/dependents", str(path)])
        elif args.system == "Darwin":
            output = command(["otool", "-L", str(path)])
        else:
            readelf = args.readelf or shutil.which("readelf")
            require(readelf, "native readelf required")
            output = command([readelf, "-d", str(path)])
        names = dependencies(output, args.system)
        validate_dependencies(names)
        imports[str(path)] = names
    # Both modules must directly load the real CNet shared image. The loader
    # intentionally has no CNet import and opens those modules at runtime.
    for path in args.peer[:2]:
        require(any(re.search(r"(?:lib)?saltscore_cnet(?:[-.]|$)", name, re.I)
                    for name in imports[str(path)]),
                "production plugin does not import the real CNet owner: " + str(path))
    return {"ok": True, "archive": str(args.archive), "runtime": str(args.runtime),
            "strong_archive_definitions": len(table), "native_owners": ownership,
            "imports": imports, "scope": "production-native-link-not-facade-parity"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--map", required=True, type=Path)
    parser.add_argument("--system", required=True)
    parser.add_argument("--nm", default="")
    parser.add_argument("--readelf", default="")
    parser.add_argument("--peer", action="append", type=Path, default=[])
    parser.add_argument("--peer-map", action="append", type=Path, default=[])
    try:
        print(json.dumps(inspect(parser.parse_args()), indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError) as error:
        print("CNet native Crypto owner: FAIL: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
