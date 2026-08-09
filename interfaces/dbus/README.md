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
or relocate those dormant records and their instances, except for one explicit
inert adoption of an installed user package: the proposed digest must equal the
exact live catalog entry, its component record must be disabled with no grants,
and every instance and placement must remain unchanged. The normal parser also
validates its new trusted settings schema. Configd may advance a protected
built-in record only through a narrowly pinned, catalog-joined data migration.
Such a migration validates every old instance shape, preserves unrelated state,
increments the configuration revision once, and commits the migrated snapshot
to recovery before active storage. Other unrecognized packages remain dormant
for future package-lifecycle handling.
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

ComponentRuntime1 is the coordinator-owned activation boundary used by
surfaced. Its plan joins one complete ComponentManager1 catalog generation
with one complete ComponentConfig1 desired-state revision. It never publishes
a partially hydrated host plan or a package path, package QML, or an
author-controlled loader target.

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
Retained, unavailable, malformed, and locally stale plans strip every
third-party instance; protected built-ins may remain. An authoritative plan
may request only factories compiled into surfaced's built-in allowlist or the
trusted `declarative-v1` text-pill renderer. Declarative plan entries inline
only coordinator-resolved plain text, an optional plain-text tooltip, and a
bounded maximum width. Desired enablement, component settings, output
matching, and bar placement still come from ComponentConfig1. Version one
keeps declarative placements in the active `main` bar layout; other layouts are
dormant for this runtime.

Surfaced must explicitly authorize the current complete plan before it exposes
any declarative entries. The coordinator writes all renderable declarative
instances to its private runtime-health recovery snapshot in one transaction
before authorization succeeds. The trusted renderer reports success only
after a two-second stabilization window, or reports a bounded render failure.
An eight-second coordinator deadline quarantines the exact component package
digest when activation does not complete. A normal last-screen removal or
graceful surfaced shutdown cancels the whole plan authorization; an interrupted
activation retained across coordinator restart is conservatively quarantined
without claiming which process caused the interruption. The surfaced systemd
restart budget is sized so quarantine and a safe-plan restart occur before its
start limit is exhausted.

`RuntimeHealthRevision` is the comparison token for listing and retrying this
state. Runtime-state rows are sorted by component ID and package digest, and
healthy packages are omitted. A probation row has an empty reason and zero
failures. A quarantined row has one of `incomplete-startup`, `timeout`,
`render-failed`, or `protocol-invalid` and a failure count from 1 through
1,000,000. `RetryComponent` removes only the exact quarantined package digest
at the expected health revision. A new package digest starts clean. Unsafe,
unsupported, exhausted, or unreadable health storage enters third-party safe
mode instead of activating untracked content. The recovery snapshot is the
durable commit point; a failed active-file mirror is repaired from its newer
revision on load.

## Component manager

ComponentManager1 exposes one joined catalog containing protected built-ins and
locally installed per-user packages. `ListComponents` returns the complete
sorted ID set and the catalog digest that owns it. A caller passes that digest
to `GetComponent` so metadata and the trusted settings schema cannot be combined
across catalog generations. For `declarative-v1`, the caller then passes both
that catalog digest and the entry's exact package digest to
`GetDeclarativeRuntime`. The reply contains only compact canonical JSON for the
manager-validated trusted primitive; it never exposes or asks surfaced to open
the author-controlled package entrypoint.
`CatalogDigest` is the exact 64-character lowercase hexadecimal SHA-256 of the
catalog entries sorted by component ID. Each entry is framed as an unsigned
64-bit big-endian ID byte length, the UTF-8 ID bytes, an unsigned 64-bit
big-endian package-digest byte length, and the lowercase hexadecimal package
digest bytes. It is an equality token, not a counter.

Origin and removability are derived from the catalog root rather than manifest
claims. For a built-in, `packageDigest` covers the exact manifest and settings
schema bytes. For a user package, it covers every validated regular package
file, including `integrity.json`, using the same length-framed path-and-content
algorithm.

`BeginPackageInspection` accepts an already-open read-only Unix file
descriptor. componentd copies and hashes those bytes into a private bounded
runtime spool before returning a random sender-bound token; it never reopens a
Settings pathname. A short-lived systemd transient helper parses only ZIP
Store/Deflate entries under strict file, expanded-size, path, type, integrity,
and metadata limits. Its normalized report is delivered only to the caller's
unique bus name through `PackageInspectionFinished`. Tokens expire, cannot be
used by another sender, and are consumed by the first install attempt.

`InstallInspectedPackage` commits only the exact reviewed archive digest into
the exact catalog generation shown during review; a concurrent catalog change
fails closed without consuming the inspection. Selecting another package with the same component ID uses
that operation for an update, reinstall, or downgrade. Installation never
mutates enablement, grants, or placements. A first install is inert; an exact
reinstall may make already-authorized retained state for that identical digest
effective again. Compatible data-only declarative bar widgets can otherwise be
activated later through ComponentConfig1;
reserved QML, process, and desktop-widget runtimes remain explicitly
unsupported and inert. After an update changes a package digest, the old
component settings remain dormant. A separate explicit adoption snapshot may
advance only to the exact live user-package digest; it must force the proposed
record disabled, clear every grant, and provide settings valid under the new
trusted schema. The old dormant record may have been enabled or granted because
its mismatched digest is already unsurfaceable. Configd preserves every
instance and placement, and adoption never activates the update.
`RemovePackage` is digest- and
catalog-guarded, applies only to user
packages without installed dependents, and removes package bytes while configd
retains any settings and placements as dormant recovery data. There is no
remote repository, downloader, updater, signature authority, or package code
execution in this boundary.

The user catalog is bounded to 511 receipts and 512 MiB of expanded active
package data. At startup, each receipt and installed tree is revalidated. An
invalid receipt or tree is excluded from the published catalog without hiding
healthy third-party packages; its dormant configd state is preserved. Selecting
the same reviewed component again repairs corrupt contents or a missing tree
under safe directory ancestry. Unsafe non-regular receipts, symlinked storage
ancestors, and non-directory storage entries remain rejected and require manual
removal. Package transactions
retain only the committed version, and post-ownership recovery removes bounded
staging/trash leftovers and receipt-unreachable versions from interrupted
transactions.

Component manager errors have these meanings:

- `org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest`: the caller's
  digest no longer identifies the active catalog; and
- `org.hyprshelld.ComponentManager1.Error.UnknownComponent`: the requested ID
  is not present in that catalog;
- `org.hyprshelld.ComponentManager1.Error.PackageDigestMismatch`: the requested
  package generation is no longer installed; and
- `org.hyprshelld.ComponentManager1.Error.RuntimeUnavailable`: the component
  has no manager-validated declarative runtime document.

Package lifecycle errors additionally distinguish invalid descriptors,
unavailable, unknown, expired, or wrong-owner inspections, archive and package
digest mismatches, protected IDs or built-in removal, same-version byte
changes, installed dependents, and persistence failures. The exact typed names
are documented beside their methods in
`org.hyprshelld.ComponentManager1.xml`.

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
