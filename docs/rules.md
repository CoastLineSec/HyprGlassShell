# Window and Layer Rules

The **Rules** page creates ordered Window Rules and Layer Rules through typed
controls. It does not expose raw JSON, arbitrary commands, plugin strings,
group-rule mini-languages, or Workspace Rules. Those are not part of this
managed editor.

Rules are desired managed configuration. The user-owned `user-custom.lua`
loads after managed modules and can still override managed behavior. The page
becomes editable only after HyprShelld manages the Hyprland entrypoint and has
a current verified rules projection. If management has not started, open
**Displays** and review the takeover confirmation first.

There is no simulated preview. Whether a rule matches depends on real window
and layer-surface identity and state, so an illustration would not verify the
result.

## Work with the combined draft

**Window Rules** and **Layer Rules** are tabs in one page and share one draft.
Adding, renaming, enabling, disabling, removing, or moving a rule changes only
that local draft until **Save & apply** succeeds. Navigation to another
Settings page also preserves it.

To add a rule:

1. Open the appropriate tab and select **Add window rule** or **Add layer
   rule**.
2. Give the rule a unique name. HyprShelld creates and preserves its hidden
   stable identifier automatically, including after a rename or reorder.
3. Include at least one matcher and one effect.
4. Leave the new rule disabled until you have reviewed its scope, then enable
   it when it should become active.
5. Return to the list and select **Save & apply**.

A newly added rule is deliberately disabled and empty. HyprShelld does not
seed a broad matcher or an active effect. The draft remains editable, but Save
stays unavailable until every record has a unique name, at least one complete
matcher, and at least one complete effect.

Each optional field has an **Include** choice. Removing that choice omits the
field. For a boolean field, including the field with **Off** is an explicit
`false` rule value and is different from omitting it. This distinction is
preserved when the draft is reordered, saved, or reloaded.

Use **Move up** and **Move down** to change the saved rule order. **Remove**
changes only the draft, so **Discard draft** can still restore the current
authoritative collection. **Reset to defaults** prepares empty Window and
Layer Rule collections.

## Match windows

Window Rules can include any reviewed managed matcher:

- current class and title;
- initial class and title;
- exact window tag;
- content type, XDG tag, or namespace;
- XWayland, floating, fullscreen, pinned, focused, grouped, or modal state;
- internal and client fullscreen modes; and
- a positive numeric, named, or special workspace selector.

Class, title, initial identity, content, XDG tag, and namespace fields use RE2
patterns. Settings checks that these fields are present, bounded, and free of
disallowed control characters. The trusted full-state parser checks the RE2
syntax when Save is requested, before any desired-state revision is replaced.
If parsing fails, the scoped Rules error remains visible and the complete draft
is preserved for correction. Settings does not substitute JavaScript regular
expression behavior for Hyprland's accepted RE2 contract.

## Apply Window Rule effects

The selected Window Rule editor groups every reviewed managed effect by task.

**Placement** includes floating, tiled, fullscreen, maximized, centered,
pseudo-tiled, pinned, internal/client fullscreen state, exact position, exact
size, monitor target, and workspace target. Monitor and workspace moves keep
their explicit **silent** choice.

**Focus and input** includes initial-focus blocking, input allowance,
focus-on-activation, focus blocking, pointer-focus exclusion, persistent focus,
pointer confinement, and shortcut-inhibition handling.

**Appearance** includes corner radius, border size, rounding power, border
gradient, active/inactive/fullscreen opacity and their override flags,
decorations, surrounding dimming, animation, blur, dimming, and shadow
disablement, opaque and X-ray treatment, unfocused rendering, RGBX handling,
and nearest-neighbor scaling.

**Behavior** includes event suppression, content classification, close delay,
Scrolling width, mouse and touchpad scroll factors, animation style, idle
inhibition, tag changes, maximum and minimum size, persistent size, aspect-ratio
preservation, maximum-size handling, fullscreen synchronization, immediate
presentation, screen-sharing exclusion, VRR and automatic HDR disablement, and
tone mapping.

Numeric rule fields accept every finite value inside the strict schema range;
they are not forced onto an invented slider increment. **Border size** has no
narrow compositor range in the managed schema, so it uses a canonical decimal
whole-number field. It accepts the complete lossless JavaScript-safe range from
−9007199254740991 through 9007199254740991 and rejects plus signs, leading
zeros, negative zero, fractions, exponents, surrounding whitespace, and
overflow.

A border gradient contains one through ten uppercase `0xRRGGBBAA` colors in
order plus an angle. Opacity keeps optional inactive and fullscreen values
separate from their explicit override switches. Event suppression keeps one or
more selected events and does not accept an empty included list.

## Match and style layer surfaces

Every Layer Rule includes a layer namespace matcher. It is an RE2 pattern and
uses the same trusted Save-time parser behavior as Window Rule patterns.

The reviewed Layer Rule effects are:

- disable animation;
- blur behind the surface and blur its popups;
- blur alpha threshold;
- dim surrounding content;
- X-ray blur;
- a pinned layer animation style;
- the rule's signed order value;
- above-lock level 0 through 2; and
- screen-sharing exclusion.

**Rule order value** uses the same canonical, lossless signed safe-integer
field as Window Rule border size. This avoids silently narrowing a valid value
to a 32-bit spin box.

## Save, conflicts, and recovery

**Save & apply** submits the complete ordered Window and Layer Rule collections
as one operation. HyprShelld validates the full desired state, persists one
whole-snapshot compare-and-swap revision while preserving every other
compositor setting, then reloads and verifies that exact revision. Saving one
tab can therefore never silently overwrite an unsaved change on the other tab.

The Rules sidebar item shows **Unsaved** while either collection differs from
the synchronized baseline. If another compositor page saves a newer revision,
it changes to **Review** and preserves both collections. Select **Load current
settings** explicitly when you are ready to discard the draft; Settings never
silently rebases it.

If the rules schema, action catalog, or full desired-state authority cannot be
authenticated, existing scalar pages may remain readable while Rules becomes
read-only. The Rules status identifies the authority-verification failure and
shows the scoped parser or projection error as plain text. A service outage,
read-only configuration, active display test, shared visual-source transition,
or another compositor mutation also locks editing without discarding the
draft.

If activation fails after a successful Save, **Retry apply** targets the exact
saved compositor revision. **Restore last working configuration** opens a
cancel-first confirmation because recovery can replace every pending
compositor setting, not only rules. It records the verified recovery state as a
new monotonic revision rather than editing either collection in place.

Return to [Settings](settings.md) or the [HyprShelld User Guide](index.md).
