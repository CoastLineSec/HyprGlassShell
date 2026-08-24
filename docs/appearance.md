# Appearance

The **Appearance** page changes a reviewed set of common Hyprland visuals,
existing custom animation curves, and animation rules through HyprShelld's
managed compositor configuration. **Visuals** and **Animations** are local tabs
over one draft: Discard, Reset, Save, conflict handling, and recovery apply to
the complete Appearance domain. These are desired managed values, not a reading
of every value effective in the running session. In particular, the user-owned
`user-custom.lua` is loaded after the managed modules and can override them.

Appearance becomes editable only after HyprShelld manages the Hyprland
entrypoint and the active desired-state revision is current. If management has
not started, open **Displays**, read the takeover confirmation, and choose
whether to allow it. Appearance never starts takeover implicitly.

## Visual choices

Appearance intentionally exposes forty common, safe options. Four
participate in HyprShelld's shared visual style:

- **Border thickness** sets the managed window border from 0 to 20 layout
  pixels. By default it follows the shared border configured on **Bar**.
- **Corner radius** sets rounded corners from 0 to 20 layout pixels. Its
  synchronized value also follows the shared border configured on **Bar**.
- **Window corner power** accepts an exact decimal from 2 through 10. The
  default 2 produces circular corners; higher values make the curve squarer.
  Hyprland also multiplies the effective radius by half the power, so this is
  a geometry control rather than only a curve-style choice. It remains a
  direct compositor value outside the synchronized border width/radius pair.
- **Inner window gaps** sets the top, right, bottom, and left gaps between
  neighboring windows. By default all four sides follow shared inner spacing.
- **Outer window gaps** sets the four monitor-edge gaps. By default the right,
  bottom, and left sides follow shared outer spacing, while the top remains
  zero because the Bar reservation already supplies it.
- **Blur backgrounds** enables or disables Hyprland's configured background
  blur. It is enabled by default.
- **Window shadows** enables or disables managed drop shadows. They are enabled
  by default.
- **Inner window glow** draws a glow just inside each window edge. It is off by
  default. Glow colors remain outside the Appearance editor and are reserved
  for HyprShelld's shared visual style.
- **Glow range** sets the inner-glow size from 0 through 100 layout pixels and
  defaults to 10. It remains editable while glow is off. Values from 0 through
  9 are retained exactly as compatibility values while glow is off, but cannot
  be enabled.
- **Glow falloff** selects an integer falloff power from 1 through 4 and
  defaults to 3. It remains saved but dormant while glow is off.
- **Include borders in window shadows** sizes each shadow from the outside edge
  of the window's effective visible border instead of from its main surface.
  It is enabled by default. The value remains saved while shadows are off, and
  has no visible effect for a window that does not currently draw a border.
- **Shadow range** sets how far each shadow extends beyond its window, from 0
  through 100 layout pixels. Its default is 4. The value remains saved while
  shadows are off and still controls the extent of sharp shadows.
- **Soft-shadow falloff** selects an integer falloff power from 1 through 4.
  Its default is 3; higher values fade more quickly. The value remains saved
  while shadows are off or sharp edges are enabled.
- **Sharp shadow edges** draws a solid-edged shadow instead of a soft falloff.
  It is disabled by default. Shadow range still controls the extent, and the
  saved soft falloff returns unchanged when sharp edges are disabled.
- **Shadow scale** accepts an exact decimal from 0 through 1 and scales each
  expanded shadow around its center. Its default 1 keeps the full size; lower
  values shrink it, and 0 makes it invisible. The exact value remains saved
  while shadows are off and applies to both soft and sharp shadows.
- **Horizontal shadow offset** moves every window shadow from -250 through 250
  layout pixels. Positive values move right and negative values move left.
  The default is 0, and the exact saved value remains dormant while shadows
  are off.
- **Vertical shadow offset** moves every window shadow from -250 through 250
  layout pixels. Positive values move down and negative values move up. The
  default is 0, and the exact saved value remains dormant while shadows are
  off. Both offset components apply to soft and sharp shadows.
