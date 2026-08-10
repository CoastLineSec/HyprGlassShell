# HyprShelld User Guide

HyprShelld is a desktop environment being built specifically for Hyprland. Its
goal is to provide a polished desktop that works out of the box while remaining
configurable for experienced users.

## Current status

HyprShelld is in early development. There is not yet a supported complete
desktop package or daily-use release. The current development installation
includes a floating bar on each display with configurable per-display workspace
switching, plus an independent Settings application for changing the bar height
and workspace presentation. Settings also provides an Appearance page for a
reviewed set of common window visuals and behaviors, a Displays page for
configuring connected outputs, and a Components page where the built-in
workspace switcher can be enabled or disabled without moving its detailed
choices away from the Bar page. Appearance drafts use validated desired-state
replacement followed by a verified Hyprland reload. Display changes require
explicit permission for HyprShelld to manage the Hyprland entrypoint, then use
a 15-second live test with keep, revert, and automatic rollback behavior. Read
the Displays guide before taking control: HyprShelld preserves the existing
entrypoint for recovery but does not import its settings.

The bar follows the workspaces Hyprland reports without filling unused places.
Core bar size and workspace-component choices recover independently, so a
workspace settings problem does not block an otherwise available height
control. The bar also reports a component that cannot recover automatically and
offers bounded recovery from Settings.

The guides below describe behavior available in the current development build.

## Available guides

- [Bar](bar.md) — switch workspaces, customize their indicators, understand the
  current layout, and change the bar height.
- [Appearance and behavior](appearance.md) — edit the reviewed window-style and
  interaction choices, work with drafts, and understand apply and recovery.
- [Displays](displays.md) — understand compositor takeover, arrange connected
  outputs, test changes safely, and interpret recovery states.
- [Settings](settings.md) — adjust bar and workspace settings, manage component
  enablement and displays, recover settings, and respond to failed desktop
  components.
