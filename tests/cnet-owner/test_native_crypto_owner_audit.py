"""Negative obligations for the production native-link ownership audit."""
from collections import Counter
import unittest

import audit_native_crypto_owner as audit


class NativeCryptoOwnerAudit(unittest.TestCase):
    def test_archive_keeps_duplicate_strong_members(self):
        for system, text in (
            ("Linux", "a.o:\n00000000 T tls_init\nb.o:\n00000000 T tls_init\n"
                      "         U tls_send\n00000000 W weak_helper\n"),
            ("Darwin", "a.o:\n00000000 T _tls_init\nb.o:\n00000000 T _tls_init\n"),
            ("Windows", "001 00000000 SECT1 notype () External | tls_init\n"
                        "002 00000000 SECT2 notype () External | tls_init\n"
                        "003 00000000 UNDEF notype () External | tls_send\n"),
        ):
            with self.subTest(system=system):
                table = audit.archive_definitions(text, system)
                self.assertEqual(table["tls_init"], 2)
                self.assertNotIn("tls_send", table)
        table = Counter({name: 1 for name in audit.ARCHIVE_SYMBOLS})
        audit.validate_archive(table)
        table["tls_init"] = 2
        with self.assertRaisesRegex(ValueError, "duplicated"):
            audit.validate_archive(table)

    def test_real_member_required_in_all_link_map_formats(self):
        for system in ("Linux", "Darwin", "Windows"):
            if system == "Darwin":
                prefix = "[ 1] /build/crypto/libsalts_crypto.a(tls.c.o)\n"
                text = prefix + "\n".join(
                    f"0x1000 0x10 [ 1] _{name}" for name in audit.NATIVE_SYMBOLS)
            elif system == "Windows":
                text = "\n".join(
                    f" 0001:00000010 {name} 0000000140001000 f salts_crypto:tls.c.obj"
                    for name in audit.NATIVE_SYMBOLS)
            else:
                prefix = " .text 0x1000 0x100 /build/crypto/libsalts_crypto.a(tls.c.o)\n"
                text = prefix + "\n".join(
                    f" 0x00001000 {name}" for name in audit.NATIVE_SYMBOLS)
            with self.subTest(system=system):
                audit.validate_runtime_map(text, system)
                with self.assertRaises(ValueError):
                    audit.validate_runtime_map(text.replace("salts_crypto", "gmssl"), system)
                with self.assertRaises(ValueError):
                    audit.validate_runtime_map(text + "\n" + text, system)
                with self.assertRaises(ValueError):
                    audit.validate_runtime_map("", system)

    def test_consumers_cannot_hide_a_second_archive(self):
        audit.validate_peer_map("LOAD libcnet.so\n0x1000 cnet_client_init\n")
        for text in (
            "LOAD crypto/libsalts_crypto.a\n",
            "[ 4] /build/crypto/libsalts_crypto.a(tls.c.o)\n",
            " 0001:00000100 tls_init 0000000140010000 f salts_crypto:tls.obj\n",
            "LOAD /unexpected/libgmssl.a\n",
            "0x1000 tls_do_handshake\n",
        ):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit.validate_peer_map(text)

    def test_external_runtime_rejected_on_each_platform(self):
        for system, text, expected in (
            ("Linux", " (NEEDED) Shared library: [libgmssl.so.3]\n", "libgmssl.so.3"),
            ("Darwin", "image:\n\t/other/libcrypto.3.dylib (compatibility version 3.0.0)\n",
             "libcrypto.3.dylib"),
            ("Windows", "    gmssl.dll\n", "gmssl.dll"),
        ):
            with self.subTest(system=system):
                names = audit.dependencies(text, system)
                self.assertIn(expected, names)
                with self.assertRaises(ValueError):
                    audit.validate_dependencies(names)
        audit.validate_dependencies(["libsaltscore_cnet.so.0.1", "bcrypt.dll", "crypt32.dll"])


if __name__ == "__main__":
    unittest.main()
