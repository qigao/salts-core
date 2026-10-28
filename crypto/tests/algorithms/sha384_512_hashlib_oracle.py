#!/usr/bin/env python3
"""Cross-check the ACTUAL native C11 SHA384/512 executable with hashlib.

No OpenSSL executable, GmSSL, second C algorithm or provider package.
One exact compiled artifact, deterministic 27 padding/block boundary inputs.
"""
from __future__ import annotations
import hashlib
import subprocess
import sys

SIZES = (0, 1, 2, 55, 56, 57, 63, 64, 65, 111, 112, 113, 119, 120,
         121, 127, 128, 129, 200, 255, 256, 257, 511, 512, 513,
         1000, 4096, 1000000)

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: sha384_512_hashlib_oracle.py <compiled-native-executable>",
              file=sys.stderr)
        return 2
    binary = sys.argv[1]
    for size in SIZES:
        source = bytes((i * 73 + 19) & 0xff for i in range(size))
        expected = (hashlib.sha384(source).hexdigest(),
                    hashlib.sha512(source).hexdigest())
        proc = subprocess.run([binary, "--oracle", str(size)], capture_output=True,
                              text=True, check=False, timeout=30)
        if proc.returncode != 0:
            print(f"FAIL length={size} exit={proc.returncode} stderr={proc.stderr}",
                  file=sys.stderr)
            return 1
        actual = tuple(line.strip() for line in proc.stdout.splitlines())
        if actual != expected:
            print(f"FAIL: FIPS SHA384/512 hashlib mismatch length={size}",
                  file=sys.stderr)
            return 1
    print("PASS native SHA384/SHA512 vs Python hashlib: 28 independent sizes")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
