"""Negative tests for no-loss qigao/crypto PR #43 source handoff."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
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

    def copy_inputs(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        for relative in {entry["destination"] for entry in self.source["files"]} | {
                code.BUILD_SNAPSHOT, code.BUILD_DIFF}:
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        self.assertEqual(code.check(root, self.source), [])
        return root

    def test_rehashing_changed_build_cannot_approve_an_unreviewed_adaptation(self):
        root = self.copy_inputs()
        current = root / code.BUILD_SOURCE
        # Reintroducing an external provider is a real owner regression, even
        # if somebody updates the manifest destination hash to match it.
        current.write_text(current.read_text() + "\nfind_package(GmSSL CONFIG REQUIRED)\n")
        data = copy.deepcopy(self.source)
        data["files"][0]["destination_blob_sha"] = code.blob_sha(current)
        self.assertTrue(any("reviewed local build adaptation" in error
                            for error in code.check(root, data)))
        # Neither rewriting the diff nor claiming its new hash is review.
        patch = root / code.BUILD_DIFF
        patch.write_text(code.build_diff(root / code.BUILD_SNAPSHOT, current))
        data["files"][0]["adaptation"]["reviewed_diff_blob_sha"] = code.blob_sha(patch)
        self.assertTrue(any("reviewed diff" in error or "adaptation diff" in error
                            for error in code.check(root, data)))

    def test_original_build_snapshot_cannot_be_replaced_or_omitted(self):
        root = self.copy_inputs()
        snapshot = root / code.BUILD_SNAPSHOT
        snapshot.write_text(snapshot.read_text() + "\n# altered historical source\n")
        data = copy.deepcopy(self.source)
        data["files"][0]["source_blob_sha"] = code.blob_sha(snapshot)
        data["files"][0]["adaptation"]["original_snapshot_blob_sha"] = code.blob_sha(snapshot)
        self.assertTrue(code.check(root, data))
        snapshot.unlink()
        self.assertTrue(any("snapshot changed or disappeared" in error
                            for error in code.check(root, self.source)))

    def test_build_adaptation_cannot_absorb_original_source_or_test_changes(self):
        root = self.copy_inputs()
        for relative in ("crypto/src/crypto_tls_gmssl.c", "crypto/tests/owner/consumer.cpp"):
            with self.subTest(path=relative):
                data = copy.deepcopy(self.source)
                current = root / relative
                original = current.read_bytes()
                current.write_bytes(original + b"\n/* changed original contract */\n")
                entry = next(row for row in data["files"] if row["destination"] == relative)
                entry["destination_blob_sha"] = code.blob_sha(current)
                entry["kind"] = "local_build_adapted"
                self.assertTrue(any("CMake source identity" in error
                                    for error in code.check(root, data)))
                current.write_bytes(original)

if __name__ == "__main__":
    unittest.main()
