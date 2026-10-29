"""Frozen full-tree provenance and original native mmap function gates.

This is NOT a functional-migration-complete signal: all 2,975 original
source blobs and each unresolved owner remain visible as a bounded debt.
"""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "migration_owner", ROOT / "tools/check_full_source_migration.py")
assert SPEC and SPEC.loader
proof = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(proof)


class ExactMigrationSourceTests(unittest.TestCase):
    def test_all_2975_source_files_and_owners_remain_accounted_for(self):
        a, b = proof.verify_origin(ROOT, "salts"), proof.verify_origin(ROOT, "salts-utils")
        self.assertEqual(len(a[0]["files"]), 1554)
        self.assertEqual(len(b[0]["files"]), 1421)
        self.assertEqual(sum(v["count"] for v in proof.ORIGINS.values()), 2975)
        for key, (manifest, _) in zip(("salts", "salts-utils"), (a, b)):
            self.assertEqual(manifest["source"]["tree"],
                             proof.ORIGINS[key]["tree"])

    def test_original_blob_cannot_be_renamed_or_reclassified_silently(self):
        for key in ("salts", "salts-utils"):
            source = ROOT / proof.ORIGINS[key]["file"]
            with self.subTest(repo=key), tempfile.TemporaryDirectory() as temp:
                dest = Path(temp) / proof.ORIGINS[key]["file"]
                dest.parent.mkdir(parents=True, exist_ok=True)
                data = source.read_bytes()
                dest.write_bytes(data.replace(
                    b"unaccounted-source-needs-owner",
                    b"upstream-retained-verify-ownership", 1))
                with self.assertRaisesRegex(ValueError, "modified"):
                    proof.verify_origin(Path(temp), key)

    def test_required_critical_native_symbols_and_source_blobs_exist(self):
        for (repo, path), sha in proof.CRITICAL_SHA.items():
            with self.subTest(repo=repo, path=path):
                data, _ = proof.verify_origin(ROOT, repo)
                found = [r for r in data["files"] if r["path"] == path]
                self.assertEqual(len(found), 1)
                self.assertEqual(found[0]["git_blob"], sha)

    def test_mmap_18_api_signatures_have_real_native_implementation(self):
        self.assertEqual(len(proof.MMAP_EXPORTS), 18)
        proof.verify_restored_mmap(ROOT)

    def test_mmap_body_cannot_be_replaced_by_an_empty_stub(self):
        paths = (
            "core/include/cmeta_mmap.h", "core/src/cmeta_mmap.c",
            "core/CMakeLists.txt", "tests/CMakeLists.txt",
            "tests/installed/CMakeLists.txt",
            "tests/test_original_native_mmap.c",
            "tests/installed/original_mmap_consumer.c",
            "tests/installed/original_mmap_consumer.cpp")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for rel in paths:
                to = root / rel
                to.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / rel, to)
            impl = root / "core/src/cmeta_mmap.c"
            source = impl.read_text(encoding="utf-8")
            self.assertIn("cmeta_mmap_group_open", source)
            impl.write_text(
                source.replace("cmeta_mmap_group_open(", "deleted_group_open(", 1),
                encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "implementation missing"):
                proof.verify_restored_mmap(root)

    def test_final_gate_stays_red_until_all_originals_qualified(self):
        report = proof.audit(ROOT)
        self.assertEqual(report["salts"]["frozen_total"], 1554)
        self.assertEqual(report["salts-utils"]["frozen_total"], 1421)
        self.assertGreater(report["salts"]["unaccounted_or_changed"], 0)
        self.assertGreater(report["salts-utils"]["unaccounted_or_changed"], 0)
        with self.assertRaisesRegex(ValueError, "FINAL MIGRATION BLOCKED"):
            proof.audit(ROOT, require_complete=True)


if __name__ == "__main__":
    unittest.main()
