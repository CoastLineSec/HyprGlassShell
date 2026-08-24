# Hyprland configuration contract v1

This directory defines the strict data boundary between HyprShelld Settings and
the compositor configuration authority. Lua is a generated artifact and is not
accepted anywhere in the desired-state document.

The first fully reviewed authority is tagged Hyprland `0.56.1`. The catalog is
usable across the `0.56.x` minor after runtime capability intersection:

- `0.56.1` is an exact reviewed match.
- Other `0.56.x` patches are supported-minor matches and must be intersected
  with the live compositor's advertised scalar options.
- `0.55.x` is migration input, not writable v1 authority.
- A newer minor is preserved read-only until its catalog is reviewed.
- An unknown major is unsupported.

`config.schema.json` describes the typed desired state. Every complex surface is
an ordered array with a stable record ID. Empty arrays are meaningful and are
therefore always present. `revision` is a canonical decimal string so it can be
round-tripped without JavaScript's integer precision limit. `overrides` contains
only explicit non-default scalar values, keyed by catalog option ID.
`catalogDigest` pins the canonical scalar catalog. `actionCatalogDigest`
separately pins the canonical action catalog, a newline separator, and the exact
raw config schema bytes referenced by that action catalog.
Each catalog option carries an explicit `writable` capability. The tagged
`input:scroll_points` custom-acceleration grammar and
`scrolling:explicit_column_widths` CSV grammar remain inventoried for
compatibility but are non-writable in managed v1; both require future typed
surfaces. Tablet and touch-device output bindings are likewise non-writable
until the device UI has a typed live-monitor selector, and `devices[].overrides`
therefore has no `output` field in v1. Desired-state validation rejects an
override for any non-writable option.

`workspaceRules` contains one protected HyprShelld record whose reserved ID,
exact `f[1]` selector, enabled state, empty monitor/layout fields, and
`gaps_out: [0, 0, 0, 0]` effect are schema-closed. Ordinary workspace rules
cannot use the reserved ID or bracket selector. Hyprland evaluates matching
workspace rules in declaration order and later values replace earlier ones, so
the authority keeps this internal record unique and final.

Every scalar catalog entry also carries an explicit managed output `module` and
an exact `luaPath` array. Registry punctuation is never interpreted by the
renderer. For example, `general:col.active_border` emits through
`["general", "col", "active_border"]`, while the upstream
`input-capture:capture_modifiers` spelling emits from the `input` module through
`["input_capture", "capture_modifiers"]`.
Complex-surface catalog records carry the same module/path metadata for their
tagged `hl.*` constructor, including `define_submap`, so no emitter dispatch is
selected from a collection name.

The `devices` collection is compatibility-preserved and selected by a
session-assigned Hyprland name, not a stable hardware identity. Its catalog
mode is Restart: additions, removals, reordering, record metadata, enabled
state, and override changes are restart-required in both directions. A
nonempty collection without an applied baseline is also Restart. Exact
unchanged device records do not elevate an unrelated reloadable delta, and
Recovery remains Reload when it restores the exact applied device collection.
Only a dedicated receipt-bound transaction may later prove a narrower live
device change.

The compatibility-preserved `bindings` and `submaps` collections also use
Restart. Additions, removals, reordering, and every record change are
restart-required in both directions until a dedicated receipt-bound shortcut
transaction can prove a narrower transition. Exact unchanged collections do
not elevate an unrelated reloadable delta. Recovery remains Reload only when
both collections exactly match the applied baseline; restoring either one to a
different value remains Restart.

The complex records mirror the public tagged `0.56.1` Lua API with closed keys
and types. Monitor and device aliases are canonicalized, gesture callbacks are
excluded, binding dispatchers are a closed inventory, and `exec_cmd`/`exec_raw`
are deliberately excluded so desired state cannot contain arbitrary commands.
Workspace `on_created_empty` is likewise excluded from managed records. Custom
commands and unsupported Lua belong in `user-custom.lua`, outside the generated
authority. Startup applications are deferred to a later supervised service and
are not accepted as Hyprland configuration state. Environment records only
describe compositor-session environment ownership.

Curves are declared before animations. Submaps are declared before bindings;
an empty reset target means Hyprland's default map. Runtime validation enforces
unique names and IDs, curve references, submap reset/reference integrity and
acyclic reset chains, duplicate normalized chords, and the tagged binding-option
invariants that cannot be expressed locally in JSON Schema.

The tagged `hl.bind` parser exposes a `mouse` option but does not assign it to
the keybind record. V1 therefore rejects that non-functional option; mouse
buttons are represented through the binding key symbol instead. `switch:*`
selectors are also deferred until they have a stable typed representation.

`catalog.schema.json` describes the checked-in scalar catalog and its UI/apply
metadata. The catalog is extracted from the tagged upstream C++ value registry;
the UI tier, risk, apply mode, and string-enum metadata are a reviewed semantic
overlay. Runtime `hyprctl descriptions` is only a compatibility probe.

`action-catalog.schema.json` describes the complete managed action inventory.
Its 47 compositor dispatchers are the tagged public `hl.dsp` surface minus raw
execution and the untyped `layout` and `window.set_prop` mini-languages; every
record points to its exact typed argument schema and declares its exact Lua
invocation shape. Renderer code therefore
never guesses whether an argument object is omitted, passed as a table, or
reduced to a scalar field. It also inventories the closed default-app,
HyprShelld, and gesture semantic action namespaces, their broker/table
marshalling, and intentional exclusions.

