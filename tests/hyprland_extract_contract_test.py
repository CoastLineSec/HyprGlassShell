#!/usr/bin/env python3
"""Focused fail-closed regressions for pinned runtime-source qualification."""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import runpy
import unittest

from jsonschema import Draft202012Validator


parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--extractor", type=Path, required=True)
arguments, remaining = parser.parse_known_args()
module = runpy.run_path(
    arguments.extractor.as_posix(), run_name="hyprshelld_extract_contract_test"
)


class BindingActivationContractTest(unittest.TestCase):
    def test_bindings_and_submaps_are_restart_quarantined(self) -> None:
        surfaces = {
            surface["id"]: surface for surface in module["_complex_surfaces"]()
        }
        self.assertEqual(
            {
                key: {
                    "applyMode": surfaces[key]["applyMode"],
                    "risk": surfaces[key]["risk"],
                    "description": surfaces[key]["description"],
                }
                for key in ("bindings", "submaps")
            },
            {
                "bindings": {
                    "applyMode": "restart",
                    "risk": "caution",
                    "description": (
                        "Compatibility-preserved normalized key chords mapped "
                        "to closed semantic actions; duplicate chords are "
                        "rejected, and generic changes require restart until a "
                        "dedicated receipt-bound shortcut transaction can prove "
                        "them."
                    ),
                },
                "submaps": {
                    "applyMode": "restart",
                    "risk": "caution",
                    "description": (
                        "Compatibility-preserved named binding submaps with an "
                        "explicit reset target; bindings reference the submap "
                        "name, and generic changes require restart until a "
                        "dedicated receipt-bound shortcut transaction can prove "
                        "them."
                    ),
                },
            },
        )


class BindingRuntimeContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_binding_runtime_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_binding_runtime_contract"](self.sources)

    def test_exact_paired_inventory_is_ordered(self) -> None:
        expected_paths = (
            Path("src/config/lua/ConfigManager.cpp"),
            Path("src/config/lua/bindings/LuaBindingsRegistration.cpp"),
            Path("src/config/lua/bindings/LuaBindingsToplevel.cpp"),
            Path("src/config/shared/actions/ConfigActions.hpp"),
            Path("src/config/shared/actions/ConfigActions.cpp"),
            Path("src/managers/KeybindManager.cpp"),
            Path("src/debug/HyprCtl.cpp"),
        )
        self.assertEqual(tuple(self.requirements), ("0.55.0", "0.56.1"))
        for version in self.requirements:
            with self.subTest(version=version):
                self.assertEqual(
                    module["BINDING_RUNTIME_SOURCE_PATHS"][version],
                    expected_paths,
                )
                self.assertEqual(tuple(self.requirements[version]), expected_paths)

    def test_exact_semantic_fragment_totals_are_frozen(self) -> None:
        self.assertEqual(
            {
                version: sum(len(fragments) for fragments in requirements.values())
                for version, requirements in self.requirements.items()
            },
            {"0.55.0": 92, "0.56.1": 92},
        )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            f"reviewed binding semantic {index} removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_binding_runtime_contract"](tampered)

    def test_resolve_binds_by_symbol_runtime_seams_fail_closed(self) -> None:
        path = Path("src/managers/KeybindManager.cpp")
        cases = (
            (
                "global translation state",
                "m_xkbTranslationState = xkb_state_new(PKEYMAP);",
                "m_xkbTranslationState = xkb_state_new(nullptr);",
            ),
            (
                "event-time symbol-state choice",
                "xkb_state_key_get_one_sym(pKeyboard->m_resolveBindsBySym ? "
                "pKeyboard->m_xkbSymState : m_xkbTranslationState, KEYCODE)",
                "xkb_state_key_get_one_sym(m_xkbTranslationState, KEYCODE)",
            ),
            (
                "raw-keycode matching independence",
                "if (key.keycode != k->keycode)",
                "if (key.keysym != k->keycode)",
            ),
        )
        for version in self.sources:
            for semantic, original, replacement in cases:
                with self.subTest(version=version, semantic=semantic):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    self.assertIn(original, source)
                    tampered[version][path] = source.replace(
                        original, replacement, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_binding_runtime_contract"](tampered)

    def test_symbol_resolution_duplicates_fail_closed(self) -> None:
        path = Path("src/managers/KeybindManager.cpp")
        additions = (
            (
                "symbol-state choice",
                "const auto duplicate = "
                "xkb_state_key_get_one_sym(pKeyboard->m_resolveBindsBySym ? "
                "pKeyboard->m_xkbSymState : m_xkbTranslationState, KEYCODE);",
            ),
            (
                "global binding translation state",
                "m_xkbTranslationState = xkb_state_new(PKEYMAP);",
            ),
            (
                "raw-keycode binding seam",
                "else if (k->keycode != 0) {\n"
                "if (key.keycode != k->keycode)",
            ),
        )
        for version in self.sources:
            for error, addition in additions:
                with self.subTest(version=version, error=error):
                    tampered = copy.deepcopy(self.sources)
                    tampered[version][path] += f"\n{addition}\n".encode("utf-8")
                    with self.assertRaisesRegex(ValueError, error):
                        module["_assert_binding_runtime_contract"](tampered)

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(next(iter(self.requirements["0.55.0"])))
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_binding_runtime_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_binding_runtime_contract"](missing_version)

    def test_clear_keybinds_must_clear_only_the_definition_vector(self) -> None:
        path = Path("src/managers/KeybindManager.cpp")
        reviewed = (
            "void CKeybindManager::clearKeybinds() { m_keybinds.clear(); }"
        )
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                expanded = (
                    "void CKeybindManager::clearKeybinds() { "
                    "m_keybinds.clear(); m_activeKeybinds.clear(); }\n"
                    + reviewed
                )
                tampered[version][path] = source.replace(
                    reviewed, expanded, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "clears only definitions"):
                    module["_assert_binding_runtime_contract"](tampered)

    def test_process_submap_state_changes_only_inside_set_submap(self) -> None:
        path = Path("src/config/shared/actions/ConfigActions.cpp")
        assignment = 'Config::Actions::state()->m_currentSubmap = "escaped";'
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                tampered[version][path] = (assignment + "\n" + source).encode(
                    "utf-8"
                )
                with self.assertRaisesRegex(ValueError, "escaped setSubmap"):
                    module["_assert_binding_runtime_contract"](tampered)

    def test_binds_inventory_cannot_claim_callback_semantics(self) -> None:
        path = Path("src/debug/HyprCtl.cpp")
        marker = '"dispatcher": "{}", "arg": "{}"'
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                tampered[version][path] = source.replace(
                    marker,
                    '"callback": "{}", ' + marker,
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "callback semantics"):
                    module["_assert_binding_runtime_contract"](tampered)

    def test_manifest_schema_keeps_binding_runtime_inventory_closed(self) -> None:
        root = arguments.extractor.resolve().parents[2]
        schema = json.loads(
            (root / "interfaces/hyprland/v1/source-manifest.schema.json").read_text(
                encoding="utf-8"
            )
        )
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("bindingRuntimeSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["bindingRuntimeSources"]["maxItems"] = 15
        with self.assertRaisesRegex(ValueError, "count/order is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        reordered = copy.deepcopy(schema)
        prefix = reordered["properties"]["bindingRuntimeSources"]["prefixItems"]
        prefix[0], prefix[1] = prefix[1], prefix[0]
        with self.assertRaisesRegex(ValueError, "count/order is stale"):
            module["_assert_source_manifest_schema"](reordered)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["bindingRuntimeSources"]["items"] = {
            "$ref": "#/$defs/bindingRuntimeSource"
        }
        with self.assertRaisesRegex(ValueError, "count/order is stale"):
            module["_assert_source_manifest_schema"](open_array)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["bindingRuntimeSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_pin = copy.deepcopy(schema)
        missing_pin["$defs"]["bindingRuntimeSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_pin)

    def test_manifest_records_reject_open_wrong_or_reordered_cases(self) -> None:
        root = arguments.extractor.resolve().parents[2]
        schema = json.loads(
            (root / "interfaces/hyprland/v1/source-manifest.schema.json").read_text(
                encoding="utf-8"
            )
        )
        manifest = json.loads(
            (root / "tests/fixtures/hyprland/source-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        validator = Draft202012Validator(schema)
        self.assertEqual(list(validator.iter_errors(manifest)), [])
        self.assertEqual(len(manifest["bindingRuntimeSources"]), 14)

        cases = []
        missing = copy.deepcopy(manifest)
        missing["bindingRuntimeSources"].pop()
        cases.append(missing)
        reordered = copy.deepcopy(manifest)
        records = reordered["bindingRuntimeSources"]
        records[0], records[1] = records[1], records[0]
        cases.append(reordered)
        open_record = copy.deepcopy(manifest)
        open_record["bindingRuntimeSources"][0]["callback"] = "opaque"
        cases.append(open_record)
        wrong_digest = copy.deepcopy(manifest)
        wrong_digest["bindingRuntimeSources"][0]["sha256"] = "0" * 64
        cases.append(wrong_digest)
        duplicate = copy.deepcopy(manifest)
        duplicate["bindingRuntimeSources"][1] = copy.deepcopy(
            duplicate["bindingRuntimeSources"][0]
        )
        cases.append(duplicate)

        for index, candidate in enumerate(cases):
            with self.subTest(index=index):
                self.assertNotEqual(list(validator.iter_errors(candidate)), [])


class MiscExclusionContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_misc_exclusion_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }
        self.occurrences = copy.deepcopy(
            module["MISC_EXCLUSION_EXPECTED_OCCURRENCES"]
        )

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_misc_exclusion_contract"](
            self.sources, self.occurrences
        )

    def test_exact_paired_inventory_and_preserved_options_are_ordered(self) -> None:
        expected_paths = (
            Path("src/config/values/ConfigValues.cpp"),
            Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
            Path("src/layout/target/WindowTarget.cpp"),
        )
        self.assertEqual(
            module["MISC_EXCLUSION_OPTION_PATHS"],
            (
                "misc:animate_manual_resizes",
                "misc:animate_mouse_windowdragging",
                "misc:layers_hog_keyboard_focus",
            ),
        )
        self.assertEqual(tuple(self.requirements), ("0.55.0", "0.56.1"))
        for version in self.requirements:
            with self.subTest(version=version):
                self.assertEqual(
                    module["MISC_EXCLUSION_SOURCE_PATHS"][version],
                    expected_paths,
                )
                self.assertEqual(tuple(self.requirements[version]), expected_paths)

    def test_exact_semantic_fragment_totals_are_frozen(self) -> None:
        self.assertEqual(
            {
                version: {
                    path.as_posix(): len(fragments)
                    for path, fragments in requirements.items()
                }
                for version, requirements in self.requirements.items()
            },
            {
                "0.55.0": {
                    "src/config/values/ConfigValues.cpp": 3,
                    "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp": 32,
                    "src/layout/target/WindowTarget.cpp": 1,
                },
                "0.56.1": {
                    "src/config/values/ConfigValues.cpp": 3,
                    "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp": 32,
                    "src/layout/target/WindowTarget.cpp": 1,
                },
            },
        )
        self.assertEqual(
            {
                version: sum(
                    len(fragments) for fragments in requirements.values()
                )
                for version, requirements in self.requirements.items()
            },
            {"0.55.0": 36, "0.56.1": 36},
        )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            f"reviewed misc exclusion semantic {index} removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_misc_exclusion_contract"](
                                tampered, self.occurrences
                            )

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(next(iter(self.requirements["0.55.0"])))
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_misc_exclusion_contract"](
                missing_source, self.occurrences
            )

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_misc_exclusion_contract"](
                missing_version, self.occurrences
            )

    def test_registry_only_and_manual_runtime_occurrences_are_global(self) -> None:
        registry = Path("src/config/values/ConfigValues.cpp")
        dwindle = Path(
            "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"
        )
        for version, occurrences in self.occurrences.items():
            with self.subTest(version=version):
                self.assertEqual(
                    occurrences,
                    {
                        "misc:animate_manual_resizes": {
                            registry: 1,
                            dwindle: 1,
                        },
                        "misc:animate_mouse_windowdragging": {registry: 1},
                        "misc:layers_hog_keyboard_focus": {registry: 1},
                    },
                )

    def test_global_occurrence_add_remove_and_duplicate_mutations_fail_closed(
        self,
    ) -> None:
        mutations = []
        added = copy.deepcopy(self.occurrences)
        added["0.55.0"]["misc:animate_mouse_windowdragging"][
            Path("src/desktop/Unexpected.cpp")
        ] = 1
        mutations.append(added)
        removed = copy.deepcopy(self.occurrences)
        removed["0.56.1"].pop("misc:layers_hog_keyboard_focus")
        mutations.append(removed)
        duplicated = copy.deepcopy(self.occurrences)
        duplicated["0.55.0"]["misc:animate_manual_resizes"][
            Path("src/config/values/ConfigValues.cpp")
        ] = 2
        mutations.append(duplicated)
        for index, occurrences in enumerate(mutations):
            with self.subTest(index=index):
                with self.assertRaisesRegex(ValueError, "global option occurrences"):
                    module["_assert_misc_exclusion_contract"](
                        self.sources, occurrences
                    )

    def test_force_consumption_and_animation_gated_snap_fail_closed(self) -> None:
        path = Path(
            "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"
        )
        for version in self.sources:
            with self.subTest(version=version, seam="leaf"):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                marker = "void CDwindleAlgorithm::newTarget"
                tampered[version][path] = source.replace(
                    marker,
                    "if (force) pTarget->setPositionGlobal(box);\n" + marker,
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "force reached"):
                    module["_assert_misc_exclusion_contract"](
                        tampered, self.occurrences
                    )

            with self.subTest(version=version, seam="snap"):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                marker = "SP<ITarget> CDwindleAlgorithm::getNextCandidate"
                tampered[version][path] = source.replace(
                    marker,
                    "if (*PANIMATE) {}\n" + marker,
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "snap behavior"):
                    module["_assert_misc_exclusion_contract"](
                        tampered, self.occurrences
                    )

    def test_manifest_schema_keeps_misc_exclusion_inventory_closed(self) -> None:
        root = arguments.extractor.resolve().parents[2]
        schema = json.loads(
            (root / "interfaces/hyprland/v1/source-manifest.schema.json").read_text(
                encoding="utf-8"
            )
        )
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("miscExclusionSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["miscExclusionSources"]["maxItems"] = 7
        with self.assertRaisesRegex(ValueError, "count/order is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        reordered = copy.deepcopy(schema)
        prefix = reordered["properties"]["miscExclusionSources"]["prefixItems"]
        prefix[0], prefix[1] = prefix[1], prefix[0]
        with self.assertRaisesRegex(ValueError, "count/order is stale"):
            module["_assert_source_manifest_schema"](reordered)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["miscExclusionSources"]["items"] = {
            "$ref": "#/$defs/miscExclusionSource"
        }
        with self.assertRaisesRegex(ValueError, "count/order is stale"):
            module["_assert_source_manifest_schema"](open_array)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["miscExclusionSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_pin = copy.deepcopy(schema)
        missing_pin["$defs"]["miscExclusionSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_pin)

    def test_manifest_records_reject_open_wrong_or_reordered_cases(self) -> None:
        root = arguments.extractor.resolve().parents[2]
        schema = json.loads(
            (root / "interfaces/hyprland/v1/source-manifest.schema.json").read_text(
                encoding="utf-8"
            )
        )
        manifest = json.loads(
            (root / "tests/fixtures/hyprland/source-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        validator = Draft202012Validator(schema)
        self.assertEqual(list(validator.iter_errors(manifest)), [])
        self.assertEqual(len(manifest["miscExclusionSources"]), 6)

        cases = []
        missing = copy.deepcopy(manifest)
        missing["miscExclusionSources"].pop()
        cases.append(missing)
        reordered = copy.deepcopy(manifest)
        records = reordered["miscExclusionSources"]
        records[0], records[1] = records[1], records[0]
        cases.append(reordered)
        open_record = copy.deepcopy(manifest)
        open_record["miscExclusionSources"][0]["reason"] = "opaque"
        cases.append(open_record)
        wrong_digest = copy.deepcopy(manifest)
        wrong_digest["miscExclusionSources"][0]["sha256"] = "0" * 64
        cases.append(wrong_digest)
        duplicate = copy.deepcopy(manifest)
        duplicate["miscExclusionSources"][1] = copy.deepcopy(
            duplicate["miscExclusionSources"][0]
        )
        cases.append(duplicate)

        for index, candidate in enumerate(cases):
            with self.subTest(index=index):
                self.assertNotEqual(list(validator.iter_errors(candidate)), [])


class MaximizeContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_maximize_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in self.requirements.items()
            }
            for version in ("0.56.0", "0.56.1")
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_maximize_contract"](self.sources)

    def test_every_pinned_source_has_a_fail_closed_semantic_assertion(self) -> None:
        for path, fragments in self.requirements.items():
            with self.subTest(path=path):
                tampered = copy.deepcopy(self.sources)
                source = tampered["0.56.1"][path].decode("utf-8")
                tampered["0.56.1"][path] = source.replace(
                    fragments[-1], "reviewed semantic removed", 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_maximize_contract"](tampered)

    def test_missing_source_or_patch_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.56.0"].pop(next(iter(self.requirements)))
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_maximize_contract"](missing_source)

        missing_patch = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_maximize_contract"](missing_patch)


class GroupBehaviorContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_group_behavior_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_group_behavior_contract"](self.sources)

    def test_exact_authored_option_inventory_is_covered(self) -> None:
        self.assertEqual(
            module["GROUP_BEHAVIOR_OPTION_PATHS"],
            (
                "group:auto_group",
                "group:insert_after_current",
                "group:focus_removed_window",
                "group:drag_into_group",
                "group:merge_groups_on_drag",
                "group:merge_groups_on_groupbar",
                "group:merge_floated_into_tiled_on_groupbar",
                "group:group_on_movetoworkspace",
            ),
        )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment, "reviewed semantic removed", 1
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_group_behavior_contract"](tampered)

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_group_behavior_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_group_behavior_contract"](missing_version)


class AppearanceBehaviorContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module[
            "_appearance_behavior_contract_requirements"
        ]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_appearance_behavior_contract"](self.sources)

    def test_exact_version_split_inventory_is_pinned(self) -> None:
        expected = {
            "0.55.0": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                Path("src/Compositor.cpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/render/Renderer.cpp"),
                Path("src/render/OpenGL.cpp"),
                Path("src/render/pass/Pass.cpp"),
                Path("src/render/ShaderLoader.hpp"),
                Path("src/render/Shader.cpp"),
                Path("src/render/GLRenderer.cpp"),
                Path("src/render/ElementRenderer.cpp"),
                Path("src/render/gl/GLElementRenderer.cpp"),
                Path("src/render/pass/PreBlurElement.hpp"),
                Path("src/render/pass/PreBlurElement.cpp"),
                Path("src/render/shaders/glsl/blurprepare.frag"),
                Path("src/render/shaders/glsl/blurprepare.glsl"),
                Path("src/render/shaders/glsl/blurfinish.frag"),
                Path("src/render/shaders/glsl/blurFinish.glsl"),
                Path("src/render/shaders/glsl/blur1.frag"),
                Path("src/render/shaders/glsl/blur1.glsl"),
                Path("src/render/shaders/glsl/gain.glsl"),
                Path("src/desktop/Workspace.cpp"),
                Path("src/render/decorations/CHyprBorderDecoration.cpp"),
                Path("src/render/decorations/DecorationPositioner.cpp"),
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                Path(
                    "src/desktop/rule/windowRule/WindowRuleApplicator.hpp"
                ),
                Path("src/desktop/view/Window.hpp"),
                Path("src/render/Renderer.hpp"),
                Path("src/render/OpenGL.hpp"),
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.hpp"
                ),
                Path("src/render/pass/BorderPassElement.hpp"),
                Path("src/render/pass/RectPassElement.hpp"),
                Path("src/render/pass/SurfacePassElement.hpp"),
                Path("src/render/pass/TexPassElement.hpp"),
                Path("src/render/shaders/glsl/border.frag"),
                Path("src/render/shaders/glsl/border.glsl"),
                Path("src/render/shaders/glsl/ext.frag"),
                Path("src/render/shaders/glsl/quad.frag"),
                Path("src/render/shaders/glsl/rounding.glsl"),
                Path("src/render/shaders/glsl/shadow.frag"),
                Path("src/render/shaders/glsl/shadow.glsl"),
                Path("src/render/shaders/glsl/surface.frag"),
                Path(
                    "src/render/decorations/CHyprInnerGlowDecoration.cpp"
                ),
                Path("src/render/shaders/glsl/inner_glow.frag"),
                Path("src/render/shaders/glsl/inner_glow.glsl"),
                Path("src/protocols/OutputManagement.cpp"),
                Path("src/config/shared/monitor/MonitorRuleManager.cpp"),
                Path("src/helpers/Monitor.cpp"),
            ),
            "0.56.1": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                Path("src/desktop/state/GlobalWindowController.cpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/render/Renderer.cpp"),
                Path("src/output/Monitor.cpp"),
                Path("src/desktop/state/LayerFadeout.cpp"),
                Path("src/desktop/state/WindowFadeout.cpp"),
                Path("src/render/OpenGL.cpp"),
                Path("src/render/pass/Pass.cpp"),
                Path("src/desktop/state/PopupFadeout.cpp"),
                Path("src/render/ShaderLoader.hpp"),
                Path("src/render/Shader.cpp"),
                Path("src/render/GLRenderer.cpp"),
                Path("src/render/ElementRenderer.cpp"),
                Path("src/render/gl/GLElementRenderer.cpp"),
                Path("src/render/pass/PreBlurElement.hpp"),
                Path("src/render/pass/PreBlurElement.cpp"),
                Path("src/render/shaders/glsl/blurprepare.frag"),
                Path("src/render/shaders/glsl/blurprepare.glsl"),
                Path("src/render/shaders/glsl/blurfinish.frag"),
                Path("src/render/shaders/glsl/blurFinish.glsl"),
                Path("src/render/shaders/glsl/blur1.frag"),
                Path("src/render/shaders/glsl/blur1.glsl"),
                Path("src/render/shaders/glsl/gain.glsl"),
                Path("src/desktop/Workspace.cpp"),
                Path("src/render/decorations/CHyprBorderDecoration.cpp"),
                Path("src/render/decorations/DecorationPositioner.cpp"),
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                Path(
                    "src/desktop/rule/windowRule/WindowRuleApplicator.hpp"
                ),
                Path("src/desktop/view/Window.hpp"),
                Path("src/render/Renderer.hpp"),
                Path("src/render/OpenGL.hpp"),
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.hpp"
                ),
                Path("src/render/pass/BorderPassElement.hpp"),
                Path("src/render/pass/RectPassElement.hpp"),
                Path("src/render/pass/SurfacePassElement.hpp"),
                Path("src/render/pass/TexPassElement.hpp"),
                Path("src/render/shaders/glsl/border.frag"),
                Path("src/render/shaders/glsl/border.glsl"),
                Path("src/render/shaders/glsl/ext.frag"),
                Path("src/render/shaders/glsl/quad.frag"),
                Path("src/render/shaders/glsl/rounding.glsl"),
                Path("src/render/shaders/glsl/shadow.frag"),
                Path("src/render/shaders/glsl/shadow.glsl"),
                Path("src/render/shaders/glsl/surface.frag"),
                Path(
                    "src/render/decorations/CHyprInnerGlowDecoration.cpp"
                ),
                Path("src/render/shaders/glsl/inner_glow.frag"),
                Path("src/render/shaders/glsl/inner_glow.glsl"),
                Path("src/protocols/OutputManagement.cpp"),
                Path("src/config/shared/monitor/MonitorRuleManager.cpp"),
            ),
        }
        self.assertEqual(module["APPEARANCE_BEHAVIOR_SOURCE_PATHS"], expected)
        self.assertEqual(
            {version: len(paths) for version, paths in expected.items()},
            {"0.55.0": 49, "0.56.1": 52},
        )
        self.assertEqual(sum(map(len, expected.values())), 101)
        self.assertEqual(
            {
                version: sum(map(len, requirements.values()))
                for version, requirements in self.requirements.items()
            },
            {"0.55.0": 754, "0.56.1": 829},
        )
        for version, paths in expected.items():
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), paths)

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            "reviewed appearance semantic removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_appearance_behavior_contract"](
                                tampered
                            )

    def test_registry_and_runtime_option_counts_fail_closed(self) -> None:
        registry_tampered = copy.deepcopy(self.sources)
        registry_tampered["0.55.0"][module["REGISTRY_PATH"]] += (
            '"decoration:active_opacity"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "registry count changed"):
            module["_assert_appearance_behavior_contract"](
                registry_tampered
            )

        shadow_registry_tampered = copy.deepcopy(self.sources)
        shadow_registry_tampered["0.55.0"][module["REGISTRY_PATH"]] += (
            'MS<Int>("decoration:shadow:range", "Shadow range (size) in layout px", 4, {.min = 0, .max = 100}),\n'.encode(
                "utf-8"
            )
        )
        with self.assertRaisesRegex(ValueError, "registry count changed"):
            module["_assert_appearance_behavior_contract"](
                shadow_registry_tampered
            )

        shadow_offset_registry_tampered = copy.deepcopy(self.sources)
        shadow_offset_registry_tampered["0.56.1"][module["REGISTRY_PATH"]] += (
            'MS<Vec2>("decoration:shadow:offset", "shadow\'s rendering offset.", Config::VEC2{},\n'.encode(
                "utf-8"
            )
        )
        with self.assertRaisesRegex(ValueError, "registry count changed"):
            module["_assert_appearance_behavior_contract"](
                shadow_offset_registry_tampered
            )

        shadow_scale_registry_tampered = copy.deepcopy(self.sources)
        shadow_scale_registry_tampered["0.55.0"][module["REGISTRY_PATH"]] += (
            'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1}),\n'.encode(
                "utf-8"
            )
        )
        with self.assertRaisesRegex(ValueError, "registry count changed"):
            module["_assert_appearance_behavior_contract"](
                shadow_scale_registry_tampered
            )

        glow_registry_tampered = copy.deepcopy(self.sources)
        glow_registry_tampered["0.55.0"][module["REGISTRY_PATH"]] += (
            'MS<Int>("decoration:glow:range", "glow range (size) in layout px", 10, {.min = 0, .max = 100}),\n'.encode(
                "utf-8"
            )
        )
        with self.assertRaisesRegex(ValueError, "registry count changed"):
            module["_assert_appearance_behavior_contract"](
                glow_registry_tampered
            )

        shadow_consumer_tampered = copy.deepcopy(self.sources)
        shadow_consumer_tampered["0.56.1"][
            Path("src/render/Renderer.cpp")
        ] += b'"decoration:shadow:sharp"\n'
        with self.assertRaisesRegex(
            ValueError, "shadow-rendering literal consumer count changed"
        ):
            module["_assert_appearance_behavior_contract"](
                shadow_consumer_tampered
            )

        shadow_offset_consumer_tampered = copy.deepcopy(self.sources)
        shadow_offset_consumer_tampered["0.55.0"][
            Path("src/render/Renderer.cpp")
        ] += b'"decoration:shadow:offset"\n'
        with self.assertRaisesRegex(
            ValueError, "shadow-rendering literal consumer count changed"
        ):
            module["_assert_appearance_behavior_contract"](
                shadow_offset_consumer_tampered
            )

        shadow_scale_consumer_tampered = copy.deepcopy(self.sources)
        shadow_scale_consumer_tampered["0.56.1"][Path("src/render/Renderer.cpp")] += (
            b'"decoration:shadow:scale"\n'
        )
        with self.assertRaisesRegex(
            ValueError, "shadow-rendering literal consumer count changed"
        ):
            module["_assert_appearance_behavior_contract"](
                shadow_scale_consumer_tampered
            )

        glow_consumer_tampered = copy.deepcopy(self.sources)
        glow_consumer_tampered["0.56.1"][Path("src/render/Renderer.cpp")] += (
            b'"decoration:glow:enabled"\n'
        )
        with self.assertRaisesRegex(
            ValueError, "glow literal consumer count changed"
        ):
            module["_assert_appearance_behavior_contract"](
                glow_consumer_tampered
            )

        window_tampered = copy.deepcopy(self.sources)
        window_tampered["0.56.1"][
            Path("src/desktop/view/Window.cpp")
        ] += '"decoration:dim_strength"\n'.encode("utf-8")
        with self.assertRaisesRegex(ValueError, "runtime gate count changed"):
            module["_assert_appearance_behavior_contract"](window_tampered)

        renderer_tampered = copy.deepcopy(self.sources)
        renderer_tampered["0.55.0"][Path("src/render/Renderer.cpp")] += (
            '"decoration:dim_around"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "renderer count changed"):
            module["_assert_appearance_behavior_contract"](
                renderer_tampered
            )

        monitor_tampered = copy.deepcopy(self.sources)
        monitor_tampered["0.56.1"][Path("src/output/Monitor.cpp")] += (
            '"decoration:dim_special"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "transition count changed"):
            module["_assert_appearance_behavior_contract"](monitor_tampered)

        opengl_tampered = copy.deepcopy(self.sources)
        opengl_tampered["0.55.0"][Path("src/render/OpenGL.cpp")] += (
            '"decoration:blur:ignore_opacity"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "blur runtime count changed"):
            module["_assert_appearance_behavior_contract"](
                opengl_tampered
            )

        brightness_tampered = copy.deepcopy(self.sources)
        brightness_tampered["0.56.1"][Path("src/render/OpenGL.cpp")] += (
            '"decoration:blur:brightness"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "blur runtime count changed"):
            module["_assert_appearance_behavior_contract"](
                brightness_tampered
            )

        extra_consumer = copy.deepcopy(self.sources)
        extra_consumer["0.55.0"][Path("src/render/Shader.cpp")] += (
            '"decoration:blur:contrast"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "literal consumer count changed"):
            module["_assert_appearance_behavior_contract"](extra_consumer)

        refresher_tampered = copy.deepcopy(self.sources)
        refresher_tampered["0.56.1"][
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp")
        ] += b"REFRESH_BLUR_FB\n"
        with self.assertRaisesRegex(ValueError, "blur refresh count changed"):
            module["_assert_appearance_behavior_contract"](
                refresher_tampered
            )

    def test_border_shadow_structural_counts_fail_closed(self) -> None:
        registry_tampered = copy.deepcopy(self.sources)
        registry_tampered["0.55.0"][module["REGISTRY_PATH"]] += (
            '"decoration:border_part_of_window"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "registry count changed"):
            module["_assert_appearance_behavior_contract"](
                registry_tampered
            )

        extra_consumer = copy.deepcopy(self.sources)
        extra_consumer["0.56.1"][Path("src/render/Renderer.cpp")] += (
            '"decoration:border_part_of_window"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(
            ValueError, "border-part literal consumer count changed"
        ):
            module["_assert_appearance_behavior_contract"](extra_consumer)

        structural_cases = (
            (
                Path("src/render/decorations/CHyprBorderDecoration.cpp"),
                "DECORATION_PART_OF_MAIN_WINDOW\n",
                "border-part flag count changed",
            ),
            (
                Path("src/render/decorations/DecorationPositioner.cpp"),
                "DECORATION_PART_OF_MAIN_WINDOW\n",
                "border-part flag count changed",
            ),
            (
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                "m_lastWindowBoxWithDecos\n",
                "included-decoration cache count changed",
            ),
            (
                Path("src/desktop/Workspace.cpp"),
                "updateWindowDecos\n",
                "workspace refresh count changed",
            ),
            (
                Path("src/desktop/view/Window.cpp"),
                "wd->updateWindow(m_self.lock());\n",
                "window-decoration chain count changed",
            ),
        )
        for version in self.sources:
            for path, addition, message in structural_cases:
                with self.subTest(version=version, path=path):
                    tampered = copy.deepcopy(self.sources)
                    tampered[version][path] += addition.encode("utf-8")
                    with self.assertRaisesRegex(ValueError, message):
                        module["_assert_appearance_behavior_contract"](
                            tampered
                        )

    def test_rounding_power_structural_counts_fail_closed(self) -> None:
        registry_tampered = copy.deepcopy(self.sources)
        registry_tampered["0.55.0"][module["REGISTRY_PATH"]] += (
            '"decoration:rounding_power"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "registry count changed"):
            module["_assert_appearance_behavior_contract"](
                registry_tampered
            )

        extra_consumer = copy.deepcopy(self.sources)
        extra_consumer["0.56.1"][Path("src/render/Shader.cpp")] += (
            '"decoration:rounding_power"\n'.encode("utf-8")
        )
        with self.assertRaisesRegex(
            ValueError, "rounding-power literal consumer count changed"
        ):
            module["_assert_appearance_behavior_contract"](extra_consumer)

        structural_cases = (
            (
                "0.55.0",
                Path("src/desktop/view/Window.cpp"),
                "const int ROUNDINGPOWER = roundingPower();\n",
                "rounding-power structural count changed",
            ),
            (
                "0.56.1",
                Path("src/desktop/view/Window.cpp"),
                "std::pow(\n",
                "rounding-power structural count changed",
            ),
            (
                "0.55.0",
                Path("src/render/Renderer.cpp"),
                "pWindow->roundingPower()\n",
                "rounding-power renderer count changed",
            ),
            (
                "0.56.1",
                Path("src/render/shaders/glsl/border.glsl"),
                "1.0 / roundingPower\n",
                "rounding-power shader count changed",
            ),
            (
                "0.56.1",
                Path("src/desktop/state/WindowFadeout.cpp"),
                "m_roundingPower = window->roundingPower();\n",
                "rounding-power fadeout cache count changed",
            ),
        )
        for version, path, addition, message in structural_cases:
            with self.subTest(version=version, path=path):
                tampered = copy.deepcopy(self.sources)
                tampered[version][path] += addition.encode("utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    module["_assert_appearance_behavior_contract"](
                        tampered
                    )

    def test_digest_authorized_rounding_power_mutations_fail_closed(self) -> None:
        cases = (
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                '"rounding power of corners (2 is a circle)", 2, {.min = 2, .max = 10}',
                '"rounding power of corners (2 is a circle)", 3, {.min = 2, .max = 10}',
            ),
            (
                "0.56.1",
                module["REGISTRY_PATH"],
                "Supplementary::REFRESH_WINDOW_STATES | Supplementary::REFRESH_BLUR_FB",
                "Supplementary::REFRESH_WINDOW_STATES",
            ),
            (
                "0.56.1",
                Path(
                    "src/desktop/rule/windowRule/WindowRuleApplicator.hpp"
                ),
                "DEFINE_PROP(Config::FLOAT, roundingPower",
                "DEFINE_PROP(Config::INT, roundingPower",
            ),
            (
                "0.55.0",
                Path("src/desktop/view/Window.cpp"),
                "const int ROUNDINGPOWER = roundingPower();",
                "const float ROUNDINGPOWER = roundingPower();",
            ),
            (
                "0.56.1",
                Path("src/desktop/view/Window.cpp"),
                "std::pow(x0 - x, ROUNDINGPOWER)",
                "std::pow(x0 - x, 2)",
            ),
            (
                "0.55.0",
                Path("src/desktop/view/Window.cpp"),
                "(roundingPower / 2.0)",
                "(roundingPower / 1.0)",
            ),
            (
                "0.56.1",
                Path("src/desktop/view/Window.cpp"),
                "std::clamp(*PROUNDINGPOWER, 1.F, 10.F)",
                "std::clamp(*PROUNDINGPOWER, 2.F, 10.F)",
            ),
            (
                "0.55.0",
                Path("src/render/Renderer.cpp"),
                "standalone || renderdata.dontRound ? 2.0f",
                "standalone || renderdata.dontRound ? 1.0f",
            ),
            (
                "0.56.1",
                Path("src/render/pass/BorderPassElement.hpp"),
                "float                      roundingPower = 2.F;",
                "float                      roundingPower = 1.F;",
            ),
            (
                "0.55.0",
                Path("src/render/shaders/glsl/rounding.glsl"),
                "1.0 / roundingPower",
                "1.0 / 2.0",
            ),
            (
                "0.56.1",
                Path("src/desktop/state/WindowFadeout.cpp"),
                ".roundingPower = m_roundingPower,",
                ".roundingPower = window->roundingPower(),",
            ),
            (
                "0.55.0",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                ".roundingPower = ROUNDINGPOWER,",
                ".roundingPower = 2.0F,",
            ),
        )
        for version, path, original, replacement in cases:
            with self.subTest(version=version, path=path, original=original):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                self.assertIn(original, source)
                tampered[version][path] = source.replace(
                    original, replacement, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_appearance_behavior_contract"](
                        tampered
                    )

    def test_digest_authorized_shadow_rendering_mutations_fail_closed(
        self,
    ) -> None:
        cases = (
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                'MS<Int>("decoration:shadow:range", "Shadow range (size) in layout px", 4, {.min = 0, .max = 100}),',
                'MS<Int>("decoration:shadow:range", "Shadow range (size) in layout px", 5, {.min = 0, .max = 100}),',
            ),
            (
                "0.56.1",
                module["REGISTRY_PATH"],
                '{.min = 1, .max = 4, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                '{.min = 0, .max = 4, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                'MS<Bool>("decoration:shadow:sharp", "whether the shadow should be sharp or not.", false),',
                'MS<Bool>("decoration:shadow:sharp", "whether the shadow should be sharp or not.", true),',
            ),
            (
                "0.55.0",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.hpp"
                ),
                "int   size          = 0;",
                "int   size          = 1;",
            ),
            (
                "0.56.1",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                'static auto PSHADOWSIZE   = CConfigValue<Config::INTEGER>("decoration:shadow:range");',
                'static auto PSHADOWSIZE   = CConfigValue<Config::INTEGER>("decoration:shadow:scale");',
            ),
            (
                "0.55.0",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                "fullBox.w += 2 * *PSHADOWSIZE;",
                "fullBox.w += *PSHADOWSIZE;",
            ),
            (
                "0.56.1",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                ".size          = *PSHADOWSIZE,",
                ".size          = 0,",
            ),
            (
                "0.55.0",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                "if (*PSHADOWSHARP)",
                "if (false)",
            ),
            (
                "0.55.0",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                "bool CHyprDropShadowDecoration::canRender(PHLMONITOR pMonitor) {\nstatic auto PSHADOWS = CConfigValue<Config::INTEGER>(\"decoration:shadow:enabled\");",
                "bool CHyprDropShadowDecoration::canRender(PHLMONITOR pMonitor) {\nstatic auto PSHADOWS = CConfigValue<Config::INTEGER>(\"decoration:shadow:sharp\");",
            ),
            (
                "0.56.1",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                ".box = box,",
                ".box = CBox{},",
            ),
            (
                "0.56.1",
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                "g_pHyprRenderer->drawShadow(box, round, roundingPower, range, grad1, grad2, lerp, a);",
                "g_pHyprRenderer->drawShadow(box, round, roundingPower, 0, grad1, grad2, lerp, a);",
            ),
            (
                "0.56.1",
                Path("src/render/GLRenderer.cpp"),
                "g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, grad1, grad2, lerp, a);",
                "g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, 0, grad1, grad2, lerp, a);",
            ),
            (
                "0.55.0",
                Path("src/render/OpenGL.cpp"),
                "std::clamp(sc<int>(*PSHADOWPOWER), 1, 4)",
                "std::clamp(sc<int>(*PSHADOWPOWER), 0, 4)",
            ),
            (
                "0.55.0",
                Path("src/render/OpenGL.cpp"),
                "const auto TOPLEFT = Vector2D(range + round, range + round);",
                "const auto TOPLEFT = Vector2D(round, round);",
            ),
            (
                "0.56.1",
                Path("src/render/OpenGL.cpp"),
                "const auto BOTTOMRIGHT = Vector2D(newBox.width - (range + round), newBox.height - (range + round));",
                "const auto BOTTOMRIGHT = Vector2D(newBox.width - round, newBox.height - round);",
            ),
            (
                "0.56.1",
                Path("src/render/OpenGL.cpp"),
                "shader->setUniformFloat(SHADER_RANGE, range);",
                "shader->setUniformFloat(SHADER_RANGE, 0);",
            ),
            (
                "0.55.0",
                Path("src/render/Shader.cpp"),
                'getUniform("shadowPower")',
                'getUniform("range")',
            ),
            (
                "0.56.1",
                Path("src/render/shaders/glsl/shadow.frag"),
                "uniform float shadowPower;",
                "uniform int shadowPower;",
            ),
            (
                "0.55.0",
                Path("src/render/shaders/glsl/shadow.glsl"),
                "if (distanceToCorner > radius) {",
                "if (distanceToCorner >= radius) {",
            ),
            (
                "0.56.1",
                Path("src/render/shaders/glsl/shadow.glsl"),
                "if (smallest < range) {",
                "if (smallest <= range) {",
            ),
            (
                "0.55.0",
                Path("src/render/gl/GLElementRenderer.cpp"),
                "m_data.deco->render(g_pHyprRenderer->m_renderData.pMonitor.lock(), m_data.a);",
                "m_data.deco->damageEntire();",
            ),
        )
        for version, path, original, replacement in cases:
            with self.subTest(version=version, path=path, original=original):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                self.assertIn(original, source)
                tampered[version][path] = source.replace(
                    original, replacement, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_appearance_behavior_contract"](
                        tampered
                    )

    def test_digest_authorized_shadow_offset_mutations_fail_closed(
        self,
    ) -> None:
        shadow_path = Path(
            "src/render/decorations/CHyprDropShadowDecoration.cpp"
        )
        cases = (
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                'MS<Vec2>("decoration:shadow:offset", "shadow\'s rendering offset.", Config::VEC2{}, {.validator = vec2Range(-250, -250, 250, 250)}),',
                'MS<Float>("decoration:shadow:offset", "shadow\'s rendering offset.", Config::VEC2{}, {.validator = vec2Range(-250, -250, 250, 250)}),',
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "vec2Range(-250, -250, 250, 250)",
                "vec2Range(-249, -250, 250, 250)",
            ),
            (
                "0.56.1",
                module["REGISTRY_PATH"],
                "{.validator = vec2Range(-250, -250, 250, 250), .refresh = Supplementary::REFRESH_WINDOW_STATES}),",
                "{.validator = vec2Range(-250, -250, 250, 250)}),",
            ),
            (
                "0.55.0",
                shadow_path,
                'static auto PSHADOWOFFSET = CConfigValue<Config::VEC2>("decoration:shadow:offset");',
                'static auto PSHADOWOFFSET = CConfigValue<Config::FLOAT>("decoration:shadow:offset");',
            ),
            (
                "0.56.1",
                shadow_path,
                'static auto PSHADOWSCALE  = CConfigValue<Config::FLOAT>("decoration:shadow:scale");',
                'static auto PSHADOWSCALE  = CConfigValue<Config::VEC2>("decoration:shadow:scale");',
            ),
            (
                "0.55.0",
                shadow_path,
                "fullBox.h += 2 * *PSHADOWSIZE;",
                "fullBox.h += *PSHADOWSIZE;",
            ),
            (
                "0.56.1",
                shadow_path,
                "const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);",
                "const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.1f, 1.f);",
            ),
            (
                "0.55.0",
                shadow_path,
                "fullBox.scaleFromCenter(SHADOWSCALE).translate({(*PSHADOWOFFSET).x, (*PSHADOWOFFSET).y});",
                "fullBox.scaleFromCenter(SHADOWSCALE).translate({(*PSHADOWOFFSET).y, (*PSHADOWOFFSET).x});",
            ),
            (
                "0.56.1",
                shadow_path,
                "m_lastWindowPos.x - fullBox.x - pMonitor->m_position.x + 2,",
                "m_lastWindowPos.x + fullBox.x - pMonitor->m_position.x + 2,",
            ),
            (
                "0.55.0",
                shadow_path,
                "fullBox.translate(PWINDOW->m_floatingOffset);",
                "fullBox.translate({});",
            ),
            (
                "0.56.1",
                shadow_path,
                "if (fullBox.width < 1 || fullBox.height < 1)",
                "if (fullBox.width <= 1 || fullBox.height <= 1)",
            ),
            (
                "0.55.0",
                shadow_path,
                "fullBox.scale(pMonitor->m_scale).round();",
                "fullBox.round();",
            ),
            (
                "0.56.1",
                shadow_path,
                "if (m_extents != m_reportedExtents)",
                "if (m_extents == m_reportedExtents)",
            ),
            (
                "0.55.0",
                shadow_path,
                "reposition();",
                "damageEntire();",
            ),
        )
        for version, path, original, replacement in cases:
            with self.subTest(version=version, path=path, original=original):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                self.assertIn(original, source)
                tampered[version][path] = source.replace(
                    original, replacement, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_appearance_behavior_contract"](
                        tampered
                    )

    def test_digest_authorized_shadow_scale_registry_mutations_fail_closed(
        self,
    ) -> None:
        cases = (
            (
                "0.55.0",
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1}),',
                'MS<Int>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1}),',
            ),
            (
                "0.55.0",
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 0.5, {.min = 0, .max = 1}),',
            ),
            (
                "0.55.0",
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0.1, .max = 1}),',
            ),
            (
                "0.56.1",
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1}),',
            ),
        )
        for version, original, replacement in cases:
            with self.subTest(version=version, replacement=replacement):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][module["REGISTRY_PATH"]].decode(
                    "utf-8"
                )
                self.assertIn(original, source)
                tampered[version][module["REGISTRY_PATH"]] = source.replace(
                    original, replacement, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "ConfigValues.cpp"):
                    module["_assert_appearance_behavior_contract"](
                        tampered
                    )

    def test_digest_authorized_glow_safety_provenance_mutations_fail_closed(
        self,
    ) -> None:
        glow_path = Path(
            "src/render/decorations/CHyprInnerGlowDecoration.cpp"
        )
        fragment_path = Path("src/render/shaders/glsl/inner_glow.frag")
        shader_path = Path("src/render/shaders/glsl/inner_glow.glsl")
        output_path = Path("src/protocols/OutputManagement.cpp")
        monitor_rule_path = Path(
            "src/config/shared/monitor/MonitorRuleManager.cpp"
        )
        cases = (
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                'MS<Bool>("decoration:glow:enabled", "enable inner glow on windows", false),',
                'MS<Bool>("decoration:glow:enabled", "enable inner glow on windows", true),',
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                'MS<Int>("decoration:glow:range", "glow range (size) in layout px", 10, {.min = 0, .max = 100}),',
                'MS<Int>("decoration:glow:range", "glow range (size) in layout px", 10, {.min = 10, .max = 100}),',
            ),
            (
                "0.56.1",
                module["REGISTRY_PATH"],
                'MS<Int>("decoration:glow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3,\n{.min = 1, .max = 4, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Int>("decoration:glow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3,\n{.min = 1, .max = 4}),',
            ),
            (
                "0.55.0",
                glow_path,
                "if (!*PGLOW)",
                "if (false)",
            ),
            (
                "0.56.1",
                glow_path,
                "if (!*PGLOW || !visible())",
                "if (!visible())",
            ),
            (
                "0.55.0",
                glow_path,
                "GLOWSIZE * pMonitor->m_scale",
                "GLOWSIZE",
            ),
            (
                "0.56.1",
                Path("src/render/OpenGL.cpp"),
                "std::clamp(sc<int>(*PGLOWPOWER), 1, 4)",
                "std::clamp(sc<int>(*PGLOWPOWER), 0, 4)",
            ),
            (
                "0.55.0",
                Path("src/render/OpenGL.cpp"),
                "void CHyprOpenGLImpl::renderInnerGlow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color, int glowPower, float a) {\nshader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);\nshader->setUniformFloat(SHADER_RANGE, range);",
                "void CHyprOpenGLImpl::renderInnerGlow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color, int glowPower, float a) {\nshader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);\nshader->setUniformFloat(SHADER_RANGE, 1);",
            ),
            (
                "0.56.1",
                fragment_path,
                '#include "inner_glow.glsl"',
                '#include "rounding.glsl"',
            ),
            (
                "0.55.0",
                shader_path,
                "return pow(1.0 - distFromEdge / range, glowPower);",
                "return pow(1.0 - distFromEdge / max(range, 1.0), glowPower);",
            ),
            (
                "0.56.1",
                shader_path,
                "float h = max(k - abs(a - b), 0.0) / k;",
                "float h = max(k - abs(a - b), 0.0) / max(k, 1.0);",
            ),
            (
                "0.55.0",
                output_path,
                "if (scale < 0.1 || scale > 10.0) {",
                "if (scale < 1.0 || scale > 10.0) {",
            ),
            (
                "0.56.1",
                output_path,
                "m_state.scale = scale;",
                "m_state.scale = 1.0;",
            ),
            (
                "0.55.0",
                monitor_rule_path,
                "rule.m_scale = CONFIG->scale;",
                "rule.m_scale = 1.0;",
            ),
            (
                "0.56.1",
                monitor_rule_path,
                "if (!m->applyMonitorRule(Config::CMonitorRule{rule})) {",
                "if (false) {",
            ),
            (
                "0.55.0",
                Path("src/helpers/Monitor.cpp"),
                "if (RULE->m_scale > 0.1)",
                "if (RULE->m_scale >= 1.0)",
            ),
            (
                "0.56.1",
                Path("src/output/Monitor.cpp"),
                "m_scale = RULE->m_scale;",
                "m_scale = getDefaultScale();",
            ),
        )
        for version, path, original, replacement in cases:
            with self.subTest(version=version, path=path, original=original):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                self.assertIn(original, source)
                tampered[version][path] = source.replace(
                    original, replacement, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_appearance_behavior_contract"](
                        tampered
                    )

    def test_digest_authorized_blur_semantic_mutations_fail_closed(self) -> None:
        # Calling the semantic gate directly models a source mutation whose
        # qualified digest was deliberately rotated: the pin alone must not
        # authorize a behavior change.
        cases = (
            (
                "0.56.1",
                module["REGISTRY_PATH"],
                'MS<Int>("decoration:blur:size", "blur size (distance)", 8, {.min = 0, .max = 100, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Int>("decoration:blur:size", "blur size (distance)", 8, {.min = 0, .max = 100, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
            ),
            (
                "0.56.1",
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                "m->m_forceFullFrames = 2;",
                "m->m_forceFullFrames = 1;",
            ),
            (
                "0.55.0",
                Path("src/render/OpenGL.cpp"),
                "std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8))",
                "std::clamp(*PBLURPASSES, sc<int64_t>(0), sc<int64_t>(10))",
            ),
            (
                "0.56.1",
                Path("src/render/OpenGL.cpp"),
                "*PBLURIGNOREOPACITY ? data.blurA : data.a * data.blurA",
                "*PBLURIGNOREOPACITY ? data.a * data.blurA : data.blurA",
            ),
            (
                "0.55.0",
                Path("src/render/Renderer.cpp"),
                "if (*PBLURSPECIAL && *PBLUR) {",
                "if (*PBLURSPECIAL) {",
            ),
            (
                "0.56.1",
                Path("src/output/Monitor.cpp"),
                "active && *PBLURSPECIAL && *PBLUR",
                "active || *PBLURSPECIAL || *PBLUR",
            ),
            (
                "0.55.0",
                Path("src/render/Renderer.cpp"),
                "std::max(*PBLURIGNOREA, 0.01F)",
                "*PBLURIGNOREA",
            ),
            (
                "0.56.1",
                Path("src/desktop/state/PopupFadeout.cpp"),
                "std::max(*PBLURIGNOREA, 0.01F)",
                "*PBLURIGNOREA",
            ),
            (
                "0.56.1",
                Path("src/render/Renderer.cpp"),
                "renderdata.blur = *PBLURIMES && *PBLUR;",
                "renderdata.blur = *PBLURIMES || *PBLUR;",
            ),
            (
                "0.56.1",
                Path("src/render/Renderer.cpp"),
                "|| *PBLURXRAY)",
                ")",
            ),
        )
        for version, path, original, replacement in cases:
            with self.subTest(version=version, path=path, original=original):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                self.assertIn(original, source)
                tampered[version][path] = source.replace(
                    original, replacement, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_appearance_behavior_contract"](
                        tampered
                    )

    def test_inactive_and_modal_branches_cannot_be_rewritten(self) -> None:
        cases = (
            (
                "!*PDIMENABLED",
                "*PDIMENABLED",
            ),
            (
                "goalDim = 0;",
                "goalDim = *PDIMSTRENGTH;",
            ),
            (
                "if (IS_SHADOWED_BY_MODAL && *PDIMMODAL)",
                "if (IS_SHADOWED_BY_MODAL && *PDIMMODAL && *PDIMENABLED)",
            ),
        )
        path = Path("src/desktop/view/Window.cpp")
        for version in self.sources:
            for original, replacement in cases:
                with self.subTest(version=version, original=original):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        original, replacement, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_appearance_behavior_contract"](
                            tampered
                        )

    def test_digest_authorized_border_shadow_mutations_fail_closed(self) -> None:
        shared_cases = (
            (
                module["REGISTRY_PATH"],
                '"decoration:border_part_of_window", "whether the border should be treated as a part of the window.", true',
                '"decoration:border_part_of_window", "whether the border should be treated as a part of the window.", false',
            ),
            (
                Path("src/render/decorations/CHyprBorderDecoration.cpp"),
                "*PPARTOFWINDOW && !doesntWantBorders()",
                "*PPARTOFWINDOW || !doesntWantBorders()",
            ),
            (
                Path("src/render/decorations/CHyprBorderDecoration.cpp"),
                "m_window->m_X11DoesntWantBorders ||",
                "m_window->m_X11DoesntWantBorders &&",
            ),
            (
                Path("src/render/decorations/DecorationPositioner.cpp"),
                "if (!(data->pDecoration->getDecorationFlags() & DECORATION_PART_OF_MAIN_WINDOW))",
                "if (data->pDecoration->getDecorationFlags() & DECORATION_PART_OF_MAIN_WINDOW)",
            ),
            (
                Path("src/render/decorations/DecorationPositioner.cpp"),
                "accum.addExtents(extentsToAdd);",
                "accum = decoBox;",
            ),
            (
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                "m_lastWindowBoxWithDecos = g_pDecorationPositioner->getBoxWithIncludedDecos(pWindow);",
                "m_lastWindowBoxWithDecos = pWindow->getWindowMainSurfaceBox();",
            ),
            (
                Path(
                    "src/render/decorations/CHyprDropShadowDecoration.cpp"
                ),
                "CBox fullBox = m_lastWindowBoxWithDecos;",
                "CBox fullBox = m_lastWindowBox;",
            ),
            (
                Path("src/desktop/Workspace.cpp"),
                "if (w->m_workspace != m_self)",
                "if (w->m_workspace == m_self)",
            ),
            (
                Path("src/desktop/view/Window.cpp"),
                "wd->updateWindow(m_self.lock());",
                "wd->damageEntire();",
            ),
            (
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                "ws->updateWindowDecos();",
                "ws->updateWindowData();",
            ),
        )
        for version in self.sources:
            for path, original, replacement in shared_cases:
                with self.subTest(version=version, path=path):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    self.assertIn(original, source)
                    tampered[version][path] = source.replace(
                        original, replacement, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_appearance_behavior_contract"](
                            tampered
                        )

    def test_opacity_precedence_and_map_branches_cannot_be_rewritten(self) -> None:
        path = Path("src/desktop/view/Window.cpp")
        for version in self.sources:
            source = self.sources[version][path].decode("utf-8")
            mutations = (
                (
                    "} else { if (m_self == Desktop::focusState()->window())",
                    "} if (m_self == Desktop::focusState()->window())",
                ),
                (
                    "} else { alpha(WINDOW_ALPHA_ACTIVE)->setValueAndWarp(*PINACTIVEALPHA);",
                    "} if (true) { alpha(WINDOW_ALPHA_ACTIVE)->setValueAndWarp(*PINACTIVEALPHA);",
                ),
                (
                    "&& !g_pInputManager->isConstrained()) {",
                    "|| !g_pInputManager->isConstrained()) {",
                ),
            )
            for original, replacement in mutations:
                with self.subTest(version=version, original=original):
                    self.assertIn(original, source)
                    tampered = copy.deepcopy(self.sources)
                    tampered[version][path] = source.replace(
                        original, replacement, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_appearance_behavior_contract"](
                            tampered
                        )

    def test_special_dim_remains_transition_cached_in_0561(self) -> None:
        renderer_tampered = copy.deepcopy(self.sources)
        renderer_tampered["0.56.1"][Path("src/render/Renderer.cpp")] += (
            'CConfigValue<Config::FLOAT>("decoration:dim_special");\n'.encode(
                "utf-8"
            )
        )
        with self.assertRaisesRegex(ValueError, "renderer count changed"):
            module["_assert_appearance_behavior_contract"](
                renderer_tampered
            )

        blur_renderer_tampered = copy.deepcopy(self.sources)
        blur_renderer_tampered["0.56.1"][
            Path("src/render/Renderer.cpp")
        ] += (
            'CConfigValue<Config::INTEGER>("decoration:blur:special");\n'.encode(
                "utf-8"
            )
        )
        with self.assertRaisesRegex(ValueError, "blur runtime count changed"):
            module["_assert_appearance_behavior_contract"](
                blur_renderer_tampered
            )

        refresher_tampered = copy.deepcopy(self.sources)
        refresher_path = Path(
            "src/config/supplementary/propRefresher/PropRefresher.cpp"
        )
        refresher_tampered["0.56.1"][refresher_path] += (
            "setSpecialWorkspaceVisualState(true);\n".encode("utf-8")
        )
        with self.assertRaisesRegex(ValueError, "transition cache changed"):
            module["_assert_appearance_behavior_contract"](
                refresher_tampered
            )

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_appearance_behavior_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_appearance_behavior_contract"](missing_version)

    def test_manifest_schema_keeps_the_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("appearanceBehaviorSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["appearanceBehaviorSources"][
            "maxItems"
        ] = 100
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        for label, mutate in (
            (
                "missing prefix",
                lambda prefix: prefix.pop(),
            ),
            (
                "extra prefix",
                lambda prefix: prefix.append(copy.deepcopy(prefix[-1])),
            ),
            (
                "duplicate prefix",
                lambda prefix: prefix.__setitem__(
                    1, copy.deepcopy(prefix[0])
                ),
            ),
            (
                "reordered prefix",
                lambda prefix: prefix.__setitem__(
                    slice(0, 2), [prefix[1], prefix[0]]
                ),
            ),
        ):
            with self.subTest(label=label):
                wrong_order = copy.deepcopy(schema)
                mutate(
                    wrong_order["properties"]["appearanceBehaviorSources"][
                        "prefixItems"
                    ]
                )
                with self.assertRaisesRegex(ValueError, "order is stale"):
                    module["_assert_source_manifest_schema"](wrong_order)

        wrong_selector = copy.deepcopy(schema)
        wrong_selector["properties"]["appearanceBehaviorSources"][
            "prefixItems"
        ][0]["allOf"][1]["properties"]["path"]["const"] = (
            "src/reviewed-appearance-source-removed.cpp"
        )
        with self.assertRaisesRegex(ValueError, "order is stale"):
            module["_assert_source_manifest_schema"](wrong_selector)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["appearanceBehaviorSources"]["items"] = {}
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](open_array)

        duplicate_array = copy.deepcopy(schema)
        duplicate_array["properties"]["appearanceBehaviorSources"][
            "uniqueItems"
        ] = False
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](duplicate_array)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["appearanceBehaviorSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_required = copy.deepcopy(schema)
        missing_required["$defs"]["appearanceBehaviorSource"][
            "required"
        ].pop()
        with self.assertRaisesRegex(ValueError, "required fields are stale"):
            module["_assert_source_manifest_schema"](missing_required)

        extra_path = copy.deepcopy(schema)
        extra_path["$defs"]["appearanceBehaviorSource"]["properties"][
            "path"
        ]["enum"].append("src/reviewed-appearance-source-added.cpp")
        with self.assertRaisesRegex(ValueError, "properties are stale"):
            module["_assert_source_manifest_schema"](extra_path)

        missing_pin = copy.deepcopy(schema)
        missing_pin["$defs"]["appearanceBehaviorSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_pin)

        extra_pin = copy.deepcopy(schema)
        extra_pin["$defs"]["appearanceBehaviorSource"]["oneOf"].append(
            copy.deepcopy(
                extra_pin["$defs"]["appearanceBehaviorSource"]["oneOf"][-1]
            )
        )
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](extra_pin)

        duplicate_pin = copy.deepcopy(schema)
        duplicate_pin["$defs"]["appearanceBehaviorSource"]["oneOf"][1] = (
            copy.deepcopy(
                duplicate_pin["$defs"]["appearanceBehaviorSource"]["oneOf"][0]
            )
        )
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](duplicate_pin)

        reordered_pins = copy.deepcopy(schema)
        branches = reordered_pins["$defs"]["appearanceBehaviorSource"]["oneOf"]
        branches[0], branches[1] = branches[1], branches[0]
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](reordered_pins)

        for field, value in (
            ("tag", "v0.0.0"),
            ("commit", "0" * 40),
            ("path", "src/reviewed-appearance-source-removed.cpp"),
            ("sha256", "0" * 64),
        ):
            with self.subTest(field=field):
                wrong_pin = copy.deepcopy(schema)
                wrong_pin["$defs"]["appearanceBehaviorSource"]["oneOf"][0][
                    "properties"
                ][field]["const"] = value
                with self.assertRaisesRegex(
                    ValueError, "inventory/pins are stale"
                ):
                    module["_assert_source_manifest_schema"](wrong_pin)

        extra_field = copy.deepcopy(schema)
        extra_field["$defs"]["appearanceBehaviorSource"]["oneOf"][0][
            "properties"
        ]["unexpected"] = {"const": True}
        with self.assertRaisesRegex(ValueError, "branch is malformed"):
            module["_assert_source_manifest_schema"](extra_field)

    def test_manifest_records_reject_every_open_or_wrong_appearance_case(
        self,
    ) -> None:
        root = arguments.extractor.resolve().parents[2]
        schema = json.loads(
            (root / "interfaces/hyprland/v1/source-manifest.schema.json").read_text(
                encoding="utf-8"
            )
        )
        fixture = json.loads(
            (root / "tests/fixtures/hyprland/source-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        validator = Draft202012Validator(schema)
        self.assertTrue(validator.is_valid(fixture))

        def reject(label: str, mutate) -> None:
            candidate = copy.deepcopy(fixture)
            mutate(candidate)
            with self.subTest(label=label):
                self.assertFalse(validator.is_valid(candidate))

        reject(
            "missing property",
            lambda value: value.pop("appearanceBehaviorSources"),
        )
        reject(
            "missing record",
            lambda value: value["appearanceBehaviorSources"].pop(),
        )
        reject(
            "extra record",
            lambda value: value["appearanceBehaviorSources"].append(
                copy.deepcopy(value["appearanceBehaviorSources"][-1])
            ),
        )
        reject(
            "duplicate record",
            lambda value: value["appearanceBehaviorSources"].__setitem__(
                1, copy.deepcopy(value["appearanceBehaviorSources"][0])
            ),
        )

        def reorder(value) -> None:
            records = value["appearanceBehaviorSources"]
            records[0], records[1] = records[1], records[0]

        reject("reordered records", reorder)
        for field, wrong_value in (
            ("tag", "v0.0.0"),
            ("commit", "0" * 40),
            ("path", "src/reviewed-appearance-source-removed.cpp"),
            ("sha256", "0" * 64),
        ):
            reject(
                f"wrong {field}",
                lambda value, field=field, wrong_value=wrong_value: value[
                    "appearanceBehaviorSources"
                ][0].__setitem__(field, wrong_value),
            )
        reject(
            "missing required field",
            lambda value: value["appearanceBehaviorSources"][0].pop("commit"),
        )
        reject(
            "extra record field",
            lambda value: value["appearanceBehaviorSources"][0].__setitem__(
                "extra", True
            ),
        )


class AdvancedRuntimeContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module[
            "_advanced_runtime_contract_requirements"
        ]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }
        self.sources["0.56.0"][module["REGISTRY_PATH"]] += (
            "\n".join(
                f'"{path}"'
                for path in (
                    *module["ADVANCED_RUNTIME_OPTION_PATHS"],
                    "render:use_fp16",
                )
                if path
                not in {
                    "input-capture:capture_modifiers",
                    "input-capture:enforce_barriers",
                }
            )
            + "\n"
        ).encode("utf-8")

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_advanced_runtime_contract"](self.sources)

    def test_exact_source_and_option_inventories_are_pinned(self) -> None:
        expected_sources = {
            "0.55.0": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
                Path("src/managers/SessionLockManager.cpp"),
                Path("src/render/Renderer.cpp"),
                Path("src/render/ElementRenderer.cpp"),
                Path("src/render/pass/TexPassElement.hpp"),
                Path("src/render/pass/TexPassElement.cpp"),
                Path("src/render/gl/GLElementRenderer.cpp"),
                Path("src/render/OpenGL.cpp"),
                Path("src/render/pass/SurfacePassElement.hpp"),
                Path("src/render/pass/SurfacePassElement.cpp"),
                Path("src/render/shaders/glsl/surface.frag"),
                Path("src/helpers/Monitor.cpp"),
                Path("src/managers/screenshare/ScreenshareSession.cpp"),
                Path("src/helpers/cm/ColorManagement.hpp"),
                Path("src/render/Framebuffer.cpp"),
                Path("src/helpers/MonitorResources.cpp"),
            ),
            "0.56.0": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/protocols/InputCapture.cpp"),
            ),
            "0.56.1": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/protocols/InputCapture.cpp"),
                Path("src/managers/SessionLockManager.cpp"),
                Path("src/render/Renderer.cpp"),
                Path("src/render/ElementRenderer.cpp"),
                Path("src/render/pass/TexPassElement.hpp"),
                Path("src/render/pass/TexPassElement.cpp"),
                Path("src/render/gl/GLElementRenderer.cpp"),
                Path("src/render/OpenGL.cpp"),
                Path("src/render/pass/SurfacePassElement.hpp"),
                Path("src/render/pass/SurfacePassElement.cpp"),
                Path("src/render/shaders/glsl/surface.frag"),
                Path("src/output/Monitor.cpp"),
                Path("src/managers/screenshare/ScreenshareSession.cpp"),
                Path("src/helpers/cm/ColorManagement.hpp"),
                Path("src/render/Framebuffer.cpp"),
                Path("src/output/MonitorResources.cpp"),
            ),
        }
        expected_options = (
            "misc:allow_session_lock_restore",
            "misc:lockdead_screen_delay",
            "misc:disable_scale_notification",
            "misc:render_unfocused_fps",
            "misc:screencopy_force_8b",
            "input-capture:capture_modifiers",
            "input-capture:enforce_barriers",
            "misc:disable_hyprland_logo",
            "misc:disable_splash_rendering",
            "misc:session_lock_xray",
            "misc:session_lock_blur",
            "xwayland:use_nearest_neighbor",
            "render:expand_undersized_textures",
            "render:direct_scanout",
            "render:fp16_sdr_tf",
            "render:xp_mode",
        )
        expected_render_options = expected_options[-9:]
        expected_render_since = (
            ("misc:disable_hyprland_logo", "0.55.0"),
            ("misc:disable_splash_rendering", "0.55.0"),
            ("misc:session_lock_xray", "0.55.0"),
            ("misc:session_lock_blur", "0.56.0"),
            ("xwayland:use_nearest_neighbor", "0.55.0"),
            ("render:expand_undersized_textures", "0.55.0"),
            ("render:direct_scanout", "0.55.0"),
            ("render:fp16_sdr_tf", "0.55.0"),
            ("render:xp_mode", "0.55.0"),
        )
        self.assertEqual(module["ADVANCED_RUNTIME_SOURCE_PATHS"], expected_sources)
        self.assertEqual(module["ADVANCED_RUNTIME_OPTION_PATHS"], expected_options)
        self.assertEqual(
            module["ADVANCED_RENDER_OPTION_PATHS"], expected_render_options
        )
        self.assertEqual(
            module["ADVANCED_RENDER_OPTION_SINCE"], expected_render_since
        )
        self.assertEqual(sum(map(len, expected_sources.values())), 42)
        self.assertEqual(
            {
                version: sum(map(len, requirements.values()))
                for version, requirements in self.requirements.items()
            },
            {"0.55.0": 456, "0.56.0": 81, "0.56.1": 548},
        )
        for version, paths in expected_sources.items():
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), paths)

    def test_runtime_source_hashes_are_pinned_per_tag(self) -> None:
        expected = {
            "0.55.0": {
                Path("src/helpers/cm/ColorManagement.hpp"):
                    "a3eaf1dffb8bef9d18ce0be5bc5b270b9d07f6e605f7161b6c2c7bcd91dfa434",
                Path("src/render/Framebuffer.cpp"):
                    "80ba41c93bb068a85ca94be6f95a6bd43b33f2da52618f2c7fb78c229f787648",
                Path("src/helpers/MonitorResources.cpp"):
                    "18ff58ce9ed2268f361a49f22c7e5d9b951234b0cab5335accf628b6ac92bb4c",
            },
            "0.56.0": {
                Path("src/config/values/ConfigValues.cpp"):
                    "a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754",
                Path("src/config/lua/ConfigManager.cpp"):
                    "bf295818d6ad5a1f01aa708a6843a968b9cbb14228482421bbe0e4e5b26600ed",
                Path("src/managers/input/InputManager.cpp"):
                    "030d4f734e1303b5ae92c5bbfbdf8ece1bc7debecbee7abaf28f1e53f6fca966",
                Path("src/protocols/InputCapture.cpp"):
                    "d034a7f2dd7c5010bfb52cc4db7717119a6c1482efe2dfeeae9da2fadab1a0fb",
            },
            "0.56.1": {
                Path("src/managers/input/InputManager.cpp"):
                    "07de27ef0f4c9a5c3bf14f42c07af29574d428a7b02412f785f90db30b03125e",
                Path("src/protocols/InputCapture.cpp"):
                    "d034a7f2dd7c5010bfb52cc4db7717119a6c1482efe2dfeeae9da2fadab1a0fb",
                Path("src/helpers/cm/ColorManagement.hpp"):
                    "c9b4823032e12cb45907aac714a19c5de89e5565a773b4eccbecd83ddb5d4de6",
                Path("src/render/Framebuffer.cpp"):
                    "80ba41c93bb068a85ca94be6f95a6bd43b33f2da52618f2c7fb78c229f787648",
                Path("src/output/MonitorResources.cpp"):
                    "90d39fd2c43852611b4f90ca14cc2d213f97c4770123021d2733cd1970488b8b",
            },
        }
        for version, pins in expected.items():
            for path, digest in pins.items():
                with self.subTest(version=version, path=path):
                    self.assertEqual(
                        module["QUALIFIED_SOURCE_HASHES"][version][path],
                        digest,
                    )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            "reviewed advanced runtime semantic removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_advanced_runtime_contract"](
                                tampered
                            )

    def test_input_capture_patch_contracts_and_occurrences_fail_closed(self) -> None:
        manager_path = Path("src/managers/input/InputManager.cpp")
        capture_path = Path("src/protocols/InputCapture.cpp")

        modifier_0560 = self.sources["0.56.0"][manager_path].decode("utf-8")
        self.assertIn(
            "if (PROTO::inputCapture->isCaptured()) return;",
            " ".join(modifier_0560.split()),
        )
        modifier_0561 = self.sources["0.56.1"][manager_path].decode("utf-8")
        self.assertIn(
            "if (PROTO::inputCapture->isCaptured()) { m_lastMods = "
            "shareModsFromAllKBs(MODS.depressed); return; }",
            " ".join(modifier_0561.split()),
        )
        self.assertEqual(
            self.sources["0.56.0"][capture_path],
            self.sources["0.56.1"][capture_path],
        )

        wrong_patch = copy.deepcopy(self.sources)
        wrong_patch["0.56.1"][manager_path] = wrong_patch["0.56.1"][
            manager_path
        ].replace(
            b"if (PROTO::inputCapture->isCaptured()) { m_lastMods = "
            b"shareModsFromAllKBs(MODS.depressed); return; }",
            b"if (PROTO::inputCapture->isCaptured()) return;",
            1,
        )
        with self.assertRaisesRegex(ValueError, "InputManager.cpp"):
            module["_assert_advanced_runtime_contract"](wrong_patch)

        missing_barrier_return = copy.deepcopy(self.sources)
        missing_barrier_return["0.56.0"][capture_path] = (
            missing_barrier_return["0.56.0"][capture_path].replace(
                b"return;", b"reviewed invalid barrier return removed;", 1
            )
        )
        with self.assertRaisesRegex(ValueError, "InputCapture.cpp"):
            module["_assert_advanced_runtime_contract"](missing_barrier_return)

        missing_validity_result = copy.deepcopy(self.sources)
        missing_validity_result["0.56.1"][capture_path] = (
            missing_validity_result["0.56.1"][capture_path].replace(
                b"return valid == 1 && partial == 0;",
                b"reviewed barrier validity result removed;",
                1,
            )
        )
        with self.assertRaisesRegex(ValueError, "InputCapture.cpp"):
            module["_assert_advanced_runtime_contract"](
                missing_validity_result
            )

        missing_active_state = copy.deepcopy(self.sources)
        missing_active_state["0.56.0"][capture_path] = missing_active_state[
            "0.56.0"
        ][capture_path].replace(
            b"return active != nullptr;",
            b"reviewed active capture state removed;",
            1,
        )
        with self.assertRaisesRegex(ValueError, "InputCapture.cpp"):
            module["_assert_advanced_runtime_contract"](missing_active_state)

        missing_modifier_forward = copy.deepcopy(self.sources)
        missing_modifier_forward["0.56.1"][capture_path] = (
            missing_modifier_forward["0.56.1"][capture_path].replace(
                b"active->modifiers(mods_depressed, mods_latched, "
                b"mods_locked, group);",
                b"reviewed active modifier forwarding removed;",
                1,
            )
        )
        with self.assertRaisesRegex(ValueError, "InputCapture.cpp"):
            module["_assert_advanced_runtime_contract"](
                missing_modifier_forward
            )

        missing_client_delivery = copy.deepcopy(self.sources)
        missing_client_delivery["0.56.0"][capture_path] = (
            missing_client_delivery["0.56.0"][capture_path].replace(
                b"m_eis->sendModifiers(modsDepressed, modsLatched, "
                b"modsLocked, group);",
                b"reviewed capture client delivery removed;",
                1,
            )
        )
        with self.assertRaisesRegex(ValueError, "InputCapture.cpp"):
            module["_assert_advanced_runtime_contract"](
                missing_client_delivery
            )

        unexpected_055 = copy.deepcopy(self.sources)
        unexpected_055["0.55.0"][module["REGISTRY_PATH"]] += (
            b'\n"input-capture:capture_modifiers"\n'
        )
        with self.assertRaisesRegex(ValueError, "capture_modifiers"):
            module["_assert_advanced_runtime_contract"](unexpected_055)

        duplicate_consumer = copy.deepcopy(self.sources)
        duplicate_consumer["0.56.1"][manager_path] += (
            b'\n"input-capture:capture_modifiers"\n'
        )
        with self.assertRaisesRegex(ValueError, "capture_modifiers"):
            module["_assert_advanced_runtime_contract"](duplicate_consumer)

    def test_xp_mode_chain_mutations_fail_closed_per_tag(self) -> None:
        registry_fragment = (
            'MS<Bool>("render:xp_mode", "Disable back buffer and bottom '
            'layer rendering.", false),'
        )
        binding_fragment = (
            'static auto PXPMODE = CConfigValue<Config::INTEGER>('
            '"render:xp_mode");'
        )
        no_workspace_fragments = {
            "0.55.0": (
                "if UNLIKELY (!pWorkspace) { // allow rendering without a "
                "workspace. In this case, just render layers. "
                "renderBackground(pMonitor); for (auto const& ls : "
                "pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { "
                "renderLayer(ls.lock(), pMonitor, time); } Event::bus()->"
                "m_events.render.stage.emit(RENDER_POST_WALLPAPER); for "
                "(auto const& ls : pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) { renderLayer(ls.lock(), "
                "pMonitor, time); } for (auto const& ls : "
                "pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_TOP]) { renderLayer(ls.lock(), "
                "pMonitor, time); } for (auto const& ls : "
                "pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) { "
                "renderLayer(ls.lock(), pMonitor, time); } return; }"
            ),
            "0.56.1": (
                "if UNLIKELY (!pWorkspace) { // allow rendering without a "
                "workspace. In this case, just render layers. "
                "renderBackground(pMonitor); for (auto const& ls : "
                "pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { "
                "renderLayer(ls.lock(), pMonitor, time); } "
                "renderFadeouts(pMonitor, "
                "Desktop::FADEOUT_PLANE_LAYER_BACKGROUND); Event::bus()->"
                "m_events.render.stage.emit(RENDER_POST_WALLPAPER); for "
                "(auto const& ls : pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) { renderLayer(ls.lock(), "
                "pMonitor, time); } renderFadeouts(pMonitor, "
                "Desktop::FADEOUT_PLANE_LAYER_BOTTOM); for (auto const& ls : "
                "pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_TOP]) { renderLayer(ls.lock(), "
                "pMonitor, time); } renderFadeouts(pMonitor, "
                "Desktop::FADEOUT_PLANE_LAYER_TOP); for (auto const& ls : "
                "pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) { "
                "renderLayer(ls.lock(), pMonitor, time); } "
                "renderFadeouts(pMonitor, "
                "Desktop::FADEOUT_PLANE_LAYER_OVERLAY); return; }"
            ),
        }
        active_workspace_fragments = {
            "0.55.0": (
                "if LIKELY (!*PXPMODE) { renderBackground(pMonitor); for "
                "(auto const& ls : pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { "
                "renderLayer(ls.lock(), pMonitor, time); } Event::bus()->"
                "m_events.render.stage.emit(RENDER_POST_WALLPAPER); for "
                "(auto const& ls : pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) { renderLayer(ls.lock(), "
                "pMonitor, time); } }"
            ),
            "0.56.1": (
                "if LIKELY (!*PXPMODE) { renderBackground(pMonitor); for "
                "(auto const& ls : pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { "
                "renderLayer(ls.lock(), pMonitor, time); } "
                "renderFadeouts(pMonitor, "
                "Desktop::FADEOUT_PLANE_LAYER_BACKGROUND); Event::bus()->"
                "m_events.render.stage.emit(RENDER_POST_WALLPAPER); for "
                "(auto const& ls : pMonitor->m_layerSurfaceLayers["
                "ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) { renderLayer(ls.lock(), "
                "pMonitor, time); } renderFadeouts(pMonitor, "
                "Desktop::FADEOUT_PLANE_LAYER_BOTTOM); }"
            ),
        }

        renderer_path = Path("src/render/Renderer.cpp")
        for version in ("0.55.0", "0.56.1"):
            cases = (
                ("registry declaration", module["REGISTRY_PATH"], registry_fragment),
                ("renderer live binding", renderer_path, binding_fragment),
                (
                    "no-workspace bypass",
                    renderer_path,
                    no_workspace_fragments[version],
                ),
                (
                    "active-workspace underlay gate",
                    renderer_path,
                    active_workspace_fragments[version],
                ),
            )
            for semantic, path, fragment in cases:
                with self.subTest(version=version, semantic=semantic):
                    self.assertIn(fragment, self.requirements[version][path])
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        fragment,
                        "reviewed xp-mode semantic removed",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_advanced_runtime_contract"](tampered)

    def test_nearest_neighbor_chain_mutations_fail_closed_per_tag(self) -> None:
        registry_fragment = (
            'MS<Bool>("xwayland:use_nearest_neighbor", "uses the nearest '
            'neighbor filtering for xwayland apps, making them pixelated '
            'rather than blurry", true),'
        )
        for version in ("0.55.0", "0.56.1"):
            monitor_path = (
                Path("src/helpers/Monitor.cpp")
                if version == "0.55.0"
                else Path("src/output/Monitor.cpp")
            )
            refresh_fragment = (
                "for (auto const& m : g_pCompositor->m_monitors) { "
                "g_layoutManager->recalculateMonitor(m); "
                "g_pHyprRenderer->damageMonitor(m); }"
                if version == "0.55.0"
                else "g_pHyprRenderer->damageMonitor(m);"
            )
            renderer_submit_fragment = (
                "m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));"
                if version == "0.55.0"
                else "addPassElement(makeUnique<CSurfacePassElement>(renderdata));"
            )
            cases = (
                (module["REGISTRY_PATH"], registry_fragment),
                (
                    Path("src/config/lua/ConfigManager.cpp"),
                    "Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);",
                ),
                (
                    Path(
                        "src/config/supplementary/propRefresher/PropRefresher.cpp"
                    ),
                    refresh_fragment,
                ),
                (
                    Path("src/render/Renderer.cpp"),
                    "if ((pWindow->m_isX11 && *PXWLUSENN) || pWindow->m_ruleApplicator->nearestNeighbor().valueOrDefault())",
                ),
                (Path("src/render/Renderer.cpp"), renderer_submit_fragment),
                (
                    Path("src/render/Renderer.cpp"),
                    "renderdata.useNearestNeighbor = false;",
                ),
                (
                    Path("src/render/pass/SurfacePassElement.hpp"),
                    "bool useNearestNeighbor = false;",
                ),
                (
                    Path("src/render/pass/SurfacePassElement.cpp"),
                    "CSurfacePassElement::CSurfacePassElement(const CSurfacePassElement::SRenderData& data_) : m_data(data_) {",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "m_renderData.useNearestNeighbor = element->m_data.useNearestNeighbor;",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "m_renderData.useNearestNeighbor = false;",
                ),
                (
                    Path("src/render/OpenGL.cpp"),
                    "tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);",
                ),
                (
                    Path("src/render/OpenGL.cpp"),
                    "tex->setTexParameter(GL_TEXTURE_MAG_FILTER, tex->magFilter);",
                ),
                (monitor_path, "void CMonitor::addDamage(const CBox& box) {"),
            )
            for path, fragment in cases:
                with self.subTest(version=version, path=path, fragment=fragment):
                    self.assertIn(fragment, self.requirements[version][path])
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        fragment,
                        "reviewed nearest-neighbor semantic removed",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_advanced_runtime_contract"](tampered)

    def test_expand_undersized_texture_chain_mutations_fail_closed_per_tag(
        self,
    ) -> None:
        registry_fragment = (
            'MS<Bool>("render:expand_undersized_textures", "Whether to '
            'expand textures that have not yet resized to be larger.", true),'
        )
        for version in ("0.55.0", "0.56.1"):
            monitor_path = (
                Path("src/helpers/Monitor.cpp")
                if version == "0.55.0"
                else Path("src/output/Monitor.cpp")
            )
            refresh_fragment = (
                "for (auto const& m : g_pCompositor->m_monitors) { "
                "g_layoutManager->recalculateMonitor(m); "
                "g_pHyprRenderer->damageMonitor(m); }"
                if version == "0.55.0"
                else "g_pHyprRenderer->damageMonitor(m);"
            )
            opengl_gate = (
                "if (data.allowCustomUV && "
                "data.primarySurfaceUVTopLeft != Vector2D(-1, -1)) {"
                if version == "0.55.0"
                else "const bool CUSTOMUV = data.allowCustomUV && "
                "data.primarySurfaceUVTopLeft != Vector2D(-1, -1);"
            )
            cases = (
                (module["REGISTRY_PATH"], registry_fragment),
                (
                    Path("src/config/lua/ConfigManager.cpp"),
                    "Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);",
                ),
                (
                    Path(
                        "src/config/supplementary/propRefresher/PropRefresher.cpp"
                    ),
                    refresh_fragment,
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "if (!pWindow || !pWindow->m_isX11) {",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "if (projSize != Vector2D{} && fixMisalignedFSV1) {",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "const bool SCALE_UNAWARE = pMonitor->m_scale != 1.f && "
                    "(MONITOR_WL_SCALE == pSurface->m_current.scale || "
                    "!pSurface->m_current.viewport.hasDestination);",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "const auto RATIO = projSize / EXPECTED_SIZE;",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "if (*PEXPANDEDGES && !SCALE_UNAWARE && "
                    "(RATIO.x > 1 || RATIO.y > 1)) {",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "uvBR = uvBR * FIX;",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    "calculateUVForSurface(m_data.pWindow, m_data.surface, "
                    "m_data.pMonitor->m_self.lock(), m_data.mainSurface, "
                    "windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);",
                ),
                (
                    Path("src/render/ElementRenderer.cpp"),
                    ".allowCustomUV = true,",
                ),
                (
                    Path("src/render/pass/TexPassElement.hpp"),
                    "bool allowCustomUV = false;",
                ),
                (
                    Path("src/render/pass/TexPassElement.cpp"),
                    "CTexPassElement::CTexPassElement(const SRenderData& data) "
                    ": m_data(data) {",
                ),
                (
                    Path("src/render/pass/TexPassElement.cpp"),
                    "CTexPassElement::CTexPassElement("
                    "CTexPassElement::SRenderData&& data) "
                    ": m_data(std::move(data)) {",
                ),
                (
                    Path("src/render/gl/GLElementRenderer.cpp"),
                    ".allowCustomUV = m_data.allowCustomUV,",
                ),
                (
                    Path("src/render/gl/GLElementRenderer.cpp"),
                    ".primarySurfaceUVBottomRight = "
                    "g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight,",
                ),
                (Path("src/render/OpenGL.cpp"), opengl_gate),
                (Path("src/render/OpenGL.cpp"), "verts[3].v = v1;"),
                (monitor_path, "void CMonitor::addDamage(const CBox& box) {"),
            )
            if version == "0.56.1":
                cases += (
                    (
                        Path("src/render/OpenGL.cpp"),
                        "shader->setUsesCustomUV(CUSTOMUV);",
                    ),
                )
            for path, fragment in cases:
                with self.subTest(version=version, path=path, fragment=fragment):
                    self.assertIn(fragment, self.requirements[version][path])
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        fragment,
                        "reviewed undersized-texture semantic removed",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_advanced_runtime_contract"](tampered)

    def test_direct_scanout_chain_mutations_fail_closed_per_tag(self) -> None:
        registry_fragment = (
            'MS<Int>("render:direct_scanout", "Enables direct scanout.", 0, '
            '{.min = 0, .max = 2, .map = OptionMap{{"disable", 0}, '
            '{"enable", 1}, {"auto", 2}}}),'
        )
        for version in ("0.55.0", "0.56.1"):
            monitor_path = (
                Path("src/helpers/Monitor.cpp")
                if version == "0.55.0"
                else Path("src/output/Monitor.cpp")
            )
            refresh_fragment = (
                "for (auto const& m : g_pCompositor->m_monitors) { "
                "g_layoutManager->recalculateMonitor(m); "
                "g_pHyprRenderer->damageMonitor(m); }"
                if version == "0.55.0"
                else "g_pHyprRenderer->damageMonitor(m);"
            )
            fullscreen_fragment = (
                "if (!inFullscreenMode()) { reasons |= SC_WINDOWED; "
                "if (!full) return reasons; }"
                if version == "0.55.0"
                else "if (Fullscreen::controller()->getFullscreenModes("
                "m_self.lock()).internal != Fullscreen::FSMODE_FULLSCREEN) "
                "{ reasons |= SC_WINDOWED; if (!full) return reasons; }"
            )
            mode_fragment = next(
                fragment
                for fragment in self.requirements[version][monitor_path]
                if fragment.startswith("if (*PDIRECTSCANOUT == 0)")
            )
            self.assertNotIn("*PDIRECTSCANOUT == 1", mode_fragment)
            self.assertIn("*PDIRECTSCANOUT == 2", mode_fragment)
            self.assertIn("CONTENT_TYPE_GAME", mode_fragment)
            self.assertIn("if (!m_mirrors.empty() || isMirror())", mode_fragment)

            software_cursor_fragment = (
                "if (g_pPointerManager->softwareLockedFor(m_self.lock())) "
                "{ reasons |= DS_BLOCK_SW; if (!full) return reasons; }"
                if version == "0.55.0"
                else "if (Pointer::mgr()->softwareLockedFor(m_self.lock())) "
                "{ reasons |= DS_BLOCK_SW; if (!full) return reasons; }"
            )
            cases = (
                (module["REGISTRY_PATH"], registry_fragment),
                (
                    Path("src/config/lua/ConfigManager.cpp"),
                    "Config::Supplementary::refresher()->scheduleRefresh("
                    "Supplementary::REFRESH_ALL);",
                ),
                (
                    Path(
                        "src/config/supplementary/propRefresher/"
                        "PropRefresher.cpp"
                    ),
                    refresh_fragment,
                ),
                (
                    Path("src/render/Renderer.cpp"),
                    "const bool canAttemptDirectScanout = "
                    "pMonitor->canAttemptDirectScanoutFast();",
                ),
                (
                    Path("src/render/Renderer.cpp"),
                    "if (pMonitor->attemptDirectScanout()) {",
                ),
                (
                    Path("src/render/Renderer.cpp"),
                    "pMonitor->handleDSleave();",
                ),
                (monitor_path, fullscreen_fragment),
                (
                    monitor_path,
                    "if (g_pSessionLockManager->isSessionLocked()) "
                    "{ reasons |= SC_LOCK; if (!full) return reasons; }",
                ),
                (
                    monitor_path,
                    "if (!m_layerSurfaceLayers["
                    "ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY].empty()) "
                    "{ reasons |= SC_OVERLAYS; if (!full) return reasons; }",
                ),
                (monitor_path, mode_fragment),
                (
                    monitor_path,
                    "if (g_pHyprRenderer->m_directScanoutBlocked) "
                    "{ reasons |= DS_BLOCK_RECORD; if (!full) return reasons; }",
                ),
                (monitor_path, software_cursor_fragment),
                (
                    monitor_path,
                    "if (!PSURFACE || !PSURFACE->m_current.texture || "
                    "!PSURFACE->m_current.buffer) { reasons |= "
                    "DS_BLOCK_SURFACE; return reasons; }",
                ),
                (
                    monitor_path,
                    "if (PSURFACE->m_current.bufferSize != m_pixelSize || "
                    "PSURFACE->m_current.transform != m_transform) "
                    "{ reasons |= DS_BLOCK_TRANSFORM; if (!full) "
                    "return reasons; }",
                ),
                (
                    monitor_path,
                    "if (!params.success || "
                    "!PSURFACE->m_current.texture->isDMA() /* dmabuf */) "
                    "{ reasons |= DS_BLOCK_DMA; if (!full) return reasons; }",
                ),
                (
                    monitor_path,
                    "const bool surfaceIsScRGB = surfaceIsHDR && "
                    "PSURFACE->m_colorManagement->isWindowsScRGB();",
                ),
                (
                    monitor_path,
                    "if (!surfaceIsHDR && needsCM() && !canNoShaderCM(true)) "
                    "reasons |= DS_BLOCK_CM; // block SDR that needs CM while "
                    "non-shader CM isn't available",
                ),
                (monitor_path, "const auto blockedReason = isDSBlocked();"),
                (
                    monitor_path,
                    "if (NEEDS_TEST && !m_state.test()) {",
                ),
                (monitor_path, "bool ok = m_output->commit();"),
                (monitor_path, "void CMonitor::handleDSleave() {"),
                (
                    monitor_path,
                    "return !m_solitaryClient.expired() || "
                    "!m_lastScanout.expired() || m_directScanoutIsActive;",
                ),
                (monitor_path, "void CMonitor::addDamage(const CBox& box) {"),
                (
                    Path("src/managers/screenshare/ScreenshareSession.cpp"),
                    "g_pHyprRenderer->m_directScanoutBlocked = true;",
                ),
            )
            for path, fragment in cases:
                with self.subTest(version=version, path=path, fragment=fragment):
                    self.assertIn(fragment, self.requirements[version][path])
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        fragment,
                        "reviewed direct-scanout semantic removed",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_advanced_runtime_contract"](tampered)

    def test_fp16_sdr_transfer_chain_mutations_fail_closed_per_tag(
        self,
    ) -> None:
        registry_policy = (
            'MS<Int>("render:use_fp16", "Use experimental internal FP16 '
            'buffer.", 2, {.min = 0, .max = 2, .map = OptionMap{{"disable", '
            '0}, {"enable", 1}, {"auto", 2}}}),'
        )
        registry_transfer = (
            'MS<Int>("render:fp16_sdr_tf", "Internal workbuffer transfer '
            'function for fp16 in SDR mode", 0, {.min = 0, .max = 1, .map '
            '= OptionMap{{"monitor", 0}, {"linear", 1}}}),'
        )
        for version in ("0.55.0", "0.56.1"):
            monitor_path = (
                Path("src/helpers/Monitor.cpp")
                if version == "0.55.0"
                else Path("src/output/Monitor.cpp")
            )
            resources_path = (
                Path("src/helpers/MonitorResources.cpp")
                if version == "0.55.0"
                else Path("src/output/MonitorResources.cpp")
            )
            refresh_fragment = (
                "for (auto const& m : g_pCompositor->m_monitors) { "
                "g_layoutManager->recalculateMonitor(m); "
                "g_pHyprRenderer->damageMonitor(m); }"
                if version == "0.55.0"
                else "if (m_propsTripped & REFRESH_BLUR_FB) { for (auto "
                "const& m : State::monitorState()->monitors()) { if (!m) "
                "continue; m->m_blurFBDirty = true; m->m_forceFullFrames = "
                "2; m->scheduleFrame(); } }"
            )
            schedule_fragment = (
                "g_pCompositor->scheduleFrameForMonitor(m_self.lock(), "
                "Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);"
                if version == "0.55.0"
                else "scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);"
            )
            linear_declaration = (
                "static const auto LINEAR_IMAGE_DESCRIPTION = "
                "CImageDescription::from(SImageDescription{"
                if version == "0.55.0"
                else "inline const auto LINEAR_IMAGE_DESCRIPTION = "
                "CImageDescription::from(SImageDescription{"
            )
            work_buffer_branch = (
                "if (isHDRLikeTF || value.windowsScRGB || *PFP16TF != 0) {"
                if version == "0.55.0"
                else "if (isHDRLikeTF || *PFP16TF != 0) {"
            )
            renderer_resource_bridge = (
                "const bool HAS_MIRROR_FB = g_pHyprRenderer->m_renderData."
                "pMonitor->resources()->hasMirrorFB();"
                if version == "0.55.0"
                else "const auto RESOURCES = g_pHyprRenderer->m_renderData."
                "pMonitor->resources();"
            )
            if version == "0.55.0":
                self.assertIn("value.windowsScRGB", work_buffer_branch)
            else:
                self.assertNotIn("windowsScRGB", work_buffer_branch)

            cases = (
                ("policy registry", module["REGISTRY_PATH"], registry_policy),
                (
                    "transfer registry",
                    module["REGISTRY_PATH"],
                    registry_transfer,
                ),
                (
                    "default-elision reset",
                    Path("src/config/lua/ConfigManager.cpp"),
                    "v.second->reset();",
                ),
                (
                    "reload scheduling",
                    Path("src/config/lua/ConfigManager.cpp"),
                    "Config::Supplementary::refresher()->scheduleRefresh("
                    "Supplementary::REFRESH_ALL);",
                ),
                (
                    "reload refresh",
                    Path(
                        "src/config/supplementary/propRefresher/"
                        "PropRefresher.cpp"
                    ),
                    refresh_fragment,
                ),
                ("damage frame", monitor_path, schedule_fragment),
                (
                    "gamma transfer mapping",
                    monitor_path,
                    "case NTransferFunction::TF_FORCED_GAMMA22: return "
                    "NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22;",
                ),
                (
                    "sRGB transfer mapping",
                    monitor_path,
                    "case NTransferFunction::TF_SRGB: return "
                    "NColorManagement::CM_TRANSFER_FUNCTION_SRGB;",
                ),
                (
                    "automatic transfer mapping",
                    monitor_path,
                    "default: return chooseTF(sdrEOTF);",
                ),
                (
                    "FP16 policy read",
                    monitor_path,
                    'static const auto PFP16 = CConfigValue<Hyprlang::INT>('
                    '"render:use_fp16");',
                ),
                (
                    "sRGB transfer predicate",
                    monitor_path,
                    "if (m_imageDescription->value().transferFunction != "
                    "CM_TRANSFER_FUNCTION_SRGB && "
                    "m_imageDescription->value().transferFunction != "
                    "CM_TRANSFER_FUNCTION_GAMMA22)",
                ),
                (
                    "sRGB primary predicate",
                    monitor_path,
                    "if (m_imageDescription->value().primariesNamed != "
                    "CM_PRIMARIES_SRGB)",
                ),
                (
                    "FP16 disabled enabled automatic modes",
                    monitor_path,
                    "bool shouldUse = *PFP16 == 1 || (*PFP16 == 2 && "
                    "!isSRGB());",
                ),
                (
                    "FP16 cache invalidation",
                    monitor_path,
                    "m_blurFBDirty = true;",
                ),
                (
                    "SDR transfer read",
                    monitor_path,
                    'static const auto PFP16TF = CConfigValue<Hyprlang::INT>('
                    '"render:fp16_sdr_tf");',
                ),
                (
                    "dormant non-FP16 non-ICC path",
                    monitor_path,
                    "if (!useFP16() && !m_imageDescription->value().icc.present)",
                ),
                (
                    "HDR-like definition",
                    monitor_path,
                    "const bool isHDRLikeTF = value.transferFunction == "
                    "CM_TRANSFER_FUNCTION_ST2084_PQ || "
                    "value.transferFunction == CM_TRANSFER_FUNCTION_HLG || "
                    "value.transferFunction == "
                    "CM_TRANSFER_FUNCTION_EXT_LINEAR;",
                ),
                ("HDR-like linear branch", monitor_path, work_buffer_branch),
                (
                    "cached linear replacement",
                    monitor_path,
                    "m_cachedInternalDescription = "
                    "LINEAR_IMAGE_DESCRIPTION->with(value.luminances);",
                ),
                (
                    "display-transfer replacement",
                    monitor_path,
                    "if (cached.transferFunction != chooseTF(m_sdrEotf))",
                ),
                (
                    "display-transfer value",
                    monitor_path,
                    ".transferFunction = chooseTF(m_sdrEotf),",
                ),
                (
                    "work-buffer sRGB primaries",
                    monitor_path,
                    ".primariesNamed = NColorManagement::CM_PRIMARIES_SRGB,",
                ),
                (
                    "resource format",
                    monitor_path,
                    "const auto DRM_FORMAT = useFP16() ? "
                    "DRM_FORMAT_ABGR16161616F : "
                    "m_output->state->state().drmFormat;",
                ),
                (
                    "resource description",
                    monitor_path,
                    "const auto DESC = workBufferImageDescription();",
                ),
                (
                    "resource construction",
                    monitor_path,
                    "m_resources = makeUnique<CMonitorResources>(m_self, "
                    "DRM_FORMAT, m_pixelSize, DESC);",
                ),
                (
                    "resource description update",
                    monitor_path,
                    "m_resources->setImageDescription(DESC);",
                ),
                (
                    "per-frame renderer entry",
                    Path("src/render/Renderer.cpp"),
                    "bool IHyprRenderer::beginRender(PHLMONITOR pMonitor, "
                    "CRegion& damage, eRenderMode mode, SP<IHLBuffer> buffer, "
                    "SP<IFramebuffer> fb, bool simple) {",
                ),
                (
                    "per-frame monitor selection",
                    Path("src/render/Renderer.cpp"),
                    "m_renderData.pMonitor = pMonitor;",
                ),
                (
                    "per-frame resource recomputation",
                    Path("src/render/Renderer.cpp"),
                    renderer_resource_bridge,
                ),
                (
                    "renderer forwarding",
                    Path("src/render/Renderer.cpp"),
                    "return m_renderData.pMonitor->workBufferImageDescription();",
                ),
                (
                    "OpenGL work-buffer read",
                    Path("src/render/OpenGL.cpp"),
                    "const auto WORK_BUFFER_IMAGE_DESCRIPTION = "
                    "g_pHyprRenderer->m_renderData.pMonitor->"
                    "workBufferImageDescription();",
                ),
                (
                    "OpenGL texture metadata",
                    Path("src/render/OpenGL.cpp"),
                    "return tex->m_imageDescription;",
                ),
                (
                    "OpenGL source fallback",
                    Path("src/render/OpenGL.cpp"),
                    "return tex->m_imageDescription ? tex->m_imageDescription "
                    ": WORK_BUFFER_IMAGE_DESCRIPTION;",
                ),
                (
                    "OpenGL current framebuffer metadata",
                    Path("src/render/OpenGL.cpp"),
                    "return g_pHyprRenderer->m_renderData.currentFB->"
                    "imageDescription();",
                ),
                (
                    "OpenGL work-buffer target fallback",
                    Path("src/render/OpenGL.cpp"),
                    "return WORK_BUFFER_IMAGE_DESCRIPTION;",
                ),
                (
                    "OpenGL color-management predicate",
                    Path("src/render/OpenGL.cpp"),
                    "|| !SOURCE_IMAGE_DESCRIPTION->needsCM("
                    "TARGET_IMAGE_DESCRIPTION) /* Source and target have "
                    "matching image descriptions */",
                ),
                (
                    "linear definition",
                    Path("src/helpers/cm/ColorManagement.hpp"),
                    linear_declaration,
                ),
                (
                    "extended-linear transfer",
                    Path("src/helpers/cm/ColorManagement.hpp"),
                    ".transferFunction = "
                    "NColorManagement::CM_TRANSFER_FUNCTION_EXT_LINEAR,",
                ),
                (
                    "extended-linear primaries",
                    Path("src/helpers/cm/ColorManagement.hpp"),
                    ".primaries = NColorPrimaries::BT709,",
                ),
                (
                    "extended-linear luminance",
                    Path("src/helpers/cm/ColorManagement.hpp"),
                    ".luminances = {.min = 0, .max = 10000, .reference = 80},",
                ),
                (
                    "framebuffer getter",
                    Path("src/render/Framebuffer.cpp"),
                    "return m_tex ? m_tex->m_imageDescription : "
                    "m_imageDescription;",
                ),
                (
                    "framebuffer stored metadata",
                    Path("src/render/Framebuffer.cpp"),
                    "m_imageDescription = desc;",
                ),
                (
                    "framebuffer texture metadata",
                    Path("src/render/Framebuffer.cpp"),
                    "m_tex->m_imageDescription = desc;",
                ),
                (
                    "resource constructor metadata",
                    resources_path,
                    "m_imageDescription(imageDescription) {",
                ),
                (
                    "new framebuffer metadata",
                    resources_path,
                    "fb->setImageDescription(m_imageDescription);",
                ),
                (
                    "blur framebuffer update",
                    resources_path,
                    "m_blurFB->setImageDescription(imageDescription);",
                ),
                (
                    "existing work-buffer update",
                    resources_path,
                    "res.buffer->setImageDescription(imageDescription);",
                ),
                (
                    "existing mirror framebuffer update",
                    resources_path,
                    "m_monitorMirrorFB->setImageDescription("
                    "getMirrorTexImageDescription());",
                ),
                (
                    "existing mirror texture update",
                    resources_path,
                    "m_mirrorTex->m_imageDescription = "
                    "getMirrorTexImageDescription();",
                ),
                (
                    "reused work-buffer",
                    resources_path,
                    "return found->buffer;",
                ),
                (
                    "new work-buffer initialization",
                    resources_path,
                    "initFB(res.buffer);",
                ),
            )
            if version == "0.56.1":
                cases += (
                    (
                        "user-set marker reset",
                        Path("src/config/lua/ConfigManager.cpp"),
                        "v.second->resetSetByUser();",
                    ),
                    (
                        "mirror framebuffer invalidation",
                        resources_path,
                        "invalidateMirrorFB();",
                    ),
                )

            for semantic, path, fragment in cases:
                with self.subTest(
                    version=version,
                    semantic=semantic,
                    path=path,
                ):
                    self.assertIn(fragment, self.requirements[version][path])
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        fragment,
                        "reviewed FP16 SDR transfer semantic removed",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_advanced_runtime_contract"](tampered)

    def test_option_runtime_counts_fail_closed(self) -> None:
        cases = (
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "misc:allow_session_lock_restore",
            ),
            (
                "0.56.1",
                Path("src/managers/SessionLockManager.cpp"),
                "misc:lockdead_screen_delay",
            ),
            (
                "0.55.0",
                Path("src/render/Renderer.cpp"),
                "misc:render_unfocused_fps",
            ),
            (
                "0.56.1",
                Path("src/output/Monitor.cpp"),
                "misc:disable_scale_notification",
            ),
            (
                "0.55.0",
                Path("src/helpers/Monitor.cpp"),
                "misc:screencopy_force_8b",
            ),
            (
                "0.56.1",
                Path("src/managers/screenshare/ScreenshareSession.cpp"),
                "misc:screencopy_force_8b",
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "misc:session_lock_blur",
            ),
            (
                "0.56.1",
                module["REGISTRY_PATH"],
                "misc:session_lock_blur",
            ),
            (
                "0.55.0",
                Path("src/render/Renderer.cpp"),
                "misc:session_lock_xray",
            ),
            (
                "0.56.1",
                Path("src/render/Renderer.cpp"),
                "misc:session_lock_blur",
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "xwayland:use_nearest_neighbor",
            ),
            (
                "0.56.1",
                Path("src/render/Renderer.cpp"),
                "xwayland:use_nearest_neighbor",
            ),
            (
                "0.55.0",
                Path("src/render/ElementRenderer.cpp"),
                "xwayland:use_nearest_neighbor",
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "render:expand_undersized_textures",
            ),
            (
                "0.56.1",
                Path("src/render/ElementRenderer.cpp"),
                "render:expand_undersized_textures",
            ),
            (
                "0.55.0",
                Path("src/render/gl/GLElementRenderer.cpp"),
                "render:expand_undersized_textures",
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "render:direct_scanout",
            ),
            (
                "0.56.1",
                Path("src/output/Monitor.cpp"),
                "render:direct_scanout",
            ),
            (
                "0.55.0",
                Path("src/render/Renderer.cpp"),
                "render:direct_scanout",
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "render:fp16_sdr_tf",
            ),
            (
                "0.56.1",
                Path("src/output/Monitor.cpp"),
                "render:fp16_sdr_tf",
            ),
            (
                "0.56.1",
                module["REGISTRY_PATH"],
                "render:use_fp16",
            ),
            (
                "0.55.0",
                Path("src/helpers/Monitor.cpp"),
                "render:use_fp16",
            ),
            (
                "0.55.0",
                module["REGISTRY_PATH"],
                "render:xp_mode",
            ),
            (
                "0.56.1",
                Path("src/render/Renderer.cpp"),
                "render:xp_mode",
            ),
        )
        for version, path, option_path in cases:
            with self.subTest(version=version, path=path, option=option_path):
                tampered = copy.deepcopy(self.sources)
                tampered[version][path] += f'"{option_path}"\n'.encode("utf-8")
                with self.assertRaisesRegex(ValueError, "runtime count changed"):
                    module["_assert_advanced_runtime_contract"](tampered)

    def test_render_catalog_metadata_and_version_boundary_fail_closed(self) -> None:
        options = [
            {
                "path": path,
                "type": (
                    "enum"
                    if path
                    in ("render:direct_scanout", "render:fp16_sdr_tf")
                    else "boolean"
                ),
                "default": (
                    0
                    if path
                    in ("render:direct_scanout", "render:fp16_sdr_tf")
                    else path
                    in (
                        "xwayland:use_nearest_neighbor",
                        "render:expand_undersized_textures",
                    )
                ),
                "control": (
                    "select"
                    if path
                    in ("render:direct_scanout", "render:fp16_sdr_tf")
                    else "toggle"
                ),
                "constraints": (
                    {
                        "min": 0,
                        "max": 2,
                        "choices": [
                            {"label": "disable", "value": 0},
                            {"label": "enable", "value": 1},
                            {"label": "auto", "value": 2},
                        ],
                    }
                    if path == "render:direct_scanout"
                    else (
                        {
                            "min": 0,
                            "max": 1,
                            "choices": [
                                {"label": "monitor", "value": 0},
                                {"label": "linear", "value": 1},
                            ],
                        }
                        if path == "render:fp16_sdr_tf"
                        else {}
                    )
                ),
                "risk": "safe" if path.startswith("misc:") else "caution",
                "applyMode": "reload",
                "since": since,
            }
            for path, since in module["ADVANCED_RENDER_OPTION_SINCE"]
        ]
        catalog = {"options": options}
        module["_assert_advanced_render_catalog"](catalog)

        for index, field, replacement in (
            (0, "type", "integer"),
            (1, "default", True),
            (2, "applyMode", "restart"),
            (3, "since", "0.56.1"),
            (4, "type", "integer"),
            (4, "default", False),
            (4, "applyMode", "restart"),
            (4, "since", "0.56.0"),
            (5, "type", "integer"),
            (5, "default", False),
            (5, "applyMode", "restart"),
            (5, "since", "0.56.0"),
            (6, "type", "integer"),
            (6, "default", 1),
            (6, "control", "toggle"),
            (6, "constraints", {}),
            (6, "applyMode", "restart"),
            (6, "since", "0.56.0"),
            (7, "type", "integer"),
            (7, "default", 1),
            (7, "control", "toggle"),
            (7, "constraints", {}),
            (7, "applyMode", "restart"),
            (7, "since", "0.56.0"),
            (8, "type", "integer"),
            (8, "default", True),
            (8, "control", "select"),
            (8, "constraints", {"min": 0}),
            (8, "risk", "safe"),
            (8, "applyMode", "restart"),
            (8, "since", "0.56.0"),
        ):
            with self.subTest(index=index, field=field):
                tampered = copy.deepcopy(catalog)
                tampered["options"][index][field] = replacement
                with self.assertRaisesRegex(ValueError, "contract changed"):
                    module["_assert_advanced_render_catalog"](tampered)

        for field, replacement in (
            ("min", -1),
            ("max", 2),
            (
                "choices",
                [
                    {"label": "linear", "value": 1},
                    {"label": "monitor", "value": 0},
                ],
            ),
        ):
            with self.subTest(fp16_sdr_tf_constraint=field):
                tampered = copy.deepcopy(catalog)
                tampered["options"][7]["constraints"][field] = replacement
                with self.assertRaisesRegex(ValueError, "contract changed"):
                    module["_assert_advanced_render_catalog"](tampered)

        for field, replacement in (
            ("min", -1),
            ("max", 3),
            (
                "choices",
                [
                    {"label": "disable", "value": 0},
                    {"label": "auto", "value": 2},
                    {"label": "enable", "value": 1},
                ],
            ),
        ):
            with self.subTest(direct_scanout_constraint=field):
                tampered = copy.deepcopy(catalog)
                tampered["options"][6]["constraints"][field] = replacement
                with self.assertRaisesRegex(ValueError, "contract changed"):
                    module["_assert_advanced_render_catalog"](tampered)

        missing = copy.deepcopy(catalog)
        missing["options"].pop()
        with self.assertRaisesRegex(ValueError, "contract changed"):
            module["_assert_advanced_render_catalog"](missing)

    def test_source_missing_extra_and_order_fail_closed(self) -> None:
        missing = copy.deepcopy(self.sources)
        missing["0.55.0"].pop(Path("src/helpers/cm/ColorManagement.hpp"))
        with self.assertRaisesRegex(ValueError, "inventory/order changed"):
            module["_assert_advanced_runtime_contract"](missing)

        extra = copy.deepcopy(self.sources)
        extra["0.56.1"][Path("src/unreviewed.cpp")] = b"unreviewed\n"
        with self.assertRaisesRegex(ValueError, "inventory/order changed"):
            module["_assert_advanced_runtime_contract"](extra)

        reordered = copy.deepcopy(self.sources)
        items = list(reordered["0.55.0"].items())
        items[15], items[16] = items[16], items[15]
        reordered["0.55.0"] = dict(items)
        with self.assertRaisesRegex(ValueError, "inventory/order changed"):
            module["_assert_advanced_runtime_contract"](reordered)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "version inventory changed"):
            module["_assert_advanced_runtime_contract"](missing_version)

    def test_manifest_schema_keeps_the_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("advancedRuntimeSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["advancedRuntimeSources"]["maxItems"] = 41
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["advancedRuntimeSources"]["uniqueItems"] = False
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](open_array)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["advancedRuntimeSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_required = copy.deepcopy(schema)
        missing_required["$defs"]["advancedRuntimeSource"]["required"].pop()
        with self.assertRaisesRegex(ValueError, "required fields are stale"):
            module["_assert_source_manifest_schema"](missing_required)

        missing_version = copy.deepcopy(schema)
        missing_version["$defs"]["advancedRuntimeSource"]["properties"][
            "version"
        ]["enum"].remove("0.56.0")
        with self.assertRaisesRegex(ValueError, "properties are stale"):
            module["_assert_source_manifest_schema"](missing_version)

        duplicate_path = copy.deepcopy(schema)
        path_enum = duplicate_path["$defs"]["advancedRuntimeSource"][
            "properties"
        ]["path"]["enum"]
        path_enum[-1] = path_enum[0]
        with self.assertRaisesRegex(ValueError, "properties are stale"):
            module["_assert_source_manifest_schema"](duplicate_path)

        missing_source = copy.deepcopy(schema)
        missing_source["$defs"]["advancedRuntimeSource"]["oneOf"].pop(15)
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_source)

        extra_source = copy.deepcopy(schema)
        extra_source["$defs"]["advancedRuntimeSource"]["oneOf"].append(
            copy.deepcopy(
                extra_source["$defs"]["advancedRuntimeSource"]["oneOf"][0]
            )
        )
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](extra_source)

        duplicate_source = copy.deepcopy(schema)
        duplicate_source["$defs"]["advancedRuntimeSource"]["oneOf"][16] = (
            copy.deepcopy(
                duplicate_source["$defs"]["advancedRuntimeSource"]["oneOf"][15]
            )
        )
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](duplicate_source)

        reordered = copy.deepcopy(schema)
        branches = reordered["$defs"]["advancedRuntimeSource"]["oneOf"]
        branches[15], branches[16] = branches[16], branches[15]
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](reordered)

        wrong_hash = copy.deepcopy(schema)
        wrong_hash["$defs"]["advancedRuntimeSource"]["oneOf"][15][
            "properties"
        ]["sha256"]["const"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](wrong_hash)

        wrong_tag = copy.deepcopy(schema)
        wrong_tag["$defs"]["advancedRuntimeSource"]["oneOf"][0][
            "properties"
        ]["tag"]["const"] = "v0.56.1"
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](wrong_tag)

        extra_field = copy.deepcopy(schema)
        extra_field["$defs"]["advancedRuntimeSource"]["oneOf"][0][
            "properties"
        ]["unexpected"] = {"const": True}
        with self.assertRaisesRegex(ValueError, "branch is malformed"):
            module["_assert_source_manifest_schema"](extra_field)


class WindowBehaviorContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_window_behavior_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_window_behavior_contract"](self.sources)

    def test_exact_version_split_inventory_is_pinned(self) -> None:
        expected = {
            "0.55.0": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/shared/actions/ConfigActions.cpp"),
                Path("src/Compositor.cpp"),
                Path("src/managers/ANRManager.cpp"),
                Path(
                    "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"
                ),
                Path(
                    "src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"
                ),
                Path(
                    "src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"
                ),
                Path(
                    "src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"
                ),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/desktop/state/FocusState.cpp"),
                Path(
                    "src/desktop/rule/windowRule/WindowRuleApplicator.cpp"
                ),
                Path("src/layout/target/WindowTarget.cpp"),
            ),
            "0.56.1": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/shared/actions/ConfigActions.cpp"),
                Path("src/desktop/state/WindowQuery.cpp"),
                Path("src/managers/fullscreen/FullscreenController.cpp"),
                Path("src/managers/ANRManager.cpp"),
                Path(
                    "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"
                ),
                Path(
                    "src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"
                ),
                Path(
                    "src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"
                ),
                Path(
                    "src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"
                ),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/desktop/state/FocusState.cpp"),
                Path(
                    "src/desktop/rule/windowRule/WindowRuleApplicator.cpp"
                ),
                Path("src/layout/target/WindowTarget.cpp"),
            ),
        }
        self.assertEqual(module["WINDOW_BEHAVIOR_SOURCE_PATHS"], expected)
        self.assertEqual(sum(map(len, expected.values())), 27)
        for version, paths in expected.items():
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), paths)

    def test_exact_authored_option_inventory_is_covered(self) -> None:
        self.assertEqual(
            module["WINDOW_BEHAVIOR_OPTION_PATHS"],
            (
                "binds:allow_pin_fullscreen",
                "binds:focus_preferred_method",
                "binds:ignore_group_lock",
                "binds:movefocus_cycles_fullscreen",
                "binds:movefocus_cycles_groupfirst",
                "binds:window_direction_monitor_fallback",
                "misc:enable_anr_dialog",
                "misc:anr_missed_pings",
                "misc:size_limits_tiled",
                "misc:always_follow_on_dnd",
                "misc:enable_swallow",
                "misc:swallow_regex",
                "misc:swallow_exception_regex",
                "misc:focus_on_activate",
                "misc:mouse_move_focuses_monitor",
                "misc:on_focus_under_fullscreen",
                "misc:exit_window_retains_fullscreen",
            ),
        )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            "reviewed window semantic removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_window_behavior_contract"](
                                tampered
                            )

    def test_duplicate_runtime_gate_is_rejected(self) -> None:
        cases = (
            (
                "0.55.0",
                Path("src/config/shared/actions/ConfigActions.cpp"),
                "binds:movefocus_cycles_groupfirst",
            ),
            (
                "0.56.1",
                Path("src/managers/fullscreen/FullscreenController.cpp"),
                "binds:allow_pin_fullscreen",
            ),
            (
                "0.56.1",
                Path("src/managers/ANRManager.cpp"),
                "misc:anr_missed_pings",
            ),
            (
                "0.55.0",
                Path("src/managers/input/InputManager.cpp"),
                "misc:always_follow_on_dnd",
            ),
            (
                "0.56.1",
                Path("src/managers/input/InputManager.cpp"),
                "misc:mouse_move_focuses_monitor",
            ),
            (
                "0.55.0",
                Path("src/desktop/view/Window.cpp"),
                "misc:focus_on_activate",
            ),
            (
                "0.55.0",
                Path("src/desktop/view/Window.cpp"),
                "misc:enable_swallow",
            ),
            (
                "0.56.1",
                Path("src/desktop/view/Window.cpp"),
                "misc:swallow_regex",
            ),
            (
                "0.55.0",
                Path("src/desktop/view/Window.cpp"),
                "misc:swallow_exception_regex",
            ),
            (
                "0.56.1",
                Path("src/desktop/view/Window.cpp"),
                "misc:exit_window_retains_fullscreen",
            ),
            (
                "0.55.0",
                Path("src/desktop/state/FocusState.cpp"),
                "misc:on_focus_under_fullscreen",
            ),
            (
                "0.56.1",
                Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"),
                "misc:size_limits_tiled",
            ),
            (
                "0.55.0",
                Path("src/layout/target/WindowTarget.cpp"),
                "misc:size_limits_tiled",
            ),
        )
        for version, path, option in cases:
            with self.subTest(version=version, option=option):
                tampered = copy.deepcopy(self.sources)
                tampered[version][path] += f'"{option}"\n'.encode("utf-8")
                with self.assertRaisesRegex(ValueError, "gate count changed"):
                    module["_assert_window_behavior_contract"](tampered)

    def test_each_layout_monitor_fallback_gate_fails_closed(self) -> None:
        layout_paths = (
            Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
            Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"),
            Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"),
            Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"),
        )
        for version in self.sources:
            for path in layout_paths:
                with self.subTest(version=version, path=path):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        "!*PMONITORFALLBACK",
                        "*PMONITORFALLBACK",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_window_behavior_contract"](tampered)

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_window_behavior_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_window_behavior_contract"](missing_version)

    def test_manifest_schema_keeps_the_window_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("windowBehaviorSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["windowBehaviorSources"][
            "maxItems"
        ] = 26
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["windowBehaviorSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_pin = copy.deepcopy(schema)
        missing_pin["$defs"]["windowBehaviorSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_pin)


class WorkspaceBehaviorContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_workspace_behavior_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_workspace_behavior_contract"](self.sources)

    def test_exact_version_split_inventory_is_pinned(self) -> None:
        expected = {
            "0.55.0": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/shared/actions/ConfigActions.cpp"),
                Path("src/desktop/history/WorkspaceHistoryTracker.cpp"),
                Path("src/desktop/history/WorkspaceHistoryTracker.hpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/Compositor.cpp"),
            ),
            "0.56.1": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/shared/actions/ConfigActions.cpp"),
                Path("src/desktop/history/WorkspaceHistoryTracker.cpp"),
                Path("src/desktop/history/WorkspaceHistoryTracker.hpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/state/WorkspacePlacementController.cpp"),
                Path("src/pointer/PointerController.cpp"),
            ),
        }
        self.assertEqual(module["WORKSPACE_BEHAVIOR_SOURCE_PATHS"], expected)
        for version, paths in expected.items():
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), paths)

    def test_exact_authored_and_interacting_option_inventories_are_covered(
        self,
    ) -> None:
        self.assertEqual(
            module["WORKSPACE_BEHAVIOR_OPTION_PATHS"],
            (
                "binds:allow_workspace_cycles",
                "binds:hide_special_on_workspace_change",
                "binds:workspace_back_and_forth",
                "binds:workspace_center_on",
                "cursor:warp_on_change_workspace",
                "cursor:warp_on_toggle_special",
            ),
        )
        self.assertEqual(
            module["WORKSPACE_BEHAVIOR_INTERACTION_OPTION_PATHS"],
            ("cursor:no_warps", "cursor:persistent_warps"),
        )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(
                        version=version, path=path, index=index
                    ):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            "reviewed workspace semantic removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_workspace_behavior_contract"](
                                tampered
                            )

    def test_no_warps_mode_one_gate_mutation_fails_closed(self) -> None:
        paths = {
            "0.55.0": Path("src/Compositor.cpp"),
            "0.56.1": Path("src/pointer/PointerController.cpp"),
        }
        for version, path in paths.items():
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                tampered[version][path] = source.replace(
                    "if (*PNOWARPS && !force) {",
                    "if (*PNOWARPS && force) {",
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_workspace_behavior_contract"](tampered)

    def test_force_mode_bypass_mutations_fail_closed(self) -> None:
        path = Path("src/config/shared/actions/ConfigActions.cpp")
        cases = (
            (
                "PLAST->warpCursor(*PWARPONWORKSPACECHANGE == 2);",
                "PLAST->warpCursor(false);",
            ),
            (
                "PLAST->warpCursor(*PWARPONTOGGLESPECIAL == 2);",
                "PLAST->warpCursor(false);",
            ),
        )
        for version in self.sources:
            for old, new in cases:
                with self.subTest(version=version, fragment=old):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        old, new, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_workspace_behavior_contract"](
                            tampered
                        )

    def test_persistent_relative_and_center_warp_mutations_fail_closed(
        self,
    ) -> None:
        path = Path("src/desktop/view/Window.cpp")
        for version in self.sources:
            requirements = self.requirements[version][path]
            relative = next(
                fragment
                for fragment in requirements
                if "coords, force" in fragment
            )
            center = next(
                fragment
                for fragment in requirements
                if "middle(), force" in fragment
            )
            for fragment in (relative, center):
                with self.subTest(version=version, fragment=fragment):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        fragment,
                        "reviewed persistent warp branch removed",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_workspace_behavior_contract"](
                            tampered
                        )

    def test_regular_and_special_warp_gates_are_independent(self) -> None:
        path = Path("src/config/shared/actions/ConfigActions.cpp")
        cases = (
            (
                '"cursor:warp_on_change_workspace"',
                '"cursor:warp_on_toggle_special"',
            ),
            (
                '"cursor:warp_on_toggle_special"',
                '"cursor:warp_on_change_workspace"',
            ),
        )
        for version in self.sources:
            for old, new in cases:
                with self.subTest(version=version, gate=old):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        old, new, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, "gate count changed"):
                        module["_assert_workspace_behavior_contract"](
                            tampered
                        )

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_workspace_behavior_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_workspace_behavior_contract"](missing_version)

    def test_manifest_schema_keeps_the_workspace_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("workspaceBehaviorSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["workspaceBehaviorSources"][
            "maxItems"
        ] = 12
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["workspaceBehaviorSources"][
            "uniqueItems"
        ] = False
        with self.assertRaisesRegex(ValueError, "array/count is stale"):
            module["_assert_source_manifest_schema"](open_array)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["workspaceBehaviorSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_pin = copy.deepcopy(schema)
        missing_pin["$defs"]["workspaceBehaviorSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_pin)


class InputBehaviorContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_input_behavior_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_input_behavior_contract"](self.sources)

    def test_exact_nineteen_role_inventory_is_pinned_at_both_tags(self) -> None:
        expected = {
            "0.55.0": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                Path("src/managers/input/InputManager.hpp"),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/managers/input/Tablets.cpp"),
                Path("src/protocols/PrimarySelection.cpp"),
                Path("src/render/Renderer.cpp"),
                Path("src/render/Renderer.hpp"),
                Path("src/managers/input/Touch.cpp"),
                Path("src/devices/Mouse.cpp"),
                Path("src/managers/PointerManager.hpp"),
                Path("src/managers/PointerManager.cpp"),
                Path("src/config/shared/actions/ConfigActions.cpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/desktop/view/Window.hpp"),
                Path("src/Compositor.cpp"),
                Path("src/Compositor.hpp"),
            ),
            "0.56.1": (
                Path("src/config/values/ConfigValues.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                Path("src/managers/input/InputManager.hpp"),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/managers/input/Tablets.cpp"),
                Path("src/protocols/PrimarySelection.cpp"),
                Path("src/render/Renderer.cpp"),
                Path("src/render/Renderer.hpp"),
                Path("src/managers/input/Touch.cpp"),
                Path("src/devices/Mouse.cpp"),
                Path("src/pointer/PointerManager.hpp"),
                Path("src/pointer/PointerManager.cpp"),
                Path("src/config/shared/actions/ConfigActions.cpp"),
                Path("src/desktop/view/Window.cpp"),
                Path("src/desktop/view/Window.hpp"),
                Path("src/pointer/PointerController.cpp"),
                Path("src/pointer/PointerController.hpp"),
            ),
        }
        self.assertEqual(module["INPUT_BEHAVIOR_SOURCE_PATHS"], expected)
        self.assertEqual(tuple(self.requirements), ("0.55.0", "0.56.1"))
        self.assertEqual(sum(map(len, expected.values())), 38)
        for version, paths in expected.items():
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), paths)

    def test_exact_option_inventory_is_covered(self) -> None:
        self.assertEqual(
            module["INPUT_BEHAVIOR_OPTION_PATHS"],
            (
                "input:force_no_accel",
                "input:rotation",
                "input:touchdevice:enabled",
                "input:touchdevice:transform",
                "input:tablet:region_position",
                "input:tablet:absolute_region_position",
                "input:tablet:region_size",
                "input:tablet:relative_input",
                "input:tablet:left_handed",
                "input:tablet:transform",
                "misc:middle_click_paste",
                "cursor:hide_on_key_press",
                "cursor:hide_on_touch",
                "cursor:hide_on_tablet",
                "cursor:inactive_timeout",
                "cursor:hotspot_padding",
                "cursor:no_warps",
                "cursor:persistent_warps",
                "cursor:warp_back_after_non_mouse_input",
                "input:follow_mouse_threshold",
                "input:resolve_binds_by_sym",
            ),
        )

    def test_exact_hyprland_semantic_fragment_totals_are_frozen(self) -> None:
        self.assertEqual(
            {
                version: sum(len(fragments) for fragments in requirements.values())
                for version, requirements in self.requirements.items()
            },
            {"0.55.0": 280, "0.56.1": 285},
        )

    def test_resolve_binds_by_symbol_chain_mutations_fail_closed_per_tag(
        self,
    ) -> None:
        registry_path = module["REGISTRY_PATH"]
        device_path = Path(
            "src/config/lua/bindings/LuaBindingsConfigRules.cpp"
        )
        refresher_path = Path(
            "src/config/supplementary/propRefresher/PropRefresher.cpp"
        )
        manager_path = Path("src/managers/input/InputManager.cpp")
        registry = (
            'MS<Bool>("input:resolve_binds_by_sym", "Determines how keybinds '
            'act when multiple layouts are used.", false, '
            "{.refresh = Supplementary::REFRESH_INPUT_DEVICES}),"
        )
        device_factory = (
            '"resolve_binds_by_sym", []() -> ILuaConfigValue* '
            "{ return new CLuaConfigBool(false); }},"
        )
        fallback = (
            'Config::mgr()->getDeviceInt(devname, "resolve_binds_by_sym", '
            '"input:resolve_binds_by_sym")'
        )
        cases = (
            ("registry Bool", registry_path, registry, registry.replace("MS<Bool>", "MS<Int>")),
            ("registry default", registry_path, registry, registry.replace(", false,", ", true,")),
            (
                "registry refresh",
                registry_path,
                registry,
                registry.replace("REFRESH_INPUT_DEVICES", "REFRESH_LAYOUT"),
            ),
            (
                "device Bool factory",
                device_path,
                device_factory,
                device_factory.replace("CLuaConfigBool(false)", "CLuaConfigInt(0)"),
            ),
            (
                "device default",
                device_path,
                device_factory,
                device_factory.replace("false", "true"),
            ),
            (
                "device refresh scheduling",
                device_path,
                "Supplementary::refresher()->scheduleRefresh("
                "Supplementary::REFRESH_INPUT_DEVICES);",
                "Supplementary::refresher()->scheduleRefresh("
                "Supplementary::REFRESH_LAYOUT);",
            ),
            (
                "input refresh execution",
                refresher_path,
                "g_pInputManager->setKeyboardLayout();",
                "g_pInputManager->setPointerConfigs();",
            ),
            (
                "per-device field",
                manager_path,
                fallback,
                fallback.replace('"resolve_binds_by_sym"', '"numlock_by_default"', 1),
            ),
            (
                "global fallback",
                manager_path,
                fallback,
                fallback.replace('"input:resolve_binds_by_sym"', '"input:numlock_by_default"'),
            ),
            (
                "global translation refresh",
                manager_path,
                "g_pKeybindManager->updateXKBTranslationState();",
                "g_pKeybindManager->m_keyToCodeCache.clear();",
            ),
        )
        for version in self.sources:
            for semantic, path, original, replacement in cases:
                with self.subTest(version=version, semantic=semantic):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    self.assertIn(original, source)
                    tampered[version][path] = source.replace(
                        original, replacement, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_input_behavior_contract"](tampered)

    def test_resolve_assignment_precedes_unchanged_keymap_return(self) -> None:
        path = Path("src/managers/input/InputManager.cpp")
        assignment = "pKeyboard->m_resolveBindsBySym = RESOLVEBINDSBYSYM;"
        unchanged_return = (
            'Log::logger->log(Log::DEBUG, "Not applying config to keyboard, '
            'it did not change.");\nreturn;'
        )
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                self.assertIn(assignment + "\n", source)
                self.assertIn(unchanged_return, source)
                source = source.replace(assignment + "\n", "", 1)
                source = source.replace(
                    unchanged_return,
                    unchanged_return + "\n" + assignment,
                    1,
                )
                tampered[version][path] = source.encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_input_behavior_contract"](tampered)

    def test_follow_mouse_threshold_chain_mutations_fail_closed_per_tag(
        self,
    ) -> None:
        registry_path = module["REGISTRY_PATH"]
        header_path = Path("src/managers/input/InputManager.hpp")
        manager_path = Path("src/managers/input/InputManager.cpp")
        action_path = Path("src/config/shared/actions/ConfigActions.cpp")
        common_cases = (
            (
                "Float registry default",
                registry_path,
                'MS<Float>("input:follow_mouse_threshold", "The smallest '
                'distance in logical pixels the mouse needs to travel for the '
                'window under it to get focused.", 0),',
            ),
            (
                "zeroed accumulator",
                header_path,
                "double m_mousePosDelta = 0;",
            ),
            (
                "live Float consumer",
                manager_path,
                'static auto PFOLLOWMOUSETHRESHOLD = CConfigValue<'
                'Config::FLOAT>("input:follow_mouse_threshold");',
            ),
            (
                "drag-and-drop effective mode",
                manager_path,
                "const auto FOLLOWMOUSE = *PFOLLOWONDND && "
                "PROTO::data->dndActive() ? 1 : *PFOLLOWMOUSE;",
            ),
            (
                "half-second accumulation window",
                manager_path,
                "if (FOLLOWMOUSE == 1 && "
                "m_lastCursorMovement.getSeconds() < 0.5)",
            ),
            (
                "distance accumulation call",
                manager_path,
                "m_mousePosDelta += "
                "MOUSECOORDSFLOORED.distance(m_lastCursorPosFloored);",
            ),
            (
                "non-follow or stale-movement reset",
                manager_path,
                "else m_mousePosDelta = 0;",
            ),
            (
                "strict threshold or refocus gate",
                manager_path,
                "if (m_mousePosDelta > *PFOLLOWMOUSETHRESHOLD || refocus) {",
            ),
            (
                "no-follow Window Rule transport",
                action_path,
                'else if (PROP == "no_follow_mouse") '
                "parsePropTrivial(PWINDOW->m_ruleApplicator->"
                "noFollowMouse(), VAL);",
            ),
            (
                "no-follow Window Rule lookup",
                manager_path,
                "const bool hasNoFollowMouse = pFoundWindow && "
                "pFoundWindow->m_ruleApplicator->noFollowMouse()"
                ".valueOrDefault();",
            ),
            (
                "ordinary gate and refocus bypass",
                manager_path,
                "if (refocus || !hasNoFollowMouse)",
            ),
            (
                "focus consumer",
                manager_path,
                "Desktop::focusState()->rawWindowFocus("
                "pFoundWindow, FOCUS_REASON, foundSurface);",
            ),
            (
                "motion call",
                manager_path,
                "mouseMoveUnified(e.timeMs, false, e.mouse);",
            ),
            (
                "post-motion timer reset",
                manager_path,
                "m_lastCursorMovement.reset(); m_lastInputTouch = false; "
                "m_lastInputTablet = false;",
            ),
        )
        for version in ("0.55.0", "0.56.1"):
            cases = list(common_cases)
            if version == "0.56.1":
                cases.append(
                    (
                        "captured-motion early return",
                        manager_path,
                        "if (PROTO::inputCapture->isCaptured()) return;",
                    )
                )
            for semantic, path, fragment in cases:
                with self.subTest(version=version, semantic=semantic):
                    self.assertIn(fragment, self.requirements[version][path])
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        fragment,
                        "reviewed follow-mouse-threshold semantic removed",
                        1,
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_input_behavior_contract"](tampered)

    def test_follow_mouse_threshold_motion_precedes_timer_reset_per_tag(
        self,
    ) -> None:
        path = Path("src/managers/input/InputManager.cpp")
        call = "mouseMoveUnified(e.timeMs, false, e.mouse);"
        reset = (
            "m_lastCursorMovement.reset(); m_lastInputTouch = false; "
            "m_lastInputTablet = false;"
        )
        for version in ("0.55.0", "0.56.1"):
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                self.assertIn(f"{call}\n{reset}", source)
                tampered[version][path] = source.replace(
                    f"{call}\n{reset}", f"{reset}\n{call}", 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_input_behavior_contract"](tampered)

    def test_follow_mouse_threshold_has_one_runtime_consumer_per_tag(
        self,
    ) -> None:
        path = Path("src/managers/input/InputManager.cpp")
        for version in ("0.55.0", "0.56.1"):
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                tampered[version][path] += (
                    b'\nCConfigValue<Config::FLOAT>("input:'
                    b'follow_mouse_threshold");\n'
                )
                with self.assertRaisesRegex(
                    ValueError, "runtime gate count changed"
                ):
                    module["_assert_input_behavior_contract"](tampered)

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        changed = list(fragments)
                        changed[index] = "reviewed input semantic removed"
                        tampered[version][path] = (
                            "\n".join(changed) + "\n"
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_input_behavior_contract"](tampered)

    def test_wheel_input_cannot_be_misclassified_as_timeout_activity(self) -> None:
        path = Path("src/managers/input/InputManager.cpp")
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                boundary = (
                    "void CInputManager::onMouseWheel(IPointer::SAxisEvent e, "
                    "SP<IPointer> pointer) {"
                )
                tampered[version][path] = source.replace(
                    boundary,
                    boundary + "\nm_lastCursorMovement.reset();",
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "wheel unexpectedly resets"):
                    module["_assert_input_behavior_contract"](tampered)

    def test_duplicate_primary_selection_gate_fails_closed(self) -> None:
        path = Path("src/protocols/PrimarySelection.cpp")
        duplicated = copy.deepcopy(self.sources)
        duplicated["0.56.1"][path] += b'"misc:middle_click_paste"\n'
        with self.assertRaisesRegex(ValueError, "gate count changed"):
            module["_assert_input_behavior_contract"](duplicated)

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_input_behavior_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_input_behavior_contract"](missing_version)

    def test_manifest_schema_keeps_the_input_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("inputBehaviorSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["inputBehaviorSources"]["maxItems"] = 37
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        reordered_prefix = copy.deepcopy(schema)
        prefix = reordered_prefix["properties"]["inputBehaviorSources"][
            "prefixItems"
        ]
        prefix[0], prefix[1] = prefix[1], prefix[0]
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](reordered_prefix)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["inputBehaviorSources"]["items"] = {}
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](open_array)

        duplicate_array = copy.deepcopy(schema)
        duplicate_array["properties"]["inputBehaviorSources"][
            "uniqueItems"
        ] = False
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](duplicate_array)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["inputBehaviorSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_required = copy.deepcopy(schema)
        missing_required["$defs"]["inputBehaviorSource"]["required"].pop()
        with self.assertRaisesRegex(ValueError, "required fields are stale"):
            module["_assert_source_manifest_schema"](missing_required)

        missing_branch = copy.deepcopy(schema)
        missing_branch["$defs"]["inputBehaviorSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_branch)

        reordered_branches = copy.deepcopy(schema)
        branches = reordered_branches["$defs"]["inputBehaviorSource"]["oneOf"]
        branches[0], branches[1] = branches[1], branches[0]
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](reordered_branches)

        for field, value in (
            ("tag", "v0.0.0"),
            ("commit", "0" * 40),
            ("path", "src/reviewed-input-source-removed.cpp"),
            ("sha256", "0" * 64),
        ):
            with self.subTest(field=field):
                wrong_pin = copy.deepcopy(schema)
                wrong_pin["$defs"]["inputBehaviorSource"]["oneOf"][0][
                    "properties"
                ][field]["const"] = value
                with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
                    module["_assert_source_manifest_schema"](wrong_pin)

        extra_branch_field = copy.deepcopy(schema)
        extra_branch_field["$defs"]["inputBehaviorSource"]["oneOf"][0][
            "properties"
        ]["extra"] = {"const": True}
        with self.assertRaisesRegex(ValueError, "branch is malformed"):
            module["_assert_source_manifest_schema"](extra_branch_field)

    def test_manifest_records_reject_every_open_or_wrong_input_case(self) -> None:
        root = arguments.extractor.resolve().parents[2]
        schema = json.loads(
            (root / "interfaces/hyprland/v1/source-manifest.schema.json").read_text(
                encoding="utf-8"
            )
        )
        fixture = json.loads(
            (root / "tests/fixtures/hyprland/source-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        validator = Draft202012Validator(schema)
        self.assertTrue(validator.is_valid(fixture))

        def reject(label: str, mutate) -> None:
            candidate = copy.deepcopy(fixture)
            mutate(candidate)
            with self.subTest(label=label):
                self.assertFalse(validator.is_valid(candidate))

        reject("missing property", lambda value: value.pop("inputBehaviorSources"))
        reject(
            "missing record",
            lambda value: value["inputBehaviorSources"].pop(),
        )
        reject(
            "extra record",
            lambda value: value["inputBehaviorSources"].append(
                copy.deepcopy(value["inputBehaviorSources"][-1])
            ),
        )
        reject(
            "duplicate record",
            lambda value: value["inputBehaviorSources"].__setitem__(
                1, copy.deepcopy(value["inputBehaviorSources"][0])
            ),
        )

        def reorder(value) -> None:
            records = value["inputBehaviorSources"]
            records[0], records[1] = records[1], records[0]

        reject("reordered records", reorder)
        for field, wrong_value in (
            ("tag", "v0.0.0"),
            ("commit", "0" * 40),
            ("path", "src/reviewed-input-source-removed.cpp"),
            ("sha256", "0" * 64),
        ):
            reject(
                f"wrong {field}",
                lambda value, field=field, wrong_value=wrong_value: value[
                    "inputBehaviorSources"
                ][0].__setitem__(field, wrong_value),
            )
        reject(
            "missing required field",
            lambda value: value["inputBehaviorSources"][0].pop("commit"),
        )
        reject(
            "extra record field",
            lambda value: value["inputBehaviorSources"][0].__setitem__(
                "extra", True
            ),
        )


class InputBehaviorDependencyContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module[
            "_input_behavior_dependency_contract_requirements"
        ]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in self.requirements.items()
            }
            for version in module["INPUT_BEHAVIOR_DEPENDENCY_SOURCES"]
        }
        self.flake_locks = {
            version: json.dumps(
                {
                    "root": "root",
                    "nodes": {
                        "root": {"inputs": {"hyprutils": "hyprutils"}},
                        "hyprutils": {
                            "locked": {
                                "owner": "hyprwm",
                                "repo": "hyprutils",
                                "rev": dependency["revision"],
                                "type": "github",
                            }
                        },
                    },
                }
            ).encode("utf-8")
            for version, dependency in module[
                "INPUT_BEHAVIOR_DEPENDENCY_SOURCES"
            ].items()
        }

    def test_reviewed_dependency_fixture_is_accepted(self) -> None:
        module["_assert_input_behavior_dependency_contract"](
            self.flake_locks, self.sources
        )

    def test_exact_six_record_dependency_inventory_is_pinned(self) -> None:
        vector = Path("include/hyprutils/math/Vector2D.hpp")
        box = Path("src/math/Box.cpp")
        self.assertEqual(
            module["HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS"],
            (vector, box),
        )
        self.assertEqual(
            module["INPUT_BEHAVIOR_DEPENDENCY_SOURCES"],
            {
                "0.55.0": {
                    "repository": "https://github.com/hyprwm/hyprutils",
                    "revision": "a2dbd8a4cc51f7cbe4224732668392bb1aa79df2",
                    "hashes": {
                        vector: "26079ea62f7a4eca1e3792e7a37c2ca6d1736e3ec879dd35997d29758c9098aa",
                        box: "2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c",
                    },
                },
                "0.56.1": {
                    "repository": "https://github.com/hyprwm/hyprutils",
                    "revision": "5f03477ab3a005ff27c527486f551883535aea2f",
                    "hashes": {
                        vector: "26079ea62f7a4eca1e3792e7a37c2ca6d1736e3ec879dd35997d29758c9098aa",
                        box: "2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c",
                    },
                },
            },
        )
        self.assertEqual(
            2
            * (
                1
                + len(module["HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS"])
            ),
            6,
        )

    def test_exact_hyprutils_semantic_fragment_totals_are_frozen(self) -> None:
        self.assertEqual(
            {path.as_posix(): len(fragments) for path, fragments in self.requirements.items()},
            {
                "include/hyprutils/math/Vector2D.hpp": 6,
                "src/math/Box.cpp": 29,
            },
        )
        self.assertEqual(
            {
                version: sum(len(fragments) for fragments in self.requirements.values())
                for version in module["INPUT_BEHAVIOR_DEPENDENCY_SOURCES"]
            },
            {"0.55.0": 35, "0.56.1": 35},
        )

    def test_flake_revision_and_root_binding_mutations_fail_closed(self) -> None:
        changed_revision = copy.deepcopy(self.flake_locks)
        changed_revision["0.55.0"] = changed_revision["0.55.0"].replace(
            b"a2dbd8a4cc51f7cbe4224732668392bb1aa79df2",
            b"0000000000000000000000000000000000000000",
        )
        with self.assertRaisesRegex(ValueError, "pinned Hyprutils revision"):
            module["_assert_input_behavior_dependency_contract"](
                changed_revision, self.sources
            )

        changed_root = copy.deepcopy(self.flake_locks)
        changed_root["0.56.1"] = changed_root["0.56.1"].replace(
            b'"hyprutils": "hyprutils"', b'"hyprutils": "other"'
        )
        with self.assertRaisesRegex(ValueError, "dependency binding"):
            module["_assert_input_behavior_dependency_contract"](
                changed_root, self.sources
            )

    def test_every_dependency_runtime_fragment_fails_closed(self) -> None:
        for version in self.sources:
            for path, fragments in self.requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        changed = list(fragments)
                        changed[index] = "reviewed dependency semantic removed"
                        tampered[version][path] = (
                            "\n".join(changed) + "\n"
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_input_behavior_dependency_contract"](
                                self.flake_locks, tampered
                            )

    def test_missing_dependency_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            module["HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS"][0]
        )
        with self.assertRaisesRegex(ValueError, "inventory is incomplete"):
            module["_assert_input_behavior_dependency_contract"](
                self.flake_locks, missing_source
            )

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "inventory is incomplete"):
            module["_assert_input_behavior_dependency_contract"](
                self.flake_locks, missing_version
            )

    def test_dependency_schema_inventory_is_required_and_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("inputBehaviorDependencySources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["inputBehaviorDependencySources"][
            "maxItems"
        ] = 5
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        reordered_prefix = copy.deepcopy(schema)
        prefix = reordered_prefix["properties"][
            "inputBehaviorDependencySources"
        ]["prefixItems"]
        prefix[0], prefix[1] = prefix[1], prefix[0]
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](reordered_prefix)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["inputBehaviorDependencySources"][
            "items"
        ] = {}
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](open_array)

        duplicate_array = copy.deepcopy(schema)
        duplicate_array["properties"]["inputBehaviorDependencySources"][
            "uniqueItems"
        ] = False
        with self.assertRaisesRegex(ValueError, "array/count/order is stale"):
            module["_assert_source_manifest_schema"](duplicate_array)

        open_record = copy.deepcopy(schema)
        open_record["$defs"]["inputBehaviorDependencySource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_record)

        missing_required = copy.deepcopy(schema)
        missing_required["$defs"]["inputBehaviorDependencySource"][
            "required"
        ].pop()
        with self.assertRaisesRegex(ValueError, "required fields are stale"):
            module["_assert_source_manifest_schema"](missing_required)

        missing_branch = copy.deepcopy(schema)
        missing_branch["$defs"]["inputBehaviorDependencySource"][
            "oneOf"
        ].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](missing_branch)

        reordered_branches = copy.deepcopy(schema)
        branches = reordered_branches["$defs"][
            "inputBehaviorDependencySource"
        ]["oneOf"]
        branches[0], branches[1] = branches[1], branches[0]
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](reordered_branches)

        for field, value in (
            ("repository", "https://github.com/example/reviewed-source"),
            ("revision", "0" * 40),
            ("path", "reviewed-dependency-source-removed.cpp"),
            ("sha256", "0" * 64),
        ):
            with self.subTest(field=field):
                wrong_pin = copy.deepcopy(schema)
                wrong_pin["$defs"]["inputBehaviorDependencySource"][
                    "oneOf"
                ][0]["properties"][field]["const"] = value
                with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
                    module["_assert_source_manifest_schema"](wrong_pin)

        extra_branch_field = copy.deepcopy(schema)
        extra_branch_field["$defs"]["inputBehaviorDependencySource"][
            "oneOf"
        ][0]["properties"]["extra"] = {"const": True}
        with self.assertRaisesRegex(ValueError, "branch is malformed"):
            module["_assert_source_manifest_schema"](extra_branch_field)

    def test_manifest_records_reject_every_open_or_wrong_dependency_case(
        self,
    ) -> None:
        root = arguments.extractor.resolve().parents[2]
        schema = json.loads(
            (root / "interfaces/hyprland/v1/source-manifest.schema.json").read_text(
                encoding="utf-8"
            )
        )
        fixture = json.loads(
            (root / "tests/fixtures/hyprland/source-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        validator = Draft202012Validator(schema)
        self.assertTrue(validator.is_valid(fixture))

        def reject(label: str, mutate) -> None:
            candidate = copy.deepcopy(fixture)
            mutate(candidate)
            with self.subTest(label=label):
                self.assertFalse(validator.is_valid(candidate))

        reject(
            "missing property",
            lambda value: value.pop("inputBehaviorDependencySources"),
        )
        reject(
            "missing record",
            lambda value: value["inputBehaviorDependencySources"].pop(),
        )
        reject(
            "extra record",
            lambda value: value["inputBehaviorDependencySources"].append(
                copy.deepcopy(value["inputBehaviorDependencySources"][-1])
            ),
        )
        reject(
            "duplicate record",
            lambda value: value["inputBehaviorDependencySources"].__setitem__(
                1, copy.deepcopy(value["inputBehaviorDependencySources"][0])
            ),
        )

        def reorder(value) -> None:
            records = value["inputBehaviorDependencySources"]
            records[0], records[1] = records[1], records[0]

        reject("reordered records", reorder)
        for field, wrong_value in (
            ("hyprlandVersion", "0.0.0"),
            ("repository", "https://github.com/example/reviewed-source"),
            ("revision", "0" * 40),
            ("path", "reviewed-dependency-source-removed.cpp"),
            ("sha256", "0" * 64),
        ):
            reject(
                f"wrong {field}",
                lambda value, field=field, wrong_value=wrong_value: value[
                    "inputBehaviorDependencySources"
                ][0].__setitem__(field, wrong_value),
            )
        reject(
            "missing required field",
            lambda value: value["inputBehaviorDependencySources"][0].pop(
                "revision"
            ),
        )
        reject(
            "extra record field",
            lambda value: value["inputBehaviorDependencySources"][0].__setitem__(
                "extra", True
            ),
        )


class InputDeviceContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_input_device_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_input_device_contract"](self.sources)

    def test_exact_seventeen_role_inventory_is_pinned_at_both_tags(self) -> None:
        expected = {
            "0.55.0": (
                Path("src/debug/HyprCtl.cpp"),
                Path("src/helpers/MiscFunctions.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                Path("src/managers/input/InputManager.hpp"),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/managers/input/Tablets.cpp"),
                Path("src/devices/IHID.cpp"),
                Path("src/devices/IPointer.cpp"),
                Path("src/devices/IKeyboard.cpp"),
                Path("src/devices/VirtualKeyboard.cpp"),
                Path("src/devices/VirtualPointer.cpp"),
                Path("src/protocols/VirtualKeyboard.cpp"),
                Path("src/protocols/VirtualPointer.cpp"),
                Path("src/Compositor.cpp"),
                Path("src/managers/PointerManager.cpp"),
            ),
            "0.56.1": (
                Path("src/debug/HyprCtl.cpp"),
                Path("src/helpers/MiscFunctions.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
                Path(
                    "src/config/supplementary/propRefresher/PropRefresher.cpp"
                ),
                Path("src/managers/input/InputManager.hpp"),
                Path("src/managers/input/InputManager.cpp"),
                Path("src/managers/input/Tablets.cpp"),
                Path("src/devices/IHID.cpp"),
                Path("src/devices/IPointer.cpp"),
                Path("src/devices/IKeyboard.cpp"),
                Path("src/devices/VirtualKeyboard.cpp"),
                Path("src/devices/VirtualPointer.cpp"),
                Path("src/protocols/VirtualKeyboard.cpp"),
                Path("src/protocols/VirtualPointer.cpp"),
                Path("src/Compositor.cpp"),
                Path("src/pointer/PointerManager.cpp"),
            ),
        }
        self.assertEqual(module["INPUT_DEVICE_SOURCE_PATHS"], expected)
        self.assertEqual(sum(map(len, expected.values())), 34)
        for version, paths in expected.items():
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), paths)

    def test_exact_semantic_fragment_totals_are_frozen(self) -> None:
        self.assertEqual(
            {
                version: sum(len(fragments) for fragments in requirements.values())
                for version, requirements in self.requirements.items()
            },
            {"0.55.0": 252, "0.56.1": 256},
        )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            "reviewed input-device semantic removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_input_device_contract"](tampered)

    def test_keyboard_symbol_state_mutations_fail_closed(self) -> None:
        path = Path("src/devices/IKeyboard.cpp")
        cases = (
            (
                "provided-keymap state",
                "m_xkbSymState = xkb_state_new(keymap);",
                "m_xkbSymState = nullptr;",
            ),
            (
                "active-layout state",
                "m_xkbSymState = xkb_state_new(KEYMAP);",
                "m_xkbSymState = nullptr;",
            ),
            (
                "fallback state",
                "m_xkbSymState = xkb_state_new(NEWKEYMAP);",
                "m_xkbSymState = nullptr;",
            ),
            (
                "explicit active group",
                "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, group);",
                "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, group, 0);",
            ),
            (
                "key-derived active group",
                "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, "
                "m_modifiersState.group);",
                "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, 0);",
            ),
        )
        for version in self.sources:
            for semantic, original, replacement in cases:
                with self.subTest(version=version, semantic=semantic):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    self.assertIn(original, source)
                    tampered[version][path] = source.replace(
                        original, replacement, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, path.name):
                        module["_assert_input_device_contract"](tampered)

    def test_keyboard_symbol_state_duplicates_fail_closed(self) -> None:
        path = Path("src/devices/IKeyboard.cpp")
        additions = (
            (
                "symbol-state creation",
                "m_xkbSymState = xkb_state_new(keymap);",
            ),
            (
                "active-group update",
                "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, group);",
            ),
        )
        for version in self.sources:
            for error, addition in additions:
                with self.subTest(version=version, error=error):
                    tampered = copy.deepcopy(self.sources)
                    tampered[version][path] += f"\n{addition}\n".encode("utf-8")
                    with self.assertRaisesRegex(ValueError, error):
                        module["_assert_input_device_contract"](tampered)

    def test_wire_field_counts_and_omissions_fail_closed(self) -> None:
        path = Path("src/debug/HyprCtl.cpp")
        wire_tokens = (
            '\\"mice\\"',
            '\\"keyboards\\"',
            '\\"tablets\\"',
            '\\"touch\\"',
            '\\"switches\\"',
            '"address"',
            '"name"',
            '"type"',
            '"belongsTo"',
            '"active_layout_index"',
            '"active_keymap"',
        )
        for version in self.sources:
            for token in wire_tokens:
                with self.subTest(version=version, token=token):
                    duplicated = copy.deepcopy(self.sources)
                    source = duplicated[version][path].decode("utf-8")
                    duplicated[version][path] = source.replace(
                        "} else {", f"{token}\n}} else {{", 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, "field/count"):
                        module["_assert_input_device_contract"](duplicated)

        forbidden = (
            '"enabled"',
            '"isVirtual"',
            '"virtual"',
            '"touchpad"',
            '"capabilities"',
            '"count"',
        )
        for version in self.sources:
            for field in forbidden:
                with self.subTest(version=version, field=field):
                    disclosed = copy.deepcopy(self.sources)
                    source = disclosed[version][path].decode("utf-8")
                    disclosed[version][path] = source.replace(
                        "} else {", f"{field}\n}} else {{", 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, "omission contract"):
                        module["_assert_input_device_contract"](disclosed)

    def test_pointer_touch_and_tablet_reset_mutations_fail_closed(self) -> None:
        path = Path("src/managers/input/InputManager.cpp")
        for version in self.sources:
            pointer_reset = copy.deepcopy(self.sources)
            source = pointer_reset[version][path].decode("utf-8")
            pointer_reset[version][path] = source.replace(
                "void CInputManager::setPointerConfigs() {",
                "void CInputManager::setPointerConfigs() {\n"
                "if (!HASCONFIG) attachPointerAgain();",
                1,
            ).encode("utf-8")
            with self.subTest(version=version, seam="pointer"):
                with self.assertRaisesRegex(ValueError, "pointer no-config"):
                    module["_assert_input_device_contract"](pointer_reset)

            for fragment in (
                "libinput_device_config_calibration_get_default_matrix",
                "libinput_device_config_calibration_set_matrix("
                "LIBINPUTDEV, IDENTITY",
            ):
                touch_reset = copy.deepcopy(self.sources)
                source = touch_reset[version][path].decode("utf-8")
                touch_reset[version][path] = source.replace(
                    "void CInputManager::setTouchDeviceConfigs(SP<ITouch> dev) {",
                    "void CInputManager::setTouchDeviceConfigs(SP<ITouch> dev) {\n"
                    + fragment,
                    1,
                ).encode("utf-8")
                with self.subTest(version=version, seam="touch", fragment=fragment):
                    with self.assertRaisesRegex(ValueError, "touch transform reset"):
                        module["_assert_input_device_contract"](touch_reset)

            for fragment in (
                '"enabled"',
                "libinput_device_config_calibration_get_default_matrix",
                "libinput_device_config_calibration_set_matrix("
                "LIBINPUTDEV, IDENTITY",
                "t->m_activeArea = {}",
            ):
                tablet_reset = copy.deepcopy(self.sources)
                source = tablet_reset[version][path].decode("utf-8")
                tablet_reset[version][path] = source.replace(
                    "void CInputManager::setTabletConfigs() {",
                    "void CInputManager::setTabletConfigs() {\n" + fragment,
                    1,
                ).encode("utf-8")
                with self.subTest(
                    version=version, seam="tablet", fragment=fragment
                ):
                    with self.assertRaisesRegex(ValueError, "tablet reset seam"):
                        module["_assert_input_device_contract"](tablet_reset)

    def test_virtual_pointer_name_assignment_fails_closed(self) -> None:
        path = Path("src/protocols/VirtualPointer.cpp")
        for version in self.sources:
            with self.subTest(version=version):
                assigned = copy.deepcopy(self.sources)
                assigned[version][path] += b'm_name = "virtual-pointer";\n'
                with self.assertRaisesRegex(ValueError, "name timing"):
                    module["_assert_input_device_contract"](assigned)

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_input_device_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_input_device_contract"](missing_version)

    def test_manifest_schema_keeps_the_input_device_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("inputDeviceSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["inputDeviceSources"]["maxItems"] = 33
        with self.assertRaisesRegex(ValueError, "count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        open_inventory = copy.deepcopy(schema)
        open_inventory["$defs"]["inputDeviceSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](open_inventory)


class GestureContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_gesture_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_gesture_runtime_contract"](self.sources)

    def test_exact_nineteen_role_inventory_is_pinned_at_both_tags(self) -> None:
        expected = (
            Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
            Path("src/config/lua/ConfigManager.cpp"),
            Path("src/managers/input/trackpad/GestureTypes.hpp"),
            Path("src/managers/input/trackpad/TrackpadGestures.cpp"),
            Path("src/managers/input/trackpad/TrackpadGestures.hpp"),
            Path(
                "src/managers/input/trackpad/gestures/"
                "ITrackpadGesture.cpp"
            ),
            Path(
                "src/managers/input/trackpad/gestures/"
                "ITrackpadGesture.hpp"
            ),
            Path(
                "src/managers/input/trackpad/gestures/"
                "WorkspaceSwipeGesture.cpp"
            ),
            Path(
                "src/managers/input/trackpad/gestures/ResizeGesture.cpp"
            ),
            Path("src/managers/input/trackpad/gestures/MoveGesture.cpp"),
            Path("src/managers/input/trackpad/gestures/CloseGesture.cpp"),
            Path(
                "src/managers/input/trackpad/gestures/"
                "ScrollMoveGesture.cpp"
            ),
            Path(
                "src/managers/input/trackpad/gestures/"
                "SpecialWorkspaceGesture.cpp"
            ),
            Path("src/managers/input/trackpad/gestures/FloatGesture.cpp"),
            Path("src/managers/input/trackpad/gestures/FloatGesture.hpp"),
            Path(
                "src/managers/input/trackpad/gestures/"
                "FullscreenGesture.cpp"
            ),
            Path(
                "src/managers/input/trackpad/gestures/"
                "FullscreenGesture.hpp"
            ),
            Path(
                "src/managers/input/trackpad/gestures/"
                "CursorZoomGesture.cpp"
            ),
            Path(
                "src/managers/input/trackpad/gestures/"
                "CursorZoomGesture.hpp"
            ),
        )
        self.assertEqual(module["GESTURE_SOURCE_PATHS"], expected)
        self.assertEqual(tuple(self.requirements), ("0.55.0", "0.56.1"))
        for version in self.requirements:
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), expected)

        self.assertEqual(
            tuple(module["GESTURE_ACTION_SOURCE_PATHS"]),
            (
                "workspace",
                "resize",
                "move",
                "close",
                "scrollMove",
                "special",
                "float",
                "fullscreen",
                "cursorZoom",
            ),
        )

    def test_exact_source_derived_action_inventory_is_authenticated(self) -> None:
        expected = (
            "workspace",
            "resize",
            "move",
            "special",
            "close",
            "float",
            "fullscreen",
            "cursorZoom",
            "scrollMove",
            "unset",
        )
        lua_path = Path(
            "src/config/lua/bindings/LuaBindingsConfigRules.cpp"
        )
        for version, sources in self.sources.items():
            with self.subTest(version=version):
                self.assertEqual(
                    module["extract_gesture_action_ids"](
                        sources[lua_path].decode("utf-8")
                    ),
                    expected,
                )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(
                        version=version, path=path, index=index
                    ):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment,
                            "reviewed gesture semantic removed",
                            1,
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_gesture_runtime_contract"](
                                tampered
                            )

    def test_action_spelling_mutation_fails_closed(self) -> None:
        lua_path = Path(
            "src/config/lua/bindings/LuaBindingsConfigRules.cpp"
        )
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][lua_path].decode("utf-8")
                tampered[version][lua_path] = source.replace(
                    'else if (action == "resize")',
                    'else if (action == "renamed")',
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "action spellings"):
                    module["_assert_gesture_runtime_contract"](tampered)

    def test_pinch_scale_transport_mutation_fails_closed(self) -> None:
        path = Path("src/managers/input/trackpad/TrackpadGestures.cpp")
        begin = (
            "m_activeGesture->gesture->begin({.pinch = &e, "
            ".direction = direction});"
        )
        scaled = (
            "m_activeGesture->gesture->begin({.pinch = &e, "
            ".direction = direction, .scale = g->deltaScale});"
        )
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                tampered[version][path] = source.replace(
                    begin, scaled, 1
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "pinch scale transport"):
                    module["_assert_gesture_runtime_contract"](tampered)

    def test_scroll_move_pinch_mutation_fails_closed(self) -> None:
        path = Path(
            "src/managers/input/trackpad/gestures/ScrollMoveGesture.cpp"
        )
        signature = (
            "static float deltaForUpdate("
            "const ITrackpadGesture::STrackpadGestureUpdate& e) {"
        )
        for version in self.sources:
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                tampered[version][path] = source.replace(
                    signature,
                    signature + "\nconst auto pinchDelta = e.pinch;",
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "pinch no-op"):
                    module["_assert_gesture_runtime_contract"](tampered)

    def test_live_cursor_zoom_non_pinch_mutations_fail_closed(self) -> None:
        path = Path(
            "src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"
        )
        mutations = (
            ("if (!e.pinch)", "if (e.pinch)"),
            (
                "if (m_mode != MODE_LIVE || !m_monitor || !e.pinch)",
                "if (m_mode != MODE_LIVE || !m_monitor)",
            ),
        )
        for version in self.sources:
            for old, new in mutations:
                with self.subTest(version=version, old=old):
                    tampered = copy.deepcopy(self.sources)
                    source = tampered[version][path].decode("utf-8")
                    tampered[version][path] = source.replace(
                        old, new, 1
                    ).encode("utf-8")
                    with self.assertRaisesRegex(ValueError, "non-pinch no-op"):
                        module["_assert_gesture_runtime_contract"](tampered)

    def test_reload_and_registry_clear_counts_fail_closed(self) -> None:
        cases = (
            (
                Path("src/config/lua/ConfigManager.cpp"),
                "g_pTrackpadGestures->clearGestures();",
                "reload clear",
            ),
            (
                Path("src/managers/input/trackpad/TrackpadGestures.cpp"),
                "m_gestures.clear();",
                "registry clear",
            ),
        )
        for version in self.sources:
            for path, clear, error in cases:
                with self.subTest(version=version, path=path):
                    duplicated = copy.deepcopy(self.sources)
                    duplicated[version][path] += (clear + "\n").encode("utf-8")
                    with self.assertRaisesRegex(ValueError, error):
                        module["_assert_gesture_runtime_contract"](
                            duplicated
                        )

    def test_shadow_registration_order_mutation_fails_closed(self) -> None:
        path = Path("src/managers/input/trackpad/TrackpadGestures.cpp")
        remove_signature = (
            "std::expected<void, std::string> "
            "CTrackpadGestures::removeGesture"
        )
        for version in self.sources:
            emplace = next(
                fragment
                for fragment in self.requirements[version][path]
                if fragment.startswith("m_gestures.emplace_back")
            )
            with self.subTest(version=version):
                tampered = copy.deepcopy(self.sources)
                source = tampered[version][path].decode("utf-8")
                source = source.replace(emplace, "__GESTURE_EMPLACE__", 1)
                source = source.replace(remove_signature, emplace, 1)
                source = source.replace(
                    "__GESTURE_EMPLACE__", remove_signature, 1
                )
                tampered[version][path] = source.encode("utf-8")
                with self.assertRaisesRegex(ValueError, path.name):
                    module["_assert_gesture_runtime_contract"](tampered)

    def test_missing_or_extra_source_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_gesture_runtime_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(
            ValueError, "source version inventory is incomplete"
        ):
            module["_assert_gesture_runtime_contract"](missing_version)

        extra_version = copy.deepcopy(self.sources)
        extra_version["0.56.0"] = copy.deepcopy(self.sources["0.56.1"])
        with self.assertRaisesRegex(
            ValueError, "source version inventory is incomplete"
        ):
            module["_assert_gesture_runtime_contract"](extra_version)

    def test_manifest_schema_keeps_the_gesture_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("gestureSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["gestureSources"]["maxItems"] = 37
        with self.assertRaisesRegex(ValueError, "count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        open_inventory = copy.deepcopy(schema)
        open_inventory["$defs"]["gestureSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](open_inventory)

        open_definition = copy.deepcopy(schema)
        open_definition["$defs"]["gestureSource"][
            "additionalProperties"
        ] = True
        with self.assertRaisesRegex(ValueError, "definition is open"):
            module["_assert_source_manifest_schema"](open_definition)

        missing_required = copy.deepcopy(schema)
        missing_required["$defs"]["gestureSource"]["required"].pop()
        with self.assertRaisesRegex(ValueError, "required fields are stale"):
            module["_assert_source_manifest_schema"](missing_required)

        open_array = copy.deepcopy(schema)
        open_array["properties"]["gestureSources"]["uniqueItems"] = False
        with self.assertRaisesRegex(ValueError, "array is not closed"):
            module["_assert_source_manifest_schema"](open_array)

        stale_branch = copy.deepcopy(schema)
        stale_branch["$defs"]["gestureSource"]["oneOf"][0][
            "properties"
        ]["tag"]["const"] = "v0.55.1"
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](stale_branch)


class GroupBarContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_group_bar_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version, requirements in self.requirements.items()
        }

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_group_bar_contract"](self.sources)

    def test_exact_authored_option_inventory_is_covered(self) -> None:
        self.assertEqual(
            module["GROUP_BAR_OPTION_PATHS"],
            (
                "group:groupbar:enabled",
                "group:groupbar:disable_when_only",
                "group:groupbar:font_family",
                "group:groupbar:font_weight_active",
                "group:groupbar:font_weight_inactive",
                "group:groupbar:font_size",
                "group:groupbar:gradients",
                "group:groupbar:height",
                "group:groupbar:indicator_gap",
                "group:groupbar:indicator_height",
                "group:groupbar:stacked",
                "group:groupbar:priority",
                "group:groupbar:render_titles",
                "group:groupbar:scrolling",
                "group:groupbar:middle_click_close",
                "group:groupbar:rounding",
                "group:groupbar:rounding_power",
                "group:groupbar:gradient_rounding",
                "group:groupbar:gradient_rounding_power",
                "group:groupbar:round_only_edges",
                "group:groupbar:gradient_round_only_edges",
                "group:groupbar:gaps_out",
                "group:groupbar:gaps_in",
                "group:groupbar:keep_upper_gap",
                "group:groupbar:text_offset",
                "group:groupbar:text_padding",
                "group:groupbar:blur",
            ),
        )

    def test_exact_five_role_inventory_is_pinned_at_both_tags(self) -> None:
        expected = (
            Path("src/config/values/ConfigValues.cpp"),
            Path("src/desktop/view/Group.cpp"),
            Path("src/render/decorations/CHyprGroupBarDecoration.cpp"),
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
            Path("src/config/lua/types/LuaConfigFontWeight.cpp"),
        )
        self.assertEqual(module["GROUP_BAR_SOURCE_PATHS"], expected)
        self.assertEqual(tuple(self.requirements), ("0.55.0", "0.56.1"))
        for version in self.requirements:
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), expected)

    def test_normalized_registry_delta_is_exact(self) -> None:
        current = module["GROUP_BAR_REGISTRY_PATHS_0561"]
        previous = tuple(
            path
            for path in current
            if path != "group:groupbar:disable_when_only"
        )
        self.assertEqual(len(previous), 34)
        self.assertEqual(len(current), 35)
        self.assertEqual(
            tuple(path for path in current if path not in previous),
            ("group:groupbar:disable_when_only",),
        )

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment, "reviewed semantic removed", 1
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_group_bar_contract"](tampered)

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_group_bar_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_group_bar_contract"](missing_version)


class AnimationContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.requirements = module["_animation_contract_requirements"]()
        tree_path = Path("src/config/shared/animation/AnimationTree.cpp")
        self.sources = {}
        for version, requirements in self.requirements.items():
            version_sources = {}
            for path, fragments in requirements.items():
                if path != tree_path:
                    source = "\n".join(fragments) + "\n"
                    if path == Path("src/config/lua/ConfigManager.cpp"):
                        source += "void CConfigManager::postConfigReload() {\n"
                else:
                    lines = [fragments[0]]
                    for name, parent in module["ANIMATION_TREE_NODES"][version]:
                        if parent is None:
                            lines.append(f'm_animationTree.createNode("{name}");')
                        else:
                            lines.append(
                                f'm_animationTree.createNode("{name}", "{parent}");'
                            )
                    for name, enabled, speed, curve in module[
                        "ANIMATION_TREE_DEFAULTS"
                    ][version]:
                        lines.append(
                            "m_animationTree.setConfigForNode("
                            f'"{name}", {1 if enabled else 0}, {speed}, '
                            f'"{curve}");'
                        )
                    lines.extend(fragments[5:])
                    source = "\n".join(lines) + "\n"
                version_sources[path] = source.encode("utf-8")
            self.sources[version] = version_sources

    def test_reviewed_semantic_fixture_is_accepted(self) -> None:
        module["_assert_animation_contract"](self.sources)

    def test_exact_version_specific_sixteen_source_inventory_is_pinned(self) -> None:
        expected = {
            "0.55.0": (
                Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/shared/animation/AnimationTree.cpp"),
                Path("src/managers/animation/AnimationManager.hpp"),
                Path("src/managers/animation/AnimationManager.cpp"),
                Path("src/managers/animation/DesktopAnimationManager.cpp"),
                Path("src/desktop/view/Window.cpp"),
            ),
            "0.56.1": (
                Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
                Path("src/config/lua/ConfigManager.cpp"),
                Path("src/config/shared/animation/AnimationTree.cpp"),
                Path("src/animation/AnimationManager.hpp"),
                Path("src/animation/AnimationManager.cpp"),
                Path(
                    "src/desktop/view/animationControllers/"
                    "WindowAnimationController.cpp"
                ),
                Path(
                    "src/desktop/view/animationControllers/"
                    "LayerSurfaceAnimationController.cpp"
                ),
                Path("src/animation/WorkspaceAnimationController.cpp"),
                Path("src/desktop/view/Window.cpp"),
            ),
        }
        self.assertEqual(module["ANIMATION_SOURCE_PATHS"], expected)
        self.assertEqual(sum(map(len, expected.values())), 16)
        for version, paths in expected.items():
            with self.subTest(version=version):
                self.assertEqual(tuple(self.requirements[version]), paths)

    def test_every_reviewed_runtime_fragment_fails_closed(self) -> None:
        for version, requirements in self.requirements.items():
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment, "reviewed semantic removed", 1
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_animation_contract"](tampered)

    def test_tree_parent_and_builtin_reference_mutations_fail_closed(self) -> None:
        tree_path = Path("src/config/shared/animation/AnimationTree.cpp")
        parent_changed = copy.deepcopy(self.sources)
        source = parent_changed["0.56.1"][tree_path].decode("utf-8")
        parent_changed["0.56.1"][tree_path] = source.replace(
            'createNode("windowsIn", "windows")',
            'createNode("windowsIn", "global")',
            1,
        ).encode("utf-8")
        with self.assertRaisesRegex(ValueError, "leaves/inheritance"):
            module["_assert_animation_contract"](parent_changed)

        builtin_changed = copy.deepcopy(self.sources)
        source = builtin_changed["0.55.0"][tree_path].decode("utf-8")
        builtin_changed["0.55.0"][tree_path] = source.replace(
            'setConfigForNode("__internal_fadeCTM", 1, 5.f, "linear")',
            'setConfigForNode("__internal_fadeCTM", 1, 5.f, "default")',
            1,
        ).encode("utf-8")
        with self.assertRaisesRegex(ValueError, "built-in references"):
            module["_assert_animation_contract"](builtin_changed)

    def test_custom_default_and_linear_names_remain_unguarded(self) -> None:
        lua_path = Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp")
        for version in self.sources:
            with self.subTest(version=version):
                guarded = copy.deepcopy(self.sources)
                source = guarded[version][lua_path].decode("utf-8")
                guarded[version][lua_path] = source.replace(
                    "static int hlAnimation(lua_State* L) {",
                    'if (name == "default" || name == "linear") return 0;\n'
                    "static int hlAnimation(lua_State* L) {",
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "shadowing semantics"):
                    module["_assert_animation_contract"](guarded)

    def test_lua_reload_does_not_clear_persistent_curve_maps(self) -> None:
        config_path = Path("src/config/lua/ConfigManager.cpp")
        for version in self.sources:
            with self.subTest(version=version):
                clearing = copy.deepcopy(self.sources)
                source = clearing[version][config_path].decode("utf-8")
                clearing[version][config_path] = source.replace(
                    "Config::animationTree()->reset();",
                    "Config::animationTree()->reset();\n"
                    "removeAllBeziers();\nremoveAllSprings();",
                    1,
                ).encode("utf-8")
                with self.assertRaisesRegex(ValueError, "curve reset semantics"):
                    module["_assert_animation_contract"](clearing)

    def test_missing_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            next(iter(self.requirements["0.55.0"]))
        )
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_animation_contract"](missing_source)

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "source inventory is incomplete"):
            module["_assert_animation_contract"](missing_version)

    def test_manifest_schema_keeps_the_animation_inventory_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("animationSources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["animationSources"]["maxItems"] = 15
        with self.assertRaisesRegex(ValueError, "count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        open_inventory = copy.deepcopy(schema)
        open_inventory["$defs"]["animationSource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](open_inventory)


class AnimationDependencyContractTest(unittest.TestCase):
    def setUp(self) -> None:
        requirements = module["_animation_dependency_contract_requirements"]()
        self.sources = {
            version: {
                path: ("\n".join(fragments) + "\n").encode("utf-8")
                for path, fragments in requirements.items()
            }
            for version in module["ANIMATION_DEPENDENCY_SOURCES"]
        }
        self.flake_locks = {
            version: json.dumps(
                {
                    "root": "root",
                    "nodes": {
                        "root": {"inputs": {"hyprutils": "hyprutils"}},
                        "hyprutils": {
                            "locked": {
                                "owner": "hyprwm",
                                "repo": "hyprutils",
                                "rev": dependency["revision"],
                                "type": "github",
                            }
                        },
                    },
                }
            ).encode("utf-8")
            for version, dependency in module[
                "ANIMATION_DEPENDENCY_SOURCES"
            ].items()
        }

    def test_reviewed_dependency_fixture_is_accepted(self) -> None:
        module["_assert_animation_dependency_contract"](
            self.flake_locks, self.sources
        )

    def test_exact_eight_source_dependency_inventory_is_pinned(self) -> None:
        self.assertEqual(
            module["HYPRUTILS_ANIMATION_SOURCE_PATHS"],
            (
                Path("include/hyprutils/animation/AnimationManager.hpp"),
                Path("src/animation/AnimationManager.cpp"),
                Path("src/animation/AnimatedVariable.cpp"),
            ),
        )
        self.assertEqual(set(module["ANIMATION_DEPENDENCY_SOURCES"]), {
            "0.55.0", "0.56.1"
        })

    def test_flake_revision_and_root_binding_mutations_fail_closed(self) -> None:
        changed_revision = copy.deepcopy(self.flake_locks)
        changed_revision["0.55.0"] = changed_revision["0.55.0"].replace(
            b"a2dbd8a4cc51f7cbe4224732668392bb1aa79df2",
            b"0000000000000000000000000000000000000000",
        )
        with self.assertRaisesRegex(ValueError, "pinned Hyprutils revision"):
            module["_assert_animation_dependency_contract"](
                changed_revision, self.sources
            )

        changed_root = copy.deepcopy(self.flake_locks)
        changed_root["0.56.1"] = changed_root["0.56.1"].replace(
            b'"hyprutils": "hyprutils"', b'"hyprutils": "other"'
        )
        with self.assertRaisesRegex(ValueError, "dependency binding"):
            module["_assert_animation_dependency_contract"](
                changed_root, self.sources
            )

    def test_every_dependency_runtime_fragment_fails_closed(self) -> None:
        requirements = module["_animation_dependency_contract_requirements"]()
        for version in self.sources:
            for path, fragments in requirements.items():
                for index, fragment in enumerate(fragments):
                    with self.subTest(version=version, path=path, index=index):
                        tampered = copy.deepcopy(self.sources)
                        source = tampered[version][path].decode("utf-8")
                        tampered[version][path] = source.replace(
                            fragment, "reviewed dependency semantic removed", 1
                        ).encode("utf-8")
                        with self.assertRaisesRegex(ValueError, path.name):
                            module["_assert_animation_dependency_contract"](
                                self.flake_locks, tampered
                            )

    def test_missing_dependency_source_or_version_is_rejected(self) -> None:
        missing_source = copy.deepcopy(self.sources)
        missing_source["0.55.0"].pop(
            module["HYPRUTILS_ANIMATION_SOURCE_PATHS"][0]
        )
        with self.assertRaisesRegex(ValueError, "inventory is incomplete"):
            module["_assert_animation_dependency_contract"](
                self.flake_locks, missing_source
            )

        missing_version = {"0.56.1": self.sources["0.56.1"]}
        with self.assertRaisesRegex(ValueError, "inventory is incomplete"):
            module["_assert_animation_dependency_contract"](
                self.flake_locks, missing_version
            )

    def test_dependency_schema_inventory_is_required_and_closed(self) -> None:
        schema_path = (
            arguments.extractor.resolve().parents[2]
            / "interfaces/hyprland/v1/source-manifest.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        module["_assert_source_manifest_schema"](schema)

        not_required = copy.deepcopy(schema)
        not_required["required"].remove("animationDependencySources")
        with self.assertRaisesRegex(ValueError, "not required"):
            module["_assert_source_manifest_schema"](not_required)

        wrong_count = copy.deepcopy(schema)
        wrong_count["properties"]["animationDependencySources"][
            "maxItems"
        ] = 7
        with self.assertRaisesRegex(ValueError, "count is stale"):
            module["_assert_source_manifest_schema"](wrong_count)

        open_inventory = copy.deepcopy(schema)
        open_inventory["$defs"]["animationDependencySource"]["oneOf"].pop()
        with self.assertRaisesRegex(ValueError, "inventory/pins are stale"):
            module["_assert_source_manifest_schema"](open_inventory)


if __name__ == "__main__":
    unittest.main(argv=[arguments.extractor.as_posix(), *remaining])
