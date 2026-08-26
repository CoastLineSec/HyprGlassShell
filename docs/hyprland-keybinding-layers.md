# Hyprland keybinding layers

HyprShelld ships a managed baseline of 64 native Hyprland shortcuts adapted
from the 117-row Legacy HyprGlassShell (HGS) keybinding receipt. The baseline
contains only actions that the pinned action catalog can represent as native,
typed Hyprland dispatchers without executing a process or sending an
active-layout-specific message.

The immutable Legacy receipt remains
[`keyboard-shortcuts-reference-v1.json`](../src/hyprland/resources/keyboard-shortcuts-reference-v1.json).
The compiled managed baseline is
[`default_keybindings.cpp`](../src/hyprland/default_keybindings.cpp). The
Legacy receipt is provenance, not an editable or active configuration file.

## Exact 64-row native baseline

The exact Legacy ordinals are:

`14; 28–32; 34–49; 52–65; 83–100; 104–112; 117`

That is 64 rows: 1 + 5 + 16 + 14 + 18 + 9 + 1. They are emitted in Legacy
ordinal order.

The table uses the Legacy Lua spelling so each row can be checked directly
against the receipt. The managed records preserve the same behavior while
using the desired-state contract's canonical spellings: lowercase XKB letter
keys, title-case arrow keysyms, canonical modifier order, and full direction
or monitor names such as `left` instead of `l`. The inert Legacy `mouse`
option on rows 105–106 is not persisted; the `mouse:272` and `mouse:273` key
tokens carry the behavior.

