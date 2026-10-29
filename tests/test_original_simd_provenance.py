"""Original SIMD source/API preservation, separate from runtime acceptance.

The immutable 610-function ledger captured zero functions from simd.h. This
supplement pins its 32 public declarations and seven original files without
rewriting that historical ledger. Native and installed tests establish runtime
behavior; hashes here establish only the explicitly bounded source changes.
"""
from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = "tools/migration/original-simd-inventory.json"
INVENTORY_BLOB = "7ea91f1654e32a8d6baf054ff67143c43d8d04c3"
ORIGINAL_FILES = "tools/migration/original-salts-files.json"
ORIGINAL_FILES_BLOB = "05bc04582efc1e000b35f76288ee3b35fbe45e06"
ORIGINAL_API = "tools/migration/original-public-functions.json"
ORIGINAL_API_BLOB = "19db69a508c6049afcfe6922e19448f5f9c5d875"
C_TOKENS = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii")
                        + b"\0" + data).hexdigest()


def tracked_bytes(path: Path) -> bytes:
    # Git checkout CRLF is the only accepted host-dependent normalization.
    return path.read_bytes().replace(b"\r\n", b"\n")


def frozen_json(root: Path, relative: str, expected: str) -> dict:
    raw = tracked_bytes(root / relative)
    if git_blob(raw) != expected:
        raise ValueError("frozen SIMD provenance modified: " + relative)
    return json.loads(raw)


def restore_original_identifiers(text: str) -> str:
    return (text.replace("salts_simd_desc", "cmeta_vector_desc")
            .replace("salts_simd_", "cmeta_vector_")
            .replace("SALTS_SIMD_LANE_", "CMETA_VECTOR_"))


def mask_c(text: str, literals: bool = True) -> str:
    def replace(match: re.Match) -> str:
        if not literals and not match[0].startswith(("//", "/*")):
            return match[0]
        return "".join("\n" if c == "\n" else " " for c in match[0])
    return C_TOKENS.sub(replace, text)


def function_blocks(text: str) -> dict[str, str]:
    """Extract the original top-level void test functions, retaining bytes."""
    masked = mask_c(text)
    blocks = {}
    for match in re.finditer(
            r"(?m)^static\s+void\s+(test_\w+)\s*\(\s*void\s*\)\s*\{", masked):
        depth, end = 1, match.end()
        while depth and end < len(masked):
            depth += (masked[end] == "{") - (masked[end] == "}")
            end += 1
        if depth or match[1] in blocks:
            raise ValueError("malformed or duplicated original SIMD test function")
        blocks[match[1]] = text[match.start():end]
    return blocks


def public_declarations(text: str) -> list[dict[str, str]]:
    text = restore_original_identifiers(mask_c(text, literals=False))
    text = re.sub(r"\bSALTS_SIMD_API\b", "", text)
    return [{"name": m[1], "declaration_tokens": " ".join(
                re.findall(r"[A-Za-z_]\w*|\d+|[^\s]", m[0]))}
            for m in re.finditer(
                r"(?m)^[ \t]*(?:void|bool)\s+(cmeta_simd_\w+)"
                r"\s*\([^;{}]*\)\s*;", text)]


def verify_original_inventory(root: Path = ROOT) -> dict:
    inventory = frozen_json(root, INVENTORY, INVENTORY_BLOB)
    originals = frozen_json(root, ORIGINAL_FILES, ORIGINAL_FILES_BLOB)
    original_api = frozen_json(root, ORIGINAL_API, ORIGINAL_API_BLOB)
    files = {row["path"]: row for row in originals["files"]}
    if len(inventory["files"]) != 7 or inventory["public_api"]["count"] != 32:
        raise ValueError("original SIMD seven-file/32-function baseline truncated")
    if len(inventory["public_api"]["functions"]) != 32:
        raise ValueError("original SIMD declarations missing")
    for row in inventory["files"]:
        historical = files.get(row["path"], {})
        if (historical.get("git_blob") != row["git_blob"] or
                historical.get("size") != row["bytes"]):
            raise ValueError("original SIMD source blob lost: " + row["path"])
    header = next(row for row in original_api["headers"]
                  if row["path"] == inventory["public_api"]["header"])
    if header["explicit_functions"] or sum(
            len(row["explicit_functions"]) for row in original_api["headers"]) != 610:
        raise ValueError("historical SIMD extraction gap or 610 ledger changed")
    reflection = inventory["upper_layer_reflection"]
    if (reflection["status"] != "pending-upstream-reflection-acceptance" or
            git_blob(reflection["original_block"].encode("utf-8")) !=
            reflection["original_block_git_blob"] or
            inventory["acceptance"]["full_migration_complete"]):
        raise ValueError("original reflection assertions falsely accepted or lost")
    return inventory


