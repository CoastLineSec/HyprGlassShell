# Displays

The **Displays** page in HyprShelld Settings reads the connected-output
topology from the running Hyprland session. It lets you arrange those outputs
and test the complete connected-display layout before saving it.

Display editing is available only when compositor settings are writable,
HyprShelld owns its managed entrypoint, and the saved and active compositor
revisions match. This prevents a display test from silently applying unrelated
pending compositor settings.

Takeover, pending-baseline Apply, display editing, and **Test changes** also
wait while shared border or spacing authority is changing or its exact source
revision has not been verified. This keeps a display generation from racing a
shared visual reconciliation. Refresh remains available because it does not
publish a configuration.

## Before the first change: take control

HyprShelld never claims an existing Hyprland entrypoint at startup. When the
Displays page says the compositor is unmanaged, select **Take control** and
review the confirmation carefully. Canceling the confirmation, pressing
Escape, or closing it makes no compositor change.

Confirming allows HyprShelld to replace the stable
`$XDG_CONFIG_HOME/hypr/hyprland.lua` entrypoint with a generated managed loader.
Before it changes that path, HyprShelld validates the existing regular file or
its absence, preserves that exact original as private recovery material, and
validates a complete managed generation. It finalizes ownership only after the
new loader has been activated and the running compositor has reported a clean
reload. If takeover cannot complete, HyprShelld attempts to restore the exact
previous bytes or absence and the previous config provider.

Takeover is replacement, not migration. HyprShelld does not parse the old
entrypoint, import its values into managed desired state, or copy it into
`user-custom.lua`. Settings that exist only in the old entrypoint can therefore
stop taking effect after takeover. Inspect that file first and manually move
only the custom Lua you intend to retain into
`$XDG_CONFIG_HOME/hypr/user-custom.lua` before confirming.

The compositor settings service initializes `user-custom.lua` with an
owner-only placeholder when the path is absent. If the path already exists, it
is preserved without being read, followed, re-permissioned, rewritten, or
deleted. Every managed generation loads this user-owned file after all managed
modules, so its statements can intentionally override choices shown in
Settings. HyprShelld never includes it in, or overwrites it with, an immutable
generation.

The privately preserved pre-takeover entrypoint is not the same thing as the
last-known-good managed desired-state document. The former records the exact
file or absence that existed before HyprShelld first took ownership; the latter
records the last managed configuration that HyprShelld successfully activated.
The current Settings build does not provide a **Stop managing** action. Do not
edit or remove the ownership record, private original, activation journals, or
generated trees to simulate one: a mismatch is treated as a conflict and
display changes are locked.

## Prepare a layout

Open **HyprShelld Settings**, select **Displays**, and select a connected-output
card. The layout preview represents the topology that Settings discovered from
the running compositor; unlike the Bar preview, it is not an illustrative
desktop mockup.

For each connected output you can change:

- whether it is enabled;
- its advertised resolution and refresh rate, or Hyprland's preferred,
  highest-refresh, or highest-resolution choice;
- scale and orientation;
- automatic arrangement or an explicit logical-pixel position; and
- under **Show advanced settings**, mirroring, bit depth, variable refresh
  rate, color-management mode, wide-color and HDR support, an ICC profile
  path, the SDR transfer/brightness/saturation range, HDR luminance metadata,
  and signed reserved workspace edges.

The mode menu includes Hyprland's preferred, highest-refresh,
highest-resolution, and widest-mode selectors. Automatic placement exposes all
nine managed direction and centered-direction forms, and **Exact display
scale** accepts any schema-valid value of at least 0.25 in addition to the
common percentage presets.

You can also drag an output in the layout preview when explicit positioning is
active. A mirror uses its enabled, non-mirrored target's position. A valid draft
must describe every currently connected connector exactly once and keep at
least one enabled output that is not a mirror. Self-mirroring, mirror chains,
missing or disabled mirror targets, and duplicate outputs are rejected.

**Reset this display** rebuilds only that output's draft from safe values
derived from the observed display. Every pinned Hyprland 0.56.2 monitor field
for that connected output is represented in the card; the stable record
identity is preserved automatically. **Discard draft** returns every connected
output to the saved baseline. Neither action changes the running compositor
until a valid draft is tested and kept.

An unrelated managed settings update preserves a dirty display draft when the
authoritative monitor records and live topology are unchanged. Testing remains
locked until that new whole-compositor baseline and both shared visual groups
are verified. An actual monitor-record or live-topology change still refreshes
the display draft through the safety path described below.

## Test, keep, or revert changes

When the draft is valid, select **Test changes**. HyprShelld stages a
monitor-only candidate, activates it, verifies the realized connected topology,
and opens a 15-second confirmation. The durable desired state and
last-known-good managed state remain unchanged during this test.

