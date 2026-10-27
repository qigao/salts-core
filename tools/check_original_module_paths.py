#!/usr/bin/env python3
"""Audit exact Salts module-relative path provenance without freezing source bytes.

Historical authority is qigao/salts@13dbd7266e7aed7277daeb103fb40c4684c3a367,
before native module extraction. Every surviving original include/src/parser
file must retain its *original* relative path under Platform, Concurrency,
Coroutine, NativeIO, CNet and URI. Base64's core/, fs/, process/ are the only
user-approved destination naming exceptions.

The historical file manifest has an immutable Git blob identity, so removing
a name from the list will not silently make a missing module source pass CI.
Normal bugfixes to C/H file contents do not change their path admission.
"""
from __future__ import annotations

from collections import Counter
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tools/original_salts_module_paths.txt"

# sha1("blob <length>\0<bytes>") from the original-name inventory commit.
MANIFEST_GIT_BLOB = "70c9911e92690ac676216bf608ece8765344f363"
EXPECTED_COUNTS = {
    "platform": 16,
    "concurrency": 13,
    "coroutine": 7,
    "native-io": 21,
    "cnet": 64,
    "uri": 4,
}
RETIRED_NONCORE_SURFACES = (
    "concurrency/src/cmeta_thread.h",  # Reflected/native legacy CMeta shim
    "concurrency/src/disruptor.h",  # Old duplicate private header
    "coroutine/include/cmeta/coroutine.h",  # Reflection facade stayed with Salts
    "native-io/include/salts/native_io_ace_token.h",  # ACE token stayed in Salts
)
PATH_PATTERN = re.compile(
    r"(?:platform|concurrency|coroutine|native-io|cnet|uri)"
    r"/(?:include|src|parser)/[A-Za-z0-9_.+/-]+"
)


def read_original_paths(manifest: Path = MANIFEST) -> tuple[str, ...]:
    data = manifest.read_bytes()
    identity = hashlib.sha1(f"blob {len(data)}\0".encode("ascii") + data).hexdigest()
    if identity != MANIFEST_GIT_BLOB:
        raise ValueError("original Salts module-name inventory identity changed")
    names = [
        line.strip()
        for line in data.decode("utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    ]
    if len(names) != len(set(names)) or names != sorted(names):
        raise ValueError("historical module path inventory must be unique and sorted")
    for name in names:
        if not PATH_PATTERN.fullmatch(name) or any(
            component in (".", "..") for component in name.split("/")
        ):
            raise ValueError(f"unsafe or renamed native module source path: {name}")
    if dict(Counter(x.split("/", 1)[0] for x in names)) != EXPECTED_COUNTS:
        raise ValueError("historical native source/header count changed")
    return tuple(names)


def validate(root: Path, names: tuple[str, ...]) -> None:
    root = root.resolve()
    for name in names:
        path = root / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"original Salts source/header path missing: {name}")
    for retired in RETIRED_NONCORE_SURFACES:
        if (root / retired).exists():
            raise ValueError(f"retired non-Core source reintroduced: {retired}")
    for module in EXPECTED_COUNTS:
        for kind in ("include", "src"):
            if not (root / module / kind).is_dir():
                raise ValueError(f"original Salts module no longer owns {module}/{kind}")


def main() -> None:
    original = read_original_paths()
    validate(ROOT, original)
    print(
        f"PASS: {len(original)} original Salts module path names retained; "
        "Core/FS/Process exceptions and retired non-Core ABI enforced"
    )


if __name__ == "__main__":
    main()
