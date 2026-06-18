# hgs-hyprglass

Minimal Hyprland plugin used by HyprGlassShell to prove the hyprglass control/status and compositor render path.

This plugin currently registers hyprctl commands, stores descriptor payloads, reports geometry diagnostics, draws an optional debug bounds overlay, and can draw an opt-in flat compositor material. It does not render Liquid Glass yet.

## Descriptor Apply Semantics

`hgs hyprglass apply` and `hgs hyprglass apply-json` send a full descriptor snapshot, not a patch.

The plugin parses the incoming snapshot into a temporary descriptor set. If parsing succeeds, the active descriptor set is replaced atomically and the active generation advances. If parsing fails, the active descriptor set, descriptor summaries, descriptor count, active generation, and apply count are left unchanged. Rejected input only updates `lastApplyStatus` and `lastError` so status can explain the failure. `hgs hyprglass clear` explicitly clears the active descriptor set and advances the active generation.

Layer-shell surface discovery is status-only. The plugin recomputes candidate surfaces during status calls and does not store Hyprland surface pointers across calls.

## Coordinate Status

Status reports monitor geometry, layer-surface geometry, and per-descriptor coordinate analysis before any compositor-side glass/material rendering exists.

Current assumptions:

- Descriptor `geometry.logical` is treated as monitor-local logical coordinates from HyprGlassShell/QML.
- `CLayerSurface::logicalBox()` appears to report global-layout logical coordinates. For example, a top layer surface on monitor `DP-2` may report `x/y` equal to the monitor's global layout position.
- The plugin computes monitor-local logical geometry as `surfaceLogical - monitorLogicalPosition`.
- The plugin computes framebuffer geometry as `monitorLocalLogical * monitorScale`.
- A descriptor can either match the full layer surface or describe a material rect contained inside the matched layer surface. Contained material rects are coordinate-aligned when they are inside the layer-surface bounds; `delta` still reports their inset from the layer-surface bounds.

The framebuffer conversion has not been verified on fractional scale, transformed/rotated monitors, or mixed-scale layouts. Coordinate confidence remains diagnostic until those cases are tested.

## Debug Bounds Overlay

The debug overlay is disabled by default and is only for coordinate validation.

```sh
hgs hyprglass debug-overlay on
hgs hyprglass debug-overlay off
hgs hyprglass debug-overlay toggle
```

The overlay uses Hyprland's `RENDER_LAST_MOMENT` render stage and draws simple `CRectPassElement` line boxes. It does not sample the backdrop, blur, refract, or render any glass material.

The drawn boxes use monitor-local logical render-pass coordinates. Descriptor bounds are drawn from descriptor `geometry.logical`; matched surface bounds are drawn from `surface logicalBox - monitor logical position`. Transformed monitors are skipped and reported as unsupported.

## Flat Material v0.1

The flat material mode is disabled by default and is only for render-order validation.

```sh
hgs hyprglass material flat
hgs hyprglass material blur-native
hgs hyprglass material off
hgs hyprglass material status
```

Material modes use Hyprland's `RENDER_POST_WINDOWS` render stage, which is after regular windows and before top/overlay layer surfaces. The intended diagnostic question is whether that order places the compositor material behind QML shell content. The modes draw into descriptor `geometry.logical`.

Material v0.1 resolves only deterministic flat color, opacity, and uniform rounding:

- If descriptor `material.tint.color` is a valid `#RRGGBB` or `#RRGGBBAA`, its RGB channels are used as the fill color.
- If the tint color is absent or invalid, the plugin falls back to a fixed pale material color.
- Final alpha is `(opacity + tintOpacity * (1 - opacity)) * tintColorAlpha`, clamped to `0..1`.
- Rounded corners use `CRectPassElement::SRectData.round` and `roundingPower = 2.0`.
- Hyprland's rectangle pass supports one uniform radius, so v0.1 uses the minimum descriptor corner radius clamped to `min(width, height) / 2`.

`flat` draws the resolved rounded/tinted rectangle without blur. `blur-native` uses the same rect, color, alpha, and radius path, but sets `CRectPassElement::SRectData.blur = true` so Hyprland applies its existing native blur behind the translucent rect.

Hyprland native blur does not expose per-descriptor blur radius through this pass. Descriptor `material.frost` is a normalized `0..1` request. In `blur-native`, HyprGlass maps it to `CRectPassElement::SRectData.blurA`, which Hyprland passes into `blurMainFramebuffer()` and then uses as the blurred background texture alpha. This is per-surface blur blend/alpha only; effective blur kernel size and passes still come from global Hyprland `decoration:blur` configuration.

Material v0.1 does not copy the framebuffer, sample a custom backdrop texture, refract, lens, draw rim lighting, draw shadows, draw glossy highlights, or run a custom shader. Per-corner radii are not represented in this v0.1 path.

Transformed monitors are skipped and reported as unsupported. Fractional scale and mixed-scale render ordering are not yet proven.

Status exposes monitor scale and transform diagnostics for each monitor and descriptor. Monitor entries include `scaleKind`, `fractionalScale`, and `transformSupported`. Descriptor coordinate entries include the raw logical rect, computed monitor-local logical rect, computed framebuffer rect, rounded framebuffer rect, logical delta, framebuffer delta, and framebuffer rounding mode. Fractional-scale framebuffer coordinates are diagnostic until tested on a live fractional-scale monitor.

