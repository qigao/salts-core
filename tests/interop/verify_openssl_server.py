#!/usr/bin/env python3
"""Independent TLS peer interoperability: OpenSSL 3 server vs CNet/GmSSL client.

No shared TLS implementation and no mock transport. Server identity, trust
roots and TLS-version restrictions are set independently per live connection.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import socket
import subprocess
import sys
import tempfile
import time


def ready(port: int, server: subprocess.Popen, seconds: float = 4.0) -> None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError(f"external OpenSSL server exited {server.returncode}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.3):
                return
        except OSError:
            time.sleep(0.05)
    raise TimeoutError(f"external OpenSSL server not accepting TCP on {port}")


def check_version(executable: str) -> None:
    result = subprocess.run([executable, "version"], check=True, text=True,
                            capture_output=True, timeout=10)
    if not re.match(r"OpenSSL 3\.", result.stdout):
        raise RuntimeError(f"requires independent OpenSSL 3, got: {result.stdout!r}")
    print(f"independent_peer={result.stdout.strip()}", flush=True)


def run_case(client: str, openssl: str, fixtures: pathlib.Path,
             version: str, name: str, ca_file: pathlib.Path,
             expect: str) -> None:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    port = listener.getsockname()[1]
    listener.close()
    label = f"{version} {expect} {name} {ca_file.name}"
    cmd = [openssl, "s_server",
           "-accept", f"127.0.0.1:{port}",
           "-cert", str(fixtures / "p384-test-server-cert.pem"),
           "-key", str(fixtures / "p384-test-server-key.pem"),
           "-www", "-tls1_2" if version == "TLSv1.2" else "-tls1_3"]
    with tempfile.TemporaryFile(mode="w+b") as logs:
        process = subprocess.Popen(cmd, stdin=subprocess.DEVNULL,
                                   stdout=logs, stderr=subprocess.STDOUT)
        try:
            ready(port, process)
            result = subprocess.run(
                [client, str(port), str(ca_file), name, expect, version],
                check=False, capture_output=True, text=True, timeout=15)
            if result.returncode:
                logs.seek(0)
                details = logs.read().decode("utf-8", errors="replace")[-2000:]
                raise RuntimeError(
                    f"case={label!r}, client code={result.returncode}\n"
                    f"client stdout={result.stdout[-1000:]}\n"
                    f"client stderr={result.stderr[-2000:]}\n"
                    f"OpenSSL peer log={details}"
                )
            print(f"interop_pass={label}", flush=True)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openssl", required=True, type=pathlib.Path)
    parser.add_argument("--client", required=True, type=pathlib.Path)
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    args = parser.parse_args()

    openssl = str(args.openssl.resolve())
    client = str(args.client.resolve())
    fixtures = args.fixtures.resolve()
    for path in [pathlib.Path(openssl), pathlib.Path(client),
                 fixtures / "p384-test-root-cert.pem",
                 fixtures / "p384-test-wrong-root-cert.pem",
                 fixtures / "p384-test-server-cert.pem",
                 fixtures / "p384-test-server-key.pem"]:
        if not path.is_file():
            raise FileNotFoundError(f"missing exact peer/tool fixture: {path}")
    check_version(openssl)
    # Exercise TLS 1.3 and TLS 1.2 independently; do not let one protocol
    # failure suppress evidence for the other. All failures still fail CTest.
    cases = [
        ("TLSv1.3", "localhost", "p384-test-root-cert.pem", "accept"),
        ("TLSv1.2", "localhost", "p384-test-root-cert.pem", "accept"),
        ("TLSv1.3", "localhost", "p384-test-wrong-root-cert.pem", "reject"),
        ("TLSv1.2", "localhost", "p384-test-wrong-root-cert.pem", "reject"),
        ("TLSv1.3", "wrong.example", "p384-test-root-cert.pem", "reject"),
        ("TLSv1.2", "wrong.example", "p384-test-root-cert.pem", "reject"),
    ]
    failures = []
    for version, name, ca_name, expect in cases:
        try:
            run_case(client, openssl, fixtures, version, name,
                     fixtures / ca_name, expect)
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            failures.append(f"{version} {expect} {name}: {error}")
            print(f"interop_case_failed={version} {expect} {name}", flush=True)
    if failures:
        raise RuntimeError(f"{len(failures)}/{len(cases)} independent TLS cases failed\n" +
                           "\n\n".join(failures))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"independent_tls_interop_failure={error}", file=sys.stderr)
        sys.exit(1)
