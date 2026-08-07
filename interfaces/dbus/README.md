# HyprShelld D-Bus Contracts

The XML files in this directory are the machine-readable authority for
HyprShelld's public per-user D-Bus interfaces. Qt adaptors and proxies are
generated from them; implementations and clients must not maintain separate
handwritten signatures.

| Service | Bus name | Object path | Interface |
| --- | --- | --- | --- |
| Coordinator | `org.hyprshelld.Coordinator1` | `/org/hyprshelld/Coordinator1` | `org.hyprshelld.Coordinator1` |
| Configuration | `org.hyprshelld.Config1` | `/org/hyprshelld/Config1` | `org.hyprshelld.Config1` |

Both services use the session bus. Configuration may be D-Bus activated so
Settings remains usable when the shell target is stopped. Settings treats an
absent coordinator bus name as coordinator unavailability.

## Coordinator

`FailedUnits` contains sorted, unique HyprShelld systemd user-unit names whose
persistent failure requires action. `Healthy` is true exactly when that list is
empty. Successful `RestartComponent` means the bounded recovery request was
accepted; health changes only after systemd reports recovery.

Coordinator1 accepts only `hyprshelld-configd.service` and
`hyprshelld-surfaced.service`. The method cannot be used as a generic systemd
restart endpoint.

Restart errors have these meanings:

- `org.hyprshelld.Coordinator1.Error.UnknownComponent`: the supplied unit name
  is not in the coordinator's allowlist;
- `org.hyprshelld.Coordinator1.Error.ComponentNotFailed`: the unit is known but
  has no persistent failure eligible for a manual restart; and
- `org.hyprshelld.Coordinator1.Error.RestartFailed`: systemd did not accept the
  bounded restart request.

## Configuration

`BarHeight` is measured in logical pixels, defaults to 48, and accepts values
from 32 through 96. `SetBarHeight` and `ResetBarHeight` persist a real change
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

## Change publication and versioning

Changing properties are published together through the standard
`org.freedesktop.DBus.Properties.PropertiesChanged` signal after persistence
succeeds. The project interfaces do not redeclare that standard interface or
duplicate it with interface-specific change signals.

The numeric suffix is the contract's major version. Compatible additions may
extend version 1. Removing or renaming members, changing signatures or accepted
input, or changing existing error meaning requires a new bus name, object path,
interface, and XML filename with the next major version.
