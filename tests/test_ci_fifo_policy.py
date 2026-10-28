"""No silent lowering of the SaltsCore FIFO runner gate."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "fifo", ROOT / "tools/check_ci_fifo_policy.py")
assert spec is not None and spec.loader is not None
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

class FIFOContractTests(unittest.TestCase):
    def test_original_workflows_all_remain_gated(self):
        for f in sorted((ROOT / ".github/workflows").glob("*.yml")):
            with self.subTest(workflow=f.name):
                self.assertEqual(gate.validate(f.read_text(encoding="utf-8"),
                                               f.name), [])
    def test_cannot_replace_pending_or_cancel_inflight(self):
        s = (ROOT / ".github/workflows/core-contract.yml").read_text(encoding="utf-8")
        for bad in (s.replace("queue: max", "queue: single", 1),
                    s.replace("cancel-in-progress: false", "cancel-in-progress: true", 1),
                    s.replace("saltscore-fifo-workflows", "other", 1)):
            self.assertTrue(gate.validate(bad, "core-contract.yml"))
    def test_cannot_remove_matrix_bound_or_runner_slot(self):
        s = (ROOT / ".github/workflows/core-contract.yml").read_text(encoding="utf-8")
        for bad in (s.replace("max-parallel: 1", "max-parallel: 4", 1),
                    s.replace("group: saltscore-fifo-runners", "other", 1)):
            self.assertTrue(gate.validate(bad, "core-contract.yml"))
    def test_cannot_double_run_feature_push_or_omit_pr(self):
        s = (ROOT / ".github/workflows/core-contract.yml").read_text(encoding="utf-8")
        for bad in (s.replace("  push:\n    branches:\n      - master", "  push:", 1),
                    s.replace("  pull_request:\n", "", 1)):
            self.assertTrue(gate.validate(bad, "core-contract.yml"))

if __name__ == "__main__":
    unittest.main()
