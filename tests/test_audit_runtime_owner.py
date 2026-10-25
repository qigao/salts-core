"""Focused parser tests for cross-platform installed runtime symbol audits."""
import unittest

from tools.audit_runtime_owner import (
    binary_dependencies, exports_from_output, is_library,
)


class AbiOwnerAuditTest(unittest.TestCase):
    def test_elf_defined_exports(self):
        values = exports_from_output(
            "00000000000010d0 T mem_global\n"
            "0000000000001080 W cmeta_mutex_init@@SALTS_1\n"
            "                 U cmeta_thread_current_token\n", "linux"
        )
        self.assertIn("mem_global", values)
        self.assertIn("cmeta_mutex_init", values)
        self.assertNotIn("cmeta_thread_current_token", values)

    def test_macho_external_defined_exports(self):
        values = exports_from_output(
            "0000000000005e00 T _mem_global\n"
            "0000000000005a00 T _cmeta_thread_current_token\n", "darwin"
        )
        self.assertEqual(values, {"mem_global", "cmeta_thread_current_token"})

    def test_pe_export_table(self):
        values = exports_from_output(
            "    ordinal hint RVA      name\n"
            "          1    0 00001220 mem_global\n"
            "          2    1 0000A123 cmeta_thread_current_token\n",
            "windows",
        )
        self.assertEqual(values, {"mem_global", "cmeta_thread_current_token"})

    def test_platform_specific_installed_images(self):
        self.assertTrue(is_library("libsaltscore_core.so.0.1.0", "core", "linux"))
        self.assertTrue(is_library("libsaltscore_core.0.1.0.dylib", "core", "darwin"))
        self.assertTrue(is_library("saltscore_native_io.dll", "native_io", "windows"))
        self.assertFalse(is_library("libsaltscore_core_copy.so", "core", "linux"))
        self.assertFalse(is_library("saltscore_core.lib", "core", "windows"))


if __name__ == "__main__":
    unittest.main()
