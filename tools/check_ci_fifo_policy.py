#!/usr/bin/env python3
"""Fail-closed FIFO Actions policy: one workflow and one job at a time.

GitHub queue:max retains up to 100 pending runs; group waiting order is
approximate FIFO, not strictly commit/dispatch ordered.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
WF = ("concurrency:\n  group: saltscore-fifo-workflows\n"
      "  cancel-in-progress: false\n  queue: max\n")
JOB = ("    concurrency:\n      group: saltscore-fifo-runners\n"
       "      cancel-in-progress: false\n      queue: max\n")
JOB_ID = re.compile(r"^  [A-Za-z0-9_-]+:\s*$")

def validate(source: str, name: str) -> list[str]:
    errors = []
    if source.count(WF) != 1:
        errors.append(f"{name}: workflow FIFO queue missing or duplicated")
    if source.count("group: saltscore-fifo-workflows") != 1:
        errors.append(f"{name}: workflow queue group altered")
    if "cancel-in-progress: true" in source:
        errors.append(f"{name}: in-progress cancellation forbidden")
    if re.search(r"(?m)^  push:\s*$", source) and not re.search(
            r"(?m)^  push:\n    branches:\n      - master(?:\n|$)", source):
        errors.append(f"{name}: feature push duplicates PR admission")
    if not re.search(r"(?m)^  pull_request:\s*$", source):
        errors.append(f"{name}: required PR gate missing")
    if "jobs:\n" not in source:
        errors.append(f"{name}: missing required jobs")
        return errors
    lines = source.splitlines()
    active = False
    count = 0
    for i, line in enumerate(lines):
        if line == "jobs:":
            active = True
            continue
        if active and re.match(r"^[A-Za-z][\w-]*:\s*$", line):
            active = False
            continue
        if not active or not JOB_ID.match(line):
            continue
        count += 1
        subset = []
        for future in lines[i + 1:]:
            if JOB_ID.match(future) or re.match(r"^[A-Za-z][\w-]*:", future):
                break
            subset.append(future)
        part = "\n".join(subset) + "\n"
        if part.count(JOB) != 1:
            errors.append(f"{name}: runner FIFO slot missing for {line.strip()}")
        for j, step in enumerate(subset):
            if step.strip() != "strategy:":
                continue
            indent = len(step) - len(step.lstrip(" "))
            nested = []
            for candidate in subset[j + 1:]:
                if not candidate.strip():
                    continue
                n = len(candidate) - len(candidate.lstrip(" "))
                if n <= indent:
                    break
                nested.append(candidate.strip())
            if "matrix:" in nested and "max-parallel: 1" not in nested:
                errors.append(f"{name}: unbounded matrix in {line.strip()}")
    if not count:
        errors.append(f"{name}: missing jobs")
    if source.count("group: saltscore-fifo-runners") != count:
        errors.append(f"{name}: runner concurrency count mismatch")
    return errors

def main() -> int:
    files = sorted((ROOT / ".github/workflows").glob("*.yml"))
    if len(files) < 17:
        print(f"FAIL: expected 17 workflows, found {len(files)}")
        return 1
    errors = []
    for f in files:
        errors += validate(f.read_text(encoding="utf-8"), f.name)
    for error in errors:
        print("FAIL:", error)
    if errors:
        return 1
    print(f"PASS: {len(files)} workflows, FIFO queue:max, one runner slot; "
          "PR/master CI retained, no duplicate feature push")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
