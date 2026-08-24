#!/usr/bin/env python3
"""Validate the Hyprland Settings UI coverage contract against its pinned catalog."""

from __future__ import annotations

from collections import Counter
import json
from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CATALOG_PATH = PROJECT_ROOT / "data/hyprland/config-catalog-v2.json"
COVERAGE_PATH = PROJECT_ROOT / "data/hyprland/settings-ui-coverage-v1.json"

EXPECTED_WIKI_ADDITION_PATHS = {
    "binds:drag_center_window",
    "cursor:warp_on_monitor_change",
    "decoration:blur:acrylic:aberration",
    "decoration:blur:acrylic:bulb",
    "decoration:blur:acrylic:clarity",
    "decoration:blur:acrylic:refraction",
    "decoration:blur:acrylic:tint",
    "decoration:blur:aurora:color1",
    "decoration:blur:aurora:color2",
    "decoration:blur:aurora:intensity",
    "decoration:blur:aurora:speed",
    "decoration:blur:drops:speed",
    "decoration:blur:fluid_jar:color",
    "decoration:blur:fluid_jar:distortion",
    "decoration:blur:fluid_jar:fill_amount",
    "decoration:blur:fluid_jar:mass",
    "decoration:blur:fluid_jar:precision",
    "decoration:blur:fluid_jar:speed",
    "decoration:blur:fluid_jar:turbulence",
    "decoration:blur:glass:refraction",
    "decoration:blur:glass:roughness",
    "decoration:blur:glass:size",
    "decoration:blur:haze:intensity",
    "decoration:blur:haze:iridescence",
    "decoration:blur:heat_shimmer:speed",
    "decoration:blur:ripple:duration",
    "decoration:blur:ripple:radius",
    "decoration:blur:ripple:strength",
    "decoration:blur:ripple:width",
    "decoration:blur:variant",
    "decoration:blur:water:damping",
    "decoration:blur:water:duration",
    "decoration:blur:water:radius",
    "decoration:blur:water:speed",
    "decoration:blur:water:strength",
    "decoration:wobble:damping",
    "decoration:wobble:enabled",
    "decoration:wobble:intensity",
    "decoration:wobble:mass",
    "decoration:wobble:mesh",
    "decoration:wobble:stiffness",
    "decoration:wobble:value_epsilon",
    "decoration:wobble:velocity_epsilon",
    "misc:bell_sound",
    "misc:float_force_onscreen",
    "misc:new_float_force_onscreen",
    "render:not_shown_fifo_lock",
}

EXPECTED_EXCLUDED_CAPABILITIES = {
    "autostart",
    "arbitrary-exec",
    "arbitrary-lua",
    "plugins",
}

EXPECTED_COMPLEX_LIMITATIONS = {
    "curves.structure",
    "environment.scope.uwsm",
    "monitors.disconnected-selectors",
}

REQUIRED_UI_FIELDS = ("category", "page", "control", "status")


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


class HyprlandSettingsCoverageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.catalog = load_json(CATALOG_PATH)
        cls.coverage = load_json(COVERAGE_PATH)
        cls.options = cls.catalog["options"]
        cls.surfaces = cls.catalog["complexSurfaces"]

    def test_pinned_catalog_identity_and_declared_totals(self) -> None:
        self.assertEqual(self.catalog["contractVersion"], 2)
        self.assertEqual(self.catalog["hyprland"]["reviewedVersion"], "0.56.2")
        self.assertEqual(len(self.options), 353)
        self.assertEqual(len(self.surfaces), 12)

        option_ids = [option["id"] for option in self.options]
        surface_ids = [surface["id"] for surface in self.surfaces]
        self.assertEqual(len(option_ids), len(set(option_ids)), "catalog scalar IDs duplicate")
        self.assertEqual(len(surface_ids), len(set(surface_ids)), "catalog surface IDs duplicate")

        self.assertEqual(self.coverage["formatVersion"], 1)
        self.assertEqual(self.coverage["review"]["targetHyprland"], "0.56.2")
        self.assertEqual(self.coverage["review"]["catalogScalarCount"], 353)
        self.assertEqual(self.coverage["review"]["catalogComplexSurfaceCount"], 12)
        self.assertEqual(
            set(self.coverage["review"]["allowedStatuses"]),
            {"implemented", "intentionally-unsupported", "version-gated"},
        )
        self.assertEqual(
            self.coverage["totals"],
            {
                "catalogScalars": 353,
                "implementedScalars": 349,
                "intentionallyUnsupportedScalars": 4,
                "catalogComplexSurfaces": 12,
                "implementedComplexSurfaces": 12,
                "currentWikiVersionGatedScalars": 47,
                "excludedConfigBearingCapabilities": 4,
            },
        )

    def test_every_catalog_scalar_has_exactly_one_ui_mapping(self) -> None:
        groups = self.coverage["scalarCoverageGroups"]
        catalog_by_id = {option["id"]: option for option in self.options}
        mapped_ids = [option_id for group in groups for option_id in group["ids"]]
        counts = Counter(mapped_ids)

        self.assertEqual(len(mapped_ids), 353)
        self.assertEqual(
            {option_id for option_id, count in counts.items() if count > 1},
            set(),
            "scalar IDs appear in more than one coverage group",
        )
        self.assertEqual(set(mapped_ids), set(catalog_by_id))

        category_by_module = {}
        category_by_id = {}
        for category in self.coverage["ui"]["categoryMappings"]:
            self.assertNotIn(category["id"], category_by_id)
            category_by_id[category["id"]] = category
            for module in category["modules"]:
                self.assertNotIn(module, category_by_module)
                category_by_module[module] = category

        mapped_statuses = Counter()
        for index, group in enumerate(groups):
            with self.subTest(group=index, module=group.get("module"), control=group.get("control")):
                for field in (*REQUIRED_UI_FIELDS, "categoryId", "module", "route", "controlComponent"):
                    self.assertTrue(str(group.get(field, "")).strip(), f"missing {field}")
                self.assertTrue(group["ids"], "empty scalar coverage group")
                self.assertIn(group["status"], {"implemented", "intentionally-unsupported"})

                category = category_by_module.get(group["module"])
                self.assertIsNotNone(category, "module has no UI category")
                self.assertEqual(group["categoryId"], category["id"])
                self.assertEqual(group["category"], category["title"])
                self.assertEqual(group["route"], category["route"])

                for option_id in group["ids"]:
                    option = catalog_by_id[option_id]
                    self.assertEqual(group["module"], option["module"])
                    self.assertEqual(group["control"], option["control"])
                    expected_status = "implemented" if option["writable"] else "intentionally-unsupported"
                    self.assertEqual(group["status"], expected_status)
                    mapped_statuses[expected_status] += 1

                if group["status"] == "intentionally-unsupported":
                    self.assertTrue(str(group.get("reason", "")).strip())

        self.assertEqual(mapped_statuses, Counter({"implemented": 349, "intentionally-unsupported": 4}))
        self.assertEqual(
            {
                option_id
                for group in groups
                if group["status"] == "intentionally-unsupported"
                for option_id in group["ids"]
            },
            {
                "hyprland.input.scroll_points",
                "hyprland.input.tablet.output",
                "hyprland.input.touchdevice.output",
                "hyprland.scrolling.explicit_column_widths",
            },
        )

    def test_every_complex_surface_has_exactly_one_implemented_page(self) -> None:
        expected_ids = {surface["id"] for surface in self.surfaces}
        records = self.coverage["complexSurfaceCoverage"]
        record_ids = [record["id"] for record in records]

        self.assertEqual(len(records), 12)
        self.assertEqual(len(record_ids), len(set(record_ids)), "complex surface IDs duplicate")
        self.assertEqual(set(record_ids), expected_ids)

        limitation_ids = set()
        for record in records:
            with self.subTest(surface=record["id"]):
                for field in (*REQUIRED_UI_FIELDS, "categoryId", "route", "implementation"):
                    self.assertTrue(str(record.get(field, "")).strip(), f"missing {field}")
                self.assertEqual(record["status"], "implemented")
                self.assertNotEqual(record["page"], "none")
                self.assertNotEqual(record["control"], "none")
                for limitation in record.get("limitations", []):
                    self.assertIn(limitation["status"], {"intentionally-unsupported", "version-gated"})
                    self.assertTrue(str(limitation.get("reason", "")).strip())
                    self.assertNotIn(limitation["id"], limitation_ids)
                    limitation_ids.add(limitation["id"])

        self.assertEqual(limitation_ids, EXPECTED_COMPLEX_LIMITATIONS)

    def test_latest_wiki_version_gates_are_exact_and_actionable(self) -> None:
        records = self.coverage["currentWikiAdditions"]
        paths = [record["path"] for record in records]
        ids = [record["id"] for record in records]
        catalog_paths = {option["path"] for option in self.options}
        catalog_ids = {option["id"] for option in self.options}

        self.assertEqual(len(records), 47)
        self.assertEqual(len(paths), len(set(paths)), "latest-wiki paths duplicate")
        self.assertEqual(len(ids), len(set(ids)), "latest-wiki IDs duplicate")
        self.assertEqual(set(paths), EXPECTED_WIKI_ADDITION_PATHS)
        self.assertTrue(set(paths).isdisjoint(catalog_paths))
        self.assertTrue(set(ids).isdisjoint(catalog_ids))

        for record in records:
            with self.subTest(path=record["path"]):
                for field in (*REQUIRED_UI_FIELDS, "categoryId", "route", "reason", "documentation", "candidateControl"):
                    self.assertTrue(str(record.get(field, "")).strip(), f"missing {field}")
                self.assertEqual(record["id"], "hyprland." + record["path"].replace(":", "."))
                self.assertEqual(record["luaPath"], record["path"].split(":"))
                self.assertEqual(record["status"], "version-gated")
                self.assertEqual(record["control"], "none (version-gated)")
                self.assertTrue(record["documentation"].startswith("https://wiki.hypr.land/"))

    def test_excluded_config_bearing_capabilities_are_explicit(self) -> None:
        records = self.coverage["excludedConfigBearingCapabilities"]
        ids = [record["id"] for record in records]

        self.assertEqual(len(records), 4)
        self.assertEqual(len(ids), len(set(ids)), "excluded capability IDs duplicate")
        self.assertEqual(set(ids), EXPECTED_EXCLUDED_CAPABILITIES)
        for record in records:
            with self.subTest(capability=record["id"]):
                for field in (*REQUIRED_UI_FIELDS, "reason", "documentation"):
                    self.assertTrue(str(record.get(field, "")).strip(), f"missing {field}")
                self.assertEqual(record["status"], "intentionally-unsupported")
                self.assertEqual(record["page"], "none")
                self.assertEqual(record["control"], "none")
                self.assertTrue(record["documentation"].startswith("https://wiki.hypr.land/"))

    def test_every_nonimplemented_record_explains_why(self) -> None:
        records = list(self.coverage["scalarCoverageGroups"])
        records += list(self.coverage["currentWikiAdditions"])
        records += list(self.coverage["currentWikiAndRegistryNotes"])
        records += list(self.coverage["excludedConfigBearingCapabilities"])
        for surface in self.coverage["complexSurfaceCoverage"]:
            records.extend(surface.get("limitations", []))

        for record in records:
            if record.get("status") in {"intentionally-unsupported", "version-gated"}:
                with self.subTest(record=record.get("id", record.get("path", "unknown"))):
                    self.assertTrue(str(record.get("reason", "")).strip())


if __name__ == "__main__":
    unittest.main()
