#!/usr/bin/env python3
"""Mutation tests for the dormant exact Hyprland v2 validation envelope."""

from __future__ import annotations

import argparse
import copy
import runpy
import subprocess
import sys
import unittest
from pathlib import Path
from typing import Any, Callable

from jsonschema import Draft202012Validator


PARSER = argparse.ArgumentParser(description=__doc__)
PARSER.add_argument(
    "--validator",
    type=Path,
    default=Path(__file__).resolve().parents[1]
    / "tools"
    / "hyprland"
    / "validate_contract.py",
)
ARGUMENTS, UNITTEST_ARGUMENTS = PARSER.parse_known_args()


class DormantV2ValidatorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.validator_path = ARGUMENTS.validator.resolve()
        cls.root = cls.validator_path.parents[2]
        cls.module = runpy.run_path(cls.validator_path)

    def _load_contract(self) -> tuple[dict[Path, bytes], dict[Path, Any]]:
        raw: dict[Path, bytes] = {}
        documents: dict[Path, Any] = {}
        paths = set(self.module["SCHEMA_PATHS"])
        paths.update(
            instance_path
            for instance_path, _schema_path in self.module["INSTANCE_SCHEMA_PAIRS"]
        )
        for path in paths:
            raw[path], documents[path] = self.module["_strict_json"](
                self.root / path
            )
        return raw, documents

    def _assert_rejected(
        self,
        mutation: Callable[[dict[Path, Any]], None],
        message: str,
    ) -> None:
        raw, shipped = self._load_contract()
        documents = copy.deepcopy(shipped)
        mutation(documents)
        with self.assertRaisesRegex(ValueError, message):
            self.module["_validate_v2_coherence"](
                raw,
                documents,
                Draft202012Validator,
            )

    def test_shipped_schema_instances_and_coherence_are_valid(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(self.validator_path), "--root", str(self.root)],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            completed.returncode,
            0,
            completed.stdout + completed.stderr,
        )

    def test_source_ledger_root_count_and_order_are_closed(self) -> None:
        source_path = self.module["V2_SOURCE_MANIFEST_PATH"]

        def reverse_sources(documents: dict[Path, Any]) -> None:
            documents[source_path]["qualifiedSources"].reverse()

        self._assert_rejected(reverse_sources, "qualifiedSources.*sorted")

        def remove_changed_path(documents: dict[Path, Any]) -> None:
            documents[source_path]["changedPaths"].pop()

        self._assert_rejected(remove_changed_path, "changedPaths count")

        def reorder_root_keys(documents: dict[Path, Any]) -> None:
            source = documents[source_path]
            source["reviewedOn"] = source.pop("reviewedOn")

        self._assert_rejected(reorder_root_keys, "root keys or canonical order")

        def redate_exact_ledger(documents: dict[Path, Any]) -> None:
            documents[source_path]["reviewedOn"] = "2026-08-21"

        self._assert_rejected(redate_exact_ledger, "canonical source manifest digest")

    def test_source_identity_and_dependency_are_exact(self) -> None:
        source_path = self.module["V2_SOURCE_MANIFEST_PATH"]

        def stale_upstream(documents: dict[Path, Any]) -> None:
            documents[source_path]["upstream"]["commit"] = self.module[
                "V2_PREDECESSOR_COMMIT"
            ]

        self._assert_rejected(stale_upstream, "source upstream")

        def wrong_predecessor_tree(documents: dict[Path, Any]) -> None:
            documents[source_path]["predecessor"]["tree"] = "0" * 40

        self._assert_rejected(wrong_predecessor_tree, "source predecessor")

        def wrong_dependency(documents: dict[Path, Any]) -> None:
            documents[source_path]["dependencySources"][0]["revision"] = "0" * 40

        self._assert_rejected(wrong_dependency, "Hyprutils dependency")

    def test_source_domains_delta_and_patch_are_exact(self) -> None:
        source_path = self.module["V2_SOURCE_MANIFEST_PATH"]

        def reverse_domains(documents: dict[Path, Any]) -> None:
            sources = documents[source_path]["qualifiedSources"]
            source = next(record for record in sources if len(record["domains"]) > 1)
            source["domains"].reverse()

        self._assert_rejected(reverse_domains, "domains are not canonical")

        def wrong_added_path(documents: dict[Path, Any]) -> None:
            added = next(
                record
                for record in documents[source_path]["changedPaths"]
                if record["status"] == "A"
            )
            added["status"] = "M"

        self._assert_rejected(wrong_added_path, "exact single added path")

        def alter_hunk(documents: dict[Path, Any]) -> None:
            documents[source_path]["patches"][0]["exactHunk"] += "\n"

        self._assert_rejected(alter_hunk, "exactHunk digest")

        def stale_patch_target(documents: dict[Path, Any]) -> None:
            documents[source_path]["patches"][0]["targetPath"] = (
                "src/layout/space/Space.cpp"
            )

        self._assert_rejected(stale_patch_target, "patch targetPath")

    def test_both_catalogs_bind_the_identical_source_digest(self) -> None:
        catalog_path = self.module["V2_CATALOG_PATH"]
        action_path = self.module["V2_ACTION_CATALOG_PATH"]

        def wrong_catalog_source(documents: dict[Path, Any]) -> None:
            documents[catalog_path]["sourceManifestDigest"] = "0" * 64

        self._assert_rejected(wrong_catalog_source, "catalog sourceManifestDigest")

        def wrong_action_source(documents: dict[Path, Any]) -> None:
            documents[action_path]["sourceManifestDigest"] = "0" * 64

        self._assert_rejected(wrong_action_source, "action catalog sourceManifestDigest")

    def test_catalog_action_and_template_digest_chain_is_canonical(self) -> None:
        catalog_path = self.module["V2_CATALOG_PATH"]
        action_path = self.module["V2_ACTION_CATALOG_PATH"]
        template_path = self.module["V2_TEMPLATE_PATH"]

        def alter_catalog(documents: dict[Path, Any]) -> None:
            documents[catalog_path]["options"][0]["description"] += " changed"

        self._assert_rejected(alter_catalog, "canonical catalog digest")

        def alter_action(documents: dict[Path, Any]) -> None:
            documents[action_path]["dispatcherActions"][0]["description"] += (
                " changed"
            )

        self._assert_rejected(alter_action, "canonical action catalog digest")

        def alter_template(documents: dict[Path, Any]) -> None:
            documents[template_path]["targetHyprland"] = "0.56.3"

        self._assert_rejected(alter_template, "template recovered semantics")

    def test_exact_patch_two_compatibility_is_required_everywhere(self) -> None:
        catalog_path = self.module["V2_CATALOG_PATH"]
        action_path = self.module["V2_ACTION_CATALOG_PATH"]
        generation_path = self.module["V2_GENERATION_MANIFEST_PATH"]

        def broad_catalog(documents: dict[Path, Any]) -> None:
            documents[catalog_path]["hyprland"]["minimumPatch"] = 0

        self._assert_rejected(broad_catalog, "catalog Hyprland")

        def broad_action(documents: dict[Path, Any]) -> None:
            documents[action_path]["hyprland"]["maximumPatch"] = None

        self._assert_rejected(broad_action, "action catalog Hyprland")

        def broad_generation(documents: dict[Path, Any]) -> None:
            documents[generation_path]["compatibleHyprland"]["maximumPatch"] = 3

        self._assert_rejected(broad_generation, "generation compatibleHyprland")

    def test_generation_is_bound_to_authority_snapshot_and_all_catalogs(self) -> None:
        generation_path = self.module["V2_GENERATION_MANIFEST_PATH"]

        def alter_authority(documents: dict[Path, Any]) -> None:
            authority = documents[generation_path]["authorityId"]
            replacement = "f" if authority[0] != "f" else "e"
            documents[generation_path]["authorityId"] = replacement + authority[1:]

        self._assert_rejected(alter_authority, "generation snapshotDigest")

        def alter_source_binding(documents: dict[Path, Any]) -> None:
            documents[generation_path]["sourceManifestDigest"] = "0" * 64

        self._assert_rejected(alter_source_binding, "generation sourceManifestDigest")

        def alter_payload(documents: dict[Path, Any]) -> None:
            documents[generation_path]["createdAt"] = "2026-08-24T00:00:00Z"

        self._assert_rejected(alter_payload, "canonical payload digest")

    def test_stale_point_one_digest_identity_and_wildcard_are_forbidden(self) -> None:
        template_path = self.module["V2_TEMPLATE_PATH"]
        generation_path = self.module["V2_GENERATION_MANIFEST_PATH"]

        def stale_digest(documents: dict[Path, Any]) -> None:
            documents[template_path]["actionCatalogDigest"] = (
                "72edec68c743e90644b11dd686a4d4520a5b16bcbefe63961cf0a0050aee8d8a"
            )

        self._assert_rejected(stale_digest, "forbidden stale 0.56.1 digest")

        def stale_identity(documents: dict[Path, Any]) -> None:
            documents[generation_path]["targetHyprland"] = "0.56.1"

        self._assert_rejected(stale_identity, "stale 0.56.1 identity")

        def wildcard_identity(documents: dict[Path, Any]) -> None:
            documents[generation_path]["targetHyprland"] = "0.56.x"

        self._assert_rejected(wildcard_identity, "forbidden 0.56.x")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *UNITTEST_ARGUMENTS])