- **Animations**, on the local **Animations** tab, enables or disables
  Hyprland's configured animations. It is enabled by default.
- **Dim inactive windows** darkens unfocused windows while leaving the focused
  window undimmed. It is disabled by default.
- **Inactive dimming strength** sets the inactive-window contribution from 0
  through 1 in 0.05 increments. Its default is 0.50. The slider is available
  only while **Dim inactive windows** is enabled; turning dimming off preserves
  the retained strength instead of resetting it.
- **Active-window opacity** sets the opacity of focused windows from 0 through
  1. Its default is 1.
- **Inactive-window opacity** sets the opacity of windows without focus from 0
  through 1. Its default is 1.
- **Fullscreen-window opacity** sets the opacity of true fullscreen windows
  from 0 through 1. Its default is 1. Maximized windows continue to use the
  focused or unfocused value.
- **Dim parents of modal dialogs** independently darkens a parent window while
  one of its modal dialogs is open. It is enabled by default.
- **Special-workspace dimming** sets how strongly the ordinary workspace is
  darkened behind an open special workspace, from 0 through 1. Its default is
  0.20.
- **Dim-around strength** sets how strongly the rest of the screen is darkened
  when a Window or Layer Rule enables **Dim around**, from 0 through 1. Its
  default is 0.40.
- **Blur size** stores and renders a blur radius from 0 through 100, default 8.
  Hyprland passes the configured size through to its blur shader.
- **Blur passes** stores a pass count from 0 through 10, default 1. Hyprland's
  renderer limits the effective count to the range 1 through 8 without
  changing the saved value.
- **Ignore window opacity** keeps window opacity from reducing the configured
  background blur contribution. It is enabled by default.
- **Optimized blur rendering** enables Hyprland's optimized blur path. It is
  enabled by default and is also required for the global X-ray choice to take
  effect.
- **X-ray floating windows** makes eligible floating-window blur ignore tiled
  windows beneath it. It is disabled by default. A matching Window Rule can
  override an individual window; Layer Rules control layer-surface X-ray
  separately.
- **Blur brightness** sets brightness modulation from 0 through 2. Its exact
  default is 1. Values below 1 dim the finished blurred color and noise;
  values above 1 brighten the input before the blur passes.
- **Blur contrast** sets contrast modulation from 0 through 2. Its exact
  default is 0.8916.
- **Blur noise** sets the amount of deterministic spatial noise added after
  blur, from 0 through 1. Its exact default is 0.0117; this is not animated
  grain.
- **Blur vibrancy** increases the saturation of blurred colors from 0 through
  1 during the blur passes. Its exact default is 0.1696.
- **Dark-area vibrancy** sets how strongly vibrancy handles dark colors, from
  0 through 1. Its default is 0. The value remains editable and retained when
  vibrancy is zero even though it then has no independent shader effect.
- **Blur special workspaces** blurs behind an open special workspace. It is
  disabled by default and can add rendering cost.
- **Blur pop-up menus** extends blur to pop-ups such as context menus. It is
  disabled by default.
- **Pop-up transparency cutoff** skips pop-up pixels below the selected alpha
  threshold. Stored values from 0 through 1 are accepted, and live mapped
  pop-ups use the stored value directly. Pop-up snapshot and fade-out capture
  use a minimum cutoff of 0.01; on that capture path, an owning layer's Rule
  `ignorealpha` value can replace the global cutoff. Its default is 0.20 and it
  is retained while pop-up blur is off.
- **Blur input methods** extends blur to input-method surfaces such as an
  on-screen candidate window. It is disabled by default.
- **Input-method transparency cutoff** skips input-method pixels below the
  selected alpha threshold. Stored values from 0 through 1 are accepted, and
  live mapped input-method surfaces use the stored value directly. Its default
  is 0.20 and it is retained while input-method blur is off.

Every 0-to-1 Appearance slider moves in 0.05 increments and shows two decimal
places. An existing finite in-range value that is not on that grid remains
displayed and is preserved exactly until that slider is moved; an edit then
uses the 0.05 grid. Values for modal, special-workspace, and Dim-around states
also remain saved while their triggering context is absent. The two blur
transparency cutoffs follow the same 0.05, two-decimal interaction contract.

