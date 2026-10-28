#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Bounded Python-only TLS1.2 CertificateVerify corruption peer.

This is an acceptance-test transport relay, NOT a TLS provider or OpenSSL
C/CLI program. It forwards original TLS records without logging them, except
for flipping precisely one bit of the *plaintext* TLS1.2 client
CertificateVerify signature. This must be refused by the native Crypto
server before application admission. TLS1.3 CertificateVerify is encrypted:
this relay never claims to mutate its plaintext TLS1.3 signature.
"""
from __future__ import annotations

import select
import socket
import time

MAX_RECORD_BYTES = 5 + 16384 + 2048
MAX_SECONDS = 20.0


class RelayContractError(Exception):
    pass


def corrupt_certificate_verify(record: bytearray, state: dict[str, object]) -> None:
    """Mutate only a fully framed TLS1.2 CertificateVerify handshake record."""
    if state.get("mutations") or len(record) < 9 or record[0] != 22:
        return
    position = 5
    while position + 4 <= len(record):
        kind = record[position]
        length = int.from_bytes(record[position + 1:position + 4], "big")
        end = position + 4 + length
        if end > len(record):
            # We do not guess about cross-record framing. The gate later
            # fails unless exactly one complete CertificateVerify was altered.
            return
        if kind == 15:
            if length < 5 or position + 8 > end:
                raise RelayContractError("invalid CertificateVerify framing")
            signature_len = int.from_bytes(
                record[position + 6:position + 8], "big"
            )
            if signature_len < 1 or position + 8 + signature_len != end:
                raise RelayContractError("invalid CertificateVerify signature length")
            record[end - 1] ^= 1
            state["mutations"] = 1
            return
        position = end


def relay_tls12_certificate_verify(
    listener: socket.socket, origin_port: int, state: dict[str, object]
) -> None:
    """One local client -> native-server connection, with bounded ownership."""
    start = time.monotonic()
    try:
        listener.settimeout(12.0)
        inbound, _ = listener.accept()
        with inbound:
            inbound.settimeout(10.0)
            upstream: socket.socket | None = None
            while time.monotonic() - start < 10.0:
                try:
                    upstream = socket.create_connection(
                        ("127.0.0.1", origin_port), timeout=3.0,
                    )
                    break
                except ConnectionRefusedError:
                    time.sleep(0.025)
            if upstream is None:
                raise RelayContractError("native server did not bind")
            with upstream:
                upstream.settimeout(10.0)
                pending = bytearray()
                while time.monotonic() - start < MAX_SECONDS:
                    ready, _, _ = select.select(
                        [inbound, upstream], [], [], 0.25,
                    )
                    if upstream in ready:
                        raw = upstream.recv(65536)
                        if not raw:
                            return
                        inbound.sendall(raw)
                    if inbound in ready:
                        raw = inbound.recv(65536)
                        if not raw:
                            return
                        pending.extend(raw)
                        if len(pending) > MAX_RECORD_BYTES * 2:
                            raise RelayContractError("record buffer exceeded bound")
                        while len(pending) >= 5:
                            size = 5 + int.from_bytes(pending[3:5], "big")
                            if size < 5 or size > MAX_RECORD_BYTES:
                                raise RelayContractError("invalid TLS record bound")
                            if len(pending) < size:
                                break
                            wire = bytearray(pending[:size])
                            del pending[:size]
                            corrupt_certificate_verify(wire, state)
                            upstream.sendall(wire)
                raise RelayContractError("TLS record relay exceeded deadline")
    except (OSError, TimeoutError, RelayContractError) as exc:
        # A peer closing immediately after the deliberately invalid signed
        # handshake is normal. Still require exactly one mutation and the
        # native server's authenticated rejection in the enclosing gate.
        if state.get("mutations") != 1:
            state["transport_error"] = type(exc).__name__
    finally:
        listener.close()
