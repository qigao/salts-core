"""Test the standalone dependency-closure gate without external packages."""
from pathlib import Path
import importlib.util
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools/check_dependency_boundaries.py"
spec = importlib.util.spec_from_file_location("dependency_gate", SCRIPT)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DependencyGateTest(unittest.TestCase):
    def check_text(self, content: str, suffix: str = ".c") -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ("source" + suffix)
            path.write_text(content, encoding="utf-8")
            return module.check_repository(Path(directory))

    def test_native_historical_symbol_is_not_cmeta_runtime(self):
        self.assertEqual(self.check_text("int cmeta_fs_stat(void *arg);\n"), [])

    def test_legacy_cmake_package_and_target_rejected(self):
        output = self.check_text("find_package(Salts CONFIG REQUIRED)\n"
                                 "target_link_libraries(app PRIVATE Salts::CFlow)\n",
                                 ".cmake")
        self.assertEqual(len(output), 2)

    def test_native_salts_targets_are_allowed(self):
        content = ("add_library(Salts::Platform ALIAS saltscore_platform)\n"
                   "target_link_libraries(app PRIVATE Salts::Core Salts::NativeIO)\n")
        self.assertEqual(self.check_text(content, ".cmake"), [])

    def test_owned_native_extra_targets_allowed_without_old_aliases(self):
        content = ("add_library(Salts::Unicode ALIAS saltscore_unicode)\n"
                   "target_link_libraries(app PRIVATE Salts::IDNA Salts::CSerde Salts::TinyTest)\n")
        self.assertEqual(self.check_text(content, ".cmake"), [])

    def test_canonical_crypto_is_allowed_but_split_tls_is_not(self):
        self.assertEqual(
            self.check_text("target_link_libraries(app PRIVATE Salts::Crypto)\n",
                            ".cmake"), [])
        self.assertEqual(len(self.check_text(
            "target_link_libraries(app PRIVATE Salts::TLS)\n", ".cmake")), 1)
        self.assertEqual(len(self.check_text(
            "target_link_libraries(app PRIVATE Crypto::TLS)\n", ".cmake")), 1)

    def test_single_static_crypto_negative_guard_is_not_a_dependency(self):
        guard = (
            "foreach(_forbidden IN ITEMS\n"
            "  Salts::TLS Salts::TLSCipherBIO Salts::CryptoProvider\n"
            "  Crypto::TLS Crypto::TLSCipherBIO Crypto::Provider)\n"
            "endforeach()\n"
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cmake = root / "tests" / "cnet-crypto-static"
            cmake.mkdir(parents=True)
            guard_file = cmake / "CMakeLists.txt"
            guard_file.write_text(guard, encoding="utf-8")
            self.assertEqual(module.check_repository(root), [])

            # Any real provider link next to negative guard remains forbidden.
            guard_file.write_text(
                guard + "target_link_libraries(app PRIVATE Crypto::TLS)\n",
                encoding="utf-8")
            self.assertEqual(len(module.check_repository(root)), 1)

            # Same deny-list words in another file still trigger scan.
            guard_file.write_text(guard, encoding="utf-8")
            (root / "extra.cmake").write_text(guard, encoding="utf-8")
            self.assertEqual(len(module.check_repository(root)), 1)

            # A renamed guard cannot hide dependencies.
            (root / "extra.cmake").unlink()
            guard_file.write_text(
                guard.replace("foreach(_forbidden IN ITEMS",
                              "set(_forbidden"), encoding="utf-8")
            self.assertEqual(len(module.check_repository(root)), 1)

    def test_deleted_consumer_guard_path_has_no_special_exemption(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cmake = root / "crypto" / "tests" / "mobile"
            cmake.mkdir(parents=True)
            guard_file = cmake / "CMakeLists.txt"
            guard_file.write_text(
                "foreach(_split IN ITEMS Salts::TLS Crypto::Crypto)\n"
                "  if(TARGET ${_split})\n"
                "    message(FATAL_ERROR \"split target present\")\n"
                "  endif()\n"
                "endforeach()\n", encoding="utf-8")
            self.assertEqual(len(module.check_repository(root)), 1)
            guard_file.write_text(
                guard_file.read_text(encoding="utf-8")
                + "target_link_libraries(app PRIVATE Crypto::Crypto)\n",
                encoding="utf-8")
            self.assertEqual(len(module.check_repository(root)), 2)

    def test_canonical_asn1_target_is_allowed(self):
        self.assertEqual(
            self.check_text("target_link_libraries(app PRIVATE Salts::ASN1)\n",
                            ".cmake"), [])

    def test_negative_alternate_target_assertion_is_allowed(self):
        self.assertEqual(
            self.check_text("if(TARGET SaltsCore::ASN1 OR TARGET Crypto::ASN1)\n"
                            "  message(FATAL_ERROR \"legacy alias present\")\n"
                            "endif()\n", ".cmake"), [])

    def test_alternate_namespaces_rejected(self):
        content = ("target_link_libraries(a PRIVATE SaltsCore::Platform)\n"
                   "target_link_libraries(b PRIVATE Crypto::TLS)\n"
                   "target_link_libraries(c PRIVATE SaltsCrypto::Crypto)\n")
        self.assertEqual(len(self.check_text(content, ".cmake")), 3)

    def test_reflection_and_actor_includes_rejected(self):
        self.assertEqual(len(self.check_text("#include <cmeta/struct.h>\n"
                                             "#include <cflow/executor.h>\n"
                                             "#include <cstl.h>\n")), 3)

    def test_metadata_macro_rejected(self):
        self.assertEqual(len(self.check_text("CMETA_STRUCT(vstr, (const char *, data));\n")), 1)


if __name__ == "__main__":
    unittest.main()
