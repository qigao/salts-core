"""Negative regressions for real ccache/re2c/vcpkg-cache CI ownership."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "cache_policy", ROOT / "tools/check_ci_native_cache.py")
assert spec and spec.loader
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)

class NativeCacheContractTests(unittest.TestCase):
    def test_all_original_jobs(self):
        self.assertEqual(len(policy.REQUIRED), 13)
        for path in sorted((ROOT / ".github/workflows").glob("*.yml")):
            with self.subTest(path=path.name):
                self.assertEqual(policy.validate_workflow(
                    path.name, path.read_text(encoding="utf-8")), [])

    def test_ccache_omission_is_rejected(self):
        source = (ROOT / ".github/workflows/core-contract.yml").read_text()
        for altered in (
            source.replace("id: ccache", "id: other", 1),
            source.replace("ccache --show-stats", "true", 1),
            source.replace("setup-ccache", "other-launcher", 1),
        ):
            self.assertTrue(policy.validate_workflow("core-contract.yml", altered))

    def test_re2c_unpinned_or_host_installed_is_rejected(self):
        source = (ROOT / ".github/workflows/uri-contract.yml").read_text()
        for altered in (
            source.replace(policy.CACHE_SHA, "master", 1),
            source + "\n          sudo apt-get install -y re2c\n",
            source.replace("  packages: read", "  packages: write", 1),
        ):
            self.assertTrue(policy.validate_workflow("uri-contract.yml", altered))

    def test_vcpkg_binary_cache_not_source_or_nuget(self):
        source = (ROOT / ".github/workflows/cnet-sanitizers.yml").read_text()
        for altered in (
            source.replace("mode: read", "mode: readwrite", 1),
            source.replace("mode: read", "mode: write", 1),
            source.replace(policy.CACHE_SHA, "master", 1),
            source + "\n          VCPKG_BINARY_SOURCES=clear;files,cache,readwrite\n",
            source + "\n          mono-complete\n",
        ):
            self.assertTrue(policy.validate_workflow("cnet-sanitizers.yml", altered))

    def test_extraneous_action_or_cache_mode_is_rejected(self):
        original = (ROOT / ".github/workflows/cnet-provider.yml").read_text()
        for changed in (
            original.replace("setup-re2c-tools@" + policy.CACHE_SHA,
                             "setup-re2c-tools@untrusted-sha", 1),
            original.replace("setup-vcpkg-curl-cache@" + policy.CACHE_SHA,
                             "setup-vcpkg-curl-cache@untrusted-sha", 1),
            original + "\n      - uses: qigao/vcpkg-cache/.github/actions/setup-re2c-tools@bad\n",
            original + "\n      - uses: qigao/vcpkg-cache/.github/actions/setup-vcpkg-curl-cache@bad\n",
            original + "\n          mode: readwrite\n",
        ):
            self.assertTrue(policy.validate_workflow("cnet-provider.yml", changed))

    def test_windows_generator_requires_ccache(self):
        source = (ROOT / ".github/workflows/core-contract.yml").read_text()
        altered = source + "\n          cmake -G 'Visual Studio 18 2026'\n"
        self.assertTrue(policy.validate_workflow("core-contract.yml", altered))

    def test_canonical_ccache_action_is_verified(self):
        path = ROOT / ".github/actions/setup-ccache/action.yml"
        source = path.read_text()
        self.assertEqual(policy.validate_action(source), [])
        self.assertTrue(policy.validate_action(
            source.replace("CMAKE_GENERATOR=Ninja", "CMAKE_GENERATOR=Visual Studio")))
        self.assertTrue(policy.validate_action(
            source.replace("macOS/X64|macOS/ARM64", "macOS/ARM64")))

if __name__ == "__main__":
    unittest.main()
