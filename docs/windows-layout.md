# Windows & Layout

The **Windows & Layout** page manages a reviewed set of Hyprland window
placement and interaction settings. It owns the default layout, layout-engine
behavior, floating-window spacing, window grouping, resizing, snapping, and
focus. It also controls pinned-window fullscreen requests, directional focus
and tiled-window movement, tiled Window Rule size limits, application
activation requests, fullscreen-or-maximized focus handoff,
window swallowing, unresponsive-application dialogs, and group-bar visibility,
interaction, geometry, and typography. This page does not configure group-bar
colors. Other visual style remains on **Appearance**, and workspace navigation
remains on **Workspaces**.

These are desired managed values rather than a reading of every value effective
in the running session. The user-owned `user-custom.lua` loads after managed
modules and can override them. The page becomes editable only after HyprShelld
manages the Hyprland entrypoint and has a current verified baseline. If it is
unmanaged, open **Displays** and review the takeover confirmation first.

## Choose the default layout

**Default layout** selects the layout used when a workspace has no more
specific workspace rule:

- **Dwindle**, the default, recursively divides the available area;
- **Master** keeps a dominant window beside a stack;
- **Scrolling** arranges windows in a moving column strip; or
- **Monocle** gives one tiled window the complete available area.

The pinned preview is illustrative. It reuses one reviewed opening, settling,
and closing story for each layout, but it does not inspect, move, or resize live
windows. It also does not simulate detailed engine, spacing, grouping, or
group-bar values. Use **Pause motion** or **Play motion** to control it. The
preview does not simulate detailed focus or window-group behavior either.
It does not simulate fullscreen or window-swallowing behavior.
Turning **Animations** off on Appearance stops the loop and leaves a stable
preview.

The **Dwindle**, **Master**, and **Scrolling** tabs farther down the page choose
which engine's settings are visible. They do not change **Default layout**.
Engine settings remain useful when a workspace rule selects that engine.

## Use fullscreen with pinned windows

**Allow pinned windows to go fullscreen** is off by default. When enabled, a
later fullscreen request can temporarily unpin a pinned window. Leaving
fullscreen restores that window's pinned state. Changing the setting does not
immediately unpin, fullscreen, or otherwise rearrange any window.

## Constrain a single tiled window

**Apply size rules to tiled windows** is off by default. When enabled, matching
Window Rule minimum and maximum sizes apply to tiled windows as well as
floating windows. A constrained tiled window is centered inside its assigned
tile without redistributing the surrounding layout. The maximum is ignored
while that window is fullscreen or maximized. Changing this value affects
later layout calculations; it does not resize a window immediately.

**Single-window aspect ratio** accepts a width and height from 0 through 1000.
Set both to 0, the default, to turn the constraint off. Set both above 0 to
choose a ratio such as 16 by 9. A draft with only one side set to 0 remains
editable so it can be repaired, but **Save & apply** stays unavailable until
both sides are 0 or both are positive.

The fields preserve valid decimal values. **Aspect-ratio tolerance** ranges
from 0 through 1 and defaults to 0.1; it leaves small differences alone until
the adjustment exceeds that share of the available area.

## Set floating-window and transition spacing

**Floating-window edge gaps** provides four signed-integer fields in the order
top, right, bottom, left. Positive values add space and negative values let a
floating window extend toward an edge. The default is 0 on every edge. The
fields form one value, so Reset and Save keep their order intact.

**Gap between animated workspaces** ranges from 0 through 100 layout pixels
and defaults to 0. It adds separation during workspace transitions and stacks
with ordinary outer gaps.

## Tune Dwindle

Open the **Dwindle** tab to configure:

- the new-window split ratio, 0.1 through 1.9 with an even-split default of 1;
- pointer-position split selection, off by default;
- forced split direction, automatic by default;
- whether the active window is preferred for splits, on by default;
- whether split-ratio bias follows the split direction or current window;
- the horizontal-split width threshold, 0.1 through 3 with a default of 1;
- preserved split orientation and persistent manual preselection, both off;
- pointer-aware resizing, on by default;
- precise pointer-based drag placement, off by default; and
- special-workspace scale, 0 through 1 with a default of 1.

Choosing splits from the pointer position makes **Forced split direction**
unavailable without discarding its draft value.

## Tune Master

Open the **Master** tab to configure:

- master position at the left, right, top, bottom, or center; left is default;
- master-area share from 0 through 1, default 0.55;
- the new-window role: master, stack (default), or inherit;
- ordinary placement or insertion before or after the focused stack window;
- adding new windows at the start of a stack, off by default;
- pointer-position drop placement, on by default;
- focusing the master after another window closes, off by default;
- allowing small multi-master splits, off by default;
- retaining master position with one tiled window, off by default;
- pointer-aware resizing, on by default;
- special-workspace scale from 0 through 1, default 1; and
- the centered-master stack threshold, fallback side, and reserved-edge rule.

