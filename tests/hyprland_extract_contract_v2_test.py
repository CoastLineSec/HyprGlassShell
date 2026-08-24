#!/usr/bin/env python3
"""Focused regressions for the dormant Hyprland 0.56.2 contract generator."""

from __future__ import annotations

import argparse
from contextlib import redirect_stderr
import hashlib
import io
import json
from pathlib import Path
import runpy
import sys
import unittest
from unittest.mock import Mock, patch

from jsonschema import Draft202012Validator, FormatChecker


parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--extractor", type=Path, required=True)
arguments, remaining = parser.parse_known_args()
module = runpy.run_path(
    arguments.extractor.as_posix(), run_name="hyprshelld_extract_contract_v2_test"
)
root = arguments.extractor.resolve().parents[2]


def load_json(relative_path: str) -> dict:
    return json.loads((root / relative_path).read_text(encoding="utf-8"))


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class V2SchemaClosureTest(unittest.TestCase):
    CASES = (
        (
            "data/hyprland/config-catalog-v2.json",
            "interfaces/hyprland/v2/catalog.schema.json",
        ),
        (
            "data/hyprland/action-catalog-v2.json",
            "interfaces/hyprland/v2/action-catalog.schema.json",
        ),
        (
            "data/hyprland/source-manifest-v2.json",
            "interfaces/hyprland/v2/source-manifest.schema.json",
        ),
        (
            "data/defaults/hyprland-template.json",
            "interfaces/hyprland/v2/config-template.schema.json",
        ),
        (
            "tests/fixtures/hyprland/generation-manifest-v2.json",
            "interfaces/hyprland/v2/generation-manifest.schema.json",
        ),
    )

    def test_every_v2_artifact_matches_its_closed_schema(self) -> None:
        for artifact_path, schema_path in self.CASES:
            with self.subTest(artifact=artifact_path):
                validator = Draft202012Validator(
                    load_json(schema_path), format_checker=FormatChecker()
                )
                errors = sorted(
                    validator.iter_errors(load_json(artifact_path)),
                    key=lambda error: tuple(str(part) for part in error.path),
                )
                self.assertEqual(
                    errors,
                    [],
                    "\n".join(
                        f"{list(error.path)}: {error.message}" for error in errors
                    ),
                )

    def test_runtime_authority_vector_matches_config_schema(self) -> None:
        template = load_json("data/defaults/hyprland-template.json")
        runtime = {
            "formatVersion": template["formatVersion"],
            "authorityId": "0123456789abcdef0123456789abcdef",
            **{
                key: value
                for key, value in template.items()
                if key != "formatVersion"
            },
        }
        Draft202012Validator(
            load_json("interfaces/hyprland/v2/config.schema.json"),
            format_checker=FormatChecker(),
        ).validate(runtime)


class V2DigestBindingTest(unittest.TestCase):
    def setUp(self) -> None:
        self.catalog = load_json("data/hyprland/config-catalog-v2.json")
        self.action = load_json("data/hyprland/action-catalog-v2.json")
        self.source = load_json("data/hyprland/source-manifest-v2.json")
        self.template = load_json("data/defaults/hyprland-template.json")
        self.generation = load_json(
            "tests/fixtures/hyprland/generation-manifest-v2.json"
        )

    def test_source_manifest_digest_binds_both_catalogs_and_generation(self) -> None:
        expected = sha256(module["_canonical_json_bytes"](self.source))
        self.assertEqual(self.catalog["sourceManifestDigest"], expected)
        self.assertEqual(self.action["sourceManifestDigest"], expected)
        self.assertEqual(self.generation["sourceManifestDigest"], expected)

    def test_template_binds_exact_catalog_and_action_schema_bytes(self) -> None:
        expected_catalog = sha256(module["_canonical_json_bytes"](self.catalog))
        config_schema = (
            root / "interfaces/hyprland/v2/config.schema.json"
        ).read_bytes()
        expected_action = sha256(
            module["_canonical_json_bytes"](self.action) + b"\n" + config_schema
        )
        self.assertEqual(self.template["catalogDigest"], expected_catalog)
        self.assertEqual(self.template["actionCatalogDigest"], expected_action)
        self.assertEqual(self.generation["catalogDigest"], expected_catalog)
        self.assertEqual(self.generation["actionCatalogDigest"], expected_action)

    def test_generation_hash_omits_only_generation(self) -> None:
        payload = {
            key: value
            for key, value in self.generation.items()
            if key != "generation"
        }
        self.assertEqual(
            self.generation["generation"],
            sha256(module["_canonical_json_bytes"](payload)),
        )

    def test_second_authority_changes_snapshot_and_generation(self) -> None:
        first = module["_v2_generation_manifest"](
            self.template,
            self.catalog["sourceManifestDigest"],
            "0123456789abcdef0123456789abcdef",
        )
        second = module["_v2_generation_manifest"](
            self.template,
            self.catalog["sourceManifestDigest"],
            "fedcba9876543210fedcba9876543210",
        )
        self.assertEqual(first, self.generation)
        self.assertNotEqual(first["authorityId"], second["authorityId"])
        self.assertNotEqual(first["snapshotDigest"], second["snapshotDigest"])
        self.assertNotEqual(first["generation"], second["generation"])
        ignored = {"authorityId", "snapshotDigest", "generation"}
        self.assertEqual(
            {key: value for key, value in first.items() if key not in ignored},
            {key: value for key, value in second.items() if key not in ignored},
        )