The five blur color-modulation fields use exact decimal entry instead of a
slider grid. Enter a finite value inside the displayed inclusive range using
`.` as the decimal separator. Spaces, a plus sign, exponent notation,
redundant leading zeroes, `.5`, and `1.` are rejected. The editor does not add
a step or round the exact defaults. A valid authoritative value remains
unchanged until edited; after an edit, insignificant trailing zeroes are not
retained. Invalid text remains in the local draft, disables **Save & apply**,
and is replaced only by correction, **Discard draft**, **Reset to defaults**,
or explicit **Load current settings**.

**Window corner power** uses the same exact plain-decimal grammar, with an
inclusive range of 2 through 10 and no invented slider step. It stays editable
while shared borders are synchronized and while the global corner radius is
zero. A matching Window Rule can supply a nonzero radius while inheriting this
global power, so a zero global radius does not make the saved value universally
dormant. Reset restores 2.

**Shadow scale** also uses that exact plain-decimal grammar, with an inclusive
range of 0 through 1 and no invented slider step. Hyprland expands the shadow
by its range, scales that box around its center, and only then applies the
horizontal and vertical offsets. Reset restores 1. A value of 0 produces no
visible shadow, while any saved value remains available after window shadows
are turned back on.

Hyprland renders the radius and power as floating-point corner geometry, but
its curved-corner pointer and resize hit testing truncates both values to
integers. With fractional values, the interaction boundary can therefore be a
coarser approximation of the rendered curve. Hyprland 0.56.1 also snapshots
the power for a window already closing; that surface may finish its fade with
the prior value, while later and still-open windows use the reloaded value.

Turning **Blur backgrounds** off makes every detailed blur choice dormant but
does not erase it. The pop-up and input-method cutoffs remain dormant while
their respective parent switches are off. X-ray remains saved but has no
effect unless both blur and optimized blur rendering are enabled. Larger
blur sizes and effective pass counts, special-workspace blur, pop-up blur, and
input-method blur can all increase rendering cost. The five color-modulation
values also remain saved while blur is off and never disable one another.
They tune compositor blur rather than HyprShelld theme colors or display color
calibration; color-managed rendering details can differ between supported
Hyprland versions.

Turning **Window shadows** off makes **Include borders in window shadows**,
**Shadow range**, **Soft-shadow falloff**, **Sharp shadow edges**, **Shadow
scale**, and both shadow-offset components dormant but does not erase their
saved values.
Turning shadows back on restores every prior choice. Enabling sharp edges makes
only the soft-shadow falloff control dormant; range, scale, and both offset
components remain active because they apply to the solid shadow too. Disabling sharp
edges restores the saved falloff unchanged. **Save & apply** activates these
values through the same verified Hyprland Reload as the other Appearance
values.

Managed inner glow can be on only when **Glow range** is at least 10. With glow
off, range remains editable and falloff remains visible but dormant. If a
previously saved state has glow on below 10, Settings shows that exact state
without clamping it, turning it off, or replacing its values. The off switch
and range remain available for repair, while **Save & apply** remains
unavailable for an unsafe draft. **Retry apply** remains unavailable while the
exact saved revision is unsafe, even when an unsaved draft would repair it;
saving the repair creates a safe revision instead. Any whole-compositor
recovery that would activate the same unsafe combination also fails closed.
The managed check does not cover user-owned `user-custom.lua`, which loads
afterward.

A matching Window Rule's **Disable dimming** (`no_dim`) effect suppresses the
inactive-window contribution for that window. It does not rewrite the global
Appearance values or suppress modal-parent, special-workspace, or Dim-around
dimming. Those effects are independent and can combine with inactive-window
dimming. Surface opacity is also independent of the black inactive-dimming
scrim, so a window can be translucent, dimmed, both, or neither. A matching
Window Rule can change the resulting per-window opacity.

