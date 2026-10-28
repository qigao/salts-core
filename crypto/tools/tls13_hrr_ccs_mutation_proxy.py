#!/usr/bin/env python3
"""Test-only TLS-record proxy: mutate one CCS specifically after a real HRR.

NEVER inspect, dump or log handshake payloads, certificate bytes or secrets.
This is a security-negative acceptance harness, not a TLS production proxy.
"""
from __future__ import annotations

import argparse
import socket
import sys
import threading

HRR_RANDOM = bytes.fromhex(
    "cf21ad74e59a6111be1d8c021e65b891"
    "c2a211167abb8c5e079e09e2c8a8339c"
)
MAX_TLS_RECORD = 5 + 18432


def read_exact(sock: socket.socket, length: int) -> bytes | None:
    parts = bytearray()
    while len(parts) < length:
        block = sock.recv(length - len(parts))
        if not block:
            if not parts:
                return None
            raise ConnectionError("truncated TLS test record")
        parts.extend(block)
    return bytes(parts)


def recv_record(sock: socket.socket) -> bytes | None:
    header = read_exact(sock, 5)
    if header is None:
        return None
    size = int.from_bytes(header[3:5], "big")
    if 5 + size > MAX_TLS_RECORD or size == 0:
        raise ValueError("invalid or oversized TLS record length")
    payload = read_exact(sock, size)
    if payload is None:
        raise ConnectionError("missing TLS payload")
    return header + payload


def is_real_hrr(record: bytes) -> bool:
    # TLS record + handshake type/length + legacy_version + random:
    # random offset = 5 + 4 + 2 = 11. Inspect only the public fixed HRR
    # marker, not cryptographic randoms or handshake application data.
    return (
        len(record) >= 43
        and record[0] == 22
        and record[5] == 2
        and record[11:43] == HRR_RANDOM
    )


def relay(
    src: socket.socket,
    dst: socket.socket,
    from_client: bool,
    hrr_seen: threading.Event,
    changed: threading.Event,
    mode: str,
) -> None:
    try:
        while True:
            record = recv_record(src)
            if record is None:
                return
            if not from_client and is_real_hrr(record):
                hrr_seen.set()
            if (
                from_client
                and hrr_seen.is_set()
                and not changed.is_set()
                and record[0] == 20
                and record[1:3] == b"\x03\x03"
                and record[3:5] == b"\x00\x01"
                and record[5:6] == b"\x01"
            ):
                if mode == "duplicate":
                    dst.sendall(record)
                    dst.sendall(record)
                elif mode == "malformed":
                    dst.sendall(record[:-1] + b"\x02")
                else:
                    raise ValueError("unsupported mutation")
                changed.set()
                # No further mutation: the native TLS server must reject
                # duplicate/malformed CCS before processing ClientHello2.
                continue
            dst.sendall(record)
    except (OSError, ConnectionError, ValueError):
        return
    finally:
        try:
            dst.shutdown(socket.SHUT_WR)
        except OSError:
            pass


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--listen", type=int, required=True)
    p.add_argument("--upstream", type=int, required=True)
    p.add_argument("--mode", required=True, choices=("duplicate", "malformed"))
    args = p.parse_args()
    if not (1024 <= args.listen <= 65535 and
            1024 <= args.upstream <= 65535 and
            args.listen != args.upstream):
        p.error("invalid loopback port allocation")

    hrr_seen = threading.Event()
    changed = threading.Event()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", args.listen))
        listener.listen(1)
        listener.settimeout(10)
        try:
            incoming, _ = listener.accept()
        except socket.timeout:
            print("CRYPTO_HRR_MUTATION_PROXY no_client", file=sys.stderr)
            return 1
        with incoming:
            incoming.settimeout(12)
            with socket.create_connection(("127.0.0.1", args.upstream),
                                          timeout=10) as native:
                native.settimeout(12)
                # The server's native HRR must be forwarded before the
                # client generates its strict one-byte compatibility CCS.
                server_to_client = threading.Thread(
                    target=relay,
                    args=(native, incoming, False, hrr_seen, changed, args.mode),
                    daemon=True,
                )
                client_to_server = threading.Thread(
                    target=relay,
                    args=(incoming, native, True, hrr_seen, changed, args.mode),
                    daemon=True,
                )
                server_to_client.start()
                client_to_server.start()
                server_to_client.join(15)
                client_to_server.join(15)
                if server_to_client.is_alive() or client_to_server.is_alive():
                    return 1
    if not changed.is_set() or not hrr_seen.is_set():
        print("CRYPTO_HRR_MUTATION_PROXY mutation_not_exercised",
              file=sys.stderr)
        return 1
    print("CRYPTO_HRR_MUTATION_PROXY injected=" + args.mode)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
