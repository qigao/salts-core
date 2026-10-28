"""Regression tests for CNet pinned-provider CI trigger ownership."""
from pathlib import Path
import unittest

from tools import check_cnet_ci_triggers as gate


class CNetTriggerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.workflow = gate.PROVIDER_WORKFLOW.read_text(encoding="utf-8")
        cls.cmake = gate.CNET_CMAKE.read_text(encoding="utf-8")

    def test_real_workflow_covers_linked_modules(self) -> None:
        expected = gate.validate(self.workflow, self.cmake)
        self.assertTrue(gate.REQUIRED_BASE <= expected)
        self.assertEqual(
            gate.linked_modules(self.cmake),
            {"core", "platform", "native-io", "concurrency", "uri"},
        )

    def test_removing_any_source_or_header_glob_is_rejected(self) -> None:
        for module in gate.linked_modules(self.cmake) | {"cnet"}:
            for kind in ("src", "include"):
                pattern = f'{module}/{kind}/**'
                with self.subTest(pattern=pattern):
                    old_line = f'      - "{pattern}"\n'
                    self.assertIn(old_line, self.workflow)
                    edited = self.workflow.replace(old_line, "", 1)
                    with self.assertRaisesRegex(ValueError, "triggers missing"):
                        gate.validate(edited, self.cmake)

    def test_parser_and_cmake_dependency_change_are_required(self) -> None:
        for pattern in ("uri/parser/**", "cmake/**", "vendor/CMakeLists.txt",
                        "cnet/CMakeLists.txt",
                        "core/CMakeLists.txt", "platform/CMakeLists.txt",
                        "native-io/CMakeLists.txt", "concurrency/CMakeLists.txt",
                        "uri/CMakeLists.txt"):
            with self.subTest(pattern=pattern):
                edited = self.workflow.replace(f'      - "{pattern}"\n', "", 1)
                with self.assertRaisesRegex(ValueError, "triggers missing"):
                    gate.validate(edited, self.cmake)

    def test_retired_root_path_does_not_satisfy_native_io(self) -> None:
        edited = self.workflow.replace(
            '      - "native-io/src/**"',
            '      - "src/native-io/**"',
            1,
        )
        with self.assertRaisesRegex(ValueError, "native-io/src"):
            gate.validate(edited, self.cmake)

    def test_unknown_future_salts_dependency_fails_closed(self) -> None:
        altered = self.cmake.replace(
            "    Salts::NativeIO\n",
            "    Salts::NativeIO\n    Salts::HypotheticalNewModule\n",
            1,
        )
        self.assertNotEqual(altered, self.cmake)
        with self.assertRaisesRegex(ValueError, "unmapped CNet linked owner"):
            gate.validate(self.workflow, altered)

    def test_negative_glob_fails_closed(self) -> None:
        altered = self.workflow.replace(
            '      - "cnet/src/**"',
            '      - "cnet/src/**"\n      - "!native-io/src/**"',
            1,
        )
        with self.assertRaisesRegex(ValueError, "negated CNet trigger"):
            gate.validate(altered, self.cmake)

    def test_push_paths_alone_are_not_accepted(self) -> None:
        only_push = 'on:\n  push:\n    paths:\n      - "native-io/src/**"\n'
        with self.assertRaisesRegex(ValueError, "missing on.pull_request.paths"):
            gate.pull_request_paths(only_push)


if __name__ == "__main__":
    unittest.main()
