#!/usr/bin/env python3
"""Real Python ssl client to the one production CNet NativeIO TLS server.

Every positive TLS1.2/TLS1.3 case requires complete mutual authentication,
a strict negotiated version, authenticated bidirectional application bytes
and a verified client certificate on the native server. Negative cases must
produce a server-side handshake failure without an authenticated connection.
Test credentials live only inside a private temporary directory.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import hashlib
import socket
import ssl
import subprocess

from cryptography import x509
from cryptography.hazmat.primitives import serialization
import sys
import tempfile
import time


CASES = ("client", "missing", "wrongca", "eku", "expired", "chain", "badsig")
VERSION = {"12": "TLSv1.2", "13": "TLSv1.3"}


def fragmented_tls13_exchange(ctx: ssl.SSLContext, raw: socket.socket) -> None:
    """One real TLS1.3 client using Python's SSLObject and MemoryBIO.

    Carry Finished and the first application record in the same outbound
    batch, then split ciphertext into bounded TCP writes. The native CNet
    peer must finish authentication and deliver F before it can send G.
    No plaintext/TLS keys are logged and no alternative native engine exists.
    """
    incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
    peer = ctx.wrap_bio(incoming, outgoing, server_hostname="localhost")
    raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    def flush_ciphertext() -> None:
        while outgoing.pending:
            wire = outgoing.read()
            offset = 0
            # Split at record-header and payload boundaries. The tail is
            # sent without sleeping or waiting for a handshake response.
            for n in (1, 2, 7, 31, 127):
                if offset >= len(wire):
                    break
                step = min(n, len(wire) - offset)
                raw.sendall(wire[offset:offset + step])
                offset += step
                time.sleep(0.001)
            if offset < len(wire):
                raw.sendall(wire[offset:])

    def receive_ciphertext(phase: str) -> None:
        wire = raw.recv(16384)
        if not wire:
            raise AssertionError(f"server closed during {phase}")
        incoming.write(wire)

    # Do not permit a second handshake or a version/security fallback.
    while True:
        try:
            peer.do_handshake()
            break
        except ssl.SSLWantWriteError:
            flush_ciphertext()
        except ssl.SSLWantReadError:
            flush_ciphertext()
            receive_ciphertext("TLS1.3 handshake")

    if peer.version() != "TLSv1.3":
        raise AssertionError(f"fragmented peer negotiated {peer.version()!r}")
    if not peer.getpeercert():
        raise AssertionError("Python TLS1.3 peer did not verify server certificate")
    # MemoryBIO must contain the encrypted client Finished from handshake.
    # The application record is then added before EITHER is sent on TCP.
    finished_bytes = outgoing.pending
    if finished_bytes == 0:
        raise AssertionError("TLS1.3 Finished already flushed; coalescing test invalid")
    if peer.write(b"F") != 1 or outgoing.pending <= finished_bytes:
        raise AssertionError("Python SSLObject did not queue app data after Finished")
    flush_ciphertext()

    while True:
        try:
            response = peer.read(1)
            if response != b"G":
                raise AssertionError(f"fragmented mTLS response missing: {response!r}")
            return
        except ssl.SSLWantWriteError:
            flush_ciphertext()
        except ssl.SSLWantReadError:
            flush_ciphertext()
            receive_ciphertext("TLS1.3 authenticated response")


def scenario(
    binary: Path, work: Path, crypto, fixtures, proto: str, case: str,
    *, fragmented: bool = False,
) -> None:
    expected = "accept" if case == "client" else "reject"
    if fragmented and (proto != "13" or case != "client"):
        raise AssertionError("fragmented ciphertext gate is valid TLS1.3 only")
    suffix = "-fragmented" if fragmented else ""
    port_path = work / f"native-{proto}-{case}{suffix}.port"
    if port_path.exists():
        raise AssertionError("stale private port marker")
    # Exact-head Python fixture generator and standard-library ssl client.
    # Context creation errors never count as native handshake rejections.
    ctx = crypto.python_tls_context("client", proto, fixtures, case)
    expected_digest = "-"
    if case == "client":
        certificate = x509.load_pem_x509_certificate(fixtures["client"].read_bytes())
        expected_digest = hashlib.sha256(
            certificate.public_bytes(serialization.Encoding.DER)
        ).hexdigest()
    native = subprocess.Popen(
        [str(binary), "--python-ssl-client", str(port_path),
         str(fixtures["ca"]), str(fixtures["server"]),
         str(fixtures["server-key"]), proto, expected, expected_digest],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    failure = None
    phase = "await-native-listen"
    peer_rejected = False
    peer_completed = False
    try:
        deadline = time.monotonic() + 12.0
        while not port_path.exists():
            if native.poll() is not None:
                raise AssertionError("native CNet server terminated before bind")
            if time.monotonic() >= deadline:
                raise AssertionError("native CNet server did not bind within deadline")
            time.sleep(0.02)
        port = int(port_path.read_text(encoding="ascii").strip())
        if not 0 < port < 65536:
            raise AssertionError("native listener port invalid")
        phase = "tcp-connect"
        with socket.create_connection(("127.0.0.1", port), timeout=8.0) as raw:
            raw.settimeout(9.0)
            try:
                if fragmented:
                    phase = "tls13-fragmented-finished-plus-application"
                    fragmented_tls13_exchange(ctx, raw)
                    peer_completed = True
                    phase = "authenticated"
                else:
                    phase = "tls-handshake"
                    with ctx.wrap_socket(raw, server_hostname="localhost") as peer:
                        peer.settimeout(9.0)
                        phase = "negotiated-version"
                        if peer.version() != VERSION[proto]:
                            raise AssertionError("Python negotiated a different TLS version")
                        # TLS1.3 wrap_socket can finish before the server
                        # verifies CertificateVerify; require real app data.
                        phase = "authenticated-request"
                        peer.sendall(b"F")
                        phase = "authenticated-response"
                        response = peer.recv(1)
                        if expected == "accept":
                            if response != b"G":
                                raise AssertionError(f"valid mTLS app data missing: {response!r}")
                            peer_completed = True
                            phase = "authenticated"
                        elif response == b"G":
                            raise AssertionError("invalid mTLS client received authenticated data")
                        else:
                            peer_rejected = True
            except (ssl.SSLError, ConnectionError, EOFError):
                if expected == "accept":
                    raise
                peer_rejected = True
        if expected == "reject" and not peer_rejected:
            raise AssertionError("invalid client was not rejected")
        if expected == "accept" and not peer_completed:
            raise AssertionError("valid bidirectional TLS data was not exchanged")
    except (socket.timeout, TimeoutError) as exc:
        failure = f"deadline exceeded during {phase}: {type(exc).__name__}"
    except Exception as exc:
        failure = f"{type(exc).__name__} during {phase}: {str(exc)[:300]}"
    finally:
        try:
            out, err = native.communicate(timeout=12)
        except subprocess.TimeoutExpired:
            native.kill()
            out, err = native.communicate()
            failure = failure or "native server retained live owner past deadline"
    if failure is not None or native.returncode != 0 or (
        f"PASS production NativeIO CNet server / TLS{proto} / {expected}" not in out
    ):
        raise AssertionError(
            f"TLS{proto} {case} failed at {phase}: {failure or ''} "
            f"native_exit={native.returncode} "
            f"stdout={out[-350:]} stderr={err[-1200:]}"
        )
    print(
        f"PASS independent Python ssl TLS{proto} {case} {expected}"
        + (" Finished+ApplicationData fragmented" if fragmented else "")
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--fixture-generator", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    if not args.binary.is_file() or not args.fixture_generator.is_file():
        parser.error("one native CNet executable and exact Python fixture source are required")
    sys.path.insert(0, str(args.fixture_generator.parent))
    import python_mtls

    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="cnet-python-peer-", dir=args.work) as temp:
        private = Path(temp)
        private.chmod(0o700)
        fixtures = python_mtls.make_fixtures(private)
        for proto in ("12", "13"):
            for case in CASES:
                scenario(args.binary, private, python_mtls, fixtures, proto, case)
        # A single additional real TLS1.3 connection, not a retry/stress
        # loop, fixes wire segmentation deterministically in the same CTest.
        scenario(
            args.binary, private, python_mtls, fixtures, "13", "client",
            fragmented=True,
        )
    print("PASS Python ssl client -> one production CNet REQUIRED-mTLS server on both TLS versions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
