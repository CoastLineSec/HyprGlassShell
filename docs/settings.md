# Settings

HyprShelld Settings provides one place to adjust the bar and its workspace
switcher, and to choose which shell components are enabled.

## Open Settings

Open **HyprShelld Settings** from your application launcher. You can also run
`hyprshelld-settings` from a terminal. Settings remains available when the shell
services are stopped, so you can inspect their state and reach recovery
information without the bar running.

Select **Bar** in the left sidebar to see a desktop preview and change the bar
height or workspace switcher. The preview uses illustrative workspaces and
applications rather than reading or controlling your current session.

Select **Components** to manage installed shell features. The page keeps four
categories in a stable order: **Bar Widgets**, **Desktop Widgets**,
**Services**, and **Shell Applications**. Built-in components are clearly
marked and can be enabled or disabled, but they cannot be removed. Their
detailed choices remain in the natural Settings page for that feature rather
than being duplicated on the Components page.

Select **Install from file…** to inspect a local `*.hyprshelld-component`
package. HyprShelld checks the package structure and integrity before showing
its name, author, runtime, requested capabilities, dependencies, and digest.
This does not make third-party code trusted or audited: the review clearly
marks it as unverified, and you should install only code you have inspected and
trust. A newly installed component remains disabled. Selecting another local
package with the same component ID uses the same review flow for an update,
reinstall, or downgrade; HyprShelld does not download packages automatically.
Installing a new or changed digest does not enable, grant, or place it. An exact
reinstall of bytes you previously reviewed may make the same retained,
digest-pinned enabled state and placements effective again.

A newly installed compatible data-only bar widget shows **Add to Bar** until it
has a placement. This performs one atomic settings change: it records the exact
installed package digest, applies the widget's component-setting defaults,
creates one enabled instance, and appends that instance to the end of the main
bar. Once placed, the row shows the normal global enable switch. Turning that
switch off preserves the instance, its settings, and its bar position. Calling
the add operation again cannot create a duplicate placement. Version-one
data-only widgets receive no capabilities and may not declare dependencies;
packages that request either remain installed but disabled. If one preserved
instance exists in an inactive older layout, **Add to Bar** moves that same
instance to the main bar instead of duplicating it. Settings refuses to guess
when multiple preserved instances make the target ambiguous.

When an update or downgrade changes the package digest of a previously
configured widget, the row instead shows **Use Installed Version**. That
separate action adopts the exact catalog-reviewed digest and new trusted
defaults, forces the component off, clears grants, and preserves every existing
instance and placement. It never activates the changed package. Afterward, the
normal switch or **Add to Bar** is a second explicit action, depending on
whether the preserved instance is already in the main bar.

Third-party bar widgets, desktop widgets, and services may provide a data-only
settings schema. Settings renders those shared choices with trusted built-in
controls; packages cannot supply QML or JavaScript for the Settings interface.
Choices belonging to one widget instance are changed where that instance is
placed. Shell applications keep their settings inside the application. Removing
a third-party package preserves its saved choices and placements as dormant
recovery data in case the same package is installed again, and never deletes
the original package file you selected.

If an installed package receipt or file tree is damaged, HyprShelld leaves that
component out of the catalog while keeping healthy components available. Select
the same trusted package file again to repair corrupt contents or a missing tree
under the component store's safe directory structure. Unsafe symlinks or other
non-directory storage entries are rejected and require manual removal. Component
settings, including local file or directory choices, are stored in owner-only
configuration files.

## Change the bar height

Drag the **Bar height** slider to preview a new size. The preview updates while
you move the slider. When you release it, HyprShelld saves the selected height
and applies it to every bar automatically.

Select **Reset** to return to the default height. Reset is available only when
the current height differs from the default.

While the new height is being saved, the size control is briefly unavailable.
If the save fails, Settings displays a bar-size warning above the preview and
keeps the last successfully saved value. Workspace controls remain independent
of a bar-size save or error.

## Customize the workspace switcher

Use the **Workspaces** card on the Bar page to choose:

- whether workspace numbers or named-workspace initials appear inside the
  circles, which is enabled by default;
- whether full custom or named workspace names appear beside the circles,
  without repeating a numeric workspace's number;
- whether application icons are appended to each workspace anchor;
- a limit of one to five visible application icons before a `+N` summary;
- whether empty workspaces are hidden while the current workspace remains
  visible; and
- whether scrolling over the switcher is off, normal, or reversed.

The identifier and name switches are independent, so the preview can show
identifiers, names, both, or neither. The filled current circle and smaller
hollow inactive circles remain in every combination.

An existing Numbers, Compact, or Names selection from an earlier development
build is converted to the equivalent pair of switches without changing the
other workspace choices.

