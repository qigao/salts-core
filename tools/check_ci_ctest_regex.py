#!/usr/bin/env python3
"""Fail CI when a multiline Bash ctest -R expression loses its closing quote.

The native sanitizer matrix can compile successfully yet fail before running
a single instrumented test if a regex edit loses "$'". Validate the *real*
workflow shell line before any Core package is built or qualified.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import sys

TEST_LINE = re.compile(r"^\s*-R\s+(['\"])(.*?)\1\s*$")


def validate(contents: str) -> list[str]:
    errors: list[str] = []
    found = 0
    for index, line in enumerate(contents.splitlines(), 1):
        if not re.match(r"^\s*-R\s+", line):
            continue
        found += 1
        match = TEST_LINE.fullmatch(line)
        if match is None:
            errors.append(f"line {index}: ctest -R is missing a closing quote")
            continue
        regex = match.group(2)
        if not (regex.startswith("^") and regex.endswith("$")):
            errors.append(f"line {index}: ctest -R must anchor both ends")
            continue
        try:
            re.compile(regex)
        except re.error as exc:
            errors.append(f"line {index}: invalid ctest regex: {exc}")
    if not found:
        errors.append("no ctest -R filter found; cannot qualify sanitizer test selection")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("workflow", type=Path)
    args = parser.parse_args()
    issues = validate(args.workflow.read_text(encoding="utf-8"))
    for issue in issues:
        print(f"CI CTest syntax: {issue}", file=sys.stderr)
    if not issues:
        print(f"CI CTest syntax: PASS ({args.workflow})")
    return int(bool(issues))


if __name__ == "__main__":
    raise SystemExit(main())
