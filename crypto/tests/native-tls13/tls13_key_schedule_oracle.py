#!/usr/bin/env python3
"""Independent stdlib hmac/hashlib oracle for actual native C11 TLS13 HKDF."""
from __future__ import annotations
import hashlib
import hmac
import subprocess
import sys

def expand_label(secret: bytes, label: bytes, context: bytes,
                 length: int, digest) -> bytes:
    name = b"tls13 " + label
    info = length.to_bytes(2, "big") + bytes([len(name)]) + name + \
           bytes([len(context)]) + context
    out = b""
    previous = b""
    counter = 0
    while len(out) < length:
        counter += 1
        previous = hmac.new(secret, previous + info + bytes([counter]),
                            digest).digest()
        out += previous
    return out[:length]

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: tls13_key_schedule_oracle.py <native-executable>", file=sys.stderr)
        return 2
    p = subprocess.run([sys.argv[1]], capture_output=True, check=False,
                       text=True, timeout=45)
    if p.returncode != 0:
        print("FAIL: native C11 vectors:", p.stderr, file=sys.stderr)
        return 1
    wanted = {"PRK256", "PRK384", "LBL256", "LBL384", "LONG256", "LONG384",
              "LONGSALT256", "LONGSALT384", "EMPTYSALT256", "EMPTYSALT384"}
    actual = {}
    for line in p.stdout.splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        if key in wanted:
            if key in actual:
                print("FAIL duplicate output " + key, file=sys.stderr)
                return 1
            actual[key] = value.strip()
    salt = bytes(range(13))
    ikm = bytes([0x0b] * 22)
    prk256 = hmac.new(salt, ikm, hashlib.sha256).digest()
    prk384 = hmac.new(salt, ikm, hashlib.sha384).digest()
    long_salt = bytes((i * 11 + 7) & 0xff for i in range(200))
    long256 = hmac.new(long_salt, ikm, hashlib.sha256).digest()
    long384 = hmac.new(long_salt, ikm, hashlib.sha384).digest()
    empty256 = hmac.new(bytes(32), b"", hashlib.sha256).digest()
    empty384 = hmac.new(bytes(48), b"", hashlib.sha384).digest()
    label = b"derived"
    expected = {
        "LONGSALT256": long256.hex(),
        "LONGSALT384": long384.hex(),
        "EMPTYSALT256": empty256.hex(),
        "EMPTYSALT384": empty384.hex(),
        "PRK256": prk256.hex(),
        "PRK384": prk384.hex(),
        "LBL256": expand_label(prk256, label, b"\x11\x22", 32, hashlib.sha256).hex(),
        "LBL384": expand_label(prk384, label, b"\x00\x01\x02", 48, hashlib.sha384).hex(),
        "LONG256": expand_label(prk256, label, b"\x11\x22", 97, hashlib.sha256).hex(),
        "LONG384": expand_label(prk384, label, b"\x00\x01\x02", 101, hashlib.sha384).hex(),
    }
    for name, value in expected.items():
        if actual.get(name) != value:
            print(f"FAIL {name}: expected={value} actual={actual.get(name)}",
                  file=sys.stderr)
            return 1
    if "PASS: native RFC5869/8446" not in p.stdout:
        print("FAIL missing native negative checks", file=sys.stderr)
        return 1
    print("PASS: native TLS13 HKDF SHA256/SHA384 equals Python hmac/hashlib"
          " for 10 independent vectors + strict C11 rejection tests")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