In Hyprland 0.56.1, an already-open special workspace retains its current
dimming amount when **Special-workspace dimming** is changed and the
configuration is reloaded. Close and reopen that special workspace to latch
the saved value. The same close-and-reopen requirement applies when changing
**Blur special workspaces** while a special workspace is already open.
**Dim-around strength** remains dormant unless a matching Window or Layer Rule
enables **Dim around**; when active, the effect follows the matched window or
layer through its fade-out.

While synchronization is on, window border thickness and corner radius remain
visible but read-only and are labeled **Controlled by HyprShelld**. Change the
shared values from **Bar**, or select **Override window borders** when Hyprland
should use a separate pair. That override affects only these two values; blur,
shadows, inactive-window dimming, and animation remain independent. **Sync
with HyprShelld** restores shared authority and reconciles the current shared
values. HyprShelld activates that reconciliation automatically only from an
exact current managed base; otherwise the matching values remain saved pending
an explicit safe apply or compositor takeover. Turning the shared border line
off sets a synchronized Hyprland border width of zero without discarding the
saved width or corner radius. Opacity and every dimming control remain
independent of border synchronization. **Include borders in window shadows**
also remains a direct compositor choice: synchronizing or overriding border
thickness and corner radius does not transfer ownership of it. Its effect
follows the effective visible border each window actually draws, including a
per-window border-size or decoration Rule.
**Window corner power** likewise remains a direct compositor choice. Whenever
ordinary Appearance controls are available, it stays editable while shared
borders are synchronized or the global radius is zero: it does not join the
synchronized width/radius pair, and a Window Rule may still combine it with its
own nonzero radius.

Window gaps have their own independent **Override window spacing** action.
While spacing synchronization is on, all four sides of both gap values remain
visible but read-only and are labeled **Controlled by HyprShelld**. Change the
shared inner and outer values from **Bar**, or use the override to edit every
side directly. **Sync with HyprShelld** restores shared spacing authority.
HyprShelld activates the reconciled gaps automatically only from an exact
current managed baseline; otherwise they remain saved pending explicit safe
Apply or compositor takeover. This does not change border synchronization.
The Bar still attaches from the visible workspace state. Once the protected
maximize rule has been safely applied, it keeps its covering window gapless
even when normal gaps are overridden.

The Bar can keep using and editing the shared style when compositor management
is unavailable. HyprShelld preserves the last applied window appearance and
reports synchronization as unavailable rather than taking over or reloading
Hyprland. Only the explicit takeover flow in **Displays** can adopt an
unmanaged compositor entrypoint.

Settings obtains the types, ranges, choices, and defaults from the exact
catalog advertised by the compositor-settings authority. If that catalog is
missing, has the wrong digest, or no longer matches this page's reviewed
contract, every Appearance control fails closed. The rest of Settings remains
available.

## Tune curves and animation rules

Open the local **Animations** tab to inspect existing custom curves and author
ordered animation rules. These collections are saved atomically with the
forty visual values; switching tabs never creates a second draft or
writer.

Settings can tune and reorder an existing custom curve, but its name and type
are read-only. Bezier curves expose the x and y coordinates of two control
points, each from -1 through 2. Spring curves expose stiffness, dampening, and
mass; every value must be finite, greater than 0.5, and no greater than
1,000,000. The internal record ID stays hidden and stable. **Reset to defaults**
restores the complete curve collection from the synchronized saved baseline,
including its order, IDs, names, types, and parameters.

Adding, removing, or renaming a curve and changing between Bezier and spring
are not available in Settings. Those structural changes remain unavailable
until HyprShelld has a verified compositor-restart workflow. This prevents
Settings from offering a save that the current reload-only authority cannot
activate.

The `default` and `linear` names are always available to animation rules. If an
existing custom curve uses either name, it appears once in the picker as a
**custom override**, not as a duplicate built-in choice. A managed rule using
that name resolves to the custom curve's authored type. Hyprland retains the
opposite-type built-in in its separate runtime map. Other existing custom
curves appear once by their saved names.