`generation-manifest.schema.json` binds one immutable generated Lua tree to the
exact snapshot, scalar and action catalogs, renderer contract, activation nonce,
and a map keyed by canonical relative file path. Schema validation alone never
authorizes activation. The compositor authority must require the entrypoint to
be a file-map key, recompute every file size and SHA-256, recompute `generation`
as SHA-256 of canonical manifest JSON with the `generation` member omitted, and
cross-check the target against the pinned compatible range. Live Reload
activation additionally requires the exact manifest activation nonce from the
same Hyprland process, the following generic config-reloaded boundary, and an
empty strict JSON config-error result.

Live activation treats the compositor session's user ID as its local trust
boundary. Retained authority directory descriptors and canonical-name checks
fail closed when configuration roots move or are replaced, and production
D-Bus name ownership prevents a second compositord instance. These mechanisms
are not a privilege boundary against a malicious process running as the same
user; such a process can interfere with that user's files and session IPC.

The source inventories and provenance are under `tests/fixtures/hyprland/`.
`source-manifest.schema.json` pins all three `VERSION` files, the complete
`0.55.0` and `0.56.1` scalar registries, all tagged source files used to
qualify the complex grammar, and
the focused `0.56.0` startup sources needed to qualify the loader readiness
guard across the full supported patch range. It separately pins
`src/debug/HyprCtl.cpp` and `src/output/Monitor.cpp` at both `0.56.0` and
`0.56.1`; extraction also asserts the exact `j/monitors all` JSON field
inventory, mode formatting, mirror/disabled semantics, reserved-edge order,
short-description construction consumed by display discovery, and the client
and workspace fields consumed by the maximize projection. A separate 14-entry
inventory pins the seven maximize sources at both patches. Extraction verifies
that `f[1]` means internal maximize mode, later matching workspace rules win,
workspace `gaps_out` supplies both tiled and floating work areas, true
fullscreen uses the monitor box, maximize uses the reserved work area, and the
fullscreen IPC event and handler strings used to invalidate/project state
remain present.
A separate 12-entry window-group behavior inventory pins six runtime roles at
both `0.55.0` and `0.56.1`. Extraction verifies automatic grouping, insertion
order, focus after removal, ordinary-window versus groupbar drag modes, group
and floated-to-tiled merge gates, and that silent and ordinary workspace moves
both traverse the version-appropriate solitary-group controller.
A separate 42-entry Advanced runtime inventory is split across eighteen
`0.55.0`, four `0.56.0`, and twenty `0.56.1` records. The earlier and latest
tags pin the scalar registry, reload manager, property refresher, session-lock
manager, renderer, element renderer, texture-pass declaration and
implementation, GL element renderer, OpenGL renderer, surface-pass declaration
and implementation, surface fragment shader, version-specific monitor
implementation, screenshare session, color-management definition, framebuffer
metadata implementation, and version-specific monitor-resource implementation.
The `0.56.0` subset pins its scalar registry, reload manager, input manager,
and input-capture protocol implementation; the latest tag also pins both input
consumers.
Its frozen option order is
`misc:allow_session_lock_restore`, `misc:lockdead_screen_delay`,
`misc:disable_scale_notification`, `misc:render_unfocused_fps`,
`misc:screencopy_force_8b`, `input-capture:capture_modifiers`, and
`input-capture:enforce_barriers`, followed by `misc:disable_hyprland_logo`,
`misc:disable_splash_rendering`, `misc:session_lock_xray`, and
`misc:session_lock_blur`, followed by
`xwayland:use_nearest_neighbor` and
`render:expand_undersized_textures`, `render:direct_scanout`, and finally
`render:fp16_sdr_tf` and `render:xp_mode`. The first five Advanced values raised
the aggregate authored scalar total from 187 to 192, the global touch-and-tablet
Input family raised it to 197, and the four-value rendering family raised safe
coverage to 201 of 282 values. The XWayland caution control raises the authored
scalar total to 202, and the undersized-texture caution control raises it to
203. The direct-scanout caution control raises it to 204, the SDR work-buffer
transfer control raises it to 205, the workspace-underlay caution control
raises it to 206, and the two input-capture controls raise it to 208, while
safe coverage remains 201 of 282. Cursor behavior then raises the authored
total to 216 and safe coverage to 209; blur modulation raises them to 221 and
214, the pointer-focus threshold to 222 and 215, and border-inclusive shadow
bounds to 223 and 216. Window corner power then raises them to 224 and 217.
Window shadow rendering raises them to 227 and 220. Tablet mapped-region
fallbacks raise them to 230 and 223. Window shadow offset raises them to 231
and 224. Active-layout shortcut resolution raises them to 232 and 225.
Window shadow scale raises them to 233 and 226.
Seven caution controls are authored, and fourteen accepted caution or risk
controls remain unauthored.
The raw safe remainder is 56 values: 13 common and 43 advanced. Three advanced
values are deliberate Settings exclusions, leaving 53: 13 common and 40
advanced. After 25 unauthored safe values with an accepted higher-level owner,
28 remain unresolved: nine common and 19 advanced.

