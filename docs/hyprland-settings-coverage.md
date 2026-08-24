# Hyprland Settings coverage

This document is the requirement-level coverage contract for the Settings UI's
managed Hyprland Lua configuration. The target authority is Hyprland **0.56.2**,
not the moving latest-git interface. Coverage was reconciled on 2026-08-23
against:

- the pinned [`config-catalog-v2.json`](../data/hyprland/config-catalog-v2.json),
  reviewed from official tag `v0.56.2` at commit
  [`efb5099`](https://github.com/hyprwm/Hyprland/commit/efb50993780079460b0cbed1363e2166a2de1d9f);
- the official [Configuring index](https://wiki.hypr.land/Configuring/) at wiki
  commit [`767416d`](https://github.com/hyprwm/hyprland-wiki/commit/767416d8ad85a3795cdcf02b70945389eeef450b);
- the official current configuration registry at Hyprland commit
  [`951ab55`](https://github.com/hyprwm/Hyprland/blob/951ab550e7ea050e4b871f99faf87d99c601c44a/src/config/values/ConfigValues.cpp).

The per-ID source of truth is
[`settings-ui-coverage-v1.json`](../data/hyprland/settings-ui-coverage-v1.json).
It names every scalar and complex surface exactly once and records its UI
category, page, route, control, status, and reason where applicable. The tables
below are its human-readable roll-up.

## Result

| Requirement set | Total | Implemented/editable | Projected read-only | Version-gated | Absent by policy |
|---|---:|---:|---:|---:|---:|
| Pinned 0.56.2 scalar IDs | 353 | 349 | 4 | 0 | 0 |
| Pinned 0.56.2 complex surfaces | 12 | 12 | 0 | 0 | 0 |
| New scalar paths in current official wiki | 47 | 0 | 0 | 47 | 0 |
| Additional config-bearing capabilities | 4 | 0 | 0 | 0 | 4 |

The four pinned read-only scalars are **not absent UI**. They are projected by
[`HyprlandCatalogPage.qml`](../src/settings/HyprlandCatalogPage.qml) through
[`HyprlandOptionRow.qml`](../src/settings/HyprlandOptionRow.qml), show their
current value and explanation, and are deliberately disabled for editing. By
contrast, the four policy exclusions near the end of this document intentionally
have no page or control.

Status meanings:

- `implemented`: the pinned record has a typed UI projection; writable scalar
  records participate in the catalog draft/save flow, and complex records use
  their dedicated draft/save transaction.
- `intentionally-unsupported`: the item is either visible but read-only (the
  four pinned scalars) or deliberately absent (the four executable/native-code
  capabilities). Every such record carries a reason.
- `version-gated`: the official current wiki/source knows the setting, but the
  qualified 0.56.2 contract does not. It does not become writable merely because
  latest-git documents it.

## Scalar UI matrix

All scalar routes use
[`HyprlandCatalogPage.qml`](../src/settings/HyprlandCatalogPage.qml), and every
row is rendered by the typed
[`HyprlandOptionRow.qml`](../src/settings/HyprlandOptionRow.qml). The overview
entry point is
[`HyprlandOverviewPage.qml`](../src/settings/HyprlandOverviewPage.qml).

| UI category | Route | Catalog modules | Total | Editable | Read-only |
|---|---|---|---:|---:|---:|
| Appearance & Motion | `hyprland/catalog/appearance` | `animations`, `cursor`, `decoration` | 67 | 67 | 0 |
| Input & Gestures | `hyprland/catalog/input` | `gestures`, `input` | 76 | 73 | 3 |
| Windows & Layouts | `hyprland/catalog/windows` | `dwindle`, `general`, `group`, `layout`, `master`, `scrolling` | 106 | 105 | 1 |
| Shortcuts & Submaps | `hyprland/catalog/shortcuts` | `binds` | 14 | 14 | 0 |
| System & Compatibility | `hyprland/catalog/system` | `debug`, `experimental`, `misc`, `opengl`, `quirks`, `render`, `xwayland` | 87 | 87 | 0 |
| Session & Security | `hyprland/catalog/session` | `ecosystem` | 3 | 3 | 0 |
| **Total** |  |  | **353** | **349** | **4** |

The per-module/control reconciliation is:

| Module | Count | Typed control counts | Status |
|---|---:|---|---|
| `animations` | 2 | toggle 2 | implemented |
| `binds` | 14 | select 2, spinBox 2, toggle 10 | implemented |
| `cursor` | 22 | select 5, slider 2, spinBox 2, text 1, toggle 12 | implemented |
| `debug` | 22 | select 3, spinBox 2, toggle 17 | implemented |
| `decoration` | 43 | gradient 4, slider 15, spinBox 8, text 1, toggle 14, vector2 1 | implemented |
| `dwindle` | 11 | select 2, slider 3, toggle 6 | implemented |
| `ecosystem` | 3 | toggle 3 | implemented |
| `experimental` | 1 | toggle 1 | implemented |
| `general` | 23 | gradient 4, select 2, spinBox 5, text 4, toggle 8 | implemented |
| `gestures` | 14 | slider 1, spinBox 4, toggle 9 | implemented |
| `group` | 47 | color 4, gradient 8, select 1, slider 2, spinBox 13, text 1, toggle 18 | implemented |
| `input` | 62 | select 11, slider 6, spinBox 9, text 9, toggle 23, vector2 4 | 59 implemented, 3 projected read-only |
| `layout` | 2 | slider 1, vector2 1 | implemented |
| `master` | 14 | select 4, slider 2, spinBox 1, toggle 7 | implemented |
| `misc` | 40 | color 2, select 4, spinBox 4, text 4, toggle 26 | implemented |
| `opengl` | 1 | toggle 1 | implemented |
| `quirks` | 2 | select 1, toggle 1 | implemented |
| `render` | 17 | select 9, toggle 8 | implemented |
| `scrolling` | 9 | select 2, slider 2, text 1, toggle 4 | 8 implemented, 1 projected read-only |
| `xwayland` | 4 | toggle 4 | implemented |
| **Total** | **353** | color 6, gradient 16, select 46, slider 34, spinBox 50, text 21, toggle 174, vector2 6 | **349 implemented, 4 projected read-only** |

### The four projected read-only scalars

| Exact ID | UI | Reason editing is intentionally unsupported |
|---|---|---|
| `hyprland.input.scroll_points` | Input & Gestures / `HyprlandCatalogPage.qml` / text row | Its custom-acceleration point sequence is an opaque mini-language that the closed scalar schema cannot parse and round-trip safely. The existing value is preserved and shown. |
| `hyprland.input.tablet.output` | Input & Gestures / `HyprlandCatalogPage.qml` / text row | Its meaning depends on runtime output identity and special empty/current semantics not represented by the scalar contract. The existing value is preserved and shown. |
| `hyprland.input.touchdevice.output` | Input & Gestures / `HyprlandCatalogPage.qml` / text row | Its meaning depends on runtime output identity plus `[[Auto]]`/empty auto-detection semantics not represented by the scalar contract. The existing value is preserved and shown. |
| `hyprland.scrolling.explicit_column_widths` | Windows & Layouts / `HyprlandCatalogPage.qml` / text row | It is a comma-separated numeric mini-language also consumed by `+conf`/`-conf` layout messages. No closed typed-list renderer is qualified, so the value is preserved and shown. |

## Complex-surface UI matrix

Every complex surface declared by the pinned catalog has a dedicated typed UI.
There are no missing complex surfaces.

| Surface | UI category / route | Page | Typed control/editor | Status |
|---|---|---|---|---|
| `animations` | Appearance & Motion / `appearance` | [`AppearancePage.qml`](../src/settings/AppearancePage.qml) | [`AnimationCollectionsSummary.qml`](../src/settings/AnimationCollectionsSummary.qml) + [`AnimationRuleEditor.qml`](../src/settings/AnimationRuleEditor.qml) | implemented |
| `curves` | Appearance & Motion / `appearance` | [`AppearancePage.qml`](../src/settings/AppearancePage.qml) | [`AnimationCollectionsSummary.qml`](../src/settings/AnimationCollectionsSummary.qml) + [`AnimationCurveEditor.qml`](../src/settings/AnimationCurveEditor.qml) | implemented |
| `bindings` | Shortcuts & Submaps / `hyprland/bindings` | [`BindingsPage.qml`](../src/settings/BindingsPage.qml) | [`BindingEditor.qml`](../src/settings/BindingEditor.qml) | implemented |
| `submaps` | Shortcuts & Submaps / `hyprland/bindings` | [`BindingsPage.qml`](../src/settings/BindingsPage.qml) | typed ordered submap cards in the page | implemented |
| `devices` | Per-device Input / `hyprland/devices` | [`InputDevicesPage.qml`](../src/settings/InputDevicesPage.qml) | [`InputDeviceEditor.qml`](../src/settings/InputDeviceEditor.qml) + [`InputDeviceOverrideRow.qml`](../src/settings/InputDeviceOverrideRow.qml) + [`InputDevicePreview.qml`](../src/settings/InputDevicePreview.qml) | implemented |
| `environment` | Environment / `hyprland/environment` | [`EnvironmentVariablesPage.qml`](../src/settings/EnvironmentVariablesPage.qml) | typed `id`/`name`/`value`/`scope` collection editor | implemented, with UWSM limitation below |
| `gestures` | Input & Gestures / `input` | [`InputPage.qml`](../src/settings/InputPage.qml) | [`GestureSummaryList.qml`](../src/settings/GestureSummaryList.qml) + [`GestureEditor.qml`](../src/settings/GestureEditor.qml) | implemented |
| `layerRules` | Rules / `rules` | [`RulesPage.qml`](../src/settings/RulesPage.qml) | [`RuleSummaryList.qml`](../src/settings/RuleSummaryList.qml) + typed layer-rule editor | implemented |
| `windowRules` | Rules / `rules` | [`RulesPage.qml`](../src/settings/RulesPage.qml) | [`RuleSummaryList.qml`](../src/settings/RuleSummaryList.qml) + typed window-rule editor | implemented |
| `monitors` | Displays / `displays` | [`DisplaysPage.qml`](../src/settings/DisplaysPage.qml) | [`DisplayTopologyPreview.qml`](../src/settings/DisplayTopologyPreview.qml) + [`DisplaySettingsCard.qml`](../src/settings/DisplaySettingsCard.qml) | implemented |
| `permissions` | Permissions / `hyprland/permissions` | [`PermissionsPage.qml`](../src/settings/PermissionsPage.qml) | typed `id`/`binary`/`type`/`mode` collection editor | implemented |
| `workspaceRules` | Workspaces / `workspaces` | [`WorkspacesPage.qml`](../src/settings/WorkspacesPage.qml) | [`WorkspaceRuleSummaryList.qml`](../src/settings/WorkspaceRuleSummaryList.qml) + [`WorkspaceRuleEditor.qml`](../src/settings/WorkspaceRuleEditor.qml) | implemented |

The environment surface preserves and edits exact-v1 `uwsm` records, but saving
such a record remains blocked (`environment.scope.uwsm`,
`intentionally-unsupported`) until there is a verified UWSM publisher. The
current renderer deliberately refuses to mis-emit UWSM ownership as `hl.env`.
Native Hyprland-scope environment records are implemented.

Two other complex editors have explicit, preserved limitations:

- Curves expose every parameter and collection ordering operation for existing
  Bezier and spring records. Adding, removing, renaming, or changing a curve
  type remains read-only because those identity-graph changes are
  restart-classified and the current service has no qualified activation path
  for a structurally different collection. Existing structure is preserved
  exactly.
- Displays exposes the complete monitor record for authenticated connected
  outputs. Description-only and currently disconnected selectors are preserved
  but not editable because the guarded preview transaction cannot safely test
  an offline identity.

## Current official wiki additions beyond 0.56.2

The official latest-git [Variables page](https://wiki.hypr.land/Configuring/Basics/Variables/)
and the current official registry agree on the following **47 scalar paths**
that are absent from the pinned 0.56.2 catalog. Their UI status is
`version-gated`, their current control is `none (version-gated)`, and their
candidate typed control is recorded per item in the machine matrix.

For every exact path below, the exact matrix ID is `hyprland.` followed by the
path with `:` replaced by `.`. For example,
`decoration:blur:variant` is ID `hyprland.decoration.blur.variant`. Both forms
are explicit in `settings-ui-coverage-v1.json`.

| Namespace | Count | Exact paths |
|---|---:|---|
| binds | 1 | `binds:drag_center_window` |
| cursor | 1 | `cursor:warp_on_monitor_change` |
| blur acrylic | 5 | `decoration:blur:acrylic:aberration`, `decoration:blur:acrylic:bulb`, `decoration:blur:acrylic:clarity`, `decoration:blur:acrylic:refraction`, `decoration:blur:acrylic:tint` |
| blur aurora | 4 | `decoration:blur:aurora:color1`, `decoration:blur:aurora:color2`, `decoration:blur:aurora:intensity`, `decoration:blur:aurora:speed` |
| blur drops | 1 | `decoration:blur:drops:speed` |
| blur fluid jar | 7 | `decoration:blur:fluid_jar:color`, `decoration:blur:fluid_jar:distortion`, `decoration:blur:fluid_jar:fill_amount`, `decoration:blur:fluid_jar:mass`, `decoration:blur:fluid_jar:precision`, `decoration:blur:fluid_jar:speed`, `decoration:blur:fluid_jar:turbulence` |
| blur glass | 3 | `decoration:blur:glass:refraction`, `decoration:blur:glass:roughness`, `decoration:blur:glass:size` |
| blur haze | 2 | `decoration:blur:haze:intensity`, `decoration:blur:haze:iridescence` |
| blur heat shimmer | 1 | `decoration:blur:heat_shimmer:speed` |
| blur ripple | 4 | `decoration:blur:ripple:duration`, `decoration:blur:ripple:radius`, `decoration:blur:ripple:strength`, `decoration:blur:ripple:width` |
| blur selector | 1 | `decoration:blur:variant` |
| blur water | 5 | `decoration:blur:water:damping`, `decoration:blur:water:duration`, `decoration:blur:water:radius`, `decoration:blur:water:speed`, `decoration:blur:water:strength` |
| wobble | 8 | `decoration:wobble:damping`, `decoration:wobble:enabled`, `decoration:wobble:intensity`, `decoration:wobble:mass`, `decoration:wobble:mesh`, `decoration:wobble:stiffness`, `decoration:wobble:value_epsilon`, `decoration:wobble:velocity_epsilon` |
| misc | 3 | `misc:bell_sound`, `misc:float_force_onscreen`, `misc:new_float_force_onscreen` |
| render | 1 | `render:not_shown_fifo_lock` |
| **Total** | **47** |  |

These are version-gated because a newer-minor setting needs its exact registry
type, bounds/choices, Lua mapping, renderer behavior, and migration policy
qualified together. Silently treating a latest-git path as compatible with
0.56.2 would break that authority boundary.

### Wiki/registry reconciliation notes

- `debug:invalidate_buffers` exists in the reviewed current official registry
  but not yet on the reviewed latest-git Variables page. It is version-gated,
  separately recorded, and deliberately excluded from the count of 47
  **wiki-documented** additions.
- `debug:fifo_pending_workaround` exists in the pinned 0.56.2 catalog and the
  reviewed latest-git Variables page, but no longer exists in the reviewed
  current registry. It remains implemented for the qualified 0.56.2 target.
- `master:center_ignores_reserved` exists in both the pinned catalog and current
  official registry even though the latest-git
  [Master Layout page](https://wiki.hypr.land/Configuring/Layouts/Master-Layout/)
  currently omits it. It remains implemented for 0.56.2.
- Registry keys `input:touchpad:tap-to-click` and
  `input:touchpad:tap-and-drag` map to Lua keys `tap_to_click` and
  `tap_and_drag`; the spelling difference does not create new settings.
- Registry category `input-capture` maps to Lua table key `input_capture`; both
  pinned scalar records are already covered.

## Other Configuring pages and ownership

The official index was also checked for config-bearing material outside the
central scalar table:

- [Monitors](https://wiki.hypr.land/Configuring/Basics/Monitors/),
  [Binds](https://wiki.hypr.land/Configuring/Basics/Binds/),
  [Window Rules](https://wiki.hypr.land/Configuring/Basics/Window-Rules/), and
  [Workspace Rules](https://wiki.hypr.land/Configuring/Basics/Workspace-Rules/)
  map to the implemented complex surfaces above.
- [Animations](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Animations/),
  [Devices](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Devices/),
  [Environment variables](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Environment-variables/),
  [Gestures](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Gestures/), and
  [Permissions](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Permissions/)
  likewise map to implemented typed surfaces.
- Layout material for
  [Dwindle](https://wiki.hypr.land/Configuring/Layouts/Dwindle-Layout/),
  [Master](https://wiki.hypr.land/Configuring/Layouts/Master-Layout/),
  [Monocle](https://wiki.hypr.land/Configuring/Layouts/Monocle-Layout/), and
  [Scrolling](https://wiki.hypr.land/Configuring/Layouts/Scrolling-Layout/)
  is covered by the pinned `general`, `layout`, `dwindle`, `master`, and
  `scrolling` scalar modules. Custom Lua layouts are executable code and follow
  the arbitrary-Lua exclusion below.
- [XWayland](https://wiki.hypr.land/Configuring/Advanced-and-Cool/XWayland/),
  tearing, performance, multi-GPU, virtual-GPU, and uncommon-tips guidance is
  represented where declarative by the scalar, monitor, environment, and rule
  surfaces. Notifications and `hyprctl` are runtime/diagnostic interfaces, not
  additional persistent configuration schemas.

## Intentionally absent config-bearing capabilities

These are not accidental omissions and are not counted among the 353 scalars or
12 catalog complex surfaces.

| Capability | UI page/control | Status | Reason |
|---|---|---|---|
| [Autostart](https://wiki.hypr.land/Configuring/Basics/Autostart/) | none / none | intentionally-unsupported | `hyprland.start` launches arbitrary processes. Shell commands do not provide closed typing, idempotence, provenance, safe preview, or transactional rollback; managed user services or reviewed components are the supported ownership boundary. |
| [Arbitrary exec and command-bearing dispatch](https://wiki.hypr.land/Configuring/Basics/Dispatchers/) | none / none | intentionally-unsupported | Raw `hl.exec_cmd`, `hl.exec_raw`, and unrestricted command actions execute code and mutate state outside the compositor transaction. Settings exposes only the closed reviewed binding/action and typed-rule catalogs. |
| [Arbitrary Lua / expanding functionality](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Expanding-functionality/) | none / none | intentionally-unsupported | Functions, callbacks, timers, `require`d modules, runtime-generated config, and custom Lua layouts are programs, not declarative desired state; Settings cannot safely diff, validate, render, or roll them back. |
| [Native plugins](https://wiki.hypr.land/Plugins/Using-Plugins/) | none / none | intentionally-unsupported | Plugins are native shared objects loaded into the compositor process with broad internal access. Installation trust, ABI/version pinning, build commands, lifecycle, and plugin-defined namespaces are outside the closed 0.56.2 authority. |

## Validation

Run the read-only focused validator from the integration root:

```sh
python3 tests/hyprland_settings_coverage_test.py -v
```

[`hyprland_settings_coverage_test.py`](../tests/hyprland_settings_coverage_test.py)
fails if a catalog scalar or complex surface is missing or duplicated, if a
catalog control/module/category/status mapping is wrong, if any declared total
changes, if the exact 47-path current-wiki set changes, or if an unsupported or
version-gated record lacks a concrete reason.
