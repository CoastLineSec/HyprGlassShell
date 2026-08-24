#!/usr/bin/env python3

from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import sys
import unittest
from pathlib import Path


def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--validator", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--artifact", type=Path, required=True)
    arguments, remaining = parser.parse_known_args()
    sys.argv = [sys.argv[0], *remaining]
    return arguments


ARGUMENTS = _arguments()
SPEC = importlib.util.spec_from_file_location(
    "validate_legacy_shortcuts", ARGUMENTS.validator
)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load legacy shortcut validator")
VALIDATOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VALIDATOR
SPEC.loader.exec_module(VALIDATOR)


class LegacyShortcutSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = ARGUMENTS.source.read_bytes()
        cls.artifact = ARGUMENTS.artifact.read_bytes()
        cls.document = VALIDATOR.build_document(cls.source)

    def test_authority_and_exact_inventory(self) -> None:
        self.assertEqual(hashlib.sha256(self.source).hexdigest(), VALIDATOR.SOURCE_SHA256)
        self.assertEqual(len(self.source), 9738)
        self.assertEqual(len(self.source.splitlines()), 171)
        self.assertTrue(self.source.endswith(b"\n"))
        self.assertNotIn(b"\r", self.source)
        self.assertNotIn(b"hl.submap", self.source)

        rows = self.document["rows"]
        self.assertEqual(len(rows), 117)
        self.assertEqual(len({row["section"] for row in rows}), 24)
        self.assertEqual(
            [row["ordinal"] for row in rows],
            list(range(1, 118)),
        )
        self.assertEqual(len({row["chord"] for row in rows}), 117)

        locked = [row for row in rows if row["options"].get("locked") is True]
        repeating = [
            row for row in rows if row["options"].get("repeating") is True
        ]
        mouse = [row for row in rows if row["options"].get("mouse") is True]
        descriptions = [
            row for row in rows if "description" in row["options"]
        ]
        both = [
            row
            for row in rows
            if row["options"].get("locked") is True
            and row["options"].get("repeating") is True
        ]
        self.assertEqual(len(locked), 12)
        self.assertEqual(len(repeating), 10)
        self.assertEqual(len(both), 6)
        self.assertEqual(len(mouse), 2)
        self.assertEqual(len(descriptions), 4)

    def test_sensitive_rows_remain_literal(self) -> None:
        rows = self.document["rows"]
        self.assertEqual(rows[0]["sourceLine"], 8)
        self.assertEqual(rows[0]["chord"], "SUPER + T")
        self.assertEqual(rows[0]["action"], 'hl.dsp.exec_cmd("hgs-terminal")')

        self.assertEqual(rows[5]["chord"], "SUPER + comma")
        self.assertEqual(
            rows[5]["action"],
            'hl.dsp.exec_cmd("hgs ipc call settings focusOrToggle")',
        )
        self.assertEqual(rows[5]["options"], {})

        self.assertEqual(rows[49]["chord"], "SUPER + Home")
        self.assertEqual(
            rows[49]["action"], 'hl.dsp.focus({ window = "first" })'
        )
        self.assertEqual(rows[50]["chord"], "SUPER + End")
        self.assertEqual(
            rows[50]["action"], 'hl.dsp.focus({ window = "last" })'
        )

        self.assertEqual(rows[104]["options"], {
            "description": "Move window",
            "mouse": True,
        })
        self.assertEqual(rows[105]["options"], {
            "description": "Resize window",
            "mouse": True,
        })

        self.assertEqual(rows[-1]["sourceLine"], 171)
        self.assertEqual(rows[-1]["chord"], "SUPER + SHIFT + P")
        self.assertEqual(
            rows[-1]["action"], 'hl.dsp.dpms({ action = "toggle" })'
        )

    def test_committed_artifact_is_exact_canonical_derivation(self) -> None:
        VALIDATOR.check_artifact(self.source, self.artifact)
        self.assertEqual(
            self.artifact,
            VALIDATOR.canonical_json_bytes(self.document),
        )
        self.assertEqual(
            VALIDATOR.parse_artifact(self.artifact),
            self.document,
        )

    def test_source_mutations_fail_closed(self) -> None:
        with self.assertRaisesRegex(
            VALIDATOR.LegacyShortcutError, "digest mismatch"
        ):
            VALIDATOR.build_document(self.source.replace(
                b"SUPER + comma", b"SUPER + period", 1
            ))

        mutations = [
            self.source.replace(
                b'hl.bind("SUPER + T",',
                b'print("unexpected")\nhl.bind("SUPER + T",',
                1,
            ),
            self.source.replace(
                b'{ locked = true }',
                b'{ locked = true, locked = true }',
                1,
            ),
            self.source.replace(
                b'{ locked = true }',
                b'{ invented = true }',
                1,
            ),
            self.source.replace(
                b'{ direction = "l" }',
                b'{ direction = { nested = true } }',
                1,
            ),
            self.source.replace(
                b'hl.dsp.exit()',
                b'hl.unknown.exit()',
                1,
            ),
            self.source.replace(
                b'[[hgs ipc call brightness increment 5 ""]]',
                b'[[safe]] and os.execute("id")[x[1]]',
                1,
            ),
            self.source.replace(
                b'-- Keybinds Configuration',
                b'--[[ignored]] os.execute("id")',
                1,
            ),
        ]
        for mutated in mutations:
            with self.subTest(digest=hashlib.sha256(mutated).hexdigest()):
                with self.assertRaises(VALIDATOR.LegacyShortcutError):
                    VALIDATOR.build_document(
                        mutated,
                        expected_digest=hashlib.sha256(mutated).hexdigest(),
                    )

    def test_artifact_semantic_mutations_fail_source_comparison(self) -> None:
        mutations = []
        for path, value in [
            (("contractVersion",), 2),
            (("source", "sha256"), "0" * 64),
            (("rows", 5, "chord"), "SUPER + period"),
            (("rows", 49, "action"), 'hl.dsp.focus({ window = "last" })'),
            (("rows", 104, "options", "mouse"), False),
            (("rows", 116, "section"), "Invented"),
        ]:
            mutated = copy.deepcopy(self.document)
            target = mutated
            for component in path[:-1]:
                target = target[component]
            target[path[-1]] = value
            mutations.append(VALIDATOR.canonical_json_bytes(mutated))

        reordered = copy.deepcopy(self.document)
        reordered["rows"][0], reordered["rows"][1] = (
            reordered["rows"][1],
            reordered["rows"][0],
        )
        mutations.append(VALIDATOR.canonical_json_bytes(reordered))

        missing = copy.deepcopy(self.document)
        missing["rows"].pop()
        mutations.append(VALIDATOR.canonical_json_bytes(missing))

        for mutated in mutations:
            with self.subTest(digest=hashlib.sha256(mutated).hexdigest()):
                with self.assertRaisesRegex(
                    VALIDATOR.LegacyShortcutError, "differs from source"
                ):
                    VALIDATOR.check_artifact(self.source, mutated)

    def test_noncanonical_and_duplicate_json_fail(self) -> None:
        with self.assertRaisesRegex(
            VALIDATOR.LegacyShortcutError, "not canonical"
        ):
            VALIDATOR.parse_artifact(self.artifact + b"\n")

        duplicate = b'{"contractVersion":1,"contractVersion":1,"rows":[],"source":{}}\n'
        with self.assertRaisesRegex(
            VALIDATOR.LegacyShortcutError, "duplicate JSON key"
        ):
            VALIDATOR.parse_artifact(duplicate)

        for constant in (b"NaN", b"Infinity", b"-Infinity"):
            with self.subTest(constant=constant):
                with self.assertRaisesRegex(
                    VALIDATOR.LegacyShortcutError,
                    "unsupported JSON constant",
                ):
                    VALIDATOR.parse_artifact(
                        b'{"value":' + constant + b"}\n"
                    )

        with self.assertRaisesRegex(
            VALIDATOR.LegacyShortcutError,
            "unsupported numeric value",
        ):
            VALIDATOR.parse_artifact(b'{"value":1e9999}\n')


if __name__ == "__main__":
    unittest.main()