Logo wallpaper and splash text are independent render gates. Disabling the
Hyprland logo skips the wallpaper resource path and clears to the configured
background color; an unavailable wallpaper uses the same color fallback, while
splash text may still render unless its own option is disabled. Session-lock
X-ray keeps the underlying workspace in the render path and omits the opaque
black primer. Session-lock blur is absent from `0.55.0`, is cataloged since
`0.56.0`, and has its runtime authenticated at `0.56.1`: it is enabled only
when both blur and X-ray are enabled, and the surface pass and shader restrict
the blurred background to non-opaque or partially transparent lock pixels.
Extraction also pins full-reload refresh, blur-buffer invalidation, monitor
damage or full-frame scheduling, and mutation-checks every implementation
fragment in the closed inventory.
Nearest-neighbor filtering is registered default-on at both tags. The global
value is read for each X11 window render and is ORed with the matching Window
Rule's per-window request. The selected flag is copied through the surface pass
into element render state, reset after that surface, and selects `GL_NEAREST`
instead of the texture's normal magnification and minification filters. Full
reload schedules `REFRESH_ALL`; its layout/window refresh damages the monitor,
and the damage path schedules a frame, so already-mapped windows read the new
filter on the following render.
Undersized-texture expansion is also registered default-on at both tags. Its
runtime read is inside the non-X11 surface path and outside the fractional
misalignment correction branch. For a scale-aware surface whose projected size
exceeds its expected destination, the enabled value clamps the projected-to-
expected ratio and expands the bottom-right texture coordinate; scale-unaware
and X11 surfaces do not take that expansion path. The resulting primary-surface
UV is enabled on each texture pass, copied through the texture-pass record and
GL element renderer, and consumed as custom OpenGL vertex coordinates. An
identity UV is replaced by the no-custom-UV sentinel. The same closed Reload,
full refresh, monitor damage, and frame-scheduling proof establishes repaint of
already-mapped surfaces after the value changes.
Direct scanout is the exact `0..2` enum `disable`, `enable`, and `auto`, with
`disable` (`0`) as its default. A refreshed frame re-enters the monitor's
attempt path while a solitary candidate or prior scanout state exists. Disable
mode rejects that attempt; an active or retained scanout then follows
`handleDSleave`, which clears the retained client and active flag, restores the
previous DRM format, and dirties the blur framebuffer before ordinary
composition continues. Enable mode skips only Auto's content-type restriction:
it still requires the same otherwise-eligible solitary candidate and every
remaining blocker. Auto additionally requires the monitor's exact internal
fullscreen mode and a fullscreen window reporting game content.

Solitary eligibility is narrower than merely having a large window. Both tags
require an opaque, exact-monitor-size and position fullscreen candidate with a
single usable surface, no special-workspace presentation, notifications,
focused error overlay, session lock, drag-and-drop, workspace fade or offset,
visible overlay or top layer, allowed floating overlay, or visible special
workspace; `0.56.1` also rejects monitor fadeouts. The direct-scanout gate then
rejects mirrors, a pending screen-capture block, a monitor software-cursor
lock, missing texture or buffer state, buffer size or transform mismatch,
non-DMA buffers, scRGB, and the pinned SDR/HDR color-management mismatches.
Passing those predicates only permits an attempt. The output state test or
commit may still fail, so the setting does not promise hardware realization or
support on a particular output, GPU, format, or client.
`render:fp16_sdr_tf` is the exact `0..1` enum `monitor` (`0`) and `linear`
(`1`), with Display transfer/`monitor` as the default. It is available since
`0.55.0` and uses Reload. The value selects the transfer function for SDR
content only after Hyprland has entered its internal FP16 or ICC work-buffer
path. It does not enable FP16, HDR, ICC, or color management, and it does not
change the monitor's output transfer function or profile. With Hyprland's
preserved `render:use_fp16` policy in Automatic mode, an ordinary sRGB monitor
without ICC may therefore leave this setting dormant: `workBufferImageDescription()`
returns the monitor image description when FP16 is not in use and ICC is
absent.

Both tags calculate FP16 use from the exact Disabled, Enabled, and Automatic
policy: Enabled always selects it, Disabled never selects it, and Automatic
selects it when the monitor description is not sRGB/gamma-2.2 with sRGB
primaries. Once an internal work buffer is needed, PQ, HLG, and extended-linear
descriptions remain linear regardless of this option. Hyprland `0.55.0` also
treats its `windowsScRGB` description flag as linear; the revised `0.56.1`
branch has no such predicate. The Linear choice takes the same cached-linear
branch for SDR. The default Display transfer choice rebuilds the cached SDR
description with `chooseTF(m_sdrEotf)`, sRGB named primaries, and BT.709
coordinates. `chooseTF` preserves the pinned gamma-2.2 and sRGB mappings and
resolves Auto through the global SDR transfer setting, with gamma 2.2 as the
Auto-on-Auto fallback.

