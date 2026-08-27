# Settings

HyprShelld Settings provides one place to adjust the bar and its workspace
switcher; change common window appearance, input, layout, grouping, focus, and
workspace behavior; configure connected displays; edit active shortcuts,
submaps, per-device profiles, environment variables, and permissions; browse
the complete pinned Hyprland scalar catalog; and choose which shell components
are enabled.

## Open Settings

Open **HyprShelld Settings** from your application launcher. You can also run
`hyprshelld-settings` from a terminal. Settings remains available when the shell
services are stopped, so you can inspect their state and reach recovery
information without the bar running.

Select **Bar** in the left sidebar to see a desktop preview and change the bar
height, shared spacing, shared border, or workspace switcher. The preview uses
illustrative workspaces and applications rather than reading or controlling
your current session. It
remains visible while the Bar setting cards scroll beneath it.

Select **Hyprland** for the compositor configuration hub. Its cards route to
Appearance, Input, per-device profiles, Windows & Layout, Displays, Workspaces,
Rules, Shortcuts & Submaps, Environment, Permissions, and the categorized
scalar catalog. The catalog contains every supported writable scalar from the
pinned 0.56.2 inventory, grouped into Appearance, Input, Windows, Shortcuts,
System, and Session rather than one generic Advanced page. Search and module
filters work across the same trusted metadata used by the Lua renderer.

Select **Appearance** to prepare one aggregate draft across local **Visuals**
and **Animations** tabs. Visuals contains 40 reviewed border, spacing, blur,
shadow, inactive-window dimming, active/inactive/true-fullscreen opacity, and
contextual dimming choices. Detailed blur controls cover size, passes, opacity
handling, optimized and X-ray behavior, special workspaces, pop-ups, and input
methods, plus exact brightness, contrast, noise, vibrancy, and dark-area
vibrancy modulation. Turning blur or a contextual blur switch off preserves its dependent
values. Blur size is stored and rendered over the full 0-100 range. Blur passes
are stored from 0-10, while Hyprland limits the effective pass count to 1-8.
The default-on **Include borders in window shadows** choice sizes a shadow from
the outside edge of each window's effective visible border. Its value remains
saved while window shadows are off.
The **Window shadow rendering** card adds an integer range from 0 through 100
layout pixels, default 4; an integer soft-shadow falloff power from 1 through
4, default 3; default-off sharp edges; exact shadow scale from 0 through 1,
default 1; and exact horizontal and vertical
offsets from -250 through 250 layout pixels, both defaulting to 0. Positive
offsets move right or down, while negative offsets move left or up. Every value
remains saved while **Window shadows** is off. Range still controls the extent
of a sharp shadow; sharp edges make only the soft falloff dormant, while scale
and both offset components remain available for sharp shadows. A scale of 0
makes the shadow invisible; lower nonzero values shrink its expanded box around
the center before the offsets are applied. Turning sharp edges off
restores the falloff unchanged. **Save & apply** activates these settings
through the same verified Hyprland Reload as the rest of the Appearance draft.
The default-off **Inner window glow** controls add range from 0 through 100,
default 10, and falloff power from 1 through 4, default 3. Range remains
editable while glow is off; falloff remains saved but dormant. Disabled ranges
from 0 through 9 are retained exactly, but the glow switch cannot be turned on
until range reaches 10. A loaded enabled low-range state remains visible and
repairable by turning glow off or raising range. Settings does not clamp,
reset, or silently disable it. **Save & apply** remains blocked for an unsafe
draft. **Retry apply** remains blocked for an unsafe saved revision even when
the current unsaved draft would repair it. Glow colors are not edited on the
Appearance page.
**Window corner power** is a direct compositor-only exact-decimal value from 2
through 10. It remains editable while the shared border width and radius are
synchronized or the global radius is zero because a Window Rule can still
provide a radius. Hyprland scales the effective radius with this value; the
preview reports it but does not simulate it. Fractional rendered curves use an
integer approximation for curved-corner pointer and resize hit testing, and an
already-closing window in Hyprland 0.56.1 may finish its fade with the prior
power.
Dimming strength is editable only while inactive dimming is enabled, and
turning the switch off preserves that retained value. Every 0-to-1 slider uses
0.05 steps; an accepted off-grid saved value remains exact until that slider
is moved. The five modulation fields, window corner power, and shadow scale
instead accept exact bounded plain-decimal
entry with no slider step or display rounding. Invalid text remains in the
local draft and blocks Save until it is corrected, discarded, reset, or
replaced by an explicit load-current action. Surface opacity and the black
dimming scrim are independent. The
pop-up and input-method alpha cutoffs accept and retain values from 0 through
1, and live mapped surfaces use the stored values directly. Pop-up snapshot
and fade-out capture use a minimum cutoff of 0.01; only on that capture path
can an owning layer's Rule `ignorealpha` value replace the global pop-up
cutoff. A Window Rule can
suppress only the inactive contribution for a matching window and can
separately change the resulting per-window opacity; it does not suppress
modal-parent, special-workspace, or Dim-around dimming. Contextual values
remain saved while
their triggering state is absent, and Dim around takes effect only when a
Window or Layer Rule enables it. On Hyprland 0.56.1, changing either dimming or
blur for a special workspace while one is already open takes effect after that
workspace is closed and reopened. Animations can tune the
parameters and ordering of existing custom curves and fully author the fixed
animation-rule tree; curve names, types, creation, and removal stay unavailable
until HyprShelld has a verified compositor-restart workflow. Turning animations
off retains the detailed values and leaves them inspectable. In the pinned
preview, the original tile moves from active toward inactive opacity and gains
the independent inactive-dimming scrim in proportion to the illustrative
second window's opening progress; the stable one-window state has no inactive
tile. The preview does not simulate true fullscreen, modal-parent,
special-workspace, or Dim-around states, blur size or effective pass clamping,
X-ray eligibility, pop-up or input-method transparency cutoffs, window corner
power, custom curve shapes, speeds, styles, or rule order. It also does not
render brightness, contrast, noise, vibrancy, dark-area vibrancy, or
border-inclusive shadow bounds. Shadow range, falloff, sharp edges, scale, both shadow-offset
components, and the exact inner-glow enabled/range/falloff trio are summary-only
and do not change the illustrative geometry or position. Inner-glow size,
falloff, color, opacity, blur, and motion are not simulated. The accessible
text alternative reports their exact values and states, including an invalid
scale or offset component, the other exact draft values or their invalid state,
and the border-in-shadow choice.

