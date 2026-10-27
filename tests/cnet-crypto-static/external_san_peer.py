#!/usr/bin/env python3
"""Independent OpenSSL server for static CNet client DNS-SAN verification.

Two separate processes, not a second provider in the CNet/Salts::Crypto
image. The same valid localhost server certificate is presented regardless
of requested SNI. A positive localhost control MUST succeed first.
"""
import argparse
import pathlib
import socket
import ssl
import subprocess
import sys
import threading


def ssl_context(cert: pathlib.Path, key: pathlib.Path, protocol: str):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls_ver = ssl.TLSVersion.TLSv1_2 if protocol == "tls12" else ssl.TLSVersion.TLSv1_3
    ctx.minimum_version = tls_ver
    ctx.maximum_version = tls_ver
    ctx.load_cert_chain(certfile=str(cert), keyfile=str(key))
    # Both protocols must use the sole Crypto facade's actual P-256 group;
    # no opportunistic X25519/other curve fallback is counted as acceptance.
    ctx.set_ecdh_curve("prime256v1")
    ctx.set_ciphers("ECDHE-RSA-AES128-GCM-SHA256")
    return ctx


def one_session(ctx, binary, protocol, identity):
    observed = {"accepted": False, "sni": None, "completed": False, "version": None,
                "failure": None}

    def on_sni(_conn, requested, _context):
        observed["sni"] = requested  # evidence only; never reject/select cert

    ctx.set_servername_callback(on_sni)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(14.0)
        port = listener.getsockname()[1]

        def serve():
            try:
                conn, _addr = listener.accept()
                observed["accepted"] = True
                conn.settimeout(12.0)
                with conn:
                    with ctx.wrap_socket(conn, server_side=True) as tls:
                        observed["completed"] = True
                        observed["version"] = tls.version()
                        tls.settimeout(1.0)
                        try:
                            tls.recv(1)
                        except (socket.timeout, ssl.SSLError, OSError):
                            pass
            except (ssl.SSLError, socket.timeout, OSError) as exc:
                observed["failure"] = type(exc).__name__ + ": " + str(exc)

        thread = threading.Thread(target=serve, name="openssl-independent-peer")
        thread.start()
        try:
            result = subprocess.run(
                [str(binary), "--external-san", str(port), protocol, identity],
                capture_output=True, text=True, timeout=25, check=False,
            )
        except subprocess.TimeoutExpired as exc:
            result = None
            observed["failure"] = "client timeout: " + str(exc)
        thread.join(timeout=15)
        if thread.is_alive():
            print("FAIL independent OpenSSL peer did not terminate", file=sys.stderr)
            return False

    expected_version = "TLSv1.2" if protocol == "tls12" else "TLSv1.3"
    passed = (result is not None and result.returncode == 0
              and observed["accepted"] and observed["sni"] == identity)
    if identity == "localhost":
        passed = (passed and observed["completed"]
                  and observed["version"] == expected_version)
    # A wrong-SAN negative may trigger an alert before the server completes
    # its own handshake; the *client* must reject after observing the valid
    # unrelated SAN. We never accept absent TCP/SNI as negative evidence.
    if not passed:
        print(f"FAIL protocol={protocol} identity={identity} peer={observed}",
              file=sys.stderr)
        if result is not None:
            print("CNet returncode:", result.returncode, file=sys.stderr)
            print("CNet stderr:", result.stderr[-3000:], file=sys.stderr)
    else:
        print(f"PASS {protocol} independent OpenSSL server "
              f"SNI={identity} client authentication/denial")
    return passed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=pathlib.Path, required=True)
    parser.add_argument("--cert", type=pathlib.Path, required=True)
    parser.add_argument("--key", type=pathlib.Path, required=True)
    parser.add_argument("--protocol", choices=("tls12", "tls13"), required=True)
    args = parser.parse_args()
    for path in (args.binary, args.cert, args.key):
        if not path.is_file():
            parser.error("exact fixture/binary missing: " + str(path))
    ctx = ssl_context(args.cert, args.key, args.protocol)
    # A matching DNS SAN positive check guards against accepting unrelated
    # protocol, signature, network, trust or cipher failures as SAN rejection.
    if not one_session(ctx, args.binary, args.protocol, "localhost"):
        return 1
    if not one_session(ctx, args.binary, args.protocol, "wrong.example"):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