The three centered-master controls are available only when **Master position**
is **Center**. Their values are preserved in every other position. The default
threshold is 2 stack windows, the fallback is left, and reserved side edges are
respected.

## Tune Scrolling

Open the **Scrolling** tab to configure:

- whether columns progress left, right (default), up, or down;
- default column width from 0.1 through 1, default 0.5;
- filling a workspace that contains one column, on by default;
- following ordinarily focused columns, on by default;
- the minimum visible share before following, 0 through 1, default 0.4;
- centering or fully fitting a newly focused column, with Fit as the default;
- wrapping focus between the first and last columns, on by default;
- wrapping columns moved past either end, on by default;
- settling move gestures on the nearest column grid, on by default; and
- moving the pointer after a gesture changes focus, on by default.

**Minimum visible share** is available only while focused columns are followed.
Turning that behavior off preserves the threshold.

## Control window groups

The **Window groups** card controls future grouping actions. Changing its
values does not rearrange groups that already exist. Window rules and group
locks can still prevent an otherwise eligible window from joining a group,
except where the explicit lock-bypass choice applies.

Under **Joining and leaving**:

- **Automatically group new windows** adds eligible new windows to the focused
  unlocked group. It is on by default.
- **Place new members after the active window** inserts a window beside the
  active group member instead of at the end. It is on by default and also
  applies to windows added manually or by dragging, so it remains available
  when automatic grouping is off.
- **Focus windows moved out** keeps focus on a window after it leaves a group
  instead of focusing the group's remaining active window. It is on by default.
- **Join the destination's only group** is off by default. When enabled, a
  future move-to-workspace action adds the moved window to an unlocked group if
  the destination has one visible window and that window belongs to the group.
- **Let movement bypass group locks** is off by default. It lets supported
  move-into-group, move-out-of-group, and move-window-or-group actions bypass
  global and per-group locks. It does not unlock the group or bypass locks for
  every group operation.

Under **Dragging and merging**:

- **Drag windows into groups** can be Disabled, accept drops on a grouped
  window or its group bar (the default), or accept drops only on the group bar.
- **Merge dragged groups** allows an entire dragged group to join another
  group. It is on by default.
- **Merge groups through the group bar** permits that merge when the source is
  dropped on the target group bar. It is on by default and additionally
  requires **Merge dragged groups**.
- **Merge floating windows into tiled groups** permits a floating window
  dropped on a tiled group bar to join that group. It is off by default. For
  an individual ungrouped floating window, this does not require either
  group-merge switch.

Turning **Drag windows into groups** off makes its three dependent switches
unavailable without discarding their values. **Merge groups through the group
bar** and **Merge floating windows into tiled groups** additionally require
group bars to be shown. Group-bar choices take effect only while a group bar is
visible. The illustrative layout preview does not simulate any grouping
operation.

## Configure group bars

Group bars are Hyprland window decorations for grouped windows. The **Group
bars** card provides:

- **Show group bars**, on by default;
- **Hide one-member group bars**, off by default and available with Hyprland
  0.56.0 or newer;
- **Switch members by scrolling**, on by default; and
- **Middle-click to close**, on by default.

Window decoration rules can still hide a group bar. Turning **Show group bars**
off makes the other 26 group-bar controls unavailable without discarding their
draft values. It also makes the two group-bar-specific drop choices in
**Window groups** unavailable, while ordinary grouped-window drops and
**Merge dragged groups** remain editable.

### Arrange the decoration

The **Group-bar layout** card controls:

- **Stack members vertically**, off by default;
- **Title and background height**, 1 through 64 layout pixels, default 14;
- **Indicator height**, 1 through 64 layout pixels, default 3;
- **Indicator-to-title gap**, 0 through 64 layout pixels, default 0;
- **Space between horizontal members**, 0 through 20 layout pixels, default 2;
- **Gap from window or stacked rows**, 0 through 20 layout pixels, default 2;
- **Keep space above**, on by default;
- **Indicator corner radius**, 0 through 20 layout pixels, default 1;
- **Indicator corner power**, 2 through 10 in 0.1 steps, default 2; circular
  corners start at 2 and higher values make the curve progressively squarer;
- **Round only outer indicators**, on by default; and
- **Decoration priority**, 0 through 6, default 3. Hyprland evaluates
  higher-priority decorations first.

Horizontal member spacing is unavailable while members are stacked. **Keep
space above** is unavailable when the outer gap is 0. The indicator corner
power and edge-only choice are unavailable when the indicator radius is 0.
**Title and background height** is unavailable only when both titles and
gradient backgrounds are off. These states retain their draft values.

### Set title typography

