"""Negative CI checks for byte-stable Salts-to-Core module naming."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/check_original_module_paths.py"
SPEC = importlib.util.spec_from_file_location("salts_original_source_paths", SOURCE)
assert SPEC is not None and SPEC.loader is not None
orig = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(orig)


def materialize_fake_layout(root: Path, names: tuple[str, ...]) -> None:
    for name in names:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("isolated source-byte fixture\n", encoding="utf-8")


class OriginalSaltsModulePathAdmission(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.names = orig.read_original_paths()

    def test_real_preextraction_inventory_preserves_all_source_paths(self) -> None:
        self.assertEqual(len(self.names), 125)
        self.assertEqual(len(orig.EXPECTED_COUNTS), 6)
        orig.validate(ROOT, self.names)

    def test_noncore_original_relative_file_name_cannot_disappear(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            materialize_fake_layout(root, self.names)
            target = root / "native-io/src/native_io_iocp.c"
            self.assertIn("native-io/src/native_io_iocp.c", self.names)
            target.unlink()
            with self.assertRaisesRegex(ValueError, "original Salts source/header path missing"):
                orig.validate(root, self.names)

    def test_renaming_module_is_not_a_compatibility_path(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            materialize_fake_layout(root, self.names)
            native = root / "platform/src/readiness.c"
            duplicate = root / "native_platform/src/readiness.c"
            duplicate.parent.mkdir(parents=True, exist_ok=True)
            native.rename(duplicate)
            with self.assertRaisesRegex(ValueError, "original Salts source/header path missing"):
                orig.validate(root, self.names)

    def test_historical_retired_ace_and_cmeta_headers_must_remain_absent(self) -> None:
        for name in orig.RETIRED_NONCORE_SURFACES:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                materialize_fake_layout(root, self.names)
                old = root / name
                old.parent.mkdir(parents=True, exist_ok=True)
                old.write_text("retired ABI\n", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "retired non-Core source reintroduced"):
                    orig.validate(root, self.names)

    def test_source_content_may_change_without_changing_its_name(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            materialize_fake_layout(root, self.names)
            (root / "platform/src/readiness.c").write_text("a legitimate bug fix\n")
            orig.validate(root, self.names)

    def test_manifest_cannot_silently_drop_or_relabel_a_path(self) -> None:
        text = orig.MANIFEST.read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "inventory.txt"
            path.write_text(text.replace("platform/src/readiness.c\n", ""), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "identity changed"):
                orig.read_original_paths(path)
            path.write_text(text.replace("cnet/src/cnet_client.c",
                                         "network/src/cnet_client.c"), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "identity changed"):
                orig.read_original_paths(path)


if __name__ == "__main__":
    unittest.main()
