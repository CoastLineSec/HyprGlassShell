# Quickshell Qualification Fixture

This fixture exercises the surface behavior HyprShelld needs from Quickshell
before product shell development begins. It is deliberately separate from the
future `hyprshelld-surfaced` implementation.

From an active Hyprland session, start it with:

```sh
qs --no-duplicate -p tests/quickshell/qualification
```

The fixture creates one floating bar per active output. Use **Open test** to
exercise animated geometry and keyboard focus. Pointer input outside the bar
and open popout, including transparent space inside the fixed top layer, must
pass through to windows underneath it. Press Escape or use **Close** to dismiss
the popout.

For an instrumented pointer and focus check, place a maximized `wev` window
under the fixture. Confirm that transparent space beside and below the visible
content delivers pointer events to `wev`, while the bar, popout, and controls do
not leak events. Open the popout while `wev` has keyboard focus, type without
first clicking the input, then press Escape and verify keyboard focus returns
to `wev`.

The fixture exposes a qualification-only IPC target for repeatable motion
measurements. This command opens and closes the popout on `DP-4` every 180 ms
for approximately 10 seconds:

```sh
qs ipc -p tests/quickshell/qualification \
    call qualification animate DP-4
```

Use this repeatable motion with Qt scene-graph timing or the QML profiler to
measure per-window frame intervals. The IPC timer is stopped by default and
does not request idle updates.

Stop the running fixture with:

```sh
qs kill -p tests/quickshell/qualification
```

The complete qualification also includes monitor reconnect, fractional-scale,
restart, frame-time, CPU, memory, and GPU measurements. Running this fixture
does not write Hyprland configuration or enable automatic startup.
