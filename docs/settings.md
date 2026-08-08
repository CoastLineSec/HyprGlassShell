# Settings

HyprShelld Settings currently provides one place to adjust the bar.

## Open Settings

Open **HyprShelld Settings** from your application launcher. You can also run
`hyprshelld-settings` from a terminal. Settings remains available when the shell
services are stopped, so you can inspect their state and reach recovery
information without the bar running.

Select **Bar** in the left sidebar to see a desktop preview and change the bar
height.

## Change the bar height

Drag the **Bar height** slider to preview a new size. The preview updates while
you move the slider. When you release it, HyprShelld saves the selected height
and applies it to every bar automatically.

Select **Reset** to return to the default height. Reset is available only when
the current height differs from the default.

While a change is being saved, the controls are briefly unavailable. If the
save fails, Settings displays the error beside the control and keeps the last
successfully saved value.

## Desktop health and recovery

The status at the bottom of the sidebar reports the health of the HyprShelld
desktop. When a component cannot recover automatically, Settings keeps a
warning visible and identifies the affected part in plain language.

Select **Restart** beside an affected component to request recovery. The
button shows **Restarting…** while the request is being submitted. The warning
remains until HyprShelld confirms that the component is running again; an
accepted restart request alone does not clear it.

If the shell health service is unavailable, Settings reads the current service
states directly from systemd. This fallback is read-only, so restart controls
remain unavailable until the health service reconnects. If service state also
cannot be read, Settings displays **Status unavailable** rather than assuming
that the desktop is healthy.

## When settings cannot be saved

The Bar page displays a separate warning when it cannot reach the configuration
service. Values shown at that time may be stale, and changes remain disabled
until the connection returns. This warning describes whether settings can be
saved; the sidebar status describes the health of the complete desktop.

You can leave Settings open while waiting. It detects the service when it
becomes available again, refreshes the displayed value, and re-enables the
controls. Avoid changing configuration files while the service is unavailable.

## Configuration recovery messages

HyprShelld keeps a last-known-good copy of its settings and checks both copies
when the configuration service starts.

- **Restored from the last known good copy** means the main settings file was
  missing or damaged, but HyprShelld recovered your previous valid settings and
  repaired the main copy. Review the displayed bar height before continuing.

- **Safe defaults are in use** means neither copy could be recovered.
  HyprShelld created valid replacement files and loaded the default bar height.
  Review the result and choose your preferred height again if needed.

On a standard Linux setup, the active settings are stored in
`~/.config/hyprshelld/settings.json`, and the recovery copy is stored in
`~/.local/state/hyprshelld/settings.last-good.json`. Systems with custom XDG
paths may store them elsewhere.

If a recovery message keeps returning, close Settings and preserve both files
before seeking help. Do not delete or edit either file as a first
troubleshooting step; doing so can remove the copy HyprShelld would otherwise
use to recover your choices.

See [Bar](bar.md) for its current layout, spacing, and complete height range.

Return to the [HyprShelld User Guide](index.md).
