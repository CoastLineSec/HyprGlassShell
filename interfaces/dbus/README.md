# HyprShelld D-Bus Contracts

The XML files in this directory are the machine-readable authority for
HyprShelld's public per-user D-Bus interfaces. Generated Qt bindings and
contract tests lock their wire signatures. A bounded client that decodes a
reply manually must still match the authoritative XML exactly and reject any
different types, field counts, or ordering.

| Service | Bus name | Object path | Interface |
| --- | --- | --- | --- |
| Coordinator | `org.hyprshelld.Coordinator1` | `/org/hyprshelld/Coordinator1` | `org.hyprshelld.Coordinator1` |
| Component runtime | `org.hyprshelld.Coordinator1` | `/org/hyprshelld/Coordinator1/Components` | `org.hyprshelld.ComponentRuntime1` |
| Configuration | `org.hyprshelld.Config1` | `/org/hyprshelld/Config1` | `org.hyprshelld.Config1` |
| Component configuration | `org.hyprshelld.Config1` | `/org/hyprshelld/Config1/Components` | `org.hyprshelld.ComponentConfig1` |
| Component manager | `org.hyprshelld.ComponentManager1` | `/org/hyprshelld/ComponentManager1` | `org.hyprshelld.ComponentManager1` |

All services use the session bus. Configuration and the component manager may
be D-Bus activated so Settings can remain independent when the shell target is
stopped. Settings treats an absent coordinator bus name as coordinator
unavailability.

## Coordinator

`FailedUnits` contains sorted, unique HyprShelld systemd user-unit names whose
persistent failure requires action. `Healthy` is true exactly when that list is
empty. Successful `RestartComponent` means the bounded recovery request was
accepted; health changes only after systemd reports recovery.

Coordinator1 accepts only `hyprshelld-configd.service`,
`hyprshelld-componentd.service`, and `hyprshelld-surfaced.service`. The method
cannot be used as a generic systemd restart endpoint.

Restart errors have these meanings:

- `org.hyprshelld.Coordinator1.Error.UnknownComponent`: the supplied unit name
  is not in the coordinator's allowlist;
- `org.hyprshelld.Coordinator1.Error.ComponentNotFailed`: the unit is known but
  has no persistent failure eligible for a manual restart; and
- `org.hyprshelld.Coordinator1.Error.RestartFailed`: systemd did not accept the
  bounded restart request.

## Configuration

`BarHeight` is measured in logical pixels, defaults to 40, and accepts values
from 24 through 96. `SetBarHeight` and `ResetBarHeight` persist a real change
atomically before publishing it and return the resulting revision. An
idempotent request returns the existing revision without writing or emitting a
change.

The active snapshot follows the XDG base directories at
`$XDG_CONFIG_HOME/hyprshelld/settings.json`, normally
`~/.config/hyprshelld/settings.json`. Its last-known-good recovery snapshot is
internal state at `$XDG_STATE_HOME/hyprshelld/settings.last-good.json`, normally
`~/.local/state/hyprshelld/settings.last-good.json`.

`RecoveryState` describes startup loading for the current service lifetime:

- `normal`: valid state loaded, or no state existed on an ordinary first run;
- `recovered`: damaged active state was replaced by last-known-good state; or
- `defaulted`: damaged active state existed and no valid last-known-good state
  was available, so defaults were used.

Clients treat unknown future values as non-normal and display a recovery
warning.

Configuration errors have these meanings:

- `org.hyprshelld.Config1.Error.InvalidBarHeight`: the requested height is
  outside the accepted range; and
- `org.hyprshelld.Config1.Error.PersistenceFailed`: the new state could not be
  persisted atomically, so the active value and revision remain unchanged.

## Component configuration

ComponentConfig1 is a separate recovery and revision domain owned by configd.
Failure or an unsupported future component snapshot does not unregister core
Config1 or block bar-height reads and writes. Its active file is
`$XDG_CONFIG_HOME/hyprshelld/components.json`; its last-known-good file is
`$XDG_STATE_HOME/hyprshelld/components.last-good.json`.

`Available` means one authoritative desired-state snapshot can be read.
`CatalogAvailable` separately means a complete live ComponentManager1 join is
available for validating writes. Ordinary componentd loss retains the last
authoritative snapshot but makes mutations fail closed. `LoadState` is one of
`normal`, `recovered`, `defaulted`, `unsupported`, or `unavailable`.
When the active snapshot is valid but its recovery file is unreadable or uses
a future format, the active snapshot remains readable while writes are
disabled and the recovery bytes are left untouched. A later successful reload
can restore writability without discarding that last-known-good active state.

