#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

DONORS = {
    "cflow-fs/CMakeLists.txt":
        ("fs/original/cflow-fs/CMakeLists.txt.original",
         "4c12e990a9e271e6f65bd73257857a38ddcf9019"),
    "cflow-fs/include/salts/fs_watch.h":
        ("fs/original/cflow-fs/include/salts/fs_watch.h.original",
         "f70748b71d1d0810bb6430c0eb4335a9b351b1ac"),
    "cflow-fs/src/fs_watch.c":
        ("fs/original/cflow-fs/src/fs_watch.c.original",
         "c5b1c113509831f1d1ad18d26a42d736fa73fabf"),
    "cflow-fs/src/fs_watch_internal.h":
        ("fs/original/cflow-fs/src/fs_watch_internal.h.original",
         "71e2dadcaec57bab8139867a111f1936b232d83c"),
    "cflow-fs/src/fs_watch_linux.c":
        ("fs/original/cflow-fs/src/fs_watch_linux.c.original",
         "364a2c16c6bd8557b4f5057cb252e339fc367ef3"),
    "cflow-fs/src/fs_watch_macos.m":
        ("fs/original/cflow-fs/src/fs_watch_macos.m.original",
         "7bc5f0bea61d407b041867b432e03778ae78be69"),
    "cflow-fs/src/fs_watch_windows.c":
        ("fs/original/cflow-fs/src/fs_watch_windows.c.original",
         "ce75afd2b113413a2913a8e05eeaca9b25b45ee3"),
    "cflow-fs/src/fs_watch_unsupported.c":
        ("fs/original/cflow-fs/src/fs_watch_unsupported.c.original",
         "5f3a1f91da394005a10dd2dcc9f41cddf66bfe39"),
    "cflow-fs/tests/cflow_fs_watch_test.c":
        ("fs/original/cflow-fs/tests/cflow_fs_watch_test.c.original",
         "78072c8725ebe9ca9d5af594f01174669147d638"),
}

ACTIVE_IDENTICAL = {
    "fs/include/salts/fs_watch.h":
        "fs/original/cflow-fs/include/salts/fs_watch.h.original",
    "fs/src/fs_watch.c":
        "fs/original/cflow-fs/src/fs_watch.c.original",
    "fs/src/fs_watch_internal.h":
        "fs/original/cflow-fs/src/fs_watch_internal.h.original",
    "fs/src/fs_watch_linux.c":
        "fs/original/cflow-fs/src/fs_watch_linux.c.original",
    "fs/src/fs_watch_macos.m":
        "fs/original/cflow-fs/src/fs_watch_macos.m.original",
    "fs/src/fs_watch_windows.c":
        "fs/original/cflow-fs/src/fs_watch_windows.c.original",
    "fs/src/fs_watch_unsupported.c":
        "fs/original/cflow-fs/src/fs_watch_unsupported.c.original",
}

WATCH_APIS = [
    "cflow_fs_watch_acknowledge_rescan",
    "cflow_fs_watch_close",
    "cflow_fs_watch_destroy",
    "cflow_fs_watch_get_stats",
    "cflow_fs_watch_is_quiescent",
    "cflow_fs_watch_open",
    "cflow_fs_watch_run_ready",
]

PLATFORM_ONLY = {
    "cmeta_mutex_destroy",
    "cmeta_mutex_init",
    "cmeta_mutex_lock",
    "cmeta_mutex_t",
    "cmeta_mutex_unlock",
    "cmeta_thread_create",
    "cmeta_thread_destroy",
    "cmeta_thread_join",
    "cmeta_thread_t",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode("ascii") + b"\0" + data
    ).hexdigest()


