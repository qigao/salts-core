#!/usr/bin/env python3
"""Stage and inspect SaltsCore SDK candidates; fail closed before publication.

Archive integrity and exported-target inventory are structural checks. They do
not replace the original migration gate or executed installed consumers.
"""
from __future__ import annotations

import argparse
from contextlib import nullcontext
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

REPOSITORY = "qigao/salts-core"
MANIFEST = "saltscore-sdk-manifest.json"
MANIFEST_SCHEMA = 2
RIDS = frozenset({"linux-x64", "linux-arm64", "windows-x64", "macos-arm64",
                  "android-arm64-v8a", "ios-arm64"})
CROSS_RIDS = frozenset({"android-arm64-v8a", "ios-arm64"})
# This independent floor intentionally includes currently missing modules.
# Removing one from the editable profile cannot make a partial SDK releasable.
RELEASE_TARGET_FLOOR = frozenset("Salts::" + name for name in (
    "Core", "Platform", "Concurrency", "Coroutine", "NativeIO", "FS",
    "Process", "URI", "Unicode", "IDNA", "CSerde", "TinyTest", "SIMD",
    "ASN1", "Crypto", "CNet", "SCTP", "Serial", "USB", "Capture", "Playback"))
FOUNDATION = frozenset("Salts::" + name for name in (
    "Core", "Platform", "Concurrency", "Coroutine", "NativeIO", "FS"))
# This implementation checks archive structure and runs controlled candidate
# builds. It does not yet verify retained, source-bound CI qualification for
# every payload, a clean machine's complete runtime-library closure, or a real
# NuGet pack/restore round trip preserving Unix link identity. These
# are implementation gaps, so an editable release_deferred list or a manifest
# saying execution=native-ctest must never turn publication on. Replace these
# blockers with actual admission checks when that work is implemented/reviewed.
RELEASE_IMPLEMENTATION_BLOCKERS = (
    "SDK publication admission is incomplete: trusted CI build/test receipts are not bound to every RID payload",
    "SDK publication admission is incomplete: clean-runtime dependency closure is not verified",
    "SDK publication admission is incomplete: NuGet pack/restore has not qualified typed Unix symlink identity",
)
VERSION_FIELDS = ("display_version", "tag", "package_id", "package_version",
                  "cmake_version", "abi_epoch")
SHA = re.compile(r"^[0-9a-f]{40}$")
DEFAULT_PROFILE = Path(__file__).resolve().parents[2] / "packaging/release-profile.json"


class SDKError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise SDKError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique_object)


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def profile(path=DEFAULT_PROFILE):
    data = load_json(path)
    require(data.get("schema") == 1, "unsupported release profile schema")
    display = data.get("display_version", "")
    require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", display) is not None,
            "display version must have three numeric fields")
    normalized = ".".join(str(int(part)) for part in display.split("."))
    require(data.get("tag") == "v" + display, "tag/display version mismatch")
    require(data.get("package_id") == "SaltsCore.Native", "wrong Core package ID")
    require(data.get("package_version") == normalized == data.get("cmake_version"),
            "NuGet and CMake versions must use the normalized display version")
    require(data.get("abi_epoch") == ".".join(normalized.split(".")[:2]), "wrong ABI epoch")
    require(isinstance(data.get("rids"), list) and len(data["rids"]) == len(RIDS)
            and set(data["rids"]) == RIDS, "release profile must preserve exactly six RIDs")
    targets = data.get("release_required_targets", [])
    require(isinstance(targets, list) and len(set(targets)) == len(targets)
            and RELEASE_TARGET_FLOOR.issubset(targets),
            "release profile dropped a required native target")
    require(all(re.fullmatch(r"Salts::[A-Za-z][A-Za-z0-9_]*", t) for t in targets),
            "invalid required target")
    deferred = data.get("release_deferred")
    require(isinstance(deferred, list) and all(isinstance(s, str) and s for s in deferred),
            "release_deferred must explicitly list remaining work")
    return data


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], text=True, capture_output=True)
    require(result.returncode == 0, "git identity check failed: " + result.stderr.strip())
    return result.stdout.strip()


