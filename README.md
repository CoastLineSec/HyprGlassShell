# HyprShelld

HyprShelld is building a cohesive desktop environment around Hyprland. The
project combines shell surfaces, configuration, system integration, and
essential desktop experiences so users do not have to assemble a complete
desktop from unrelated components.

HyprShelld is in early development and does not yet provide a supported complete
desktop package. The current source includes an installable development runtime,
a floating bar, its Settings application, and the supporting services.

User documentation begins in [docs/index.md](docs/index.md).

## Build checks

The current Arch Linux build checks require CMake 3.24 or newer, Ninja, a C++23
compiler, Qt 6.8 or newer, Quickshell 0.3.0, `dbus-run-session`, and systemd 257
or newer with `busctl` and `systemd-analyze`, plus the libzip development
package. The build checks also use
`desktop-file-validate` from `desktop-file-utils`.

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
