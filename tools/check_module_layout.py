#!/usr/bin/env python3
"""SaltsCore module-root admission. No root include/src, old paths or aliases.

Source ownership is keyed by existing modules. Three explicitly retained
module names are core/, fs/, and process/. Do not change the other module
names or flatten their source/include layout in later migrations.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MODULES = (
    "core", "platform", "concurrency", "coroutine", "native-io",
    "cnet", "uri", "fs", "process",
)
OWNER_FILES = (
    "core/include/base64_utils.h",
    "core/src/base64_utils.c",
    "core/include/cmeta_api.h",
    "core/include/cmeta_buffer.h",
    "core/src/cmeta_buffer.c",
    "core/include/vstr.h",
    "core/src/bytes.c",
    "fs/include/salts/fs.h",
    "fs/src/fs.c",
    "fs/src/fs_root.c",
    "process/include/salts/process.h",
    "process/src/process.c",
    "platform/include/salts/random.h",
    "platform/src/random.c",
    "concurrency/src/thread_pool.c",
    "coroutine/src/coro.c",
    "native-io/src/native_io.c",
    "cnet/src/cnet_client.c",
    "uri/include/uri_parser.h",
    "uri/src/uri_parser.c",
    "uri/parser/uri_lexer.re",
)
for old in ("include", "src", "utils", "cflow-fs", "cflow-process"):
    if (ROOT / old).exists():
        raise SystemExit(f"legacy root module directory still present: {old}")

for module in MODULES:
    if not (ROOT / module / "src").is_dir():
        raise SystemExit(f"missing module-local implementation: {module}/src")
    if not (ROOT / module / "include").is_dir():
        raise SystemExit(f"missing module-local public surface: {module}/include")

for rel in OWNER_FILES:
    if not (ROOT / rel).is_file():
        raise SystemExit(f"missing original module-owned file: {rel}")

source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
extra = {
    "core/src/base64_utils.c": "Salts::Core",
    "fs/src/fs.c": "Salts::FS",
    "process/src/process.c": "Salts::Process",
    "platform/src/clock.c": "Salts::Platform",
    "concurrency/src/thread_pool.c": "Salts::Concurrency",
    "coroutine/src/coro.c": "Salts::Coroutine",
    "native-io/src/native_io.c": "Salts::NativeIO",
    "uri/parser/uri_lexer.re": "Salts::URI",
}
for rel in extra:
    target = source if rel not in ("fs/src/fs.c", "process/src/process.c") else (
        ROOT / "cmake" / ("SaltsCoreFS.cmake" if rel.startswith("fs/") else "SaltsCoreProcess.cmake")
    ).read_text(encoding="utf-8")
    if rel not in target:
        raise SystemExit(f"missing real {extra[rel]} source input {rel}")

for file in (ROOT / "tests").rglob("*"):
    if file.suffix not in (".c", ".h", ".cpp", ".cmake") and file.name != "CMakeLists.txt":
        continue
    text = file.read_text(encoding="utf-8")
    if re.search(r"(?m)^\s*#\s*include\s*[\"<](?:\.\./)+src/", text):
        raise SystemExit(f"test imports removed root source via relative include: {file}")
    if re.search(r"\$\{PROJECT_SOURCE_DIR\}/(?:src|include)/", text):
        raise SystemExit(f"test links removed root source directory: {file}")
# A workflow that still watches the retired root silently misses source edits.
# Direct cross-compiler commands using old root headers/sources are also invalid.
# Scan executable workflows, not only CMake and CTest source lists.
WORKFLOWS = ROOT / ".github" / "workflows"
OLD_SOURCE = re.compile(
    r"(?<![A-Za-z0-9_./-])src/(?:core|platform|concurrency|coroutine|"
    r"native-io|cnet|uri|fs|process)/"
)
OLD_INCLUDE = re.compile(
    r"(?<![A-Za-z0-9_./-])include/(?:salts/|salts_core/|cnet/|"
    r"coro|vstr|cmeta|ring_buffer|base64_utils|uri_parser)"
)
OLD_COMPILER_INCLUDE = re.compile(r"(?<!\S)-Iinclude(?=[/'\" \t]|$)")
for workflow in WORKFLOWS.glob("*.yml"):
    for line_number, line in enumerate(
        workflow.read_text(encoding="utf-8").splitlines(), 1
    ):
        if (OLD_SOURCE.search(line) or OLD_INCLUDE.search(line)
                or OLD_COMPILER_INCLUDE.search(line)):
            raise SystemExit(
                f"obsolete root path in workflow "
                f"{workflow.relative_to(ROOT)}:{line_number}: {line.strip()}"
            )

print("PASS: one source/header owner per existing SaltsCore module; no root include/src")