def identity(root):
    return {"repository": REPOSITORY, "commit": git(root, "rev-parse", "HEAD"),
            "tree": git(root, "rev-parse", "HEAD^{tree}"),
            "clean": not bool(git(root, "status", "--porcelain", "--untracked-files=normal"))}


def check_identity(value):
    require(isinstance(value, dict) and value.get("repository") == REPOSITORY
            and SHA.fullmatch(value.get("commit", ""))
            and SHA.fullmatch(value.get("tree", ""))
            and isinstance(value.get("clean"), bool), "invalid source identity")


def safe_path(value):
    require(isinstance(value, str) and value and "\\" not in value and ":" not in value
            and not any(ord(c) < 32 for c in value), f"unsafe SDK path: {value!r}")
    path = PurePosixPath(value)
    require(not path.is_absolute() and all(p not in ("", ".", "..") for p in value.split("/")),
            f"unsafe SDK path: {value!r}")
    # Use Windows-portable entry names, including archives built on Unix.
    for part in path.parts:
        require(not part.endswith((" ", ".")) and not any(c in part for c in '<>"|?*'),
                f"nonportable SDK path: {value!r}")
        require(not re.fullmatch(r"(?i)(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?", part),
                f"reserved Windows SDK path: {value!r}")
    return path


def validate_links(paths, links):
    """Resolve a file-only link graph within one SDK root, without filesystem I/O.

    Every intermediate directory must exist in the payload tree. In particular,
    lexical '..' normalization must not hide a missing directory or traverse a
    symlink-as-parent. Directory links are outside this SDK format.
    """
    paths = set(paths)
    folded = {name.casefold() for name in paths}
    require(len(folded) == len(paths), "case-insensitive duplicate SDK path")
    directories = {""}
    for name in paths:
        safe_path(name)
        parents = [p.as_posix() for p in PurePosixPath(name).parents if str(p) != "."]
        require(not any(p.casefold() in folded for p in parents),
                "SDK file or symlink is also a parent directory: " + name)
        directories.update(parents)
    destinations = {}
    for name, target in links.items():
        require(name in paths and name != MANIFEST, "SDK manifest must be a regular file")
        require(isinstance(target, str) and target and not target.startswith("/")
                and "\\" not in target and ":" not in target
                and not any(ord(c) < 32 for c in target),
                "unsafe or absolute SDK symlink target: " + repr(target))
        try:
            target.encode("utf-8")
        except UnicodeEncodeError as error:
            raise SDKError("SDK symlink target is not UTF-8: " + name) from error
        parts = list(PurePosixPath(name).parent.parts)
        raw = target.split("/")
        for index, part in enumerate(raw):
            if part == "..":
                require(parts, "SDK symlink escapes its SDK root: " + name)
                parts.pop()
            else:
                safe_path(part)
                parts.append(part)
            if index < len(raw) - 1:
                require("/".join(parts) in directories,
                        "SDK symlink traverses a missing or non-directory parent: " + name)
        destination = "/".join(parts)
        require(destination in paths, "dangling or directory SDK symlink: " + name)
        destinations[name] = destination
    resolved = {}
    for start in links:
        chain, seen = [], set()
        current = start
        while current in destinations and current not in resolved:
            require(current not in seen, "cyclic SDK symlink: " + start)
            seen.add(current)
            chain.append(current)
            current = destinations[current]
        endpoint = resolved.get(current, current)
        require(endpoint in paths and endpoint not in links,
                "SDK symlink does not resolve to a regular file: " + start)
        for name in chain:
            resolved[name] = endpoint
    return resolved


def filesystem_links(files):
    return {name: os.readlink(path) for name, path in files.items() if path.is_symlink()}


def entry_bytes(path):
    # Hash/archive the link text itself, never the dereferenced library bytes.
    return os.readlink(path).encode("utf-8") if path.is_symlink() else path.read_bytes()