Choose **Add animation rule** to create an override for the first unused leaf
in Hyprland's fixed animation-tree order. Each leaf can appear only once, so at
most 34 rules can be authored even though the underlying bounded collection can
hold up to 256 records. The supported leaves are:

`global`, `windows`, `layers`, `fade`, `border`, `borderangle`,
`shadowangle`, `glowangle`, `workspaces`, `zoomFactor`, `monitorAdded`,
`layersIn`, `layersOut`, `windowsIn`, `windowsOut`, `windowsMove`, `fadeIn`,
`fadeOut`, `fadeSwitch`, `fadeShadow`, `fadeGlow`, `fadeDim`, `fadeLayers`,
`fadeLayersIn`, `fadeLayersOut`, `fadePopups`, `fadePopupsIn`,
`fadePopupsOut`, `fadeDpms`, `workspacesIn`, `workspacesOut`,
`specialWorkspace`, `specialWorkspaceIn`, and `specialWorkspaceOut`.

Every rule has a retained enabled state, speed greater than 0 and no greater
than 100, one available curve, and a leaf-specific style:

- `windows`, `windowsIn`, `windowsOut`, and `windowsMove` offer the default
  style, Slide with an optional direction, GNOME, GNOME dynamic, and Pop in
  with an optional 0-100 percent amount.
- `workspaces`, `workspacesIn`, `workspacesOut`, `specialWorkspace`,
  `specialWorkspaceIn`, and `specialWorkspaceOut` offer the default style,
  Fade, Slide, Vertical slide, Slide and fade, and Vertical slide and fade.
  Each slide style accepts an optional direction and optional 0-100 percent
  amount.
- `borderangle`, `shadowangle`, and `glowangle` offer the default style, Loop,
  and Once.
- `layers`, `layersIn`, and `layersOut` offer the default style, Fade, Slide
  with an optional direction, and Pop in with an optional 0-100 percent amount.
- Every other leaf uses the default style without an extension.

Rules can be added, removed, enabled, disabled, edited, and reordered. A
disabled rule remains in the draft with its speed, curve, style, and position.
Turning the master **Animations** switch off also retains every curve and rule.
The lists and detail values remain available for inspection, while curve and
rule mutation controls stay unavailable until the master switch is enabled
again. The aggregate Discard, Reset, and Save actions remain reachable.

The window preview is deterministic and illustrative. It uses one stable
Dwindle arrangement to compare borders, corners, blur, shadows, and animation
without making Appearance a second layout editor. When animations are enabled,
it demonstrates one full-width tile dividing into two equal tiles and closing
again. When inactive dimming is enabled, a scrim builds over the original tile
in proportion to that opening motion as the illustrative second window takes
focus, then reverses while it closes. Independently, the original tile moves
from active-window opacity toward inactive-window opacity while the second
window opens, and the new focused tile uses active-window opacity. The stable
one-window preview has no inactive tile and therefore no inactive scrim; its
single tile uses active-window opacity. Use **Pause motion** or **Play motion**
to control the demonstration. Turning animations off stops the loop and leaves
a stable preview. The preview remains at the top of the page while the setting
cards scroll beneath it. Nothing in the preview changes the live session. It
does not evaluate Window Rules or simulate true fullscreen, modal-parent,
special-workspace, or Dim-around states. It also does not simulate blur size,
effective pass clamping, X-ray eligibility, special-workspace blur, or the
pop-up and input-method transparency cutoffs. Brightness, contrast, noise,
vibrancy, and dark-area vibrancy are likewise not rendered by the
illustration. It also does not simulate whether the visible border is included
in a window shadow's bounds, how window corner power changes geometry, or the
shadow range, soft falloff, sharp-edge, horizontal or vertical offset, or
shadow-scale choices. Its accessible text alternative reports the exact shadow
range, falloff power, scale, and both offset components, the on/off sharp-edge
and
border-in-shadow choices, and each other exact numeric draft value, including
corner power, or identifies an invalid draft, without pretending to simulate
those effects. It also reports the exact inner-glow enabled state, range, and
falloff power. The shadow-rendering and inner-glow summary values do not alter
preview geometry, position, glow or shadow size, falloff, color, opacity,
blur, or motion. The four-layout demonstration and layout selector are on
[Windows & Layout](windows-layout.md). Custom curve
shapes, animation speeds, styles, and rule order are not simulated in the
Appearance preview.