def main() -> int:
    inventory = json.loads(
        (ROOT / "tools/migration/original-salts-utils-files.json").read_text())
    rows = {row["path"]: row for row in inventory["files"]}
    for donor_path, (copy_path, blob) in DONORS.items():
        row = rows.get(donor_path)
        require(row is not None, "frozen FS-watch donor path vanished: " + donor_path)
        require(row["git_blob"] == blob,
                "frozen FS-watch donor inventory changed: " + donor_path)
        copy = ROOT / copy_path
        require(copy.is_file(), "frozen FS-watch donor copy missing: " + copy_path)
        require(git_blob(copy.read_bytes().replace(b"\r\n", b"\n")) == blob,
                "frozen FS-watch donor copy changed: " + donor_path)

    for active_path, copy_path in ACTIVE_IDENTICAL.items():
        active = (ROOT / active_path).read_bytes().replace(b"\r\n", b"\n")
        donor = (ROOT / copy_path).read_bytes().replace(b"\r\n", b"\n")
        require(active == donor,
                "active callback-only FS watch source drifted from donor: " + active_path)

    public = json.loads(
        (ROOT / "tools/migration/original-public-functions.json").read_text())
    headers = {row["path"]: row for row in public["headers"]}
    require(
        headers["cflow-fs/include/salts/fs_watch.h"]["explicit_functions"] ==
        WATCH_APIS,
        "frozen seven-function FS-watch API inventory changed")
    require(
        len(headers["cflow-fs/include/salts/fs_watch_publisher.h"]
            ["explicit_functions"]) == 4,
        "frozen Publisher split inventory changed")

    cmake = (ROOT / "fs/CMakeLists.txt").read_text()
    require("src/fs_watch.c" in cmake,
            "callback-only FS watch owner is not compiled")
    for backend in ("fs_watch_linux.c", "fs_watch_macos.m",
                    "fs_watch_windows.c", "fs_watch_unsupported.c"):
        require(backend in cmake, "FS watch backend is absent: " + backend)
    require("fs_watch_publisher" not in cmake and "Salts::CFlow" not in cmake,
            "Core FS incorrectly absorbed the reactive Publisher/CFlow layer")

    combined = "\n".join(
        (ROOT / path).read_text()
        for path in ACTIVE_IDENTICAL if path.startswith("fs/src/"))
    used = set(re.findall(r"\bcmeta_[A-Za-z0-9_]+", combined))
    require(used <= PLATFORM_ONLY,
            "FS watch acquired non-Platform cmeta dependencies: " +
            ", ".join(sorted(used - PLATFORM_ONLY)))
    for forbidden in ('#include "cmeta_', "TLOG_", "SALTS_C_API"):
        require(forbidden not in combined,
                "FS watch acquired forbidden runtime/export dependency: " + forbidden)

    test = (ROOT / "tests/test_fs_watch.c").read_text()
    for marker in (
        "test_create_rename_remove",
        "test_bounded_loss",
        "test_root_change",
        "test_recursive",
        "CFLOW_FS_WATCH_RESCAN_REQUIRED",
        "cflow_fs_watch_acknowledge_rescan",
    ):
        require(marker in test, "native FS-watch behavior coverage missing: " + marker)

    installed = (ROOT / "tests/fs-installed/CMakeLists.txt").read_text()
    require("saltscore.fs.watch_installed_c" in installed and
            "saltscore.fs.watch_installed_cpp" in installed,
            "installed C11/C++17 FS-watch consumers are not registered")

    workflow = (ROOT / ".github/workflows/native-fs.yml").read_text()
    require("CMeta-runtime-free owner and exact Platform ABI boundary" in workflow,
            "FS workflow does not enforce the exact Platform-only dependency boundary")
    require(
        "cmeta_mutex_(destroy|init|lock|unlock)" in workflow and
        "cmeta_thread_(create|destroy|join)" in workflow and
        "cmeta_mutex_t" in workflow and "cmeta_thread_t" in workflow,
        "FS workflow Platform-only whitelist changed")

    print("PASS: callback-only FS watch keeps exact donor bytes, 7 APIs, native backends and CFlow split")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
