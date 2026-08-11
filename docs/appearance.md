# Appearance and behavior

The **Appearance** page changes a small, reviewed set of common Hyprland
options through HyprShelld's managed compositor configuration. These are
desired managed values, not a reading of every value effective in the running
session. In particular, the user-owned `user-custom.lua` is loaded after the
managed modules and can override them.

Appearance becomes editable only after HyprShelld manages the Hyprland
entrypoint and the active desired-state revision is current. If management has
not started, open **Displays**, read the takeover confirmation, and choose
whether to allow it. Appearance never starts takeover implicitly.

## Available choices

The first Appearance page intentionally exposes eight common, safe options.
Two of them participate in HyprShelld's shared visual style:

- **Border thickness** sets the managed window border from 0 to 20 layout
  pixels. By default it follows the shared border configured on **Bar**.
- **Corner radius** sets rounded corners from 0 to 20 layout pixels. Its
  synchronized value also follows the shared border configured on **Bar**.
- **Blur backgrounds** enables or disables Hyprland's configured background
  blur. It is enabled by default.
- **Window shadows** enables or disables managed drop shadows. They are enabled
  by default.
- **Animations** enables or disables Hyprland's configured animations. They are
  enabled by default.
- **Default layout** selects Dwindle, Master, Scrolling, or Monocle. Dwindle is
  the default.
- **Resize from borders and gaps** allows pointer drags from those areas to
  resize windows. It is disabled by default.
- **Snap floating windows** enables Hyprland's managed floating-window
  snapping. It is disabled by default.

While synchronization is on, window border thickness and corner radius remain
visible but read-only and are labeled **Controlled by HyprShelld**. Change the
shared values from **Bar**, or select **Override window borders** when Hyprland
should use a separate pair. That override affects only these two values; blur,
shadows, animation, layout, resize, and snapping remain independent. **Sync
with HyprShelld** restores shared authority and reconciles the current shared
values. HyprShelld activates that reconciliation automatically only from an
exact current managed base; otherwise the matching values remain saved pending
an explicit safe apply or compositor takeover. Turning the shared border line
off sets a synchronized Hyprland border width of zero without discarding the
saved width or corner radius.

The Bar can keep using and editing the shared style when compositor management
is unavailable. HyprShelld preserves the last applied window appearance and
reports synchronization as unavailable rather than taking over or reloading
Hyprland. Only the explicit takeover flow in **Displays** can adopt an
unmanaged compositor entrypoint.

Settings obtains the types, ranges, choices, and defaults from the exact
catalog advertised by the compositor-settings authority. If that catalog is
missing, has the wrong digest, or no longer matches this page's reviewed
contract, every Appearance control fails closed. The rest of Settings remains
available.

The window preview is deterministic and illustrative. It helps compare the
draft's layout, borders, corners, blur, shadows, resize handles, and snap guides,
but it is not a capture or pixel-accurate simulation of the running desktop.
When animations are enabled, it repeatedly demonstrates a window opening,
settling into the selected layout, and closing again. Dwindle divides one
full-width tile into two equal tiles. Master keeps a dominant master while two
stack windows divide the remaining column. Scrolling moves an equal-width
column strip through the visible area, and Monocle replaces one full-area
window with another. Use **Pause motion** or **Play motion** to control the
demonstration. Turning animations off stops the loop and leaves a stable
preview. The preview remains at the top of the page while the setting cards
scroll beneath it, so draft changes stay visible while you work. Nothing in
the preview changes the live session.

## Draft, save, and apply

Changing a control creates a local draft. Use:

- **Discard draft** to return to the authoritative values without writing;
- **Reset to defaults** to prepare catalog defaults for direct compositor
  controls while retaining any synchronized border pair; or
- **Save & apply** to persist and activate the complete validated draft.

Resetting to defaults removes redundant managed overrides rather than pinning
copies of defaults in the desired-state document. It does not edit
`user-custom.lua`.

**Save & apply** is deliberately a two-stage operation. HyprShelld first uses a
compare-and-swap replacement to create exactly one desired-state revision while
preserving every setting outside the eight reviewed options. It then reloads
Hyprland and verifies that exact saved revision. Display tests and other
compositor mutations cannot overlap this operation.

If another client changes the compositor revision while you are editing,
Settings preserves the visible draft and refuses to silently rebase or
overwrite the newer state. Select **Load current settings** when you are ready
to discard that draft and review the new baseline.

If saving succeeds but activation fails, the desired revision remains saved
and Settings says that it is not active. **Retry apply** retries the exact saved
revision when the authority says that is safe. It does not create another
desired-state revision.

Shared-border synchronization follows the same durable compositor activation
rules. A successful managed update advances the normal last-known-good state.
If synchronization fails, the Bar still reflects HyprShelld's saved style, Hyprland
keeps its last verified live state, and Settings offers an explicit retry. It
does not repeatedly reload the same failed tuple in the background.

## Restore the last working configuration

When recovery is available, **Restore last working configuration** opens a
cancel-first confirmation. This is whole-compositor recovery, not an Appearance
reset: it can replace every pending compositor setting, including display and
future settings, with the last verified working snapshot.

After confirmation, HyprShelld records that snapshot as a new monotonic
desired-state revision, reloads Hyprland, and verifies it. Canceling does not
change desired files or the running compositor. An ambiguous recovery response
is not retried automatically. If window-border synchronization remains on,
HyprShelld then reasserts the current shared border pair instead of leaving
those two values under the recovered compositor snapshot.

Appearance uses the same compositor desired-state and last-known-good files as
Displays. On a standard installation these are
`~/.local/state/hyprshelld/compositor/desired.json` and
`~/.local/state/hyprshelld/compositor/last-good.json`. See
[Displays](displays.md) for the managed loader, preserved pre-takeover
entrypoint, immutable generations, and `user-custom.lua` locations.

Return to [Settings](settings.md) or the [HyprShelld User Guide](index.md).
