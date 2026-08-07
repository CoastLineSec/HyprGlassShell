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
and open popout must pass through to windows underneath it. Press Escape or use
**Close** to dismiss the popout.

Stop the running fixture with:

```sh
qs kill -p tests/quickshell/qualification
```

The complete qualification also includes monitor reconnect, fractional-scale,
restart, frame-time, CPU, memory, and GPU measurements. Running this fixture
does not write Hyprland configuration or enable automatic startup.