**Show window titles** is on by default. When it is on, the **Group-bar
titles** card also provides:

- **Title font family**. Leave it empty, the default, to use Hyprland's global
  `misc:font_family` value. Settings preserves the family name exactly as
  entered, including case and spaces, but does not verify that it is installed;
- **Font size**, 2 through 64 layout pixels, default 8;
- **Active-title weight** and **Inactive-title weight**, both default 400.
  Their editable numeric fields accept 0 through 2,147,483,647; 400 is normal
  and 700 is bold;
- **Horizontal title padding**, 0 through 22 layout pixels, default 0; and
- **Vertical title offset**, -20 through 20 layout pixels, default 0. Positive
  values move the title upward.

Turning titles off makes those six typography choices unavailable without
discarding their values. The family field is deliberately a stored name rather
than an installed-font picker.

### Draw group-bar backgrounds

The **Group-bar backgrounds** card provides:

- **Blur group-bar surfaces**, off by default. It applies to member indicators
  and any drawn gradient backgrounds;
- **Draw gradient backgrounds**, off by default;
- **Background corner radius**, 0 through 20 layout pixels, default 2;
- **Background corner power**, 2 through 10 in 0.1 steps, default 2; and
- **Round only outer backgrounds**, on by default.

Background geometry is unavailable until gradient backgrounds are drawn. Its
power and edge-only choice additionally require a radius above 0, and dormant
values are preserved. These controls leave group-bar colors unchanged.

The pinned layout preview does not simulate group-bar appearance or
interaction. Accurate rendering would depend on real group membership and
order, focus and lock state, per-window decoration rules, monitor scale, live
window titles, installed fonts, and the configured group-bar colors.

## Resize windows

The **Resize** card provides four choices:

- **Resize from borders and gaps** allows pointer drags in those areas to
  resize a window. It is off by default.
- **Border grab area** extends the draggable resize area by 0 to 100 layout
  pixels. The default is 15. It is available when border resizing is on.
- **Show resize cursor** changes the pointer icon over a draggable border. It
  is on by default and is available when border resizing is on.
- **Floating resize corner** can remain automatic, the default, or be fixed to
  the top-left, top-right, bottom-right, or bottom-left corner. It also applies
  to other floating-window resize paths, so it remains available when border
  resizing is off.

Turning border resizing off dims only its grab-area and cursor choices. Their
draft values are preserved.

## Snap floating windows

**Snap floating windows** is off by default. Turning it on enables:

- **Overlap adjacent borders**, off by default, to leave only one border width
  between snapped windows;
- **Monitor snap distance**, 0 to 100 layout pixels with a default of 10;
- **Respect window gaps**, off by default, to keep configured gaps at the
  snapped position; and
- **Window snap distance**, 0 to 100 layout pixels with a default of 10.

Turning snapping off dims these four dependent controls without discarding
their values.

## Choose focus behavior

The **Focus** card provides:

- **Pointer focus**: Click to focus, Follow pointer (the default), Detached
  pointer focus, or Separate pointer focus.
- **Follow pointer during drag and drop**, on by default, forces pointer focus
  mode 1 only while a protocol drag-and-drop operation is active. It does not
  apply to dragging a window. The ordinary follow threshold and per-window
  no-follow behavior still apply. The choice becomes dormant—but remains
  saved—when ordinary Pointer focus already uses Follow pointer mode.
- **Focus movement threshold** accepts an exact plain decimal from 0 through
  1,000,000 logical pixels and defaults to 0. In effective Follow pointer mode,
  including the protocol drag-and-drop override, accumulated pointer movement
  must strictly exceed the threshold before a different hovered window can
  receive ordinary focus. A value of 0 therefore still requires positive
  movement. A gap of at least 0.5 seconds between movement events resets the
  accumulator. Explicit refocus bypasses the threshold, while a matching **No
  follow mouse** Window Rule still blocks ordinary pointer focus. The field is
  available when ordinary Follow pointer mode or the drag-and-drop override is
  enabled; otherwise its exact saved value remains dormant.
- **Focus monitors under the pointer**, on by default, moves monitor focus on
  passive pointer crossing. Turning it off does not block clicks, explicit
  refocus, or focus on a hovered window from selecting that monitor.
- **Refocus hovered windows**, on by default, and **Focus dead zone**, 0 to 300
  pixels with a default of 0. Both are available only in Follow pointer mode.
- **Focus after floating changes**: keep the current focus, follow the pointer
  for tiled/floating transitions (the default), or include all floating
  transitions.
- **Focus after closing**: the next window (the default), the window under the
  pointer, or the most recently used window.
- **Honor application focus requests**, off by default, lets an application
  activation focus its window and raise it when floating; the normal
  cursor-warp policy may also move the pointer. When disabled, activation still
  marks the window urgent. A matching Window Rule can override the global
  choice, while an activation-suppression rule or an unmapped window can still
  prevent focus and pointer movement.
