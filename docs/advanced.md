# Advanced compositor settings

The **Advanced** page contains a small reviewed set of compositor behaviors
that do not belong to the visual, input, window, workspace, display, or rule
pages. It appears after **Rules** in the Settings sidebar.

These controls change managed Hyprland configuration. They do not expose the
complete upstream option catalog or a free-form configuration editor.

Across the five authored scalar pages, Settings currently exposes 236 values:
40 in Appearance, 49 in Input, 110 in Windows & Layout, 21 in Workspaces, and
sixteen in Advanced. Exactly 229 cover the 282 reviewed safe-reload controls,
leaving 53 safe controls unauthored: ten common and 43 advanced. Three of those
advanced values have the deliberate preserved-exclusion disposition described
below, leaving 50 controls: ten common and 40 advanced.
After also accounting for 25 unauthored safe controls that already have an
accepted higher-level owner—four common and 21 advanced—25 remain unresolved:
six common and 19 advanced.
Advanced includes seven caution controls across six cards. Those cautions
count toward the 236 authored total but not the 229-of-282 safe-coverage
figure. Fourteen accepted caution or risk controls remain unauthored.

## Preserved compatibility values

HyprShelld deliberately provides no Settings control for these three reviewed
Hyprland values:

- `misc:animate_manual_resizes`, off by default;
- `misc:animate_mouse_windowdragging`, off by default; and
- `misc:layers_hog_keyboard_focus`, on by default.

In both pinned Hyprland 0.55.0 and 0.56.1 sources,
`animate_manual_resizes` is read only by Dwindle's manual-resize path. That
value is passed into recursive layout updates, but the leaf update does not
use it and a dragged target then causes every leaf to snap to its target. The
setting therefore does not change the result in either supported source.
`animate_mouse_windowdragging` and `layers_hog_keyboard_focus` are registered
but never read by either pinned runtime, so their values are inert there.
Settings omits all three instead of presenting switches that do not deliver
their described behavior.

The values remain valid writable managed compatibility settings. An ordinary
save from any authored Settings page replaces only that page's owned values,
so an existing explicit nondefault (`true`, `true`, and `false` respectively)
is preserved. Default values remain omitted from generated configuration;
explicit nondefaults are still written to the managed `90-advanced.lua`
module.

Whole-compositor Recovery follows its last verified working snapshot rather
than merging with the current draft. It restores whichever values that
last-good snapshot contains for these settings, including their defaults when
no explicit override is present, and can therefore replace current values.

## Session lock recovery

**Allow another session-lock client** is off by default. Ordinarily, Hyprland
denies a new lock request when it already considers the session locked. When
this setting is on, a new client can be admitted after the current lock has
reached a locked or denied state, or its client is missing. This can recover
after a lock client crashes, but the admission does not itself prove a crash
and Hyprland does not launch or restart a client. A client still between its
request and locked or denied state is not replaced.

Hyprland 0.56.1 has one startup exception: the initial client launched through
Hyprland's start-locked command can be admitted while this setting is off, so
that client can take over the compositor's startup lock.

**Incomplete lock screen grace period** controls when Hyprland stops rendering
ordinary workspaces after a lock request if the lock client has not completed.
The value is measured in milliseconds, defaults to `1000`, and accepts values
from `0` through `5000`. Rendering stops sooner if the client reaches locked or
denied state, or is missing. A lock surface may exist but still be incomplete.
With **X-ray lock composition** enabled below, Hyprland keeps ordinary
workspaces in the lock composition and this cutoff does not suppress them.

## Session lock rendering

**X-ray lock composition** is off by default. Turning it on keeps ordinary
workspaces rendering beneath the lock screen and removes Hyprland's opaque
black primer. It does not unlock the session, bypass the lock client, or change
input routing. A lock surface can still cover the composition completely;
workspace content is visible only where the lock client's surfaces are not
opaque. This setting exists in both pinned Hyprland 0.55.0 and 0.56.1.

