# Input

The **Input** page has local **Global**, **Devices**, and **Gestures** tabs.
Global changes a reviewed set of Hyprland keyboard, virtual keyboard, mouse,
pointer behavior, cursor visibility and placement, touchpad, touch-device,
drawing-tablet orientation and mapped-region, and advanced scrolling options
through HyprShelld's managed compositor configuration.
Devices provides read-only session diagnostics and shows preserved saved
per-device records. It does not add, edit, forget, reassign, reset, or test a
device setting. Gestures authors an ordered collection of touchpad bindings
from nine fixed actions. The separate **Hyprland > Devices** editor authors the
complete ordered saved per-device collection with all 39 typed 0.56.2 override
fields. Input does not expose free-form keyboard maps, key bindings, or custom
gesture actions.

Input becomes editable only after HyprShelld manages the Hyprland entrypoint
and has a current trusted option catalog. If management has not started, open
**Displays**, review the takeover confirmation, and decide whether to allow it.
Input never starts takeover implicitly.

## Session device diagnostics

Open **Devices** to inspect the latest authenticated query of the current
Hyprland session. The observation is a one-time receipt rather than a live
connection monitor, so Settings uses **Observed** and **Not observed when last
checked** instead of claiming that a device is continuously connected or
offline. Select **Refresh** on this tab to perform another query, or **Manage
device profiles** to open the separate editable per-device collection.

Addressable devices are grouped as Keyboards, Pointing devices, Touch devices,
and Tablets. A keyboard can include its reported active keymap as diagnostic
text. That text is not proof that a saved keymap was applied. Switches, tablet
pads, and tablet tools that cannot be matched to saved settings appear only in
a count summary.

**Other saved device settings** preserves the exact saved order and identifies:

- **Not observed when last checked** for an addressable saved selector absent
  from the latest successful observation;
- **Connection unknown** when Hyprland cannot expose a matchable selector for
  that saved kind;
- **Saved kind differs** when the exact selector appears in another coarse
  device class; and
- **Connection status unavailable** when no current authenticated observation
  exists.

Hyprland device names are session-assigned diagnostic selectors, not stable
hardware identities. Identical devices can receive numbered names according
to which devices are attached and their connection order. HyprShelld therefore
does not guess, repair, or reassign a saved selector. It preserves saved records
without exposing an editing or Forget action.

Discovery and saved-record verification are independent. If live discovery
fails, saved records remain visible with connection status unavailable. If a
saved record is invalid, the live observation remains visible with its saved-
settings association unavailable. Neither condition hides the Global Input
values. The complete Global-and-Gestures draft remains preserved and marked
while Devices is open.

The enabled value shown for a saved record is desired configuration, not an
observation of the device's current enabled state. The device inventory is not
a device-support or capability test.

## Editable per-device profiles

Open **Hyprland > Devices** to create, edit, remove, and reorder saved device
profiles. Each record uses one exact Hyprland session device name and exposes
all 39 fields in the pinned `deviceOverride` schema through typed controls.
Only explicitly enabled overrides are emitted; unchecked fields continue to
inherit the matching global Input value or Hyprland default.

The editor keeps device discovery and desired configuration separate. Its
illustration explains selector matching and generated `hl.device` records, but
it does not claim that a selector is currently present or that hardware
supports an override. It validates the entire ordered collection, preserves a
local draft across navigation, detects a newer whole-compositor revision, and
saves the collection atomically while preserving every unrelated configuration
surface.

## Gestures

Open **Gestures** to create, edit, remove, and reorder up to 64 managed
touchpad gesture bindings. The collection is ordered because an earlier broad
finger, direction, and modifier match can shadow a later one. Settings allows
a move only when the complete resulting order remains valid.

New bindings use the first unused stable name in the `gesture-N` sequence.
Choose:

- two through nine fingers;
- a general swipe, an exact left, right, up, or down swipe, a horizontal or
  vertical swipe, or a general, inward, or outward pinch;