def files_under(prefix):
    prefix = Path(prefix).resolve()
    require(prefix.is_dir(), "SDK prefix is missing")
    seen = set()
    files = {}
    for current, directories, names in os.walk(prefix, followlinks=False):
        for name in directories:
            require(not (Path(current) / name).is_symlink(), "directory symlinks cannot enter SDK archives")
        for name in names:
            path = Path(current) / name
            relative = path.relative_to(prefix).as_posix()
            safe_path(relative)
            require(relative.casefold() not in seen, f"case-insensitive duplicate SDK path: {relative}")
            seen.add(relative.casefold())
            mode = path.lstat().st_mode
            require(stat.S_ISREG(mode) or stat.S_ISLNK(mode),
                    f"SDK entry is not a regular file or symlink: {relative}")
            files[relative] = path
    validate_links(files, filesystem_links(files))
    return files


def exported_targets(files, read):
    targets = {}
    imported_binaries = set()
    binary_owners = set()
    exports = [name for name in files if re.search(r"(?:^|/)lib/cmake/.+Targets[^/]*\.cmake$", name)]
    require(exports, "SDK has no installed CMake exports")
    for name in exports:
        content = read(name).decode("utf-8")
        active = "\n".join(line for line in content.splitlines() if not line.lstrip().startswith("#"))
        for target, kind in re.findall(r"\badd_library\s*\(\s*(Salts::[\w]+)\s+(STATIC|SHARED|INTERFACE|MODULE|UNKNOWN)\s+IMPORTED\s*\)", active):
            require(target not in targets, f"duplicate installed target owner: {target}")
            targets[target] = kind
        for owner, properties in re.findall(r'\bset_target_properties\(\s*(Salts::[\w]+)\s+PROPERTIES\s+(.*?)\n\s*\)', active, re.S):
            for location in re.findall(r'IMPORTED_(?:LOCATION|IMPLIB)(?:_[A-Z0-9_]+)?\s+"([^"\n]+)"', properties):
                require(location.startswith("${_IMPORT_PREFIX}/"), f"non-relocatable imported location in {name}")
                relative = location[len("${_IMPORT_PREFIX}/"):]
                safe_path(relative)
                require(relative in files, f"imported binary is absent: {relative}")
                imported_binaries.add(relative)
                binary_owners.add(owner)
    require(FOUNDATION.issubset(targets), "SDK is missing mandatory shared foundation targets")
    require(all(targets[t] == "SHARED" for t in FOUNDATION), "native foundation must have one shared owner")
    if "Salts::SIMD" in targets:
        require(targets["Salts::SIMD"] == "STATIC", "SIMD must be the static native module")
    require(not any(t in targets for t in ("Salts::CMeta", "Salts::CFlow", "Salts::CSTL")),
            "Core SDK must not export an upper-layer runtime")
    require(imported_binaries, "SDK exports do not identify any real installed binary")
    require(all(t in binary_owners for t, kind in targets.items() if kind != "INTERFACE"),
            "an installed binary target has no imported binary location")
    return targets, sorted(imported_binaries)


def native_backend(build_dir, rid):
    if build_dir is None:
        return {"status": "unverified", "sources": [], "compile_database_sha256": None}
    database = Path(build_dir) / "compile_commands.json"
    require(database.is_file(), "compile_commands.json is required to inspect the selected native backend")
    commands = load_json(database)
    sources = sorted({Path(item["file"].replace("\\", "/")).name for item in commands
                      if "native_io_" in item["file"]})
    if "native_io_unsupported.c" in sources:
        state = "unsupported"
    else:
        expected = ({"native_io_iocp.c"} if rid == "windows-x64" else
                    {"native_io_bsd.c", "native_io_kqueue.c"} if rid in {"macos-arm64", "ios-arm64"} else
                    {"native_io_linux.c", "native_io_epoll.c"})
        state = "selected" if expected.issubset(sources) else "unverified"
    return {"status": state, "sources": sources, "compile_database_sha256": digest(database.read_bytes())}


def release_blockers(manifest, config):
    problems = [*config["release_deferred"], *RELEASE_IMPLEMENTATION_BLOCKERS]
    missing = sorted(set(config["release_required_targets"]) - set(manifest["targets"]))
    if missing:
        problems.append("Missing native targets: " + ", ".join(missing))
    source = manifest.get("source")
    if source is None or not source.get("clean"):
        problems.append("Source identity is absent or the source checkout was dirty")
    if manifest.get("native_backend", {}).get("status") != "selected":
        problems.append("A supported native I/O backend has not been selected")
    expected_execution = ("cross-compiled-consumer" if manifest["rid"] in CROSS_RIDS else "native-ctest")
    if manifest.get("execution") != expected_execution:
        problems.append("Installed consumer qualification has not been recorded")
    if any(manifest["targets"].get(t) not in {"STATIC", "SHARED"}
           for t in config["release_required_targets"] if t in manifest["targets"]):
        problems.append("A required native target is not a real static or shared library")
    return problems


