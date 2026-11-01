"""Freeze all 39 Capture inputs without claiming physical-device acceptance."""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL_CMAKE = {
    "capture/CMakeLists.txt": "capture/original/CMakeLists.txt.original",
    "capture/tests/CMakeLists.txt": "capture/original/tests/CMakeLists.txt.original",
    "capture/tests/native_tsan/CMakeLists.txt": "capture/original/tests/native_tsan/CMakeLists.txt.original",
}


class CaptureSourceProvenance(unittest.TestCase):
    def test_all_thirty_nine_frozen_inputs_are_retained(self):
        index = json.loads((ROOT / "tools/migration/original-salts-utils-files.json").read_text())
        inputs = [row for row in index["files"] if row["path"].startswith("capture/")]
        self.assertEqual(len(inputs), 39)
        for row in inputs:
            with self.subTest(path=row["path"]):
                path = ROOT / ORIGINAL_CMAKE.get(row["path"], row["path"])
                data = path.read_bytes()
                blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
                self.assertEqual(blob, row["git_blob"])

    def test_ios_public_gap_is_bounded_without_editing_frozen_donors(self):
        cmake = (ROOT / "capture/CMakeLists.txt").read_text()
        match = re.search(
            r"# BEGIN bounded iOS API surface completion(.*?)"
            r"# END bounded iOS API surface completion",
            cmake, re.S)
        self.assertIsNotNone(match)
        block = match.group(1)
        expected = [
            "salts_capture_list_gpu_devices",
            "salts_video_capture_get_control_range",
            "salts_video_capture_set_control",
            "salts_video_capture_get_control",
            "salts_video_capture_set_crop",
            "salts_video_capture_get_crop",
        ]
        self.assertEqual(
            re.findall(r"(?m)^int\s+(salts_[A-Za-z0-9_]+)\s*\(", block),
            expected)
        self.assertIn('file(WRITE "${_salts_capture_ios_surface}"', block)
        self.assertIn('target_sources(salts_capture PRIVATE "${_salts_capture_ios_surface}")', block)
        self.assertEqual(block.count("SALTS_CAPTURE_ERR_UNSUPPORTED"), 5)

    def test_export_lists_keep_all_original_public_symbols(self):
        def symbols(header):
            return re.findall(r"SALTS_CAPTURE_API\s+.*?\b(salts_[A-Za-z0-9_]+)\s*\(",
                              (ROOT / header).read_text(), re.S)
        common = symbols("capture/include/capture.h")
        android = symbols("capture/include/capture_android.h")
        self.assertEqual(len(common), 26)
        self.assertEqual(len(android), 3)
        apple = (ROOT / "capture/apple_exports.txt").read_text().splitlines()
        self.assertEqual(apple, ["_" + name for name in common])
        elf = re.findall(r"^\s+(salts_[a-z0-9_]+);$",
                         (ROOT / "capture/exports.map").read_text(), re.M)
        self.assertEqual(elf, common + android)
        self.assertEqual(len(common + android), 29)


if __name__ == "__main__":
    unittest.main()