When application icons are enabled, repeated applications on an inactive
workspace are grouped and show their window count. The current workspace keeps
its windows individually represented and emphasizes the active one. Selecting a
window icon on the current workspace brings that window forward. Icons on
inactive workspaces are not hidden-window focus controls; selecting their area
switches to that workspace.

Normal and reversed scrolling move between the real workspaces currently shown
on that display. Scrolling stops at the first or last workspace and never wraps
around. The bar shows only workspaces reported by Hyprland, including empty
workspaces configured to remain persistent; it does not add unused places.

Workspace switcher changes are saved automatically as one atomic set and
applied to every bar. While that set is being saved, only the **Workspaces**
card is briefly unavailable. Select **Reset** in the card to show workspace
identifiers, hide full workspace names and application icons, show empty
persistent workspaces, and turn scrolling off. The maximum icon setting returns
to three.

To remove the switcher from the bar, open **Components**, find **Workspace
Switcher** under **Bar Widgets**, and turn it off. Its saved choices are
preserved. The **Workspaces** card remains in the Bar page, but its controls and
Reset button are dimmed and unavailable beneath this message:

> This feature has been disabled. Enable it from Components → Bar Widgets to
> change these settings.

The illustrative preview also omits the workspace switcher while it is
disabled. Turning the component back on restores the same saved choices in the
card and preview.

## Desktop health and recovery

The status at the bottom of the sidebar reports the health of the HyprShelld
desktop. When a component cannot recover automatically, Settings keeps a
warning visible and identifies the affected part in plain language.

Select **Restart** beside an affected component to request recovery. The
button shows **Restarting…** while the request is being submitted. The warning
remains until HyprShelld confirms that the component is running again; an
accepted restart request alone does not clear it.

Third-party widget activation failures are isolated from service restarts. If
a widget does not complete its trusted-renderer stability window, HyprShelld
quarantines that exact package digest and removes it from the active surface
plan. An interrupted activation is treated conservatively; it does not prove
that package data caused the interruption. The component row explains that
activation did not complete and offers **Try Again**. Retrying clears only that
digest's quarantine; saved settings and bar placement remain intact.

If the shell health service is unavailable, Settings reads the current service
states directly from systemd. This fallback is read-only, so restart controls
remain unavailable until the health service reconnects. If service state also
cannot be read, Settings displays **Status unavailable** rather than assuming
that the desktop is healthy.

## When settings cannot be saved

Bar size and component settings use separate persistence and recovery areas.
The Bar page reports them separately: a component-settings or catalog failure
does not make the core bar-size setting read-only. Both interfaces are served
by the configuration service, so a complete core service outage can make both
areas temporarily unavailable:

- A **bar size settings** warning disables only the height control. Its
  displayed value may be stale until core settings reconnect.
- A **workspace settings** warning disables only the **Workspaces** card. It
  appears when component settings or the catalog are unavailable or read-only,
  or when the expected built-in workspace-switcher record or placement cannot
  be read. This unavailable warning is distinct from the intentional disabled
  message.
- A newer unsupported component-settings file is preserved rather than
  overwritten. When the active choices are still readable but their recovery
  copy uses the newer format, the choices remain visible but read-only.

The preview remains deterministic and illustrative while either area is
unavailable. During a temporary service loss, it can retain the last trusted
workspace presentation; that retained preview does not mean its controls are
writable. These configuration warnings describe whether choices can be saved;
the sidebar status describes the health of the complete desktop.

You can leave Settings open while waiting. It detects each service when it
becomes available again, refreshes that area's displayed values, and re-enables
only the controls that are safe to change. Avoid changing configuration files
while their service is unavailable.

## Configuration recovery messages

HyprShelld keeps separate active and last-known-good copies for core settings
and component settings. Each area checks its own copies when the configuration
service starts, so damage in the workspace component state does not block a bar
height change.

- **Restored from the last known good copy** identifies the affected area. Its
  main file was missing or damaged, but HyprShelld recovered the previous valid
  value and repaired that area's active copy. Review the displayed bar height
  or workspace choices before continuing.

- **Safe defaults are in use** means neither copy for the named area could be
  recovered. HyprShelld created valid replacements and loaded that area's
  defaults. The other settings area is not reset.

On a standard Linux setup, core settings use
`~/.config/hyprshelld/settings.json` and
`~/.local/state/hyprshelld/settings.last-good.json`. Component settings use
`~/.config/hyprshelld/components.json` and
`~/.local/state/hyprshelld/components.last-good.json`. Systems with custom XDG
paths may store them elsewhere.

If a recovery message keeps returning, close Settings and preserve the active
and last-known-good file named above for the affected area before seeking help.
Do not delete or edit either file as a first troubleshooting step; doing so can
remove the copy HyprShelld would otherwise use to recover your choices.

See [Bar](bar.md) for its current layout, workspace behavior, spacing, and
complete height range.

Return to the [HyprShelld User Guide](index.md).
