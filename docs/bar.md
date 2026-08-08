# Bar

HyprShelld places one floating bar at the top of every active display. Each bar
sits 12 logical pixels from the top and side edges and keeps 8 logical pixels
of inward space between the bar and the tiled workspace. This reserved space
keeps windows from tiling under the bar.

The bar height applies to every display. It can be set from 24 to 96 logical
pixels and defaults to 40 logical pixels.

## What the bar shows

The current bar shows configuration availability on the left, the date and
time in the center, and the display name on the right. A **!** badge appears
beside the HyprShelld name only when a desktop component needs attention.

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
3. Use the desktop preview to see the new height while you drag.
4. Release the slider to save the change and apply it to every bar.

Changes are saved automatically. Select **Reset** to return every bar to the
default height of 40 logical pixels.

If the settings service is unavailable, Settings shows a warning and disables
the height control until the service reconnects. The displayed value may be
out of date while that warning is visible.

See [Settings](settings.md) for recovery messages and configuration-file
locations.

Return to the [HyprShelld User Guide](index.md).