| Ordinal | Legacy chord | Legacy native action |
| ---: | --- | --- |
| 14 | `SUPER + SHIFT + E` | `hl.dsp.exit()` |
| 28 | `SUPER + Q` | `hl.dsp.window.kill()` |
| 29 | `SUPER + F` | `hl.dsp.window.fullscreen({ mode = "maximized", action = "toggle" })` |
| 30 | `SUPER + SHIFT + F` | `hl.dsp.window.fullscreen({ mode = "fullscreen", action = "toggle" })` |
| 31 | `SUPER + SHIFT + T` | `hl.dsp.window.float({ action = "toggle" })` |
| 32 | `SUPER + W` | `hl.dsp.group.toggle()` |
| 34 | `SUPER + left` | `hl.dsp.focus({ direction = "l" })` |
| 35 | `SUPER + down` | `hl.dsp.focus({ direction = "d" })` |
| 36 | `SUPER + up` | `hl.dsp.focus({ direction = "u" })` |
| 37 | `SUPER + right` | `hl.dsp.focus({ direction = "r" })` |
| 38 | `SUPER + H` | `hl.dsp.focus({ direction = "l" })` |
| 39 | `SUPER + J` | `hl.dsp.focus({ direction = "d" })` |
| 40 | `SUPER + K` | `hl.dsp.focus({ direction = "u" })` |
| 41 | `SUPER + L` | `hl.dsp.focus({ direction = "r" })` |
| 42 | `SUPER + SHIFT + left` | `hl.dsp.window.move({ direction = "l" })` |
| 43 | `SUPER + SHIFT + down` | `hl.dsp.window.move({ direction = "d" })` |
| 44 | `SUPER + SHIFT + up` | `hl.dsp.window.move({ direction = "u" })` |
| 45 | `SUPER + SHIFT + right` | `hl.dsp.window.move({ direction = "r" })` |
| 46 | `SUPER + SHIFT + H` | `hl.dsp.window.move({ direction = "l" })` |
| 47 | `SUPER + SHIFT + J` | `hl.dsp.window.move({ direction = "d" })` |
| 48 | `SUPER + SHIFT + K` | `hl.dsp.window.move({ direction = "u" })` |
| 49 | `SUPER + SHIFT + L` | `hl.dsp.window.move({ direction = "r" })` |
| 52 | `SUPER + CTRL + left` | `hl.dsp.focus({ monitor = "l" })` |
| 53 | `SUPER + CTRL + right` | `hl.dsp.focus({ monitor = "r" })` |
| 54 | `SUPER + CTRL + H` | `hl.dsp.focus({ monitor = "l" })` |
| 55 | `SUPER + CTRL + J` | `hl.dsp.focus({ monitor = "d" })` |
| 56 | `SUPER + CTRL + K` | `hl.dsp.focus({ monitor = "u" })` |
| 57 | `SUPER + CTRL + L` | `hl.dsp.focus({ monitor = "r" })` |
| 58 | `SUPER + SHIFT + CTRL + left` | `hl.dsp.window.move({ monitor = "l" })` |
| 59 | `SUPER + SHIFT + CTRL + down` | `hl.dsp.window.move({ monitor = "d" })` |
| 60 | `SUPER + SHIFT + CTRL + up` | `hl.dsp.window.move({ monitor = "u" })` |
| 61 | `SUPER + SHIFT + CTRL + right` | `hl.dsp.window.move({ monitor = "r" })` |
| 62 | `SUPER + SHIFT + CTRL + H` | `hl.dsp.window.move({ monitor = "l" })` |
| 63 | `SUPER + SHIFT + CTRL + J` | `hl.dsp.window.move({ monitor = "d" })` |
| 64 | `SUPER + SHIFT + CTRL + K` | `hl.dsp.window.move({ monitor = "u" })` |
| 65 | `SUPER + SHIFT + CTRL + L` | `hl.dsp.window.move({ monitor = "r" })` |
| 83 | `SUPER + 1` | `hl.dsp.focus({ workspace = "1" })` |
| 84 | `SUPER + 2` | `hl.dsp.focus({ workspace = "2" })` |
| 85 | `SUPER + 3` | `hl.dsp.focus({ workspace = "3" })` |
| 86 | `SUPER + 4` | `hl.dsp.focus({ workspace = "4" })` |
| 87 | `SUPER + 5` | `hl.dsp.focus({ workspace = "5" })` |
| 88 | `SUPER + 6` | `hl.dsp.focus({ workspace = "6" })` |
| 89 | `SUPER + 7` | `hl.dsp.focus({ workspace = "7" })` |
| 90 | `SUPER + 8` | `hl.dsp.focus({ workspace = "8" })` |
| 91 | `SUPER + 9` | `hl.dsp.focus({ workspace = "9" })` |
| 92 | `SUPER + SHIFT + 1` | `hl.dsp.window.move({ workspace = "1" })` |
| 93 | `SUPER + SHIFT + 2` | `hl.dsp.window.move({ workspace = "2" })` |
| 94 | `SUPER + SHIFT + 3` | `hl.dsp.window.move({ workspace = "3" })` |
| 95 | `SUPER + SHIFT + 4` | `hl.dsp.window.move({ workspace = "4" })` |
| 96 | `SUPER + SHIFT + 5` | `hl.dsp.window.move({ workspace = "5" })` |
| 97 | `SUPER + SHIFT + 6` | `hl.dsp.window.move({ workspace = "6" })` |
| 98 | `SUPER + SHIFT + 7` | `hl.dsp.window.move({ workspace = "7" })` |
| 99 | `SUPER + SHIFT + 8` | `hl.dsp.window.move({ workspace = "8" })` |
| 100 | `SUPER + SHIFT + 9` | `hl.dsp.window.move({ workspace = "9" })` |
| 104 | `SUPER + CTRL + F` | `hl.dsp.window.fullscreen({ mode = "maximized", action = "set" })` |
| 105 | `SUPER + mouse:272` | `hl.dsp.window.drag()` |
| 106 | `SUPER + mouse:273` | `hl.dsp.window.resize()` |
| 107 | `SUPER + code:20` | `hl.dsp.window.resize({ x = -100, y = 0, relative = true })` |
| 108 | `SUPER + code:21` | `hl.dsp.window.resize({ x = 100, y = 0, relative = true })` |
| 109 | `SUPER + minus` | `hl.dsp.window.resize({ x = -100, y = 0, relative = true })` |
| 110 | `SUPER + equal` | `hl.dsp.window.resize({ x = 100, y = 0, relative = true })` |
| 111 | `SUPER + SHIFT + minus` | `hl.dsp.window.resize({ x = 0, y = -100, relative = true })` |
| 112 | `SUPER + SHIFT + equal` | `hl.dsp.window.resize({ x = 0, y = 100, relative = true })` |
| 117 | `SUPER + SHIFT + P` | `hl.dsp.dpms({ action = "toggle" })` |

