# HyprShelld

HyprShelld is building a cohesive desktop environment around Hyprland. The
project combines shell surfaces, configuration, system integration, and
essential desktop experiences so users do not have to assemble a complete
desktop from unrelated components.

HyprShelld is in early development and does not yet provide an installable
desktop. The first development milestone is qualifying the visual foundation
and then delivering one complete, configurable vertical slice.

User documentation begins in [docs/index.md](docs/index.md).

## Build checks

The current scaffold requires CMake, Ninja, Qt 6.8 or newer, and Quickshell
0.3.0.

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
