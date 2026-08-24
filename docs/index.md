# HyprShelld User Guide

HyprShelld is a desktop environment being built specifically for Hyprland. Its
goal is to provide a polished desktop that works out of the box while remaining
configurable for experienced users.

## Current status

HyprShelld is in early development. There is not yet a supported complete
desktop package or daily-use release. The current development installation
includes a floating bar on each display with configurable per-display workspace
switching, plus an independent Settings application for changing the bar height
and workspace presentation. Shared spacing can synchronize normal Hyprland
window gaps, and a covering maximized (not fullscreen) window attaches only
its display's bar to the top edge. Settings also provides an Appearance page
with 40 reviewed window visuals, including direct exact window corner power,
whether effective visible borders extend window-shadow bounds, and shadow
range from 0 through 100 (default 4), falloff power from 1 through 4 (default
3), default-off sharp edges, exact scale from 0 through 1 (default 1), and
exact horizontal and vertical offsets from -250 through 250 layout pixels
(both default 0). Those shadow-rendering values are retained while shadows are
off; range, scale, and offset still apply to sharp
shadows, and the saved falloff returns after sharp edges are turned off. The
preview reports them in its accessible summary without simulating geometry,
position, shadow size, color, opacity, blur, or motion, and **Save & apply**
activates them through a verified Reload. The page also provides default-off
managed inner glow with exact range 0 through 100 (default 10) and falloff
power 1 through 4 (default 3). Disabled low ranges remain editable and exact,
but glow cannot be enabled below 10; an inherited enabled low-range state stays
visible for repair instead of being normalized. Glow size, falloff, color,
opacity, blur, and motion remain summary-only in the preview. Appearance also
provides parameter tuning and ordering for existing
custom animation curves and full animation-rule authoring; curve structure remains
read-only until a verified compositor-restart workflow exists. An Input page
handles global keyboard, virtual keyboard, mouse, pointer behavior, cursor
visibility and placement, touchpad, touch-device, and drawing-tablet behavior
and provides authenticated read-only device diagnostics with
preserved saved-record status, plus an ordered editor for nine fixed gesture
actions and exact read-only compatibility records. A dedicated Hyprland hub
organizes the complete supported 0.56.2 scalar inventory into Appearance,
Input, Windows, Shortcuts, System, and Session catalogs and routes to the
structured editors. A Displays page configures connected outputs,
and a Windows & Layout
page covers
layout, grouping, resize, snap, exact pointer-focus movement thresholds,
focus and window-swallowing behavior, and
group-bar interaction, geometry, and typography. There is also an
independent Workspaces page for compositor workspace switching, history,
pointer placement, and gesture behavior plus ordered typed user Workspace
Rules, an active ordered editor for reviewed shortcuts and modal submaps, a
39-field per-device input-profile editor, ordered environment and permission
editors, an authored Rules page for ordered Window and Layer Rules,
a bounded sixteen-value Advanced page for session-lock recovery and rendering,
compositor fallback-background choices, Rule-qualified frame callbacks, new
screen-share format preference, caution-marked workspace underlay rendering,
SDR work-buffer transfer and direct scanout, native Wayland resize
compatibility, XWayland nearest-neighbor filtering, input-capture protocol
choices, and one display-warning choice. A
Components page lets the built-in workspace switcher
be enabled or disabled without moving its detailed choices away from the Bar
page.
Appearance, Input, Windows & Layout, Workspaces,
Rules, and Advanced drafts use validated desired-state replacement followed by
a verified Hyprland reload. Display changes require explicit permission for
HyprShelld to manage the Hyprland entrypoint, then use a 15-second live test
with keep, revert, and automatic rollback behavior. Read
the Displays guide before taking control: HyprShelld preserves the existing
entrypoint for recovery but does not import its settings.

The bar follows the workspaces Hyprland reports without filling unused places.
Core bar size and workspace-component choices recover independently, so a
workspace settings problem does not block an otherwise available height
control. The bar also reports a component that cannot recover automatically and
offers bounded recovery from Settings.

The guides below describe behavior available in the current development build.

## Available guides

- [Hyprland Settings coverage](hyprland-settings-coverage.md) — trace every
  pinned 0.56.2 scalar and complex surface to its page and control, review
  current-wiki version gates, and see concrete reasons for intentional limits.

- [Bar](bar.md) — switch workspaces, customize shared spacing and borders,
  understand floating and maximized attachment, and change the bar height.
- [Appearance](appearance.md) — edit 40 reviewed window visuals, including
  exact window corner power, border-inclusive shadow bounds, and retained
  shadow range, falloff, sharp-edge behavior, exact scale, and signed two-axis
  offset; configure bounded managed inner glow without normalizing disabled low
  ranges; enter exact blur color-modulation values,
  tune existing custom curves, author animation rules, work with the aggregate
  draft, and understand apply and recovery.
- [Input](input.md) — configure reviewed global keyboard, virtual keyboard,
  mouse, pointer behavior, cursor visibility and placement, touchpad,
  touch-device, and drawing-tablet controls and ordered gesture bindings,
  inspect read-only session device diagnostics, author all 39 typed per-device
  overrides from the Hyprland hub, and understand compatibility, saved-record,
  draft, retry, and recovery semantics.
- [Displays](displays.md) — understand compositor takeover, arrange connected
  outputs, test changes safely, and interpret recovery states.
- [Windows & Layout](windows-layout.md) — choose a layout and tune grouping,
  group-bar behavior and presentation, resizing, spacing, layout-engine
  behavior, snapping, exact pointer-focus movement thresholds, focus, and
  process-parent window swallowing.
- [Workspaces](workspaces.md) — tune workspace transitions and history, pointer
  placement, launch tracking, swipe response, special-workspace gestures, and
  typed per-workspace rules.
- [Shortcuts and submaps](keyboard-shortcuts.md) — edit ordered active managed
  bindings and modal submaps with reviewed actions, complete bind behavior,
  device filters, validation, and restart-aware persistence.
- [Window and Layer Rules](rules.md) — create ordered typed rules, understand
  match and effect fields, and recover a preserved combined draft.
- [Advanced](advanced.md) — configure reviewed session-lock recovery and
  rendering, compositor fallback image and splash behavior, render-unfocused
  frame callbacks, screen-share format, workspace underlay rendering, SDR
  work-buffer transfer, direct scanout, native Wayland resize compatibility,
  XWayland filtering, input-capture protocol, and display-warning behavior.
- [Settings](settings.md) — adjust bar and workspace settings, manage component
  enablement and displays, recover settings, and respond to failed desktop
  components.