def verify_module(root: Path = ROOT) -> None:
    inventory = verify_original_inventory(root)
    original = {row["path"]: row["git_blob"] for row in inventory["files"]}
    for path in original:
        if not (root / path).is_file():
            raise ValueError("original SIMD destination missing: " + path)

    source = restore_original_identifiers(
        tracked_bytes(root / "simd/src/simd.c").decode("utf-8"))
    if git_blob(source.encode("utf-8")) != original["simd/src/simd.c"]:
        raise ValueError("original SIMD algorithm source changed or truncated")
    header = tracked_bytes(root / inventory["public_api"]["header"]).decode("utf-8")
    if public_declarations(header) != inventory["public_api"]["functions"]:
        raise ValueError("original SIMD public declaration missing or changed")

    cpp = inventory["original_cpp_test"]["file"]
    if git_blob(tracked_bytes(root / cpp)) != original[cpp]:
        raise ValueError("original SIMD C++17 test changed")
    test_info = inventory["original_numeric_tests"]
    test_source = restore_original_identifiers(
        tracked_bytes(root / test_info["file"]).decode("utf-8"))
    blocks = function_blocks(test_source)
    for function in test_info["functions"]:
        block = blocks.get(function["name"], "")
        if git_blob(block.encode("utf-8")) != function["git_blob"]:
            raise ValueError("original SIMD numerical test missing or changed: "
                             + function["name"])
    cases = [{"name": m[1], "function": m[2]} for m in re.finditer(
        r'\bit\("([^"]+)"\)\s*\{\s*(test_\w+)\(\);\s*\}',
        mask_c(test_source, literals=False))]
    if cases != test_info["case_registrations"] or "test_descriptors" not in blocks:
        raise ValueError("original SIMD TinyTest case registration missing or changed")


@contextmanager
def isolated_copy():
    inventory = verify_original_inventory()
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        for relative in [INVENTORY, ORIGINAL_FILES, ORIGINAL_API] + [
                row["path"] for row in inventory["files"]]:
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        yield root


class OriginalSIMDProvenanceTests(unittest.TestCase):
    def test_seven_original_files_and_32_additional_declarations_stay_pinned(self):
        inventory = verify_original_inventory()
        self.assertEqual(inventory["original_numeric_tests"]["count"], 24)
        self.assertEqual(len(inventory["original_numeric_tests"]["functions"]), 24)
        self.assertEqual(len(inventory["original_numeric_tests"]["case_registrations"]), 25)
        self.assertEqual(inventory["baseline_extraction_gap"]["simd_functions_captured"], 0)

    def test_native_sources_preserve_original_algorithm_and_numerical_cases(self):
        verify_module()

    def test_supplement_cannot_drop_an_original_file_or_public_function(self):
        for field in ("files", "functions"):
            with self.subTest(field=field), isolated_copy() as root:
                path = root / INVENTORY
                data = json.loads(path.read_text(encoding="utf-8"))
                (data["files"] if field == "files" else
                 data["public_api"]["functions"]).pop()
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "frozen SIMD provenance modified"):
                    verify_module(root)

    def test_deleted_renamed_or_retyped_original_public_api_is_rejected(self):
        for mutation in ("deleted", "renamed", "retyped"):
            with self.subTest(mutation=mutation), isolated_copy() as root:
                path = root / "simd/include/salts/simd.h"
                header = path.read_text(encoding="utf-8")
                declaration = re.search(
                    r"(?:SALTS_SIMD_API\s+)?bool\s+cmeta_simd_extract_lane"
                    r"\s*\([^;{}]*\)\s*;", header)
                self.assertIsNotNone(declaration)
                old = declaration[0]
                replacement = {"deleted": "", "renamed": old.replace(
                    "cmeta_simd_extract_lane", "removed_simd_extract_lane"),
                    "retyped": old.replace("uint16_t lane", "uint32_t lane")}[mutation]
                self.assertNotEqual(old, replacement)
                path.write_text(header.replace(old, replacement, 1), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "public declaration missing or changed"):
                    verify_module(root)

    def test_truncated_or_modified_algorithm_is_rejected(self):
        for mutation in ("truncated", "modified"):
            with self.subTest(mutation=mutation), isolated_copy() as root:
                path = root / "simd/src/simd.c"
                source = path.read_text(encoding="utf-8")
                changed = source[:-128] if mutation == "truncated" else source.replace(
                    "simde_wasm_v128_load8_splat", "simde_wasm_v128_load16_splat", 1)
                self.assertNotEqual(source, changed)
                path.write_text(changed, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "algorithm source changed or truncated"):
                    verify_module(root)

    def test_original_numerical_case_body_and_registration_cannot_disappear(self):
        for mutation in ("body", "assertion", "registration"):
            with self.subTest(mutation=mutation), isolated_copy() as root:
                path = root / "simd/tests/simd_test.c"
                source = path.read_text(encoding="utf-8")
                block = function_blocks(source)["test_load_extend"]
                if mutation == "body":
                    changed = source.replace(block, "", 1)
                elif mutation == "assertion":
                    changed = source.replace(block, block.replace(
                        "check(memcmp(", "check(0 && memcmp(", 1), 1)
                else:
                    changed = source.replace(
                        'it("load extend") { test_load_extend(); }', "", 1)
                self.assertNotEqual(source, changed)
                path.write_text(changed, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "numerical test|case registration"):
                    verify_module(root)


if __name__ == "__main__":
    unittest.main()
