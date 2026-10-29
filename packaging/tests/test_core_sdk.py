"""Archive/provenance negative tests, not native implementation qualification.

Fixture libraries deliberately contain dummy bytes. Real compiler/loader tests
belong to build_core_sdk.py and the installed consumer suites.
"""
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
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

    def test_soname_chain_retains_one_runtime_owner_through_archive(self):
        # Real three-name CMake SONAME chain. Equal regular-file bytes are not
        # one owner: the downstream audit deliberately compares resolve().
        library = self.prefix / "lib/libsaltscore_core.so.0.1.0"
        (self.prefix / "lib/Core.so").rename(library)
        exports = self.prefix / "lib/cmake/SaltsCore/SaltsCoreTargets.cmake"
        exports.write_text(exports.read_text().replace("lib/Core.so", "lib/libsaltscore_core.so.0.1.0"))
        (self.prefix / "lib/libsaltscore_core.so.0.1").symlink_to(library.name)
        (self.prefix / "lib/libsaltscore_core.so").symlink_to("libsaltscore_core.so.0.1")
        staged = self.stage()

        def assert_one_owner(prefix):
            names = [prefix / "lib" / name for name in
                     ("libsaltscore_core.so", "libsaltscore_core.so.0.1", library.name)]
            self.assertTrue(names[0].is_symlink())
            self.assertTrue(names[1].is_symlink())
            self.assertFalse(names[2].is_symlink())
            self.assertEqual(len({path.resolve(strict=True) for path in names}), 1)
            self.assertTrue(all(path.samefile(names[2]) for path in names))

        assert_one_owner(staged)
        archive = self.zip(staged)
        with zipfile.ZipFile(archive) as package:
            entry = package.getinfo("sdk/linux-x64/lib/libsaltscore_core.so")
            self.assertTrue(stat.S_ISLNK(entry.external_attr >> 16))
            self.assertEqual(package.read(entry), b"libsaltscore_core.so.0.1")
        extracted = self.root / "extracted"
        sdk.extract_archive(archive, extracted, self.config)
        assert_one_owner(extracted / "sdk/linux-x64")
        manifest = sdk.verify_sdk(extracted / "sdk/linux-x64", self.config)
        self.assertIn("lib/libsaltscore_core.so.0.1.0", manifest["imported_binaries"])
        rows = {row["path"]: row for row in manifest["files"]}
        link = rows["lib/libsaltscore_core.so"]
        self.assertEqual(manifest["schema"], 2)
        self.assertEqual(link["kind"], "symlink")
        self.assertEqual(link["target"], "libsaltscore_core.so.0.1")
        self.assertEqual(link["sha256"], sdk.digest(b"libsaltscore_core.so.0.1"))
        self.assertEqual(link["size"], len(b"libsaltscore_core.so.0.1"))
        self.assertEqual(rows["lib/libsaltscore_core.so.0.1.0"]["kind"], "file")

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

    def test_external_and_directory_symlinks_rejected_before_staging(self):
        external = self.root / "external"
        external.write_bytes(b"outside")
        link = self.prefix / "lib/link.so"
        link.symlink_to(external)
        with self.assertRaisesRegex(sdk.SDKError, "absolute"):
            self.stage()
        self.assertFalse((self.root / "staged").exists())
        link.unlink()
        link.symlink_to("../lib", target_is_directory=True)
        with self.assertRaisesRegex(sdk.SDKError, "directory symlinks"):
            self.stage()
        self.assertFalse((self.root / "staged").exists())

    def test_internal_relative_parent_link_is_preserved(self):
        link = self.prefix / "lib/link.so"
        link.symlink_to("../lib/Core.so")
        staged = self.stage()
        self.assertEqual(os.readlink(staged / "lib/link.so"), "../lib/Core.so")
        self.assertTrue((staged / "lib/link.so").samefile(staged / "lib/Core.so"))
        extracted = self.root / "extracted"
        sdk.extract_archive(self.zip(staged), extracted, self.config)
        prefix = extracted / "sdk/linux-x64"
        self.assertTrue((prefix / "lib/link.so").samefile(prefix / "lib/Core.so"))

    def test_source_cycles_and_dangling_links_rejected_before_staging(self):
        first, second = self.prefix / "lib/first.so", self.prefix / "lib/second.so"
        first.symlink_to(second.name)
        with self.assertRaisesRegex(sdk.SDKError, "dangling"):
            self.stage()
        self.assertFalse((self.root / "staged").exists())
        second.symlink_to(first.name)
        with self.assertRaisesRegex(sdk.SDKError, "cyclic"):
            self.stage()
        self.assertFalse((self.root / "staged").exists())

    def test_manifest_distinguishes_symlink_from_equal_regular_file_bytes(self):
        (self.prefix / "lib/link.so").symlink_to("Core.so")
        staged = self.stage()
        link = staged / "lib/link.so"
        data = link.read_bytes()
        link.unlink()
        link.write_bytes(data)
        with self.assertRaisesRegex(sdk.SDKError, "entry kind changed"):
            sdk.verify_sdk(staged, self.config)
        # The reverse substitution also fails even with unchanged file bytes.
        other = self.stage("other")
        core = other / "lib/Core.so"
        shutil.copyfile(core, other / "lib/SIMD.a")
        core.unlink()
        core.symlink_to("SIMD.a")
        with self.assertRaisesRegex(sdk.SDKError, "entry kind changed"):
            sdk.verify_sdk(other, self.config)

    def test_archive_link_graph_rejected_before_any_extraction(self):
        base = "sdk/linux-x64/lib/"
        regular = (base + "Core.so", stat.S_IFREG, b"library")
        variants = {
            "absolute": ([(base + "link.so", stat.S_IFLNK, b"/etc/passwd")], "absolute"),
            "escape": ([(base + "link.so", stat.S_IFLNK, b"../../outside")], "escapes"),
            "cross-rid": ([(base + "link.so", stat.S_IFLNK, b"../../ios-arm64/lib/other"),
                           ("sdk/ios-arm64/lib/other", stat.S_IFREG, b"other")], "escapes"),
            "dangling": ([(base + "link.so", stat.S_IFLNK, b"absent")], "dangling"),
            "cycle": ([(base + "a", stat.S_IFLNK, b"b"),
                       (base + "b", stat.S_IFLNK, b"a")], "cyclic"),
            "self-cycle": ([(base + "a", stat.S_IFLNK, b"a")], "cyclic"),
            "parent-link": ([(base + "a", stat.S_IFLNK, b"Core.so"),
                             (base + "a/child", stat.S_IFREG, b"child")], "parent directory"),
            "directory": ([(base + "a", stat.S_IFLNK, b"sub"),
                           (base + "sub/child", stat.S_IFREG, b"child")], "directory SDK symlink"),
            "missing-parent": ([(base + "a", stat.S_IFLNK, b"absent/../Core.so")], "missing"),
            "file-parent": ([(base + "a", stat.S_IFLNK, b"Core.so/../Core.so")], "non-directory"),
            "backslash": ([(base + "a", stat.S_IFLNK, b"..\\Core.so")], "unsafe"),
            "drive": ([(base + "a", stat.S_IFLNK, b"C:/Core.so")], "unsafe"),
            "nul": ([(base + "a", stat.S_IFLNK, b"Core.so\0")], "unsafe"),
            "non-utf8": ([(base + "a", stat.S_IFLNK, b"\xff")], "UTF-8"),
            "metadata": ([("SaltsCore.Native.nuspec", stat.S_IFLNK, b"sdk/linux-x64/lib/Core.so")], "canonical SDK root"),
            "manifest": ([("sdk/linux-x64/" + sdk.MANIFEST, stat.S_IFLNK, b"lib/Core.so")], "manifest must"),
            "directory-flag": ([(base + "a/", stat.S_IFLNK, b"Core.so")], "conflicting entry type"),
        }
        for name, (rows, message) in variants.items():
            with self.subTest(name=name):
                path = self.root / (name + ".zip")
                with zipfile.ZipFile(path, "w") as archive:
                    for entry_name, kind, content in [regular, *rows]:
                        entry = zipfile.ZipInfo(entry_name)
                        entry.create_system = 3
                        entry.external_attr = (kind | 0o777) << 16
                        archive.writestr(entry, content)
                destination = self.root / (name + "-out")
                with self.assertRaisesRegex(sdk.SDKError, message):
                    sdk.extract_archive(path, destination, self.config)
                self.assertFalse(destination.exists())

    def test_zip_entry_kind_and_manifest_target_must_both_match(self):
        (self.prefix / "lib/link.so").symlink_to("Core.so")
        staged = self.stage()
        original = self.zip(staged)
        for variant in ("flattened", "link-text-as-regular", "different-target"):
            with self.subTest(variant=variant):
                changed = self.root / (variant + ".zip")
                with zipfile.ZipFile(original) as source, zipfile.ZipFile(changed, "w") as target:
                    for entry in source.infolist():
                        data = source.read(entry)
                        if entry.filename == "sdk/linux-x64/lib/link.so":
                            if variant != "different-target":
                                entry.external_attr = (stat.S_IFREG | 0o644) << 16
                                if variant == "flattened":
                                    data = (staged / "lib/Core.so").read_bytes()
                            else:
                                data = b"SIMD.a"
                        target.writestr(entry, data)
                destination = self.root / (variant + "-out")
                with self.assertRaisesRegex(sdk.SDKError, "entry kind changed|target metadata changed"):
                    sdk.extract_archive(changed, destination, self.config)
                self.assertFalse(destination.exists())

    def test_nuget_preserves_link_records_or_fails_closed(self):
        (self.prefix / "lib/link.so").symlink_to("Core.so")
        archive = self.zip(self.stage(), "linked.nupkg")
        with zipfile.ZipFile(archive, "a") as package:
            package.writestr("SaltsCore.Native.nuspec", "<package><metadata><id>SaltsCore.Native</id>"
                             "<version>0.1.0</version></metadata></package>")
        sdk.inspect_archive(archive, self.config, nuget=True)
        sdk.extract_archive(archive, self.root / "nuget-out", self.config, nuget=True)
        prefix = self.root / "nuget-out/sdk/linux-x64"
        self.assertTrue((prefix / "lib/link.so").samefile(prefix / "lib/Core.so"))
        # Model a packer following the SDK symlink while retaining its manifest.
        flattened = self.root / "flattened.nupkg"
        with zipfile.ZipFile(archive) as source, zipfile.ZipFile(flattened, "w") as target:
            for entry in source.infolist():
                data = source.read(entry)
                if stat.S_ISLNK(entry.external_attr >> 16):
                    entry.external_attr = (stat.S_IFREG | 0o644) << 16
                    data = (prefix / "lib/Core.so").read_bytes()
                target.writestr(entry, data)
        with self.assertRaisesRegex(sdk.SDKError, "entry kind changed"):
            sdk.inspect_archive(flattened, self.config, nuget=True)

    def test_old_untyped_manifest_is_not_reinterpreted(self):
        staged = self.stage()
        manifest = sdk.load_json(staged / sdk.MANIFEST)
        manifest["schema"] = 1
        sdk.write_json(staged / sdk.MANIFEST, manifest)
        with self.assertRaisesRegex(sdk.SDKError, "unsupported SDK manifest schema"):
            sdk.verify_sdk(staged, self.config)

    def test_symlink_permission_failure_does_not_fall_back_to_regular_files(self):
        (self.prefix / "lib/link.so").symlink_to("Core.so")
        archive = self.zip(self.stage())
        destination = self.root / "no-symlink-privilege"
        with patch.object(Path, "symlink_to", side_effect=OSError("symlink permission denied")):
            with self.assertRaisesRegex(OSError, "permission denied"):
                sdk.extract_archive(archive, destination, self.config)
        self.assertFalse((destination / "sdk/linux-x64/lib/link.so").exists())

    def test_windows_regular_zip_attributes_are_supported_but_ambiguous_links_are_not(self):
        staged = self.stage()
        original = self.zip(staged)
        windows = self.root / "windows-created.zip"
        with zipfile.ZipFile(original) as source, zipfile.ZipFile(windows, "w") as target:
            for entry in source.infolist():
                entry.create_system = 0
                entry.external_attr = 0x20  # DOS archive bit, no Unix file type.
                target.writestr(entry, source.read(entry))
        sdk.extract_archive(windows, self.root / "windows-created", self.config)
        for name, creator, attributes in (("dos-directory-link", 3, 0x10),
                                           ("non-unix-link", 0, 0)):
            with self.subTest(name=name):
                path = self.root / (name + ".zip")
                with zipfile.ZipFile(path, "w") as archive:
                    entry = zipfile.ZipInfo("sdk/linux-x64/lib/link.so")
                    entry.create_system = creator
                    entry.external_attr = ((stat.S_IFLNK | 0o777) << 16) | attributes
                    archive.writestr(entry, "Core.so")
                destination = self.root / name
                with self.assertRaisesRegex(sdk.SDKError, "conflicting platform/directory"):
                    sdk.extract_archive(path, destination, self.config)
                self.assertFalse(destination.exists())

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
