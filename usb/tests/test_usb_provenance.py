"""Bind the native USB source and original tests to the frozen donor blobs.

This is source-retention evidence. It does not admit hardware behavior or
change any of the 2,975-file / 610-function completion records.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


MODULE = Path(__file__).resolve().parents[1]
ROOT = MODULE.parent
SOURCE_REPO = "qigao/salts-utils"
SOURCE_COMMIT = "5b0be65de673261fc37b29485b0364ca38cc6cd6"
ORIGINAL_PATHS = frozenset({
    "CMakeLists.txt",
    "README.md",
    "include/cflow/usb.h",
    "src/usb.c",
    "tests/CMakeLists.txt",
    "tests/cflow_usb_header_cpp_test.cpp",
    "tests/cflow_usb_state_test.c",
    "tests/cflow_usb_test.c",
    "tests/fake/fake_libusb.c",
    "tests/fake/libusb.h",
})
ADAPTED_FILES = frozenset({"CMakeLists.txt", "tests/CMakeLists.txt", "README.md"})


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def retained_path(relative: str) -> Path:
    if relative in ADAPTED_FILES:
        return Path("original") / (relative + ".original")
    return Path(relative)


def verify_frozen_sources(module: Path, inventory: dict) -> None:
    source = inventory["source"]
    if source["repo"] != SOURCE_REPO or source["commit"] != SOURCE_COMMIT:
        raise ValueError("USB donor revision changed")
    rows = {
        row["path"].removeprefix("cflow-usb/"): row
        for row in inventory["files"]
        if row["path"].startswith("cflow-usb/")
    }
    if set(rows) != ORIGINAL_PATHS:
        raise ValueError("USB original file coverage changed")
    for relative, row in rows.items():
        path = module / retained_path(relative)
        if not path.is_file():
            raise ValueError(f"USB original file missing: {relative}")
        # Compare Git blob bytes independently of a Windows checkout's EOLs.
        data = path.read_bytes().replace(b"\r\n", b"\n")
        if len(data) != row["size"] or git_blob(data) != row["git_blob"]:
            raise ValueError(f"USB original bytes changed: {relative}")


class USBProvenance(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.inventory = json.loads(
            (ROOT / "tools/migration/original-salts-utils-files.json").read_text())

    def test_all_ten_original_files_are_retained(self) -> None:
        verify_frozen_sources(MODULE, self.inventory)

    def test_original_header_retains_all_thirteen_functions(self) -> None:
        inventory = json.loads(
            (ROOT / "tools/migration/original-public-functions.json").read_text())
        matching = [row for row in inventory["headers"]
                    if row["source_repo"] == SOURCE_REPO
                    and row["path"] == "cflow-usb/include/cflow/usb.h"]
        self.assertEqual(len(matching), 1)
        row = matching[0]
        self.assertEqual(row["source_commit"], SOURCE_COMMIT)
        self.assertEqual(row["unparsed_declarations"], [])
        expected = set(row["explicit_functions"])
        self.assertEqual(len(expected), 13)
        header = (MODULE / "include/cflow/usb.h").read_text()
        implementation = (MODULE / "src/usb.c").read_text()
        self.assertEqual(set(re.findall(r"\b(cflow_usb_\w+)\s*\(", header)), expected)
        definitions = set(re.findall(
            r"(?m)^(?:int|bool)\s+(cflow_usb_\w+)\s*\(", implementation))
        self.assertEqual(definitions, expected)

    def test_source_drift_cannot_be_accepted_as_retention(self) -> None:
        for relative in ("src/usb.c", "tests/cflow_usb_state_test.c",
                         "tests/fake/fake_libusb.c", "tests/CMakeLists.txt"):
            with self.subTest(relative=relative), tempfile.TemporaryDirectory() as temporary:
                candidate = Path(temporary)
                for original in ORIGINAL_PATHS:
                    target = candidate / retained_path(original)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(MODULE / retained_path(original), target)
                damaged = candidate / retained_path(relative)
                damaged.write_bytes(damaged.read_bytes() + b"\n/* changed */\n")
                with self.assertRaisesRegex(ValueError, "original bytes changed"):
                    verify_frozen_sources(candidate, self.inventory)

    def test_missing_public_header_cannot_be_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            candidate = Path(temporary)
            for original in ORIGINAL_PATHS - {"include/cflow/usb.h"}:
                target = candidate / retained_path(original)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(MODULE / retained_path(original), target)
            with self.assertRaisesRegex(ValueError, "original file missing"):
                verify_frozen_sources(candidate, self.inventory)


class USBWindowsRuntimePackaging(unittest.TestCase):
    """Exercise package-path admission; fixtures do not claim real PE execution."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.cmake = os.environ.get("SALTSCORE_USB_TEST_CMAKE") or shutil.which("cmake")
        if not cls.cmake:
            raise AssertionError("CMake is required for Windows USB package-path admission tests")

    def package(self, root: Path) -> None:
        for relative in ("include/libusb-1.0/libusb.h", "lib/libusb-1.0.lib",
                         "lib/libusb-1.0.a",
                         "bin/libusb-1.0.dll", "debug/lib/libusb-1.0.lib",
                         "debug/bin/libusb-1.0.dll", "share/libusb/copyright"):
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("package-path fixture: " + relative, encoding="utf-8")

    def resolve(self, temporary: Path, libraries: list[Path], include: Path):
        script, result = temporary / "resolve.cmake", temporary / "selected.txt"
        helper = MODULE / "cmake/WindowsLibusbRuntime.cmake"
        quoted = " ".join(f"[[{path.as_posix()}]]" for path in libraries)
        script.write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            f"include([[{helper.as_posix()}]])\n"
            "saltscore_usb_windows_provider_files(runtime copyright\n"
            f"  LIBRARIES {quoted} INCLUDE_DIRS [[{include.as_posix()}]])\n"
            f"file(WRITE [[{result.as_posix()}]] \"${{runtime}}\\n${{copyright}}\\n\")\n",
            encoding="utf-8")
        process = subprocess.run([self.cmake, "-P", str(script)],
                                 capture_output=True, text=True)
        return process, result

    def test_release_runtime_and_license_match_the_selected_provider(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            provider = temporary / "package with spaces"
            self.package(provider)
            process, result = self.resolve(temporary, [provider / "lib/libusb-1.0.lib"],
                                           provider / "include/libusb-1.0")
            self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            self.assertEqual([Path(line) for line in result.read_text().splitlines()],
                             [(provider / "bin/libusb-1.0.dll").resolve(),
                              (provider / "share/libusb/copyright").resolve()])

    def test_debug_import_uses_its_debug_runtime_and_common_license(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            provider = temporary / "package"
            self.package(provider)
            process, result = self.resolve(temporary, [provider / "debug/lib/libusb-1.0.lib"],
                                           provider / "include/libusb-1.0")
            self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            self.assertEqual([Path(line) for line in result.read_text().splitlines()],
                             [(provider / "debug/bin/libusb-1.0.dll").resolve(),
                              (provider / "share/libusb/copyright").resolve()])

    def test_mingw_static_provider_has_no_runtime_to_stage(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            provider = temporary / "package"
            self.package(provider)
            process, result = self.resolve(temporary, [provider / "lib/libusb-1.0.a"],
                                           provider / "include/libusb-1.0")
            self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            selected = result.read_text().splitlines()
            self.assertEqual(selected[0], "")
            self.assertEqual(Path(selected[1]),
                             (provider / "share/libusb/copyright").resolve())

    def test_missing_files_and_mixed_providers_fail_closed(self) -> None:
        for failure in ("runtime", "license", "header_prefix", "ambiguous_import"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                temporary = Path(directory)
                provider, other = temporary / "package", temporary / "other"
                self.package(provider)
                self.package(other)
                libraries = [provider / "lib/libusb-1.0.lib"]
                include = provider / "include/libusb-1.0"
                if failure == "runtime":
                    (provider / "bin/libusb-1.0.dll").unlink()
                elif failure == "license":
                    (provider / "share/libusb/copyright").unlink()
                elif failure == "header_prefix":
                    include = other / "include/libusb-1.0"
                else:
                    libraries.append(other / "lib/libusb-1.0.lib")
                process, result = self.resolve(temporary, libraries, include)
                self.assertNotEqual(process.returncode, 0)
                self.assertIn("Windows USB", process.stderr)
                self.assertFalse(result.exists())


if __name__ == "__main__":
    unittest.main()
