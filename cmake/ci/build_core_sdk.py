#!/usr/bin/env python3
"""Build a real, explicitly partial SDK candidate and consume its relocated ZIP.

All build and install outputs live outside the source checkout. Cross SDKs are
compiled and linked, not reported as having run on a device. This driver never
relaxes the independent full-migration or full-native release gates.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import core_sdk as sdk


def run(arguments, environment):
    print("+ " + " ".join(map(str, arguments)), flush=True)
    subprocess.run(list(map(str, arguments)), env=environment, check=True)


def ctest(directory, required, environment):
    listing = subprocess.run(["ctest", "--test-dir", str(directory), "-C", "Release",
                              "--show-only=json-v1"], env=environment,
                             check=True, capture_output=True, text=True)
    data = json.loads(listing.stdout)
    sdk.write_json(directory / "ctest-inventory.json", data)
    cases = data.get("tests", [])
    sdk.require(cases, "CTest selected no tests")
    names = {case["name"] for case in cases}
    sdk.require(set(required).issubset(names), "required CTest entries are absent: " +
                ", ".join(sorted(set(required) - names)))
    for case in cases:
        properties = {p["name"]: p["value"] for p in case.get("properties", [])}
        sdk.require(not properties.get("DISABLED", False), "disabled CTest: " + case["name"])
        sdk.require("SKIP_RETURN_CODE" not in properties and "SKIP_REGULAR_EXPRESSION" not in properties,
                    "skippable tests cannot qualify this release candidate: " + case["name"])
    run(["ctest", "--test-dir", directory, "-C", "Release", "--output-on-failure",
         "--no-tests=error", "--timeout", "180", "--output-junit", directory / "ctest-results.xml"], environment)


def cross_options(rid):
    if rid == "android-arm64-v8a":
        ndk = Path(os.environ.get("ANDROID_NDK_HOME", ""))
        toolchain = ndk / "build/cmake/android.toolchain.cmake"
        sdk.require(toolchain.is_file(), "Android NDK toolchain is absent")
        return [f"-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE={toolchain}",
                "-DCMAKE_SYSTEM_NAME=Android", "-DANDROID_ABI=arm64-v8a",
                "-DANDROID_PLATFORM=android-26", "-DANDROID_STL=c++_shared"]
    if rid == "ios-arm64":
        return ["-DCMAKE_SYSTEM_NAME=iOS", "-DCMAKE_OSX_ARCHITECTURES=arm64",
                "-DCMAKE_OSX_SYSROOT=iphoneos", "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
                "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO"]
    return []


def required_native_tests(rid):
    required = {
        "saltscore.simd_original_c11", "saltscore.simd_original_cpp17",
        "saltscore.simd_native_boundaries", "saltscore.all_original_source_provenance",
        "saltscore.all_original_source_negative", "saltscore.migration_evidence_negative",
        "saltscore.mpmc_delivery_contract", "saltscore.original_bucket_mpmc",
        "saltscore.error_native_contract", "saltscore.original_error",
        "saltscore.original_error_source_provenance",
        "saltscore.playback_original_buffer", "saltscore.playback_original_lifecycle",
        "saltscore.playback_production_contract", "saltscore.playback_source_provenance",
        "saltscore.serial_original_c11", "saltscore.serial_public_abi",
        "saltscore.serial_original_fixes", "saltscore.serial_backend_contract",
        "saltscore.serial_storage_ownership", "saltscore.serial_source_provenance",
        "saltscore.usb_original_c11", "saltscore.usb_original_cpp17",
        "saltscore.usb_original_state", "saltscore.usb_lifecycle",
        "saltscore.usb_source_provenance", "saltscore.original_zstd",
        "saltscore.zstd_native_contract", "saltscore.original_zstd_source_provenance",
        "saltscore.capture_source_provenance",
        "crypto.algorithms.sha256_sm3_ed448_rfc8032", "crypto.algorithms.cpp17_header",
        "crypto.asn1.crypto", "crypto.asn1.header_cpp17",
        "crypto.x509.real_der_and_negative", "crypto.x509.migrated_saltsnet_fixture",
        "saltscore.cnet_policy", "saltscore.cnet_tcp_loopback",
        "saltscore.two_cnet_plugins", "saltscore.cnet_tls_ca_san",
        "saltscore.gmssl_tls_io_abi", "saltscore.gmssl_shangmi_provider",
        "saltscore.cnet_owner_original", "saltscore.cnet_native_crypto_owner",
        "saltscore.cnet_native_crypto_owner_negative",
    }
    required.update("saltscore.original_concurrency." + name for name in (
        "disruptor_test", "thread_pool_test", "thread_pool_lf_test",
        "deadline_queue_test", "rcu_test", "typed_primitives_test",
        "concurrency_header_cpp_test", "typed_rcu_cpp_test"))
    if rid == "windows-x64":
        required.update({"capture_windows_test", "capture_exposure_windows_test"})
        required.update({"saltscore.cnet_owner_original_numeric_host",
                         "saltscore.cnet_owner_numeric_host_gate_contract"})
    else:
        required.update({"capture_posix_test", "saltscore.serial_posix_pty"})
    if rid.startswith("linux-"):
        required.add("saltscore.zstd_private_archive")
    return required


def build(args):
    root = Path(__file__).resolve().parents[2]
    config = sdk.profile()
    output = args.output.resolve()
    sdk.require(not output.exists(), "candidate output already exists; use a fresh directory")
    sdk.require(not output.is_relative_to(root), "build output must be outside the source checkout")
    source = sdk.identity(root)
    sdk.require(source["clean"], "candidate CI requires a clean exact source checkout")
    sdk.require(source["commit"] == args.expected_sha, "checkout differs from the requested source SHA")
    sdk.require(not args.sanitize or args.rid == "linux-x64", "sanitizer row must be native Linux x64")
    output.mkdir(parents=True)
    sdk.write_json(output / "source.json", source)
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    if args.sanitize:
        environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1:abort_on_error=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    cross = args.rid in sdk.CROSS_RIDS
    common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
              "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW", "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded",
              f"-DCMAKE_TOOLCHAIN_FILE={args.toolchain}",
              f"-DVCPKG_TARGET_TRIPLET={args.triplet}", f"-DVCPKG_HOST_TRIPLET={args.host_triplet}",
              *cross_options(args.rid)]
    if args.sanitize:
        flags = "-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
        common += ["-DCMAKE_C_FLAGS=" + flags, "-DCMAKE_CXX_FLAGS=" + flags,
                   "-DCMAKE_C_FLAGS_RELEASE=-O1 -g -DNDEBUG",
                   "-DCMAKE_CXX_FLAGS_RELEASE=-O1 -g -DNDEBUG"]
    build_dir, prefix = output / "build", output / "install"
    configured_options = dict(config["candidate_cmake_options"])
    # Preserve the real original Owner gate on every native candidate. Cross
    # profiles build/link installed consumers, without claiming native runs.
    configured_options["SALTSCORE_CNET_OWNER_SOURCE_TESTS"] = "OFF" if cross else "ON"
    options = [f"-D{key}={value}" for key, value in configured_options.items()]
    run(["cmake", "-S", root, "-B", build_dir, *common, *options,
         "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_INSTALL_LIBDIR=lib",
         f"-DCMAKE_INSTALL_PREFIX={prefix}",
         "-DVCPKG_MANIFEST_FEATURES=" + ";".join(config["candidate_vcpkg_features"]),
         "-DBUILD_TESTING=" + ("OFF" if cross else "ON")], environment)
    backend = sdk.native_backend(build_dir, args.rid)
    sdk.write_json(output / "native-backend.json", backend)
    sdk.require(backend["status"] == "selected",
                f"{args.rid}: native backend {backend['status']}; unsupported does not qualify")
    # No selected-target build: every target in the configured graph must build.
    run(["cmake", "--build", build_dir, "--config", "Release", "--parallel", "2"], environment)
    if not cross:
        ctest(build_dir, required_native_tests(args.rid), environment)
    run(["cmake", "--install", build_dir, "--config", "Release"], environment)
    staged = output / "staged"
    stage_args = SimpleNamespace(prefix=prefix, output=staged, rid=args.rid,
                                 identity=output / "source.json", source_root=root,
                                 build_dir=build_dir, execution="unverified")
    sdk.stage(stage_args, config)
    preliminary = output / "consumer-input.zip"
    sdk.archive_sdk(staged, preliminary, config)
    relocated = output / "relocated"
    sdk.extract_archive(preliminary, relocated, config)
    consumer_prefix = relocated / "sdk" / args.rid
    # Exercise the bytes re-extracted from the archive, away from the install
    # tree. Each module's original installed consumers are a separate graph.
    environment["PATH"] = str(consumer_prefix / "bin") + os.pathsep + environment.get("PATH", "")
    for variable in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
        environment[variable] = str(consumer_prefix / "lib") + os.pathsep + environment.get(variable, "")
    consumers = (
        ("foundation", "tests/installed",
         {"sdk.simd_c11", "sdk.simd_cpp17", "sdk.consumer_c", "sdk.consumer_cpp",
          "sdk.error_c11", "sdk.error_cpp17", "sdk.mpmc_c11", "sdk.mpmc_cpp17",
          "sdk.asn1_from_core_c11", "sdk.asn1_from_core_cpp17",
          "sdk.crypto_from_core_c11", "sdk.crypto_from_core_cpp17",
          "sdk.uri_consumer_c", "sdk.uri_consumer_cpp",
          "sdk.cnet_consumer_c", "sdk.cnet_consumer_cpp", "sdk.abi_owner_consumer"}),
        ("fs", "tests/fs-installed", {"saltscore.fs.installed_c", "saltscore.fs.installed_cpp"}),
        ("process", "tests/process-installed",
         {"saltscore.process.installed_c", "saltscore.process.installed_cpp"}),
        ("sctp", "sctp/tests/installed", {"cnet_sctp_installed_consumer"}),
        ("usb", "usb/tests/installed", {"sdk.usb_c11", "sdk.usb_cpp17"}),
        ("playback", "playback/tests/installed", {"sdk.playback_c11", "sdk.playback_cpp17"}),
        ("serial", "salts_serial/test/installed", {"sdk.serial_c11", "sdk.serial_cpp17"}),
        ("capture", "capture/tests/installed", {"sdk.capture_c11", "sdk.capture_cpp17"}),
        ("zstd", "tests/zstd-installed", {"sdk.zstd_c11", "sdk.zstd_cpp17"}),
    )
    for name, relative, required in consumers:
        consumer_dir = output / ("consumer-" + name)
        run(["cmake", "-S", root / relative, "-B", consumer_dir, *common,
             "-DVCPKG_MANIFEST_MODE=OFF", f"-DCMAKE_PREFIX_PATH={consumer_prefix}",
             f"-DSaltsCore_DIR={consumer_prefix / 'lib/cmake/SaltsCore'}",
             "-DSALTSCORE_REQUIRE_SIMD=ON"], environment)
        run(["cmake", "--build", consumer_dir, "--config", "Release", "--parallel", "2"], environment)
        if not cross:
            ctest(consumer_dir, required, environment)
    sdk.require(sdk.identity(root) == source, "source changed during candidate qualification")
    # Record qualification only after the relocated consumer completed successfully.
    manifest = sdk.load_json(staged / sdk.MANIFEST)
    manifest["execution"] = "cross-compiled-consumer" if cross else "native-ctest"
    sdk.write_json(staged / sdk.MANIFEST, manifest)
    archive = output / f"SaltsCore.Native.{config['display_version']}.{args.rid}.zip"
    sdk.archive_sdk(staged, archive, config)
    print(json.dumps({"status": "CANDIDATE_QUALIFIED_FOR_REVIEW", "published": False,
                      "rid": args.rid, "source": source, "archive": str(archive),
                      "release_blockers": sdk.release_blockers(manifest, config)}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rid", choices=sorted(sdk.RIDS), required=True)
    parser.add_argument("--triplet", required=True)
    parser.add_argument("--host-triplet", required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--expected-sha", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    try:
        build(parser.parse_args())
        return 0
    except (sdk.SDKError, OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"SDK candidate failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