def validate_manifest(manifest, config, files, read, links):
    require(manifest.get("schema") == MANIFEST_SCHEMA, "unsupported SDK manifest schema")
    resolved = validate_links(files, links)
    for key in VERSION_FIELDS:
        require(manifest.get(key) == config[key], f"SDK manifest has wrong {key}")
    require(manifest.get("rid") in RIDS, "unknown SDK RID")
    if manifest.get("source") is not None:
        check_identity(manifest["source"])
    rows = manifest.get("files", [])
    require(isinstance(rows, list) and rows, "empty SDK file manifest")
    inventory = {}
    folded = set()
    for row in rows:
        path = row.get("path")
        safe_path(path)
        require(path != MANIFEST and path.casefold() not in folded,
                "duplicate/self-referential SDK manifest entry")
        folded.add(path.casefold())
        kind = row.get("kind")
        require(kind in {"file", "symlink"}, "SDK manifest must record the entry kind")
        expected_keys = {"path", "kind", "size", "sha256", "executable"}
        if kind == "symlink":
            expected_keys.add("target")
        require(set(row) == expected_keys, "SDK entry metadata differs from its kind: " + path)
        require((path in links) == (kind == "symlink"), "SDK entry kind changed: " + path)
        if kind == "symlink":
            require(row["target"] == links[path] and row.get("executable") is False,
                    "SDK symlink target metadata changed: " + path)
        require(isinstance(row.get("size"), int) and row["size"] >= 0
                and re.fullmatch(r"[0-9a-f]{64}", row.get("sha256", ""))
                and isinstance(row.get("executable"), bool), "invalid SDK digest record")
        inventory[path] = row
    require(set(inventory) == set(files) - {MANIFEST}, "SDK files differ from their exact manifest")
    for path, row in inventory.items():
        data = read(path)
        require(len(data) == row["size"] and digest(data) == row["sha256"], "SDK bytes changed: " + path)
    read_content = lambda name: read(resolved.get(name, name))
    targets, binaries = exported_targets(files, read_content)
    require(manifest.get("targets") == targets and manifest.get("imported_binaries") == binaries,
            "SDK target inventory differs from installed exports")
    version_file = "lib/cmake/SaltsCore/SaltsCoreConfigVersion.cmake"
    require(version_file in files, "installed Core version file is absent")
    require(re.search(r'\bset\(PACKAGE_VERSION\s+"' + re.escape(config["cmake_version"]) + r'"\)',
                      read_content(version_file).decode("utf-8")), "installed CMake version does not match package")
    return manifest


def verify_sdk(prefix, config):
    files = files_under(prefix)
    require(MANIFEST in files, "SDK manifest is absent")
    manifest = load_json(files[MANIFEST])
    return validate_manifest(manifest, config, files,
                             lambda name: entry_bytes(files[name]), filesystem_links(files))


