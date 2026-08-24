# Shortcuts and submaps

The **Shortcuts & Submaps** page is the active editor for the managed
Hyprland Lua configuration. It edits the ordered `bindings` and `submaps`
collections in desired state and saves them as one validated transaction.

Each shortcut has a stable ID, ordered modifiers, a reviewed key token, an
action type and reviewed action, typed action arguments, a description,
enabled state, optional submap, and the complete supported binding behavior:
repeat, locked, release, non-consuming, auto-consuming, transparent,
ignore-modifiers, do-not-inhibit, long-press, universal-submap, click, drag,
input-capture, and an optional inclusive or exclusive device list.

The editor rejects duplicate enabled chords in the same submap, unknown
actions, invalid key or mouse codes, incompatible repeat/release/click/drag
combinations, duplicate device names, missing submaps, and catch-all bindings
outside a submap. Actions come from the pinned reviewed action catalog; the UI
does not offer an arbitrary command field.

The **Submaps** tab creates, orders, renames, enables, and removes modal
shortcut layers. A submap may reset to global bindings or another known
submap. Removing a referenced submap also removes that reference from its
shortcuts so the saved collection cannot dangle.

Saving preserves every unrelated scalar option and structured configuration
surface. Binding and submap changes are restart-classified by the pinned
Hyprland contract, so Settings can persist correct Lua while reporting that a
verified compositor restart is still required before the running session uses
the new keymap.

The former 117-row bundled keybinding table remains a source fixture for
compatibility tests only. It is no longer the Settings navigation destination
and is not treated as the user's active keymap.