class V2ClosedInventoryTest(unittest.TestCase):
    def setUp(self) -> None:
        self.catalog = load_json("data/hyprland/config-catalog-v2.json")
        self.action = load_json("data/hyprland/action-catalog-v2.json")
        self.source = load_json("data/hyprland/source-manifest-v2.json")

    def test_exact_patch_range_is_two_in_both_catalogs(self) -> None:
        for hyprland in (self.catalog["hyprland"], self.action["hyprland"]):
            self.assertEqual(hyprland["reviewedVersion"], "0.56.2")
            self.assertEqual(hyprland["minimumPatch"], 2)
            self.assertEqual(hyprland["maximumPatch"], 2)
        self.assertEqual(
            self.catalog["compatibility"]["fullyQualified"], ["0.56.2"]
        )

    def test_scalar_inventory_and_action_arrays_are_preserved_exactly(self) -> None:
        predecessor_catalog = load_json("data/hyprland/config-catalog-v1.json")
        predecessor_action = load_json("data/hyprland/action-catalog-v1.json")
        self.assertEqual(len(self.catalog["options"]), 353)
        self.assertEqual(self.catalog["options"], predecessor_catalog["options"])
        self.assertEqual(
            self.catalog["complexSurfaces"], predecessor_catalog["complexSurfaces"]
        )
        for key in (
            "dispatcherActions",
            "semanticActions",
            "gestureActions",
            "excluded",
        ):
            self.assertEqual(self.action[key], predecessor_action[key])

    def test_scalar_fixture_and_delta_are_exactly_empty(self) -> None:
        predecessor = load_json(
            "tests/fixtures/hyprland/v0.56.1.scalar-options.json"
        )
        upstream = load_json("tests/fixtures/hyprland/v0.56.2.scalar-options.json")
        delta = load_json(
            "tests/fixtures/hyprland/v0.56.1-to-v0.56.2.delta.json"
        )
        self.assertEqual(upstream["optionCount"], 353)
        self.assertEqual(upstream["options"], predecessor["options"])
        self.assertEqual(
            delta,
            {
                "formatVersion": 1,
                "from": "0.56.1",
                "to": "0.56.2",
                "added": [],
                "removed": [],
                "changed": [],
            },
        )

    def test_closed_source_change_dependency_and_patch_ledgers(self) -> None:
        self.assertEqual(
            list(self.source),
            [
                "formatVersion",
                "repository",
                "reviewedOn",
                "predecessor",
                "upstream",
                "qualifiedSources",
                "dependencySources",
                "changedPaths",
                "patches",
                "documentation",
            ],
        )
        qualified = self.source["qualifiedSources"]
        changed = self.source["changedPaths"]
        self.assertEqual(len(qualified), 174)
        self.assertEqual(len(changed), 35)
        self.assertEqual(
            [record["path"] for record in qualified],
            sorted(record["path"] for record in qualified),
        )
        self.assertEqual(
            [record["path"] for record in changed],
            sorted(record["path"] for record in changed),
        )
        effective_changes = [
            record
            for record in qualified
            if record["upstreamSha256"] != record["effectiveSha256"]
        ]
        self.assertEqual(
            [record["path"] for record in effective_changes],
            ["src/config/lua/bindings/LuaBindingsConfigRules.cpp"],
        )
        added = [record for record in changed if record["status"] == "A"]
        self.assertEqual(
            [(record["path"], record["preimageSha256"]) for record in added],
            [("hyprtester/src/tests/main/monitor_rules.cpp", None)],
        )
        domain_rank = {
            domain: index for index, domain in enumerate(module["V2_DOMAIN_ORDER"])
        }
        for record in qualified:
            with self.subTest(path=record["path"]):
                self.assertTrue(record["domains"])
                self.assertEqual(len(record["domains"]), len(set(record["domains"])))
                self.assertEqual(
                    record["domains"],
                    sorted(record["domains"], key=domain_rank.__getitem__),
                )
        dependency = self.source["dependencySources"]
        self.assertEqual(len(dependency), 1)
        self.assertEqual(
            [record["path"] for record in dependency[0]["paths"]],
            [path for path, _ in module["V2_HYPRUTILS"]["paths"]],
        )
        patch = self.source["patches"]
        self.assertEqual(len(patch), 1)
        self.assertEqual(patch[0]["targetPath"], module["V2_PATCH"]["targetPath"])
        self.assertEqual(
            sha256(patch[0]["exactHunk"].encode("utf-8")),
            module["V2_PATCH"]["hunkSha256"],
        )
        self.assertTrue(patch[0]["exactHunk"].endswith("\n"))