## Draft, save, and apply

Changing a control creates a local draft. Use:

- **Discard draft** to return to the authoritative values without writing;
- **Reset to defaults** to prepare catalog defaults for direct compositor
  controls—including inactive dimming off with its retained strength at
  0.50, all three opacity values at 1, modal-parent dimming on,
  special-workspace dimming at 0.20, Dim-around strength at 0.40, blur size 8,
  one blur pass, opacity-ignore and optimized rendering on, X-ray and special-
  workspace blur off, and both pop-up and input-method blur off with their
  cutoffs at 0.20, blur brightness 1, contrast 0.8916, noise 0.0117,
  vibrancy 0.1696, dark-area vibrancy 0, border inclusion in window shadows
  on, window corner power 2, shadow range 4, soft-shadow falloff power 3,
  sharp shadow edges off, shadow scale 1, horizontal and vertical shadow
  offsets 0, inner glow off, glow range 10, and glow falloff power 3—retain
  synchronized border and spacing values,
  restore the saved
  curve collection, and clear every authored animation rule; or
- **Save & apply** to persist and activate the complete validated draft.

Resetting to defaults removes redundant managed overrides rather than pinning
copies of defaults in the desired-state document. It does not edit
`user-custom.lua`.

**Save & apply** is deliberately a two-stage operation. HyprShelld first uses a
compare-and-swap replacement to create exactly one desired-state revision while
preserving every setting outside the forty visual values, custom curves,
and animation rules owned by Appearance. It then reloads Hyprland and verifies
that exact saved revision. Display tests and other compositor mutations cannot
overlap this operation.

If another client changes the compositor revision while you are editing,
Settings preserves the visible draft and refuses to silently rebase or
overwrite the newer state. Select **Load current settings** when you are ready
to discard that draft and review the new baseline.

You can visit another Settings page without discarding the draft. The
Appearance sidebar item marks unsaved work. If another compositor page saves a
new whole-snapshot revision while the Appearance draft is preserved, the
marker changes to **Review** and **Load current settings** remains the explicit
way to resolve the conflict.

If saving succeeds but activation fails, the desired revision remains saved
and Settings says that it is not active. **Retry apply** retries the exact saved
compositor revision when the authority says that is safe, even when another
compositor page initiated that revision. It does not create another
desired-state revision.

Shared-border and shared-spacing synchronization follow the same durable
compositor activation rules as independent authority groups. A successful
managed update advances the normal last-known-good state. If synchronization
fails, the Bar still reflects HyprShelld's saved style, Hyprland keeps its last
verified live state, and Settings offers a retry for the affected group. It
does not repeatedly reload the same failed tuple in the background.

## Restore the last working configuration

When recovery is available, **Restore last working configuration** opens a
cancel-first confirmation. This is whole-compositor recovery, not an Appearance
reset: it can replace every pending compositor setting, including display,
Input, Windows & Layout, Workspaces, and Rules settings, with the last verified
working snapshot.

After confirmation, HyprShelld records that snapshot as a new monotonic
desired-state revision, reloads Hyprland, and verifies it. Canceling does not
change desired files or the running compositor. An ambiguous recovery response
is not retried automatically. If window-border or window-spacing
synchronization remains on, HyprShelld then reasserts each current shared group
instead of leaving those values under the recovered compositor snapshot.

Appearance uses the same compositor desired-state and last-known-good files as
Displays. On a standard installation these are
`~/.local/state/hyprshelld/compositor/desired.json` and
`~/.local/state/hyprshelld/compositor/last-good.json`. See
[Displays](displays.md) for the managed loader, preserved pre-takeover
entrypoint, immutable generations, and `user-custom.lua` locations.

Return to [Settings](settings.md) or the [HyprShelld User Guide](index.md).