- any combination of Shift, Caps Lock, Ctrl, Alt, Mod2, Mod3, Super, and Mod5;
- a swipe scale from 0.1 through 10; and
- whether the binding may run while an application inhibits compositor
  gestures.

Pinch scale is fixed at 1 and is not shown as an editable field. The inhibit
override is advanced: it can activate a managed gesture during an interaction
that expects compositor gestures to remain reserved.

The authored actions are:

- **Close**, using **Close timeout** from 10 through 2000 milliseconds
  (default 1000);
- **Cursor zoom**, with Toggle, Multiply, and Live pinch modes and a zoom level
  from 0.01 through 100; Live is available only for pinch directions;
- **Floating state**, with Float, Tile, and Toggle modes;
- **Fullscreen state**, with Fullscreen and Maximize modes;
- **Move window** and **Resize window**;
- **Move scrolling window**, which accepts swipe directions but not pinch;
- **Special workspace**, with one exact non-empty special-workspace name; and
- **Navigate workspaces**.

These are fixed first-party forms, not a command field or a UI generated from
untrusted action metadata. Changing an action also adjusts an incompatible
direction safely—for example, choosing Live cursor zoom selects a pinch and
choosing Move scrolling window replaces a pinch with a swipe.

An existing core-valid row that uses `unset`, a non-unit pinch scale, a pinch
for Move scrolling window, or Live cursor zoom outside a pinch appears as
**Compatibility record — read only**. Settings preserves its exact fields and
order during ordinary edits and saves. You can remove it, and you can reorder
it only when the complete candidate remains valid, but you cannot edit or
duplicate it or create another compatibility-only row.

The editor validates the managed record and reload transaction; it is not a
hardware test. This development build has automated structural verification,
but its physical gesture behavior has not yet been verified on hardware.

## Keyboard

- **Repeat rate** controls how many repeats a held key sends each second. Its
  range is 0 through 200 and its default is 25.
- **Repeat delay** controls how long a key must be held before repetition
  begins. Its range is 0 through 2000 milliseconds and its default is 600.
- **Num Lock by default** asks Hyprland to engage Num Lock whenever it
  configures a keyboard. It is off by default. Turning the setting off does
  not clear a Num Lock state that is already active.
- **Shortcuts follow the active layout** is off by default. Resolve symbol-
  based shortcuts from each keyboard's active layout instead of the primary
  globally configured layout. Exact saved per-device values win. Keycode-
  based shortcuts are unchanged.

## Virtual keyboards

These global choices remain available without device discovery. They apply to
compatible on-screen keyboards and input methods when those clients use the
virtual-keyboard protocol.

- **Share key states** chooses whether pressed keys and modifiers are combined
  with other keyboards. It offers Never, Always, and Except input methods.
  Except input methods is the default. A changed sharing mode takes effect
  when the virtual keyboard next connects.
- **Release held keys on close** sends release events for keys that remain
  pressed when a virtual keyboard closes. It is off by default and is checked
  when the current virtual keyboard closes; reconnecting first is not required.
- **Name after creating application** gives newly connected virtual keyboards
  a device name based on the creating process instead of a generic name. It is
  on by default and does not rename an already connected keyboard. Because the
  device name can be used for matching, this choice can affect which future
  per-device rules apply.

## Mouse

- **Pointer sensitivity** ranges from -1.00 through 1.00 in 0.05 interaction
  steps. Its default is 0.00.
- **Acceleration profile** offers Automatic, Adaptive, and Flat. Automatic is
  the default and leaves profile selection to the input stack.
- **Natural scrolling** moves content in the same direction as the scroll
  gesture. It is off by default.
- **Left-handed buttons** swaps the primary and secondary buttons. It is off
  by default.
- **Scroll speed** multiplies external-mouse scroll movement from 0.0 through
  2.0 in 0.1 interaction steps. Its default is 1.0.