- Select **Keep changes** to recheck the live topology and commit the tested
  layout.
- Select **Revert now** to restore the last confirmed layout immediately.
- If the countdown expires, the settings window closes, its private
  confirmation session disappears, the compositor or topology drifts, or the
  confirmation can no longer be verified, the daemon starts the same rollback
  automatically.

The daemon owns the deadline; a stalled or disconnected Settings window cannot
extend it, and a late confirmation is rejected. Another Settings window can see
that a test is active but cannot keep or revert a test it did not initiate. Once
confirmation has entered its final committing step, the tested layout can no
longer be reverted from the prompt.

If automatic rollback cannot itself be verified, Settings reports **Display
recovery needs attention**. Avoid making more compositor changes. If the
desktop health warning reports **Compositor settings**, you can restart it
there; an accepted restart request does not prove recovery, so wait until the
warning clears and review the live layout. If that warning is not present,
preserve the current compositor state and seek recovery guidance instead of
assuming a restart control exists.

## Connecting and disconnecting displays

Use **Refresh** to request a new live connected-output inventory when no test is
active. A newly connected output starts with a safe draft derived from its
observed mode and placement. It is not persisted merely because it appeared.

When the connected topology changes while you are editing, Settings rebuilds
the connected draft from the saved desired state and marks that the inventory
changed. Unsaved edits are not silently applied to the new topology; review the
refreshed draft before testing again. A topology change during the 15-second
live test causes the tested layout to be rejected and the last confirmed layout
to be restored.

Desired records that are not represented by the current exact connector list,
including disconnected-output and description-selected rules, remain outside
the editable connected draft. Settings reports how many other saved rules are
being preserved. Testing and keeping connected-output changes merges those
exact connector records without deleting or rewriting the preserved rules.

## Status and recovery messages

The Displays page can make its controls read-only to protect a state it cannot
prove safe:

- **Display settings are unavailable** means the compositor settings service or
  live discovery is unavailable. Leave Settings open or select **Refresh** after
  the service recovers.
- **This compositor configuration is read-only and has been preserved** means
  the service cannot safely write the desired-state format it found.
- **Restored from the last known good copy** means damaged or interrupted
  managed desired state was replaced with the last valid managed content.
  Review the full display layout before changing it.
- **Safe defaults are in use** means no usable managed desired-state copy could
  be recovered. Review the defaults before continuing.
- **Take control** means the stable entrypoint is still unmanaged. Read the
  takeover section above before confirming.
- **The managed compositor entrypoint changed unexpectedly** is an ownership
  conflict. The stable loader, ownership record, or generated state no longer
  agrees with the authority. Display mutations stay locked rather than
  overwriting the unexpected bytes.
- **Other compositor settings are waiting to be applied** means the desired and
  active managed revisions differ. **Apply pending changes** applies that whole
  exact compositor baseline, not only display values; display testing remains
  locked until it is current.
- **Shared visual settings are changing** or **waiting for an exact verified
  compositor baseline** means border or spacing synchronization has not
  settled. Takeover, Apply, editing, and layout testing unlock after both
  independent shared groups reach a verified safe state.
- **Another display test is active** means this window does not own the private
  confirmation capability. Its initiating window or the daemon timeout remains
  responsible for the outcome.

A `recovered` or `defaulted` desired-state message is distinct from an
entrypoint conflict. Recovery describes which managed settings document was
loaded; conflict means HyprShelld cannot prove ownership of the live entrypoint.

## Configuration locations

With standard XDG paths, the compositor files are:

| Purpose | Location |
| --- | --- |
| Stable Hyprland entrypoint | `~/.config/hypr/hyprland.lua` |
| User-owned Lua loaded last | `~/.config/hypr/user-custom.lua` |
| Managed ownership, private original, live-activation journal, and immutable generations | `~/.config/hypr/hyprshelld/` |
| Current managed desired state | `~/.local/state/hyprshelld/compositor/desired.json` |
| Last successfully applied managed state | `~/.local/state/hyprshelld/compositor/last-good.json` |
| Internal desired-state activation and recovery transaction records | `~/.local/state/hyprshelld/compositor/` |

Systems that set `XDG_CONFIG_HOME` or `XDG_STATE_HOME` store these files below
those locations instead. Except for `user-custom.lua`, these are service-owned
authority or generated files. Preserve them for troubleshooting and do not edit
them while trying to resolve an unavailable, recovery, or conflict message.

See [Settings](settings.md) for bar, component, and desktop-health recovery
messages.

Return to the [HyprShelld User Guide](index.md).