**Save & apply** first persists one validated desired-state revision, then
reloads and verifies that exact revision. The page preserves a draft if the
authority changes elsewhere and never silently rebases it.
Appearance requires the compositor takeover described below, but never starts
takeover itself. See [Appearance](appearance.md) for the complete controls,
reset behavior, retry, and whole-compositor recovery behavior.

Select **Input** for local **Global**, **Devices**, and **Gestures** tabs.
Global prepares the reviewed keyboard, virtual keyboard, mouse, pointer
behavior, cursor visibility and placement, touchpad, touch-device,
drawing-tablet, and advanced scrolling values. Cursor controls cover
event-triggered hiding, an inactivity timeout, display-layout edge padding,
ordinary jump suppression, remembered per-window positions, and restoration
of the last physical-mouse position after non-mouse input. Touch-device and
drawing-tablet values are global fallbacks for compatible libinput-backed
hardware; an exact saved per-device override wins.
Their controls do not claim that hardware is present or supports a setting.
Devices shows the latest authenticated, read-only observation of the current
Hyprland session and compares it with preserved saved device records without
editing or reassigning them; **Refresh** requests another observation. Device names are
session-assigned diagnostics rather than stable hardware identities. Gestures
creates and orders touchpad bindings through nine fixed first-party action
forms; compatibility-only records remain exact and read-only. Global values
and Gestures share one draft, one conflict state, and one atomic save. Global
touchpad and compatible-device controls remain visible without hardware
detection and apply when supported hardware is present. Input has no simulated
preview because one could not verify real compositor, device, or physical
gesture behavior. It uses the same whole-snapshot save, exact
Apply, Retry, and cancel-first Recovery authority as Appearance. See
[Input](input.md) for gesture actions, compatibility records, device-status
wording, selector limits, defaults, dependencies, and recovery behavior.

Select **Hyprland > Devices** to author the ordered saved per-device collection
separately from those diagnostics. The editor exposes all 39 pinned override
fields, emits only selected overrides, validates the complete collection, and
saves it atomically without changing global Input values.

Select **Hyprland > Environment** or **Permissions** for their ordered managed
collections. Environment records use typed process or UWSM targets; records
whose UWSM publication path is unavailable remain preserved but cannot be
saved deceptively. Permission records expose exact regex identity, request
class, and allow/deny/ask decisions. Both editors provide add, edit, remove,
reorder, conflict, discard, reset, and atomic-save workflows.

Select **Displays** to inspect the connected-output topology and prepare changes
to resolution, refresh rate, scale, orientation, arrangement, mirroring, and
advanced display properties. This page reads the live topology. It does not
change the running session until you select **Test changes**. Each live test
lasts 15 seconds and must be kept explicitly or it is reverted automatically.
Before the first change, Settings asks for explicit permission to replace the
Hyprland entrypoint with HyprShelld's managed loader. The existing entrypoint is
preserved for recovery but is not imported. Read [Displays](displays.md) before
selecting **Take control**.

