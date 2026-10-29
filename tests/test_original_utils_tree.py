"""Negative tests for complete old-utils inventory admission (offline; no download)."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

BASE = Path(__file__).resolve().parents[1]
SCRIPT = BASE / "tools" / "audit_original_utils_tree.py"
spec = importlib.util.spec_from_file_location("original_utils_tree_gate", SCRIPT)
module = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)

class UtilsTreeInventoryTests(unittest.TestCase):
    def setUp(self):
        self.base = BASE
        self.manifest = json.loads((BASE / module.INVENTORY).read_text(encoding="utf-8"))

    def test_original_175_files_and_eight_directories(self):
        self.assertEqual(module.check_inventory(BASE, self.manifest), [])

    def test_removed_source_entry_is_rejected(self):
        fixture = copy.deepcopy(self.manifest)
        fixture["files"].pop()
        self.assertTrue(module.check_inventory(BASE, fixture))

    def test_changed_original_blob_is_rejected(self):
        fixture = copy.deepcopy(self.manifest)
        fixture["files"][0]["source_blob_sha"] = "0" * 40
        self.assertTrue(module.check_inventory(BASE, fixture))

    def test_invalid_destination_and_fake_acceptance_rejected(self):
        fixture = copy.deepcopy(self.manifest)
        fixture["files"][0]["status"] = "verified"
        fixture["files"][0]["destination_candidates"] = [{"path":"../escape","blob_sha":"bad"}]
        result = module.check_inventory(BASE, fixture)
        self.assertTrue(any("cannot mark original file verified" in x for x in result))
        self.assertTrue(any("invalid candidate path" in x for x in result))

    def test_full_migration_must_stay_red_until_each_file_is_verified(self):
        self.assertEqual(module.check_inventory(BASE, self.manifest), [])
        self.assertTrue(any("FULL MIGRATION NOT ACCEPTED" in x
                            for x in module.check_inventory(BASE,self.manifest,full=True)))

if __name__ == "__main__":
    unittest.main()
