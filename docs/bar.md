# Bar

HyprShelld places one floating bar at the top of every active display. Each bar
sits 12 logical pixels from the top and side edges and keeps 8 logical pixels
of inward space between the bar and the tiled workspace. This reserved space
keeps windows from tiling under the bar.

The bar height applies to every display. It can be set from 24 to 96 logical
pixels and defaults to 40 logical pixels.

The bar also uses HyprShelld's shared border shape. The border line is enabled
by default with a width of 1 logical pixel and a corner radius of 15 logical
pixels. Both values can be set from 0 through 20. On a short bar, the rendered
corner radius is limited to half the bar height so the shape remains valid.

## What the bar shows

The current bar shows configuration availability and, when enabled, the
built-in workspace switcher component for that display on the left. The date
and time remain in the center, and the display name remains on the right. A
**!** badge appears beside the HyprShelld name only when a desktop component
needs attention.

## Switch workspaces

Each bar shows the existing numbered and named workspaces assigned to that
display. Numbered workspaces appear first in numeric order, followed by named
workspaces in alphabetical order. This includes empty workspaces that Hyprland
is configured to keep persistent. Special workspaces are not included, and the
bar does not add unused workspace places of its own.

By default, the current workspace appears as a filled circle and the others as
smaller hollow circles. Numbered workspaces place their number inside the
circle; named workspaces use their initial. A workspace that needs attention
receives a separate attention marker. Select an inactive workspace to switch to
it. The live bar does not take keyboard focus, so use the pointer or your normal
Hyprland shortcuts to change workspaces.

The switcher follows workspaces as they are created, removed, renamed, or moved
between displays. When an ordinary empty workspace disappears from Hyprland,
its circle also disappears. A persistent empty workspace remains visible.

If the workspace connection becomes unavailable, the bar marks the switcher as
unavailable and leaves it inactive until the connection returns.

## Customize the workspace switcher

Open **HyprShelld Settings**, select **Bar**, and use the **Workspaces** card to
choose how the switcher behaves:

- **Show workspace identifiers** places numbers or named-workspace initials
  inside the circles. This is on by default.
- **Show workspace names** appends the full name of each custom or named
  workspace. Numeric workspaces do not repeat their number as a name.
- **Show application icons** appends the applications open on each workspace to
  that workspace's anchor.
  Repeated applications on an inactive workspace are grouped with a count. On
  the current workspace, windows remain individually represented and the active
  one is emphasized. Select a window icon on the current workspace to bring
  that window forward. Icons on inactive workspaces, including grouped icons,
  switch to that workspace instead of targeting a hidden window. You can show
  from one to five icons per workspace; any remaining applications appear as a
  `+N` summary.
- **Show occupied only** hides empty workspaces, including persistent empty
  workspaces, but always keeps the current workspace visible.
- **Scroll to switch** can be off, normal, or reversed. Scrolling moves only
  among the real workspaces currently shown on that display and stops at the
  first or last one instead of wrapping around.

The identifier and name choices are independent. You can show identifiers,
names, both, or neither without changing the filled-current and hollow-inactive
circle design.

Changes are saved automatically as one workspace-switcher update and apply to
every bar. The Settings preview uses illustrative workspaces and applications;
it is not a live view of your session. The preview remains at the top of the
Bar page while the Size and Workspaces settings scroll beneath it, so each
change stays visible while you configure the bar.

The workspace switcher itself can be disabled from **Settings → Components →
Bar Widgets**. Disabling it removes the switcher without discarding its saved
choices. Its natural **Workspaces** settings card remains visible but dimmed
and read-only until the component is enabled again.

Workspace choices are stored separately from the core Bar settings. If the
component settings or component catalog is unavailable, Settings keeps the
workspace preview visible but makes only the **Workspaces** card read-only. You
can still change the bar height and shared border when core settings are
available.

## Component failure notices

When a component cannot restart automatically, one bar temporarily replaces
its clock with a failure notice. The notice remains for up to six seconds and
closes sooner if the component recovers. While the failure remains, a **!**
badge stays on every bar after the notice closes so the problem is not lost.

Open **HyprShelld Settings** to see the affected component. When the shell
health service is available, Settings also provides a **Restart** action. The
warning and badges clear only after HyprShelld confirms recovery. If the
desktop-surface component itself is unavailable, it cannot draw its own notice,
but the persistent warning remains available in Settings.

## Change the bar height

1. Open **HyprShelld Settings** and select **Bar** in the sidebar.
2. Drag the **Bar height** slider to the size you want.
3. Use the pinned desktop preview to see the new height while you drag.
4. Release the slider to save the change and apply it to every bar.

Changes are saved automatically. Select **Reset** to return every bar to the
default height of 40 logical pixels.

If core settings are unavailable, Settings shows a Bar warning and
disables the height and border controls until they reconnect. The displayed
height and border may be out of date while that warning is visible. A separate
workspace warning does not disable the core Bar controls.

## Customize the shared border

Open **HyprShelld Settings**, select **Bar**, and use the **Border** card:

- **Show border** draws or removes the border line on the bar and, while
  synchronization is on, on Hyprland windows. Hiding the line does not discard
  its saved width or corner radius.
- **Border width** sets the line from 0 through 20 logical pixels.
- **Corner radius** rounds the bar from 0 through 20 logical pixels.
- **Sync Hyprland window borders** makes HyprShelld the visual authority for
  the matching managed Hyprland border options. While synchronization is on,
  those Hyprland controls remain visible but read-only. Turn synchronization
  off when window borders should use their explicit Hyprland override without
  changing the bar.

Each interaction automatically saves the complete enabled, width, radius, and
synchronization choice as one shared-border update. The pinned desktop preview
renders the real Bar component as an illustration; viewing the preview does not
itself change the live session. Saving updates every live Bar through Config1.
With synchronization enabled, the same saved geometry reaches managed Hyprland
windows automatically only from an exact current managed baseline. That
verified activation may reload the managed Hyprland configuration. Otherwise,
the matching geometry remains saved until explicit safe Apply or compositor
takeover. Select **Reset** to enable the border, restore width 1 and radius 15,
and synchronize Hyprland window borders again.

See [Settings](settings.md) for recovery messages and configuration-file
locations.

Return to the [HyprShelld User Guide](index.md).
