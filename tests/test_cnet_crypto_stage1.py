"""Negative and positive Stage 1 CNet -> Crypto production source admissions."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "cnet_crypto_stage1", ROOT / "tools/audit_cnet_crypto_stage1.py")
assert SPEC is not None and SPEC.loader is not None
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)


def write(root: Path, path: str, content: str) -> None:
    target = root / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def fixture(root: Path) -> None:
    write(root, "CMakeLists.txt", "add_subdirectory(asn1)\nadd_subdirectory(crypto)\n")
    write(root, "asn1/CMakeLists.txt",
          "add_library(salts_asn1 STATIC src/a.c)\n"
          "add_library(Salts::ASN1 ALIAS salts_asn1)\n")
    write(root, "crypto/CMakeLists.txt",
          "find_package(GmSSL CONFIG REQUIRED)\n"
          "add_library(salts_crypto STATIC src/crypto_tls_gmssl.c)\n"
          "add_library(Salts::Crypto ALIAS salts_crypto)\n")
    write(root, "cnet/CMakeLists.txt",
          "add_library(saltscore_cnet SHARED cnet/src/cnet_tls.c)\n"
          "target_link_libraries(saltscore_cnet PRIVATE Salts::Crypto)\n")
    write(root, "cnet/src/cnet_tls.c",
          "#include <crypto/tls.h>\n#include <crypto/cipher_bio.h>\n"
          "int cnet_tls_owner(void) { return 0; }\n")
    for name in gate.HEADERS:
        write(root, "crypto/include/crypto/" + name, "/* required contract */\n")


class Stage1OwnerGate(unittest.TestCase):
    def test_one_private_gmssl_engine_is_valid_stage1_source_shape(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            self.assertEqual(gate.source_errors(root), [])

    def test_one_native_engine_is_valid_source_shape(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            write(root, "crypto/CMakeLists.txt",
                  "add_library(salts_crypto STATIC src/crypto_tls_native.c)\n"
                  "add_library(Salts::Crypto ALIAS salts_crypto)\n")
            self.assertEqual(gate.source_errors(root), [])

    def test_two_engines_rejected_even_with_one_exported_target(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            p = root / "crypto/CMakeLists.txt"
            p.write_text(p.read_text() +
                         "target_sources(salts_crypto PRIVATE src/crypto_tls_native.c)\n")
            self.assertIn("CRYPTO_NOT_EXACTLY_ONE_TLS_ENGINE",
                          gate.source_errors(root))

    def test_direct_cnet_gmssl_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            p = root / "cnet/CMakeLists.txt"
            p.write_text(p.read_text() + "find_package(GmSSL CONFIG REQUIRED)\n"
                         "target_link_libraries(saltscore_cnet PRIVATE GmSSL::GmSSL)\n")
            self.assertIn("PRODUCTION_CNET_DIRECT_THIRD_PARTY_TLS",
                          gate.source_errors(root))

    def test_cnet_foreign_header_is_rejected_even_if_disabled(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            write(root, "cnet/src/cnet_transport.c",
                  "#if 0\n#include <gmssl/tls.h>\n#endif\n")
            self.assertTrue(any(item.startswith("CNET_FOREIGN_TLS_HEADER")
                                for item in gate.source_errors(root)))

    def test_test_only_root_dual_owner_is_not_production(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            p = root / "CMakeLists.txt"
            p.write_text(p.read_text() +
                         "option(SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST \"test\" OFF)\n")
            self.assertIn("ROOT_STILL_TEST_ONLY_DUAL_OWNER",
                          gate.source_errors(root))

    def test_test_only_crypto_link_is_not_production(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            write(root, "cnet/CMakeLists.txt",
                  "add_library(saltscore_cnet SHARED cnet/src/cnet_tls.c)\n"
                  "target_link_libraries(qualification PRIVATE Salts::Crypto)\n")
            self.assertIn("PRODUCTION_CNET_NOT_LINKING_CRYPTO",
                          gate.source_errors(root))

    def test_cmake_comment_does_not_satisfy_crypto_link(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            write(root, "cnet/CMakeLists.txt",
                  "add_library(saltscore_cnet SHARED cnet/src/cnet_tls.c)\n"
                  "# target_link_libraries(saltscore_cnet PRIVATE Salts::Crypto)\n")
            self.assertIn("PRODUCTION_CNET_NOT_LINKING_CRYPTO",
                          gate.source_errors(root))

    def test_second_third_party_tls_engine_forbidden_inside_crypto(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            p = root / "crypto/CMakeLists.txt"
            p.write_text(p.read_text() + "target_link_libraries(salts_crypto PRIVATE OpenSSL::SSL)\n")
            self.assertIn("CRYPTO_ALTERNATE_THIRD_PARTY_TLS_ENGINE",
                          gate.source_errors(root))

    def test_missing_mtls_contract_cannot_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            (root / "crypto/include/crypto/tls_auth.h").unlink()
            self.assertIn("CRYPTO_MISSING_PUBLIC_API tls_auth.h", gate.source_errors(root))

    def test_legacy_backend_state_and_split_target_fail(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            p = root / "cnet/src/cnet_tls.c"
            p.write_text(p.read_text() + "static TLS_CTX *old_ctx;\n")
            p = root / "cnet/CMakeLists.txt"
            p.write_text(p.read_text() +
                         "target_link_libraries(saltscore_cnet PRIVATE Crypto::TLS)\n")
            failures = gate.source_errors(root)
            self.assertIn("PRODUCTION_CNET_OWNS_FOREIGN_TLS_STATE", failures)
            self.assertIn("PRODUCTION_CNET_SPLIT_TLS_TARGET", failures)


if __name__ == "__main__":
    unittest.main()
