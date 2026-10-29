"""Evidence integrity regressions; fixture reports never qualify production."""
from contextlib import contextmanager
from copy import deepcopy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "migration_evidence", ROOT / "tools/check_migration_evidence.py")
evidence = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(evidence)


def early_receipt():
    """Incomplete fixture used only to reach and reject an earlier boundary."""
    targets = {
        name: {"codemodel": {"name": name, "type": kind, "sources": [
            {"path": path, "compileGroupIndex": 0} for path in sources]}}
        for name, (kind, sources) in evidence.NATIVE_TARGETS.items()}
    return {
        "schema": 1, "module": "simd", "profile": "linux-release",
        "source": {"head_commit": "a" * 40, "worktree_sha256": "b" * 64, "file_count": 1},
        "contract_blob": evidence.git_blob(evidence.canonical((ROOT / evidence.CONTRACT).read_bytes())),
        "host": {"system": "Linux", "machine": "x86_64"}, "created_utc": "fixture",
        "builds": {
            "native": {"config": "Release", "source_root": str(ROOT),
                       "cache": {"BUILD_TESTING": "ON", "SALTSCORE_BUILD_SIMD": "ON",
                                 "SALTSCORE_BUILD_TINYTEST": "ON"}, "targets": targets,
                       "toolchains": {"toolchains": [
                           {"language": language, "compiler": {"path": "/fixture/cc", "id": "GNU"}}
                           for language in ("C", "CXX")]}},
            "installed": {}},
        "tests": {"native": {}, "installed": {}}, "sdk": {},
    }


@contextmanager
def receipt_file(data):
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "receipt.json"
        path.write_text(json.dumps(data))
        yield path


