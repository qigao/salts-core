"""Negative tests for no-loss qigao/crypto PR #43 source handoff."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "tools/check_crypto_pr43_handoff.py"
spec = importlib.util.spec_from_file_location("donor_handoff", MODULE)
code = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(code)

class CryptoRetirementSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = json.loads((ROOT / "tools/crypto_pr43_retirement_manifest.json"
                                ).read_text(encoding="utf-8"))

    def test_all_original_sources_present_and_unchanged(self):
        self.assertEqual(code.check(ROOT, self.source), [])

    def test_missing_donor_asset_is_rejected(self):
        data = copy.deepcopy(self.source)
        data["files"].pop()
        self.assertTrue(code.check(ROOT, data))

    def test_false_migration_path_is_rejected(self):
        data = copy.deepcopy(self.source)
        data["files"][0]["destination"] = "../outside.c"
        self.assertTrue(code.check(ROOT, data))

    def test_changed_source_sha_is_rejected(self):
        data = copy.deepcopy(self.source)
        data["files"][0]["destination_blob_sha"] = "0" * 40
        self.assertTrue(code.check(ROOT, data))

    def test_root_checker_cannot_disappear(self):
        data = copy.deepcopy(self.source)
        data["files"] = [entry for entry in data["files"]
                         if entry["original"] != "tools/check_module_ownership.py"]
        self.assertTrue(code.check(ROOT, data))

if __name__ == "__main__":
    unittest.main()
