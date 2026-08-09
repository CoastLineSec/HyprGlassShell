# HyprShelld User Guide

HyprShelld is a desktop environment being built specifically for Hyprland. Its
goal is to provide a polished desktop that works out of the box while remaining
configurable for experienced users.

## Current status

HyprShelld is in early development. There is not yet a supported complete
desktop package or daily-use release. The current development installation
includes a floating bar on each display with configurable per-display workspace
switching, plus an independent Settings application for changing the bar height
and workspace presentation. Settings also provides a Components page where
the built-in workspace switcher can be enabled or disabled without moving its
detailed choices away from the Bar page. The bar follows the workspaces
Hyprland reports without filling unused places. Core bar size and
workspace-component choices recover independently, so a workspace settings
problem does not block an otherwise available height control. The bar also
reports a component that cannot recover automatically and offers bounded
recovery from Settings.

The guides below describe behavior available in the current development build.

## Available guides

- [Bar](bar.md) — switch workspaces, customize their indicators, understand the
  current layout, and change the bar height.
- [Settings](settings.md) — adjust bar and workspace settings, manage component
  enablement, recover settings, and respond to failed desktop components.