## The 21 blocked native rows

The Legacy receipt contains 85 non-`exec_cmd` rows. The 64 above are exact
portable dispatcher ports. The remaining 21 are intentionally not seeded; a
nearby action with different behavior is not an acceptable default.

| Legacy ordinals | Count | Why they are blocked |
| --- | ---: | --- |
| 50–51 | 2 | `focus({ window = "first" })` and `focus({ window = "last" })` are not variants in the pinned `focusArguments` schema. The records cannot pass desired-state validation. |
| 66–73 | 8 | These focus or move with workspace selectors `e+1` and `e-1`. The pinned `workspaceSpec` deliberately excludes those selectors. Mapping them to `next` or `previous` would change behavior: `e±1` cycles existing workspaces and wraps, while `next`/`previous` do not preserve that contract. |
| 75–82 | 8 | These are the Page Up/Down, letter, and wheel variants of the same `e+1`/`e-1` behavior and are blocked for the same reason. |
| 101–103 | 3 | `layout("preselect l")`, `layout("preselect r")`, and `layout("togglesplit")` use active-layout or plugin-specific message mini-languages. The action catalog explicitly excludes `layout` as non-portable managed v1 state. |

## Classification of the 32 process-command rows

All 32 Legacy `exec_cmd` rows are excluded from the shipped baseline. The
action catalog does not admit arbitrary commands into managed desired state.
Seventeen have a direct catalog-level semantic destination, but typed
`defaultApp` and `hyprshelld` bindings are not renderable until the fixed
action broker exists; the current renderer fails them closed with
`renderer.broker-unavailable`. Volume and brightness destinations preserve the
intent but cannot yet encode Legacy's exact 3% and 5% steps.

| Legacy ordinals | Count | Typed destination after broker implementation |
| --- | ---: | --- |
| 1 | 1 | `defaultApp.terminal` |
| 2 | 1 | `hyprshelld.launcher` |
| 6 | 1 | `hyprshelld.settings` |
| 11 | 1 | `hyprshelld.sessionMenu` |
| 12 | 1 | `hyprshelld.shortcutGuide` |
| 13 | 1 | `hyprshelld.lock` |
| 16–17 | 2 | `hyprshelld.media.volumeUp` / `hyprshelld.media.volumeDown` |
| 18–19 | 2 | `hyprshelld.media.toggleMute` / `hyprshelld.media.toggleMicMute` |
| 20–21 | 2 | `hyprshelld.media.playPause` |
| 22–23 | 2 | `hyprshelld.media.previous` / `hyprshelld.media.next` |
| 26–27 | 2 | `hyprshelld.display.brightnessUp` / `hyprshelld.display.brightnessDown` |
| 113 | 1 | `hyprshelld.screenshot.region` |

Six more rows have only behavior-changing approximations and therefore must
not be seeded:

| Legacy ordinals | Count | Why the nearby catalog action is not equivalent |
| --- | ---: | --- |
| 3 | 1 | `hyprshelld.launcher` loses the distinct Legacy Magnifier Bar mode. |
| 5, 15 | 2 | `defaultApp.systemMonitor` launches an external application; it is not the HGS Process List overlay. |
| 8 | 1 | `defaultApp.textEditor` launches an external application; it is not the HGS Notepad overlay. |
| 114 | 1 | `hyprshelld.screenshot.full` captures all displays; Legacy captured the focused output. |
| 115 | 1 | `hyprshelld.screenshot.window` selects a window interactively; Legacy captured the focused window. |

