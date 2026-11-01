"""A preserved native file must also be part of the reviewed compiled profile."""
from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "local_native_source", ROOT / "crypto/tools/check_local_native_source.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class NativeSourceAdmission(unittest.TestCase):
    def setUp(self):
        self.lock = json.loads((CHECK.NATIVE / "source-lock.json").read_text())

    def test_original_reviewed_recipe_and_source_inputs(self):
        self.assertEqual(CHECK.verify(), 166)

    def test_source_presence_cannot_hide_an_uncompiled_algorithm(self):
        text = (CHECK.NATIVE / "CMakeLists.txt").read_text()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(text.replace("  src/sm4_xts.c\n", ""))
            with patch.object(CHECK, "NATIVE", root):
                with self.assertRaisesRegex(ValueError, "changed reviewed"):
                    CHECK.verify_build_recipe(self.lock)

    def test_hash_update_cannot_self_approve_a_changed_recipe(self):
        text = (CHECK.NATIVE / "CMakeLists.txt").read_text()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            changed = text.replace("  ENABLE_SM4_XTS\n", "").encode()
            (root / "CMakeLists.txt").write_bytes(changed)
            self.lock["build_recipe"]["git_blob"] = CHECK.git_blob(changed)
            with patch.object(CHECK, "NATIVE", root):
                with self.assertRaisesRegex(ValueError, "no reviewed identity"):
                    CHECK.verify_build_recipe(self.lock)

    def test_lock_cannot_shrink_the_compiled_source_or_feature_profile(self):
        for key in ("common_sources", "compile_definitions"):
            with self.subTest(profile=key):
                changed = copy.deepcopy(self.lock)
                changed[key].pop()
                with self.assertRaisesRegex(ValueError, "profile differs"):
                    CHECK.verify_build_recipe(changed)

    def test_platform_rng_cannot_be_substituted(self):
        self.lock["platform_sources"]["windows"][0] = "src/rand.c"
        with self.assertRaisesRegex(ValueError, "platform source selection"):
            CHECK.verify_build_recipe(self.lock)


if __name__ == "__main__":
    unittest.main()
