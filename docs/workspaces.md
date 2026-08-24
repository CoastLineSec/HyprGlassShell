# Workspaces

The **Workspaces** page manages Hyprland workspace behavior, switching history,
pointer placement, application placement tracking, workspace gestures, and
ordered Workspace Rules. It is separate from the **Workspaces** card on the Bar
page: that card controls how the workspace switcher is presented, while this
page changes compositor behavior.

These are desired managed values. The user-owned `user-custom.lua` loads after
managed modules and can override them. The page becomes editable only after
HyprShelld manages the Hyprland entrypoint and has a current verified baseline.
If it is unmanaged, open **Displays** and review the takeover confirmation.

Workspaces has no illustrative preview. A simulation would not verify the
running compositor's workspaces, devices, gesture bindings, or application
launch behavior.

Use the **Behavior** and **Workspace Rules** tabs to move between the two parts
of the same draft. Changing tabs does not save or discard anything.

## Choose workspace behavior

**Wrap slide direction at the ends** is off by default. It reverses the visual
direction of slide-style workspace animations when moving between the first
and last workspace.

**Close an empty special workspace** is on by default. It closes a special
workspace after its last window leaves.

## Control switching and history

**Keep workspace cycle history** is off by default. Turning it on retains each
previous-workspace link after a workspace cycle, so repeating a cycle can
continue through recently visited workspaces.

**Hide special workspace after a change** is off by default. It hides the
monitor's active special workspace when switching workspaces or moving the
active workspace to another output.

**Switch back from the current workspace** is off by default. When it is on, a
workspace command that targets the workspace already focused returns to the
previously focused workspace.

**Cross-output pointer target** chooses the point used when a workspace switch
moves focus to another output. **Workspace center** targets the center of that
workspace. **Last active window**, the default, targets its last active window.

## Place the pointer after workspace changes

**After changing workspace** and **When toggling a special workspace** each
offer the same three modes:

- **Disabled**, the default, leaves this pointer movement off;
- **Enabled** moves the pointer to the target workspace's last-focused window
  unless `cursor:no_warps` blocks the warp; and
- **Force** makes the same move while bypassing `cursor:no_warps`.

The separate `cursor:persistent_warps` value determines whether Hyprland uses a
remembered position relative to that window or its center. The **Input** page's
**Cursor placement** card is the sole writable owner of both
`cursor:no_warps` and `cursor:persistent_warps`; Workspaces does not change
either value. Changing one pointer-placement mode does not disable, clear, or
rewrite any of the other values, and the page's non-preview presentation does
not simulate pointer movement.

## Track an application's initial workspace

**Initial workspace tracking** can be:

- **Disabled**;
- **First window**, the default, which tracks the first window associated with
  an application launch; or
- **Window and children**, which follows the launched window and its children.

**Initial-token timeout** is available in First window mode. It ranges from 1
through 3600 seconds and defaults to 10. Switching tracking modes preserves the
timeout value.

## Tune workspace swipes

The swipe controls tune Hyprland's response to a configured touchpad workspace
gesture. They do not create a touchpad gesture binding. The controls are:

- **Swipe distance**, 0 through 2000 pixels, default 300;
- **Completion threshold**, 0 through 1, default 0.5, adjusted in 0.05
  increments;
- **Allow a new workspace**, on by default;
- **Lock swipe direction**, on by default;
- **Direction-lock threshold**, 0 through 200 pixels, default 10;
- **Continue across workspaces**, off by default;
- **Invert touchpad direction**, on by default;
- **Minimum completion speed**, 0 through 200, default 30; and
- **Use adjacent workspace numbers**, off by default, to move through adjacent
  valid identifiers instead of cycling only existing workspaces on the monitor.

The direction-lock threshold is available only while direction lock is on.
Turning direction lock off preserves the threshold.

**Enable touchscreen edge swipes** is off by default. Unlike the touchpad
scalars above, this switch enables Hyprland's own touchscreen workspace-swipe
path. **Invert touchscreen direction** is available only while touchscreen edge
swipes are enabled, and is off by default.

## Create a Workspace Rule

Open **Workspace Rules** and select **Add workspace rule**. New rules begin
disabled and incomplete so they cannot accidentally target a broad set of
workspaces. Choose one exact target before saving:

- **Numeric** accepts a positive workspace number;
- **Named** accepts one exact named workspace;
- **Any special workspace** targets the special-workspace class; or
- **Named special workspace** accepts one exact special-workspace name.

Each target can appear only once in the user-rule list. HyprShelld generates a
stable hidden identity for the record, so renaming its target or moving it does
not create a different rule.

The assignment controls can leave a rule available on any output or bind it to
one exact output name or description. A rule can keep its workspace persistent,
make it the default for its assigned output, and select Dwindle, Master,
Scrolling, or Monocle instead of the global layout. Enable the rule when its
target and behavior are ready. A disabled rule remains saved in the same
position.

## Add typed workspace overrides

An override is absent until its checkbox is selected. Removing the checkbox
removes that override; for a Boolean override, an included **Off** value remains
an explicit value and is different from removing it. The authored editor
supports:

- inner, outer, and floating gaps as signed **top, right, bottom, left**
  values;
- a signed border size;
- explicit border, rounding, decoration, and shadow behavior;
- a default workspace name;
- Fade, Slide, Vertical slide, Slide and fade, or Vertical slide and fade,
  with an optional direction and distance percentage for slide styles; and
- optional Master orientation and Scrolling direction details.

Gap and border fields accept the complete lossless JSON safe-integer range.
Incomplete, fractional, noncanonical, or out-of-range values remain visible
for correction but disable **Save & apply**. All other typed fields are checked
against the managed Workspace Rule contract before submission.

Use **Move up** and **Move down** to set precedence, or **Remove** to take a
record out of the draft. The list is user-owned: HyprShelld's protected rule
for gapless maximized-window integration is never displayed, reordered,
edited, or removed here. **Reset to defaults** clears the user-rule list while
leaving that internal integration protected.

## Draft, save, and recover

All 21 behavior values and the complete ordered user-rule list form one
Workspaces draft. **Discard draft** restores both from the current authoritative
projection without writing. **Reset to defaults** prepares the trusted catalog
defaults and an empty user-rule list. **Save & apply** validates the complete draft,
persists one whole desired-state revision while preserving every other
compositor setting, and reloads and verifies that exact revision.

Navigation preserves the draft. The Workspaces sidebar item shows **Unsaved**
while it differs from the current baseline. If another page saves a newer
whole-compositor revision, it changes to **Review** and keeps the draft intact.
Select **Load current settings** explicitly when you are ready to discard it.

If activation fails after saving, **Retry apply** targets the exact saved
revision regardless of which compositor page created it. **Restore last
working configuration** opens a cancel-first confirmation because recovery can
replace every pending compositor setting, not only Workspaces. Display tests,
shared visual-source transitions, saves, retries, and recovery lock editing
until their authoritative state is settled.

Return to [Settings](settings.md) or the [HyprShelld User Guide](index.md).