**Blur behind lock surfaces** is off by default and is editable only while
X-ray is on. Its saved value is retained when X-ray is turned off, so turning
X-ray back on restores the prior blur choice. Blur is visible only through
non-opaque lock-surface pixels. Hyprland 0.55.0 has no session-lock blur
setting; it was introduced in 0.56.0 and is present in the pinned 0.56.1
runtime.

## Background rendering and capture

**Hide compositor fallback image** is off by default. Turning it on suppresses
Hyprland's whole compositor-owned random fallback background image, so the
compositor background color is used instead. Splash text remains separately
controlled. This does not hide, replace, or manage a user-configured wallpaper.

**Hide Hyprland splash text** is also off by default and suppresses only the
splash text. The two controls are independent: hiding the fallback image does
not hide the splash, which can still render over Hyprland's compositor
background color until its own switch is enabled. Hiding the splash neither
suppresses the fallback image nor changes a user wallpaper. Both settings
exist in the pinned Hyprland 0.55.0 and 0.56.1 runtimes.

**Render-unfocused frame callbacks** controls how often Hyprland sends frame
callbacks to windows marked by the `renderunfocused` Window Rule when those
windows otherwise are not being rendered. It does not add the Rule to any
window or cap ordinary rendered frames. The value defaults to 15 FPS and
accepts values from 1 through 120 FPS.

**Prefer 8-bit screen capture** is on by default. When a new screen-share
session calculates its format constraints, supported 10-bit monitor formats
are replaced with `XRGB8888`. Other monitor formats are unchanged. This is the
screen-share session's preferred read-format path; it does not imply that
every compositor capture path is converted to 8 bit.

## Workspace underlay rendering

**Skip workspace underlays** is off by default. When it is on and Hyprland is
rendering an active workspace, the compositor does not construct its fallback
background, the layer-shell background and bottom passes, or the
post-wallpaper pass. Workspace windows and layer-shell top and overlay surfaces
continue to render. Rendering without an active workspace is unchanged.

This is a caution setting. It does not stop the skipped layer surfaces or their
processes, and it does not clear prior buffer pixels. Areas left uncovered by
the remaining windows and upper layers may therefore contain stale or
undefined pixels. The same active-workspace boundary applies in both pinned
Hyprland 0.55.0 and 0.56.1 runtimes.

## SDR work-buffer transfer

**SDR work-buffer transfer** defaults to **Display transfer (default)**. This
choice applies the display transfer to SDR content only while it passes through
Hyprland's internal FP16 or ICC work buffer. **Linear** instead keeps that SDR
work-buffer content linear.

This is a caution setting because it changes only that internal work-buffer
representation. It does not enable FP16, HDR, or ICC, and it does not change
the output transfer function or color profile. On ordinary sRGB output without
an ICC profile, the choice may remain dormant. HDR-like paths stay linear
regardless of this setting.

## Direct scanout rendering

**Direct scanout** defaults to **Disabled**, which keeps Hyprland's normal
composition path active. **Enabled** permits an otherwise-eligible solitary
DMA surface that exactly fills the output in fullscreen to be presented
directly. **Automatic (games only)** adds a requirement that the client
advertises game content. Neither enabled mode guarantees that direct scanout
will activate, and Settings does not test display or GPU support.

This is a caution setting because an eligible direct-scanout presentation
bypasses normal composition. Overlays such as the lock screen or notifications,
screen capture, mirrored outputs, a software cursor, and incompatible buffers,
transforms, or color management can keep Hyprland on its normal composition
path.

## Native Wayland resize compatibility

**Extend undersized surface textures** is on by default. When a native Wayland
client's submitted buffer is temporarily undersized for its current surface
mapping, Hyprland extends edge texture sampling across the mapped area while
the client catches up. Turning the setting off may expose unfilled or
stale-size edges until the client submits a matching buffer.

This is a caution setting, not a resize or responsiveness control. It does not
resize the client's buffer, window, or display, and it does not make the
application respond sooner. X11 application windows handled through XWayland
are unaffected. Some scale-unaware and misaligned-fullscreen correction paths
bypass the texture extension, so this choice does not apply to every native
Wayland resize path.

## XWayland compatibility