The exact extended-linear description uses extended-linear transfer, sRGB
named/BT.709 primaries, and luminance `0/10000/80` for minimum, maximum, and
reference. Monitor resource selection recomputes both the FP16 DRM format and
work-buffer description, constructs resources with that description, and
updates them when the description changes. Resource initialization stamps new
blur and work framebuffers; the setter updates the blur framebuffer and every
existing reusable work buffer, monitor-mirror framebuffer, and mirror texture;
`0.56.1` additionally invalidates its mirror framebuffer. Framebuffer setters
retain metadata even before a texture exists and copy it to an existing
texture, and getters prefer current texture metadata. Renderer forwarding and
OpenGL source/target selection then use the monitor work-buffer description,
current framebuffer metadata, reviewed fallbacks, and the exact `needsCM`
comparison. Full Reload first resets every scalar to its registry default
before reapplying the authored config, which closes Linear-to-elided/default
convergence. It then reaches the property refresher, monitor damage, and frame
scheduling. Each `beginRender()` assigns the target monitor and calls
`resources()` before render setup, so the scheduled frame recomputes the
description and converges cached and retained framebuffer metadata.
A separate 27-entry window-behavior inventory pins 17 authored behavior
scalars across both tags: the registry, directional and group actions,
directional window query, pinned-fullscreen and ANR handling, Dwindle, Master,
Scrolling, and Monocle movement, input focus routing, window activation and
fullscreen transitions, focus-state arbitration, window swallowing, size-rule
application, and tiled target sizing. In `0.55.0`, directional query and pinned fullscreen
behavior share `Compositor.cpp`; `0.56.1` splits them into `WindowQuery` and
`FullscreenController`. Extraction verifies the exact defaults and bounds,
history-versus-shared-edge focus ranking, fullscreen and group-first focus
cycling, global and per-group lock gates, temporary unpin and restore around
fullscreen, monitor-edge focus and tiled-window movement in all four supported
layouts, ANR gating, drag-and-drop and monitor focus, activation requests,
fullscreen/maximized focus transfer, close-time mode retention, class and title
RE2 swallowing filters over the parent-process chain, swallow/unhide lifecycle,
and tiled min/max centering. Version-specific layout-managed fullscreen and
swallow-chain safety behavior remains explicit rather than being treated as
identical source text.
A separate 10-entry groupbar inventory pins the value registry, group
decoration lifecycle, groupbar renderer, property refresher, and Lua font-weight
parser at both `0.55.0` and `0.56.1`. Extraction verifies the closed typed
option declarations, version-specific singleton visibility, reserved geometry,
stacked and title/gradient dependencies, rounding, spacing, input behavior,
font fallback and weight transport, gradient/color/blur ownership boundaries,
and the refresh path used when visibility-affecting values change.
A separate 13-entry workspace-behavior inventory pins six source roles at
`0.55.0` and the same closed behavior chain split across seven files at
`0.56.1`: scalar definitions, workspace actions, the history implementation
and API, window-relative cursor placement, moved-workspace placement, and the
ordinary-warp gate. Extraction verifies the six authored workspace options,
while treating `cursor:no_warps` and `cursor:persistent_warps` only as
interacting runtime evidence. It proves current-workspace back-and-forth and
MRU-cycle behavior, ordinary switching and moved-workspace placement hide
paths, monitor-versus-window centering, independent regular and special warp
modes, mode `2` force bypass of `no_warps`, and persistent relative-window
coordinates with a window-center fallback. The 0.55 compositor contains both
placement and warp gates; 0.56 moves those gates to
`WorkspacePlacementController` and `PointerController` respectively.
A separate 44-record Input-behavior authority contains 38 Hyprland records,
nineteen ordered roles at each of `0.55.0` and `0.56.1`, plus six dependency
records. The Hyprland records pin the scalar registry, reload reset and Lua
global-to-device fallback, device-field binding, input refresh, retained input
state and transform matrices, pointer/touch/tablet configuration and events,
primary selection, renderer hide state, physical mouse identity, monitor-layout
clamping, cursor actions, persistent window coordinates, and the
version-specific ordinary-warp wrapper. Extraction verifies that forced
no-acceleration selects the unaccelerated cursor delta without rewriting
sensitivity or acceleration-profile settings; global device rotation is
bounded to 0 through 359 degrees, applies only when libinput reports support,
and yields to an explicit per-device rotation; and middle-click paste gates
ordinary primary-selection requests independently of touchpad middle-button
emulation. It also proves the global touch and tablet defaults, their
`REFRESH_INPUT_DEVICES` reload path, explicit per-device override precedence,
the exact transform matrices, and identity reapplication on global Reset.

The same closed Input-behavior inventory now proves
`input:follow_mouse_threshold` as a Float with default `0`, its zero-initialized
movement accumulator, and its single live runtime consumer at each tag. The
effective focus-follow mode is forced to `1` only while drag-and-drop is active
and `misc:always_follow_on_dnd` is enabled; otherwise it is the configured
`input:follow_mouse` mode. In effective mode `1`, movements less than `0.5`
seconds apart accumulate the result of the pinned
`MOUSECOORDSFLOORED.distance(m_lastCursorPosFloored)` call; other modes or a
longer gap reset the accumulator. The extractor deliberately does not assign a
metric name that the pinned sources do not establish. A different hovered
window is focused only when the accumulated value is strictly greater than the
live threshold or the call is a refocus. The `no_follow_mouse` Window Rule
blocks that ordinary focus path, while refocus bypasses it. Ordinary motion
calls `mouseMoveUnified` before resetting the movement timer. Hyprland `0.56.1`
returns on captured motion before both operations, so captured movement does
not advance this threshold state. The already-pinned configuration reset and
replay path authenticates default and non-default Reload convergence without a
new source record.

That closed inventory also proves `input:resolve_binds_by_sym` as a Boolean
with default `false` and ordinary Input Reload refresh. The global value is a
fallback: an exact saved per-device `resolve_binds_by_sym` value wins. Every
keyboard receives the effective value before Hyprland's unchanged-keymap early
return, so resetting the global value to `false` is symmetric. Each later key
event then selects either that keyboard's active-layout symbol state or the
primary globally configured translation state. Keycode-based shortcuts are
unchanged. This control does not rewrite a keymap, binding, submap, chord,
action, pressed-key record, or per-device override.