## Pointer behavior

These global choices remain visible without device discovery. HyprShelld does
not claim that a particular pointing device is present or supports rotation.

- **Raw cursor movement** makes Hyprland move its compositor cursor with the
  device's unaccelerated motion. It is off by default. While it is on,
  **Pointer sensitivity** and **Acceleration profile** remain saved but their
  controls are unavailable and they do not affect that cursor. Turning raw
  movement off restores both controls with their retained values. This setting
  describes the compositor cursor; it does not claim to change an
  application's relative-pointer behavior.
- **Pointer rotation** rotates motion clockwise from 0 through 359 degrees and
  defaults to 0. Hyprland applies it only to compatible pointing devices;
  unsupported devices ignore it.
- **Middle-click paste** allows applications to update the primary selection
  used for middle-click paste. It is on by default. Turning it off takes effect
  when an application next tries to set that selection rather than promising
  an immediate change when the draft is saved.

## Cursor visibility and placement

The cursor visibility controls apply to later input events and to Hyprland's
periodic hide-state check:

- **Hide after keyboard input** is off by default. When enabled, a keyboard key
  event hides the cursor until physical mouse movement.
- **Hide after touch input** is on by default. Hyprland can consume the retained
  touch condition before clearing it, so the cursor can remain hidden through
  the first following physical mouse movement. A subsequent movement reveals
  it.
- **Hide after tablet input** is off by default and has the same first-motion
  ordering as touch input.
- **Inactivity timeout** accepts 0 through 20 seconds in 0.1-second interaction
  steps and defaults to 0. An exact valid off-grid saved value remains unchanged
  until this slider is moved. Zero disables only inactivity-based hiding; it
  does not override any of the three input-triggered hide reasons. Pointer,
  touch, or tablet movement and a physical pointer-button event restart the
  timer. Wheel scrolling alone does not.

Hyprland evaluates the combined hide state every 500 milliseconds. The timeout
therefore has 500-ms evaluation resolution rather than a sub-500-ms boundary,
and disabling a hide reason can take until the next check to reveal a stationary
cursor.

The placement controls are independent and remain editable together:

- **Edge padding** accepts 0 through 20 logical pixels and defaults to 0. On
  the next pointer move or warp, Hyprland checks the corners of a
  hotspot-centered square against the combined active display layout and moves
  it away from an outside edge when possible. At an adjacent-display seam, the
  square can span both displays. This does not change cursor artwork or theme
  padding and does not retroactively move a stationary pointer.
- **Suppress ordinary pointer jumps** is off by default. When enabled, it
  suppresses ordinary compositor-requested warps. A suppressed request can
  still update monitor focus. Explicit Force paths and raw pointer-manager
  warps bypass it, so this switch does not block every possible pointer move.
- **Remember position in each window** is off by default. On the next eligible
  window-warp request, Hyprland uses a remembered window-relative point only
  when it is strictly inside that window; otherwise it chooses the center. This
  setting selects the destination and does not create a warp. Ordinary jump
  suppression still wins, while a Force request can use the remembered point.
- **Restore mouse position after other input** is off by default. After
  non-mouse pointer input, the next physical mouse motion, button, or wheel
  event raw-warps to the stored physical-mouse position. That return
  intentionally bypasses ordinary jump suppression, but the target still
  passes through the current display-layout and hotspot-padding clamp.

These settings control cursor behavior only. Shared cursor appearance, theme,
size, renderer and backend selection, and synchronization remain outside
Input.

## Touchpad

Touchpad choices remain visible whether or not a touchpad is currently
connected. HyprShelld preserves them and Hyprland uses them whenever a
touchpad is present.

- **Tap to click** is on by default.
- **Tap and drag** is on by default.
- **Natural scrolling** is off by default.
- **Disable while typing** is on by default.
- **Scroll speed** ranges from 0.0 through 2.0 in 0.1 interaction steps and
  defaults to 1.0.