HyprGlassShell normally keeps each QML fallback visible until plugin status proves that the same descriptor is actively drawable by the compositor material path. The shell only hides the fallback for that specific descriptor when the plugin is loaded, material mode is enabled, status is fresh, the layer surface is matched, coordinates are aligned or near, and `compositorMaterial.drawable` is true. Unsupported or skipped monitors keep the QML fallback.

The shell keeps the current full descriptor snapshot locally. If the plugin appears after shell startup, reloads and loses descriptors, or reports a descriptor set missing current shell IDs, the shell reapplies the full snapshot with a conservative cooldown. If the plugin unloads or status becomes stale, the QML fallback returns automatically.

On normal HyprGlassShell shutdown, `HyprGlassService` attempts one fire-and-forget `hgs hyprglass clear` so plugin descriptors do not remain after shell restart. The cleanup does not disable material mode. If the plugin is absent or the command fails during shutdown, the failure is ignored and runtime safety falls back to status/matching: the plugin recomputes layer-shell surfaces during status and render passes, and stale descriptors with no live matching surface are reported as unmatched/non-drawable. Abrupt process death can still skip the QML destruction hook, so stale descriptors must remain non-drawable.

To force global fallback isolation during manual testing, start HyprGlassShell with:

```sh
HGS_HYPRGLASS_ISOLATE_QML_FALLBACK=1 qs -p quickshell
```

This disables the QML bar fallback fill, shadow, border, and bar blur region while leaving bar layout, content, input, and descriptor publishing intact. It is disabled by default, bypasses the per-descriptor safety checks, and is not a production material mode.

## Shell Appearance Contract

HyprGlassShell resolves user-facing appearance choices before descriptors reach this plugin. The plugin consumes only final descriptor material values: `preset`, `tint.color`, `tint.opacity`, `opacity`, and `frost`.

The shell-side model is intentionally small:

- Default mode uses neutral automatic light/dark glass.
- Optional color glass resolves a final tint from a selected shell-side color source. The first persisted source is custom color; matugen and curated presets are future shell/settings work.
- `frost` is a normalized `0..1` requested frost amount. In `blur-native`, it controls native blur blend/alpha; actual blur kernel strength still comes from global Hyprland `decoration:blur` config.

The current persisted shell settings are `hyprGlassColorGlassEnabled`, `hyprGlassColorSource`, `hyprGlassCustomColor`, and `hyprGlassFrostAmount`. Temporary dev-only shell overrides are also available for descriptor validation and are disabled by default. Override precedence is environment, then persisted settings, then built-in defaults:

```sh
HGS_HYPRGLASS_COLOR_ENABLED=1
HGS_HYPRGLASS_COLOR_SOURCE=custom
HGS_HYPRGLASS_COLOR="#88AADD"
HGS_HYPRGLASS_FROST=0.75
```

These are not the final settings API.

For lifecycle testing only, the shell can also reconcile the plugin material mode after plugin startup/reload:

```sh
HGS_HYPRGLASS_MATERIAL_MODE=off
HGS_HYPRGLASS_MATERIAL_MODE=flat
HGS_HYPRGLASS_MATERIAL_MODE=blur-native
```

This override is disabled by default. When unset, manual `hgs hyprglass material ...` commands are not overridden by the shell.

## Build

Build dependencies:

- Hyprland development headers exposed through `pkg-config`
- CMake
- C++ compiler with C++26 support
- `nlohmann/json.hpp`

From the repository root:

```sh
make -C plugins/hgs-hyprglass build
```

The plugin artifact is written to:

```text
plugins/hgs-hyprglass/build/hgs-hyprglass.so
```

To print the artifact path:

```sh
make -C plugins/hgs-hyprglass artifact
```

## Dev Artifact Workflow

Hyprland can keep a deleted plugin `.so` mapped after unload/reload when the same artifact path is reused. During development, prefer loading a uniquely named copy so status can prove which build is active.

Create a unique ignored artifact:

```sh
make -C plugins/hgs-hyprglass dev-artifact
```

The command prints a path like:

```text
plugins/hgs-hyprglass/build/hgs-hyprglass-dev-20260618T123456Z-b02bde4c.so
```

Load that exact printed path:

```sh
hyprctl plugin load "/absolute/path/printed/by/dev-artifact.so"
hgs hyprglass status | jq '.build, .capabilities'
```

Unload the exact path you loaded:

```sh
hyprctl plugin unload "/absolute/path/printed/by/dev-artifact.so"
hgs hyprglass status
```

Status includes a compact `build` object with `id`, `pluginVersion`, `gitCommit`, `buildTime`, and `buildType`, plus a `capabilities` object listing supported material modes and render stages. If git metadata is unavailable, the build still succeeds and reports `unknown`.

The whole `build/` directory is ignored by git. Hyprland plugin ABI can change with Hyprland updates, so rebuild the plugin after updating Hyprland.

## Manual Load Test

Load only when you intend to test against the live Hyprland session:

```sh
hyprctl plugin load "$(pwd)/plugins/hgs-hyprglass/build/hgs-hyprglass.so"
hyprctl -j plugin list
hgs hyprglass status
```

Unload:

```sh
hyprctl plugin unload "$(pwd)/plugins/hgs-hyprglass/build/hgs-hyprglass.so"
```
