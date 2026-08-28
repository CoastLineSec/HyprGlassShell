# HyprShelld

HyprShelld is building a cohesive desktop environment around Hyprland. The
project combines shell surfaces, configuration, system integration, and
essential desktop experiences so users do not have to assemble a complete
desktop from unrelated components.

HyprShelld is in early development and does not yet provide a supported complete
desktop package. The current source includes an installable development runtime,
a floating bar, automatic shell colors, `hyprsunset`-backed Night Light, a
Settings application for the bar, common window appearance, displays, and shell
components, and the supporting services. Appearance uses a validated draft and
verified compositor reload. Display changes use an explicit compositor-takeover
step and a timed live test before they are saved.

User documentation begins in [docs/index.md](docs/index.md).

## Build checks

The current Arch Linux build checks require CMake 3.24 or newer, Ninja, a C++23
compiler, Qt 6.8 or newer, Quickshell 0.3.0, `dbus-run-session`, and systemd 257
or newer with `busctl` and `systemd-analyze`, plus the double-conversion,
libzip, libxkbcommon, and RE2 development packages. Compositor generation
checks require the pinned Hyprland 0.56.2 executable and the Lua
interpreter/compiler (`lua` and `luac`). Night Light additionally requires
`hyprsunset`, `hyprctl`, and `pgrep`; configure fails if any executable is
missing. On Arch Linux those commands are supplied by the `hyprsunset`,
`hyprland`, and `procps-ng` packages. The current command contract targets
hyprsunset 0.4.0 with Hyprland 0.56.2. The optional `geoclue` package provides
permission-aware current-location schedules, while fixed times and manual
coordinates work without it. Canonical-JSON qualification also requires
Node.js as an independent ECMAScript number-serialization oracle. The build
checks also use `desktop-file-validate` from `desktop-file-utils` and the Python
`jsonschema` package for Draft 2020-12 contract validation.

This source tree does not yet include an Arch `PKGBUILD` or another package
manager manifest. CMake verifies the source-build environment only; a package
must declare `hyprsunset`, `hyprland`, and `procps-ng` as runtime dependencies
and may offer `geoclue` as an optional dependency.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The Quickshell fixture is a development qualification tool rather than a
product shell. Run it only from an active Wayland session:

```sh
qs --no-duplicate -p tests/quickshell/qualification
```
