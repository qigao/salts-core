#!/usr/bin/env python3
"""Check installed pre-stable 0.1 package metadata against future minor ABI."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def probe(cmake: str, data: str, provider: str, consumer: str,
          accepted: bool, root: Path) -> None:
    directory = root / (provider.replace(".", "_") + "_" + consumer.replace(".", "_"))
    directory.mkdir()
    metadata = directory / "SaltsCoreConfigVersion.cmake"
    metadata.write_text(data.replace("0.1.1", provider), encoding="utf-8")
    major, minor, patch = consumer.split(".")
    check = directory / "probe.cmake"
    check.write_text(
        f'set(PACKAGE_FIND_VERSION "{consumer}")\n'
        f'set(PACKAGE_FIND_VERSION_MAJOR "{major}")\n'
        f'set(PACKAGE_FIND_VERSION_MINOR "{minor}")\n'
        f'set(PACKAGE_FIND_VERSION_PATCH "{patch}")\n'
        'set(PACKAGE_FIND_VERSION_COUNT 3)\n'
        f'include("{metadata.as_posix()}")\n'
        'if(PACKAGE_VERSION_COMPATIBLE)\n'
        '  set(found TRUE)\n'
        'else()\n'
        '  set(found FALSE)\n'
        'endif()\n'
        f'if(NOT found STREQUAL "{str(accepted).upper()}")\n'
        f'  message(FATAL_ERROR "Incorrect ABI version admission: provider={provider} consumer={consumer}")\n'
        'endif()\n',
        encoding="utf-8",
    )
    result = subprocess.run([cmake, "-P", str(check)], capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(result.stdout + result.stderr)
    print(f"PASS: {provider} {'accepts' if accepted else 'rejects'} {consumer}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version-file", type=Path, required=True)
    parser.add_argument("--cmake", default="cmake")
    args = parser.parse_args()
    metadata = args.version_file.read_text(encoding="utf-8")
    if 'set(PACKAGE_VERSION "0.1.1")' not in metadata or metadata.count("0.1.1") < 2:
        raise ValueError("Installed 0.1.1 package version metadata is absent or malformed")
    with tempfile.TemporaryDirectory() as tmp:
        for provider, consumer, accepted in (
            ("0.1.1", "0.1.0", True),
            ("0.1.9", "0.1.1", True),
            ("0.2.0", "0.1.1", False),
            ("0.1.1", "0.2.0", False),
        ):
            probe(args.cmake, metadata, provider, consumer, accepted, Path(tmp))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
