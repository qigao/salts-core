"""Unit tests for the sole TLS provider migration admission gate."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCRIPT=ROOT/"tools"/"audit_single_tls_owner.py"
spec=importlib.util.spec_from_file_location("sole_tls_owner",SCRIPT)
module=importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)

def write(root: Path, path: str, text: str) -> None:
    p=root/path
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(text,encoding="utf-8")

def valid_fixture(root: Path):
    write(root,"CMakeLists.txt",
        "add_subdirectory(asn1)\nadd_subdirectory(crypto)\n")
    write(root,"asn1/CMakeLists.txt",
        "add_library(salts_asn1 STATIC src/reader.c)\n"
        "add_library(Salts::ASN1 ALIAS salts_asn1)\n")
    write(root,"cnet/CMakeLists.txt",
        "add_library(saltscore_cnet SHARED cnet/src/cnet_tls.c)\n"
        "target_link_libraries(saltscore_cnet PRIVATE Salts::Crypto)\n")
    write(root,"cnet/src/cnet_tls.c",
        "#include <crypto/tls.h>\n#include <crypto/cipher_bio.h>\n"
        "int native_tls_owner(void) { return 0; }\n")
    write(root,"crypto/CMakeLists.txt",
        "add_library(salts_crypto STATIC src/crypto_tls_native.c)\n"
        "add_library(Salts::Crypto ALIAS salts_crypto)\n")
    write(root,"crypto/src/crypto_tls_native.c",
        "#include <crypto/tls.h>\n")
    for name in module.TLS_HEADERS:
        write(root,name,"/* real compiled provider API belongs here */\n")

class SoleTLSOwnerGateTests(unittest.TestCase):
    def test_native_one_owner_fixture_passes_source_only(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path)
            valid_fixture(root)
            self.assertEqual(module.source_errors(root),[])

    def test_external_crypto_package_is_not_a_source_owner(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path)
            valid_fixture(root)
            f = root / "cnet/CMakeLists.txt"
            f.write_text(f.read_text() +
                "find_package(Crypto 0.1 CONFIG REQUIRED)\n")
            self.assertIn("CNET_USES_EXTERNAL_CRYPTO_PACKAGE",
                          module.source_errors(root))

    def test_missing_local_crypto_module_is_blocked(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path)
            valid_fixture(root)
            (root / "CMakeLists.txt").write_text(
                "add_subdirectory(asn1)\n")
            self.assertIn("ROOT_NOT_BUILDING_NATIVE_CRYPTO",
                          module.source_errors(root))

    def test_cnet_direct_gmssl_cannot_be_hidden_under_crypto_alias(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path)
            valid_fixture(root)
            p=root/"cnet/CMakeLists.txt"
            p.write_text(p.read_text()+
                "target_link_libraries(saltscore_cnet PRIVATE GmSSL::GmSSL)\n")
            self.assertIn("CNET_DIRECT_THIRD_PARTY_TLS_LINK",
                          module.source_errors(root))

    def test_backend_gmssl_inside_static_crypto_is_still_third_party(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path)
            valid_fixture(root)
            p=root/"crypto/CMakeLists.txt"
            p.write_text(p.read_text()+
                "find_package(GmSSL CONFIG REQUIRED)\n"
                "target_sources(salts_crypto PRIVATE src/crypto_tls_gmssl.c)\n")
            errs=module.source_errors(root)
            self.assertIn("CRYPTO_THIRD_PARTY_TLS_PACKAGE",errs)
            self.assertIn("CRYPTO_GMSSL_BACKEND_STILL_COMPILED",errs)

    def test_gmssl_header_forbidden_even_under_disabled_branch(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path)
            valid_fixture(root)
            write(root,"cnet/src/cnet_transport.c",
                "#if 0\n#include <gmssl/tls.h>\n#endif\n")
            self.assertTrue(any(e.startswith(
                "CNET_PRIVATE_THIRD_PARTY_HEADER") for e in
                module.source_errors(root)))

    def test_missing_mtls_auth_api_cannot_be_qualified(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path)
            valid_fixture(root)
            (root/"crypto/include/crypto/tls_auth.h").unlink()
            self.assertIn("CRYPTO_MISSING_VERSIONED_TLS_API "
                "crypto/include/crypto/tls_auth.h",module.source_errors(root))

    def test_split_provider_is_forbidden(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path)
            valid_fixture(root)
            p=root/"cnet/CMakeLists.txt"
            p.write_text(p.read_text()+
                "target_link_libraries(saltscore_cnet PRIVATE Crypto::TLS)\n")
            self.assertIn("CNET_SPLIT_TLS_PROVIDER",module.source_errors(root))

    def test_real_source_baseline_must_not_be_misreported_as_cutover(self):
        errors=module.source_errors(ROOT)
        self.assertIn("CNET_DIRECT_THIRD_PARTY_TLS_LINK",errors)
        self.assertIn("CRYPTO_GMSSL_BACKEND_STILL_COMPILED",errors)
        self.assertIn("CRYPTO_MISSING_VERSIONED_TLS_API "
            "crypto/include/crypto/tls_auth.h",errors)

if __name__=="__main__":
    unittest.main()