The remaining nine rows have no catalog counterpart:

| Legacy ordinals | Count | Legacy-only behavior |
| --- | ---: | --- |
| 4 | 1 | Clipboard overlay |
| 7 | 1 | Notification overlay |
| 9 | 1 | Wallpaper browser |
| 10 | 1 | Overview overlay |
| 24–25 | 2 | Per-player MPRIS volume adjustment, which is not equivalent to system output volume |
| 33 | 1 | Window-rules overlay |
| 74 | 1 | Interactive workspace-rename overlay |
| 116 | 1 | Output-profile cycling |

These must not be approximated as a text editor, system monitor, ordinary
volume action, or another superficially similar operation. They can be added
later only after a typed action with the same contract exists. Users who need
the old command immediately may author it explicitly in `user-custom.lua`.

## Sparse desired-state semantics

The desired-state `bindings` array is the persistent **user layer**, not a
materialized copy of the 64 defaults. An empty array means “use every shipped
default.” The Settings projection merges the compiled baseline with that
sparse array.

| UI state | Persisted user-layer record | Effective result |
| --- | --- | --- |
| Default | None | The compiled default is shown and rendered. |
| Override | One record with the default's stable `hyprshelld.default.*` ID | The matching baseline record is skipped and the replacement is rendered once. |
| Disabled default | The same stable ID with `enabled: false` | The baseline is skipped and no replacement bind is rendered. |
| Custom | A non-default stable ID | The custom record is rendered after the remaining baseline. |
| Reset | Delete the override or disabled record | Absence restores the current compiled default. |

For compatibility with snapshots created before layering, a global record that
matches a shipped default's normalized chord is treated as its override even
when it has an older ID. New Settings saves use the shipped stable ID. IDs are
the durable identity: if a future default chord changes, an existing override
or disable remains attached to the intended default.

The renderer first determines which default IDs are replaced, omits those
records from the baseline, and then renders enabled user records. It does not
emit `hl.unbind`. Hyprland's unbind operation is not submap-scoped and could
remove the same display chord from unrelated maps, so omission is the safe and
deterministic negative-override representation.

## Generated Lua and the manual escape hatch

There is deliberately no hand-owned `keybinds-user.lua`. Desired state is the
source of truth, and each active configuration is an immutable, receipt-checked
generation. Both logical layers are rendered into
`modules/70-keybinds.lua` in this order:

1. Managed `binds` scalar configuration, when present.
2. Shipped defaults that are not replaced or disabled.
3. Persisted user overrides, disabled-default omissions, custom bindings, and
   submap definitions.

The entrypoint requires every managed module in order and requires the
user-owned `user-custom.lua` last. HyprShelld preserves that file and does not
store or hash its contents as desired state. It remains the escape hatch for
raw process commands, layout messages, experimental Lua, and other deliberately
unsupported behavior. Changes made there do not appear as editable Settings
records and do not gain managed validation or reset behavior.

## Renderer-v1 pinning assumption

The compiled 64-row set affects active renderer-v1 output but is not copied
into desired-state JSON and is not independently named in the snapshot digest.
This implementation therefore relies on one explicit compatibility assumption:
the current branch is completing renderer v1 before that exact output contract
is treated as externally shipped. Under that assumption, the baseline may be
introduced without migrating existing sparse snapshots.

Once released, the ordered 64 records—their stable IDs, chords, actions,
payloads, and options—are pinned renderer behavior. A later material change
requires a renderer-version bump or a new receipt/digest migration that binds
the defaults explicitly. Changing the compiled set while continuing to call it
renderer v1 would allow equal desired state and catalogs to produce different
Lua bytes and would prevent exact re-render verification of earlier v1
generations.

Dormant renderer-v2 qualification output intentionally does not inherit this
active baseline. That preserves its previously frozen bytes while the active
Settings implementation is completed.
