#!/usr/bin/env python3
"""Collect and verify bounded native migration evidence, never self-acceptance.

The first policy covers SIMD's supplemental native execution obligations only.
It does not accept the 610 historical APIs or the seven original SIMD files'
remaining reflection/ownership obligations. Receipts belong outside the source
tree. Use collect on each native runner; verify the retained bundles in CI.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import stat
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = "tools/migration/native-evidence-contracts.json"
SUPPLEMENT = "tools/migration/original-simd-inventory.json"
SUPPLEMENT_BLOB = "7ea91f1654e32a8d6baf054ff67143c43d8d04c3"
NATIVE_TARGETS = {
    "saltscore_simd": ("STATIC_LIBRARY", ["simd/src/simd.c", "simd/src/simd_types.c"]),
    "saltscore_simd_original_c11": ("EXECUTABLE", ["simd/tests/simd_test.c"]),
    "saltscore_simd_original_cpp17": ("EXECUTABLE", ["simd/tests/simd_header_cpp_test.cpp"]),
    "saltscore_simd_native_boundaries": ("EXECUTABLE", ["simd/tests/simd_native_boundaries.c"]),
}
INSTALLED_TARGETS = {
    "simd_consumer_c": ("EXECUTABLE", ["tests/installed/simd_consumer.c"]),
    "simd_consumer_cpp": ("EXECUTABLE", ["tests/installed/simd_consumer.cpp"]),
}
TESTS = {
    "native": {
        "saltscore.simd_original_c11": "saltscore_simd_original_c11",
        "saltscore.simd_original_cpp17": "saltscore_simd_original_cpp17",
        "saltscore.simd_native_boundaries": "saltscore_simd_native_boundaries",
    },
    "installed": {"sdk.simd_c11": "simd_consumer_c", "sdk.simd_cpp17": "simd_consumer_cpp"},
}
PROFILES = {
    "linux-release": ("Linux", {"x86_64", "amd64"}, False),
    "linux-asan-ubsan": ("Linux", {"x86_64", "amd64"}, True),
    "windows-release": ("Windows", {"x86_64", "amd64"}, False),
    "macos-release": ("Darwin", {"arm64", "aarch64"}, False),
}
PUBLIC_HEADERS = ("salts/simd.h", "salts/simd_types.h")


def canonical(data: bytes) -> bytes:
    """Normalize only the explicitly reviewed textual SIMD contract inputs."""
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return data
    return data if b"\0" in data else data.replace(b"\r\n", b"\n")


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run(command: list[str], *, cwd: Path | None = None, env: dict | None = None) -> str:
    result = subprocess.run(command, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise ValueError(f"command failed ({result.returncode}): {command}\n{result.stdout}")
    return result.stdout


def source_identity(root: Path) -> dict:
    """Bind tracked AND new candidate files, including an uncommitted merge.

    No Git objects or index are written. Only Python bytecode is excluded;
    ignored build outputs are already excluded by git ls-files. Therefore all
    build/receipt directories used by collect must be outside this repository.
    Bind exact file bytes, including text-shaped binary protocol fixtures.
    Native runners must disable checkout EOL conversion before checkout rather
    than erase content differences while collecting or verifying evidence.
    """
    conflict = run(["git", "diff", "--name-only", "--diff-filter=U"], cwd=root)
    if conflict.strip():
        raise ValueError("unresolved source merge")
    names = run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
                cwd=root).split("\0")
    index_modes = {}
    for entry in run(["git", "ls-files", "--stage", "-z"], cwd=root).split("\0"):
        if entry:
            metadata, name = entry.split("\t", 1)
            index_modes[name] = metadata.split()[0]
    entries = []
    for name in sorted(set(names) - {""}):
        if "__pycache__" in PurePosixPath(name).parts or name.endswith((".pyc", ".pyo")):
            continue
        path = root / name
        if path.is_symlink():
            mode, data = "120000", os.readlink(path).encode()
        elif path.is_file():
            mode = index_modes.get(name, "100644")
            if os.name != "nt":
                mode = "100755" if path.stat().st_mode & stat.S_IXUSR else "100644"
            data = path.read_bytes()
        else:
            entries.append([name, "missing", None])
            continue
        entries.append([name, mode, git_blob(data)])
    if not entries:
        raise ValueError("empty candidate source tree")
    return {"head_commit": run(["git", "rev-parse", "HEAD"], cwd=root).strip(),
            "worktree_sha256": digest(json.dumps(entries, separators=(",", ":")).encode()),
            "file_count": len(entries)}


def load_contract(root: Path) -> dict:
    policy = json.loads((root / CONTRACT).read_text(encoding="utf-8"))
    if (set(policy) != {"schema", "module", "scope", "supplement_git_blob",
                       "required_profiles", "source_blobs"} or
            policy["schema"] != 1 or policy["module"] != "simd" or
            policy["scope"] != "supplemental-native-obligations-only" or
            policy["supplement_git_blob"] != SUPPLEMENT_BLOB or
            set(policy["required_profiles"]) != set(PROFILES)):
        raise ValueError("unsupported or self-accepting migration contract")
    if git_blob(canonical((root / SUPPLEMENT).read_bytes())) != SUPPLEMENT_BLOB:
        raise ValueError("frozen SIMD supplement changed")
    # This is reviewed source/test identity, not a search for magic behavior strings.
    for relative, expected in policy["source_blobs"].items():
        path = PurePosixPath(relative)
        if path.is_absolute() or ".." in path.parts or not re.fullmatch(r"[0-9a-f]{40}", expected):
            raise ValueError("invalid source contract path/blob")
        if git_blob(canonical((root / relative).read_bytes())) != expected:
            raise ValueError("reviewed SIMD source/test blob changed: " + relative)
    required = {p for _, paths in (*NATIVE_TARGETS.values(), *INSTALLED_TARGETS.values())
                for p in paths}
    required.update({"simd/CMakeLists.txt", "simd/tests/CMakeLists.txt",
                     "simd/include/salts/simd.h", "simd/include/salts/simd_types.h",
                     "tests/test_original_simd_provenance.py"})
    if set(policy["source_blobs"]) != required:
        raise ValueError("source/test contract coverage was reduced or widened")
    spec = importlib.util.spec_from_file_location(
        "simd_source_provenance", root / "tests/test_original_simd_provenance.py")
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    module.verify_module(root)
    return policy


def safe_file(bundle: Path, relative: str) -> Path:
    path = PurePosixPath(relative)
    if path.is_absolute() or ".." in path.parts or "\\" in relative:
        raise ValueError("evidence path escapes bundle")
    resolved = (bundle / relative).resolve()
    if not resolved.is_relative_to(bundle.resolve()) or not resolved.is_file():
        raise ValueError("missing evidence file: " + relative)
    return resolved


def file_record(bundle: Path, source: Path, relative: str) -> dict:
    destination = bundle / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    return {"path": relative, "sha256": digest(destination.read_bytes())}


def checked_file(bundle: Path, record: dict) -> Path:
    if set(record) != {"path", "sha256"}:
        raise ValueError("invalid evidence file record")
    path = safe_file(bundle, record["path"])
    if digest(path.read_bytes()) != record["sha256"]:
        raise ValueError("evidence artifact/report hash mismatch: " + record["path"])
    return path


def normal_path(path: str) -> str:
    return path.replace("\\", "/").rstrip("/")


def source_relative(path: str, source_root: str, prefix: str = "") -> str:
    path, base = normal_path(path), normal_path(source_root)
    if path.startswith(base + "/"):
        path = path[len(base) + 1:]
    if path.startswith("/") or re.match(r"^[A-Za-z]:", path) or ".." in PurePosixPath(path).parts:
        raise ValueError("compiled source outside declared owner")
    return prefix + path


def read_cache(build: Path) -> dict:
    result = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith(("//", "#")) and ":" in line and "=" in line:
            key, value = line.split("=", 1)
            result[key.split(":", 1)[0]] = value
    return result


def capture_build(build: Path, source: Path, config: str, names: dict, bundle: Path,
                  kind: str) -> dict:
    reply = build / ".cmake/api/v1/reply"
    indices = sorted(reply.glob("index-*.json"))
    if not indices:
        raise ValueError("CMake File API reply missing")
    index = json.loads(indices[-1].read_text())
    def object_data(object_kind):
        obj = next((x for x in index["objects"] if x["kind"] == object_kind), None)
        if obj is None:
            raise ValueError("CMake File API object missing: " + object_kind)
        return json.loads(safe_file(reply, obj["jsonFile"]).read_text())
    model = object_data("codemodel")
    if Path(model["paths"]["source"]).resolve() != source.resolve():
        raise ValueError("build points at another candidate source tree")
    configurations = [c for c in model["configurations"] if c["name"] == config]
    if len(configurations) != 1:
        raise ValueError("configured build type missing from CMake codemodel")
    available = {t["name"]: t for t in configurations[0]["targets"]}
    targets = {}
    for name in names:
        if name not in available:
            raise ValueError("real compiled target missing: " + name)
        target = json.loads(safe_file(reply, available[name]["jsonFile"]).read_text())
        artifacts = target.get("artifacts", [])
        if len(artifacts) != 1:
            raise ValueError("target must produce one real artifact: " + name)
        artifact = build / artifacts[0]["path"]
        targets[name] = {"codemodel": target, "artifact": file_record(
            bundle, artifact, f"artifacts/{kind}/{name}/{artifact.name}")}
    cache = read_cache(build)
    keys = ("CMAKE_BUILD_TYPE", "CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS",
            "BUILD_TESTING", "SALTSCORE_BUILD_SIMD", "SALTSCORE_BUILD_TINYTEST",
            "SALTSCORE_REQUIRE_SIMD", "CMAKE_PREFIX_PATH")
    return {"source_root": str(source), "build_root": str(build), "config": config,
            "cache": {key: cache.get(key, "") for key in keys}, "targets": targets,
            "toolchains": object_data("toolchains")}


def validate_junit(path: Path, expected: str) -> None:
    document = ET.parse(path).getroot()
    cases = list(document.iter("testcase"))
    if len(cases) != 1 or cases[0].get("name") != expected:
        raise ValueError("missing, extra or renamed executed CTest: " + expected)
    if cases[0].get("status") != "run":
        raise ValueError("CTest was not executed: " + expected)
    for element in document.iter():
        if element.tag in {"failure", "error", "skipped"}:
            raise ValueError("failed or skipped CTest: " + expected)
        for attribute in ("failures", "errors", "disabled", "skipped"):
            if int(element.get(attribute, "0")) != 0:
                raise ValueError("nonpassing CTest report: " + expected)


def compile_flags(target: dict) -> str:
    return " ".join(fragment["fragment"] for group in target.get("compileGroups", [])
                    for fragment in group.get("compileCommandFragments", []))


def verify_receipt(root: Path, receipt_path: Path, identity: dict | None = None) -> str:
    policy = load_contract(root)
    bundle = receipt_path.parent
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    expected_keys = {"schema", "module", "profile", "source", "contract_blob", "host",
                     "created_utc", "builds", "tests", "sdk"}
    if set(receipt) != expected_keys or receipt["schema"] != 1 or receipt["module"] != "simd":
        raise ValueError("unsupported receipt; accepted flags are not evidence")
    if receipt["source"] != (identity or source_identity(root)):
        raise ValueError("evidence belongs to a different candidate source tree")
    if receipt["contract_blob"] != git_blob(canonical((root / CONTRACT).read_bytes())):
        raise ValueError("evidence contract changed since execution")
    profile = receipt["profile"]
    if profile not in PROFILES:
        raise ValueError("unknown native evidence profile")
    system, machines, sanitized = PROFILES[profile]
    if (receipt["host"].get("system") != system or
            receipt["host"].get("machine", "").lower() not in machines):
        raise ValueError("wrong native platform for evidence profile")
    if set(receipt["builds"]) != set(TESTS) or set(receipt["tests"]) != set(TESTS):
        raise ValueError("native or installed evidence missing")
    for kind, expected_targets in (("native", NATIVE_TARGETS), ("installed", INSTALLED_TARGETS)):
        build = receipt["builds"][kind]
        if build["config"] not in (("Release", "RelWithDebInfo") if sanitized else ("Release",)):
            raise ValueError("unsupported evidence build configuration")
        if set(build["targets"]) != set(expected_targets):
            raise ValueError("real compiled target set differs")
        compilers = {t.get("language"): t.get("compiler", {})
                     for t in build["toolchains"].get("toolchains", [])}
        if any(not compilers.get(language, {}).get("path") or
               compilers.get(language, {}).get("id") not in
               {"GNU", "Clang", "AppleClang", "MSVC"}
               for language in ("C", "CXX")):
            raise ValueError("native C11/C++17 compiler identity missing")
        if kind == "native":
            for option in ("BUILD_TESTING", "SALTSCORE_BUILD_SIMD", "SALTSCORE_BUILD_TINYTEST"):
                if build["cache"].get(option) != "ON":
                    raise ValueError("required native feature disabled: " + option)
        elif build["cache"].get("SALTSCORE_REQUIRE_SIMD") != "ON":
            raise ValueError("installed SIMD consumer was optional")
        for name, (target_type, sources) in expected_targets.items():
            record = build["targets"][name]
            target = record["codemodel"]
            if target.get("name") != name or target.get("type") != target_type:
                raise ValueError("compiled target type/owner differs: " + name)
            actual = {source_relative(s["path"], build["source_root"],
                      "tests/installed/" if kind == "installed" else "")
                      for s in target.get("sources", []) if "compileGroupIndex" in s}
            if actual != set(sources):
                raise ValueError("compiled target sources differ: " + name)
            artifact = checked_file(bundle, record["artifact"])
            magic = artifact.read_bytes()[:8]
            if target_type == "STATIC_LIBRARY":
                if magic != b"!<arch>\n":
                    raise ValueError("SIMD artifact is not a static archive")
            elif not (magic.startswith((b"\x7fELF", b"MZ")) or
                      magic[:4] in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf",
                                    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")):
                raise ValueError("test artifact is not a native executable")
            flags = compile_flags(target)
            if sanitized and ("-fsanitize=address,undefined" not in flags or
                              "-fno-sanitize" in flags):
                raise ValueError("native/installed target lacks required instrumentation")
        expected_tests = TESTS[kind]
        if set(receipt["tests"][kind]) != set(expected_tests):
            raise ValueError("required executed test set differs")
        for name, target_name in expected_tests.items():
            record = receipt["tests"][kind][name]
            inventory = json.loads(checked_file(bundle, record["inventory"]).read_text())
            tests = inventory.get("tests", [])
            target = build["targets"][target_name]["codemodel"]
            executable = normal_path(str(Path(build["build_root"]) / target["artifacts"][0]["path"]))
            if (len(tests) != 1 or tests[0].get("name") != name or
                    [normal_path(x) for x in tests[0].get("command", [])] != [executable] or
                    any(p.get("name") == "DISABLED" and p.get("value")
                        for p in tests[0].get("properties", []))):
                raise ValueError("CTest does not execute the real expected target: " + name)
            validate_junit(checked_file(bundle, record["junit"]), name)
            checked_file(bundle, record["output"])
    native = receipt["builds"]["native"]
    owner = native["targets"]["saltscore_simd"]["codemodel"]
    for name in set(NATIVE_TARGETS) - {"saltscore_simd"}:
        dependencies = native["targets"][name]["codemodel"].get("dependencies", [])
        if owner["id"] not in {d["id"] for d in dependencies}:
            raise ValueError("native test does not link the one SIMD owner")
    compilers = native["toolchains"].get("toolchains", [])
    if any(t.get("language") == "C" and t.get("compiler", {}).get("id") == "GNU"
           for t in compilers) and "-fwrapv" not in compile_flags(owner):
        raise ValueError("GNU SIMD wrap contract missing from actual compilation")
    sdk = receipt["sdk"]
    archive = checked_file(bundle, sdk["archive"])
    if digest(archive.read_bytes()) != native["targets"]["saltscore_simd"]["artifact"]["sha256"]:
        raise ValueError("installed archive differs from the compiled SIMD owner")
    if set(sdk["headers"]) != set(PUBLIC_HEADERS):
        raise ValueError("installed public header set differs")
    for header, record in sdk["headers"].items():
        data = checked_file(bundle, record).read_bytes()
        if git_blob(canonical(data)) != policy["source_blobs"]["simd/include/" + header]:
            raise ValueError("installed public header differs from reviewed source")
    exports = checked_file(bundle, sdk["exports"]).read_text(encoding="utf-8")
    if "add_library(Salts::SIMD STATIC IMPORTED)" not in exports:
        raise ValueError("installed SDK does not export canonical static Salts::SIMD")
    for record in receipt["builds"]["installed"]["targets"].values():
        fragments = record["codemodel"].get("link", {}).get("commandFragments", [])
        line = normal_path(" ".join(f["fragment"] for f in fragments)).replace('"', "")
        if normal_path(sdk["original_archive"]) not in line:
            raise ValueError("installed consumer does not link this installed SIMD archive")
        if system != "Windows" and not re.search(r"(?:^|\s)-lm(?:\s|$)", line):
            raise ValueError("static SIMD math dependency lost at installed link")
    return profile


def audit_evidence(root: Path, evidence_dir: Path | None = None) -> dict:
    load_contract(root)
    profiles = set()
    if evidence_dir is not None:
        if not evidence_dir.is_dir():
            raise ValueError("evidence directory missing")
        identity = source_identity(root)
        receipts = sorted(evidence_dir.rglob("receipt.json"))
        if not receipts:
            raise ValueError("explicit evidence directory contains no receipts")
        for receipt in receipts:
            profile = verify_receipt(root, receipt, identity)
            if profile in profiles:
                raise ValueError("duplicate native evidence profile: " + profile)
            profiles.add(profile)
    complete = profiles == set(PROFILES)
    return {"qualified_file_ids": [], "qualified_api_ids": [],
            "simd_supplement": {"functions": 32,
                               "qualified_native_functions": 32 if complete else 0,
                               "verified_profiles": sorted(profiles),
                               "pending_profiles": sorted(set(PROFILES) - profiles),
                               "original_files_pending": 7,
                               "upper_reflection": "pending"}}


def collect(root: Path, native: Path, installed: Path, sdk: Path, bundle: Path,
            profile: str, config: str) -> None:
    policy = load_contract(root)
    del policy
    for path in (native, installed, sdk, bundle):
        if path.resolve().is_relative_to(root.resolve()):
            raise ValueError("build, SDK and evidence outputs must be outside the source tree")
    if bundle.exists():
        raise ValueError("receipt output already exists; do not overwrite prior execution")
    system, machines, sanitized = PROFILES[profile]
    if platform.system() != system or platform.machine().lower() not in machines:
        raise ValueError("collector must run on the declared native platform")
    identity = source_identity(root)
    bundle.mkdir(parents=True)
    env = os.environ.copy()
    if sanitized:
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    env["PATH"] = str(sdk / "bin") + os.pathsep + env.get("PATH", "")
    library_path = "DYLD_LIBRARY_PATH" if system == "Darwin" else "LD_LIBRARY_PATH"
    env[library_path] = str(sdk / "lib") + os.pathsep + env.get(library_path, "")
    receipt = {"schema": 1, "module": "simd", "profile": profile, "source": identity,
               "contract_blob": git_blob(canonical((root / CONTRACT).read_bytes())),
               "host": {"system": system, "machine": platform.machine()},
               "created_utc": datetime.now(timezone.utc).isoformat(),
               "builds": {}, "tests": {}, "sdk": {}}
    for kind, build, source, targets in (
            ("native", native, root, NATIVE_TARGETS),
            ("installed", installed, root / "tests/installed", INSTALLED_TARGETS)):
        if not (build / "CMakeCache.txt").is_file():
            raise ValueError("configure the native/installed build with explicit flags first")
        query = build / ".cmake/api/v1/query"
        query.mkdir(parents=True, exist_ok=True)
        for name in ("codemodel-v2", "toolchains-v1"):
            (query / name).touch()
        arguments = ["cmake", "-S", str(source), "-B", str(build)]
        if kind == "installed":
            arguments += ["-DSALTSCORE_REQUIRE_SIMD=ON", "-DCMAKE_PREFIX_PATH=" + str(sdk)]
        run(arguments, env=env)
        # Never build a selected subset and call the package graph qualified.
        run(["cmake", "--build", str(build), "--config", config, "--parallel", "2"], env=env)
        if kind == "native":
            run(["cmake", "--install", str(native), "--config", config, "--prefix", str(sdk)], env=env)
        receipt["builds"][kind] = capture_build(build, source, config, targets, bundle, kind)
        receipt["tests"][kind] = {}
        for name in TESTS[kind]:
            directory = bundle / "reports" / kind / name
            directory.mkdir(parents=True)
            command = ["ctest", "--test-dir", str(build), "-C", config,
                       "-R", "^" + re.escape(name) + "$"]
            inventory_path = directory / "inventory.json"
            inventory_path.write_text(run(command + ["--show-only=json-v1"], env=env))
            junit = directory / "junit.xml"
            output = run(command + ["--no-tests=error", "--output-on-failure",
                                    "--timeout", "120", "--output-junit", str(junit)], env=env)
            output_path = directory / "output.log"
            output_path.write_text(output)
            receipt["tests"][kind][name] = {
                key: {"path": path.relative_to(bundle).as_posix(), "sha256": digest(path.read_bytes())}
                for key, path in (("inventory", inventory_path), ("junit", junit), ("output", output_path))}
    candidates = [p for p in sdk.rglob("*") if p.is_file() and
                  p.name.lower().startswith(("saltscore_simd", "libsaltscore_simd"))]
    expected = "saltscore_simd.lib" if system == "Windows" else "libsaltscore_simd.a"
    if len(candidates) != 1 or candidates[0].name != expected:
        raise ValueError("SDK contains missing/extra/static-or-dynamic SIMD artifacts")
    exports = list(sdk.rglob("SaltsCoreTargets.cmake"))
    if len(exports) != 1:
        raise ValueError("canonical SDK export file missing or ambiguous")
    receipt["sdk"] = {
        "archive": file_record(bundle, candidates[0], "sdk/" + expected),
        "original_archive": str(candidates[0]),
        "exports": file_record(bundle, exports[0], "sdk/SaltsCoreTargets.cmake"),
        "headers": {h: file_record(bundle, sdk / "include" / h, "sdk/include/" + h)
                    for h in PUBLIC_HEADERS}}
    if source_identity(root) != identity:
        raise ValueError("candidate source tree changed during evidence collection")
    receipt_path = bundle / "receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    try:
        verify_receipt(root, receipt_path, identity)
    except Exception:
        receipt_path.unlink()
        raise
    print(f"PASS: collected {profile} bounded native SIMD evidence: {receipt_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    commands = parser.add_subparsers(dest="command", required=True)
    verifier = commands.add_parser("verify")
    verifier.add_argument("--evidence-dir", type=Path)
    collector = commands.add_parser("collect")
    for name in ("native-build", "installed-build", "sdk-prefix", "output"):
        collector.add_argument("--" + name, type=Path, required=True)
    collector.add_argument("--profile", choices=tuple(PROFILES), required=True)
    collector.add_argument("--config", default="Release")
    args = parser.parse_args()
    try:
        root = args.root.resolve()
        if args.command == "collect":
            collect(root, args.native_build.resolve(), args.installed_build.resolve(),
                    args.sdk_prefix.resolve(), args.output.resolve(), args.profile, args.config)
        else:
            print(json.dumps(audit_evidence(root, args.evidence_dir), indent=2))
    except (ValueError, OSError, KeyError, TypeError, StopIteration, ET.ParseError) as exc:
        print("FAIL: migration evidence:", exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
