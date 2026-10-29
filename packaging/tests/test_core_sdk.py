"""Archive/provenance negative tests, not native implementation qualification.

Fixture libraries deliberately contain dummy bytes. Real compiler/loader tests
belong to build_core_sdk.py and the installed consumer suites.
"""
import copy
import importlib.util
import json
from pathlib import Path
import stat
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zipfile


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("core_sdk", ROOT / "cmake/ci/core_sdk.py")
sdk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdk)


class SDKTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config = sdk.profile()
        self.prefix = self.root / "install"
        exports = self.prefix / "lib/cmake/SaltsCore"
        exports.mkdir(parents=True)
        (exports / "SaltsCoreConfigVersion.cmake").write_text('set(PACKAGE_VERSION "0.1.0")\n')
        lines = []
        for target in sorted(sdk.FOUNDATION | {"Salts::SIMD"}):
            kind = "STATIC" if target == "Salts::SIMD" else "SHARED"
            relative = f"lib/{target.split('::')[1]}." + ("a" if kind == "STATIC" else "so")
            (self.prefix / relative).write_bytes(b"structural fixture only\0")
            lines += [f"add_library({target} {kind} IMPORTED)",
                      f"set_target_properties({target} PROPERTIES",
                      f'  IMPORTED_LOCATION_RELEASE "${{_IMPORT_PREFIX}}/{relative}"', "  )"]
        (exports / "SaltsCoreTargets.cmake").write_text("\n".join(lines) + "\n")

    def stage(self, name="staged", rid="linux-x64", **kwargs):
        args = SimpleNamespace(prefix=self.prefix, output=self.root / name, rid=rid,
                               identity=None, source_root=None, build_dir=None,
                               execution="unverified")
        for key, value in kwargs.items():
            setattr(args, key, value)
        sdk.stage(args, self.config)
        return args.output

    def zip(self, staged, name="sdk.zip"):
        path = self.root / name
        sdk.archive_sdk(staged, path, self.config)
        return path

    def test_version_normalization_is_explicit(self):
        self.assertEqual(self.config["display_version"], "0.1.000")
        self.assertEqual(self.config["tag"], "v0.1.000")
        self.assertEqual(self.config["package_version"], "0.1.0")
        bad = copy.deepcopy(self.config)
        bad["package_version"] = "0.1.000"
        path = self.root / "bad.json"
        sdk.write_json(path, bad)
        with self.assertRaisesRegex(sdk.SDKError, "normalized"):
            sdk.profile(path)

    def test_profile_cannot_drop_missing_native_modules_or_rids(self):
        for name in ("Capture", "Playback", "Serial", "USB"):
            with self.subTest(target=name):
                bad = copy.deepcopy(self.config)
                bad["release_deferred"] = []
                bad["release_required_targets"].remove("Salts::" + name)
                path = self.root / "bad.json"
                sdk.write_json(path, bad)
                with self.assertRaisesRegex(sdk.SDKError, "dropped"):
                    sdk.profile(path)
        bad = copy.deepcopy(self.config)
        bad["rids"].remove("android-arm64-v8a")
        sdk.write_json(path, bad)
        with self.assertRaisesRegex(sdk.SDKError, "six RIDs"):
            sdk.profile(path)

    def test_archive_roundtrip_and_deterministic_bytes(self):
        staged = self.stage()
        first, second = self.zip(staged), self.zip(staged, "second.zip")
        self.assertEqual(first.read_bytes(), second.read_bytes())
        extracted = self.root / "extracted"
        sdk.extract_archive(first, extracted, self.config)
        observed = sdk.verify_sdk(extracted / "sdk/linux-x64", self.config)
        self.assertEqual(observed, sdk.verify_sdk(staged, self.config))
        self.assertIsNone(observed["source"])
        self.assertTrue(sdk.release_blockers(observed, self.config))

    def test_changed_payload_rejected(self):
        staged = self.stage()
        (staged / "lib/Core.so").write_bytes(b"tampered")
        with self.assertRaisesRegex(sdk.SDKError, "bytes changed"):
            sdk.verify_sdk(staged, self.config)

    def test_unmanifested_payload_rejected(self):
        staged = self.stage()
        (staged / "surprise.dll").write_bytes(b"unrecorded")
        with self.assertRaisesRegex(sdk.SDKError, "exact manifest"):
            sdk.verify_sdk(staged, self.config)

    def test_missing_imported_binary_rejected(self):
        (self.prefix / "lib/Core.so").unlink()
        with self.assertRaisesRegex(sdk.SDKError, "binary is absent"):
            self.stage()

    def test_shared_simd_is_rejected(self):
        path = self.prefix / "lib/cmake/SaltsCore/SaltsCoreTargets.cmake"
        path.write_text(path.read_text().replace("Salts::SIMD STATIC", "Salts::SIMD SHARED"))
        with self.assertRaisesRegex(sdk.SDKError, "static native"):
            self.stage()

    def test_manifest_duplicate_key_rejected(self):
        path = self.root / "bad.json"
        path.write_text('{"source":null,"source":{"clean":true}}')
        with self.assertRaisesRegex(sdk.SDKError, "duplicate JSON"):
            sdk.load_json(path)

    def test_archive_traversal_collision_and_symlink_rejected(self):
        variants = [["../escape"], ["sdk/a", "sdk/A"], ["sdk/a", "sdk/a/child"],
                    ["sdk/CON.txt"], ["sdk/a."]]
        for index, names in enumerate(variants):
            with self.subTest(names=names):
                path = self.root / f"bad{index}.zip"
                with zipfile.ZipFile(path, "w") as archive:
                    for name in names:
                        archive.writestr(name, "bad")
                with zipfile.ZipFile(path) as archive, self.assertRaises(sdk.SDKError):
                    sdk.archive_entries(archive)
        path = self.root / "link.zip"
        with zipfile.ZipFile(path, "w") as archive:
            entry = zipfile.ZipInfo("sdk/link")
            entry.create_system = 3
            entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(entry, "../../outside")
        with zipfile.ZipFile(path) as archive, self.assertRaisesRegex(sdk.SDKError, "symlinks"):
            sdk.archive_entries(archive)

    def test_external_symlink_rejected_but_internal_soname_is_copied(self):
        external = self.root / "external"
        external.write_bytes(b"outside")
        link = self.prefix / "lib/link.so"
        try:
            link.symlink_to(external)
        except OSError as error:
            self.skipTest(f"OS denied symlink creation: {error}")
        with self.assertRaisesRegex(sdk.SDKError, "escapes"):
            self.stage()
        link.unlink()
        link.symlink_to("Core.so")
        staged = self.stage()
        self.assertFalse((staged / "lib/link.so").is_symlink())
        self.assertEqual((staged / "lib/link.so").read_bytes(), (staged / "lib/Core.so").read_bytes())

    def test_backend_unsupported_is_never_selected(self):
        build = self.root / "build"
        build.mkdir()
        database = build / "compile_commands.json"
        sdk.write_json(database, [{"file": "/source/native-io/src/native_io_unsupported.c"}])
        self.assertEqual(sdk.native_backend(build, "android-arm64-v8a")["status"], "unsupported")
        sdk.write_json(database, [{"file": "native_io_linux.c"}, {"file": "native_io_epoll.c"}])
        self.assertEqual(sdk.native_backend(build, "linux-x64")["status"], "selected")
        self.assertEqual(sdk.native_backend(build, "windows-x64")["status"], "unverified")

    def test_source_identity_change_rejected(self):
        source = {"repository": sdk.REPOSITORY, "commit": "a" * 40, "tree": "b" * 40, "clean": True}
        path = self.root / "identity.json"
        sdk.write_json(path, source)
        with patch.object(sdk, "identity", return_value={**source, "commit": "c" * 40}):
            with self.assertRaisesRegex(sdk.SDKError, "changed during"):
                self.stage(identity=path, source_root=self.root)

    def test_single_rid_cannot_be_six_rid_package(self):
        archive = self.zip(self.stage())
        with self.assertRaisesRegex(sdk.SDKError, "all six RIDs"):
            sdk.inspect_archive(archive, self.config, require_all=True)

    def test_nuspec_must_use_normalized_core_identity(self):
        archive = self.zip(self.stage(), "wrong.nupkg")
        with zipfile.ZipFile(archive, "a") as package:
            package.writestr("SaltsCore.Native.nuspec", "<package><metadata><id>SaltsCore.Native</id>"
                             "<version>0.1.000</version></metadata></package>")
        with self.assertRaisesRegex(sdk.SDKError, "normalized Core"):
            sdk.inspect_archive(archive, self.config, nuget=True)

    def test_deferred_and_dirty_source_block_publication(self):
        with self.assertRaisesRegex(sdk.SDKError, "explicit deferred"):
            sdk.release_check(SimpleNamespace(), self.config)
        cleared = {**self.config, "release_deferred": []}
        with patch.object(sdk, "identity", return_value={"clean": False}):
            with self.assertRaisesRegex(sdk.SDKError, "clean exact"):
                sdk.release_check(SimpleNamespace(source_root=self.root), cleared)

    def test_no_source_or_cross_label_cannot_qualify_native_sdk(self):
        staged = self.stage(execution="cross-compiled-consumer")
        manifest = sdk.verify_sdk(staged, self.config)
        problems = sdk.release_blockers(manifest, {**self.config, "release_deferred": []})
        self.assertTrue(any("Source identity" in item for item in problems))
        self.assertTrue(any("Installed consumer" in item for item in problems))
        self.assertTrue(any("Capture" in item and "Playback" in item for item in problems))

    def test_full_target_manifest_cannot_self_certify_publication(self):
        # Review reproduced that structural ZIP validity, all target names and
        # self-declared execution can describe non-executable fixture bytes.
        # Even after the editable debt list is cleared, this is not admission.
        manifest = {
            "rid": "linux-x64",
            "targets": {name: "SHARED" if name in sdk.FOUNDATION else "STATIC"
                        for name in sdk.RELEASE_TARGET_FLOOR},
            "source": {"repository": sdk.REPOSITORY, "commit": "a" * 40,
                       "tree": "b" * 40, "clean": True},
            "native_backend": {"status": "selected"},
            "execution": "native-ctest",
        }
        problems = sdk.release_blockers(manifest, {**self.config, "release_deferred": []})
        self.assertTrue(any("build/test receipts" in item for item in problems))
        self.assertTrue(any("dependency closure" in item for item in problems))

    def test_publication_reruns_strict_original_migration_gate(self):
        source = {"repository": sdk.REPOSITORY, "commit": "a" * 40, "tree": "b" * 40, "clean": True}
        args = SimpleNamespace(source_root=self.root, expected_sha=source["commit"], tag="v0.1.000")
        failure = SimpleNamespace(returncode=1, stdout="missing files and APIs", stderr="")
        with patch.object(sdk, "identity", return_value=source), \
             patch.object(sdk, "git", return_value=source["commit"]), \
             patch.object(sdk.subprocess, "run", return_value=failure) as invoked:
            with self.assertRaisesRegex(sdk.SDKError, "final migration gate refused"):
                sdk.release_check(args, {**self.config, "release_deferred": []})
            self.assertIn("--require-complete", invoked.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