The cursor-behavior proof covers exact defaults and bounds, the 500-ms hide
ticker, keyboard/touch/tablet producer and renderer-consumer ordering, timeout
resets on motion and physical buttons but not wheel input, option-off clearing,
and hide-image restoration. It separately verifies hotspot-centered
multi-output clamping, strict interior remembered coordinates with center
fallback, ordinary-versus-forced `no_warps` behavior, monitor-focus retention,
the raw-manager warp-back bypass, and tablet mapped-region routing. The tablet
proof follows global fallback and exact per-device precedence through reload,
then covers output-relative versus compositor-absolute offsets, optional size
replacement, relative-input dormancy, and final componentwise absolute
projection. Each tag's dependency records pin its
exact `flake.lock` Hyprutils revision and the identical reviewed
`Vector2D.hpp` and `Box.cpp` geometry. Those sources prove componentwise strict
vector comparison and multiplication, vector addition, box translation,
epsilon-based empty sizing, strict box interior, half-open containment,
midpoint, and epsilon-adjusted closest-point behavior rather than inferring
geometry from Hyprland alone. The final independent mutation inventory contains
280 Hyprland semantic fragments for `0.55.0` and 285 for `0.56.1`; Hyprutils
contributes a separate 35 geometry fragments for each pinned revision.
Dependency-pin JSON checks are not counted as semantic fragments.
A separate 34-entry input-device inventory pins seventeen discovery and runtime
roles at both `0.55.0` and `0.56.1`. Extraction verifies the five-array
`j/devices` wire, its bare `active_layout_index: none` branch, exact field
counts, and the omission of enabled, virtual, capability, touchpad, and count
claims. It also verifies selector normalization and connection-order suffix
allocation; Lua device-map clearing, deferred refresh order, exact per-device
lookup and global fallback; keyboard, pointer, touch, and tablet application;
physical and virtual creation timing; and the version-specific pointer-manager
path. The pinned negative seams prove why generic device changes remain
Restart-only: pointer removal has no no-config reattach path, an explicit
per-device touch or tablet transform of `-1` suppresses matrix assignment and
has no identity reset, tablet active area has no empty reset, and virtual
identity/keymap timing cannot be inferred from the discovery wire. This does
not apply to the separate global transform controls, whose closed 0-through-6
range makes Reset code 0 actively reapply the identity matrix; explicit
per-device overrides still take precedence over those global fallbacks.
A separate 38-entry gesture inventory pins nineteen parser, reload, registry,
base-gesture, and executable-action roles at both `0.55.0` and `0.56.1`.
Extraction derives the exact ten canonical action IDs from the tagged
`hl.gesture` Lua branches, including compatibility `unset`, rather than
trusting only the checked-in action catalog. It verifies ordered
shadow-before-append registration and exact-match removal, both shortcut
inhibit gates, phase-two registry clearing before Lua replay, and all nine
executable action implementations. It also pins the runtime asymmetries that
the managed editor must preserve: pinch ignores the configured scale,
Scroll Move is a pinch no-op, and live cursor zoom is a no-op for non-pinch
events. The scale and `disable_inhibit` fields still participate in exact
ordered removal, so compatibility `unset` records cannot be normalized.
A separate 16-entry animation inventory pins the version-specific Lua binding,
animation tree, inherited manager boundary, linear-curve registration, style
validator, Lua reload lifecycle, and window, layer, workspace, and angle-style
consumers at both `0.55.0` and `0.56.1`. Extraction verifies exact Bezier point marshalling,
spring fields, curve-reference tagging, every leaf-parent edge, the inherited
`default` and explicit `linear` references, and the runtime style families.
Each tag's exact `flake.lock` Hyprutils revision and the dependency's curve-map
header, implementation, and typed animated-variable dispatch are pinned too.
They prove the built-in Bezier and spring defaults, separate typed maps,
replacement assignment, and `spring:` dispatch rather than inferring those
semantics from Hyprland alone. The tagged `hl.curve` path deliberately has no
reserved-name guard: defining a custom curve named `default` or `linear`
replaces that managed runtime reference, and the managed editor preserves that
existing upstream behavior explicitly. Lua reload does not clear the dependency
maps. Generated animation modules therefore restore the exact built-in defaults
before emitting custom records, while transaction classification requires a
compositor restart whenever the logical name-to-type map changes: additions,
deletions, renames, and Bezier/Spring type changes all restart. Only reorder or
stable-ID changes and same-name, same-type parameter edits remain reload-safe.
This bidirectional equality is required for rollback too: a failed target
activation followed by the prior generation cannot otherwise remove a
target-only persistent map entry. Within an unchanged map, the pinned assignment
replaces the selected entry deterministically.
They can be reproduced without network access from official tagged source trees:

```sh
python3 tools/hyprland/extract_contract.py \
  --source-055 /path/to/Hyprland-0.55.0 \
  --source-0560 /path/to/Hyprland-0.56.0 \
  --source-056 /path/to/Hyprland-0.56.1 \
  --hyprutils-055 /path/to/hyprutils-a2dbd8a4 \
  --hyprutils-056 /path/to/hyprutils-5f03477a \
  --output-root .
```