`GetSnapshot` returns the complete strict JSON snapshot, its monotonic
configuration revision, and the full catalog digest against which it was last
validated. `ReplaceSnapshot` is the single whole-state compare-and-swap
operation. It checks both the desired-state revision and the live catalog
digest, validates every current component value against manager-owned package
digests and schemas, persists atomically, and only then publishes the new
revision. Caller-supplied digests are equality assertions and never establish
package or schema authority.
Missing packages and package-digest mismatches remain inert, structurally
bounded desired-state records. ReplaceSnapshot cannot edit, delete, advance,
or relocate those dormant records and their instances. Configd may advance a
protected built-in record only through a narrowly pinned, catalog-joined data
migration. Such a migration validates every old instance shape, preserves
unrelated state, increments the configuration revision once, and commits the
migrated snapshot to recovery before active storage. Unrecognized packages
remain dormant for future package-lifecycle handling.
Dormant numeric values remain canonical JSON numbers and must be finite with a
magnitude no greater than 9007199254740991, so parsing and persistence cannot
silently round an unknown component's retained values.

Component configuration errors have these meanings:

- `Unavailable`: no writable authoritative component state is available;
- `CatalogUnavailable`: live catalog/schema truth is unavailable;
- `StaleRevision`: another desired-state mutation won the comparison;
- `StaleCatalogDigest`: the component catalog changed;
- `InvalidSnapshot`: the proposed complete state violates its structural,
  schema, capability, instance, or placement contract;
- `RevisionExhausted`: the unsigned revision cannot advance; and
- `PersistenceFailed`: active state could not be atomically committed.

## Component runtime

ComponentRuntime1 is the coordinator-owned, read-only activation boundary used
by surfaced. Its plan joins one complete ComponentManager1 catalog generation
with one complete ComponentConfig1 desired-state revision. It never publishes
a partially hydrated host plan.

`SurfacePlanRevision` is an opaque unsigned equality token for the complete
normalized plan; zero means no authoritative plan has been produced in this
coordinator lifetime. `SurfacePlanDigest` is the full lowercase SHA-256 of the
exact plan bytes and is empty exactly while the revision is zero.
`SurfacePlanState` is one of:

- `hydrating`: no valid complete plan has been accepted yet;
- `authoritative`: the published plan is joined to the current catalog and
  configuration snapshots;
- `retained`: an earlier authoritative plan remains the last-known-good plan
  while one of its authorities is unavailable or malformed; or
- `unavailable`: no authoritative plan can be served.

`GetSurfacePlan` accepts the caller's expected revision and returns the exact
immutable strict-JSON plan bytes plus their digest. A mismatched token fails
with `StaleSurfacePlanRevision`; revision zero or a missing plan fails with
`PlanUnavailable`. Consumers validate the bytes atomically against both the
returned digest and the installed `surface-plan.schema.json` contract.

Before its first authoritative plan, surfaced may render only the compiled
first-run built-in plan. Once any authoritative plan is accepted, including an
authoritative empty plan, surfaced retains that last-known-good authority
through ordinary coordinator loss and never resurrects the compiled fallback.
The runtime plan may request only factories compiled into surfaced's built-in
allowlist; desired enablement, instance settings, output matching, and bar
placement still come from ComponentConfig1.

## Component manager

ComponentManager1 currently exposes the protected built-in component catalog
read-only. `ListComponents` returns the complete sorted ID set and the catalog
digest that owns it. A caller passes that digest to `GetComponent` so metadata
and the trusted settings schema cannot be combined across catalog generations.
`CatalogDigest` is the exact 64-character lowercase hexadecimal SHA-256 of the
catalog entries sorted by component ID. Each entry is framed as an unsigned
64-bit big-endian ID byte length, the UTF-8 ID bytes, an unsigned 64-bit
big-endian package-digest byte length, and the lowercase hexadecimal package
digest bytes. It is an equality token, not a counter.

Origin and removability are derived from the protected install root rather
than manifest claims. `packageDigest` is a manager-derived SHA-256 digest of
the exact built-in manifest and settings-schema bytes. The first catalog entry
is `io.github.coastlinesec.hyprshelld.workspace-switcher`; package inspection,
installation, updates, removal, and execution are intentionally absent from
the current read-only component-manager interface.

Component manager errors have these meanings:

- `org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest`: the caller's
  digest no longer identifies the active catalog; and
- `org.hyprshelld.ComponentManager1.Error.UnknownComponent`: the requested ID
  is not present in that catalog.

## Change publication and versioning

Changing properties are published together through the standard
`org.freedesktop.DBus.Properties.PropertiesChanged` signal after persistence
succeeds. The project interfaces do not redeclare that standard interface or
duplicate it with interface-specific change signals.

The numeric suffix is the contract's major version. Before the first public
release, an accepted design decision may revise version 1 while its source,
generated bindings, tests, and documentation change together; compatibility
with older pre-release builds is not promised. After version 1 ships,
compatible additions may extend it. Removing or renaming members, changing
signatures or accepted input, changing default or reset behavior, or changing
existing error meaning requires a new bus name, object path, interface, and XML
filename with the next major version.
