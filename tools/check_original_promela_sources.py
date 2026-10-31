#!/usr/bin/env python3
"""Fail CI if any of the 10 original Spin models/README are lost or edited.

The source checksum gate is NOT a claim Spin accepted all 50 original claims.
Restore and independently run original models against mapped modern C11 code.
"""
from __future__ import annotations
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL_SOURCES = {
    "README.md": "07aaddf74a478d1987d29762a11eb74ee4d7d6d6",
    "buffer_ownership.pml": "f114505c47e071f110108c5120cc5d0143ee7adc",
    "byte_buffer.pml": "850b5478d163b8207bfb186d7c55fbbd38706d6c",
    "byte_buffer_errors.pml": "f6c1ab9a753ba6d23d810fd240705f3f286b536a",
    "disruptor_broadcast.pml": "5c1f0a640fabc47ac28bb2f68ad30991b2e167c4",
    "disruptor_topology.pml": "94ce08e20ee2c8614618c0762fb1c7fd45cac7ad",
    "disruptor_wait_shutdown.pml": "8308e83bbbf49cd6356590abc408da4217ab75eb",
    "disruptor_worker.pml": "6be6e525b06a1cbaeaa1050a21232cf982cbc859",
    "priority_queue.pml": "4525c62ad4fbb8b8cf8274b81d691dedc9b37a17",
    "priority_queue_timeout.pml": "e0b11404ddbb9e3b96247e2ea50d7ee6dd11f799",
    "ring_spsc.pml": "73f0ebe702ac713f13e22941cd135762f19fabdd",
}
def problems(root: Path = ROOT) -> list[str]:
    errors = []
    for name, expected in ORIGINAL_SOURCES.items():
        path = root / "verification" / name
        if not path.is_file():
            errors.append(f"original model/README absent: {path}")
            continue
        raw = path.read_bytes().replace(b"\r\n", b"\n")
        git_hash = hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()
        if git_hash != expected:
            errors.append(f"original model mutated: {name} expected {expected} got {git_hash}")
    return errors

if __name__ == "__main__":
    failures=problems()
    for failure in failures:
        print("FAIL:",failure)
    if failures:
        raise SystemExit(1)
    print("PASS: 11/11 original verification source Git blobs preserved (10 Promela models)")
    print("NOT A FORMAL ACCEPTANCE: original 50 Spin claims must run independently")
