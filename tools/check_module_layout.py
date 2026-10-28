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
    "cnet", "uri", "fs", "process", "sctp",
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
    if not (ROOT / module / "CMakeLists.txt").is_file():
        raise SystemExit(f"missing module-local CMake owner: {module}/CMakeLists.txt")
    if not (ROOT / module / "src").is_dir():
        raise SystemExit(f"missing module-local implementation: {module}/src")
    if not (ROOT / module / "include").is_dir():
        raise SystemExit(f"missing module-local public surface: {module}/include")

for rel in OWNER_FILES:
    if not (ROOT / rel).is_file():
        raise SystemExit(f"missing original module-owned file: {rel}")

source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

# ASN1 and Crypto are separately packaged qigao/crypto source modules.
# They retain module-local CMakeLists.txt and exact first-party target
# ownership, but must NOT be rebuilt in SaltsCore's root along with a
# separately imported static Crypto owner: that duplicates TLS state.
# CNet will import Salts::Crypto only after full provider parity; no fallback.
EXTERNAL_SOURCE_MODULES = {
    "asn1": ("Salts::ASN1", "salts_asn1"),
    "crypto": ("Salts::Crypto", "salts_crypto"),
}

# Discover every *current or future* root-level source module. An explicit
# MODULES tuple alone is insufficient: it would silently miss a new vendor/,
# usb/, capture/ etc. A source-owning directory must have its own CMake
# definition AND an actual add_subdirectory() route from the root.
CODE_SUFFIXES = frozenset({".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".m", ".mm", ".S"})
root_code_modules = {
    child.name for child in ROOT.iterdir()
    if child.is_dir() and not child.name.startswith(".")
    and any(path.is_file() and path.suffix in CODE_SUFFIXES
            for path in child.rglob("*"))
}
expected_module_dirs = set(MODULES) | {
    "cserde", "idna", "tinytest", "unicode", "tests", "vendor",
}
missing_sources = expected_module_dirs - root_code_modules
if missing_sources:
    raise SystemExit("missing required root source module(s): "
                     + ", ".join(sorted(missing_sources)))
for module in sorted(root_code_modules):
    owner_cmake = ROOT / module / "CMakeLists.txt"
    if not owner_cmake.is_file():
        raise SystemExit(f"missing root source module CMakeLists.txt: {module}")
    if module in EXTERNAL_SOURCE_MODULES:
        alias, native_target = EXTERNAL_SOURCE_MODULES[module]
        code = owner_cmake.read_text(encoding="utf-8")
        if f"add_library({alias} ALIAS {native_target})" not in code:
            raise SystemExit(f"missing independent first-party module target: {module}/{alias}")
        # These files originate from qigao/crypto and have their own package
        # exports. Building the source copy as well as importing the archive
        # would violate the single *physical* TLS owner invariant.
        if f"add_subdirectory({module})" in source:
            raise SystemExit(f"duplicate external Crypto/ASN1 source owner: {module}")
        continue
    if f"add_subdirectory({module})" not in source:
        raise SystemExit(f"root does not assemble source module: {module}")

for module in MODULES:
    # Only package assembly belongs to the root; no module binary is made
    # from source files in the package-wide build description.
    if f"add_subdirectory({module})" not in source:
        raise SystemExit(f"root does not assemble {module}/CMakeLists.txt")
if (ROOT / "cnet" / "sctp").exists():
    raise SystemExit("nested cnet/sctp source owner must not exist")
for retired in ("SaltsCoreCNet.cmake", "SaltsCoreExtraModules.cmake",
                "SaltsCoreFS.cmake", "SaltsCoreProcess.cmake",
                "SaltsCoreCNetCryptoStaticOwner.cmake"):
    if (ROOT / "cmake" / retired).exists():
        raise SystemExit(f"obsolete central CMake module owner: {retired}")
extra = {
    "core/src/base64_utils.c": "Salts::Core",
    "fs/src/fs.c": "Salts::FS",
    "process/src/process.c": "Salts::Process",
    "platform/src/clock.c": "Salts::Platform",
    "concurrency/src/thread_pool.c": "Salts::Concurrency",
    "coroutine/src/coro.c": "Salts::Coroutine",
    "native-io/src/native_io.c": "Salts::NativeIO",
    "uri/parser/uri_lexer.re": "Salts::URI",
    "sctp/src/sctp_wire.c": "Salts::SCTPWire",
}
for rel, alias in extra.items():
    module, local = rel.split("/", 1)
    module_cmake = (ROOT / module / "CMakeLists.txt").read_text(encoding="utf-8")
    if local not in module_cmake or alias not in module_cmake:
        raise SystemExit(f"missing real module-owned {alias} build input {rel}")

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

print(f"PASS: {len(root_code_modules)} root source modules have module-local CMake; "
      f"{len(root_code_modules & set(EXTERNAL_SOURCE_MODULES))} qigao/crypto source modules "
      "remain external package owners; every other source module assembled by root; "
      "SCTP root; no legacy root include/src")
