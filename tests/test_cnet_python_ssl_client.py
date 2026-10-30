#!/usr/bin/env python3
"""Independent Python SSL client -> production CNet REQUIRED-mTLS conformance.

The native executable is the *same* Salts::CNet source-graph test binary
used for CA/SAN, CNet-to-CNet mTLS and Python server peer checks. Credentials
are ephemeral and not printed. Each SSLContext fixes one TLS version.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import socket
import ssl
import subprocess
import sys
import tempfile


CASES = ("client", "missing", "wrongca", "eku", "expired", "chain", "badsig")


def one_case(binary: Path, fixtures: dict, protocol: str,
             case: str, ctx: ssl.SSLContext) -> bool:
    expected = "accept" if case == "client" else "reject"
    command = [
        str(binary), "--external-python-client", protocol,
        str(fixtures["ca"]), str(fixtures["server"]),
        str(fixtures["server-key"]), expected,
    ]
    peer_connected = False
    negotiated = False
    saw_reply = False
    failure = None
    with subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, bufsize=1,
    ) as proc:
        assert proc.stdout is not None
        port_line = proc.stdout.readline().strip()
        match = re.fullmatch(r"CNET_PY_SSL_PORT=([0-9]{1,5})", port_line)
        if not match or not 0 < int(match.group(1)) < 65536:
            proc.kill()
            out, err = proc.communicate()
            print(f"FAIL {protocol} {case}: no native listener admission, "
                  f"output={out[-300:]} stderr={err[-800:]}", file=sys.stderr)
            return False
        port = int(match.group(1))
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=5.0) as raw:
                peer_connected = True
                raw.settimeout(8.0)
                with ctx.wrap_socket(raw, server_hostname="localhost") as tls:
                    negotiated = True
                    version = "TLSv1.2" if protocol == "tls12" else "TLSv1.3"
                    if tls.version() != version or not tls.cipher():
                        raise ValueError("Python peer negotiated unexpected TLS profile")
                    tls.sendall(b"P")
                    saw_reply = tls.recv(1) == b"Q"
        except (ssl.SSLError, ConnectionError, OSError) as exc:
            # Only the SSL alert/reason is reported, never certificate or key bytes.
            failure = f"{type(exc).__name__}: {str(exc)[:220]}"
        except ValueError as exc:
            failure = str(exc)
        try:
            output, error = proc.communicate(timeout=18)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate()
            print(f"FAIL {protocol} {case}: native CNet admission timed out",
                  file=sys.stderr)
            return False
        signal = f"PASS CNET_PY_SSL_NATIVE_{'ACCEPT' if expected == 'accept' else 'REJECT'} {protocol}"
        native_ok = proc.returncode == 0 and signal in output
        if expected == "accept":
            passed = peer_connected and negotiated and saw_reply and failure is None and native_ok
        else:
            # A negative is valid only after a real TCP peer connects, CNet
            # independently reports HANDSHAKE failure, and no app reply escapes.
            passed = peer_connected and native_ok and not saw_reply
        if not passed:
            print(
                f"FAIL {protocol} {case}: tcp={peer_connected} "
                f"handshake={negotiated} app={saw_reply} "
                f"python_error={failure} native_return={proc.returncode} "
                f"native_stdout={output[-500:]} native_stderr={error[-1200:]}",
                file=sys.stderr,
            )
        else:
            print(f"PASS independent Python ssl client / native CNet {protocol} "
                  f"REQUIRED-mTLS {case} {expected}")
        return passed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--fixture-generator", type=Path, required=True)
    parser.add_argument("--protocol", choices=("tls12", "tls13"), required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    if not args.binary.is_file() or not args.fixture_generator.is_file():
        parser.error("exact production CNet executable and Crypto fixtures are required")
    sys.path.insert(0, str(args.fixture_generator.parent))
    import python_mtls

    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="cnet-python-client-", dir=args.work) as temp:
        root = Path(temp)
        root.chmod(0o700)
        fixtures = python_mtls.make_fixtures(root)
        version = "12" if args.protocol == "tls12" else "13"
        for case in CASES:
            context = python_mtls.python_tls_context(
                "client", version, fixtures, case,
                hostname="localhost", trusted_ca="ca")
            if not one_case(args.binary, fixtures, args.protocol, case, context):
                return 1
    print(f"PASS native CNet server / external Python ssl client {args.protocol} "
          "REQUIRED-mTLS all seven scenarios")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