class V1IsolationTest(unittest.TestCase):
    FROZEN_SHA256 = {
        "data/hyprland/config-catalog-v1.json": "b6630b5cad74535c87415f2fe3716529c7fa7e80fa9e2b52d0fe8af48d4f9042",
        "data/hyprland/action-catalog-v1.json": "b4e0fe0daa693477340287b933ca8ea752816221e5f80cec51c249d892904f5e",
        "data/defaults/hyprland.json": "fb760dcef60232ccce78545b77f2b6eea8dd34429cda707cfa82ee8db39050de",
        "tests/fixtures/hyprland/v0.55.0.scalar-options.json": "65368b0f2a2d3a85df0d2b0be73623225afbd63556a429d7d53601bf9b537362",
        "tests/fixtures/hyprland/v0.56.1.scalar-options.json": "1317153f21c74ad478b02a284cbced4c3f68dfb742d10b7254ba986d5b78ef6f",
        "tests/fixtures/hyprland/v0.55.0-to-v0.56.1.delta.json": "348b8fe500200312f370d62b9d4a7a5fbcbfe755c7a46392e8ceee21f2d9ced3",
        "tests/fixtures/hyprland/source-manifest.json": "eeba4cfdb69f507aa6a4c9a3ace60e17190dbf2e6545090d33cfd3998e857be4",
        "tests/fixtures/hyprland/generation-manifest.json": "0c588152700bcc260b58cd4c1f75ac5ac79efb7d51726340a7f1321ddcf65401",
    }

    def test_v1_artifacts_remain_byte_identical(self) -> None:
        for relative_path, expected in self.FROZEN_SHA256.items():
            with self.subTest(path=relative_path):
                self.assertEqual(sha256((root / relative_path).read_bytes()), expected)

    def test_v1_and_v2_output_maps_are_disjoint(self) -> None:
        v2 = {
            Path("data/hyprland/config-catalog-v2.json"),
            Path("data/hyprland/action-catalog-v2.json"),
            Path("data/hyprland/source-manifest-v2.json"),
            Path("data/defaults/hyprland-template.json"),
            Path("tests/fixtures/hyprland/v0.56.2.scalar-options.json"),
            Path("tests/fixtures/hyprland/v0.56.1-to-v0.56.2.delta.json"),
            Path("tests/fixtures/hyprland/generation-manifest-v2.json"),
        }
        v1 = {Path(path) for path in self.FROZEN_SHA256}
        self.assertFalse(v1 & v2)


class LegacyCliCompatibilityTest(unittest.TestCase):
    LEGACY_ARGUMENTS = [
        "extract_contract.py",
        "--source-055",
        "source-055",
        "--source-0560",
        "source-0560",
        "--source-056",
        "source-056",
        "--hyprutils-055",
        "hyprutils-055",
        "--hyprutils-056",
        "hyprutils-056",
        "--output-root",
        ".",
        "--check",
    ]

    def test_no_v2_arguments_runs_only_the_legacy_builder(self) -> None:
        v1_builder = Mock(return_value={})
        v2_builder = Mock(side_effect=AssertionError("v2 builder must stay dormant"))
        globals_ = module["main"].__globals__
        with (
            patch.object(sys, "argv", self.LEGACY_ARGUMENTS),
            patch.dict(
                globals_,
                {
                    "build_documents": v1_builder,
                    "build_v2_documents": v2_builder,
                },
            ),
        ):
            self.assertEqual(module["main"](), 0)
        v1_builder.assert_called_once()
        v2_builder.assert_not_called()

    def test_partial_v2_argument_group_returns_two_before_generation(self) -> None:
        v1_builder = Mock(return_value={})
        v2_builder = Mock(return_value={})
        globals_ = module["main"].__globals__
        stderr = io.StringIO()
        with (
            patch.object(
                sys,
                "argv",
                [*self.LEGACY_ARGUMENTS, "--source-0562", "source-0562"],
            ),
            patch.dict(
                globals_,
                {
                    "build_documents": v1_builder,
                    "build_v2_documents": v2_builder,
                },
            ),
            redirect_stderr(stderr),
        ):
            self.assertEqual(module["main"](), 2)
        self.assertIn("must be supplied together", stderr.getvalue())
        v1_builder.assert_not_called()
        v2_builder.assert_not_called()

    def test_complete_v2_argument_group_is_selected(self) -> None:
        selected = module["_v2_cli_inputs"](
            Path("source-0562"),
            Path("hyprutils-0562"),
            Path("protected.patch"),
        )
        self.assertEqual(
            selected,
            (
                Path("source-0562"),
                Path("hyprutils-0562"),
                Path("protected.patch"),
            ),
        )


if __name__ == "__main__":
    unittest.main(argv=[arguments.extractor.as_posix(), *remaining])
