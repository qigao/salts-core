"""Fail-closed static guard for Issue #79 native M5/M6 local-source graph."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


class M5LocalOwnerGraph(unittest.TestCase):
    def test_exact_first_party_roots(self):
        root = source("CMakeLists.txt")
        self.assertIn("add_subdirectory(asn1)", root)
        self.assertIn("add_subdirectory(crypto)", root)
        self.assertNotIn("SALTSCORE_BUILD_ASN1", root)
        self.assertNotIn("SALTSCORE_BUILD_CRYPTO", root)
        self.assertIn("if(DEFINED CRYPTO_GMSSL_ROOT)", root)
        self.assertIn("if(NOT (BUILD_TESTING AND SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST))", root)
        self.assertIn("Dual TLS source assembly is forbidden", root)

    def test_candidate_requires_local_unimported_single_static_modules(self):
        cmake = source("tests/cnet-crypto-static/CMakeLists.txt")
        self.assertNotRegex(cmake, r"(?m)^\s*find_package\s*\(\s*(?:Crypto|ASN1)\b")
        for local in ("salts_asn1", "salts_crypto"):
            self.assertIn("TARGET " + local, cmake)
            self.assertIn("get_target_property(", cmake)
        self.assertIn("ALIASED_TARGET", cmake)
        self.assertIn('NOT _asn1_type STREQUAL "STATIC_LIBRARY"', cmake)
        self.assertIn('NOT _static_kind STREQUAL "STATIC_LIBRARY"', cmake)
        self.assertIn("_asn1_imported OR _static_imported", cmake)

    def test_candidate_never_links_two_tls_owners(self):
        cmake = source("tests/cnet-crypto-static/CMakeLists.txt")
        for owner in ("saltscore_cnet_crypto_static_original_owner",
                      "saltscore_cnet_crypto_m6_shared"):
            pattern = rf"target_link_libraries\(\s*{owner}\b(.*?)\)"
            match = re.search(pattern, cmake, re.DOTALL)
            self.assertIsNotNone(match, owner)
            linked = match.group(1)
            self.assertIn("Salts::Crypto", linked)
            self.assertNotIn("Salts::CNet", linked)
            self.assertNotIn("GmSSL::GmSSL", linked)
        self.assertIn("Second native Crypto owner forbidden", cmake)

    def test_source_is_local_and_manifest_is_pinned(self):
        workflow = source(".github/workflows/cnet-m5-python-mtls.yml")
        for forbidden in ("qigao/crypto/tarball", "crypto-src", "CRYPTO_SHA"):
            self.assertNotIn(forbidden, workflow)
        for required in (
            "python3 tools/check_crypto_pr43_handoff.py",
            "python tools/check_crypto_pr43_handoff.py",
            "crypto/tools/stage_mtls_gmssl_overlay.py",
            "crypto/tools/audit_static_crypto.py",
            "crypto/tests/sdk/python_mtls.py",
            "crypto/tests/fixtures",
            "CRYPTO_GMSSL_ROOT",
            "VCPKG_INSTALLED_DIR",
        ):
            self.assertIn(required, workflow)

    def test_full_native_m5_m6_differential_is_preserved(self):
        workflow = source(".github/workflows/cnet-m5-python-mtls.yml")
        cmake = source("tests/cnet-crypto-static/CMakeLists.txt")
        for name in ("original_owner_tls12_tls13", "python_san_tls12",
                     "python_san_tls13", "original_owner_python_mtls",
                     "two_plugin_(shared_owner|concurrent_mtls)"):
            self.assertIn(name, workflow)
        for target in ("saltscore_cnet_crypto_static_original_owner",
                       "saltscore_cnet_crypto_m6_shared",
                       "saltscore_cnet_m6_two_plugin_loader"):
            self.assertIn(target, cmake)
        self.assertIn("--repeat until-fail:3", workflow)
        self.assertIn("cnet-m6-host-imports.txt", workflow)

    def test_cnet_source_change_triggers_acceptance(self):
        workflow = source(".github/workflows/cnet-m5-python-mtls.yml")
        for path in ("asn1/**", "crypto/**", "vendor/gmssl-extensions/**",
                     "cnet/src/cnet_tls_crypto_qualification.*"):
            self.assertIn(f'"{path}"', workflow)


if __name__ == "__main__":
    unittest.main()