def stage(args, config):
    require(args.rid in RIDS, "unsupported SDK RID")
    requested = Path(args.output)
    require(not requested.is_symlink(), "staging output must not be a symlink")
    destination = requested.resolve()
    require(not destination.exists(), "staging output already exists; choose a fresh directory")
    source_files = files_under(args.prefix)
    require(MANIFEST not in source_files, "input is already staged; verify its original manifest instead")
    source_identity = None
    if args.identity:
        require(args.source_root is not None, "source-root is required to verify build identity")
        source_identity = load_json(args.identity)
        check_identity(source_identity)
        require(identity(args.source_root) == source_identity, "source identity changed during the build")
    destination.mkdir(parents=True)
    # Files first, then links. Validated links can never be a parent directory.
    for relative, path in sorted(source_files.items(), key=lambda item: item[1].is_symlink()):
        target = destination.joinpath(*PurePosixPath(relative).parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        if path.is_symlink():
            target.symlink_to(os.readlink(path))
        else:
            shutil.copyfile(path, target)
            shutil.copymode(path, target)
    files = files_under(destination)
    targets, binaries = exported_targets(files, lambda name: files[name].read_bytes())
    rows = []
    for name, path in sorted(files.items()):
        data = entry_bytes(path)
        row = {"path": name, "kind": "symlink" if path.is_symlink() else "file",
               "size": len(data), "sha256": digest(data),
               "executable": not path.is_symlink() and bool(path.stat().st_mode & 0o111)}
        if path.is_symlink():
            row["target"] = os.readlink(path)
        rows.append(row)
    manifest = {"schema": MANIFEST_SCHEMA, **{key: config[key] for key in VERSION_FIELDS},
                "rid": args.rid, "profile": config["candidate_profile"], "source": source_identity,
                "native_backend": native_backend(args.build_dir, args.rid),
                "execution": args.execution, "targets": targets, "imported_binaries": binaries,
                "files": rows}
    write_json(destination / MANIFEST, manifest)
    verify_sdk(destination, config)
    return manifest


def archive_sdk(prefix, output, config):
    manifest = verify_sdk(prefix, config)
    files = files_under(prefix)
    require(not Path(output).exists(), "archive output already exists")
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, path in sorted(files.items()):
            entry = zipfile.ZipInfo("sdk/" + manifest["rid"] + "/" + name, (1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            mode = (stat.S_IFLNK | 0o777 if path.is_symlink() else
                    stat.S_IFREG | (0o755 if path.stat().st_mode & 0o111 else 0o644))
            entry.external_attr = mode << 16
            archive.writestr(entry, entry_bytes(path), compress_type=zipfile.ZIP_DEFLATED)
    return manifest


def archive_entries(archive):
    entries = {}
    folded = set()
    links = {}
    for item in archive.infolist():
        name = item.filename.rstrip("/")
        safe_path(name)
        require(name.casefold() not in folded, "duplicate archive path: " + name)
        folded.add(name.casefold())
        kind = stat.S_IFMT(item.external_attr >> 16)
        if item.is_dir():
            require(kind in {0, stat.S_IFDIR}, "archive directory has conflicting entry type: " + name)
            continue
        require(kind in {0, stat.S_IFREG, stat.S_IFLNK}, "unsupported archive entry type: " + name)
        entries[name] = item
        if kind == stat.S_IFLNK:
            require(item.create_system == 3 and not item.external_attr & 0x10,
                    "archive symlink has conflicting platform/directory attributes: " + name)
            parts = PurePosixPath(name).parts
            require(len(parts) >= 3 and parts[0] == "sdk" and parts[1] in RIDS,
                    "archive symlinks must be inside a canonical SDK root: " + name)
            try:
                target = archive.read(item).decode("utf-8")
            except UnicodeDecodeError as error:
                raise SDKError("archive symlink target is not UTF-8: " + name) from error
            links.setdefault(parts[1], {})["/".join(parts[2:])] = target
    file_paths = {name.casefold() for name in entries}
    for name in entries:
        require(not any(parent.as_posix().casefold() in file_paths
                        for parent in PurePosixPath(name).parents if str(parent) != "."),
                "archive file is also a parent directory: " + name)
    for rid, targets in links.items():
        prefix = f"sdk/{rid}/"
        validate_links({name[len(prefix):] for name in entries if name.startswith(prefix)}, targets)
    return entries


def inspect_archive(path, config, require_all=False, nuget=False):
    # Extraction validates and reads the same open archive, not a reopened path.
    manager = nullcontext(path) if isinstance(path, zipfile.ZipFile) else zipfile.ZipFile(path)
    with manager as archive:
        entries = archive_entries(archive)
        if not nuget:
            require(all(n.startswith("sdk/") for n in entries), "candidate archive has unexpected non-SDK content")
        if nuget:
            specs = [n for n in entries if "/" not in n and n.endswith(".nuspec")]
            require(len(specs) == 1, "NuGet archive requires one root nuspec")
            metadata_files = {specs[0], "README.md", "[Content_Types].xml", "_rels/.rels", ".signature.p7s"}
            require(all(n.startswith("sdk/") or n in metadata_files or
                        re.fullmatch(r"package/services/metadata/core-properties/[A-Za-z0-9-]+\.psmdcp", n)
                        for n in entries), "NuGet contains undeclared payload outside SDK roots")
            xml = ET.fromstring(archive.read(specs[0]))
            metadata = next((e for e in xml if e.tag.rsplit("}", 1)[-1] == "metadata"), None)
            require(metadata is not None, "NuGet metadata missing")
            values = {e.tag.rsplit("}", 1)[-1]: e.text for e in metadata}
            require(values.get("id") == config["package_id"] and values.get("version") == config["package_version"],
                    "NuGet identity must use the normalized Core version")
            require("dependencies" not in values, "native SDK package must not declare managed dependencies")
        manifests = {}
        for name in entries:
            if name.endswith("/" + MANIFEST):
                parts = PurePosixPath(name).parts
                require(len(parts) == 3 and parts[0] == "sdk" and parts[1] in RIDS,
                        "manifest is outside a canonical SDK root")
                rid = parts[1]
                prefix = f"sdk/{rid}/"
                files = {n[len(prefix):] for n in entries if n.startswith(prefix)}
                manifest = json.loads(archive.read(name), object_pairs_hook=unique_object)
                require(manifest.get("rid") == rid, "manifest RID differs from archive path")
                links = {n: archive.read(prefix+n).decode("utf-8") for n in files
                         if stat.S_ISLNK(entries[prefix+n].external_attr >> 16)}
                manifests[rid] = validate_manifest(manifest, config, files,
                                                   lambda n, p=prefix: archive.read(p+n), links)
        require(manifests, "archive has no actual SDK manifests")
        require({PurePosixPath(n).parts[1] for n in entries if n.startswith("sdk/") and len(PurePosixPath(n).parts)>1}
                == set(manifests), "SDK payload without its required manifest")
        if require_all:
            require(set(manifests) == RIDS, "package must contain all six RIDs; missing " + ", ".join(sorted(RIDS - set(manifests))))
        return manifests


def extract_archive(path, output, config, nuget=False):
    with zipfile.ZipFile(path) as archive:
        manifests = inspect_archive(archive, config, nuget=nuget)
        entries = archive_entries(archive)
        requested = Path(output)
        require(not requested.is_symlink(), "extraction output must not be a symlink")
        destination = requested.resolve()
        require(not destination.exists(), "extraction output already exists")
        destination.mkdir(parents=True)
        for name, entry in entries.items():
            if stat.S_ISLNK(entry.external_attr >> 16):
                continue
            target = destination.joinpath(*PurePosixPath(name).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as stream:
                stream.write(archive.read(entry))
            target.chmod(0o755 if (entry.external_attr >> 16) & 0o111 else 0o644)
        # Only already-validated manifest link text is used to create links.
        # Hosts that deny symlink creation fail; copying bytes is not equivalent.
        for rid, manifest in manifests.items():
            prefix = destination / "sdk" / rid
            for row in manifest["files"]:
                if row["kind"] == "symlink":
                    target = prefix.joinpath(*PurePosixPath(row["path"]).parts)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.symlink_to(row["target"])
            verify_sdk(prefix, config)


def release_check(args, config):
    require(not config["release_deferred"], "publication blocked by explicit deferred work: " + "; ".join(config["release_deferred"]))
    actual = identity(args.source_root)
    require(actual["clean"], "publication requires a clean exact source checkout")
    require(actual["commit"] == args.expected_sha, "release source SHA differs from qualified source")
    require(args.tag == config["tag"], "wrong display-version tag")
    require(git(args.source_root, "rev-parse", "--verify", f"refs/tags/{args.tag}^{{commit}}") == args.expected_sha,
            "release tag does not identify the qualified source")
    result = subprocess.run([sys.executable, str(Path(args.source_root)/"tools/check_full_source_migration.py"),
                             "--require-complete"], text=True, capture_output=True)
    require(result.returncode == 0, "final migration gate refused publication:\n" + result.stdout + result.stderr)
    packages = inspect_archive(args.archive, config, require_all=True, nuget=True)
    for rid, manifest in packages.items():
        require(manifest["source"] == actual, f"{rid}: SDK source/tree differs from release source")
        blockers = release_blockers(manifest, config)
        require(not blockers, f"{rid}: " + "; ".join(blockers))
    return {"status": "ELIGIBLE_FOR_EXPLICIT_TAG_PUBLICATION", "published": False,
            "source": actual, "tag": args.tag, "package_sha256": digest(Path(args.archive).read_bytes())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    commands = parser.add_subparsers(dest="command", required=True)
    item = commands.add_parser("identity")
    item.add_argument("--source-root", type=Path, required=True)
    item.add_argument("--output", type=Path, required=True)
    item = commands.add_parser("stage")
    item.add_argument("--prefix", type=Path, required=True)
    item.add_argument("--output", type=Path, required=True)
    item.add_argument("--rid", required=True)
    item.add_argument("--identity", type=Path)
    item.add_argument("--source-root", type=Path)
    item.add_argument("--build-dir", type=Path)
    item.add_argument("--execution", choices=("unverified", "native-ctest", "cross-compiled-consumer"), default="unverified")
    item = commands.add_parser("check-native-backend")
    item.add_argument("--build-dir", type=Path, required=True)
    item.add_argument("--rid", choices=sorted(RIDS), required=True)
    item = commands.add_parser("verify-sdk")
    item.add_argument("--prefix", type=Path, required=True)
    item = commands.add_parser("archive")
    item.add_argument("--prefix", type=Path, required=True)
    item.add_argument("--output", type=Path, required=True)
    for name in ("verify-archive", "extract"):
        item = commands.add_parser(name)
        item.add_argument("--archive", type=Path, required=True)
        if name == "extract":
            item.add_argument("--output", type=Path, required=True)
            item.add_argument("--nuget", action="store_true")
        else:
            item.add_argument("--require-six-rids", action="store_true")
            item.add_argument("--nuget", action="store_true")
            item.add_argument("--source-root", type=Path,
                              help="require every SDK to match this clean exact source checkout")
    item = commands.add_parser("release-check")
    item.add_argument("--archive", type=Path, required=True)
    item.add_argument("--source-root", type=Path, required=True)
    item.add_argument("--expected-sha", required=True)
    item.add_argument("--tag", required=True)
    args = parser.parse_args()
    try:
        config = profile(args.profile)
        if args.command == "identity":
            result = identity(args.source_root)
            write_json(args.output, result)
        elif args.command == "check-native-backend":
            result = native_backend(args.build_dir, args.rid)
            require(result["status"] == "selected", f"{args.rid}: native backend is {result['status']}; unsupported is not a qualified SDK")
        elif args.command == "stage":
            manifest = stage(args, config)
            result = {"status": "CANDIDATE_STRUCTURALLY_VALID", "published": False,
                      "rid": manifest["rid"], "release_blockers": release_blockers(manifest, config)}
        elif args.command in ("archive", "verify-sdk"):
            manifest = (archive_sdk(args.prefix, args.output, config) if args.command == "archive"
                        else verify_sdk(args.prefix, config))
            result = {"status": "CANDIDATE_STRUCTURALLY_VALID", "published": False,
                      "rid": manifest["rid"], "release_blockers": release_blockers(manifest, config)}
        elif args.command == "extract":
            extract_archive(args.archive, args.output, config, args.nuget)
            result = {"status": "VERIFIED_ARCHIVE_EXTRACTED", "published": False}
        elif args.command == "verify-archive":
            manifests = inspect_archive(args.archive, config, args.require_six_rids, args.nuget)
            if args.source_root:
                source = identity(args.source_root)
                require(source["clean"], "archive source verification requires a clean checkout")
                require(all(m["source"] == source for m in manifests.values()),
                        "SDK source/tree differs from the exact source checkout")
            result = {"status": "CANDIDATE_ARCHIVE_STRUCTURALLY_VALID", "published": False,
                      "rids": sorted(manifests), "sha256": digest(args.archive.read_bytes()),
                      "release_blockers": {rid: release_blockers(m, config) for rid, m in manifests.items()}}
        else:
            result = release_check(args, config)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (SDKError, OSError, ValueError, KeyError, TypeError, ET.ParseError, zipfile.BadZipFile) as error:
        print(f"SDK qualification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
