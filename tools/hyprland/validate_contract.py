#!/usr/bin/env python3
"""Validate active Hyprland v1 and the parallel dormant v2 envelope."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
from typing import Any


V1_SCHEMA_DIRECTORY = Path("interfaces/hyprland/v1")
V2_SCHEMA_DIRECTORY = Path("interfaces/hyprland/v2")
SCHEMA_PATHS = (
    V1_SCHEMA_DIRECTORY / "catalog.schema.json",
    V1_SCHEMA_DIRECTORY / "config.schema.json",
    V1_SCHEMA_DIRECTORY / "action-catalog.schema.json",
    V1_SCHEMA_DIRECTORY / "generation-manifest.schema.json",
    V1_SCHEMA_DIRECTORY / "source-manifest.schema.json",
    V2_SCHEMA_DIRECTORY / "config.schema.json",
    V2_SCHEMA_DIRECTORY / "config-template.schema.json",
    V2_SCHEMA_DIRECTORY / "action-catalog.schema.json",
)
SCHEMA_PATHS += (
    V2_SCHEMA_DIRECTORY / "catalog.schema.json",
    V2_SCHEMA_DIRECTORY / "generation-manifest.schema.json",
    V2_SCHEMA_DIRECTORY / "source-manifest.schema.json",
)
INSTANCE_SCHEMA_PAIRS = (
    (Path("data/hyprland/config-catalog-v1.json"), V1_SCHEMA_DIRECTORY / "catalog.schema.json"),
    (Path("data/defaults/hyprland.json"), V1_SCHEMA_DIRECTORY / "config.schema.json"),
    (Path("data/hyprland/action-catalog-v1.json"), V1_SCHEMA_DIRECTORY / "action-catalog.schema.json"),
    (Path("tests/fixtures/hyprland/source-manifest.json"), V1_SCHEMA_DIRECTORY / "source-manifest.schema.json"),
    (Path("tests/fixtures/hyprland/generation-manifest.json"), V1_SCHEMA_DIRECTORY / "generation-manifest.schema.json"),
    (Path("data/defaults/hyprland-template.json"), V2_SCHEMA_DIRECTORY / "config-template.schema.json"),
    (Path("data/hyprland/action-catalog-v2.json"), V2_SCHEMA_DIRECTORY / "action-catalog.schema.json"),
)
INSTANCE_SCHEMA_PAIRS += (
    (Path("data/hyprland/config-catalog-v2.json"), V2_SCHEMA_DIRECTORY / "catalog.schema.json"),
    (Path("data/hyprland/source-manifest-v2.json"), V2_SCHEMA_DIRECTORY / "source-manifest.schema.json"),
    (Path("tests/fixtures/hyprland/generation-manifest-v2.json"), V2_SCHEMA_DIRECTORY / "generation-manifest.schema.json"),
)

V1_IMMUTABLE_SHA256 = {
    V1_SCHEMA_DIRECTORY / "README.md": "ba508ba626f34bb386b4c627c6cb508023375a7fcbd79e4c105c47054640d913",
    V1_SCHEMA_DIRECTORY / "action-catalog.schema.json": "0ea21439dc0122bf301207254a2e3319a09072a4101349a646eeb296027fa381",
    V1_SCHEMA_DIRECTORY / "catalog.schema.json": "7fc5a3d7530030caad442bb4877674d5b2110c07273627ffc88da1b3a8a6ff2a",
    V1_SCHEMA_DIRECTORY / "config.schema.json": "75e299cc9f5d3a3289450df089cad4aa22efd26ba2f11fbbf27d46f78b898202",
    V1_SCHEMA_DIRECTORY / "generation-manifest.schema.json": "578b0c5a8d7d936265d74545a7fc606e29e4fdd169c227000e60d2f8c9664fc2",
    V1_SCHEMA_DIRECTORY / "source-manifest.schema.json": "ad6a519cd441e3890adb7f8fa0ecd4ac0838c739a2593e995ffb6ffee3d13b66",
    Path("data/hyprland/config-catalog-v1.json"): "b6630b5cad74535c87415f2fe3716529c7fa7e80fa9e2b52d0fe8af48d4f9042",
    Path("data/hyprland/action-catalog-v1.json"): "b4e0fe0daa693477340287b933ca8ea752816221e5f80cec51c249d892904f5e",
    Path("data/defaults/hyprland.json"): "fb760dcef60232ccce78545b77f2b6eea8dd34429cda707cfa82ee8db39050de",
    Path("tests/fixtures/hyprland/source-manifest.json"): "eeba4cfdb69f507aa6a4c9a3ace60e17190dbf2e6545090d33cfd3998e857be4",
    Path("tests/fixtures/hyprland/generation-manifest.json"): "0c588152700bcc260b58cd4c1f75ac5ac79efb7d51726340a7f1321ddcf65401",
}

V2_CONFIG_SCHEMA_PATH = V2_SCHEMA_DIRECTORY / "config.schema.json"
V2_CATALOG_PATH = Path("data/hyprland/config-catalog-v2.json")
V2_ACTION_CATALOG_PATH = Path("data/hyprland/action-catalog-v2.json")
V2_SOURCE_MANIFEST_PATH = Path("data/hyprland/source-manifest-v2.json")
V2_TEMPLATE_PATH = Path("data/defaults/hyprland-template.json")
V2_GENERATION_MANIFEST_PATH = Path(
    "tests/fixtures/hyprland/generation-manifest-v2.json"
)

V2_HYPRLAND_REPOSITORY = "https://github.com/hyprwm/Hyprland"
V2_HYPRLAND_VERSION = "0.56.2"
V2_HYPRLAND_TAG = "v0.56.2"
V2_HYPRLAND_COMMIT = "efb50993780079460b0cbed1363e2166a2de1d9f"
V2_HYPRLAND_TREE = "a0a517a96596fd50c829f0c65eaaa30b52b62fd1"
V2_PREDECESSOR_VERSION = "0.56.1"
V2_PREDECESSOR_TAG = "v0.56.1"
V2_PREDECESSOR_COMMIT = "5c9377c15f85c50648f35ca5a213754f95b93ca0"
V2_PREDECESSOR_TREE = "3dd768e2339d34931da46bc001773356808883f8"
V2_HYPRUTILS_REPOSITORY = "https://github.com/hyprwm/hyprutils"
V2_HYPRUTILS_COMMIT = "5a7b8cf221914ce4714407950e4ffbdddcd8b66f"
V2_HYPRUTILS_TREE = "24edbdcc602db7da9af36aeb7b5de8b357ac7ec3"
V2_SOURCE_MANIFEST_DIGEST = (
    "f67f9214c47770268e66dd43d94b3af68dcbcac701312edcd52eedffb157f60d"
)
V2_CATALOG_DIGEST = (
    "3158d318945aeb03728426412933d737b5cf9cbd1dc384e296c11a9628ff6b88"
)
V2_ACTION_CATALOG_DIGEST = (
    "3625e37617810539823ae829de80eb5488b06b0e71d4c0be1f0356e00d019db8"
)
V2_SNAPSHOT_DIGEST = (
    "d0ba9d6640ee281789416bbed615ec64dee418175a09dab58393feb542db20cd"
)
V2_GENERATION_DIGEST = (
    "d5a4ab2f71d396d2a16b0dfa2ad3c647b583bf4c5f0c6f9f24a0e1b3934957f6"
)
V2_PATCH_SHA256 = "ceae6095bd0dd5352ffa35819348e494d3bb17ec133ab0fd3906cbdd1f0f3242"
V2_PATCH_PREIMAGE_SHA256 = (
    "157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9"
)
V2_PATCH_POSTIMAGE_SHA256 = (
    "d9607574beee09177aee9acc72f9fe15d96fe9f22bf8bd700b10938203fb11a4"
)
V2_PATCH_FILE_NAME = "hyprland-0.56.2-protected-f1-float-gaps.patch"
V2_PATCH_TARGET_PATH = "src/config/lua/bindings/LuaBindingsConfigRules.cpp"
V2_PATCH_HUNK_SHA256 = (
    "1b98a3173032283ff7cf5c526e12e5c2bdfc62df6712624aef57de548dadaf14"
)

V2_DEPENDENCY_PATHS = (
    {
        "path": "include/hyprutils/math/Vector2D.hpp",
        "sha256": "967bd9d7e12efb8f68694760f6d28b8b3c4810055b966b8c61e2ecebb6de2ddb",
    },
    {
        "path": "src/math/Box.cpp",
        "sha256": "2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c",
    },
    {
        "path": "include/hyprutils/animation/AnimationManager.hpp",
        "sha256": "6b07e7f1b25d19935081bc329a4face09b69a38cf97b9d9cd9bef9575783f847",
    },
    {
        "path": "src/animation/AnimationManager.cpp",
        "sha256": "cd46df7bd7f8bfb193ace37b32370c99056e9c633cb88793045c32d6d4cdb097",
    },
    {
        "path": "src/animation/AnimatedVariable.cpp",
        "sha256": "e9aa6712d9987e9415a0644a7e9521fd3813992f32043650ffdbb7f008d7c16a",
    },
)

V2_SOURCE_DOMAIN_ORDER = (
    "advanced-runtime",
    "animation",
    "appearance",
    "bindings",
    "complex-config",
    "dependency",
    "gestures",
    "group",
    "input",
    "input-device",
    "maximize",
    "monitor",
    "observation",
    "release",
    "renderer",
    "scalar-options",
    "startup",
    "test",
    "tooling",
    "window",
    "workspace",
)

V2_SOURCE_ROOT_KEYS = (
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
)
V2_CHANGED_PATHS = (
    "VERSION",
    "flake.lock",
    "hyprctl/src/Strings.hpp",
    "hyprtester/plugin/src/main.cpp",
    "hyprtester/src/tests/main/dwindle.cpp",
    "hyprtester/src/tests/main/keybinds.cpp",
    "hyprtester/src/tests/main/monitor_rules.cpp",
    "hyprtester/src/tests/main/scroll.cpp",
    "hyprtester/src/tests/main/workspaces.cpp",
    "meta/generateLuaStubs.py",
    "src/config/ConfigValue.cpp",
    "src/config/lua/bindings/LuaBindingsDispatchers.cpp",
    "src/config/shared/actions/ConfigActions.cpp",
    "src/config/shared/monitor/MonitorRuleManager.cpp",
    "src/config/shared/workspace/WorkspaceRule.hpp",
    "src/debug/HyprCtl.cpp",
    "src/desktop/state/ViewHitTester.cpp",
    "src/desktop/state/ViewHitTester.hpp",
    "src/desktop/state/WindowQuery.cpp",
    "src/desktop/view/Window.cpp",
    "src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp",
    "src/layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.cpp",
    "src/layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.hpp",
    "src/layout/space/Space.cpp",
    "src/managers/fullscreen/FullscreenController.cpp",
    "src/managers/input/InputManager.cpp",
    "src/managers/input/InputMethodPopup.cpp",
    "src/managers/input/InputMethodPopup.hpp",
    "src/managers/input/InputMethodRelay.cpp",
    "src/managers/screenshare/ScreenshareFrame.cpp",
    "src/pointer/PointerManager.cpp",
    "src/pointer/PointerManager.hpp",
    "src/render/OpenGL.cpp",
    "src/render/Renderer.cpp",
    "src/render/Renderer.hpp",
)

V2_FORBIDDEN_STALE_SHA256 = frozenset(
    {
        "07873f93e175c13551372f22aade7779b140b6ab3d900636c3d8446a94a87dcb",
        "fe067bc307c58b591f53579c6ff3d6a2490f9207e50eb1f18cddcc347558d1b5",
        "5a07ee1aa6a27424e6011bc27ea9adf0ec059249dc6f9efbe9ac84876c6431bf",
        "9fd19f5a236d938615618bae4e99f4786f460a6146e1f4e2eae87d0e23eb08b0",
        "72edec68c743e90644b11dd686a4d4520a5b16bcbefe63961cf0a0050aee8d8a",
        "52b80bf178b1c3d3d6157303366234d6ecf1f410f18cbfe0cef7830341031335",
        "75e299cc9f5d3a3289450df089cad4aa22efd26ba2f11fbbf27d46f78b898202",
        "402c8a8c570dd3760d4d7bea8c358c7f12021a7c51457e62a4771d69a581254b",
        "1438f04a169b4ecfc945078403d6286154bc89a0e32cb3a1a5073d209e0c358b",
        "44b6a09c3d314af3250e76615c5b33742d7830f0231671ca9abbdb8b0e6a33d2",
        "1b1a29e72e33f1d4aca6651c398987123cac3263bbc7a8169529ff45610bc712",
        "49463670579daac11a301bfd6cf78e17027d5ba1821db9a62abab9430461ad54",
    }
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


def _require_exact(actual: Any, expected: Any, label: str) -> None:
    if actual != expected:
        raise ValueError(f"{label} differs from the exact v2 authority")


def _ordered_unique_field(
    records: list[dict[str, Any]],
    field: str,
    expected_count: int,
    label: str,
) -> tuple[Any, ...]:
    if len(records) != expected_count:
        raise ValueError(
            f"{label} count is {len(records)}, expected {expected_count}"
        )
    values = tuple(record[field] for record in records)
    if values != tuple(sorted(values)):
        raise ValueError(f"{label} is not canonically sorted by {field}")
    if len(set(values)) != len(values):
        raise ValueError(f"{label} contains a duplicate {field}")
    return values


def _all_strings(value: Any) -> list[str]:
    if isinstance(value, str):
        return [value]
    if isinstance(value, list):
        return [item for child in value for item in _all_strings(child)]
    if isinstance(value, dict):
        return [item for child in value.values() for item in _all_strings(child)]
    return []


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
    config_path = V1_SCHEMA_DIRECTORY / "config.schema.json"
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


def _validate_v1_immutable(root: Path) -> None:
    for relative, expected in V1_IMMUTABLE_SHA256.items():
        actual = _sha256((root / relative).read_bytes())
        if actual != expected:
            raise ValueError(
                f"active v1 artifact bytes changed: {relative} ({actual} != {expected})"
            )


def _validate_v2_source_manifest(source_manifest: dict[str, Any]) -> None:
    if tuple(source_manifest) != V2_SOURCE_ROOT_KEYS:
        raise ValueError("v2 source manifest root keys or canonical order changed")
    _require_exact(source_manifest["formatVersion"], 2, "v2 source formatVersion")
    _require_exact(
        source_manifest["repository"],
        V2_HYPRLAND_REPOSITORY,
        "v2 source repository",
    )
    _require_exact(
        source_manifest["predecessor"],
        {
            "version": V2_PREDECESSOR_VERSION,
            "tag": V2_PREDECESSOR_TAG,
            "commit": V2_PREDECESSOR_COMMIT,
            "tree": V2_PREDECESSOR_TREE,
        },
        "v2 source predecessor",
    )
    _require_exact(
        source_manifest["upstream"],
        {
            "version": V2_HYPRLAND_VERSION,
            "tag": V2_HYPRLAND_TAG,
            "commit": V2_HYPRLAND_COMMIT,
            "tree": V2_HYPRLAND_TREE,
            "versionPath": "VERSION",
            "versionSha256": (
                "83983962d9161a0ec80cc585a3d3a1deb1f7e6c5ed8a5ba806f7dc338ebf790f"
            ),
        },
        "v2 source upstream",
    )

    qualified_sources = source_manifest["qualifiedSources"]
    qualified_paths = _ordered_unique_field(
        qualified_sources,
        "path",
        174,
        "v2 qualifiedSources",
    )
    domain_rank = {
        domain: index for index, domain in enumerate(V2_SOURCE_DOMAIN_ORDER)
    }
    for source in qualified_sources:
        domains = source["domains"]
        if any(domain not in domain_rank for domain in domains):
            raise ValueError(f"v2 qualified source has an unknown domain: {source['path']}")
        if domains != sorted(domains, key=domain_rank.__getitem__):
            raise ValueError(
                f"v2 qualified source domains are not canonical: {source['path']}"
            )
    patched_sources = [
        source
        for source in qualified_sources
        if source["upstreamSha256"] != source["effectiveSha256"]
    ]
    if V2_PATCH_TARGET_PATH not in qualified_paths:
        raise ValueError("v2 protected patch target is not a qualified source")
    _require_exact(
        patched_sources,
        [
            {
                "path": V2_PATCH_TARGET_PATH,
                "upstreamSha256": V2_PATCH_PREIMAGE_SHA256,
                "effectiveSha256": V2_PATCH_POSTIMAGE_SHA256,
                "domains": next(
                    source["domains"]
                    for source in qualified_sources
                    if source["path"] == V2_PATCH_TARGET_PATH
                ),
            }
        ],
        "v2 effective-source patch boundary",
    )

    changed_paths = source_manifest["changedPaths"]
    actual_changed_paths = _ordered_unique_field(
        changed_paths,
        "path",
        35,
        "v2 changedPaths",
    )
    _require_exact(
        actual_changed_paths,
        V2_CHANGED_PATHS,
        "v2 0.56.1-to-0.56.2 changed path set",
    )

    dependency_sources = source_manifest["dependencySources"]
    if len(dependency_sources) != 1:
        raise ValueError("v2 dependencySources must contain exactly one Hyprutils record")
    _require_exact(
        dependency_sources[0],
        {
            "hyprlandVersion": V2_HYPRLAND_VERSION,
            "lockPath": "flake.lock",
            "lockSha256": (
                "ba52e6238661708fdf94d693d7290698206ada41f1d6ca93053f553fe2beef0f"
            ),
            "repository": V2_HYPRUTILS_REPOSITORY,
            "revision": V2_HYPRUTILS_COMMIT,
            "tree": V2_HYPRUTILS_TREE,
            "paths": list(V2_DEPENDENCY_PATHS),
        },
        "v2 Hyprutils dependency",
    )

    added_paths = [record for record in changed_paths if record["status"] == "A"]
    if len(added_paths) != 1 or added_paths[0]["path"] != (
        "hyprtester/src/tests/main/monitor_rules.cpp"
    ):
        raise ValueError("v2 changedPaths does not have the exact single added path")
    if added_paths[0]["preimageSha256"] is not None:
        raise ValueError("v2 added changedPath unexpectedly has a preimage")
    modified_paths = [record for record in changed_paths if record["status"] == "M"]
    if len(modified_paths) != 34:
        raise ValueError("v2 changedPaths must contain exactly 34 modified paths")
    if any(record["preimageSha256"] is None for record in modified_paths):
        raise ValueError("v2 modified changedPath is missing its preimage")

    patches = source_manifest["patches"]
    if len(patches) != 1:
        raise ValueError("v2 source manifest must contain exactly one protected patch")
    patch = patches[0]
    if tuple(patch) != (
        "id",
        "fileName",
        "targetPath",
        "patchSha256",
        "preimageSha256",
        "postimageSha256",
        "hunkSha256",
        "exactHunk",
    ):
        raise ValueError("v2 protected patch fields or canonical order changed")
    expected_patch_fields = {
        "id": "protected-f1-float-gaps",
        "fileName": V2_PATCH_FILE_NAME,
        "targetPath": V2_PATCH_TARGET_PATH,
        "patchSha256": V2_PATCH_SHA256,
        "preimageSha256": V2_PATCH_PREIMAGE_SHA256,
        "postimageSha256": V2_PATCH_POSTIMAGE_SHA256,
        "hunkSha256": V2_PATCH_HUNK_SHA256,
    }
    for field, expected in expected_patch_fields.items():
        _require_exact(patch[field], expected, f"v2 patch {field}")
    exact_hunk = patch["exactHunk"]
    if not isinstance(exact_hunk, str):
        raise ValueError("v2 patch exactHunk is not a string")
    _require_exact(
        patch["hunkSha256"],
        _sha256(exact_hunk.encode("utf-8")),
        "v2 patch exactHunk digest",
    )


def _validate_v2_stale_guards(
    raw: dict[Path, bytes],
    documents: dict[Path, Any],
) -> None:
    v2_paths = (
        V2_CONFIG_SCHEMA_PATH,
        V2_SCHEMA_DIRECTORY / "config-template.schema.json",
        V2_SCHEMA_DIRECTORY / "action-catalog.schema.json",
        V2_SCHEMA_DIRECTORY / "catalog.schema.json",
        V2_SCHEMA_DIRECTORY / "generation-manifest.schema.json",
        V2_SCHEMA_DIRECTORY / "source-manifest.schema.json",
        V2_CATALOG_PATH,
        V2_ACTION_CATALOG_PATH,
        V2_SOURCE_MANIFEST_PATH,
        V2_TEMPLATE_PATH,
        V2_GENERATION_MANIFEST_PATH,
    )
    for path in v2_paths:
        digest = _sha256(raw[path])
        if digest in V2_FORBIDDEN_STALE_SHA256:
            raise ValueError(f"forbidden stale 0.56.1 v2 bytes remain at {path}")

    instance_paths = (
        V2_CATALOG_PATH,
        V2_ACTION_CATALOG_PATH,
        V2_SOURCE_MANIFEST_PATH,
        V2_TEMPLATE_PATH,
        V2_GENERATION_MANIFEST_PATH,
    )
    for path in instance_paths:
        stale_digests = set(_all_strings(documents[path])) & V2_FORBIDDEN_STALE_SHA256
        if stale_digests:
            raise ValueError(f"{path}: contains a forbidden stale 0.56.1 digest")

    stale_identity = {
        V2_PREDECESSOR_VERSION,
        V2_PREDECESSOR_TAG,
        V2_PREDECESSOR_COMMIT,
    }
    current_documents = (
        documents[V2_CATALOG_PATH],
        documents[V2_ACTION_CATALOG_PATH],
        documents[V2_TEMPLATE_PATH],
        documents[V2_GENERATION_MANIFEST_PATH],
    )
    for document in current_documents:
        values = set(_all_strings(document))
        if values & stale_identity:
            raise ValueError("a current v2 authority contains stale 0.56.1 identity")
        if "0.56.x" in values:
            raise ValueError("a current v2 authority contains forbidden 0.56.x")

    source_without_predecessor = {
        key: value
        for key, value in documents[V2_SOURCE_MANIFEST_PATH].items()
        if key != "predecessor"
    }
    if set(_all_strings(source_without_predecessor)) & stale_identity:
        raise ValueError("v2 source authority contains stale identity outside predecessor")
    if "0.56.x" in set(_all_strings(source_without_predecessor)):
        raise ValueError("v2 source authority contains forbidden 0.56.x")


def _validate_v2_coherence(
    raw: dict[Path, bytes],
    documents: dict[Path, Any],
    validator_type: type,
) -> None:
    v1_defaults = documents[Path("data/defaults/hyprland.json")]
    config_schema = documents[V2_CONFIG_SCHEMA_PATH]
    catalog = documents[V2_CATALOG_PATH]
    action_catalog = documents[V2_ACTION_CATALOG_PATH]
    source_manifest = documents[V2_SOURCE_MANIFEST_PATH]
    template = documents[V2_TEMPLATE_PATH]
    generation = documents[V2_GENERATION_MANIFEST_PATH]

    _validate_v2_source_manifest(source_manifest)
    _validate_v2_stale_guards(raw, documents)

    expected_source_digest = _sha256(_canonical_json_bytes(source_manifest))
    _require_exact(
        expected_source_digest,
        V2_SOURCE_MANIFEST_DIGEST,
        "v2 canonical source manifest digest",
    )
    _require_exact(
        catalog["sourceManifestDigest"],
        expected_source_digest,
        "v2 catalog sourceManifestDigest",
    )
    _require_exact(
        action_catalog["sourceManifestDigest"],
        expected_source_digest,
        "v2 action catalog sourceManifestDigest",
    )
    _require_exact(
        generation["sourceManifestDigest"],
        expected_source_digest,
        "v2 generation sourceManifestDigest",
    )

    expected_catalog_authority = {
        "major": 0,
        "minor": 56,
        "reviewedVersion": V2_HYPRLAND_VERSION,
        "reviewedTag": V2_HYPRLAND_TAG,
        "reviewedCommit": V2_HYPRLAND_COMMIT,
        "repository": V2_HYPRLAND_REPOSITORY,
        "minimumPatch": 2,
        "maximumPatch": 2,
    }
    _require_exact(catalog["contractVersion"], 2, "v2 catalog contractVersion")
    _require_exact(catalog["hyprland"], expected_catalog_authority, "v2 catalog Hyprland")
    _require_exact(
        catalog["compatibility"],
        {
            "minimumSupported": V2_HYPRLAND_VERSION,
            "fullyQualified": [V2_HYPRLAND_VERSION],
            "olderMinor": "migration",
            "newerMinor": "read-only",
            "unknownMajor": "unsupported",
        },
        "v2 catalog compatibility",
    )
    _ordered_unique_field(catalog["options"], "id", 353, "v2 catalog options")
    _ordered_unique_field(
        catalog["complexSurfaces"],
        "id",
        12,
        "v2 catalog complexSurfaces",
    )

    expected_action_authority = {
        "reviewedVersion": V2_HYPRLAND_VERSION,
        "reviewedTag": V2_HYPRLAND_TAG,
        "reviewedCommit": V2_HYPRLAND_COMMIT,
        "minimumPatch": 2,
        "maximumPatch": 2,
    }
    _require_exact(
        action_catalog["contractVersion"],
        2,
        "v2 action catalog contractVersion",
    )
    _require_exact(
        action_catalog["hyprland"],
        expected_action_authority,
        "v2 action catalog Hyprland",
    )
    _require_exact(
        action_catalog["source"],
        {
            "repository": V2_HYPRLAND_REPOSITORY,
            "tag": V2_HYPRLAND_TAG,
            "commit": V2_HYPRLAND_COMMIT,
            "path": "src/config/lua/bindings/LuaBindingsDispatchers.cpp",
            "sha256": "a109eeb982856e0fe2ac9d88c29115a09984511787e19a20e7b4804e14a9d4de",
        },
        "v2 action catalog dispatcher source",
    )
    for collection_name, count in (
        ("dispatcherActions", 47),
        ("semanticActions", 29),
        ("gestureActions", 10),
    ):
        _ordered_unique_field(
            action_catalog[collection_name],
            "id",
            count,
            f"v2 action catalog {collection_name}",
        )
    excluded = action_catalog["excluded"]
    if len(excluded) != 9 or len({record["id"] for record in excluded}) != 9:
        raise ValueError("v2 action catalog excluded count or identity changed")
    excluded_order = tuple((record["surface"], record["id"]) for record in excluded)
    if excluded_order != tuple(sorted(excluded_order)):
        raise ValueError("v2 action catalog excluded is not canonically ordered")

    expected_config_digest = _sha256(raw[V2_CONFIG_SCHEMA_PATH])
    _require_exact(
        action_catalog["configSchemaDigest"],
        expected_config_digest,
        "v2 action catalog configSchemaDigest",
    )
    expected_action_digest = _sha256(
        _canonical_json_bytes(action_catalog) + b"\n" + raw[V2_CONFIG_SCHEMA_PATH]
    )
    expected_catalog_digest = _sha256(_canonical_json_bytes(catalog))
    _require_exact(
        expected_catalog_digest,
        V2_CATALOG_DIGEST,
        "v2 canonical catalog digest",
    )
    _require_exact(
        expected_action_digest,
        V2_ACTION_CATALOG_DIGEST,
        "v2 canonical action catalog digest",
    )

    if "authorityId" in template:
        raise ValueError("the packaged v2 template must not contain authorityId")
    expected_template = dict(v1_defaults)
    expected_template.update(
        {
            "formatVersion": 2,
            "targetHyprland": V2_HYPRLAND_VERSION,
            "catalogDigest": expected_catalog_digest,
            "actionCatalogDigest": expected_action_digest,
        }
    )
    _require_exact(
        template,
        expected_template,
        "v2 template recovered semantics and authority bindings",
    )

    workspace_rules = config_schema["properties"]["workspaceRules"]
    if workspace_rules.get("maxItems") != 1025:
        raise ValueError("v2 lost the 1024-user-plus-one workspace-rule bound")

    runtime = dict(template)
    runtime["authorityId"] = generation["authorityId"]
    runtime_validator = validator_type(config_schema)
    runtime_errors = list(runtime_validator.iter_errors(runtime))
    if runtime_errors:
        raise ValueError("generation authorityId did not produce valid dormant v2 state")

    for invalid in (
        "0" * 32,
        "0123456789ABCDEF0123456789ABCDEF",
        "0123456789abcdef0123456789abcde",
        "g123456789abcdef0123456789abcdef",
    ):
        invalid_runtime = dict(template)
        invalid_runtime["authorityId"] = invalid
        if not list(runtime_validator.iter_errors(invalid_runtime)):
            raise ValueError(f"the dormant v2 schema accepted authorityId {invalid!r}")

    expected_range = {
        "major": 0,
        "minor": 56,
        "reviewedVersion": V2_HYPRLAND_VERSION,
        "minimumPatch": 2,
        "maximumPatch": 2,
    }
    for field, expected in (
        ("formatVersion", 2),
        ("contractVersion", 2),
        ("rendererVersion", 2),
        ("revision", template["revision"]),
        ("targetHyprland", V2_HYPRLAND_VERSION),
        ("compatibleHyprland", expected_range),
        ("catalogDigest", expected_catalog_digest),
        ("actionCatalogDigest", expected_action_digest),
        ("snapshotDigest", _sha256(_canonical_json_bytes(runtime))),
    ):
        _require_exact(generation[field], expected, f"v2 generation {field}")
    _require_exact(
        generation["snapshotDigest"],
        V2_SNAPSHOT_DIGEST,
        "v2 exact authority-bound snapshot digest",
    )
    if generation["entrypoint"] not in generation["files"]:
        raise ValueError("v2 generation entrypoint is not a files key")
    _require_exact(
        generation["files"],
        {"hyprland.lua": {"sha256": _sha256(b""), "size": 0}},
        "v2 generation file metadata",
    )
    generation_payload = {
        key: value for key, value in generation.items() if key != "generation"
    }
    _require_exact(
        generation["generation"],
        _sha256(_canonical_json_bytes(generation_payload)),
        "v2 generation canonical payload digest",
    )
    _require_exact(
        generation["generation"],
        V2_GENERATION_DIGEST,
        "v2 exact generation digest",
    )

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
                raise ValueError(f"noncanonical v2 config schema reference: {reference}")
            if reference[len(reference_prefix):] not in definitions:
                raise ValueError(f"missing v2 config schema definition: {reference}")


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

        _validate_v1_immutable(root)

        for relative in SCHEMA_PATHS:
            raw[relative], documents[relative] = _strict_json(root / relative)
            Draft202012Validator.check_schema(documents[relative])
            _validate_local_references(documents[relative], relative.as_posix())

        for instance_path, schema_path in INSTANCE_SCHEMA_PAIRS:
            raw[instance_path], documents[instance_path] = _strict_json(
                root / instance_path
            )
            schema = documents[schema_path]
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
        _validate_v2_coherence(raw, documents, Draft202012Validator)
    except (OSError, ValueError) as error:
        print(f"validate_contract.py: {error}", file=sys.stderr)
        return 1

    print(
        "Hyprland active v1 bytes and parallel dormant v2 envelope are valid"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
