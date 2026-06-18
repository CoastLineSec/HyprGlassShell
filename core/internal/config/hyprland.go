package config

import _ "embed"

//go:embed embedded/hyprland/hyprland.lua
var HyprlandLuaConfig string

//go:embed embedded/hyprland/hgs/animations.lua
var HGSAnimationsLuaConfig string

//go:embed embedded/hyprland/hgs/colors.lua
var HGSColorsLuaConfig string

//go:embed embedded/hyprland/hgs/cursor.lua
var HGSCursorLuaConfig string

//go:embed embedded/hyprland/hgs/custom.lua
var HGSCustomLuaConfig string

//go:embed embedded/hyprland/hgs/environment.lua
var HGSEnvironmentLuaConfig string

//go:embed embedded/hyprland/hgs/general.lua
var HGSGeneralLuaConfig string

//go:embed embedded/hyprland/hgs/input.lua
var HGSInputLuaConfig string

//go:embed embedded/hyprland/hgs/keybinds.lua
var HGSKeybindsLuaConfig string

//go:embed embedded/hyprland/hgs/layout.lua
var HGSLayoutLuaConfig string

//go:embed embedded/hyprland/hgs/misc.lua
var HGSMiscLuaConfig string

//go:embed embedded/hyprland/hgs/monitors.lua
var HGSMonitorsLuaConfig string

//go:embed embedded/hyprland/hgs/permissions.lua
var HGSPermissionsLuaConfig string

//go:embed embedded/hyprland/hgs/rules.lua
var HGSRulesLuaConfig string

//go:embed embedded/hyprland/hgs/user-keybinds.lua
var HGSUserKeybindsLuaConfig string