class MigrationEvidenceTests(unittest.TestCase):
    def test_no_receipts_leave_every_baseline_obligation_pending(self):
        result = evidence.audit_evidence(ROOT)
        self.assertEqual(result["qualified_file_ids"], [])
        self.assertEqual(result["qualified_api_ids"], [])
        self.assertEqual(result["simd_supplement"]["qualified_native_functions"], 0)
        self.assertEqual(len(result["simd_supplement"]["pending_profiles"]), 4)
        self.assertEqual(result["simd_supplement"]["original_files_pending"], 7)
        self.assertEqual(result["simd_supplement"]["upper_reflection"], "pending")

    def test_handwritten_acceptance_and_empty_evidence_directory_are_rejected(self):
        with receipt_file({"schema": 1, "module": "simd", "accepted": True}) as path:
            with self.assertRaisesRegex(ValueError, "accepted flags"):
                evidence.verify_receipt(ROOT, path)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "contains no receipts"):
                evidence.audit_evidence(ROOT, Path(directory))

    def test_stale_tree_cannot_be_relabelled_as_current_evidence(self):
        data = early_receipt()
        current = dict(data["source"], worktree_sha256="c" * 64)
        with receipt_file(data) as path:
            with self.assertRaisesRegex(ValueError, "different candidate source tree"):
                evidence.verify_receipt(ROOT, path, current)

    def test_source_hash_change_is_rejected_even_when_symbol_names_remain(self):
        source = ROOT / "simd/src/simd.c"
        original_read = Path.read_bytes
        def read_bytes(path):
            data = original_read(path)
            return data + b"\n/* changed execution source */\n" if path == source else data
        with patch.object(Path, "read_bytes", read_bytes):
            with self.assertRaisesRegex(ValueError, "source/test blob changed"):
                evidence.load_contract(ROOT)

    def test_shared_owner_wrong_source_and_disabled_native_build_are_rejected(self):
        data = early_receipt()
        scenarios = []
        shared = deepcopy(data)
        shared["builds"]["native"]["targets"]["saltscore_simd"]["codemodel"]["type"] = "SHARED_LIBRARY"
        scenarios.append((shared, "type/owner differs"))
        wrong_source = deepcopy(data)
        wrong_source["builds"]["native"]["targets"]["saltscore_simd"]["codemodel"]["sources"][0]["path"] = "tests/stub.c"
        scenarios.append((wrong_source, "compiled target sources differ"))
        disabled = deepcopy(data)
        disabled["builds"]["native"]["cache"]["SALTSCORE_BUILD_SIMD"] = "OFF"
        scenarios.append((disabled, "required native feature disabled"))
        for mutated, message in scenarios:
            with self.subTest(message=message), receipt_file(mutated) as path:
                with self.assertRaisesRegex(ValueError, message):
                    evidence.verify_receipt(ROOT, path, data["source"])

    def test_changed_artifact_report_and_path_escape_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory)
            output = bundle / "junit.xml"
            output.write_text("actual test report")
            record = {"path": "junit.xml", "sha256": evidence.digest(output.read_bytes())}
            evidence.checked_file(bundle, record)
            output.write_text("rewritten as passing")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                evidence.checked_file(bundle, record)
            with self.assertRaisesRegex(ValueError, "escapes bundle"):
                evidence.safe_file(bundle, "../report.xml")

    def test_real_ctest_reports_require_executed_exact_test_without_skip_or_failure(self):
        # CMake can configure this project through an absolute executable even
        # when its installation is not on PATH. Use that configured toolchain.
        ctest = (os.environ.get("SALTSCORE_CTEST_COMMAND") or
                 os.environ.get("CMAKE_CTEST_COMMAND") or shutil.which("ctest"))
        cmake = (os.environ.get("SALTSCORE_CMAKE_COMMAND") or
                 os.environ.get("CMAKE_COMMAND") or shutil.which("cmake"))
        if not cmake and ctest:
            companion = Path(ctest).with_name("cmake.exe" if os.name == "nt" else "cmake")
            if companion.is_file():
                cmake = str(companion)
        self.assertTrue(ctest and Path(ctest).is_file(),
                        "CTest is required; supply SALTSCORE_CTEST_COMMAND or put it on PATH")
        self.assertTrue(cmake and Path(cmake).is_file(),
                        "CMake is required; supply SALTSCORE_CMAKE_COMMAND or put it on PATH")
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            cmake = Path(cmake).as_posix()
            (build / "CTestTestfile.cmake").write_text(
                f'add_test(evidence.pass "{cmake}" "-E" "true")\n'
                f'add_test(evidence.fail "{cmake}" "-E" "false")\n'
                f'add_test(evidence.skip "{cmake}" "-E" "true")\n'
                'set_tests_properties(evidence.skip PROPERTIES DISABLED TRUE)\n')
            for name, success in (("evidence.pass", True), ("evidence.fail", False),
                                  ("evidence.skip", False)):
                report = build / (name + ".xml")
                subprocess.run(
                    [ctest, "--test-dir", str(build), "-R", "^" + name + "$",
                     "--no-tests=error", "--output-junit", str(report)],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
                if success:
                    evidence.validate_junit(report, name)
                    with self.assertRaisesRegex(ValueError, "missing, extra or renamed"):
                        evidence.validate_junit(report, "wrong.test")
                else:
                    with self.assertRaisesRegex(ValueError, "not executed|failed|nonpassing"):
                        evidence.validate_junit(report, name)
            empty = build / "empty.xml"
            empty.write_text('<testsuite tests="0" failures="0"/>')
            with self.assertRaisesRegex(ValueError, "missing, extra or renamed"):
                evidence.validate_junit(empty, "evidence.pass")

    def test_uncommitted_and_new_source_content_changes_tree_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "owned.c").write_text("int owned(void) { return 1; }\n")
            names = ["owned.c"]
            def git(command, **kwargs):
                if command[1:3] == ["diff", "--name-only"]:
                    return ""
                if "--stage" in command:
                    return "100644 " + "0" * 40 + " 0\towned.c\0"
                if command[1] == "ls-files":
                    return "\0".join(names) + "\0"
                if command[1:3] == ["rev-parse", "HEAD"]:
                    return "a" * 40 + "\n"
                self.fail("unexpected Git command")
            with patch.object(evidence, "run", side_effect=git):
                original = evidence.source_identity(root)
                (root / "owned.c").write_text("int owned(void) { return 2; }\n")
                self.assertNotEqual(original, evidence.source_identity(root))
                changed = evidence.source_identity(root)
                (root / "new.h").write_text("int added(void);\n")
                names.append("new.h")
                self.assertNotEqual(changed, evidence.source_identity(root))

    def test_binary_fixture_line_endings_change_the_candidate_identity(self):
        # This binary protocol fixture is valid UTF-8 and contains no NUL.
        # Git explicitly treats it as bytes; Unicode heuristics must not erase
        # a wire-format change when binding test evidence to the candidate.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            subprocess.run(["git", "-C", str(root), "config", "core.autocrlf", "false"], check=True)
            (root / ".gitattributes").write_text("*.packet -text\n")
            fixture = root / "wire.packet"
            fixture.write_bytes(b"HEADER\r\nfield:value\r\n\r\n")
            subprocess.run(["git", "-C", str(root), "add", "."], check=True)
            original_run = evidence.run

            def temporary_repository(command, **kwargs):
                # No commit is needed in this temporary fixture repository.
                if command[1:] == ["rev-parse", "HEAD"]:
                    return "a" * 40 + "\n"
                return original_run(command, **kwargs)

            with patch.object(evidence, "run", side_effect=temporary_repository):
                original = evidence.source_identity(root)
                fixture.write_bytes(b"HEADER\nfield:value\n\n")
                self.assertNotEqual(original, evidence.source_identity(root))

    def test_even_all_native_profiles_cannot_pay_historical_or_reflection_debt(self):
        # Test only aggregation policy. No synthetic receipt is runtime evidence.
        profiles = list(evidence.PROFILES)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for profile in profiles:
                path = root / profile / "receipt.json"
                path.parent.mkdir()
                path.write_text("{}")
            with patch.object(evidence, "verify_receipt", side_effect=profiles):
                result = evidence.audit_evidence(ROOT, root)
        self.assertEqual(result["simd_supplement"]["qualified_native_functions"], 32)
        self.assertEqual(result["qualified_file_ids"], [])
        self.assertEqual(result["qualified_api_ids"], [])
        self.assertEqual(result["simd_supplement"]["original_files_pending"], 7)
        self.assertEqual(result["simd_supplement"]["upper_reflection"], "pending")


if __name__ == "__main__":
    unittest.main()
