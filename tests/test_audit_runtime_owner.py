"""Focused parser tests for cross-platform installed runtime symbol audits."""
import unittest

from tools.audit_runtime_owner import (
    exports_from_output, is_library, parse_dependencies,
    parse_native_abi_identity,
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

    def test_macho_executable_first_import_is_not_dropped(self):
        data = (
            "/tmp/build/abi_owner_consumer:\n"
            "\t@rpath/libsaltscore_cnet.0.dylib (compatibility version 0.0.0)\n"
            "\t@rpath/libsaltscore_core.0.dylib (compatibility version 0.0.0)\n"
            "\t@rpath/libsaltscore_platform.0.dylib (compatibility version 0.0.0)\n"
        )
        self.assertEqual(
            parse_dependencies(data, "darwin", is_dylib=False),
            {"cnet", "core", "platform"},
        )
        dylib = (
            "/tmp/libsaltscore_cnet.dylib:\n"
            "\t@rpath/libsaltscore_cnet.0.dylib (compatibility version 0.0.0)\n"
            "\t@rpath/libsaltscore_native_io.0.dylib (compatibility version 0.0.0)\n"
        )
        self.assertEqual(
            parse_dependencies(dylib, "darwin", is_dylib=True),
            {"native_io"},
        )

    def test_native_abi_loader_id(self):
        self.assertEqual(
            parse_native_abi_identity(
                " 0x000000000000000e (SONAME) Library soname: [libsaltscore_core.so.0.1]\n",
                "core", "linux"),
            "libsaltscore_core.so.0.1",
        )
        self.assertEqual(
            parse_native_abi_identity(
                "/prefix/lib/libsaltscore_platform.0.1.0.dylib:\n"
                "@rpath/libsaltscore_platform.0.1.dylib\n",
                "platform", "darwin"),
            "@rpath/libsaltscore_platform.0.1.dylib",
        )
        with self.assertRaises(ValueError):
            parse_native_abi_identity(
                " 0x0 (SONAME) Library soname: [libsaltscore_core.so.0]\n",
                "core", "linux",
            )

    def test_platform_specific_installed_images(self):
        self.assertTrue(is_library("libsaltscore_core.so.0.1.0", "core", "linux"))
        self.assertTrue(is_library("libsaltscore_core.0.1.0.dylib", "core", "darwin"))
        self.assertTrue(is_library("saltscore_native_io-0.1.dll", "native_io", "windows"))
        self.assertFalse(is_library("saltscore_native_io.dll", "native_io", "windows"))
        self.assertFalse(is_library("libsaltscore_core_copy.so", "core", "linux"))
        self.assertFalse(is_library("saltscore_core.lib", "core", "windows"))


if __name__ == "__main__":
    unittest.main()
