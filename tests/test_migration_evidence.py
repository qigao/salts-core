"""Evidence integrity regressions; fixture reports never qualify production."""
from contextlib import contextmanager
from copy import deepcopy
import importlib.util
from itertools import permutations
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

    def test_codemodel_selects_primary_binary_with_pdb_and_import_outputs(self):
        # Windows File API reports sidecars as well as the executable. Capture
        # the declared primary file in every order, never whichever comes first.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            reply = build / ".cmake/api/v1/reply"
            reply.mkdir(parents=True)
            (build / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\n")
            name = "saltscore_simd_original_c11"
            primary = f"bin/{name}.exe"
            outputs = {primary: b"MZprimary executable fixture",
                       f"bin/{name}.pdb": b"Microsoft C/C++ MSF 7.00 debug fixture",
                       f"lib/{name}.lib": b"!<arch>\nimport-library fixture"}
            for relative, data in outputs.items():
                path = build / relative
                path.parent.mkdir(exist_ok=True)
                path.write_bytes(data)
            documents = {
                "index-test.json": {"objects": [
                    {"kind": kind, "jsonFile": kind + ".json"}
                    for kind in ("codemodel", "toolchains")]},
                "codemodel.json": {"paths": {"source": str(ROOT)},
                    "configurations": [{"name": "Release", "targets": [
                        {"name": name, "jsonFile": "target.json"}]}]},
                "toolchains.json": {"toolchains": []},
            }
            for filename, data in documents.items():
                (reply / filename).write_text(json.dumps(data))
            for index, order in enumerate(permutations(outputs)):
                target = {"name": name, "type": "EXECUTABLE", "nameOnDisk": name + ".exe",
                          "artifacts": [{"path": path} for path in order]}
                (reply / "target.json").write_text(json.dumps(target))
                bundle = root / f"bundle-{index}"
                captured = evidence.capture_build(
                    build, ROOT, "Release", {name: evidence.NATIVE_TARGETS[name]}, bundle, "native")
                record = captured["targets"][name]["artifact"]
                self.assertEqual(record["path"], f"artifacts/native/{name}/{name}.exe")
                self.assertEqual(evidence.checked_file(bundle, record).read_bytes(), outputs[primary])

    def test_primary_artifact_rejects_missing_or_ambiguous_metadata_across_hosts(self):
        target = {"name": "consumer", "type": "EXECUTABLE", "nameOnDisk": "consumer.exe",
                  "artifacts": [{"path": r"bin\consumer.pdb"},
                                {"path": r"bin\consumer.exe"}, {"path": r"lib\consumer.lib"}]}
        build = r"D:\candidate\build"
        self.assertEqual(evidence.primary_artifact(target, "EXECUTABLE", build),
                         "D:/candidate/build/bin/consumer.exe")
        absolute = deepcopy(target)
        absolute["artifacts"][1]["path"] = r"E:\output\consumer.exe"
        self.assertEqual(evidence.primary_artifact(absolute, "EXECUTABLE", build),
                         "E:/output/consumer.exe")
        archive = deepcopy(target)
        archive.update(type="STATIC_LIBRARY", nameOnDisk="consumer.lib")
        self.assertEqual(evidence.primary_artifact(archive, "STATIC_LIBRARY", build),
                         "D:/candidate/build/lib/consumer.lib")
        missing_name, missing_file, ambiguous = (deepcopy(target) for _ in range(3))
        del missing_name["nameOnDisk"]
        missing_file["artifacts"].pop(1)
        ambiguous["artifacts"].append({"path": "elsewhere/consumer.exe"})
        for changed in (missing_name, missing_file, ambiguous):
            with self.subTest(target=changed), self.assertRaisesRegex(ValueError, "primary"):
                evidence.primary_artifact(changed, "EXECUTABLE", build)

    def test_retained_binary_and_ctest_cannot_substitute_a_sidecar_or_other_executable(self):
        # Synthetic native records reach rejection boundaries only; they do not
        # qualify Windows execution or bypass any complete receipt verification.
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory)
            data = early_receipt()
            data.update(profile="windows-release", host={"system": "Windows", "machine": "AMD64"})
            build = data["builds"]["native"]
            build["build_root"] = r"D:\candidate\build"
            for name, record in build["targets"].items():
                static = record["codemodel"]["type"] == "STATIC_LIBRARY"
                filename = name + (".lib" if static else ".exe")
                record["codemodel"].update(nameOnDisk=filename, artifacts=[
                    {"path": "bin/" + name + ".pdb"}, {"path": "bin/" + filename}])
                relative = f"artifacts/native/{name}/{filename}"
                path = bundle / relative
                path.parent.mkdir(parents=True)
                path.write_bytes(b"!<arch>\narchive fixture" if static else b"MZexecutable fixture")
                record["artifact"] = {"path": relative, "sha256": evidence.digest(path.read_bytes())}
            name = "saltscore_simd_original_c11"
            receipt = bundle / "receipt.json"
            forged = deepcopy(data)
            original = bundle / forged["builds"]["native"]["targets"][name]["artifact"]["path"]
            substitute = original.with_name("other.exe")
            substitute.write_bytes(original.read_bytes())
            forged["builds"]["native"]["targets"][name]["artifact"]["path"] = substitute.relative_to(bundle).as_posix()
            receipt.write_text(json.dumps(forged))
            with self.assertRaisesRegex(ValueError, "retained artifact differs"):
                evidence.verify_receipt(ROOT, receipt, data["source"])
            sidecar = deepcopy(data)
            record = sidecar["builds"]["native"]["targets"][name]
            record["codemodel"]["nameOnDisk"] = name + ".pdb"
            path = original.with_suffix(".pdb")
            path.write_bytes(b"Microsoft C/C++ MSF 7.00 debug fixture")
            record["artifact"] = {"path": path.relative_to(bundle).as_posix(),
                                  "sha256": evidence.digest(path.read_bytes())}
            receipt.write_text(json.dumps(sidecar))
            with self.assertRaisesRegex(ValueError, "not a native executable"):
                evidence.verify_receipt(ROOT, receipt, data["source"])
            test = "saltscore.simd_original_c11"
            data["tests"]["native"] = {key: {} for key in evidence.TESTS["native"]}
            inventory = bundle / "inventory.json"
            for executable in (name + ".pdb", name + ".lib", "other.exe"):
                inventory.write_text(json.dumps({"tests": [{"name": test,
                    "command": ["D:\\candidate\\build\\bin\\" + executable]}]}))
                data["tests"]["native"][test] = {"inventory": {
                    "path": inventory.name, "sha256": evidence.digest(inventory.read_bytes())}}
                receipt.write_text(json.dumps(data))
                with self.subTest(executable=executable), self.assertRaisesRegex(ValueError, "CTest does not execute"):
                    evidence.verify_receipt(ROOT, receipt, data["source"])

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
