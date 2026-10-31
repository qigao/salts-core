"""Negative public API inventory regression tests."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
P=ROOT/"tools/audit_original_utils_public_api.py"
spec=importlib.util.spec_from_file_location("api_audit",P)
module=importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)
class PublicAPIAuditTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads((ROOT/module.PATH).read_text(encoding="utf-8"))
    def test_exact_35_header_465_function_baseline(self):
        self.assertEqual(module.check(self.data),[])
    def test_missing_header_is_red(self):
        d=copy.deepcopy(self.data)
        d["headers"].pop()
        self.assertTrue(module.check(d))
    def test_missing_original_function_is_red(self):
        d=copy.deepcopy(self.data)
        d["headers"][0]["functions"].pop()
        self.assertTrue(module.check(d))
    def test_false_verified_without_exact_evidence_is_red(self):
        d=copy.deepcopy(self.data)
        d["headers"][0]["functions"][0]["review_status"]="verified"
        self.assertTrue(any("false verified" in e for e in module.check(d)))
    def test_full_parity_stays_red(self):
        self.assertTrue(any("FULL FUNCTION PARITY NOT ACCEPTED" in e
                            for e in module.check(self.data,full=True)))
if __name__=="__main__":
    unittest.main()
