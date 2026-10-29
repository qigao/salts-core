"""Keep every frozen Playback input; source retention is not device acceptance."""
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL_CMAKE = {
    "playback/CMakeLists.txt": "playback/original/CMakeLists.txt.original",
    "playback/tests/CMakeLists.txt": "playback/original/tests/CMakeLists.txt.original",
}


class PlaybackSourceProvenance(unittest.TestCase):
    def test_all_eleven_frozen_inputs_are_retained(self):
        manifest = json.loads((ROOT / "tools/migration/original-salts-utils-files.json").read_text())
        inputs = [row for row in manifest["files"] if row["path"].startswith("playback/")]
        self.assertEqual(len(inputs), 11)
        for row in inputs:
            with self.subTest(path=row["path"]):
                path = ROOT / ORIGINAL_CMAKE.get(row["path"], row["path"])
                data = path.read_bytes().replace(b"\r\n", b"\n")
                blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
                self.assertEqual(blob, row["git_blob"])


if __name__ == "__main__":
    unittest.main()