Select **Windows & Layout** to prepare one draft for the default layout,
single-window proportions, floating-window spacing, Dwindle, Master, and
Scrolling engine behavior, window grouping, group-bar visibility, interaction,
geometry and typography, pinned-window fullscreen requests, resizing, snapping,
pointer and directional focus behavior, tiled Window Rule size limits,
an exact focus-movement threshold, application activation and
fullscreen-or-maximized focus handoff, process-parent window swallowing, and
unresponsive-application dialog handling. Its
pinned layout preview reuses the four illustrative Dwindle, Master, Scrolling,
and Monocle stories; engine details, spacing, fullscreen, focus, grouping,
group-bar appearance or interaction, and swallowing are not simulated. The
local engine tabs do not change the default layout. See
[Windows & Layout](windows-layout.md) for every group and dependency.

Select **Workspaces** to prepare one draft for workspace transition and history
behavior, cross-output and post-switch pointer placement, initial application
placement tracking, touchpad swipe response, optional touchscreen edge swipes,
special-workspace close behavior, and ordered typed Workspace Rules. Behavior
and user rules share one atomic save and one sidebar **Unsaved** or **Review**
state. The protected maximized-window rule is kept internal and cannot be
edited from Settings. This page is different from the Bar page's
switcher-presentation card and has no simulated preview. See
[Workspaces](workspaces.md) for the controls, defaults, pointer-warp
interactions, and gesture and rule workflow.

Select **Shortcuts & Submaps** to edit the active managed binding and submap
collections. The page provides ordered shortcut and modal-submap CRUD, reviewed
actions, every supported bind option, device filters, validation, visual chord
previews, and an atomic save. It never substitutes arbitrary commands for the
reviewed action catalog. See [Shortcuts and submaps](keyboard-shortcuts.md) for
the complete workflow. Binding and submap changes remain restart-classified,
so a successful save can correctly report that activation needs a verified
compositor restart.

Select **Rules** to create ordered Window Rules and Layer Rules with fixed,
typed match and effect controls. The two tabs share one complete draft and are
saved atomically. New rules begin disabled and empty, and no raw JSON, command,
or generic rule language is exposed. See [Window and Layer Rules](rules.md)
for the authored fields, RE2 validation boundary, ordering, and recovery flow.

Select **Advanced** for reviewed session-lock recovery and rendering,
compositor fallback-background, capture, workspace underlay rendering, SDR
work-buffer transfer, direct scanout, native Wayland resize compatibility,
XWayland compatibility, input-capture protocol, and display-warning controls.
The page
can admit another session-lock client while Hyprland already considers the
session locked, set the incomplete-lock grace period, keep ordinary
workspaces rendering beneath lock surfaces, retain dependent lock blur, hide
Hyprland's fallback image or its independent splash text, control
Rule-qualified render-unfocused frame callbacks, prefer 8-bit formats for new
screen-share sessions, skip the active-workspace fallback-background,
layer-shell background and bottom, and post-wallpaper passes, choose between
**Display transfer (default)** and **Linear** for SDR content in Hyprland's
internal FP16 or ICC work buffer,
choose whether an otherwise-eligible exact-fullscreen solitary DMA surface may
use direct scanout, extend a temporarily undersized submitted buffer across its
native Wayland surface mapping, choose
nearest-neighbor filtering for scaled or transformed X11 surfaces, choose
whether later keyboard modifier-state changes are forwarded to an authorized
active input capture, reject or admit each newly requested invalid capture
barrier, and hide the clean-divisor scale warning. These sixteen values share
one draft and one atomic save. Workspace underlay rendering, SDR work-buffer
transfer, direct scanout, both compatibility choices, and the two
input-capture protocol choices are seven caution controls across six cards.
Underlay
skipping affects only the active-workspace render path: windows and layer-shell
top and overlay surfaces continue, the no-workspace path is unchanged, and the
setting stops neither skipped surfaces nor their processes. It also does not
clear prior buffer pixels, so uncovered pixels may be stale or undefined. The
SDR choice does not enable FP16, HDR, or ICC or change the output transfer
function or color profile. It may remain
dormant on ordinary sRGB output without an ICC profile, while HDR-like paths
stay linear regardless.
Direct scanout defaults to Disabled; Enabled permits eligible surfaces, while
Automatic additionally requires game content. Activation is never guaranteed,
and eligible scanout bypasses normal composition. Overlays such as lock-screen
or notification surfaces, capture, mirrored outputs, a software cursor, and
incompatible buffers, transforms, or color management can prevent it; Settings
does not prove display or GPU support. Texture extension does not resize the
client buffer, window, or display or make the application respond sooner; X11
is unaffected, some scale-unaware and misaligned-fullscreen correction paths
bypass it, and disabling it may expose unfilled or stale-size edges. For the
XWayland choice, native Wayland windows are unchanged, and a per-window
**Nearest-neighbor scaling** Rule can still enable the filter while the global
choice is off. The input-capture choices do not grant permission, create or
enable a capture session, or release an active one. They apply only to later
modifier events and new barrier requests: changing modifier forwarding does
not synthesize or retract held modifier state, and changing barrier enforcement
does not recheck, repair, or remove existing barriers. The two choices are
independent. The page has no illustrative preview. See
[Advanced compositor settings](advanced.md) for
defaults, ranges, exact runtime limits, conflicts, retry, and whole-compositor
recovery.

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
If the save fails, Settings displays a bar-size warning in the scrolling
settings area beneath the pinned preview and keeps the last successfully saved
value. Workspace controls remain independent of a bar-size save or error.

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

