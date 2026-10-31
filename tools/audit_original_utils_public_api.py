#!/usr/bin/env python3
"""Immutable original 35-header, 465-function public-API inventory.

A PASS proves baseline coverage only, not that current functions link or retain
behavior. Native macro/inline APIs are not counted by this conservative
SALTS_C_API prototype extraction; they require further separate verification.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PATH=Path("tools/original_utils_public_api_inventory.json")
TREE="9c5b53d7194d142a89be576c3314ee4bfd569a37"
CHECKSUM="d3705cd434062e25"
KNOWN_STATES={
"stage_a_declared_impl_present_unverified_behavior",
"crypto_repo_function_parity_required",
"fs_rename_behavior_parity_required",
"process_rename_behavior_parity_required",
"platform_function_parity_required",
"mdns_implementation_owner_required",
"unmapped_or_unverified_implementation",
"verified"
}

def checksum(headers: list[dict]) -> str:
    data="".join(
        ("".join(f"{h['file']}\t{h['source_sha']}\t{f['name']}\n"
                 for f in h["functions"])
         if h["functions"] else f"{h['file']}\t{h['source_sha']}\t\n")
        for h in sorted(headers,key=lambda h:h["file"])
    ).encode("utf-8")
    digest=0xcbf29ce484222325
    for byte in data:
        digest=((digest ^ byte)*0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{digest:016x}"

def check(data: dict, full: bool = False) -> list[str]:
    errors=[]
    baseline=data.get("baseline",{})
    if baseline.get("original_utils_tree_sha")!=TREE:
        errors.append("original utils immutable source tree has changed")
    if baseline.get("public_header_count")!=35:
        errors.append("must track all 35 original public headers")
    headers=data.get("headers",[])
    if len(headers)!=35 or checksum(headers)!=CHECKSUM:
        errors.append("35 original header/source-hash/declared-function inventory changed")
    if len({h.get("file") for h in headers})!=len(headers):
        errors.append("duplicate original public header")
    count=sum(len(h.get("functions",[])) for h in headers)
    if count!=465:
        errors.append(f"expected 465 original SALTS_C_API declarations; found {count}")
    unresolved=0
    for header in headers:
        names=[f.get("name") for f in header.get("functions",[])]
        if len(set(names))!=len(names):
            errors.append(f"duplicate function in {header.get('file')}")
        for function in header.get("functions",[]):
            state=function.get("review_status")
            if state not in KNOWN_STATES:
                errors.append(f"invalid parity status {header.get('file')}:{function.get('name')}")
            if state!="verified":
                unresolved+=1
            else:
                evidence=function.get("acceptance_evidence",{})
                if not (evidence.get("owner") and evidence.get("source")
                        and evidence.get("tests") and evidence.get("ci_head_sha")):
                    errors.append(f"false verified declaration: {header.get('file')}:{function.get('name')}")
    if full and unresolved:
        errors.append(f"FULL FUNCTION PARITY NOT ACCEPTED: {unresolved}/465 "
                      "original functions lack exact-owner behavioral acceptance")
    return errors

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path,default=ROOT)
    parser.add_argument("--full",action="store_true")
    args=parser.parse_args()
    source=json.loads((args.root/PATH).read_text(encoding="utf-8"))
    failures=check(source,full=args.full)
    for x in failures:
        print("FAIL:",x)
    if failures:return 1
    stats=Counter(f["review_status"] for h in source["headers"] for f in h["functions"])
    print("PASS: immutable 35-header / 465 SALTS_C_API function declaration inventory")
    print("NOT A FUNCTION PARITY PASS:",", ".join(
        f"{n}={v}" for n,v in sorted(stats.items())))
    return 0
if __name__=="__main__":
    raise SystemExit(main())