Use `--check` in qualification jobs to reject source drift, stale generated
files, unexpected option additions/removals, or unversioned Hyprland wiki links.
Before reading an inventory, the extractor verifies `VERSION`, both scalar
registries, every complex-surface source, and the focused startup sources
and monitor-query, maximize, window-group, appearance-behavior,
Advanced-runtime, window-behavior, and misc-exclusion sources against immutable
SHA-256 pins for the reviewed tags.
A focused appearance extraction verifies its closed 101-record paired inventory:
49 ordered Hyprland `0.55.0` records followed by 52 ordered `0.56.1` records.
The landed semantic tuples contain 754 independently mutation-checked fragments
for `0.55.0` and 829 for `0.56.1`; these are measured per version rather than
inferred from a shared estimate. The appearance inventory adds no dependency
records. Its shadow-offset and shadow-scale proofs reuse the existing exact
six-record Hyprutils geometry dependency inventory and its unchanged
component-wise `CBox::translate` and center-preserving `CBox::scaleFromCenter`
contracts. The inventory pins the exact active, inactive, and
fullscreen opacity defaults/ranges and their fullscreen-first, focused, unfocused,
map-time, Window Rule composition, and mapped-window refresh branches. It also
pins the exact modal, inactive, special-workspace, and rule-driven dimming
defaults/ranges; focused/no-dim/disabled and inactive-strength behavior; modal
composition; window and layer dim-around rendering and fadeout capture; and
full-reload propagation. Hyprland `0.55.0` reads special-workspace dimming in
the renderer. Hyprland `0.56.1` instead renders a monitor-cached value that is
reassigned by `setSpecialWorkspaceVisualState()` on a special-workspace visual
transition; the window-state refresher does not retarget that cached value.
The extraction preserves and mutation-tests that tagged-version distinction
instead of claiming immediate retargeting for an already-open special
workspace.
The same inventory pins ten blur mechanics and context controls. Blur size is
an integer defaulting to `8` with registry range `0..100`, and passes default to
`1` with registry range `0..10`; the renderer clamps passes to `1..8` and the
damage radius clamps size to `1..40`, while the blur shader still receives the
configured size. Ignore opacity and new optimizations default on; X-ray,
special-workspace blur, popup blur, and input-method blur default off. Popup
and input-method alpha thresholds default to `0.2` with range `0..1`.
The exact paths are `decoration:blur:size`, `decoration:blur:passes`,
`decoration:blur:ignore_opacity`, `decoration:blur:new_optimizations`,
`decoration:blur:xray`, `decoration:blur:special`,
`decoration:blur:popups`, `decoration:blur:popups_ignorealpha`,
`decoration:blur:input_methods`, and
`decoration:blur:input_methods_ignorealpha`.
The same closed inventory authenticates five floating-point blur-modulation
registrations in upstream order: noise defaults to `0.0117` over `0..1`,
contrast to `0.8916` over `0..2`, brightness to `1` over `0..2`, vibrancy to
`0.1696` over `0..1`, and vibrancy darkness to `0` over `0..1`. Contrast passes
through `gain()` before blur. Brightness intentionally uses
`max(1, brightness)` in prepare and `min(1, brightness)` in finish. Noise is a
deterministic spatial hash added after blur. Vibrancy is applied during every
downsample pass and divided by the effective pass count; vibrancy darkness
modifies that calculation and has no independent shader effect when vibrancy is
zero. The core formulas are shared, while the pinned prepare and finish
transfer-function surroundings remain tag-specific, so the proof does not
assert pixel identity across color-managed versions.
Qualification covers the ignore-opacity alpha expression, optimized blur-FB
and X-ray/window-rule branches, popup and input-method enable/threshold gates,
and their unchanged live threshold values. Popup snapshot/fade-out capture
alone floors the selected global or layer-owner Rule threshold to `0.01`.
Full reload reaches the blur-FB refresher in both tags after scalar reset,
generated-value replay, and the
post-reload `REFRESH_ALL` schedule. Hyprland `0.56.1` additionally attaches
`REFRESH_BLUR_FB` to all fifteen reviewed blur definitions and dirties,
force-frames, and schedules each monitor. Hyprland `0.55.0` reaches the dirty
cache through its generic refresh and layout-damage path. Optimized and X-ray
rendering insert an undiscardable pre-blur element, delegate through the render
backend, and clear both cache flags only after recomputation. With no eligible
blur consumer, the dirty state is retained for a later eligible frame rather
than being falsely marked current. Hyprland `0.55.0` reads special blur and
popup-fadeout settings live in the renderer; `0.56.1` captures special blur on a special
workspace visual transition and popup blur when the fadeout is created. The
semantic mutation tests preserve those reload, shader-order, uniform-mapping,
cache, and transition boundaries.
The inventory also authenticates `decoration:border_part_of_window` as a
default-on live flag. A border contributes to the main-window box only while
that flag is enabled and the window has a nonzero border, has not refused
borders through X11, and has not disabled decoration through a Window Rule.
The decoration positioner unions every flagged border extent into the main
surface box. The drop-shadow decoration caches that included-decoration box,
uses the cached box for the current shadow calculation, then refreshes the
cache through the window-decoration update chain for the next calculation.
Both supported tags construct the border and shadow decorations for XWayland
and XDG windows and route full reload through workspace, window, and decoration
updates. The `0.55.0` refresher submits full monitor damage through its layout
refresh path. The `0.56.1` refresher additionally uncaches decoration
positioning, forces full frames, damages each monitor, and explicitly schedules
the frame. The source proof also pins complete border and shadow damage-region
submission, rather than treating a changed positioning cache as sufficient
repaint evidence.
The same inventory authenticates `decoration:rounding_power` as a floating-point
global value defaulting to `2` with inclusive registry bounds `2..10`. A
floating-point Window Rule value takes precedence when present; otherwise
`CWindow::roundingPower()` uses the global value clamped to `1..10`. The
selected Rule or global base radius is multiplied by `rounding_power / 2`, so
power changes effective corner geometry as well as the superellipse shape.
Window, border, surface, texture, and shadow render data carry the floating
value through the render passes and OpenGL uniforms. The pinned border,
rounding, surface, and shadow shaders use the corresponding power and inverse-
power formulas rather than substituting a circular exponent.

The same inventory closes the compositor-local shadow-rendering chain.
`decoration:shadow:range` is an integer defaulting to `4` over `0..100`,
`decoration:shadow:render_power` defaults to `3` over `1..4`, and
`decoration:shadow:sharp` defaults off. Range expands all four sides of the
included-decoration box, is returned with the shadow render data, and is
forwarded through the renderer and both gradient overloads in `0.56.1`. Sharp
mode dispatches the resulting box through the solid rounded-rectangle pass;
soft mode instead forwards range and the live falloff power to OpenGL, where
power is clamped to `1..4` before both values reach the shader uniforms. The
shader proof covers corner and edge falloff and the strict guards that avoid a
range division when range is zero. Hyprland `0.56.1` attaches window-state
refresh metadata to all three registrations; the already-pinned reload,
decoration-refresh, damage, and frame-scheduling chain supplies convergence.