**Use nearest-neighbor filtering** is on by default. It affects only X11
application windows handled through XWayland. When Hyprland scales or
transforms one of those surfaces, nearest-neighbor filtering can keep pixel
art crisp but can make other content look pixelated. Turning the setting off
uses smoother filtering, which can look blurry. Native Wayland windows are
unchanged.

This is a caution setting because one global choice may not suit every X11
application. A per-window **Nearest-neighbor scaling** Window Rule can still
enable the filter for a matching window while the global setting is off. This
control does not create, edit, or remove that Rule. It changes surface
filtering only; it does not change display scale or XWayland coordinate
scaling.

## Input capture protocol

**Send modifiers to input capture** is off by default. When an authorized
capture is active, turning it on sends later keyboard modifier-state changes
to the capture client. Those forwarded changes stop before ordinary seat or
input-method delivery while the capture remains active. With the setting off,
Hyprland sends no modifier state to the capture client and leaves ordinary
modifier routing in place.

**Reject invalid capture barriers** is on by default. For each new request,
Hyprland rejects a barrier that is not a valid full edge of exactly one output
with a protocol error. Turning the setting off instead logs the invalid request
and adds the barrier. Existing barriers are not rechecked, repaired, removed,
or otherwise changed.

These are caution settings introduced in Hyprland 0.56.0 and present in the
pinned 0.56.1 runtime. They do not grant input-capture permission, create or
enable a capture session, or release an active one. They affect only later
modifier events and new barrier requests: changing modifier forwarding does
not synthesize or retract held modifier state, and changing barrier enforcement
does not retroactively validate existing barriers. The two controls are
independent.

## Display warnings

**Hide clean-divisor scale warning** is off by default. Turning it on hides the
warning shown when a display scale is not a clean divisor. It does not change
the configured scale, connected-output topology, or the Displays page's live
test and confirmation workflow.

## Save one Advanced draft

The sixteen values form one Advanced draft. Moving to another Settings page
keeps that draft, and the sidebar shows **Unsaved** until it is discarded or
saved.

Select **Save & apply** to persist one validated desired-state revision, reload
Hyprland, and verify that exact revision. **Discard draft** returns to the
values from the synchronized revision. **Reset to defaults** prepares these
sixteen defaults without saving immediately:

- admission of another session-lock client off;
- incomplete-lock grace period `1000` ms;
- session-lock X-ray off;
- session-lock blur off;
- compositor fallback image visible;
- Hyprland splash text visible;
- clean-divisor scale warning visible;
- render-unfocused frame callbacks at 15 FPS;
- 8-bit screen-capture preference on;
- workspace underlay skipping off;
- SDR work-buffer transfer set to Display transfer;
- direct scanout disabled;
- undersized native Wayland surface-texture extension on;
- nearest-neighbor filtering for XWayland on;
- input-capture modifier forwarding off; and
- invalid input-capture barrier rejection on.

If the compositor revision changes while the draft is open, Settings preserves
the draft and marks **Advanced** with **Review**. It does not silently combine
the draft with the newer revision. Select **Load current settings** to discard
the preserved draft and start from the current verified values.

Advanced requires the managed compositor takeover described in the
[Displays guide](displays.md). The page can take you to Displays to review that
workflow, but it never starts takeover by itself. A running display test also
locks Advanced changes until the test is kept or reverted.

If a saved revision cannot be activated, **Retry apply** targets that exact
saved compositor revision. **Restore last working configuration** opens a
separate confirmation because recovery replaces every pending compositor
setting with the last verified working snapshot; it is not limited to the
Advanced page.

The page has no illustrative preview. These settings affect session-lock
admission and composition, compositor fallback rendering, Rule-qualified frame
callbacks, new screen-share sessions, active-workspace underlay construction,
internal SDR work-buffer transfer, direct output presentation, temporarily
undersized native Wayland surface mappings, XWayland surface sampling, and
later input-capture modifier events and new barrier requests, and warning
presentation, so a simulated picture would not verify their real behavior.

Return to [Settings](settings.md) or the
[HyprShelld User Guide](index.md).
