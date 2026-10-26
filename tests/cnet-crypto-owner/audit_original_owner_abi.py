#!/usr/bin/env python3
"""Fail-closed source and object admission for ORIGINAL CNet Owner TLS calls.

This is NOT a proof of a linked or functioning CNet Owner DSO. It proves the
unchanged original Owner can compile next to the test-only opaque Crypto TLS
implementation without a direct GmSSL header, and its entire currently used
private cnet_tls_* symbol contract is defined by that candidate. A subsequent
separate qualification must link the full graph and run session commands.
"""
import argparse
from pathlib import Path
import re
import sys

EXPECTED_OWNER_CALLS = {
    "cnet_tls_cipher_input_capacity",
    "cnet_tls_context_release",
    "cnet_tls_feed_cipher",
    "cnet_tls_get_negotiated_alpn",
    "cnet_tls_handshake",
    "cnet_tls_probe_peer_close",
    "cnet_tls_read",
    "cnet_tls_shutdown",
    "cnet_tls_state_destroy",
    "cnet_tls_state_export_channel_binding",
    "cnet_tls_state_handshake_complete",
    "cnet_tls_state_init",
    "cnet_tls_state_io_buffer_bytes",
    "cnet_tls_state_negotiated_cipher",
    "cnet_tls_state_negotiated_version",
    "cnet_tls_state_peer_certificate_sha256",
    "cnet_tls_state_read_buffer",
    "cnet_tls_state_server_end_point_binding",
    "cnet_tls_state_write_buffer",
    "cnet_tls_take_cipher",
    "cnet_tls_write",
}
# This intentionally omits private factory functions that the ORIGINAL
# CNet client/server policy still consumes. They remain a separate hard
# blocker to link or switch original CNet's complete module and runtime.


def required_calls(code: str) -> set[str]:
    return set(re.findall(r"\b(cnet_tls_[A-Za-z0-9_]+)\s*\(", code))


def native_definitions(code: str) -> set[str]:
    # Actual function definitions start with a C11 return type. The
    # ending '{' must occur before a ';', ruling out mere call references.
    output: set[str] = set()
    for match in re.finditer(
        r"(?m)^(?:void\s*\*?|int|bool|size_t|cnet_tls_context\s*\*|"
        r"const\s+char\s*\*)\s*(cnet_tls_[A-Za-z0-9_]+)\s*\(",
        code,
    ):
        end = code.find(")", match.end())
        if end < 0:
            continue
        brace = code.find("{", end)
        semicolon = code.find(";", end)
        if brace >= 0 and (semicolon < 0 or brace < semicolon):
            output.add(match.group(1))
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--owner", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--production-cmake", required=True, type=Path)
    parser.add_argument("--owner-object", required=True, type=Path)
    parser.add_argument("--candidate-object", required=True, type=Path)
    args = parser.parse_args()

    original = args.owner.read_text(encoding="utf-8")
    candidate = args.candidate.read_text(encoding="utf-8")
    header = args.header.read_text(encoding="utf-8")
    cmake = args.production_cmake.read_text(encoding="utf-8")

    if not original.lstrip().startswith('#include "cnet_owner.h"'):
        raise RuntimeError("not the canonical original CNet Owner source")
    if any(token in candidate for token in
           ("#include <gmssl/", 'GmSSL::GmSSL', 'TLS_CONNECT', "TLS_CTX")):
        raise RuntimeError("test-only candidate exposes or embeds native GmSSL")
    if 'src/cnet/cnet_tls.c' not in cmake or 'GmSSL::GmSSL' not in cmake:
        raise RuntimeError("production CNet/GmSSL target changed unexpectedly")
    if 'cnet_tls_crypto_qualification.c' in cmake:
        raise RuntimeError("test-only candidate was put into production CNet")

    observed = required_calls(original)
    if observed != EXPECTED_OWNER_CALLS:
        raise RuntimeError(
            "CNet original Owner private TLS call graph changed: "
            f"missing={sorted(EXPECTED_OWNER_CALLS-observed)}, "
            f"new={sorted(observed-EXPECTED_OWNER_CALLS)}"
        )
    missing = observed - native_definitions(candidate)
    if missing:
        raise RuntimeError("test-only candidate lacks original Owner TLS ABI: "
                           + repr(sorted(missing)))
    undeclared = observed - required_calls(header)
    if undeclared:
        raise RuntimeError("original CNet private header lost declarations: "
                           + repr(sorted(undeclared)))

    # CMake builds these true C11 objects before running this test. A static
    # source-only check is NOT enough: prove actual object artifacts exist.
    for obj in (args.owner_object, args.candidate_object):
        if not obj.is_file() or obj.stat().st_size < 128:
            raise RuntimeError(f"native C11 object is missing/empty: {obj}")

    print("Original CNet Owner C11 source/object and opaque Crypto TLS ABI: "
          f"PASS ({len(observed)} exact Owner call contracts; "
          "no GmSSL native source in candidate)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as exc:
        print("CNet Owner ABI qualification FAIL: " + str(exc), file=sys.stderr)
        raise SystemExit(1)
