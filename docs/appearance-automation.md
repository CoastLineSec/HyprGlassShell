# Appearance automation and Night Light

The **Appearance** page controls two related but independent systems:

- **Color mode** selects the light or dark HyprShelld palette used by Settings
  and shell surfaces.
- **Night Light** changes the display color temperature through the Hypr
  ecosystem's
  [hyprsunset](https://wiki.hypr.land/Hypr-Ecosystem/hyprsunset/) daemon.

They can use separate schedules, or the automatic color mode can follow the
Night Light schedule. Neither system changes Hyprland window decoration,
animation, blur, or opacity settings; those remain in the compositor-backed
Appearance draft described in the main [Appearance guide](appearance.md).

## Choose a color mode

Choose **Light** or **Dark** to keep an explicit palette. The saved automatic
source and its schedule remain available but dormant while an explicit mode is
selected.

Choose **Automatic** to expose one of three sources:

- **Desktop** follows Qt's current desktop color-scheme preference. This is the
  default automatic behavior and needs no schedule or location.
- **Custom schedule** switches independently at fixed local times or at the
  calculated sunrise and sunset for a location.
- **Night Light** follows the day/night decision from Night Light's automatic
  schedule. Turn on **Automatic schedule** in the Night Light card first. The
  display filter itself may remain off when only the shell palette should
  follow that schedule.

The effective-mode badge reports the palette in use. Scheduled sources also
show the next transition and, for a solar schedule, the calculated sunrise and
sunset. A waiting-location or no-transition status is shown instead of silently
guessing when a schedule cannot currently produce a transition.

### Fixed-time schedules

Enter distinct **Dark from** and **Light from** times using a 24-hour clock.
The intervals may cross midnight. The initial custom appearance schedule
changes to dark at 18:00 and light at 06:00; changing either value saves both
as one validated schedule.

The schedule uses the machine's local date, time zone, and daylight-saving
rules. The always-running HyprShelld configuration service owns the timer, so
Settings does not have to remain open for a transition to occur.

### Sunrise and sunset schedules

Select **Sunrise & sunset**, then choose a location source:

- **Current location** requests coordinates from the system
  [GeoClue](https://gitlab.freedesktop.org/geoclue/geoclue) service. GeoClue may
  require desktop permission and a working location provider. HyprShelld waits
  visibly if no usable result is available.
- **Manual coordinates** accepts latitude from -90 through 90 and longitude
  from -180 through 180. A coordinate pair at 0, 0 is valid; clearing the
  location is a separate action.

HyprShelld performs the solar calculation locally. It does not send an IP
address or coordinates to an IP-geolocation or weather service. Manually
entered coordinates are part of the locally persisted core settings; a
GeoClue result is consumed by the local scheduling service rather than copied
into the manual-coordinate fields.

At extreme latitudes, a date may have no sunrise or no sunset. HyprShelld
reports that condition and keeps the appropriate polar-day or polar-night
state instead of inventing a transition.

## Configure Night Light

Night Light requires `hyprsunset`. Turn it on to warm the display, then choose
the temperature behavior:

- With **Automatic schedule** off, the selected filter temperature is applied
  continuously.
- With **Automatic schedule** on and **By time** selected, separate night and
  day temperatures are applied at the saved dark and light times. The initial
  schedule is 20:00 to 06:00, with 4000 K at night and 6500 K during the day.
- With **Sunrise & sunset** selected, the same manual-coordinate and GeoClue
  choices described above determine the solar schedule. **Gradual sunset**
  blends between the saved night and day temperatures through twilight rather
  than changing once at the transition.

Lower Kelvin values are warmer. Settings validates the complete temperature
pair before saving it, including that the night value cannot be cooler than
the day value. The status panel reports the current applied temperature,
sunrise, sunset, and next scheduled transition when those values are
available.

The Night Light schedule remains saved while the filter is off. This allows
automatic shell colors to follow it without forcing display warming, and it
lets the previous filter behavior return when Night Light is enabled again.
The custom appearance schedule and Night Light schedule are separate records;
editing one does not rewrite the other.

## Runtime ownership and upstream boundary

HyprShelld calculates fixed-time and solar transitions itself, then asks
`hyprsunset` to apply the target temperature using the official
`hyprctl hyprsunset` IPC commands. It does not replace or generate
`~/.config/hypr/hyprsunset.conf`.

The current command contract targets hyprsunset 0.4.0 with Hyprland 0.56.2. It
starts the companion with `hyprsunset --temperature <kelvin>` and changes or
clears the filter with `hyprctl hyprsunset temperature <kelvin>` and
`hyprctl hyprsunset identity`. CMake verifies executable presence rather than
the installed command version, so an incompatible command interface can still
fail at runtime.

This division is intentional. The current upstream `hyprsunset` interface
supports temperatures and time-based profiles but does not provide native
sunrise/sunset scheduling. Upstream solar-schedule work is tracked in
[hyprsunset pull request #72](https://github.com/hyprwm/hyprsunset/pull/72).
Keeping solar math in HyprShelld also lets the shell palette and display filter
share a result without duplicating network lookups or requiring Settings to
stay open.

When Night Light is enabled and no `hyprsunset` process is already running,
HyprShelld starts one and owns that child process. Disabling Night Light first
requests the identity transform and then stops only the process HyprShelld
started. If another session already owns a `hyprsunset` process, Settings
reports **Managed by another hyprsunset session** and does not take it over or
mutate its temperature. Stop the other owner before asking HyprShelld to
manage Night Light.

The Arch `hyprsunset` package also installs a user service. Do not enable that
service while HyprShelld owns Night Light. If it is already enabled or running,
stop and disable it before turning on HyprShelld Night Light:

```sh
systemctl --user disable --now hyprsunset.service
```

Theme scheduling remains available if display-temperature application fails.
An unavailable or failed Night Light runtime is reported separately and does
not rewrite the saved color mode or schedule.

## Required software

Source configuration and the installed runtime require:

- [`hyprsunset`](https://github.com/hyprwm/hyprsunset), the display-temperature
  daemon, supplied by the Arch `hyprsunset` package;
- `hyprctl`, supplied by the Arch `hyprland` package, for `hyprsunset` IPC; and
- `pgrep`, supplied by the Arch `procps-ng` package, so HyprShelld can detect
  an existing daemon before starting its own.

The Arch `geoclue` package is an optional runtime integration. Fixed-time
schedules and manual coordinates work without it. If **Current location** is
selected without a usable GeoClue daemon or permission, the schedule remains
saved and reports that it is waiting for a location.

This source tree currently has no package-manager manifest. CMake checks that
the three required executables exist on the build host, but that is not a
substitute for installed package metadata. A distributor must declare the
three hard runtime packages above and may expose `geoclue` as an optional
dependency.

Return to the [HyprShelld User Guide](index.md).
