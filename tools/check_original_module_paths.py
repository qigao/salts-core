#!/usr/bin/env python3
"""Audit exact Salts module-relative path provenance without freezing source bytes.

Historical authority is qigao/salts@13dbd7266e7aed7277daeb103fb40c4684c3a367,
before native module extraction. Every surviving original include/src/parser
file remains accounted for under Platform, Concurrency, Coroutine, NativeIO,
CNet and URI. The immutable inventory keeps donor path names; explicitly
approved public-header flattening is translated separately below.

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
    "native-io/include/native_io_ace_token.h",  # ACE token stayed in Salts
)
CURRENT_PATH_RENAMES = {
    "concurrency/include/salts/atomic.h": "concurrency/include/atomic.h",
    "concurrency/include/salts/deadline_queue.h": "concurrency/include/deadline_queue.h",
    "concurrency/include/salts/disruptor.h": "concurrency/include/disruptor.h",
    "concurrency/include/salts/rcu.h": "concurrency/include/rcu.h",
    "concurrency/include/salts/spsc_ring.h": "concurrency/include/spsc_ring.h",
    "concurrency/include/salts/thread_pool.h": "concurrency/include/thread_pool.h",
    "native-io/include/salts/native_io.h": "native-io/include/native_io.h",
    "native-io/include/salts/native_io_sharded.h": "native-io/include/native_io_sharded.h",
    "native-io/include/salts/native_ipc.h": "native-io/include/native_ipc.h",
    "platform/include/salts/clock.h": "platform/include/clock.h",
    "platform/include/salts/error_codes.h": "platform/include/error_codes.h",
    "platform/include/salts/fastpath.h": "platform/include/fastpath.h",
    "platform/include/salts/random.h": "platform/include/random.h",
    "platform/include/salts/readiness.h": "platform/include/readiness.h",
    "platform/include/salts/thread.h": "platform/include/thread.h",
}
PATH_PATTERN = re.compile(
    r"(?:platform|concurrency|coroutine|native-io|cnet|uri)"
    r"/(?:include|src|parser)/[A-Za-z0-9_.+/-]+"
)


def read_original_paths(manifest: Path = MANIFEST) -> tuple[str, ...]:
    # Git's checkout may materialize text files with CRLF on Windows while
    # the tracked Git blob remains LF. Compare the original *tracked* bytes,
    # not a platform-specific working-tree newline representation.
    data = manifest.read_bytes().replace(b"\r\n", b"\n")
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
        path = root / CURRENT_PATH_RENAMES.get(name, name)
        if not path.is_file() or path.is_symlink():
            raise ValueError(
                "original Salts source/header path missing after approved rename: "
                + name
            )
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
        f"PASS: {len(original)} original Salts module files accounted for; "
        "approved header renames, Core/FS/Process exceptions, and retired "
        "non-Core ABI enforced"
    )


if __name__ == "__main__":
    main()