- **Focus request under fullscreen or maximized** chooses what happens when a
  tiled window requests focus underneath another window in either mode: Keep
  current mode, Transfer current mode, or Exit current mode (the default).
  Floating-window requests bypass this choice and are raised over the current
  mode instead.
- **Keep fullscreen or maximized after closing**, off by default, transfers the
  closing focused window's internal mode to the chosen replacement. The next
  member of the same group inherits the mode regardless of this setting.
- **Choose directional targets by**: Recent focus (the default) chooses the
  most recently focused adjacent tiled candidate. Longest shared edge chooses
  the adjacent tiled candidate sharing the greatest edge length with the
  current tiled window. Floating-window directional targeting uses a separate
  vector-angle path and is not changed by this setting.
- **Cycle fullscreen windows**, off by default, lets directional focus from
  ordinary fullscreen cycle through windows that fullscreen otherwise blocks.
  Layout-managed fullscreen modes retain their own behavior.
- **Cycle group members first**, off by default, makes left and right
  directional focus switch members inside the current group before leaving it.
- **Continue across monitors**, on by default, lets directional focus or
  tiled-window movement continue to the next monitor in that direction when no
  target remains on the current monitor.
- **Focus through special workspaces**, off by default.
- **Stop directional focus at the edge**, off by default.
- **Block modal parent interaction**, on by default.

Changing Pointer focus preserves the refocus and dead-zone values while they
are unavailable. Reloading a changed movement threshold does not itself focus
a window or reset the current accumulator. Movement collected before the
reload remains, and the new threshold is used when a later movement event next
evaluates a different hovered window; a later event gap of at least 0.5 seconds
still resets that retained accumulation. These directional choices affect
later actions; changing them does not move or refocus a window immediately.
With **Continue across
monitors** enabled, the next monitor in that direction is tried before
**Stop directional focus at the edge** ends the action.

## Swallow process-parent windows

**Swallow matching parent windows** is off by default. When it is enabled, a
newly mapped window walks up to 25 entries in its process-parent chain and
considers mapped, input-accepting ancestor windows. **Parent class pattern** is
a required, nonempty RE2 full-match pattern for those ancestors. An empty
class pattern, the default, deliberately leaves swallowing dormant even when
the switch is on.

**Parent title exception** is an optional RE2 full-match pattern. A candidate
whose current title matches it is excluded. Because both patterns are full
matches, use explicit `.*` terms when a substring match is intended. If more
than one ancestor remains, Hyprland prefers the most recently focused one and
otherwise uses the first candidate it found.

When a match is selected, Hyprland temporarily removes and hides that ancestor
while the child is mapped, then restores it when the child unmaps or closes.
Turning the feature off makes both pattern fields unavailable without
discarding their draft values. Clearing a pattern returns it to its empty
catalog default when saved; a nonempty invalid RE2 pattern is rejected before
the desired state is persisted. Reloading these values affects future window
mappings only. It does not retroactively swallow, unswallow, or otherwise
rearrange an existing pair.

## Handle unresponsive applications

**Show unresponsive app dialogs** is on by default. When Hyprland detects that
an application has stopped responding, the dialog offers **Wait** and
**Terminate** choices. This behavior is available only when Hyprland's
`hyprland-dialog` helper is installed and discoverable; enabling the setting
does not install or start that helper.

**Missed-response threshold** chooses how many missed checks are allowed before
Hyprland offers the dialog. It accepts 1 through 20 and defaults to 5. Turning
dialogs off makes the threshold unavailable without discarding its draft
value. Disabling dialogs stops later checks from opening a new dialog, but a
dialog that is already open may remain until the application responds or an
action is chosen.

## Draft, save, and recover

All 110 values form one Windows & Layout draft. **Discard draft** restores the
current authoritative projection without writing. **Reset to defaults**
prepares the trusted catalog defaults and removes redundant overrides when the
draft is saved. **Save & apply** validates the complete draft, persists one
whole desired-state revision while preserving every other setting, and then
reloads and verifies that exact revision.

You can navigate away without losing the draft. The sidebar shows **Unsaved**
while it differs from the current baseline. If another page saves a newer
whole-compositor revision, the badge changes to **Review** and the draft stays
intact. Select **Load current settings** explicitly when you are ready to
discard it.

If the saved revision cannot be activated, **Retry apply** targets that exact
revision regardless of which compositor page created it. **Restore last
working configuration** opens a cancel-first confirmation because recovery can
replace every pending compositor setting, not only Windows & Layout. Display
tests, shared visual-source transitions, saves, retries, and recovery lock the
page until their authoritative state is settled.

Return to [Settings](settings.md) or the [HyprShelld User Guide](index.md).