Core Bar settings and component settings use separate persistence and recovery areas.
The Bar page reports them separately: a component-settings or catalog failure
does not make the core Bar settings read-only. Both interfaces are served
by the configuration service, so a complete core service outage can make both
areas temporarily unavailable:

- A **Bar settings** warning disables the height, shared-spacing, and
  shared-border controls.
  Their displayed values may be stale until core settings reconnect.
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

Compositor and display settings use a third, independent recovery domain. Their
desired and last-known-good documents normally live at
`~/.local/state/hyprshelld/compositor/desired.json` and
`~/.local/state/hyprshelld/compositor/last-good.json`. The managed Hyprland
loader, immutable generations, ownership record, preserved pre-takeover
entrypoint, and `user-custom.lua` are deliberately not stored in the core or
component settings files. See [Displays](displays.md) for their roles and
locations.

Appearance, Input, Windows & Layout, Workspaces, Rules, and Advanced share this
compositor recovery domain.
If a managed settings save is persisted but cannot be activated, **Retry apply** targets the
exact saved compositor revision regardless of which page initiated it.
**Restore last working configuration** is broader: after a separate
cancel-first confirmation it can replace all pending compositor settings, not
only the page where it was opened. See
[Appearance](appearance.md), [Input](input.md),
[Windows & Layout](windows-layout.md), [Workspaces](workspaces.md),
[Window and Layer Rules](rules.md), and [Advanced](advanced.md) before using
that recovery action.

Core settings also own HyprShelld's shared border geometry and inner and outer
spacing. The Bar consumes both groups directly. Compositord mirrors the border
width and radius and the normal window gaps into managed Hyprland when each
independent synchronization choice is enabled. The corresponding Appearance
values remain visible but read-only and can be separated with **Override
window borders** or **Override window spacing**. Core recovery never requires
the two services to overwrite one another's persistence files; compositord
reconciles from Config1's verified projection.

Core settings also persist the shell appearance preference as exactly one of
`automatic`, `light`, or `dark`. `automatic` follows Qt's current desktop color
scheme at runtime; the other two modes remain explicit. Existing version 1–3
core snapshots migrate to `dark` without advancing their revision, preserving
the presentation used before the preference was introduced.

If a recovery message keeps returning, close Settings and preserve the active
and last-known-good file named above for the affected area before seeking help.
Do not delete or edit either file as a first troubleshooting step; doing so can
remove the copy HyprShelld would otherwise use to recover your choices.

See [Bar](bar.md) for its current layout, workspace behavior, spacing, and
complete height range. See [Appearance](appearance.md) for visual window
settings, [Input](input.md) for keyboard, mouse, cursor visibility and
placement, touchpad, touch-device, and drawing-tablet settings, and
[Windows & Layout](windows-layout.md) for layout, grouping, group bars, resize,
snap, fullscreen, focus, and window-swallowing behavior, and
[Workspaces](workspaces.md) for workspace switching, history, pointer placement,
and gesture behavior. See
[Window and Layer Rules](rules.md) to create typed ordered rules. See
[Advanced](advanced.md) for session-lock recovery and rendering, compositor
fallback rendering, Rule-qualified frame callbacks, screen-share format
preference, workspace underlay rendering, SDR work-buffer transfer, direct
scanout, native Wayland resize compatibility, XWayland filtering,
input-capture protocol, and display-warning controls. See
[Displays](displays.md) before allowing HyprShelld to manage the compositor
entrypoint.

Return to the [HyprShelld User Guide](index.md).
