"""Immutable qigao/salts + qigao/salts-utils source/function loss gates."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/audit_dual_source_migration.py"
SPEC = importlib.util.spec_from_file_location("salts_dual_original", SCRIPT)
assert SPEC and SPEC.loader
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class DualSourceMigrationLossTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources, cls.api = audit.read_all(ROOT)

    def test_exact_original_git_trees_are_not_truncated_or_replaced(self):
        self.assertEqual(self.sources["salts"]["raw"]["source"]["count"], 1554)
        self.assertEqual(self.sources["salts-utils"]["raw"]["source"]["count"], 1421)
        self.assertEqual(len(self.api["headers"]), 96)
        self.assertEqual(sum(len(h["functions"]) for h in self.api["headers"]), 1191)
        self.assertEqual(audit.gap_summary(self.sources, self.api)
                         ["native_core_file_candidates"], 897)

    def test_apparently_upper_runtime_does_not_hide_native_code(self):
        for source, required in audit.REQUIRED_SOURCES.items():
            for path, owner in required.items():
                with self.subTest(source=source, path=path):
                    self.assertEqual(self.sources[source]["index"][path]["owner"], owner)
        # Critical native functions must remain explicitly numbered and owned.
        headers = {(h["source"], h["header"]): h for h in self.api["headers"]}
        for source, path, symbol in (
            ("salts-utils", "capture/include/salts_capture.h", "salts_capture_start"),
            ("salts-utils", "playback/include/salts_playback.h", "salts_playback_drain"),
            ("salts-utils", "salts_serial/salts_serial.h", "salts_serial_event_wait_ex"),
            ("salts-utils", "cflow-usb/include/cflow/usb.h", "cflow_usb_submit"),
            ("salts-utils", "cflow-fs/include/salts/fs_watch.h", "cflow_fs_watch_open"),
            ("salts", "cnet/include/cnet/cnet.h", "cnet_client_init"),
            ("salts", "native-io/include/salts/native_io.h", "native_io_backend_init"),
        ):
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, headers[(source, path)]["functions"])

    def test_original_git_blob_hashes_are_immutable(self):
        self.assertEqual(audit.git_blob(b"test\n"),
                         "9daeafb9864cf43055ae93beb0afd6c7d144bfa4")
        for source, data in audit.MANIFESTS.items():
            with self.subTest(repo=source):
                path = ROOT / data["path"]
                self.assertEqual(audit.git_blob(path.read_bytes().replace(b"\r\n", b"\n")),
                                 data["blob"])

    def test_source_deletion_or_modified_inventory_is_rejected(self):
        for owner in ("salts", "salts-utils"):
            manifest = audit.MANIFESTS[owner]
            with self.subTest(owner=owner), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                file = root / manifest["path"]
                file.parent.mkdir(parents=True)
                raw = (ROOT / manifest["path"]).read_bytes()
                file.write_bytes(raw)
                self.assertEqual(audit.verified_json(root, manifest["path"],
                                                     manifest["blob"])["source"]["count"],
                                 manifest["count"])
                file.write_bytes(raw.replace(b'"owner": "salts-core"',
                                             b'"owner": "salts"', 1))
                with self.assertRaisesRegex(ValueError, "Git blob changed"):
                    audit.verified_json(root, manifest["path"], manifest["blob"])
                file.unlink()
                with self.assertRaisesRegex(ValueError, "missing immutable"):
                    audit.verified_json(root, manifest["path"], manifest["blob"])

    def test_public_api_ledger_cannot_drop_functions(self):
        path = audit.API_FILE
        expected = audit.API_BLOB
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            f = root / path
            f.parent.mkdir(parents=True)
            raw = (ROOT / path).read_bytes()
            f.write_bytes(raw)
            self.assertEqual(len(audit.verified_json(root, path, expected)["headers"]), 96)
            f.write_bytes(raw.replace(b'"salts_capture_start"', b'"removed_start"', 1))
            with self.assertRaisesRegex(ValueError, "Git blob changed"):
                audit.verified_json(root, path, expected)

    def test_full_qualification_cannot_pass_from_file_names(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "FULL MIGRATION NOT ACCEPTED"):
                audit.enforce_full(Path(tmp), self.sources, self.api)
        # Explicitly preserve the fact that inline/macro functionality needs
        # separate review; plain C prototypes are a lower bound, not completeness.
        summary = audit.gap_summary(self.sources, self.api)
        self.assertGreater(summary["macro_inline_review_headers"], 0)
        self.assertGreater(summary["original_unaccounted_core_baseline"], 0)


if __name__ == "__main__":
    unittest.main()
