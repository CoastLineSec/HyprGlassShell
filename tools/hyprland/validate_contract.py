#!/usr/bin/env python3
"""Validate the checked-in Hyprland v1 schemas, instances, and digests."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
from typing import Any


SCHEMA_DIRECTORY = Path("interfaces/hyprland/v1")
SCHEMA_FILES = (
    "catalog.schema.json",
    "config.schema.json",
    "action-catalog.schema.json",
    "generation-manifest.schema.json",
    "source-manifest.schema.json",
)
INSTANCE_SCHEMA_PAIRS = (
    (Path("data/hyprland/config-catalog-v1.json"), "catalog.schema.json"),
    (Path("data/defaults/hyprland.json"), "config.schema.json"),
    (Path("data/hyprland/action-catalog-v1.json"), "action-catalog.schema.json"),
    (Path("tests/fixtures/hyprland/source-manifest.json"), "source-manifest.schema.json"),
    (Path("tests/fixtures/hyprland/generation-manifest.json"), "generation-manifest.schema.json"),
)


def _invalid_json_constant(token: str) -> None:
    raise ValueError(f"non-JSON numeric constant {token!r}")


def _strict_json(path: Path) -> tuple[bytes, Any]:
    data = path.read_bytes()

    def object_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{path}: duplicate JSON key {key!r}")
            result[key] = value
        return result

    try:
        document = json.loads(
            data,
            object_pairs_hook=object_pairs,
            parse_constant=_invalid_json_constant,
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"{path}: invalid JSON: {error}") from error
    return data, document


def _canonical_json_bytes(document: Any) -> bytes:
    def normalize(value: Any) -> Any:
        if isinstance(value, dict):
            return {key: normalize(child) for key, child in value.items()}
        if isinstance(value, list):
            return [normalize(child) for child in value]
        if isinstance(value, float) and math.isfinite(value) and value.is_integer():
            return int(value)
        return value

    return json.dumps(
        normalize(document),
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _validate_local_references(schema: dict[str, Any], label: str) -> None:
    definitions = schema.get("$defs")
    if not isinstance(definitions, dict):
        raise ValueError(f"{label}: schema has no $defs object")

    def visit(value: Any, path: str) -> None:
        if isinstance(value, list):
            for index, child in enumerate(value):
                visit(child, f"{path}/{index}")
            return
        if not isinstance(value, dict):
            return
        reference = value.get("$ref")
        if reference is not None:
            if not isinstance(reference, str) or not reference.startswith("#/$defs/"):
                raise ValueError(f"{label}{path}: nonlocal or malformed $ref {reference!r}")
            name = reference.removeprefix("#/$defs/")
            if not name or "/" in name or name not in definitions:
                raise ValueError(f"{label}{path}: unresolved local $ref {reference!r}")
        for key, child in value.items():
            visit(child, f"{path}/{key}")

    visit(schema, "")


def _validate_coherence(
    raw: dict[Path, bytes],
    documents: dict[Path, Any],
) -> None:
    catalog_path = Path("data/hyprland/config-catalog-v1.json")
    config_path = SCHEMA_DIRECTORY / "config.schema.json"
    action_path = Path("data/hyprland/action-catalog-v1.json")
    defaults_path = Path("data/defaults/hyprland.json")
    generation_path = Path("tests/fixtures/hyprland/generation-manifest.json")

    catalog = documents[catalog_path]
    action_catalog = documents[action_path]
    defaults = documents[defaults_path]
    generation = documents[generation_path]
    config_schema = documents[config_path]

    expected_catalog_digest = _sha256(_canonical_json_bytes(catalog))
    if defaults["catalogDigest"] != expected_catalog_digest:
        raise ValueError("default catalogDigest does not bind the canonical catalog")

    expected_config_digest = _sha256(raw[config_path])
    if action_catalog["configSchemaDigest"] != expected_config_digest:
        raise ValueError("action catalog configSchemaDigest does not bind config.schema.json")

    expected_action_digest = _sha256(
        _canonical_json_bytes(action_catalog) + b"\n" + raw[config_path]
    )
    if defaults["actionCatalogDigest"] != expected_action_digest:
        raise ValueError(
            "default actionCatalogDigest does not bind the action catalog and config schema"
        )

    if generation["catalogDigest"] != defaults["catalogDigest"]:
        raise ValueError("generation fixture catalogDigest differs from the default")
    if generation["actionCatalogDigest"] != defaults["actionCatalogDigest"]:
        raise ValueError("generation fixture actionCatalogDigest differs from the default")
    if generation["snapshotDigest"] != _sha256(_canonical_json_bytes(defaults)):
        raise ValueError("generation fixture snapshotDigest does not bind the default")
    if generation["entrypoint"] not in generation["files"]:
        raise ValueError("generation fixture entrypoint is not a files key")
    expected_files = {
        "hyprland.lua": {"sha256": _sha256(b""), "size": 0},
    }
    if generation["files"] != expected_files:
        raise ValueError("generation fixture file metadata changed")
    generation_payload = {
        key: value for key, value in generation.items() if key != "generation"
    }
    if generation["generation"] != _sha256(
        _canonical_json_bytes(generation_payload)
    ):
        raise ValueError("generation fixture digest does not bind its canonical payload")
    expected_range = {
        "major": 0,
        "minor": 56,
        "reviewedVersion": "0.56.1",
        "minimumPatch": 0,
        "maximumPatch": None,
    }
    if generation["compatibleHyprland"] != expected_range:
        raise ValueError("generation fixture compatibility range differs from catalog policy")
    if generation["targetHyprland"] != defaults["targetHyprland"]:
        raise ValueError("generation fixture target differs from the default snapshot")

    definitions = config_schema["$defs"]
    reference_prefix = "config.schema.json#/$defs/"
    for collection, field in (
        (action_catalog["dispatcherActions"], "argumentsSchemaRef"),
        (action_catalog["semanticActions"], "argumentsSchemaRef"),
        (action_catalog["gestureActions"], "actionSchemaRef"),
        (catalog["complexSurfaces"], "schemaRef"),
    ):
        for record in collection:
            reference = record[field]
            if not reference.startswith(reference_prefix):
                raise ValueError(f"noncanonical config schema reference: {reference}")
            definition = reference[len(reference_prefix):]
            if definition not in definitions:
                raise ValueError(f"missing config schema definition: {definition}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="repository root containing interfaces/, data/, and tests/",
    )
    arguments = parser.parse_args()
    root = arguments.root.resolve()

    try:
        from jsonschema import Draft202012Validator, FormatChecker
    except ImportError:
        print(
            "validate_contract.py: Python package 'jsonschema' is required",
            file=sys.stderr,
        )
        return 2

    raw: dict[Path, bytes] = {}
    documents: dict[Path, Any] = {}
    try:
        try:
            json.loads(b'{"value":NaN}', parse_constant=_invalid_json_constant)
        except ValueError:
            pass
        else:
            raise ValueError("strict JSON self-test accepted NaN")

        for name in SCHEMA_FILES:
            relative = SCHEMA_DIRECTORY / name
            raw[relative], documents[relative] = _strict_json(root / relative)
            Draft202012Validator.check_schema(documents[relative])
            _validate_local_references(documents[relative], relative.as_posix())

        for instance_path, schema_name in INSTANCE_SCHEMA_PAIRS:
            raw[instance_path], documents[instance_path] = _strict_json(
                root / instance_path
            )
            schema = documents[SCHEMA_DIRECTORY / schema_name]
            validator = Draft202012Validator(
                schema,
                format_checker=FormatChecker(),
            )
            errors = sorted(
                validator.iter_errors(documents[instance_path]),
                key=lambda error: tuple(str(part) for part in error.absolute_path),
            )
            if errors:
                details = "; ".join(
                    f"/{'/'.join(str(part) for part in error.absolute_path)}: "
                    f"{error.message}"
                    for error in errors
                )
                raise ValueError(f"{instance_path}: schema validation failed: {details}")

        _validate_coherence(raw, documents)
    except (OSError, ValueError) as error:
        print(f"validate_contract.py: {error}", file=sys.stderr)
        return 1

    print("Hyprland v1 contract schemas, instances, and digests are valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
