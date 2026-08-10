# Hyprland configuration contract v1

This directory defines the strict data boundary between HyprShelld Settings and
the compositor configuration authority. Lua is a generated artifact and is not
accepted anywhere in the desired-state document.

The first fully reviewed authority is tagged Hyprland `0.56.1`. The catalog is
usable across the `0.56.x` minor after runtime capability intersection:

- `0.56.1` is an exact reviewed match.
- Other `0.56.x` patches are supported-minor matches and must be intersected
  with the live compositor's advertised scalar options.
- `0.55.x` is migration input, not writable v1 authority.
- A newer minor is preserved read-only until its catalog is reviewed.
- An unknown major is unsupported.

`config.schema.json` describes the typed desired state. Every complex surface is
an ordered array with a stable record ID. Empty arrays are meaningful and are
therefore always present. `revision` is a canonical decimal string so it can be
round-tripped without JavaScript's integer precision limit. `overrides` contains
only explicit non-default scalar values, keyed by catalog option ID.
`catalogDigest` pins the canonical scalar catalog. `actionCatalogDigest`
separately pins the canonical action catalog, a newline separator, and the exact
raw config schema bytes referenced by that action catalog.
Each catalog option carries an explicit `writable` capability. The tagged
`input:scroll_points` custom-acceleration grammar and
`scrolling:explicit_column_widths` CSV grammar remain inventoried for
compatibility but are non-writable in managed v1; both require future typed
surfaces. Tablet and touch-device output bindings are likewise non-writable
until the device UI has a typed live-monitor selector, and `devices[].overrides`
therefore has no `output` field in v1. Desired-state validation rejects an
override for any non-writable option.

Every scalar catalog entry also carries an explicit managed output `module` and
an exact `luaPath` array. Registry punctuation is never interpreted by the
renderer. For example, `general:col.active_border` emits through
`["general", "col", "active_border"]`, while the upstream
`input-capture:capture_modifiers` spelling emits from the `input` module through
`["input_capture", "capture_modifiers"]`.
Complex-surface catalog records carry the same module/path metadata for their
tagged `hl.*` constructor, including `define_submap`, so no emitter dispatch is
selected from a collection name.

The complex records mirror the public tagged `0.56.1` Lua API with closed keys
and types. Monitor and device aliases are canonicalized, gesture callbacks are
excluded, binding dispatchers are a closed inventory, and `exec_cmd`/`exec_raw`
are deliberately excluded so desired state cannot contain arbitrary commands.
Workspace `on_created_empty` is likewise excluded from managed records. Custom
commands and unsupported Lua belong in `user-custom.lua`, outside the generated
authority. Startup applications are deferred to a later supervised service and
are not accepted as Hyprland configuration state. Environment records only
describe compositor-session environment ownership.

Curves are declared before animations. Submaps are declared before bindings;
an empty reset target means Hyprland's default map. Runtime validation enforces
unique names and IDs, curve references, submap reset/reference integrity and
acyclic reset chains, duplicate normalized chords, and the tagged binding-option
invariants that cannot be expressed locally in JSON Schema.

The tagged `hl.bind` parser exposes a `mouse` option but does not assign it to
the keybind record. V1 therefore rejects that non-functional option; mouse
buttons are represented through the binding key symbol instead. `switch:*`
selectors are also deferred until they have a stable typed representation.

`catalog.schema.json` describes the checked-in scalar catalog and its UI/apply
metadata. The catalog is extracted from the tagged upstream C++ value registry;
the UI tier, risk, apply mode, and string-enum metadata are a reviewed semantic
overlay. Runtime `hyprctl descriptions` is only a compatibility probe.

`action-catalog.schema.json` describes the complete managed action inventory.
Its 47 compositor dispatchers are the tagged public `hl.dsp` surface minus raw
execution and the untyped `layout` and `window.set_prop` mini-languages; every
record points to its exact typed argument schema and declares its exact Lua
invocation shape. Renderer code therefore
never guesses whether an argument object is omitted, passed as a table, or
reduced to a scalar field. It also inventories the closed default-app,
HyprShelld, and gesture semantic action namespaces, their broker/table
marshalling, and intentional exclusions.

`generation-manifest.schema.json` binds one immutable generated Lua tree to the
exact snapshot, scalar and action catalogs, renderer contract, activation nonce,
and a map keyed by canonical relative file path. Schema validation alone never
authorizes activation. The compositor authority must require the entrypoint to
be a file-map key, recompute every file size and SHA-256, recompute `generation`
as SHA-256 of canonical manifest JSON with the `generation` member omitted, and
cross-check the target against the pinned compatible range. Live Reload
activation additionally requires the exact manifest activation nonce from the
same Hyprland process, the following generic config-reloaded boundary, and an
empty strict JSON config-error result.

Live activation treats the compositor session's user ID as its local trust
boundary. Retained authority directory descriptors and canonical-name checks
fail closed when configuration roots move or are replaced, and production
D-Bus name ownership prevents a second compositord instance. These mechanisms
are not a privilege boundary against a malicious process running as the same
user; such a process can interfere with that user's files and session IPC.

The source inventories and provenance are under `tests/fixtures/hyprland/`.
`source-manifest.schema.json` pins all three `VERSION` files, the two scalar
registries, all tagged source files used to qualify the complex grammar, and
the focused `0.56.0` startup sources needed to qualify the loader readiness
guard across the full supported patch range. It separately pins
`src/debug/HyprCtl.cpp` and `src/output/Monitor.cpp` at both `0.56.0` and
`0.56.1`; extraction also asserts the exact `j/monitors all` JSON field
inventory, mode formatting, mirror/disabled semantics, reserved-edge order,
and short-description construction consumed by display discovery.
They can be reproduced without network access from official tagged source trees:

```sh
python3 tools/hyprland/extract_contract.py \
  --source-055 /path/to/Hyprland-0.55.0 \
  --source-0560 /path/to/Hyprland-0.56.0 \
  --source-056 /path/to/Hyprland-0.56.1 \
  --output-root .
```

Use `--check` in qualification jobs to reject source drift, stale generated
files, unexpected option additions/removals, or unversioned Hyprland wiki links.
Before reading an inventory, the extractor verifies `VERSION`, both scalar
registries, every complex-surface source, and the focused startup sources
and monitor-query sources against immutable SHA-256 pins for the reviewed tags.
A caller-supplied or locally modified tree is rejected before trusted tag and
commit provenance can be emitted.

After extraction, validate all five Draft 2020-12 schemas, their checked-in
instances, cross-file references, and catalog digests with:

```sh
python3 tools/hyprland/validate_contract.py --root .
```

The validator requires the Python `jsonschema` package. Qualification jobs must
install it in an isolated environment rather than weakening validation when the
dependency is unavailable.
