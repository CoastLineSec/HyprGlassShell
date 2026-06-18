# HyprGlassShell

<div align="center">
  <a href="https://github.com/CoastLineSec/HyprGlassShell">
    <img src="assets/hgslogo.svg" alt="HyprGlassShell" width="180">
  </a>

### A Hyprland-first desktop shell and compositor glass experiment

Built with [Quickshell](https://quickshell.org/), [Go](https://go.dev/), and a Hyprland plugin.

</div>

HyprGlassShell is a Hyprland-first desktop shell forked from DankMaterialShell and redirected toward CoastLineSec goals. The project is currently focused on building a compositor-aware glass material pipeline for Hyprland while retaining a working shell foundation.

This is not a finished Liquid Glass implementation yet. The current milestone proves descriptor publishing, plugin communication, surface matching, coordinate diagnostics, debug overlays, and opt-in compositor material rendering.

## Current Status

HyprGlassShell is in early development. The current foundation includes:

- `hgs hyprglass` CLI commands for descriptor validation, apply, clear, status, debug overlay, and material mode control.
- A formal HyprGlass descriptor model shared between the shell, Go CLI, and Hyprland plugin.
- A repo-local `hgs-hyprglass` Hyprland plugin build workflow.
- Plugin status for absent, loaded, empty, valid, invalid, and cleared descriptor states.
- Layer-shell surface discovery and descriptor-to-surface matching.
- Coordinate diagnostics for monitor-local logical, global logical, framebuffer, scale, and transform state.
- Opt-in compositor debug bounds overlay.
- Opt-in compositor material modes:
  - `flat`: rounded/tinted diagnostic material.
  - `blur-native`: Hyprland native blur plus resolved tint/alpha.
- Shell-side appearance resolver for neutral glass, color glass, and normalized frost amount.
- Per-surface QML fallback handoff when plugin status proves compositor material is drawing that descriptor.
- Lifecycle resync when the plugin appears, reloads, or loses descriptors.
- Best-effort descriptor cleanup on normal shell shutdown.

Not implemented yet:

- Final Liquid Glass visual design.
- Custom backdrop sampling.
- Custom blur shaders.
- Refraction or lensing.
- Rim lighting, glossy highlights, shadows, saturation, vibrancy, or adaptive contrast.
- Fractional-scale proof.
- Transformed monitor rendering support.
- Production packaging for the Hyprland plugin.

## Supported Compositor

Hyprland is the supported compositor target.

Other compositor support inherited from DankMaterialShell is being removed instead of maintained as a compatibility target. The HyprGlass compositor material path depends on Hyprland plugin APIs and is not expected to work on Niri, Sway, Mango, Labwc, or other compositors.

## Repository Structure

```text
HyprGlassShell/
├── quickshell/              # QML shell interface
│   ├── Modules/             # Bars, widgets, popouts, settings, overlays
│   ├── Services/            # Shell services and IPC/status bridges
│   ├── Widgets/             # Reusable QML controls
│   └── Common/              # Shared settings, paths, theme, helpers
├── core/                    # Go backend and hgs CLI
│   ├── cmd/hgs/             # hgs command line interface
│   └── internal/hyprglass/  # HyprGlass descriptor/status model
├── plugins/
│   └── hgs-hyprglass/       # Hyprland plugin for compositor material work
├── distro/                  # Distribution packaging inherited from the fork
└── assets/                  # Desktop/service/icon assets
```

## Development Build

Build the Go CLI:

```sh
make -C core build
```

Run QML lint:

```sh
make lint-qml
```

Run the shell during development:

```sh
PATH="$PWD/core/bin:$PATH" qs -p quickshell
```

Install locally for user-level testing:

```sh
make -C core build
make install PREFIX="$HOME/.local"
systemctl --user daemon-reload
systemctl --user start hgs.service
```

## HyprGlass Plugin

Build the Hyprland plugin:

```sh
make -C plugins/hgs-hyprglass build
```

The plugin artifact is written to:

```text
plugins/hgs-hyprglass/build/hgs-hyprglass.so
```

Load it manually only when testing against the live Hyprland session:

```sh
hyprctl plugin load "$PWD/plugins/hgs-hyprglass/build/hgs-hyprglass.so"
```

Unload it:

```sh
hyprctl plugin unload "$PWD/plugins/hgs-hyprglass/build/hgs-hyprglass.so"
```

Detailed plugin notes live in [plugins/hgs-hyprglass/README.md](plugins/hgs-hyprglass/README.md).

## HyprGlass CLI

The current development command surface is:

```sh
hgs hyprglass status
hgs hyprglass validate <descriptor.json>
hgs hyprglass apply <descriptor.json>
hgs hyprglass apply-json <descriptor-json>
hgs hyprglass clear

hgs hyprglass debug-overlay on
hgs hyprglass debug-overlay off
hgs hyprglass debug-overlay toggle
hgs hyprglass debug-overlay status

hgs hyprglass material off
hgs hyprglass material flat
hgs hyprglass material blur-native
hgs hyprglass material status
```

The plugin exposes matching Hyprland commands:

```sh
hyprctl -j hyprglass-status
hyprctl hyprglass-apply-json '<descriptor-json>'
hyprctl hyprglass-clear
hyprctl hyprglass-debug-overlay on|off|toggle|status
hyprctl hyprglass-material off|flat|blur-native|status
```

## Shell Appearance Contract

The shell resolves appearance settings before sending descriptors to the plugin. The plugin receives only final material values:

- material preset
- tint color
- tint opacity
- opacity
- frost amount

The current user-facing model is intentionally small:

- Default glass: automatic neutral light/dark tint.
- Color Glass: optional resolved color tint, currently backed by custom color settings.
- Frost Amount: one normalized `0.0` to `1.0` requested frost value.

For `blur-native`, the frost amount maps to Hyprland rectangle-pass blur alpha. The actual blur kernel size and passes still come from global Hyprland `decoration:blur` settings.

## Development Environment Overrides

These are temporary dev/test controls, disabled by default:

```sh
HGS_HYPRGLASS_ISOLATE_QML_FALLBACK=1
HGS_HYPRGLASS_MATERIAL_MODE=flat
HGS_HYPRGLASS_MATERIAL_MODE=blur-native
HGS_HYPRGLASS_COLOR_ENABLED=1
HGS_HYPRGLASS_COLOR_SOURCE=custom
HGS_HYPRGLASS_COLOR="#88AADD"
HGS_HYPRGLASS_FROST=0.75
```

`HGS_HYPRGLASS_ISOLATE_QML_FALLBACK=1` is for manual compositor material screenshots only. It bypasses the normal per-descriptor fallback safety checks.

## Validation

Current checkpoint validation:

```sh
cd core
go test ./internal/hyprglass ./cmd/hgs

cd ..
make -C core build
make -C plugins/hgs-hyprglass build
make lint-qml
```

## Roadmap

Near-term work should stay focused on foundation before final visual tuning:

1. Keep descriptor/status/plugin lifecycle reliable.
2. Prove fractional-scale and mixed-monitor coordinate behavior.
3. Keep transformed monitors safely skipped until transform handling is designed.
4. Improve compositor material only after render order and fallback handoff remain stable.
5. Add real glass effects incrementally, with status/debug output for every backend capability.

## Credits

HyprGlassShell is forked from DankMaterialShell and keeps that inheritance visible while the project is redirected toward Hyprland-first compositor material work.

- [DankMaterialShell](https://github.com/AvengeMedia/DankMaterialShell) - upstream project foundation.
- [Quickshell](https://quickshell.org/) - QML shell framework.
- [Hyprland](https://hyprland.org/) - supported compositor target and plugin API.
- [CoastLineSec](https://github.com/CoastLineSec) - HyprGlassShell direction and downstream development.

## License

MIT License. See [LICENSE](LICENSE) for details.