The same inventory authenticates `decoration:shadow:scale` as a floating-point
value defaulting to `1` with inclusive registry bounds `0..1`. Hyprland
`0.56.1` attaches window-state refresh metadata to the registration. Both
supported tags bind the value once in the sole drop-shadow render-data path
and clamp it to the registered bounds. Range expansion happens first; the box
is then scaled around its center before the separately configured offset is
applied and the decoration extents are recomputed. A scale of `0` reaches the
sub-one-size guard and produces no shadow. Every nonzero result is shared by
the sharp and soft branches, and the saved value remains dormant while window
shadows are disabled. The pinned Hyprutils proof establishes the exact width,
height, X, and Y arithmetic used by `CBox::scaleFromCenter` rather than
inferring center preservation from its name.

The same closed inventory authenticates `decoration:shadow:offset` as a
two-component vector defaulting to `[0, 0]`, with each component accepted from
`-250` through `250`. Hyprland `0.56.1` attaches window-state refresh metadata
to the registration. Both supported tags bind the value once in the sole
drop-shadow render-data path. Range expansion and the preceding shadow-scale
center transform happen before exact X/Y translation;
the translated box determines decoration extents, then receives the window's
floating offset and monitor scaling. A sub-one-size guard prevents an invisible
translated box from entering either the sharp rectangle or soft-shadow render
branch. The returned box is shared by both branches, and repositioning reports
changed extents through the decoration positioner. The unchanged Hyprutils
geometry dependency proof establishes that `CBox::translate` adds the supplied
X and Y components independently; no new dependency record is introduced.

The same inventory authenticates the complete managed inner-glow safety
boundary. `decoration:glow:enabled` defaults off,
`decoration:glow:range` defaults to `10` over the unchanged inclusive catalog
range `0..100`, and `decoration:glow:render_power` defaults to `3` over `1..4`.
Both supported decorations stop before drawing when glow is disabled, transport
the range through an integer parameter after multiplying it by the monitor
scale, and pass the resulting value through the fragment wrapper to the shared
inner-glow shader. The shader divides once by `range` in its alpha falloff and
once by the same value as `k` in its smooth-min calculation. The proof also
pins the output-management protocol's stock `0.1..10` scale admission, transfer
into monitor configuration state, monitor-rule assignment and application, and
the monitor's replacement of `0.1` or lower by its default scale while
consuming a requested scale above `0.1` directly. Consequently, raw ranges
`0..9` can truncate to zero at a valid consumed fractional scale just above
`0.1`; raw range `10` cannot. HyprShelld therefore requires range `10` only
when enabling managed glow. This is not a Hyprland minimum: the upstream
catalog remains `0..100`, disabled low values remain representable exactly,
and HyprShelld does not alter the compositor runtime. The existing reset/load,
post-reload refresh, decoration update, damage, and repaint provenance closes
the managed Reload chain without changing catalog, renderer, or public-schema
bytes.

Curved-corner pointer and resize hit testing intentionally truncates the
effective radius and power to integers before its four corner power tests.
Fractional values can therefore render a curve whose interaction boundary is
an integer approximation. Full reload resets and replays the value before the
generic refresh chain in both tags. Hyprland `0.56.1` additionally marks both
window-state and blur-framebuffer refresh in the registry, then uncaches
decorations, damages monitors, forces full frames, and schedules repaint. That
version snapshots rounding radius and power when a window close fadeout is
created and reuses the cached values for its pre-blur pass, so an already-
closing surface can retain the prior power until its fade completes.
A focused Advanced runtime extraction verifies the exact registry defaults and
bounds: lock restore is off, the missing-lock delay is `1000` milliseconds
bounded to `0..5000`, scale-warning suppression is off, render-unfocused rate
is `15` FPS bounded to `1..120`, forced 8-bit screencopy is on, XWayland
nearest-neighbor filtering is on, and workspace-underlay suppression is off.
A new lock request reads the restore toggle at admission time; `0.56.1` retains
its start-locked-command exception. An
accepted lock resets its timer, and the missing-lock predicate is strictly
elapsed time greater than the configured delay. Workspace rendering, the lock
primer, and the missing-lock screen consume that predicate on later render
passes.

The render-unfocused rate applies only to windows admitted by a
`renderunfocused` Window Rule. Its recurring timer schedules the next wake at
`1000 / fps`, skips windows already being rendered, and removes dead windows or
ones that no longer carry the Rule; new admission schedules an unarmed timer.
Scale-warning suppression gates only the future notification produced when an
explicit invalid scale has a clean-divisor suggestion. The corrected scale is
still applied, so this setting neither owns display topology nor disables scale
validation. Forced 8-bit screencopy maps the five reviewed 10-bit monitor
formats to `XRGB8888` only when choosing a preferred read format; it does not
change monitor output depth. Screenshare constraints consume that preferred
format at initialization and recalculate it on monitor mode-change events, so
an existing session is not promised an immediate mid-session renegotiation
without such an event or recreation. XWayland nearest-neighbor filtering is a
render-time choice for X11 windows; a per-window nearest-neighbor Rule remains
an independent enabling path. The selected surface flag reaches the OpenGL
texture filters and is cleared after the surface draw.