### Buttons and touchpad behavior

These choices remain visible without hardware detection. Hyprland applies
them only when the connected touchpad and its input capabilities support the
requested behavior.

- **Click with fingers** uses finger count for physical clickpad presses:
  one, two, and three fingers produce primary, secondary, and middle clicks.
  It is off by default, so supported devices use button areas instead.
- **Tap button order** offers Automatic, Primary/Secondary/Middle, and
  Primary/Middle/Secondary. It is editable only while **Tap to click** is on;
  turning tapping off preserves the selected order.
- **Two-button middle click** treats a simultaneous primary-and-secondary
  physical-button press as a middle click. It is off by default.
- **Drag lock** offers Off, Timed, and Sticky. It is editable only while both
  **Tap to click** and **Tap and drag** are on. Turning either prerequisite
  off preserves the chosen lock mode for later.
- **Multi-finger drag** independently offers Off, Three fingers, and Four
  fingers on compatible touchpads. It is off by default.
- **Reverse horizontal movement** and **Reverse vertical movement** invert
  touchpad pointer motion on each axis independently. Both are off by default.

## Touch devices

These values are global fallbacks for compatible libinput-backed touch
devices. They remain available without device discovery, but their presence is
not a claim that a touch device is connected or supports either setting. An
exact saved per-device value takes precedence over the global fallback, and
the Devices tab remains read-only.

- **Enable touch input** allows input from compatible touch devices and is on
  by default.
- **Touch transform code** accepts 0 through 6 and defaults to 0. A compatible
  libinput touch device applies this transform only when it exposes
  calibration-matrix support.

## Drawing tablets

These values are global fallbacks for compatible libinput-backed drawing
tablets. They remain available without device discovery, but their presence is
not a claim that a tablet is connected or supports these settings. An exact
saved per-device value takes precedence over the global fallback, and the
Devices tab remains read-only.

- **Relative tablet motion** is off by default. Off uses absolute pen
  placement; on moves the pointer by pen-motion deltas. A tablet tool reported
  as a mouse tool still moves relatively regardless of this fallback.
- **Left-handed tablet orientation** is off by default. It asks libinput to
  rotate a compatible tablet by 180 degrees.
- **Tablet transform code** accepts 0 through 6 and defaults to 0.

### Tablet mapped region

These additional global fallbacks control absolute pen mapping on compatible
libinput-backed drawing tablets. Exact saved per-device values still take
precedence. While **Relative tablet motion** is on, the mapped-region controls
are unavailable but every exact value remains saved for later absolute use.

- **Mapped position X** and **Mapped position Y** accept exact signed decimal
  logical-layout coordinates from -20000 through 20000 and default to `[0,0]`.
  With a saved per-device output binding, Hyprland treats the pair as an offset
  within that output. Without an output binding, it offsets the complete
  monitor layout unless **Exact layout position** is on.
- **Exact layout position** is off by default. With no saved output binding,
  turning it on treats the X and Y pair as exact compositor-space coordinates
  instead of offsets from the complete layout. A saved output binding always
  takes precedence and keeps the pair as an offset.
- **Mapped width** and **Mapped height** accept exact signed decimal logical-
  layout sizes from -100 through 4000 and default to `[0,0]`. Hyprland replaces
  the selected output/layout dimensions only when both components have
  magnitude at least `0.000000001`; if either is effectively zero, both base
  dimensions remain selected. A negative nonzero width or height is valid and
  reverses that axis.

The four component fields have no invented step or slider grid. Settings
preserves every finite in-range decimal exactly, shows invalid text until it
is corrected, and blocks **Save & apply** while any component is invalid.
Reset restores `[0,0]` and turns **Exact layout position** off; Hyprland
unconditionally reapplies those global fallbacks during input refresh, so the
prior mapped region is cleared. This reset symmetry is why these controls are
authored while the separate `active_area_*` pair remains excluded.