Workspace-underlay suppression is read on each workspace render. With an
active workspace, enabling it skips compositor fallback-background rendering,
background-layer pass construction, the post-wallpaper render stage, and
bottom-layer pass construction. Hyprland `0.56.1` also skips the corresponding
background- and bottom-layer fadeout passes. The option does not stop those
layer surfaces or clear prior buffer contents. The earlier no-workspace branch
does not inspect the option and continues to render fallback background plus
background, bottom, top, and overlay layers, including all four layer fadeout
planes on `0.56.1`.

Input-capture modifier forwarding is read on each later keyboard modifier
event. When enabled, Hyprland offers that event's modifier state to the capture
protocol, which forwards it only to an active capture; while capture is active,
the input manager returns before ordinary seat or input-method forwarding. It
does not synthesize or retract held state. Invalid-barrier enforcement is read
for each later barrier request. A valid barrier is a nonzero axis-aligned
segment on one complete monitor edge, after endpoint normalization, and cannot
partially overlap another monitor edge. Enabled enforcement rejects an invalid
barrier with a protocol error, while disabled enforcement records it after
logging. It does not revalidate or remove existing barriers. Neither option
grants capture permission or creates, enables, releases, or otherwise controls
a capture session. The pinned `0.56.1` input manager additionally updates its
merged internal modifier bookkeeping before the captured-event return; the
`0.56.0` implementation does not.

The 42-entry inventory and per-source semantic mutation gate preserve 1,085
ordered fragments—456 for `0.55.0`, 81 for `0.56.0`, and 548 for `0.56.1`—covering every registry,
reload, repaint, lock, render, scale, format, filter, propagation, reset,
direct-scanout eligibility, backend attempt, leave, capture-block, work-buffer
transfer, image-description, framebuffer-metadata, workspace-underlay,
input-capture event and request policy, and event-driven constraint fragment
before manifest emission.
A window-behavior extraction verifies its closed 27-source version-split
inventory and every scalar definition, directional/group dispatcher gate,
focus-ranking branch, pinned-fullscreen and ANR transition, per-layout
monitor-edge movement path, focus/activation gate, swallow candidate and
lifecycle path, and tiled size-rule path before emitting the manifest.
A groupbar extraction also verifies its dedicated ten-source inventory and
semantic assertions before emitting the manifest.
Workspace behavior extraction verifies its closed 13-source version-split
inventory and each switching, history, placement, and warp interaction before
emitting the manifest.
A separate 14-entry binding runtime inventory pins seven sources in each
supported tag. It verifies that reload releases every `__lua` binding registry
reference, clears only the keybinding-definition vector, rebuilds the Lua
state, and replays the config. It also verifies that `hl.bind` stores a numeric
registry reference behind the `__lua` dispatcher, that the dispatcher executes
that reference, and that `hl.define_submap` runs its definition function
immediately while assigning declarations. The process-lifetime current submap
is changed only by `setSubmap`, participates in binding matching, and is not
cleared when definitions are cleared. Consequently, reloading to an empty
binding set does not itself reset the active runtime submap or active-key state.
The `j/binds` response exposes the literal dispatcher and argument, including
`__lua` and its numeric registry reference, but cannot expose or authenticate
the callback's semantics.
A separate six-entry misc-exclusion inventory pins the scalar registry,
Dwindle algorithm, and WindowTarget implementation at both supported tags.
Its 36 independently mutation-checked fragments per tag prove the exact three
registrations and defaults. A full `src`-tree occurrence scan proves that
`misc:animate_mouse_windowdragging` and
`misc:layers_hog_keyboard_focus` have no runtime reference outside the
registry. `misc:animate_manual_resizes` is read only by Dwindle: its value is
converted to a force argument at twelve resize-recalculation call sites, but
that argument is only forwarded recursively and is discarded at every leaf.
Both drag-target completion paths then snap each leaf unconditionally, and
WindowTarget unconditionally warps the real size and position at `0.55.0` or
finishes the animation at `0.56.1`. These three writable compatibility values
therefore remain losslessly preserved and rendered when explicit, but have no
authored Settings controls.
Input extraction verifies its ordered 38-source Hyprland inventory, nineteen
roles per supported tag, and six-record dependency inventory before emitting
the manifest. It independently mutation-checks 280 Hyprland fragments for
`0.55.0`, 285 for `0.56.1`, and 35 Hyprutils geometry fragments for each pinned
revision.
Input-device extraction verifies its closed seventeen-role-by-two-version
inventory and every discovery, selector, refresh, per-kind, virtual-device,
and non-reset assertion before emitting the manifest.
Gesture extraction verifies its closed nineteen-role-by-two-version inventory,
source-derived action IDs, ordered registry/reload behavior, scale and
direction transport, and every executable action before emitting the manifest.
Animation extraction likewise verifies its closed sixteen-source Hyprland
inventory, eight-source Hyprland/Hyprutils dependency inventory, and
cross-version semantic assertions before emitting the manifest.
A caller-supplied or locally modified tree is rejected before trusted tag and
commit provenance can be emitted.

Transaction loading recognizes only the exact deployed catalog predecessor
chain and pre-protected-rule action authority for canonical persisted
snapshots. The catalog rotations and action rotation compose only across their
exact reviewed digest tuples. After immutable applied and pending proof is
checked, transaction loading preserves that proof and migrates desired
authority without changing the revision; older, newer, malformed, or otherwise
unknown authorities remain fail-closed.

After extraction, validate all five Draft 2020-12 schemas, their checked-in
instances, cross-file references, and catalog digests with:

```sh
python3 tools/hyprland/validate_contract.py --root .
```

The validator requires the Python `jsonschema` package. Qualification jobs must
install it in an isolated environment rather than weakening validation when the
dependency is unavailable.