Both transform controls use the same exact codes:

- 0 — identity (**Normal**);
- 1 — **Rotate 90°**;
- 2 — **Rotate 180°**;
- 3 — **Rotate 270°**;
- 4 — **Flipped**;
- 5 — **Flipped + 90°**; and
- 6 — **Flipped + 180°**.

Resetting a Global transform to 0 actively prepares the identity fallback; it
does not edit a saved device record. An exact per-device transform remains in
control, including an explicit per-device `-1` value that skips matrix
assignment. Use of the Global controls therefore neither repairs nor tests a
per-device record.

## Advanced scrolling

These global choices apply to compatible pointing devices. A device may ignore
a method it does not support; HyprShelld does not claim that a particular
device is present or compatible.

- **Scroll method** offers Automatic, Two-finger, Edge, Button scrolling, and
  Disabled. Automatic is the default and uses the device's default method.
- **Scroll button** accepts button numbers from 0 through 300. Zero uses the
  device default. **Keep button scrolling active** lets the method remain
  active without holding the selected button and is off by default. Both
  controls are editable only while Button scrolling is selected; choosing
  another method preserves their values.
- **Scroll outside a window** chooses whether an event is ignored, sent to the
  window, clamped to its edge, or moves the pointer to its edge. Sending the
  event is the default.
- **High-resolution wheel compatibility** controls whether Hyprland
  synthesizes traditional wheel steps. It can be Off, apply When needed to
  non-standard events (the default), or apply to All wheel events.

Input has no illustrative preview. A simulated input surface would not verify
actual Hyprland or device behavior. Changing a Global control or gesture
binding creates only one local aggregate draft until you select
**Save & apply**.

## Draft, save, and apply

Use:

- **Discard draft** to return to the current authoritative Global values and
  exact ordered gesture collection;
- **Reset to defaults** to prepare the trusted Hyprland scalar defaults and
  clear every managed gesture binding; or
- **Save & apply** to persist and activate the complete validated Global and
  Gestures draft.

Reset removes redundant managed scalar overrides instead of storing copies of
every default and explicitly prepares an empty gesture collection. It changes
only the reviewed Input group and does not edit `user-custom.lua` or any saved
per-device record.

Save uses one whole-compositor compare-and-swap revision while preserving
Appearance, Windows & Layout, Displays, custom overrides, and every other
desired-state field.
HyprShelld then reloads Hyprland and verifies that exact saved revision.
Display tests and other compositor mutations cannot overlap the operation.

Settings preserves an Input draft when you visit another page. The Input item
in the sidebar marks hidden unsaved work. If another page saves a newer
whole-compositor revision while the Input draft is open, the marker changes to
**Review** and Input requires **Load current settings**. HyprShelld never
silently rebases or discards the draft.

The user-owned `user-custom.lua` loads after managed modules and can override
managed Input values. The page represents the desired managed settings, not a
claim that every displayed value is the final runtime value after custom Lua.

## Retry and recovery

If saving succeeds but activation fails, **Retry apply** retries the exact
saved compositor revision without creating another revision. The saved
revision may include a change initiated from another compositor page, so Retry
always describes the whole saved compositor state rather than only Input.

**Restore last working configuration** opens a cancel-first confirmation. This
is whole-compositor recovery: it can replace pending Input, Appearance,
Windows & Layout, Displays, and other managed compositor settings with the last
verified working snapshot. Confirming records that snapshot as a new monotonic
revision, reloads Hyprland, and verifies it. Canceling leaves desired files and the
running compositor unchanged.

Input uses the same desired-state and last-known-good files as Appearance,
Windows & Layout, and Displays. On a standard installation they are
`~/.local/state/hyprshelld/compositor/desired.json` and
`~/.local/state/hyprshelld/compositor/last-good.json`.

Return to [Settings](settings.md) or the [HyprShelld User Guide](index.md).
