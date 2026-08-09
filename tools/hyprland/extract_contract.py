#!/usr/bin/env python3
"""Extract and verify HyprShelld's pinned Hyprland configuration contract.

This tool intentionally reads the tagged Hyprland C++ value registry rather than
`hyprctl descriptions`: the latter is runtime state, does not expose a stable
type discriminator in 0.56.1, and does not cover the non-scalar Lua surfaces.

The extractor is deliberately small and dependency-free.  It accepts only the
`makeConfigValue` forms used by the qualified tags and fails closed when a new
form appears.  The semantic UI/apply overlay below is reviewed code; rerunning
the extractor therefore makes upstream drift visible without silently deciding
how a new option should be presented.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


REGISTRY_PATH = Path("src/config/values/ConfigValues.cpp")
DISPATCHER_SOURCE_PATH = Path("src/config/lua/bindings/LuaBindingsDispatchers.cpp")
COMPLEX_SOURCE_PATHS = (
    Path("src/Compositor.cpp"),
    Path("src/animation/AnimationManager.cpp"),
    Path("src/animation/WorkspaceAnimationController.cpp"),
    Path("src/config/lua/ConfigManager.cpp"),
    Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
    DISPATCHER_SOURCE_PATH,
    Path("src/config/lua/bindings/LuaBindingsInternal.cpp"),
    Path("src/config/lua/bindings/LuaBindingsInternal.hpp"),
    Path("src/config/lua/bindings/LuaBindingsToplevel.cpp"),
    Path("src/config/shared/actions/ConfigActions.cpp"),
    Path("src/config/shared/animation/AnimationTree.cpp"),
    Path("src/config/shared/monitor/Parser.cpp"),
    Path("src/config/shared/Types.hpp"),
    Path("src/desktop/rule/Rule.cpp"),
    Path("src/desktop/rule/windowRule/WindowRule.cpp"),
    Path("src/desktop/rule/windowRule/WindowRule.hpp"),
    Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"),
    Path("src/desktop/state/ViewQuery.cpp"),
    Path("src/desktop/state/ViewQuery.hpp"),
    Path("src/desktop/types/OverridableVar.hpp"),
    Path("src/desktop/view/Window.cpp"),
    Path("src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"),
    Path("src/desktop/view/animationControllers/WindowAnimationController.cpp"),
    Path("src/devices/IPointer.hpp"),
    Path("src/helpers/CMType.cpp"),
    Path("src/helpers/MiscFunctions.cpp"),
    Path("src/helpers/MiscFunctions.hpp"),
    Path("src/helpers/TransferFunction.cpp"),
    Path("src/main.cpp"),
    Path("src/managers/fullscreen/FullscreenController.hpp"),
    Path("src/managers/KeybindManager.cpp"),
    Path("src/managers/input/InputManager.cpp"),
    Path("src/managers/input/trackpad/TrackpadGestures.cpp"),
    Path("src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"),
    Path("src/managers/input/trackpad/gestures/FloatGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/FloatGesture.hpp"),
    Path("src/managers/input/trackpad/gestures/FullscreenGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/FullscreenGesture.hpp"),
    Path("src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"),
    Path("src/protocols/types/ContentType.cpp"),
    Path("src/protocols/types/ContentType.hpp"),
    Path("src/render/decorations/CHyprGroupBarDecoration.cpp"),
    Path("src/state/MonitorQueryCore.cpp"),
    Path("src/state/MonitorQueryCore.hpp"),
    Path("src/state/WorkspaceQueryCore.cpp"),
    Path("src/state/WorkspaceQueryCore.hpp"),
)
STARTUP_SOURCE_PATHS_0560 = (
    Path("src/Compositor.cpp"),
    Path("src/config/lua/ConfigManager.cpp"),
    Path("src/config/shared/actions/ConfigActions.cpp"),
    Path("src/main.cpp"),
)
REPOSITORY = "https://github.com/hyprwm/Hyprland"
REVIEWED_ON = "2026-08-09"
WIKI_ROOT = "https://wiki.hypr.land/0.56.0"

QUALIFIED_SOURCES = {
    "0.55.0": {
        "tag": "v0.55.0",
        "commit": "af923e30d1d24f1f4a4f5cb8308065173c1d9539",
        "count": 341,
    },
    "0.56.1": {
        "tag": "v0.56.1",
        "commit": "5c9377c15f85c50648f35ca5a213754f95b93ca0",
        "count": 353,
    },
    "0.56.0": {
        "tag": "v0.56.0",
        "commit": "36b2e0cfe0c6094dbc47bd42a437431315bb3087",
    },
}

# A VERSION string and option count are not provenance: a caller can edit a
# source file while leaving both unchanged.  These immutable digests are the
# reviewed bytes at the exact tags/commits above.  The extractor refuses to
# generate or check artifacts until every source file used to qualify the
# contract matches its pin, so generated manifests can never stamp a trusted
# commit over caller-supplied content.
QUALIFIED_SOURCE_HASHES: dict[str, dict[Path, str]] = {
    "0.55.0": {
        Path("VERSION"): "ac6c7168f4989720dc4505c85ea89d7cfdbd57f14f903bb8689cd23c992ed19e",
        REGISTRY_PATH: "290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce",
    },
    "0.56.1": {
        Path("VERSION"): "2aaeb543208a766598b45262f3eabb0600c2a6055350ab8be22b5bf944a484e9",
        REGISTRY_PATH: "a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754",
        Path("src/Compositor.cpp"): "74833ecbf0e2b6f8ad84345ac0716a3295a0e347420a188be0ab4f6a684af7c0",
        Path("src/animation/AnimationManager.cpp"): "8c37cd0d1e972e8789468fdf063456a6a40ed17d482b898b0639a6d2a1fa7985",
        Path("src/animation/WorkspaceAnimationController.cpp"): "0698720a19698186197a0f1c98893b839502ad454d751d3e590f2eb0ae2b5e5b",
        Path("src/config/lua/ConfigManager.cpp"): "94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502",
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): "157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9",
        DISPATCHER_SOURCE_PATH: "76488e1f4893fcf835c13ed98e51ab4d1c72d76a12c753eb0ad3a2237bf95223",
        Path("src/config/lua/bindings/LuaBindingsInternal.cpp"): "5f6534641d58073bbcbd3e004b168659d497072596cf10c9f2b189c55c2233e9",
        Path("src/config/lua/bindings/LuaBindingsInternal.hpp"): "83630401a5b3d2cd99ec42705a7b70b0fdd73ddb3989c561a4316d15d72f0edd",
        Path("src/config/lua/bindings/LuaBindingsToplevel.cpp"): "706b29eb52de087c1d6e64770cf85fb3ebc0f2fbe9ddcff086905499bcd332f5",
        Path("src/config/shared/actions/ConfigActions.cpp"): "29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab",
        Path("src/config/shared/animation/AnimationTree.cpp"): "313ed20167618dfe163fceee8753e9a49d8ef356bc3f580963e07039839e7bae",
        Path("src/config/shared/monitor/Parser.cpp"): "616468fd0576d8b201ef00cd69289234f0ffb1c3137ae2a0ac954fabe37dd589",
        Path("src/config/shared/Types.hpp"): "337556a3fbedda3e34f31f600f4a44d4e85f53a04220f3c90d8394f98d1eb638",
        Path("src/desktop/rule/Rule.cpp"): "257687efd814cb714024c14e2952adb26e1e7a1f5907d20aa6aaea1e4c098e2e",
        Path("src/desktop/rule/windowRule/WindowRule.cpp"): "7034c3325fee476cb501266af67999534ca36cf2841d83c9940db96d116d1f78",
        Path("src/desktop/rule/windowRule/WindowRule.hpp"): "12cf28cc2aab0126a52ed61b7dcabd77ec6aebb643846883cd4d439e1603853a",
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"): "fec33f9a0bc279ffc94ca27a6e9843964840d6269bc28d330d566e05fed3307b",
        Path("src/desktop/state/ViewQuery.cpp"): "10acb6240350d3fcb07544ab34279bd050984f02b1087d2d4a1febe4d2cbf1cc",
        Path("src/desktop/state/ViewQuery.hpp"): "e7037e695a48e3b856f2c42285225db03f62aa41b4f694539db4b65be5736a7e",
        Path("src/desktop/types/OverridableVar.hpp"): "4d111c95b15133ca8f6f8a579fe36f9652a7298d031c45d84d83576670c06501",
        Path("src/desktop/view/Window.cpp"): "4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f",
        Path("src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"): "22b223f086b454c54f475e7f16b2d499d92cfd071f2083ea58a9f6a4412f0b1d",
        Path("src/desktop/view/animationControllers/WindowAnimationController.cpp"): "9a8c3c2961a1a55fca7b8783e1c3b808b424291e832bf4cdf88ed7a73e5bab08",
        Path("src/devices/IPointer.hpp"): "bf4b2309526ca44ac30d2c3d3ad6a66cc7bf8d7a398b22a8fdde58c0f0ab1310",
        Path("src/helpers/CMType.cpp"): "b3df474366f4c2e4dae85beae34eef900c5390faaf2683db2e14c919abdd8e00",
        Path("src/helpers/MiscFunctions.cpp"): "065418241a3b40e21273bca1fd29036221942554cddb61e08422d86f0a13c1d9",
        Path("src/helpers/MiscFunctions.hpp"): "084844f04b5be8ca1e9d7c6e49f94a4b6fc9cd3e387a060d6f70dcc0b68c7c79",
        Path("src/helpers/TransferFunction.cpp"): "503eafd06a295b1ecbf0506d552db337f79498180f183c0bbc7a12f8855baeaf",
        Path("src/main.cpp"): "98e5752cd485378c58c2a8cb0ba89265eaea27144925ea059e5564917dd3b645",
        Path("src/managers/fullscreen/FullscreenController.hpp"): "7f3585f23e4d756f3f165670a604de39717eb3fd704114e2a230bdd6ffba2378",
        Path("src/managers/KeybindManager.cpp"): "8d8f35fc84c4a2f8de63ac2bf6f6531c651c6fa6d37b1aac7c8fc5d9342d4e40",
        Path("src/managers/input/InputManager.cpp"): "07de27ef0f4c9a5c3bf14f42c07af29574d428a7b02412f785f90db30b03125e",
        Path("src/managers/input/trackpad/TrackpadGestures.cpp"): "5e23524d8a6a0778fc8199173978488e4b54e70611ec354b036a99ae6b9610b7",
        Path("src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"): "5967e293ccdc7e99b6f6201a1d2e0aeff95b1d763ce5ad3d7fb50fabd76c6753",
        Path("src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"): "9861e8cf3c6d26632731f164da5a4a016553d4f8ea6f71d68855cc7a682a6962",
        Path("src/managers/input/trackpad/gestures/FloatGesture.cpp"): "d10abdf34e7e966777c91e225ed63038da72511e5eb73f81ee3acfd93859934e",
        Path("src/managers/input/trackpad/gestures/FloatGesture.hpp"): "dfc7559d79b9109ed36f202483d4bcd64d1f08b4e2daef182fd44d5433b0e307",
        Path("src/managers/input/trackpad/gestures/FullscreenGesture.cpp"): "b72fb2976e43899884afd9f8d9f6c6b3c6da04c9797482ef6df614e1bdc483e5",
        Path("src/managers/input/trackpad/gestures/FullscreenGesture.hpp"): "576dd48493e13dc26c56b7fb028084debe2080ac02f7ef644b38675f1217889e",
        Path("src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"): "10e6bdece28c998bfbf574d6f455ba53f7286b61a243d13122cb2086ab81bbae",
        Path("src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"): "d1d2be5027b63050fa5ef7f71a964c95ae9bdceae47d08765298977e6cf63c22",
        Path("src/protocols/types/ContentType.cpp"): "54fad7c983134f5da812204e14a4e57280891b2d543b1eb3df225d6309cec1f6",
        Path("src/protocols/types/ContentType.hpp"): "760baaa2d18559bc377df5fdc5a6bc8be2bd4dd5eb70ece5f7295c0c33216db3",
        Path("src/render/decorations/CHyprGroupBarDecoration.cpp"): "39cb87fc2b28c81433bfd34d3900e58c2c58f4f336be728e07461a8f16c095e6",
        Path("src/state/MonitorQueryCore.cpp"): "19e76f03b679d151afa0d55a03c42071ef30578f889df1d15167126cc7c350bc",
        Path("src/state/MonitorQueryCore.hpp"): "829581d4aeb084cdd30de9c8c8e310ead38d357061df4e82a8279d634870e0f4",
        Path("src/state/WorkspaceQueryCore.cpp"): "b810515fb0720d1fe6b3e3e1e5d5ebfa57e5503e83840337a614b0862860b3d7",
        Path("src/state/WorkspaceQueryCore.hpp"): "698a1814b9d47ef08fe8ed54e723a839d73b9191335f3ba879f2e8389b3025a2",
    },
    "0.56.0": {
        Path("VERSION"): "3fea81d177087f5d3380893d95b86573a803b34ed45419ec381bdd776f526cee",
        Path("src/Compositor.cpp"): "0c08837447683a23a62aefbfcc8332881e7f8e49d3a87f41aba856df29cca9fb",
        Path("src/config/lua/ConfigManager.cpp"): "bf295818d6ad5a1f01aa708a6843a968b9cbb14228482421bbe0e4e5b26600ed",
        Path("src/config/shared/actions/ConfigActions.cpp"): "29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab",
        Path("src/main.cpp"): "98e5752cd485378c58c2a8cb0ba89265eaea27144925ea059e5564917dd3b645",
    },
}

CANONICAL_MODIFIERS = (
    "shift",
    "caps",
    "ctrl",
    "alt",
    "mod2",
    "mod3",
    "super",
    "mod5",
)

TAGGED_MODIFIER_ALIASES = {
    "SHIFT": ("SHIFT",),
    "CAPS": ("CAPS",),
    "CTRL": ("CTRL", "CONTROL"),
    "ALT": ("ALT", "MOD1"),
    "MOD2": ("MOD2",),
    "MOD3": ("MOD3",),
    "META": ("SUPER", "WIN", "LOGO", "MOD4", "META"),
    "MOD5": ("MOD5",),
}

TAGGED_FULLSCREEN_MODES = {
    "FSMODE_NONE": 0,
    "FSMODE_MAXIMIZED": 1,
    "FSMODE_FULLSCREEN": 2,
}

MONITOR_MODE_KEYWORDS = ("preferred", "highrr", "highres", "maxwidth")
MONITOR_AUTO_POSITIONS = (
    "auto", "auto-right", "auto-left", "auto-up", "auto-down",
    "auto-center-right", "auto-center-left", "auto-center-up",
    "auto-center-down",
)
MONITOR_MODE_PATTERN = (
    r"^[1-9][0-9]{0,4}x[1-9][0-9]{0,4}"
    r"(?:@(?:[1-9][0-9]{0,3}(?:\.[0-9]{1,3})?|0\.[0-9]{0,2}[1-9]))?$"
)
MONITOR_POSITION_PATTERN = (
    r"^(?:0|[+-]?(?:[1-9][0-9]{0,5}|1000000))"
    r"x(?:0|[+-]?(?:[1-9][0-9]{0,5}|1000000))$"
)

FLOAT_MAX = 3.4028234663852886e38
BINDING_MOUSE_PATTERN = (
    r"^mouse:(?:27[2-9]|2[89][0-9]|[3-6][0-9]{2}|7[0-5][0-9]|76[0-7])$"
)
BINDING_CODE_PATTERN = (
    r"^code:(?:0|[1-9][0-9]{0,8}|[1-3][0-9]{9}|4[01][0-9]{8}|"
    r"42[0-8][0-9]{7}|429[0-3][0-9]{6}|4294[0-8][0-9]{5}|"
    r"42949[0-5][0-9]{4}|429496[0-6][0-9]{3}|4294967[01][0-9]{2}|"
    r"42949672[0-8][0-9]|429496729[0-4])$"
)
KEYSYM_TOKEN_PATTERN = (
    r"^(?!(?:catchall|mouse_down|mouse_up|mouse_left|mouse_right)$)"
    r"[A-Za-z0-9_]+$"
)
ACTION_MODIFIER_TOKENS = (
    "SHIFT", "CAPS", "CTRL", "ALT", "MOD2", "MOD3", "SUPER", "MOD5",
)
ACTION_MODIFIERS_PATTERN = (
    r"^(?! )(?!.* $)(?!.*  )(?:SHIFT(?: |$))?(?:CAPS(?: |$))?"
    r"(?:CTRL(?: |$))?(?:ALT(?: |$))?(?:MOD2(?: |$))?(?:MOD3(?: |$))?"
    r"(?:SUPER(?: |$))?(?:MOD5)?$"
)
ACTION_CODE_PATTERN = (
    r"^code:(?:0|[1-9][0-9]{0,8}|1[0-9]{9}|20[0-9]{8}|"
    r"21[0-3][0-9]{7}|214[0-6][0-9]{6}|2147[0-3][0-9]{5}|"
    r"21474[0-7][0-9]{4}|214748[0-2][0-9]{3}|2147483[0-5][0-9]{2}|"
    r"21474836[0-3][0-9]|214748364[0-7])$"
)
WINDOW_SELECTOR_REGEX_PATTERN = (
    r"^(?:class|initialclass|title|initialtitle|tag):"
    r"[^\u0000-\u001f\u007f]{1,512}$"
)
WINDOW_SELECTOR_PID_PATTERN = (
    r"^pid:(?:[1-9][0-9]{0,8}|1[0-9]{9}|20[0-9]{8}|"
    r"21[0-3][0-9]{7}|214[0-6][0-9]{6}|2147[0-3][0-9]{5}|"
    r"21474[0-7][0-9]{4}|214748[0-2][0-9]{3}|2147483[0-5][0-9]{2}|"
    r"21474836[0-3][0-9]|214748364[0-7])$"
)
WORKSPACE_SELECTOR_ID_PATTERN = (
    r"^(?:[1-9][0-9]{0,8}|1[0-9]{9}|20[0-9]{8}|"
    r"21[0-3][0-9]{7}|214[0-6][0-9]{6}|2147[0-3][0-9]{5}|"
    r"21474[0-7][0-9]{4}|214748[0-2][0-9]{3}|2147483[0-5][0-9]{2}|"
    r"21474836[0-3][0-9]|214748364[0-7])$"
)
WORKSPACE_SELECTOR_NAME_PATTERN = r"^name:[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$"
WORKSPACE_SELECTOR_SPECIAL_PATTERN = (
    r"^special(?::[A-Za-z0-9_][A-Za-z0-9_.-]{0,127})?$"
)
WORKSPACE_SPEC_KEYWORDS = (
    "previous", "previous_per_monitor", "next", "empty",
)
MONITOR_SPEC_ID_PATTERN = (
    r"^(?:0|[1-9][0-9]{0,8}|1[0-9]{9}|20[0-9]{8}|"
    r"21[0-3][0-9]{7}|214[0-6][0-9]{6}|2147[0-3][0-9]{5}|"
    r"21474[0-7][0-9]{4}|214748[0-2][0-9]{3}|2147483[0-5][0-9]{2}|"
    r"21474836[0-3][0-9]|214748364[0-7])$"
)
MONITOR_SPEC_RELATIVE_PATTERN = (
    r"^[+-](?:[1-9][0-9]{0,8}|1[0-9]{9}|20[0-9]{8}|"
    r"21[0-3][0-9]{7}|214[0-6][0-9]{6}|2147[0-3][0-9]{5}|"
    r"21474[0-7][0-9]{4}|214748[0-2][0-9]{3}|2147483[0-5][0-9]{2}|"
    r"21474836[0-3][0-9]|214748364[0-7])$"
)
MONITOR_SPEC_NAME_PATTERN = (
    r"^(?!(?:current|left|right|up|down)$)(?![0-9]+$)"
    r"[A-Za-z][A-Za-z0-9_.-]{0,127}$"
)
MONITOR_SPEC_DESCRIPTION_PATTERN = r"^desc:[^\u0000-\u001f\u007f]{1,256}$"
UINT64_DECIMAL_PATTERN = (
    r"^(?:0|[1-9][0-9]{0,18}|(?:1[0-7][0-9]{18}|18[0-3][0-9]{17}|"
    r"184[0-3][0-9]{16}|1844[0-5][0-9]{15}|18446[0-6][0-9]{14}|"
    r"184467[0-3][0-9]{13}|1844674[0-3][0-9]{12}|184467440[0-6][0-9]{10}|"
    r"1844674407[0-2][0-9]{9}|18446744073[0-6][0-9]{8}|"
    r"1844674407370[0-8][0-9]{6}|18446744073709[0-4][0-9]{5}|"
    r"184467440737095[0-4][0-9]{4}|18446744073709550[0-9]{3}|"
    r"18446744073709551[0-5][0-9]{2}|1844674407370955160[0-9]|"
    r"1844674407370955161[0-4]|18446744073709551615))$"
)
HYPRLAND_TARGET_PATTERN = (
    r"^0\.56\.(?:0|[1-9][0-9]{0,8}|[1-3][0-9]{9}|4[01][0-9]{8}|"
    r"42[0-8][0-9]{7}|429[0-3][0-9]{6}|4294[0-8][0-9]{5}|"
    r"42949[0-5][0-9]{4}|429496[0-6][0-9]{3}|4294967[01][0-9]{2}|"
    r"42949672[0-8][0-9]|429496729[0-5])$"
)

TAGGED_GESTURE_MODES = {
    "float": ("float", "tile", "toggle"),
    "fullscreen": ("fullscreen", "maximize"),
    "cursorZoom": ("toggle", "mult", "live"),
}

INHERITED_DEFAULTS = {
    "decoration:shadow:color_inactive": "decoration:shadow:color",
    "decoration:glow:color_inactive": "decoration:glow:color",
    "group:groupbar:text_color_inactive": "group:groupbar:text_color",
    "group:groupbar:text_color_locked_active": "group:groupbar:text_color",
    "group:groupbar:text_color_locked_inactive": "group:groupbar:text_color_inactive",
}

ANIMATION_LEAVES = (
    "global", "windows", "layers", "fade", "border", "borderangle",
    "shadowangle", "glowangle", "workspaces", "zoomFactor", "monitorAdded",
    "layersIn", "layersOut", "windowsIn", "windowsOut", "windowsMove",
    "fadeIn", "fadeOut", "fadeSwitch", "fadeShadow", "fadeGlow", "fadeDim",
    "fadeLayers", "fadeLayersIn", "fadeLayersOut", "fadePopups",
    "fadePopupsIn", "fadePopupsOut", "fadeDpms", "workspacesIn",
    "workspacesOut", "specialWorkspace", "specialWorkspaceIn",
    "specialWorkspaceOut",
)

WINDOW_ANIMATION_LEAVES = tuple(
    leaf for leaf in ANIMATION_LEAVES if leaf.startswith("window")
)
WORKSPACE_ANIMATION_LEAVES = tuple(
    leaf for leaf in ANIMATION_LEAVES
    if leaf.startswith(("workspaces", "specialWorkspace"))
)
ANGLE_ANIMATION_LEAVES = tuple(
    leaf for leaf in ANIMATION_LEAVES if leaf.endswith("angle")
)
LAYER_ANIMATION_LEAVES = tuple(
    leaf for leaf in ANIMATION_LEAVES if leaf.startswith("layers")
)
NO_STYLE_ANIMATION_LEAVES = tuple(
    leaf for leaf in ANIMATION_LEAVES
    if leaf not in {
        *WINDOW_ANIMATION_LEAVES,
        *WORKSPACE_ANIMATION_LEAVES,
        *ANGLE_ANIMATION_LEAVES,
        *LAYER_ANIMATION_LEAVES,
    }
)

WINDOW_ANIMATION_STYLE_PATTERN = (
    r"^(?:|slide(?: (?:top|bottom|left|right))?|gnome|gnomed|"
    r"popin(?: (?:0|[1-9][0-9]?|100)%)?)$"
)
WORKSPACE_ANIMATION_STYLE_PATTERN = (
    r"^(?:|fade|(?:slide|slidevert|slidefade|slidefadevert)"
    r"(?: (?:top|bottom|left|right))?(?: (?:0|[1-9][0-9]?|100)%)?)$"
)
LAYER_ANIMATION_STYLE_PATTERN = (
    r"^(?:|fade|slide(?: (?:top|bottom|left|right))?|"
    r"popin(?: (?:0|[1-9][0-9]?|100)%)?)$"
)

TYPE_NAMES = {
    "Bool": "boolean",
    "Int": "integer",
    "Float": "number",
    "String": "string",
    "Color": "color",
    "Gradient": "gradient",
    "Vec2": "vector2",
    "CssGap": "cssGap",
    "FontWeight": "fontWeight",
}

TYPE_CONTROLS = {
    "boolean": "toggle",
    "integer": "spinBox",
    "number": "slider",
    "string": "text",
    "color": "color",
    "gradient": "gradient",
    "vector2": "vector2",
    "cssGap": "text",
    "fontWeight": "spinBox",
    "enum": "select",
}

# String-valued enumerations cannot be discovered from ConfigValues.cpp's type
# alone.  These values were reviewed against the tagged 0.56.1 implementation.
STRING_ENUMS: dict[str, list[tuple[str, str]]] = {
    "general:layout": [
        ("dwindle", "dwindle"),
        ("master", "master"),
        ("scrolling", "scrolling"),
        ("monocle", "monocle"),
    ],
    "input:accel_profile": [
        ("automatic", ""),
        ("adaptive", "adaptive"),
        ("flat", "flat"),
    ],
    "input:scroll_method": [
        ("automatic", ""),
        ("two-finger", "2fg"),
        ("edge", "edge"),
        ("button", "on_button_down"),
        ("disabled", "no_scroll"),
    ],
    "input:touchpad:tap_button_map": [
        ("automatic", ""),
        ("left-right-middle", "lrm"),
        ("left-middle-right", "lmr"),
    ],
    "render:cm_sdr_eotf": [
        ("default", "default"),
        ("automatic", "auto"),
        ("sRGB", "srgb"),
        ("gamma 2.2", "gamma22"),
        ("forced gamma 2.2", "gamma22force"),
    ],
    "master:new_status": [
        ("master", "master"),
        ("slave", "slave"),
        ("inherit", "inherit"),
    ],
    "master:new_on_active": [
        ("none", "none"),
        ("before", "before"),
        ("after", "after"),
    ],
    "master:orientation": [
        ("left", "left"),
        ("right", "right"),
        ("top", "top"),
        ("bottom", "bottom"),
        ("center", "center"),
    ],
    "master:center_master_fallback": [
        ("left", "left"),
        ("right", "right"),
        ("top", "top"),
        ("bottom", "bottom"),
    ],
    "scrolling:direction": [
        ("left", "left"),
        ("right", "right"),
        ("up", "up"),
        ("down", "down"),
    ],
}

# Integer enums documented by the versioned 0.56 wiki but missing an OptionMap
# in ConfigValues.cpp. Keeping these in the reviewed overlay prevents Settings
# from presenting opaque numeric controls or reviving legacy labels.
INTEGER_ENUMS: dict[str, list[tuple[str, int]]] = {
    "input:float_switch_override_focus": [
        ("disabled", 0),
        ("tiled/floating transitions", 1),
        ("all floating transitions", 2),
    ],
    "input:touchpad:drag_lock": [
        ("disabled", 0),
        ("enabled with timeout", 1),
        ("sticky", 2),
    ],
    "misc:force_default_wallpaper": [
        ("random", -1),
        ("wallpaper 0", 0),
        ("wallpaper 1", 1),
        ("wallpaper 2", 2),
    ],
    "misc:initial_workspace_tracking": [
        ("disabled", 0),
        ("single-shot", 1),
        ("persistent for children", 2),
    ],
    "binds:workspace_center_on": [
        ("workspace center", 0),
        ("last active window", 1),
    ],
    "binds:focus_preferred_method": [
        ("history", 0),
        ("shared edge length", 1),
    ],
}

# Path-specific managed lexical constraints that are stricter than an arbitrary
# registry string. Cursor monitor selection has no relative monitor context, so
# only empty or stable static output selectors are writable.
STRING_PATTERNS = {
    "cursor:default_monitor": (
        r"^(?:|(?!(?:current|left|right|up|down)$)(?![0-9]+$)"
        r"[A-Za-z][A-Za-z0-9_.-]{0,127}|desc:[^\u0000-\u001f\u007f]{1,256})$"
    ),
}

# The tagged registry leaves these two numeric domains open even though their
# consumers narrow them.  Managed desired state uses deliberate safe bounds:
# logical movement thresholds share the compositor coordinate envelope, while
# tablet eraser overrides use the Linux input-event code domain (0 is the
# tagged default sentinel).
CATALOG_CONSTRAINT_OVERRIDES: dict[str, dict[str, int | float]] = {
    "input:follow_mouse_threshold": {"min": 0, "max": 1000000},
    "input:tablettool:eraser_button_override": {"min": 0, "max": 767},
}

RESTART_OPTIONS = {
    "xwayland:enabled",
    "xwayland:create_abstract_socket",
    "render:cm_enabled",
    "render:commit_timing_enabled",
    "debug:full_cm_proto",
    "ecosystem:enforce_permissions",
    "experimental:xx_color_management_v4",
}

DANGEROUS_OPTIONS = {
    "debug:manual_crash",
}

CAUTION_PREFIXES = (
    "debug:",
    "experimental:",
    "input-capture:",
    "opengl:",
    "quirks:",
    "render:",
    "xwayland:",
)

CAUTION_OPTIONS = {
    "general:allow_tearing",
    "decoration:screen_shader",
    "cursor:invisible",
    "cursor:no_hardware_cursors",
    "misc:disable_autoreload",
    "ecosystem:enforce_permissions",
    "decoration:motion_blur:enabled",
    "decoration:motion_blur:samples",
}

EXPERT_PREFIXES = (
    "debug:",
    "experimental:",
    "opengl:",
    "quirks:",
)

EXPERT_OPTIONS = {
    "ecosystem:enforce_permissions",
}

ADVANCED_PREFIXES = (
    "binds:",
    "cursor:",
    "ecosystem:",
    "group:",
    "input-capture:",
    "layout:",
    "misc:",
    "render:",
    "xwayland:",
)

ADVANCED_PATH_PARTS = (
    ":tablet:",
    ":tablettool:",
    ":touchdevice:",
    ":numlock_by_default",
    ":follow_mouse_threshold",
    ":scroll_points",
    ":screen_shader",
)

ADVANCED_OPTIONS = {
    "decoration:motion_blur:enabled",
    "decoration:motion_blur:samples",
}

EXTERNAL_OPTIONS = {
    "general:locale",
    "general:col.inactive_border",
    "general:col.active_border",
    "general:col.nogroup_border",
    "general:col.nogroup_border_active",
    "group:col.border_active",
    "group:col.border_inactive",
    "group:col.border_locked_inactive",
    "group:col.border_locked_active",
    "misc:col.splash",
}

DOCS_BY_NAMESPACE = {
    "animations": f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Animations/",
    "binds": f"{WIKI_ROOT}/Configuring/Basics/Binds/",
    "gestures": f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Gestures/",
    "input": f"{WIKI_ROOT}/Configuring/Basics/Variables/",
    "xwayland": f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/XWayland/",
}
VARIABLES_DOC = f"{WIKI_ROOT}/Configuring/Basics/Variables/"

# Closed public dispatcher inventory from registerDispatcherBindings() in the
# tagged Lua source. exec_cmd and exec_raw are intentionally absent; their raw
# command strings are not representable in managed desired state.
DISPATCHER_ARGUMENT_SCHEMAS: dict[str, str] = {
    **{action: "emptyArguments" for action in (
        "exit", "force_renderer_reload", "release_input_capture",
        "window.toggle_swallow", "window.bring_to_top", "window.drag", "no_op",
    )},
    **{action: "emptyArguments" for action in (
        "group.toggle", "group.next", "group.prev", "window.close", "window.kill",
        "window.center", "window.clear_tags",
    )},
    **{action: "toggleArguments" for action in (
        "group.lock", "group.lock_active", "window.deny_from_group",
    )},
    **{action: "toggleArguments" for action in (
        "window.float", "window.pseudo", "window.pin",
    )},
    "cursor.move_to_corner": "cursorCornerArguments",
    "cursor.move": "cursorMoveArguments",
    "group.move_window": "groupMoveWindowArguments",
    "group.active": "groupActiveArguments",
    "submap": "submapArguments",
    "pass": "passArguments",
    "dpms": "dpmsArguments",
    "event": "eventArguments",
    "global": "globalArguments",
    "force_idle": "forceIdleArguments",
    "send_shortcut": "sendShortcutArguments",
    "send_key_state": "sendKeyStateArguments",
    "window.signal": "windowSignalArguments",
    "window.fullscreen": "windowFullscreenArguments",
    "window.fullscreen_state": "windowFullscreenStateArguments",
    "window.move": "windowMoveArguments",
    "window.swap": "windowSwapArguments",
    "window.cycle_next": "windowCycleArguments",
    "window.tag": "windowTagArguments",
    "window.alter_zorder": "windowAlterZOrderArguments",
    "window.resize": "windowResizeArguments",
    "focus": "focusArguments",
    "workspace.toggle_special": "workspaceToggleSpecialArguments",
    "workspace.rename": "workspaceRenameArguments",
    "workspace.change_id": "workspaceChangeIdArguments",
    "workspace.move": "workspaceMoveArguments",
    "workspace.swap_monitors": "workspaceSwapMonitorsArguments",
}

# Lua call-shape metadata is deliberately explicit.  Slice 2 must never infer
# whether a validated JSON argument object becomes no Lua argument, a table, or
# one extracted scalar.  These records are checked against the pinned wrapper
# source below and are shipped in the action catalog.
DISPATCHER_NONE_INVOCATIONS = (
    "exit", "force_renderer_reload", "release_input_capture", "no_op",
    "group.toggle", "group.next", "group.prev",
    "window.close", "window.kill", "window.center", "window.clear_tags",
    "window.toggle_swallow", "window.bring_to_top", "window.drag",
)
DISPATCHER_TABLE_INVOCATIONS = (
    "cursor.move", "cursor.move_to_corner", "dpms", "focus", "group.active",
    "group.lock", "group.lock_active", "group.move_window", "pass",
    "send_key_state", "send_shortcut", "window.alter_zorder",
    "window.cycle_next", "window.deny_from_group",
    "window.float", "window.fullscreen", "window.fullscreen_state",
    "window.move", "window.pin", "window.pseudo", "window.signal",
    "window.swap", "window.tag",
    "workspace.change_id", "workspace.move", "workspace.rename",
    "workspace.swap_monitors",
)
DISPATCHER_SCALAR_INVOCATIONS = {
    "event": "event",
    "force_idle": "seconds",
    "global": "name",
    "submap": "name",
    "workspace.toggle_special": "name",
}
DISPATCHER_INVOCATIONS: dict[str, dict[str, str]] = {
    **{action: {"kind": "none"} for action in DISPATCHER_NONE_INVOCATIONS},
    **{action: {"kind": "table"} for action in DISPATCHER_TABLE_INVOCATIONS},
    **{
        action: {"kind": "scalar", "field": field}
        for action, field in DISPATCHER_SCALAR_INVOCATIONS.items()
    },
    "window.resize": {"kind": "empty-object-none-otherwise-table"},
}

DEFAULT_APP_ACTIONS = (
    "defaultApp.terminal", "defaultApp.browser", "defaultApp.fileManager",
    "defaultApp.textEditor", "defaultApp.codeEditor", "defaultApp.mail",
    "defaultApp.calendar", "defaultApp.music", "defaultApp.video",
    "defaultApp.imageViewer", "defaultApp.pdfViewer", "defaultApp.systemMonitor",
)

HYPRSHELLD_ACTIONS = (
    "hyprshelld.launcher", "hyprshelld.settings", "hyprshelld.lock",
    "hyprshelld.sessionMenu", "hyprshelld.shortcutGuide",
    "hyprshelld.screenshot.full", "hyprshelld.screenshot.region",
    "hyprshelld.screenshot.window", "hyprshelld.media.volumeUp",
    "hyprshelld.media.volumeDown", "hyprshelld.media.toggleMute",
    "hyprshelld.media.toggleMicMute", "hyprshelld.media.playPause",
    "hyprshelld.media.next", "hyprshelld.media.previous",
    "hyprshelld.display.brightnessUp", "hyprshelld.display.brightnessDown",
)

GESTURE_ACTIONS = (
    ("workspace", "workspace"), ("resize", "resize"), ("move", "move"),
    ("special", "special"), ("close", "close"), ("float", "float"),
    ("fullscreen", "fullscreen"), ("cursorZoom", "cursorZoom"),
    ("scrollMove", "scroll_move"), ("unset", "unset"),
)

GESTURE_INVOCATION_PARAMETERS: dict[str, tuple[tuple[str, str], ...]] = {
    "workspace": (),
    "resize": (),
    "move": (),
    "special": (("workspace", "workspace_name"),),
    "close": (),
    "float": (("mode", "mode"),),
    "fullscreen": (("mode", "mode"),),
    "cursorZoom": (("zoomLevel", "zoom_level"), ("mode", "mode")),
    "scrollMove": (),
    "unset": (),
}

EXPERT_DISPATCHERS = {
    "event", "global", "force_renderer_reload", "release_input_capture",
    "window.signal",
}
DANGEROUS_DISPATCHERS = {"exit", "window.kill", "window.signal"}
CAUTION_DISPATCHERS = EXPERT_DISPATCHERS | {
    "dpms", "pass", "send_shortcut", "send_key_state",
}

# Reviewed presentation copy. These descriptions explain outcomes rather than
# exposing Lua implementation names to the searchable shortcut picker.
DISPATCHER_DESCRIPTIONS = {
    "cursor.move_to_corner": "Move the pointer to a selected corner of the focused window.",
    "cursor.move": "Move the pointer to exact compositor coordinates.",
    "group.toggle": "Toggle the active window's membership in a window group.",
    "group.next": "Activate the next window in the current group.",
    "group.prev": "Activate the previous window in the current group.",
    "group.active": "Activate a window at a specific index in the current group.",
    "group.move_window": "Reorder the active window forward or backward within its group.",
    "group.lock": "Toggle, enable, or disable global window-group locking.",
    "group.lock_active": "Toggle, enable, or disable locking for the active window group.",
    "window.close": "Ask the focused window to close.",
    "window.kill": "Forcefully terminate the focused window.",
    "window.signal": "Send a numeric process signal to the focused window.",
    "window.float": "Toggle, enable, or disable floating for the focused window.",
    "window.fullscreen": "Change fullscreen or maximized state for the focused window.",
    "window.fullscreen_state": "Set the internal and client fullscreen states explicitly.",
    "window.pseudo": "Toggle, enable, or disable pseudotiling for the focused window.",
    "window.move": "Move a window by direction, coordinates, workspace, monitor, or group relation.",
    "window.swap": "Swap the active window with a directional, selected, next, or previous window.",
    "window.center": "Center the focused window in its monitor work area.",
    "window.cycle_next": "Cycle window focus with optional tiled or floating filters.",
    "window.tag": "Apply a declarative tag to the focused window.",
    "window.clear_tags": "Remove all tags from the focused window.",
    "window.toggle_swallow": "Toggle swallowing state for the active window.",
    "window.pin": "Toggle, enable, or disable pinning for the focused window.",
    "window.bring_to_top": "Raise the active window to the top of the compositor stack.",
    "window.alter_zorder": "Raise or lower the focused window in the compositor stack.",
    "window.deny_from_group": "Toggle whether the active window may join a group.",
    "window.drag": "Begin an interactive move operation for the active window.",
    "window.resize": "Begin interactive resizing or resize the focused window explicitly.",
    "workspace.rename": "Rename a selected workspace.",
    "workspace.change_id": "Assign a new positive numeric ID to a selected workspace.",
    "workspace.move": "Move the current or selected workspace to a monitor.",
    "workspace.swap_monitors": "Swap the active workspaces between two monitors.",
    "workspace.toggle_special": "Show or hide a named special workspace.",
    "exit": "Exit the running Hyprland session.",
    "submap": "Switch to a named keybinding submap.",
    "pass": "Pass the current key event to a selected window.",
    "send_shortcut": "Send a modifier and key shortcut to a selected window.",
    "send_key_state": "Send an explicit key press, release, or repeat state to a selected window.",
    "dpms": "Toggle, enable, or disable display power management.",
    "event": "Emit a custom Hyprland IPC event.",
    "global": "Invoke a registered global shortcut.",
    "force_renderer_reload": "Force Hyprland to reload renderer resources.",
    "force_idle": "Advance idle timers by a specified number of seconds.",
    "release_input_capture": "Release the active input-capture session.",
    "focus": "Focus a window, workspace, monitor, direction, urgent target, or prior target.",
    "no_op": "Perform no compositor action.",
}

DEFAULT_APP_DESCRIPTIONS = {
    "defaultApp.terminal": "Launch the configured default terminal, with xdg-terminal-exec as the fallback.",
    "defaultApp.browser": "Launch the configured default web browser.",
    "defaultApp.fileManager": "Launch the configured default file manager.",
    "defaultApp.textEditor": "Launch the configured default text editor.",
    "defaultApp.codeEditor": "Launch the configured default code editor.",
    "defaultApp.mail": "Launch the configured default mail application.",
    "defaultApp.calendar": "Launch the configured default calendar application.",
    "defaultApp.music": "Launch the configured default music application.",
    "defaultApp.video": "Launch the configured default video application.",
    "defaultApp.imageViewer": "Launch the configured default image viewer.",
    "defaultApp.pdfViewer": "Launch the configured default PDF viewer.",
    "defaultApp.systemMonitor": "Launch the configured default system monitor.",
}

HYPRSHELLD_DESCRIPTIONS = {
    "hyprshelld.launcher": "Open the HyprShelld application launcher.",
    "hyprshelld.settings": "Open HyprShelld Settings.",
    "hyprshelld.lock": "Lock the current session through the HyprShelld service.",
    "hyprshelld.sessionMenu": "Open the HyprShelld session menu.",
    "hyprshelld.shortcutGuide": "Open the searchable HyprShelld shortcut guide.",
    "hyprshelld.screenshot.full": "Capture all displays through the HyprShelld screenshot service.",
    "hyprshelld.screenshot.region": "Interactively capture a selected screen region.",
    "hyprshelld.screenshot.window": "Interactively capture a selected window.",
    "hyprshelld.media.volumeUp": "Increase the default audio output volume.",
    "hyprshelld.media.volumeDown": "Decrease the default audio output volume.",
    "hyprshelld.media.toggleMute": "Toggle mute for the default audio output.",
    "hyprshelld.media.toggleMicMute": "Toggle mute for the default microphone.",
    "hyprshelld.media.playPause": "Toggle playback for the active media player.",
    "hyprshelld.media.next": "Skip to the next item in the active media player.",
    "hyprshelld.media.previous": "Return to the previous item in the active media player.",
    "hyprshelld.display.brightnessUp": "Increase display brightness.",
    "hyprshelld.display.brightnessDown": "Decrease display brightness.",
}

GESTURE_DESCRIPTIONS = {
    "workspace": "Navigate workspaces along the gesture direction.",
    "resize": "Resize the active window continuously with the gesture.",
    "move": "Move the active window continuously with the gesture.",
    "special": "Show or hide a named special workspace with the gesture.",
    "close": "Close the active window when the gesture completes.",
    "float": "Change floating state for the active window with the gesture.",
    "fullscreen": "Change fullscreen state for the active window with the gesture.",
    "cursorZoom": "Control compositor cursor zoom with the gesture.",
    "scrollMove": "Move the active scrolling-layout window with the gesture.",
    "unset": "Remove the gesture matching these fingers, direction, and modifiers.",
}


@dataclass(frozen=True)
class RawOption:
    source_type: str
    path: str
    description: str
    default_expression: str
    options_expression: str


def _split_top_level(text: str) -> list[str]:
    """Split a C++ argument/initializer list at top-level commas."""
    result: list[str] = []
    stack: list[str] = []
    quote: str | None = None
    escaped = False
    start = 0
    matching = {")": "(", "}": "{", "]": "["}

    for index, char in enumerate(text):
        if quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            continue

        if char in ('"', "'"):
            quote = char
        elif char in "({[":
            stack.append(char)
        elif char in ")}]":
            if not stack or stack.pop() != matching[char]:
                raise ValueError("unbalanced C++ initializer")
        elif char == "," and not stack:
            result.append(text[start:index].strip())
            start = index + 1

    if quote is not None or stack:
        raise ValueError("unterminated C++ initializer")
    result.append(text[start:].strip())
    return result


def _macro_invocations(source: str) -> Iterable[tuple[str, list[str]]]:
    cursor = 0
    while True:
        start = source.find("MS<", cursor)
        if start < 0:
            return

        type_end = source.find(">", start + 3)
        if type_end < 0:
            raise ValueError("unterminated MS type")
        source_type = source[start + 3 : type_end].strip()
        open_paren = source.find("(", type_end + 1)
        if open_paren < 0:
            raise ValueError("MS invocation has no argument list")

        depth = 0
        quote: str | None = None
        escaped = False
        close_paren = -1
        for index in range(open_paren, len(source)):
            char = source[index]
            if quote is not None:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == quote:
                    quote = None
                continue
            if char in ('"', "'"):
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    close_paren = index
                    break
        if close_paren < 0:
            raise ValueError("unterminated MS invocation")

        yield source_type, _split_top_level(source[open_paren + 1 : close_paren])
        cursor = close_paren + 1


def _cpp_string(expression: str) -> str:
    # C++ concatenates adjacent string literals.  Python's parser does too, but
    # indentation in a multi-line C++ argument is not necessarily legal Python,
    # so decode each literal and prove that only whitespace separated them.
    literal_pattern = re.compile(r'"(?:\\.|[^"\\])*"', re.DOTALL)
    pieces: list[str] = []
    cursor = 0
    for match in literal_pattern.finditer(expression):
        if expression[cursor : match.start()].strip():
            raise ValueError(f"unsupported C++ string expression: {expression}")
        try:
            value = ast.literal_eval(match.group(0))
        except (SyntaxError, ValueError) as error:
            raise ValueError(f"unsupported C++ string expression: {expression}") from error
        if not isinstance(value, str):
            raise ValueError(f"expected a C++ string literal: {expression}")
        pieces.append(value)
        cursor = match.end()
    if expression[cursor:].strip() or not pieces:
        raise ValueError(f"unsupported C++ string expression: {expression}")
    return "".join(pieces)


def extract_raw_options(source: str) -> list[RawOption]:
    result: list[RawOption] = []
    for source_type, arguments in _macro_invocations(source):
        if source_type not in TYPE_NAMES:
            raise ValueError(f"unreviewed Hyprland value type: {source_type}")
        if len(arguments) not in (2, 3, 4):
            raise ValueError(
                f"unsupported MS<{source_type}> arity {len(arguments)}: {arguments!r}"
            )
        if len(arguments) == 2 and source_type != "FontWeight":
            raise ValueError(f"only FontWeight may use an implicit default: {arguments!r}")

        default_expression = arguments[2] if len(arguments) >= 3 else "400"
        options_expression = arguments[3] if len(arguments) == 4 else "{}"
        result.append(
            RawOption(
                source_type=source_type,
                path=_cpp_string(arguments[0]),
                description=_cpp_string(arguments[1]),
                default_expression=default_expression,
                options_expression=options_expression,
            )
        )

    paths = [option.path for option in result]
    if len(paths) != len(set(paths)):
        duplicates = sorted(path for path in set(paths) if paths.count(path) > 1)
        raise ValueError(f"duplicate option paths: {duplicates}")
    return result


def extract_dispatcher_ids(source: str) -> set[str]:
    """Extract the closed hl.dsp registration tree from tagged Lua source."""
    start = source.find("void Internal::registerDispatcherBindings(lua_State* L)")
    if start < 0:
        raise ValueError("dispatcher registration function not found")
    block = source[start:]
    end = block.find('\n}')
    if end < 0:
        raise ValueError("dispatcher registration function is unterminated")
    block = block[:end]

    cursor = 0
    result: set[str] = set()
    for namespace in ("cursor", "group", "window", "workspace"):
        marker = f'lua_setfield(L, -2, "{namespace}");'
        boundary = block.find(marker, cursor)
        if boundary < 0:
            raise ValueError(f"dispatcher namespace {namespace} not found")
        segment = block[cursor:boundary]
        names = re.findall(r'Internal::setFn\(L, "([a-z0-9_]+)",', segment)
        result.update(f"{namespace}.{name}" for name in names)
        cursor = boundary + len(marker)

    end_marker = 'lua_setfield(L, -2, "dsp");'
    boundary = block.find(end_marker, cursor)
    if boundary < 0:
        raise ValueError("dispatcher root table terminator not found")
    root_names = re.findall(
        r'Internal::setFn\(L, "([a-z0-9_]+)",', block[cursor:boundary]
    )
    result.update(root_names)
    return result


def extract_modifier_aliases(source: str) -> dict[str, tuple[str, ...]]:
    """Extract the exact public modifier branches from tagged Lua source."""
    function = re.search(
        r"static std::optional<eKeyboardModifiers> modFromSv\([^)]*\) \{"
        r"(?P<body>.*?)\n\}",
        source,
        re.DOTALL,
    )
    if not function:
        raise ValueError("Lua modifier parser function not found")

    result: dict[str, tuple[str, ...]] = {}
    branches = re.finditer(
        r"if \((?P<condition>.*?)\)\s*"
        r"return HL_MODIFIER_(?P<modifier>[A-Z0-9_]+);",
        function.group("body"),
        re.DOTALL,
    )
    for branch in branches:
        modifier = branch.group("modifier")
        aliases = tuple(
            re.findall(r'sv == "([A-Z0-9]+)"', branch.group("condition"))
        )
        if not aliases:
            raise ValueError(f"Lua modifier {modifier} has no string aliases")
        if modifier in result:
            raise ValueError(f"duplicate Lua modifier branch: {modifier}")
        result[modifier] = aliases
    return result


def extract_fullscreen_modes(source: str) -> dict[str, int]:
    """Extract the numeric fullscreen-mode enum from its tagged header."""
    declaration = re.search(
        r"enum eFullscreenMode\s*:\s*int8_t\s*\{(?P<body>.*?)\};",
        source,
        re.DOTALL,
    )
    if not declaration:
        raise ValueError("fullscreen mode enum not found")

    result: dict[str, int] = {}
    previous = -1
    for raw_entry in declaration.group("body").split(","):
        entry = re.sub(r"//.*", "", raw_entry).strip()
        if not entry:
            continue
        match = re.fullmatch(
            r"(?P<name>FSMODE_[A-Z0-9_]+)(?:\s*=\s*(?P<value>[0-9]+))?",
            entry,
        )
        if not match:
            raise ValueError(f"unsupported fullscreen mode enum entry: {entry!r}")
        value = int(match.group("value")) if match.group("value") else previous + 1
        result[match.group("name")] = value
        previous = value
    return result


def _assert_gesture_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    def constructor_body(path: str, qualified_name: str) -> str:
        source = complex_sources[Path(path)].decode("utf-8")
        match = re.search(
            re.escape(qualified_name) + r"\([^)]*\) \{(?P<body>.*?)\n\}",
            source,
            re.DOTALL,
        )
        if not match:
            raise ValueError(f"tagged gesture constructor not found: {qualified_name}")
        return match.group("body")

    float_body = constructor_body(
        "src/managers/input/trackpad/gestures/FloatGesture.cpp",
        "CFloatTrackpadGesture::CFloatTrackpadGesture",
    )
    float_modes = tuple(re.findall(r'lc\.starts_with\("([A-Za-z]+)"\)', float_body))
    if (*float_modes, "toggle") != TAGGED_GESTURE_MODES["float"]:
        raise ValueError(f"tagged float gesture modes changed: {float_modes!r}")

    fullscreen_body = constructor_body(
        "src/managers/input/trackpad/gestures/FullscreenGesture.cpp",
        "CFullscreenTrackpadGesture::CFullscreenTrackpadGesture",
    )
    fullscreen_modes = tuple(
        re.findall(r'lc\.starts_with\("([A-Za-z]+)"\)', fullscreen_body)
    )
    if fullscreen_modes != TAGGED_GESTURE_MODES["fullscreen"]:
        raise ValueError(
            f"tagged fullscreen gesture modes changed: {fullscreen_modes!r}"
        )

    cursor_path = Path(
        "src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"
    )
    cursor_source = complex_sources[cursor_path].decode("utf-8")
    cursor_body = constructor_body(
        cursor_path.as_posix(),
        "CCursorZoomTrackpadGesture::CCursorZoomTrackpadGesture",
    )
    cursor_modes = (
        "toggle",
        *re.findall(r'second == "([A-Za-z]+)"', cursor_body),
    )
    if cursor_modes != TAGGED_GESTURE_MODES["cursorZoom"]:
        raise ValueError(f"tagged cursor zoom gesture modes changed: {cursor_modes!r}")
    if cursor_source.count("1.0F, 100.0F") < 3:
        raise ValueError("tagged cursor zoom result clamp changed")

    special_source = complex_sources[Path(
        "src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"
    )].decode("utf-8")
    if special_source.count('"special:" + m_specialWorkspaceName') != 2:
        raise ValueError("tagged special-workspace gesture naming changed")

    variants = config_schema.get("$defs", {}).get("gestureAction", {}).get("oneOf")
    if not isinstance(variants, list):
        raise ValueError("config schema gesture action variants are missing")
    by_type: dict[str, dict[str, Any]] = {}
    for variant in variants:
        if not isinstance(variant, dict):
            continue
        properties = variant.get("properties")
        if not isinstance(properties, dict):
            continue
        discriminator = properties.get("type")
        if isinstance(discriminator, dict) and isinstance(
            discriminator.get("const"), str
        ):
            by_type[discriminator["const"]] = variant

    for action_type, modes in TAGGED_GESTURE_MODES.items():
        variant = by_type.get(action_type)
        if not variant:
            raise ValueError(f"config schema has no {action_type} gesture variant")
        actual_modes = (
            variant.get("properties", {}).get("mode", {}).get("enum")
        )
        if actual_modes != list(modes):
            raise ValueError(
                f"config schema {action_type} modes do not match tagged source: "
                f"expected {list(modes)!r}, found {actual_modes!r}"
            )
    cursor_zoom = by_type["cursorZoom"].get("properties", {}).get("zoomLevel")
    if cursor_zoom != {"$ref": "#/$defs/cursorZoomLevel"}:
        raise ValueError("cursorZoom.zoomLevel must use the bounded numeric contract")


def _assert_inherited_default_contract(
    options: list[RawOption],
    complex_sources: dict[Path, bytes],
) -> None:
    by_path = {option.path: option for option in options}
    for path, inherited_from in INHERITED_DEFAULTS.items():
        option = by_path.get(path)
        source = by_path.get(inherited_from)
        if option is None or source is None:
            raise ValueError(
                f"inherited default path is absent: {path} -> {inherited_from}"
            )
        if option.default_expression.strip() != "-1":
            raise ValueError(f"inherited default sentinel changed for {path}")
        if option.source_type != source.source_type:
            raise ValueError(
                f"inherited default type mismatch: {path} ({option.source_type}) "
                f"-> {inherited_from} ({source.source_type})"
            )

    for start in INHERITED_DEFAULTS:
        seen: set[str] = set()
        cursor = start
        while cursor in INHERITED_DEFAULTS:
            if cursor in seen:
                raise ValueError(f"inherited default cycle starts at {start}")
            seen.add(cursor)
            cursor = INHERITED_DEFAULTS[cursor]

    window_source = complex_sources[Path("src/desktop/view/Window.cpp")].decode(
        "utf-8"
    )
    for path in (
        "decoration:shadow:color_inactive",
        "decoration:glow:color_inactive",
    ):
        if f'getConfigValue("{path}")' not in window_source:
            raise ValueError(f"inactive gradient fallback consumer changed for {path}")
    if window_source.count("COLORINACTIVE.setByUser ?") != 2:
        raise ValueError("inactive shadow/glow setByUser fallback semantics changed")

    groupbar_source = complex_sources[Path(
        "src/render/decorations/CHyprGroupBarDecoration.cpp"
    )].decode("utf-8")
    expected_fallbacks = (
        "*PTEXTCOLORINACTIVE == -1 ? COLORACTIVE",
        "*PTEXTCOLORLOCKEDACTIVE == -1 ? COLORACTIVE",
        "*PTEXTCOLORLOCKEDINACTIVE == -1 ? COLORINACTIVE",
    )
    for fallback in expected_fallbacks:
        if fallback not in groupbar_source:
            raise ValueError(f"groupbar inherited color fallback changed: {fallback}")


def _assert_animation_style_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    source = complex_sources[Path("src/animation/AnimationManager.cpp")].decode(
        "utf-8"
    )
    required_source_fragments = (
        'config.starts_with("window")',
        'style.starts_with("slide") || style == "gnome" || style == "gnomed"',
        'style.starts_with("popin")',
        'config.starts_with("workspaces") || config.starts_with("specialWorkspace")',
        'style == "slide" || style == "slidevert" || style == "fade"',
        'config.ends_with("angle")',
        'style == "loop" || style == "once"',
        'config.starts_with("layers")',
        'style.empty() || style == "fade" || style.starts_with("slide")',
    )
    for fragment in required_source_fragments:
        if fragment not in source:
            raise ValueError(
                f"tagged animation style grammar changed near {fragment!r}"
            )

    definitions = config_schema.get("$defs", {})
    actual_leaves = definitions.get("animationLeaf", {}).get("enum")
    if actual_leaves != list(ANIMATION_LEAVES):
        raise ValueError("config schema animation leaf inventory changed")
    expected_style_definitions = {
        "windowAnimationStyle": {
            "type": "string",
            "maxLength": 128,
            "pattern": WINDOW_ANIMATION_STYLE_PATTERN,
        },
        "workspaceAnimationStyle": {
            "type": "string",
            "maxLength": 128,
            "pattern": WORKSPACE_ANIMATION_STYLE_PATTERN,
        },
        "angleAnimationStyle": {"enum": ["", "loop", "once"]},
        "layerAnimationStyle": {
            "type": "string",
            "maxLength": 128,
            "pattern": LAYER_ANIMATION_STYLE_PATTERN,
        },
    }
    for name, expected in expected_style_definitions.items():
        if definitions.get(name) != expected:
            raise ValueError(
                f"config schema {name} does not match reviewed animation grammar"
            )
    for name in (
        "windowAnimationStyle",
        "workspaceAnimationStyle",
        "layerAnimationStyle",
    ):
        pattern = definitions[name]["pattern"]
        if re.fullmatch(pattern, "") is None:
            raise ValueError(f"config schema {name} must accept omitted/empty style")
    if "" not in definitions["angleAnimationStyle"]["enum"]:
        raise ValueError("config schema angleAnimationStyle must accept empty style")

    animation_rules = definitions.get("animation", {}).get("allOf")
    if not isinstance(animation_rules, list):
        raise ValueError("config schema animation style rules are missing")
    actual_groups: dict[str, tuple[str, ...]] = {}
    for rule in animation_rules:
        names = (
            rule.get("if", {})
            .get("properties", {})
            .get("name", {})
            .get("enum")
        )
        style = rule.get("then", {}).get("properties", {}).get("style")
        if not isinstance(names, list) or not isinstance(style, dict):
            raise ValueError("malformed config schema animation style rule")
        key = style.get("$ref") or ("const:" + str(style.get("const")))
        actual_groups[key] = tuple(names)
    expected_groups = {
        "#/$defs/windowAnimationStyle": WINDOW_ANIMATION_LEAVES,
        "#/$defs/workspaceAnimationStyle": WORKSPACE_ANIMATION_LEAVES,
        "#/$defs/angleAnimationStyle": ANGLE_ANIMATION_LEAVES,
        "#/$defs/layerAnimationStyle": LAYER_ANIMATION_LEAVES,
        "const:": NO_STYLE_ANIMATION_LEAVES,
    }
    if actual_groups != expected_groups:
        raise ValueError(
            "config schema animation leaf/style grouping does not match tagged source"
        )

    nested_style_refs = {
        "workspace override": (
            definitions.get("workspaceOverrides", {}).get("properties", {}).get("animation"),
            "#/$defs/workspaceAnimationStyle",
        ),
        "window rule effect": (
            definitions.get("windowEffects", {}).get("properties", {}).get("animation"),
            "#/$defs/windowAnimationStyle",
        ),
        "layer rule effect": (
            definitions.get("layerEffects", {}).get("properties", {}).get("animation"),
            "#/$defs/layerAnimationStyle",
        ),
    }
    for surface, (actual, expected_ref) in nested_style_refs.items():
        if actual != {"$ref": expected_ref}:
            raise ValueError(f"{surface} bypasses its closed animation style grammar")

    controller_fragments = {
        Path("src/desktop/view/animationControllers/WindowAnimationController.cpp"): (
            'STYLE.starts_with("slide")', 'STYLE == "gnomed" || STYLE == "gnome"',
            "applyPopin(ctx, close, percentageFromStyle(STYLE))",
        ),
        Path("src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"): (
            'ANIMSTYLE.starts_with("slide")', 'ANIMSTYLE.starts_with("popin")',
        ),
        Path("src/animation/WorkspaceAnimationController.cpp"): (
            'ANIMSTYLE.starts_with("slidevert")',
            'ANIMSTYLE.starts_with("slidefade")', 'ANIMSTYLE == "fade"',
        ),
    }
    for path, fragments in controller_fragments.items():
        controller_source = complex_sources[path].decode("utf-8")
        for fragment in fragments:
            if fragment not in controller_source:
                raise ValueError(
                    f"tagged rule animation controller changed in {path} near {fragment!r}"
                )


def _assert_monitor_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    source = complex_sources[
        Path("src/config/shared/monitor/Parser.cpp")
    ].decode("utf-8")
    required_source_fragments = (
        'value.starts_with("pref")',
        'value.starts_with("highrr")',
        'value.starts_with("highres")',
        'value.starts_with("maxwidth")',
        "parseModeLine(value, m_rule.m_drmMode)",
        'value == "auto-right"',
        'value == "auto-left"',
        'value == "auto-up"',
        'value == "auto-down"',
        'value == "auto-center-right"',
        'value == "auto-center-left"',
        'value == "auto-center-up"',
        'value == "auto-center-down"',
        "m_rule.m_offset.x = stoi",
        "m_rule.m_offset.y = stoi",
    )
    for fragment in required_source_fragments:
        if fragment not in source:
            raise ValueError(f"tagged monitor grammar changed near {fragment!r}")

    definitions = config_schema.get("$defs", {})
    expected_mode = {
        "oneOf": [
            {"enum": list(MONITOR_MODE_KEYWORDS)},
            {
                "type": "string",
                "maxLength": 24,
                "pattern": MONITOR_MODE_PATTERN,
            },
        ],
        "$comment": (
            "Managed v1 intentionally excludes raw DRM modelines. Width and "
            "height are bounded to five decimal digits; refresh is greater than "
            "zero and below 10000 Hz with at most three fractional digits."
        ),
    }
    expected_position = {
        "oneOf": [
            {"enum": list(MONITOR_AUTO_POSITIONS)},
            {
                "type": "string",
                "maxLength": 17,
                "pattern": MONITOR_POSITION_PATTERN,
            },
        ],
        "$comment": (
            "Explicit monitor coordinates use the same plus-or-minus one-million "
            "logical-pixel safety envelope as typed compositor coordinates."
        ),
    }
    if definitions.get("monitorMode") != expected_mode:
        raise ValueError(
            "config schema monitor mode grammar is not the reviewed contract"
        )
    if definitions.get("monitorPosition") != expected_position:
        raise ValueError(
            "config schema monitor position grammar is not the reviewed contract"
        )

    mode_pattern = re.compile(MONITOR_MODE_PATTERN)
    position_pattern = re.compile(MONITOR_POSITION_PATTERN)
    valid_modes = ("1920x1080", "3840x2160@144", "800x600@59.94")
    invalid_modes = (
        "modeline 1 2 3 4 5 6 7 8 9 10",
        "preferred-extra", "0x1080", "1920x0", "1920x1080@0",
        "100000x1080", "1920x1080@10000",
    )
    if not all(mode_pattern.fullmatch(value) for value in valid_modes):
        raise ValueError("reviewed monitor mode regex rejects a valid managed mode")
    if any(
        value in MONITOR_MODE_KEYWORDS or mode_pattern.fullmatch(value)
        for value in invalid_modes
    ):
        raise ValueError("reviewed monitor mode regex accepts an excluded mode")

    valid_positions = ("0x0", "+1x-1", "-1000000x1000000")
    invalid_positions = (
        "1000001x0", "0x-1000001", "auto-diagonal", "1.5x2", "1x2x3",
    )
    if not all(position_pattern.fullmatch(value) for value in valid_positions):
        raise ValueError("reviewed monitor position regex rejects a valid position")
    if any(
        value in MONITOR_AUTO_POSITIONS or position_pattern.fullmatch(value)
        for value in invalid_positions
    ):
        raise ValueError("reviewed monitor position regex accepts an excluded position")


def _assert_dispatcher_action_bounds(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    source = complex_sources[
        Path("src/config/shared/actions/ConfigActions.cpp")
    ].decode("utf-8")
    if "if (sig < 1 || sig > 31)" not in source:
        raise ValueError("tagged window.signal range changed")
    signal = (
        config_schema.get("$defs", {})
        .get("windowSignalArguments", {})
        .get("properties", {})
        .get("signal")
    )
    if signal != {"type": "integer", "minimum": 1, "maximum": 31}:
        raise ValueError("config schema window.signal range is not the tagged range")


def _cpp_lua_wrapper(source: str, name: str) -> str:
    marker = f"static int {name}(lua_State* L) {{"
    start = source.find(marker)
    if start < 0:
        raise ValueError(f"tagged Lua wrapper {name} is missing")
    end = source.find("\n}\n", start)
    if end < 0:
        raise ValueError(f"tagged Lua wrapper {name} is unterminated")
    return source[start:end + 2]


def _assert_action_invocation_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    if set(DISPATCHER_INVOCATIONS) != set(DISPATCHER_ARGUMENT_SCHEMAS):
        raise ValueError("dispatcher invocation inventory is incomplete")
    kind_counts: dict[str, int] = {}
    for descriptor in DISPATCHER_INVOCATIONS.values():
        kind = descriptor["kind"]
        kind_counts[kind] = kind_counts.get(kind, 0) + 1
    if kind_counts != {
        "none": 14,
        "table": 27,
        "scalar": 5,
        "empty-object-none-otherwise-table": 1,
    }:
        raise ValueError(f"dispatcher invocation kind inventory changed: {kind_counts}")

    definitions = config_schema.get("$defs", {})
    for action in DISPATCHER_NONE_INVOCATIONS:
        schema_name = DISPATCHER_ARGUMENT_SCHEMAS[action]
        if schema_name != "emptyArguments":
            raise ValueError(f"no-argument dispatcher {action} has a nonempty schema")
    for action, field in DISPATCHER_SCALAR_INVOCATIONS.items():
        schema_name = DISPATCHER_ARGUMENT_SCHEMAS[action]
        argument_schema = definitions.get(schema_name, {})
        if field not in argument_schema.get("required", []):
            raise ValueError(
                f"scalar dispatcher {action} field {field!r} is not required"
            )
        if field not in argument_schema.get("properties", {}):
            raise ValueError(
                f"scalar dispatcher {action} field {field!r} is not defined"
            )
    if DISPATCHER_ARGUMENT_SCHEMAS.get("window.resize") != "windowResizeArguments":
        raise ValueError("window.resize conditional invocation schema changed")

    dispatcher_source = complex_sources[DISPATCHER_SOURCE_PATH].decode("utf-8")
    scalar_wrappers = {
        "event": ("hlEvent", "Check::string(L, 1)"),
        "force_idle": ("hlForceIdle", "Check::number(L, 1)"),
        "global": ("hlGlobal", "Check::string(L, 1)"),
        "submap": ("hlSubmap", "Check::string(L, 1)"),
        "workspace.toggle_special": (
            "hlWorkspaceToggleSpecial",
            "lua_pushstring(L, lua_tostring(L, 1))",
        ),
    }
    for action, (wrapper_name, fragment) in scalar_wrappers.items():
        if action not in DISPATCHER_SCALAR_INVOCATIONS:
            raise ValueError(f"tagged scalar wrapper {action} is not classified")
        if fragment not in _cpp_lua_wrapper(dispatcher_source, wrapper_name):
            raise ValueError(
                f"tagged scalar wrapper {wrapper_name} changed near {fragment!r}"
            )

    no_argument_wrappers = {
        "exit": "hlExit",
        "force_renderer_reload": "hlForceRendererReload",
        "release_input_capture": "hlReleaseInputCapture",
        "no_op": "hlNoop",
        "group.toggle": "hlGroupToggle",
        "group.next": "hlGroupNext",
        "group.prev": "hlGroupPrev",
        "window.close": "hlWindowClose",
        "window.kill": "hlWindowKill",
        "window.center": "hlWindowCenter",
        "window.clear_tags": "hlWindowClearTags",
        "window.toggle_swallow": "hlWindowToggleSwallow",
        "window.bring_to_top": "hlWindowBringToTop",
        "window.drag": "hlWindowDrag",
    }
    for action, wrapper_name in no_argument_wrappers.items():
        body = _cpp_lua_wrapper(dispatcher_source, wrapper_name)
        if action not in DISPATCHER_NONE_INVOCATIONS:
            raise ValueError(f"tagged no-argument wrapper {action} is not classified")
        if any(fragment in body for fragment in (
            "Check::", "lua_istable(L, 1)", "lua_tostring(L, 1)",
            "lua_tonumber(L, 1)",
        )):
            raise ValueError(f"tagged no-argument wrapper {wrapper_name} now reads input")

    resize = _cpp_lua_wrapper(dispatcher_source, "hlWindowResize")
    resize_fragments = (
        "lua_gettop(L) == 0 || lua_isnil(L, 1)",
        "if (!lua_istable(L, 1))",
        'tableOptNum(L, 1, "x")',
        'tableOptNum(L, 1, "y")',
        'tableOptBool(L, 1, "keep_aspect_ratio")',
    )
    for fragment in resize_fragments:
        if fragment not in resize:
            raise ValueError(
                f"tagged window.resize invocation changed near {fragment!r}"
            )

    gesture_source = complex_sources[
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp")
    ].decode("utf-8")
    gesture_fragments = (
        'Internal::parseTableField(L, 1, "action", actionParser)',
        'GET_ACTION_STRING(zoomLevel, "zoom_level")',
        'GET_ACTION_STRING(workspaceName, "workspace_name")',
        'GET_ACTION_STRING(mode, "mode")',
    )
    for fragment in gesture_fragments:
        if fragment not in gesture_source:
            raise ValueError(
                f"tagged gesture table invocation changed near {fragment!r}"
            )


def _assert_key_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    definitions = config_schema.get("$defs", {})
    binding_key = definitions.get("bindingKey", {}).get("oneOf")
    if not isinstance(binding_key, list) or len(binding_key) != 4:
        raise ValueError("config schema bindingKey is not the reviewed four-form union")
    if binding_key[0].get("enum") != [
        "catchall", "mouse_down", "mouse_up", "mouse_left", "mouse_right",
    ]:
        raise ValueError("config schema binding special-key inventory changed")
    if binding_key[1].get("pattern") != BINDING_MOUSE_PATTERN:
        raise ValueError("config schema binding mouse range changed")
    if binding_key[2].get("pattern") != BINDING_CODE_PATTERN:
        raise ValueError("config schema binding keycode range changed")
    if binding_key[3].get("pattern") != KEYSYM_TOKEN_PATTERN:
        raise ValueError("config schema binding keysym token grammar changed")

    action_modifiers = definitions.get("actionModifiers", {})
    if action_modifiers.get("pattern") != ACTION_MODIFIERS_PATTERN:
        raise ValueError("config schema action modifier grammar changed")
    modifier_regex = re.compile(ACTION_MODIFIERS_PATTERN)
    canonical_modifier_strings = {
        " ".join(
            token for index, token in enumerate(ACTION_MODIFIER_TOKENS)
            if mask & (1 << index)
        )
        for mask in range(1 << len(ACTION_MODIFIER_TOKENS))
    }
    if len(canonical_modifier_strings) != 256 or not all(
        modifier_regex.fullmatch(value) for value in canonical_modifier_strings
    ):
        raise ValueError("action modifier grammar rejects a canonical ordered subset")
    invalid_modifiers = (
        " SHIFT", "SHIFT ", "SHIFT  CTRL", "SHIFTSUPER", "SUPER SHIFT",
        "CTRL CTRL", "CONTROL", "MOD1", "shift", "CAPS SHIFT",
    )
    if any(modifier_regex.fullmatch(value) for value in invalid_modifiers):
        raise ValueError("action modifier grammar accepts a noncanonical string")

    action_key = definitions.get("actionKey", {}).get("oneOf")
    if not isinstance(action_key, list) or len(action_key) != 3:
        raise ValueError("config schema actionKey is not the reviewed three-form union")
    if action_key[0].get("pattern") != BINDING_MOUSE_PATTERN:
        raise ValueError("config schema action mouse range changed")
    if action_key[1].get("pattern") != ACTION_CODE_PATTERN:
        raise ValueError("config schema action keycode range changed")
    if action_key[2].get("pattern") != KEYSYM_TOKEN_PATTERN:
        raise ValueError("config schema action keysym token grammar changed")

    boundary_cases = (
        (BINDING_MOUSE_PATTERN, ("mouse:272", "mouse:767"), ("mouse:271", "mouse:768")),
        (BINDING_CODE_PATTERN, ("code:0", "code:4294967294"), ("code:4294967295", "code:00")),
        (ACTION_CODE_PATTERN, ("code:0", "code:2147483647"), ("code:2147483648", "code:00")),
    )
    for pattern, accepted, rejected in boundary_cases:
        compiled = re.compile(pattern)
        if not all(compiled.fullmatch(value) for value in accepted):
            raise ValueError(f"key token grammar rejects boundary for {pattern!r}")
        if any(compiled.fullmatch(value) for value in rejected):
            raise ValueError(f"key token grammar accepts out-of-range value for {pattern!r}")

    bind_source = complex_sources[
        Path("src/config/lua/bindings/LuaBindingsToplevel.cpp")
    ].decode("utf-8")
    bind_fragments = (
        'sv == "catchall"',
        'sv == "mouse_down" || sv == "mouse_up" || sv == "mouse_left" || sv == "mouse_right"',
        'arg.starts_with("code:")',
        "strToNumber<uint32_t>(arg.substr(5))",
        "xkb_keysym_from_name",
        "XKB_KEYSYM_CASE_INSENSITIVE",
    )
    for fragment in bind_fragments:
        if fragment not in bind_source:
            raise ValueError(f"tagged binding key grammar changed near {fragment!r}")

    dispatcher_source = complex_sources[DISPATCHER_SOURCE_PATH].decode("utf-8")
    for fragment in (
        'key.starts_with("code:")',
        'key.starts_with("mouse:")',
        "if (code < 272)",
        "xkb_keysym_from_name",
    ):
        if fragment not in dispatcher_source:
            raise ValueError(f"tagged shortcut key resolver changed near {fragment!r}")

    keybind_source = complex_sources[Path("src/managers/KeybindManager.cpp")].decode(
        "utf-8"
    )
    for token in ("mouse_down", "mouse_up", "mouse_left", "mouse_right"):
        if f'.keyName = "{token}"' not in keybind_source:
            raise ValueError(f"tagged wheel key token {token!r} changed")
    if '"mouse:" + std::to_string(e.button)' not in keybind_source:
        raise ValueError("tagged mouse button token emission changed")


def _assert_window_selector_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    definitions = config_schema.get("$defs", {})
    selector = definitions.get("windowSelector", {}).get("oneOf")
    if not isinstance(selector, list) or len(selector) != 5:
        raise ValueError("config schema windowSelector is not the reviewed union")
    if selector[0].get("enum") != ["active", "floating", "tiled"]:
        raise ValueError("config schema exact window selectors changed")
    if selector[1].get("pattern") != WINDOW_SELECTOR_REGEX_PATTERN:
        raise ValueError("config schema regex window selector grammar changed")
    if selector[2].get("pattern") != r"^stableid:[1-9a-f][0-9a-f]{0,15}$":
        raise ValueError("config schema stable-id selector grammar changed")
    if selector[3].get("pattern") != r"^address:0x[1-9a-f][0-9a-f]{0,15}$":
        raise ValueError("config schema address selector grammar changed")
    if selector[4].get("pattern") != WINDOW_SELECTOR_PID_PATTERN:
        raise ValueError("config schema PID selector grammar changed")

    selector_source = complex_sources[Path("src/desktop/state/ViewQuery.cpp")].decode(
        "utf-8"
    )
    selector_fragments = (
        'regexp.starts_with("active")',
        'regexp.starts_with("floating") || regexp.starts_with("tiled")',
        'regexp.starts_with("class:")',
        'regexp.starts_with("initialclass:")',
        'regexp.starts_with("title:")',
        'regexp.starts_with("initialtitle:")',
        'regexp.starts_with("tag:")',
        'regexp.starts_with("stableid:")',
        'regexp.starts_with("address:")',
        'regexp.starts_with("pid:")',
        "RE2::FullMatch",
        'std::format("{:x}", w->m_stableID)',
        'std::format("0x{:x}", rc<uintptr_t>(w.get()))',
    )
    for fragment in selector_fragments:
        if fragment not in selector_source:
            raise ValueError(f"tagged window selector grammar changed near {fragment!r}")

    selector_fields: list[tuple[str, str]] = []
    for root in sorted(set(DISPATCHER_ARGUMENT_SCHEMAS.values())):
        def visit(value: Any) -> None:
            if isinstance(value, dict):
                properties = value.get("properties")
                if isinstance(properties, dict):
                    for field in ("window", "target"):
                        if field in properties:
                            if properties[field] != {"$ref": "#/$defs/windowSelector"}:
                                raise ValueError(
                                    f"action selector {root}.{field} bypasses windowSelector"
                                )
                            selector_fields.append((root, field))
                for child in value.values():
                    visit(child)
            elif isinstance(value, list):
                for child in value:
                    visit(child)

        visit(definitions[root])
    if sorted(selector_fields) != [
        ("passArguments", "window"),
        ("sendKeyStateArguments", "window"),
        ("sendShortcutArguments", "window"),
        ("windowSwapArguments", "target"),
    ]:
        raise ValueError(
            f"managed action selector inventory is not fail-closed: {selector_fields!r}"
        )

    dispatcher_source = complex_sources[DISPATCHER_SOURCE_PATH].decode("utf-8")
    fail_closed_fragments = (
        'if (!PWINDOW)\n        return Internal::dispatcherError(L, "hl.pass: window not found"',
        'return Internal::dispatcherError(L, "send_shortcut: window not found"',
        'return Internal::dispatcherError(L, "send_key_state: window not found"',
        'return Internal::dispatcherError(L, "hl.window.swap: target window not found"',
    )
    for fragment in fail_closed_fragments:
        if fragment not in dispatcher_source:
            raise ValueError(f"tagged action selector stopped failing closed near {fragment!r}")
    actions_source = complex_sources[
        Path("src/config/shared/actions/ConfigActions.cpp")
    ].decode("utf-8")
    if "return window.value_or(Desktop::focusState()->window());" not in actions_source:
        raise ValueError("tagged focused-window fallback semantics changed")


def _assert_workspace_selector_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    definitions = config_schema.get("$defs", {})
    selector = definitions.get("workspaceSelector", {}).get("oneOf")
    if not isinstance(selector, list) or len(selector) != 3:
        raise ValueError("config schema workspaceSelector is not the reviewed union")
    expected_patterns = (
        WORKSPACE_SELECTOR_ID_PATTERN,
        WORKSPACE_SELECTOR_NAME_PATTERN,
        WORKSPACE_SELECTOR_SPECIAL_PATTERN,
    )
    if tuple(branch.get("pattern") for branch in selector) != expected_patterns:
        raise ValueError("config schema workspace selector grammar changed")

    accepted = (
        "1", "2147483647", "name:main", "name:work.one",
        "special", "special:scratch-pad",
    )
    rejected = (
        "", "0", "2147483648", "01", "main", "name:", "special:",
        "name:two words", "special:two words", "m+1", "r[1-3]", "[name:foo]",
        "name:bad\nname", "special:\u200bhidden",
    )
    compiled = tuple(re.compile(pattern) for pattern in expected_patterns)
    if any(not any(regex.fullmatch(value) for regex in compiled) for value in accepted):
        raise ValueError("workspace selector grammar rejects a managed selector")
    if any(any(regex.fullmatch(value) for regex in compiled) for value in rejected):
        raise ValueError("workspace selector grammar accepts an excluded selector")

    existing = definitions.get("existingWorkspaceSelector", {})
    if existing.get("$ref") != "#/$defs/workspaceSelector":
        raise ValueError("existing workspace selector does not reuse workspaceSelector")
    workspace_spec = definitions.get("workspaceSpec", {}).get("oneOf")
    if workspace_spec != [
        {"$ref": "#/$defs/workspaceSelector"},
        {"enum": list(WORKSPACE_SPEC_KEYWORDS)},
    ]:
        raise ValueError("workspace spec inventory changed")

    monitor_spec = definitions.get("monitorSpec", {}).get("oneOf")
    if not isinstance(monitor_spec, list) or len(monitor_spec) != 5:
        raise ValueError("monitorSpec is not the reviewed five-form union")
    if monitor_spec[0].get("enum") != ["current", "left", "right", "up", "down"]:
        raise ValueError("monitor exact selector inventory changed")
    expected_monitor_patterns = (
        MONITOR_SPEC_RELATIVE_PATTERN,
        MONITOR_SPEC_ID_PATTERN,
        MONITOR_SPEC_NAME_PATTERN,
        MONITOR_SPEC_DESCRIPTION_PATTERN,
    )
    if tuple(branch.get("pattern") for branch in monitor_spec[1:]) != expected_monitor_patterns:
        raise ValueError("monitor selector grammar changed")

    monitor_patterns = tuple(re.compile(pattern) for pattern in expected_monitor_patterns)
    monitor_exact = set(monitor_spec[0]["enum"])
    accepted_monitors = (
        "current", "left", "+1", "-2147483647", "0", "2147483647",
        "DP-1", "desc:Built-in Display",
    )
    rejected_monitors = (
        "+0", "-0", "+01", "2147483648", "01", "desc:",
        "two outputs", "current ", "DP-1\nDP-2",
    )
    matches_monitor = lambda value: value in monitor_exact or any(
        pattern.fullmatch(value) for pattern in monitor_patterns
    )
    if any(not matches_monitor(value) for value in accepted_monitors):
        raise ValueError("monitor selector grammar rejects a managed selector")
    if any(matches_monitor(value) for value in rejected_monitors):
        raise ValueError("monitor selector grammar accepts an excluded selector")

    static_monitor = definitions.get("staticMonitorSelector", {}).get("oneOf")
    if not isinstance(static_monitor, list) or len(static_monitor) != 2:
        raise ValueError("staticMonitorSelector is not the reviewed union")
    if (
        static_monitor[0].get("pattern") != MONITOR_SPEC_NAME_PATTERN
        or static_monitor[1].get("pattern") != MONITOR_SPEC_DESCRIPTION_PATTERN
    ):
        raise ValueError("static monitor selector grammar changed")
    workspace_monitor = (
        definitions.get("workspaceRule", {}).get("properties", {}).get("monitor")
    )
    if workspace_monitor != {
        "oneOf": [{"const": ""}, {"$ref": "#/$defs/staticMonitorSelector"}],
    }:
        raise ValueError("workspace rule monitor is not a static selector")
    monitor_selector = (
        definitions.get("monitor", {}).get("properties", {}).get("selector")
    )
    if monitor_selector != {"$ref": "#/$defs/staticMonitorSelector"}:
        raise ValueError("managed monitor selector is not a static selector")
    monitor_mirror = (
        definitions.get("monitor", {}).get("properties", {}).get("mirror")
    )
    if monitor_mirror != {
        "oneOf": [{"const": ""}, {"$ref": "#/$defs/staticMonitorSelector"}],
    }:
        raise ValueError("monitor mirror is not empty/static-selector typed")

    workspace_source = complex_sources[Path("src/helpers/MiscFunctions.cpp")].decode(
        "utf-8"
    )
    for fragment in (
        'in.starts_with("special")', 'in.starts_with("name:")',
        'in.starts_with("empty")', 'in.starts_with("prev")', 'in == "next"',
        "in[0] == 'r'", "(in[0] == 'm' || in[0] == 'e')",
    ):
        if fragment not in workspace_source:
            raise ValueError(f"tagged workspace spec grammar changed near {fragment!r}")
    existing_source = complex_sources[Path("src/state/WorkspaceQueryCore.cpp")].decode(
        "utf-8"
    )
    for fragment in (
        'm_string->starts_with("name:")', '*m_string == "special"',
        'm_string->starts_with("special:")', "isNumber(std::string{*m_string})",
    ):
        if fragment not in existing_source:
            raise ValueError(
                f"tagged existing-workspace selector changed near {fragment!r}"
            )
    monitor_source = complex_sources[Path("src/state/MonitorQueryCore.cpp")].decode(
        "utf-8"
    )
    for fragment in (
        'sv == "current"', "isDirection(sv)", "sv[0] == '+' || sv[0] == '-'",
        "isNumber(std::string{sv})", "m->matchesStaticSelector(sv)",
    ):
        if fragment not in monitor_source:
            raise ValueError(f"tagged monitor selector grammar changed near {fragment!r}")


def _assert_window_effect_contract(
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    definitions = config_schema.get("$defs", {})
    expected_fullscreen_state = {
        "type": "object",
        "additionalProperties": False,
        "required": ["internal"],
        "properties": {
            "internal": {"$ref": "#/$defs/fullscreenMode"},
            "client": {"$ref": "#/$defs/fullscreenMode"},
        },
        "$comment": "A missing client mode preserves the tagged parser's optional client state.",
    }
    if definitions.get("windowFullscreenStateEffect") != expected_fullscreen_state:
        raise ValueError("config schema window fullscreen-state effect changed")
    opacity = definitions.get("windowOpacityEffect", {})
    if opacity.get("required") != [
        "active", "overrideActive", "overrideInactive", "overrideFullscreen",
    ]:
        raise ValueError("config schema opacity canonical fields changed")
    opacity_properties = opacity.get("properties", {})
    for field in ("active", "inactive", "fullscreen"):
        if opacity_properties.get(field) != {
            "type": "number", "minimum": 0, "maximum": 1,
        }:
            raise ValueError(f"config schema opacity field {field} is not bounded")
    for field in ("overrideActive", "overrideInactive", "overrideFullscreen"):
        if opacity_properties.get(field) != {"type": "boolean"}:
            raise ValueError(f"config schema opacity flag {field} changed")
    suppress = definitions.get("suppressEvents", {})
    expected_events = [
        "fullscreen", "maximize", "activate", "activatefocus",
        "fullscreenoutput", "x11configurerequest",
    ]
    if (
        suppress.get("minItems") != 1
        or suppress.get("maxItems") != len(expected_events)
        or suppress.get("uniqueItems") is not True
        or suppress.get("items", {}).get("enum") != expected_events
    ):
        raise ValueError("config schema suppress-event inventory changed")

    window_effects = definitions.get("windowEffects", {}).get("properties", {})
    if "group" in window_effects:
        raise ValueError("contextual window group mini-language is managed")
    for field, expected_ref in (
        ("monitor", "#/$defs/monitorEffect"),
        ("workspace", "#/$defs/workspaceEffect"),
        ("tag", "#/$defs/windowTagToken"),
    ):
        if window_effects.get(field) != {"$ref": expected_ref}:
            raise ValueError(f"config schema window effect {field} is not typed")
    window_match_workspace = (
        definitions.get("windowMatch", {}).get("properties", {}).get("workspace")
    )
    if window_match_workspace != {"$ref": "#/$defs/workspaceSelector"}:
        raise ValueError("window match workspace bypasses the workspace selector")
    if window_effects.get("content") != {
        "enum": ["none", "photo", "video", "game"]
    }:
        raise ValueError("config schema content effect inventory changed")
    no_close = window_effects.get("no_close_for", {})
    if (
        no_close.get("type") != "integer"
        or no_close.get("minimum") != 0
        or no_close.get("maximum") != 2147483647
    ):
        raise ValueError("config schema no_close_for is not bounded to tagged int")

    source_fragments = {
        Path("src/desktop/rule/windowRule/WindowRule.hpp"): (
            "int                internal = 0;",
            "std::optional<int> client;",
            "Types::SAlphaValue alphaInactive;",
            "Types::SAlphaValue alphaFullscreen;",
        ),
        Path("src/desktop/rule/windowRule/WindowRule.cpp"): (
            "if (opacityIDX == 1)",
            "result.alphaInactive   = result.alpha;",
            'if (r == "override")',
        ),
        Path("src/desktop/types/OverridableVar.hpp"): (
            "float alpha      = 1.F;",
            "bool  overridden = false;",
        ),
        Path("src/protocols/types/ContentType.cpp"): (
            '{"none", CONTENT_TYPE_NONE}',
            '{"photo", CONTENT_TYPE_PHOTO}',
            '{"video", CONTENT_TYPE_VIDEO}',
            '{"game", CONTENT_TYPE_GAME}',
        ),
        Path("src/desktop/view/Window.cpp"): tuple(
            f'var == "{event}"' for event in expected_events
        ),
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"): (
            "static_.noCloseFor = std::get<int64_t>(value);",
        ),
    }
    for path, fragments in source_fragments.items():
        source = complex_sources[path].decode("utf-8")
        for fragment in fragments:
            if fragment not in source:
                raise ValueError(
                    f"tagged window-effect grammar changed in {path} near {fragment!r}"
                )


def _assert_numeric_safety_contract(
    options: list[RawOption],
    complex_sources: dict[Path, bytes],
    config_schema: dict[str, Any],
) -> None:
    definitions = config_schema.get("$defs", {})
    monitor = definitions.get("monitor", {}).get("properties", {})
    expected_monitor_bounds = {
        "sdrBrightness": {"type": "number", "minimum": 0, "maximum": 10},
        "sdrSaturation": {"type": "number", "minimum": 0, "maximum": 10},
        "sdrMinLuminance": {"type": "number", "minimum": 0, "maximum": 10000},
        "sdrMaxLuminance": {"type": "integer", "minimum": -1, "maximum": 2147483647},
        "minLuminance": {"type": "number", "minimum": -1, "maximum": 10000},
        "maxLuminance": {"type": "integer", "minimum": -1, "maximum": 2147483647},
        "maxAvgLuminance": {"type": "integer", "minimum": -1, "maximum": 2147483647},
    }
    for field, expected in expected_monitor_bounds.items():
        if monitor.get(field) != expected:
            raise ValueError(f"config schema monitor numeric bound changed for {field}")
    scale_numeric = definitions.get("monitorScale", {}).get("anyOf", [{}])[0]
    if scale_numeric != {"type": "number", "minimum": 0.25, "maximum": FLOAT_MAX}:
        raise ValueError("config schema monitor scale is not finite-float bounded")
    device_transform = (
        definitions.get("deviceOverrides", {}).get("properties", {}).get("transform")
    )
    if device_transform != {"type": "integer", "minimum": -1, "maximum": 7}:
        raise ValueError("config schema device transform is not tagged -1..7")
    device_properties = definitions.get("deviceOverrides", {}).get("properties", {})
    if device_properties.get("accel_profile") != {
        "enum": ["", "adaptive", "flat"],
    }:
        raise ValueError("device accel profile exposes an untyped custom curve")
    if any(field in device_properties for field in ("scroll_points", "tags", "output")):
        raise ValueError("device overrides expose a deferred opaque mini-language")
    scrolling_width = (
        definitions.get("windowEffects", {})
        .get("properties", {})
        .get("scrolling_width", {})
    )
    if (
        scrolling_width.get("type") != "number"
        or scrolling_width.get("minimum") != 0
        or scrolling_width.get("maximum") != 1
    ):
        raise ValueError("config schema scrolling_width is not bounded 0..1")

    raw_by_path = {option.path: option for option in options}
    for path, expected in CATALOG_CONSTRAINT_OVERRIDES.items():
        option = raw_by_path.get(path)
        if option is None:
            raise ValueError(f"numeric safety overlay path disappeared: {path}")
        actual = catalog_constraints(option)
        for key, value in expected.items():
            if actual.get(key) != value:
                raise ValueError(f"numeric safety overlay changed for {path}.{key}")

    types_source = complex_sources[Path("src/config/shared/Types.hpp")].decode(
        "utf-8"
    )
    if "typedef float                   FLOAT;" not in types_source:
        raise ValueError("tagged Config::FLOAT storage changed")
    input_source = complex_sources[Path("src/managers/input/InputManager.cpp")].decode(
        "utf-8"
    )
    if 'getDeviceInt(NAME, "eraser_button_override"' not in input_source:
        raise ValueError("tagged eraser-button override consumer changed")


def _assert_reload_event_runtime_order(
    startup_sources_0560: dict[Path, bytes],
    complex_sources_0561: dict[Path, bytes],
) -> None:
    """Pin every supported 0.56.x startup boundary used by the nonce guard.

    In 0.56.0 the initial normal config is loaded before EventManager exists;
    0.56.1 moved EventManager before that load.  Both versions run config-only
    verification without EventManager and publish hyprland.lock only later from
    startCompositor().  Because the Lua config-reloaded callback can call the
    unguarded Actions::event dispatcher, the generated loader may emit its nonce
    only once that exact-PID lock is present.
    """

    def assert_version(
        version: str,
        sources: dict[Path, bytes],
        *,
        event_manager_before_config: bool,
    ) -> None:
        compositor = sources[Path("src/Compositor.cpp")].decode("utf-8")
        main = sources[Path("src/main.cpp")].decode("utf-8")
        lua_config = sources[Path("src/config/lua/ConfigManager.cpp")].decode(
            "utf-8"
        )
        actions = sources[
            Path("src/config/shared/actions/ConfigActions.cpp")
        ].decode("utf-8")

        verification = compositor.find("if (m_onlyConfigVerification) {")
        verification_init = compositor.find("Config::mgr()->init();", verification)
        verification_return = compositor.find("return;", verification_init)
        if not 0 <= verification < verification_init < verification_return:
            raise ValueError(
                f"tagged {version} config-verification initialization ordering changed"
            )

        priority = compositor.find("case STAGE_PRIORITY: {")
        event_manager = compositor.find(
            "g_pEventManager = makeUnique<CEventManager>();", priority
        )
        normal_config_init = compositor.find("Config::mgr()->init();", priority)
        priority_end = compositor.find("case STAGE_BASICINIT: {", priority)
        if event_manager_before_config:
            expected_order = (
                0 <= priority < event_manager < normal_config_init < priority_end
            )
        else:
            expected_order = (
                0 <= priority < normal_config_init < event_manager < priority_end
            )
        if not expected_order:
            raise ValueError(
                f"tagged {version} EventManager/config initialization ordering changed"
            )
        if compositor.find("initManagers(STAGE_PRIORITY);") < 0:
            raise ValueError(
                f"tagged {version} initServer no longer initializes priority managers"
            )

        start = compositor.find("void CCompositor::startCompositor()")
        lock = compositor.find("createLockFile();", start)
        if start < 0 or lock < start or "createLockFile();" in compositor[:start]:
            raise ValueError(
                f"tagged {version} Hyprland lock publication ordering changed"
            )
        init_server_call = main.find(
            "g_pCompositor->initServer(socketName, socketFd);"
        )
        start_call = main.find(
            "g_pCompositor->startCompositor();", init_server_call
        )
        if not 0 <= init_server_call < start_call:
            raise ValueError(
                f"tagged {version} initServer/startCompositor ordering changed"
            )

        callback = lua_config.find("Event::bus()->m_events.config.reloaded.emit();")
        guarded_native_event = lua_config.find("if (g_pEventManager)", callback)
        native_event = lua_config.find(
            'g_pEventManager->postEvent(SHyprIPCEvent{"configreloaded", ""});',
            guarded_native_event,
        )
        if not 0 <= callback < guarded_native_event < native_event:
            raise ValueError(
                f"tagged {version} Lua config-reloaded event ordering changed"
            )

        action = actions.find("ActionResult Actions::event(const std::string& data)")
        unguarded_dispatch = actions.find(
            'g_pEventManager->postEvent(SHyprIPCEvent{.event = "custom", .data = data});',
            action,
        )
        action_end = actions.find("return {};", unguarded_dispatch)
        if not 0 <= action < unguarded_dispatch < action_end:
            raise ValueError(
                f"tagged {version} custom-event dispatcher contract changed"
            )

    assert_version(
        "0.56.0",
        startup_sources_0560,
        event_manager_before_config=False,
    )
    assert_version(
        "0.56.1",
        complex_sources_0561,
        event_manager_before_config=True,
    )


def _number(expression: str) -> int | float:
    value = expression.strip()
    constants: dict[str, int | float] = {
        "std::numeric_limits<int>::max()": 2147483647,
        "std::numeric_limits<int>::min()": -2147483648,
        "std::numeric_limits<int64_t>::max()": 9223372036854775807,
        "std::numeric_limits<int64_t>::min()": -9223372036854775808,
        "std::numeric_limits<float>::max()": 3.4028234663852886e38,
    }
    if value in constants:
        return constants[value]
    if re.fullmatch(r"[-+]?0[xX][0-9A-Fa-f]+", value):
        return int(value, 16)
    value = re.sub(r"[fFlL]+$", "", value)
    if re.fullmatch(r"[-+]?\d+", value):
        return int(value, 10)
    if re.fullmatch(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?", value):
        return float(value)
    raise ValueError(f"unsupported numeric expression: {expression}")


def _canonical_color(expression: str) -> str:
    match = re.fullmatch(r"CHyprColor\{(.+)\}", expression.strip())
    numeric = _number(match.group(1) if match else expression)
    if not isinstance(numeric, int):
        raise ValueError(f"color default is not integral: {expression}")
    return f"0x{numeric & 0xFFFFFFFF:08X}"


def normalized_default(option: RawOption) -> Any:
    expression = option.default_expression.strip()
    if option.source_type in ("Color", "Gradient") and expression == "-1":
        inherited_from = INHERITED_DEFAULTS.get(option.path)
        if inherited_from is None:
            raise ValueError(
                f"unreviewed inherited {option.source_type} default: {option.path}"
            )
        return {"kind": "inherit", "from": inherited_from}
    if option.source_type == "Bool":
        if expression not in ("true", "false"):
            raise ValueError(f"unsupported bool default: {expression}")
        return expression == "true"
    if option.source_type == "Int":
        value = _number(expression)
        if not isinstance(value, int):
            raise ValueError(f"integer default is not integral: {expression}")
        return value
    if option.source_type == "Float":
        return _number(expression)
    if option.source_type == "String":
        if expression == "STRVAL_EMPTY" or expression == '"[[EMPTY]]"':
            return ""
        return _cpp_string(expression)
    if option.source_type == "Color":
        return _canonical_color(expression)
    if option.source_type == "Gradient":
        return {"colors": [_canonical_color(expression)], "angle": 0}
    if option.source_type == "Vec2":
        match = re.fullmatch(r"Config::VEC2\{\s*(.*?)\s*\}", expression)
        if not match:
            raise ValueError(f"unsupported vector default: {expression}")
        values = _split_top_level(match.group(1)) if match.group(1) else []
        if not values:
            return [0, 0]
        if len(values) != 2:
            raise ValueError(f"vector default does not have two values: {expression}")
        return [_number(value) for value in values]
    if option.source_type == "CssGap":
        value = _number(expression)
        if not isinstance(value, int):
            raise ValueError(f"CSS gap default is not integral: {expression}")
        return [value, value, value, value]
    if option.source_type == "FontWeight":
        value = _number(expression)
        if not isinstance(value, int):
            raise ValueError(f"font weight default is not integral: {expression}")
        return value
    raise AssertionError(option.source_type)


def _extract_member(expression: str, member: str) -> str | None:
    marker = f".{member} ="
    start = expression.find(marker)
    if start < 0:
        return None
    start += len(marker)
    tail = expression[start:].lstrip()
    stack: list[str] = []
    quote: str | None = None
    escaped = False
    matching = {")": "(", "}": "{", "]": "["}
    for index, char in enumerate(tail):
        if quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            continue
        if char in ('"', "'"):
            quote = char
        elif char in "({[":
            stack.append(char)
        elif char in ")}]":
            if stack and stack[-1] == matching[char]:
                stack.pop()
            elif char == "}" and not stack:
                return tail[:index].strip()
            else:
                raise ValueError(f"unbalanced member expression: {expression}")
        elif char == "," and not stack:
            return tail[:index].strip()
    return tail.strip().rstrip("}").strip()


def source_constraints(option: RawOption) -> dict[str, Any]:
    constraints: dict[str, Any] = {}
    minimum = _extract_member(option.options_expression, "min")
    maximum = _extract_member(option.options_expression, "max")
    if minimum is not None:
        constraints["min"] = _number(minimum)
    if maximum is not None:
        constraints["max"] = _number(maximum)

    map_expression = _extract_member(option.options_expression, "map")
    if map_expression is not None:
        match = re.fullmatch(r"OptionMap\{(.*)\}", map_expression, re.DOTALL)
        if not match:
            raise ValueError(f"unsupported OptionMap expression: {map_expression}")
        choices: list[dict[str, Any]] = []
        for entry in _split_top_level(match.group(1)):
            entry_match = re.fullmatch(r"\{\s*(\"(?:\\.|[^\"])*\")\s*,\s*(.+)\}", entry, re.DOTALL)
            if not entry_match:
                raise ValueError(f"unsupported OptionMap entry: {entry}")
            choices.append(
                {
                    "label": _cpp_string(entry_match.group(1)),
                    "value": _number(entry_match.group(2)),
                }
            )
        constraints["choices"] = choices

    validator = _extract_member(option.options_expression, "validator")
    if validator is not None:
        match = re.fullmatch(r"vec2Range\((.*)\)", validator, re.DOTALL)
        if match:
            values = [_number(value) for value in _split_top_level(match.group(1))]
            if len(values) != 4:
                raise ValueError(f"vec2Range does not have four values: {validator}")
            constraints["min"] = [values[0], values[1]]
            constraints["max"] = [values[2], values[3]]
        else:
            match = re.fullmatch(r"strChoice\(\{(.*)\}\)", validator, re.DOTALL)
            if not match:
                raise ValueError(f"unreviewed validator: {validator}")
            constraints["choices"] = [
                {"label": value, "value": value}
                for value in (_cpp_string(item) for item in _split_top_level(match.group(1)))
            ]
    return constraints


def catalog_constraints(option: RawOption) -> dict[str, Any]:
    constraints = source_constraints(option)
    if option.path in STRING_ENUMS:
        constraints["choices"] = [
            {"label": label, "value": value}
            for label, value in STRING_ENUMS[option.path]
        ]
    if option.path in STRING_PATTERNS:
        constraints["pattern"] = STRING_PATTERNS[option.path]
    if option.path in INTEGER_ENUMS:
        constraints["choices"] = [
            {"label": label, "value": value}
            for label, value in INTEGER_ENUMS[option.path]
        ]
    if option.source_type == "String":
        constraints["maxLength"] = 4096
    if option.source_type == "FontWeight":
        constraints["min"] = 0
        constraints["max"] = 2147483647
    constraints.update(CATALOG_CONSTRAINT_OVERRIDES.get(option.path, {}))
    return constraints


def option_type(option: RawOption) -> str:
    if option.path in STRING_ENUMS or option.path in INTEGER_ENUMS or "choices" in source_constraints(option):
        return "enum"
    return TYPE_NAMES[option.source_type]


def option_id(path: str) -> str:
    return "hyprland." + path.replace(":", ".")


def lua_path(path: str) -> list[str]:
    """Return the exact Lua table path used by hl.config.

    Upstream registry paths use colons for table levels and dots for nested
    color groups. Lua identifiers use underscores for the few hyphenated
    upstream spellings. Keeping this mapping in generated data prevents the
    renderer from guessing or special-casing namespaces.
    """
    segments = [segment.replace("-", "_") for segment in re.split(r"[:.]", path)]
    if not segments or any(
        not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", segment)
        for segment in segments
    ):
        raise ValueError(f"option has no safe Lua path: {path}")
    return segments


def output_module(path: str) -> str:
    namespace = path.split(":", 1)[0]
    # input-capture is a sibling Lua table emitted in the input component.
    if namespace == "input-capture":
        return "input"
    return namespace.replace("-", "_")


def ui_tier(path: str) -> str:
    if path in EXTERNAL_OPTIONS:
        return "external"
    if path in EXPERT_OPTIONS or path.startswith(EXPERT_PREFIXES):
        return "expert"
    if path in ADVANCED_OPTIONS or path.startswith(ADVANCED_PREFIXES) or any(part in path for part in ADVANCED_PATH_PARTS):
        return "advanced"
    return "common"


def apply_mode(path: str) -> str:
    if path in RESTART_OPTIONS:
        return "restart"
    return "reload"


def risk(path: str) -> str:
    if path in DANGEROUS_OPTIONS:
        return "dangerous"
    if path in CAUTION_OPTIONS or path.startswith(CAUTION_PREFIXES):
        return "caution"
    return "safe"


def documentation(path: str) -> str:
    return DOCS_BY_NAMESPACE.get(path.split(":", 1)[0], VARIABLES_DOC)


def inventory_record(option: RawOption) -> dict[str, Any]:
    return {
        "path": option.path,
        "module": output_module(option.path),
        "luaPath": lua_path(option.path),
        "sourceType": option.source_type,
        "type": option_type(option),
        "default": normalized_default(option),
        "defaultExpression": option.default_expression,
        "constraints": source_constraints(option),
        "description": option.description,
    }


def catalog_record(option: RawOption, added_in_056: set[str]) -> dict[str, Any]:
    semantic_type = option_type(option)
    return {
        "id": option_id(option.path),
        "path": option.path,
        "module": output_module(option.path),
        "luaPath": lua_path(option.path),
        "type": semantic_type,
        "defaultPolicy": "hyprland",
        "writable": option.path not in {
            "input:scroll_points",
            "input:tablet:output",
            "input:touchdevice:output",
            "scrolling:explicit_column_widths",
        },
        "default": normalized_default(option),
        "uiTier": ui_tier(option.path),
        "control": TYPE_CONTROLS[semantic_type],
        "constraints": catalog_constraints(option),
        "applyMode": apply_mode(option.path),
        "risk": risk(option.path),
        "since": "0.56.0" if option.path in added_in_056 else "0.55.0",
        "description": option.description,
        "documentation": documentation(option.path),
    }


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _validate_qualified_source_hashes() -> None:
    expected_paths = {
        "0.55.0": {Path("VERSION"), REGISTRY_PATH},
        "0.56.0": {Path("VERSION"), *STARTUP_SOURCE_PATHS_0560},
        "0.56.1": {Path("VERSION"), REGISTRY_PATH, *COMPLEX_SOURCE_PATHS},
    }
    if set(QUALIFIED_SOURCE_HASHES) != set(expected_paths):
        raise ValueError("qualified source hash table has an unexpected version set")

    digest_expression = re.compile(r"^[0-9a-f]{64}$")
    for version, paths in expected_paths.items():
        actual_paths = set(QUALIFIED_SOURCE_HASHES[version])
        if actual_paths != paths:
            raise ValueError(
                f"Hyprland {version} qualified source hash coverage changed: "
                f"missing={sorted(str(path) for path in paths - actual_paths)}, "
                f"unexpected={sorted(str(path) for path in actual_paths - paths)}"
            )
        for path, digest in QUALIFIED_SOURCE_HASHES[version].items():
            if not digest_expression.fullmatch(digest):
                raise ValueError(
                    f"Hyprland {version} has an invalid pinned SHA-256 for {path}"
                )


def _read_qualified_source(source_root: Path, version: str, path: Path) -> bytes:
    expected = QUALIFIED_SOURCE_HASHES[version][path]
    data = (source_root / path).read_bytes()
    actual = _sha256(data)
    if actual != expected:
        source = QUALIFIED_SOURCES[version]
        raise ValueError(
            f"Hyprland {version} source provenance mismatch for {path}: "
            f"reviewed {source['tag']}@{source['commit']} requires SHA-256 "
            f"{expected}, found {actual}"
        )
    return data


def _json_bytes(document: Any) -> bytes:
    return (
        json.dumps(document, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    ).encode("utf-8")


def _canonical_json_bytes(document: Any) -> bytes:
    """Match QJsonDocument::Compact over recursively key-sorted QJson data."""
    def normalize(value: Any) -> Any:
        if isinstance(value, dict):
            return {key: normalize(child) for key, child in value.items()}
        if isinstance(value, list):
            return [normalize(child) for child in value]
        # QJson stores every JSON number as double and emits integral values in
        # their shortest representation (1, not Python json's 1.0).
        if isinstance(value, float) and value.is_integer():
            return int(value)
        return value

    return json.dumps(
        normalize(document),
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def _invalid_json_constant(token: str) -> None:
    raise ValueError(f"non-JSON numeric constant {token!r}")


def _strict_json(data: bytes, label: str) -> Any:
    def object_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{label}: duplicate JSON key {key!r}")
            result[key] = value
        return result

    try:
        return json.loads(
            data,
            object_pairs_hook=object_pairs,
            parse_constant=_invalid_json_constant,
        )
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as error:
        raise ValueError(f"{label}: invalid JSON: {error}") from error


def _assert_bounded_action_numbers(config_schema: dict[str, Any]) -> None:
    """Fail if a managed action can reach an open numeric payload leaf."""
    definitions = config_schema.get("$defs")
    if not isinstance(definitions, dict):
        raise ValueError("config schema has no $defs object")

    roots = set(DISPATCHER_ARGUMENT_SCHEMAS.values()) | {
        "emptyArguments",
        "gestureAction",
    }
    visited_definitions: set[str] = set()
    unbounded: list[str] = []

    def visit(value: Any, path: str) -> None:
        if isinstance(value, list):
            for index, child in enumerate(value):
                visit(child, f"{path}[{index}]")
            return
        if not isinstance(value, dict):
            return

        reference = value.get("$ref")
        if isinstance(reference, str) and reference.startswith("#/$defs/"):
            name = reference.removeprefix("#/$defs/")
            if name not in definitions:
                raise ValueError(f"action schema reference does not exist: {reference}")
            if name not in visited_definitions:
                visited_definitions.add(name)
                visit(definitions[name], f"$defs.{name}")

        declared_type = value.get("type")
        declared_types = (
            {declared_type}
            if isinstance(declared_type, str)
            else set(declared_type)
            if isinstance(declared_type, list)
            else set()
        )
        numeric = bool(declared_types & {"integer", "number"})
        closed = "enum" in value or "const" in value
        if numeric and not closed:
            minimum = value.get("minimum")
            maximum = value.get("maximum")
            if (
                not isinstance(minimum, (int, float))
                or isinstance(minimum, bool)
                or not isinstance(maximum, (int, float))
                or isinstance(maximum, bool)
                or not math.isfinite(minimum)
                or not math.isfinite(maximum)
                or minimum > maximum
            ):
                unbounded.append(path)

        for key, child in value.items():
            if key != "$ref":
                visit(child, f"{path}.{key}")

    for root in sorted(roots):
        if root not in definitions:
            raise ValueError(f"managed action schema root does not exist: {root}")
        if root not in visited_definitions:
            visited_definitions.add(root)
            visit(definitions[root], f"$defs.{root}")
    if unbounded:
        raise ValueError(
            "managed action numeric leaves require finite minimum and maximum: "
            + ", ".join(sorted(set(unbounded)))
        )


def _assert_source_manifest_schema(source_schema: dict[str, Any]) -> None:
    properties = source_schema.get("properties", {})
    complex_property = properties.get("complexSources", {})
    expected_count = len(COMPLEX_SOURCE_PATHS)
    if (
        complex_property.get("minItems") != expected_count
        or complex_property.get("maxItems") != expected_count
    ):
        raise ValueError("source manifest complex-source count is stale")

    definitions = source_schema.get("$defs", {})
    branches = definitions.get("complexSource", {}).get("oneOf")
    if not isinstance(branches, list):
        raise ValueError("source manifest has no closed complex source inventory")
    actual: list[tuple[str, str]] = []
    for branch in branches:
        branch_properties = branch.get("properties", {})
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if not isinstance(path, str) or not isinstance(digest, str):
            raise ValueError("source manifest complex source branch is malformed")
        actual.append((path, digest))
    expected = [
        (path.as_posix(), QUALIFIED_SOURCE_HASHES["0.56.1"][path])
        for path in sorted(COMPLEX_SOURCE_PATHS)
    ]
    if actual != expected:
        raise ValueError("source manifest complex source inventory/pins are stale")

    startup_property = properties.get("startupSources", {})
    startup_count = len(STARTUP_SOURCE_PATHS_0560)
    if (
        startup_property.get("minItems") != startup_count
        or startup_property.get("maxItems") != startup_count
    ):
        raise ValueError("source manifest startup-source count is stale")
    startup_branches = definitions.get("startupSource", {}).get("oneOf")
    if not isinstance(startup_branches, list):
        raise ValueError("source manifest has no closed startup source inventory")
    actual_startup: list[tuple[str, str]] = []
    for branch in startup_branches:
        branch_properties = branch.get("properties", {})
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if not isinstance(path, str) or not isinstance(digest, str):
            raise ValueError("source manifest startup source branch is malformed")
        actual_startup.append((path, digest))
    expected_startup = [
        (path.as_posix(), QUALIFIED_SOURCE_HASHES["0.56.0"][path])
        for path in sorted(STARTUP_SOURCE_PATHS_0560)
    ]
    if actual_startup != expected_startup:
        raise ValueError("source manifest startup source inventory/pins are stale")

    version_branches = definitions.get("versionSource", {}).get("oneOf")
    if not isinstance(version_branches, list) or len(version_branches) != 3:
        raise ValueError("source manifest VERSION inventory is incomplete")
    actual_versions = []
    for branch in version_branches:
        branch_properties = branch.get("properties", {})
        actual_versions.append((
            branch_properties.get("version", {}).get("const"),
            branch_properties.get("sha256", {}).get("const"),
        ))
    expected_versions = [
        (version, QUALIFIED_SOURCE_HASHES[version][Path("VERSION")])
        for version in ("0.55.0", "0.56.0", "0.56.1")
    ]
    if actual_versions != expected_versions:
        raise ValueError("source manifest VERSION pins are stale")


def _assert_snapshot_number_contract(
    config_schema: dict[str, Any],
    generation_schema: dict[str, Any],
) -> None:
    for name, schema in (
        ("config", config_schema),
        ("generation manifest", generation_schema),
    ):
        definitions = schema.get("$defs", {})
        if definitions.get("revision", {}).get("pattern") != UINT64_DECIMAL_PATTERN:
            raise ValueError(f"{name} revision is not canonical uint64 decimal")
        if definitions.get("hyprlandVersion", {}).get("pattern") != HYPRLAND_TARGET_PATTERN:
            raise ValueError(f"{name} target is not bounded canonical 0.56.x")

    revision = re.compile(UINT64_DECIMAL_PATTERN)
    if not all(revision.fullmatch(value) for value in (
        "0", "1", "9999999999999999999", "18446744073709551615",
    )):
        raise ValueError("uint64 revision grammar rejects a boundary")
    if any(revision.fullmatch(value) for value in (
        "", "00", "01", "18446744073709551616", "99999999999999999999",
    )):
        raise ValueError("uint64 revision grammar accepts a noncanonical value")

    target = re.compile(HYPRLAND_TARGET_PATTERN)
    if not all(target.fullmatch(value) for value in (
        "0.56.0", "0.56.1", "0.56.4294967295",
    )):
        raise ValueError("Hyprland target grammar rejects a supported boundary")
    if any(target.fullmatch(value) for value in (
        "0.56", "0.55.1", "0.56.01", "0.56.4294967296",
    )):
        raise ValueError("Hyprland target grammar accepts an unsupported value")

    expected_range = {
        "type": "object",
        "additionalProperties": False,
        "required": ["major", "minor", "reviewedVersion", "minimumPatch", "maximumPatch"],
        "properties": {
            "major": {"const": 0},
            "minor": {"const": 56},
            "reviewedVersion": {"const": "0.56.1"},
            "minimumPatch": {"const": 0},
            "maximumPatch": {"const": None},
        },
    }
    if generation_schema.get("$defs", {}).get("hyprlandRange") != expected_range:
        raise ValueError("generation manifest compatibility range is not pinned")


def _source_document(version: str, registry_bytes: bytes, options: list[RawOption]) -> dict[str, Any]:
    source = QUALIFIED_SOURCES[version]
    return {
        "formatVersion": 1,
        "hyprlandVersion": version,
        "tag": source["tag"],
        "commit": source["commit"],
        "source": {
            "path": REGISTRY_PATH.as_posix(),
            "sha256": _sha256(registry_bytes),
        },
        "optionCount": len(options),
        "options": [inventory_record(option) for option in options],
    }


def _delta_document(old: dict[str, Any], new: dict[str, Any]) -> dict[str, Any]:
    old_by_path = {item["path"]: item for item in old["options"]}
    new_by_path = {item["path"]: item for item in new["options"]}
    added = sorted(set(new_by_path) - set(old_by_path))
    removed = sorted(set(old_by_path) - set(new_by_path))
    changed: list[dict[str, Any]] = []
    compared_fields = ("sourceType", "type", "default", "constraints", "description")
    for path in sorted(set(old_by_path) & set(new_by_path)):
        changes: dict[str, Any] = {}
        for field in compared_fields:
            before = old_by_path[path][field]
            after = new_by_path[path][field]
            if before != after:
                changes[field] = {"from": before, "to": after}
        if changes:
            changed.append({"path": path, "changes": changes})
    return {
        "formatVersion": 1,
        "from": "0.55.0",
        "to": "0.56.1",
        "added": added,
        "removed": removed,
        "changed": changed,
    }


def _complex_surfaces() -> list[dict[str, Any]]:
    emission = {
        "monitors": ("monitors", ["monitor"]),
        "devices": ("input", ["device"]),
        "curves": ("animations", ["curve"]),
        "animations": ("animations", ["animation"]),
        "gestures": ("input", ["gesture"]),
        "workspaceRules": ("workspaces", ["workspace_rule"]),
        "windowRules": ("rules", ["window_rule"]),
        "layerRules": ("rules", ["layer_rule"]),
        "submaps": ("bindings", ["define_submap"]),
        "bindings": ("bindings", ["bind"]),
        "permissions": ("permissions", ["permission"]),
        "environment": ("environment", ["env"]),
    }
    specifications = [
        (
            "monitors",
            "monitor",
            True,
            "id",
            "reload",
            "caution",
            "Connected-output selection, mode, position, scale, transform, mirroring, VRR, and HDR.",
            "monitor",
            f"{WIKI_ROOT}/Configuring/Basics/Monitors/",
        ),
        (
            "devices",
            "device",
            True,
            "id",
            "reload",
            "caution",
            "Per-device input overrides selected by the stable Hyprland device name.",
            "device",
            f"{WIKI_ROOT}/Configuring/Basics/Variables/",
        ),
        (
            "curves",
            "curve",
            True,
            "id",
            "reload",
            "safe",
            "Named typed Bezier and spring curves declared before animation records.",
            "curve",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Animations/",
        ),
        (
            "animations",
            "animation",
            True,
            "id",
            "reload",
            "safe",
            "Ordered animation overrides using a declared curve and optional style.",
            "animation",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Animations/",
        ),
        (
            "gestures",
            "gesture",
            True,
            "id",
            "reload",
            "safe",
            "Declarative trackpad gesture actions; live Lua callbacks are outside managed state.",
            "gesture",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Gestures/",
        ),
        (
            "workspaceRules",
            "workspaceRule",
            True,
            "id",
            "reload",
            "caution",
            "Ordered workspace selectors, monitor assignment, persistence, layout, and typed effects.",
            "workspaceRule",
            f"{WIKI_ROOT}/Configuring/Basics/Workspace-Rules/",
        ),
        (
            "windowRules",
            "windowRule",
            True,
            "id",
            "reload",
            "caution",
            "Named ordered window matchers and effects. Arbitrary Lua and shell effects are excluded.",
            "windowRule",
            f"{WIKI_ROOT}/Configuring/Basics/Window-Rules/",
        ),
        (
            "layerRules",
            "layerRule",
            True,
            "id",
            "reload",
            "caution",
            "Named ordered layer-surface matchers and effects.",
            "layerRule",
            f"{WIKI_ROOT}/Configuring/Basics/Window-Rules/",
        ),
        (
            "submaps",
            "submap",
            True,
            "id",
            "reload",
            "caution",
            "Named binding submaps with an explicit reset target; bindings reference the submap name.",
            "submap",
            f"{WIKI_ROOT}/Configuring/Basics/Binds/",
        ),
        (
            "bindings",
            "binding",
            True,
            "id",
            "reload",
            "caution",
            "Normalized key chords mapped to closed semantic actions; duplicate chords are rejected.",
            "binding",
            f"{WIKI_ROOT}/Configuring/Basics/Binds/",
        ),
        (
            "permissions",
            "permission",
            True,
            "id",
            "restart",
            "dangerous",
            "Ordered dynamic-permission rules using only types and modes accepted by Hyprland 0.56.1.",
            "permission",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Permissions/",
        ),
        (
            "environment",
            "environmentVariable",
            True,
            "id",
            "session",
            "caution",
            "Environment variables routed to native Hyprland or UWSM ownership as appropriate.",
            "environmentVariable",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Environment-variables/",
        ),
    ]
    surfaces = [
        {
            "id": surface_id,
            "kind": kind,
            "module": emission[surface_id][0],
            "luaPath": emission[surface_id][1],
            "ordered": ordered,
            "identityField": identity,
            "applyMode": mode,
            "risk": surface_risk,
            "description": description,
            "schemaRef": f"config.schema.json#/$defs/{schema_name}",
            "documentation": docs,
        }
        for (
            surface_id,
            kind,
            ordered,
            identity,
            mode,
            surface_risk,
            description,
            schema_name,
            docs,
        ) in specifications
    ]
    return sorted(surfaces, key=lambda item: item["id"])


def _action_label(action: str, strip_namespace: bool = False) -> str:
    value = action.split(".", 1)[1] if strip_namespace and "." in action else action
    words: list[str] = []
    for segment in re.split(r"[._]", value):
        words.extend(re.findall(r"[A-Z]+(?=[A-Z][a-z]|$)|[A-Z]?[a-z]+|[0-9]+", segment))
    acronyms = {"dpms": "DPMS", "pdf": "PDF", "rgbx": "RGBX", "hdr": "HDR", "id": "ID"}
    return " ".join(acronyms.get(word.lower(), word.capitalize()) for word in words)


def _action_catalog(dispatcher_source: bytes, config_schema: bytes) -> dict[str, Any]:
    binds_doc = f"{WIKI_ROOT}/Configuring/Basics/Binds/"
    gestures_doc = f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Gestures/"

    if len(DISPATCHER_ARGUMENT_SCHEMAS) != 47:
        raise ValueError("reviewed dispatcher inventory must contain exactly 47 managed actions")
    if set(DISPATCHER_DESCRIPTIONS) != set(DISPATCHER_ARGUMENT_SCHEMAS):
        raise ValueError("dispatcher presentation copy does not match the closed inventory")
    if set(DEFAULT_APP_DESCRIPTIONS) != set(DEFAULT_APP_ACTIONS):
        raise ValueError("default-app presentation copy does not match the closed inventory")
    if set(HYPRSHELLD_DESCRIPTIONS) != set(HYPRSHELLD_ACTIONS):
        raise ValueError("HyprShelld presentation copy does not match the closed inventory")
    if set(GESTURE_DESCRIPTIONS) != {action for action, _ in GESTURE_ACTIONS}:
        raise ValueError("gesture presentation copy does not match the closed inventory")
    if set(DISPATCHER_INVOCATIONS) != set(DISPATCHER_ARGUMENT_SCHEMAS):
        raise ValueError("dispatcher invocation descriptors do not match the closed inventory")
    if set(GESTURE_INVOCATION_PARAMETERS) != {
        action for action, _ in GESTURE_ACTIONS
    }:
        raise ValueError("gesture invocation descriptors do not match the closed inventory")
    registered = extract_dispatcher_ids(dispatcher_source.decode("utf-8"))
    expected_registered = set(DISPATCHER_ARGUMENT_SCHEMAS) | {
        "exec_cmd", "exec_raw", "layout", "window.set_prop",
    }
    if registered != expected_registered:
        raise ValueError(
            "tagged dispatcher inventory changed; review additions/removals before updating: "
            f"added={sorted(registered - expected_registered)}, "
            f"removed={sorted(expected_registered - registered)}"
        )

    dispatcher_actions = []
    for action, arguments_schema in sorted(DISPATCHER_ARGUMENT_SCHEMAS.items()):
        dispatcher_actions.append({
            "id": action,
            "label": _action_label(action),
            "description": DISPATCHER_DESCRIPTIONS[action],
            "luaPath": ["dsp", *action.split(".")],
            "invocation": DISPATCHER_INVOCATIONS[action],
            "uiTier": "expert" if action in EXPERT_DISPATCHERS else "advanced",
            "risk": (
                "dangerous" if action in DANGEROUS_DISPATCHERS
                else "caution" if action in CAUTION_DISPATCHERS
                else "safe"
            ),
            "argumentsSchemaRef": f"config.schema.json#/$defs/{arguments_schema}",
            "documentation": binds_doc,
        })

    semantic_actions = [
        {
            "id": action,
            "label": _action_label(action, strip_namespace=True),
            "description": (
                DEFAULT_APP_DESCRIPTIONS[action]
                if action_type == "defaultApp"
                else HYPRSHELLD_DESCRIPTIONS[action]
            ),
            "actionType": action_type,
            "invocation": {"kind": "broker", "namespace": action_type},
            "uiTier": "common",
            "risk": "safe",
            "argumentsSchemaRef": "config.schema.json#/$defs/emptyArguments",
            "documentation": binds_doc,
        }
        for action_type, actions in (
            ("defaultApp", DEFAULT_APP_ACTIONS),
            ("hyprshelld", HYPRSHELLD_ACTIONS),
        )
        for action in actions
    ]

    gesture_actions = [
        {
            "id": action,
            "label": _action_label(action),
            "description": GESTURE_DESCRIPTIONS[action],
            "luaAction": lua_action,
            "invocation": {
                "kind": "gesture-table",
                "actionField": "action",
                "parameters": [
                    {"argument": argument, "field": field}
                    for argument, field in GESTURE_INVOCATION_PARAMETERS[action]
                ],
            },
            "uiTier": "common",
            "risk": "safe",
            "actionSchemaRef": "config.schema.json#/$defs/gestureAction",
            "documentation": gestures_doc,
        }
        for action, lua_action in GESTURE_ACTIONS
    ]

    return {
        "contractVersion": 1,
        "hyprland": {
            "reviewedVersion": "0.56.1",
            "reviewedTag": QUALIFIED_SOURCES["0.56.1"]["tag"],
            "reviewedCommit": QUALIFIED_SOURCES["0.56.1"]["commit"],
        },
        "configSchemaDigest": _sha256(config_schema),
        "source": {
            "repository": REPOSITORY,
            "tag": QUALIFIED_SOURCES["0.56.1"]["tag"],
            "commit": QUALIFIED_SOURCES["0.56.1"]["commit"],
            "path": "src/config/lua/bindings/LuaBindingsDispatchers.cpp",
            "sha256": _sha256(dispatcher_source),
        },
        "dispatcherActions": dispatcher_actions,
        "semanticActions": sorted(semantic_actions, key=lambda item: item["id"]),
        "gestureActions": sorted(gesture_actions, key=lambda item: item["id"]),
        "excluded": sorted([
            {
                "id": "exec_cmd",
                "surface": "dispatcher",
                "reason": "Arbitrary process commands are outside managed desired state.",
            },
            {
                "id": "exec_raw",
                "surface": "dispatcher",
                "reason": "Raw process commands are outside managed desired state.",
            },
            {
                "id": "callback",
                "surface": "gesture",
                "reason": "Live Lua callbacks cannot be represented as declarative state.",
            },
            {
                "id": "on_created_empty",
                "surface": "workspaceRule",
                "reason": "Arbitrary workspace creation commands belong in user-custom.lua.",
            },
            {
                "id": "mouse",
                "surface": "bindingOption",
                "reason": "The tagged Lua parser documents this option but never assigns it; mouse buttons are represented by key symbols.",
            },
            {
                "id": "switch:*",
                "surface": "bindingKey",
                "reason": "Tagged switch selectors are not representable as one stable, portable managed key token in v1.",
            },
            {
                "id": "window.set_prop",
                "surface": "dispatcher",
                "reason": "untyped-prop-value: tagged property values use a property-specific mini-language that is not part of managed v1.",
            },
            {
                "id": "layout",
                "surface": "dispatcher",
                "reason": "layout-dependent-message: tagged layout messages use active-layout and plugin-specific mini-languages outside managed v1.",
            },
            {
                "id": "group",
                "surface": "windowRuleEffect",
                "reason": "contextual-group-command: tagged group effects use an order- and window-context-dependent mini-language outside managed v1.",
            },
        ], key=lambda item: (item["surface"], item["id"])),
    }


def build_documents(
    source_055: Path,
    source_0560: Path,
    source_056: Path,
    output_root: Path = Path.cwd(),
) -> dict[Path, bytes]:
    _validate_qualified_source_hashes()
    try:
        _strict_json(b'{"value":NaN}', "strict JSON self-test")
    except ValueError:
        pass
    else:
        raise ValueError("strict JSON self-test accepted NaN")

    schema_directory = output_root / "interfaces/hyprland/v1"
    schema_documents: dict[str, Any] = {}
    for schema_name in (
        "catalog.schema.json",
        "config.schema.json",
        "action-catalog.schema.json",
        "generation-manifest.schema.json",
        "source-manifest.schema.json",
    ):
        schema_path = schema_directory / schema_name
        schema_documents[schema_name] = _strict_json(
            schema_path.read_bytes(),
            schema_path.as_posix(),
        )
    _assert_bounded_action_numbers(schema_documents["config.schema.json"])
    _assert_source_manifest_schema(schema_documents["source-manifest.schema.json"])
    _assert_snapshot_number_contract(
        schema_documents["config.schema.json"],
        schema_documents["generation-manifest.schema.json"],
    )

    source_roots = {
        "0.55.0": source_055,
        "0.56.0": source_0560,
        "0.56.1": source_056,
    }
    version_file_bytes: dict[str, bytes] = {}
    for version, source_root in source_roots.items():
        version_bytes = _read_qualified_source(
            source_root,
            version,
            Path("VERSION"),
        )
        actual_version = version_bytes.decode("utf-8").strip()
        if actual_version != version:
            raise ValueError(
                f"expected Hyprland {version} at {source_root}, found {actual_version!r}"
            )
        version_file_bytes[version] = version_bytes

    sources: dict[str, tuple[bytes, list[RawOption]]] = {}
    for version in ("0.55.0", "0.56.1"):
        source_root = source_roots[version]
        registry_bytes = _read_qualified_source(
            source_root,
            version,
            REGISTRY_PATH,
        )
        options = extract_raw_options(registry_bytes.decode("utf-8"))
        expected_count = int(QUALIFIED_SOURCES[version]["count"])
        if len(options) != expected_count:
            raise ValueError(
                f"Hyprland {version} registry has {len(options)} options; "
                f"review expected count {expected_count} before updating the contract"
            )
        sources[version] = (registry_bytes, options)

    complex_source_bytes = {
        path: _read_qualified_source(source_056, "0.56.1", path)
        for path in COMPLEX_SOURCE_PATHS
    }
    startup_source_bytes_0560 = {
        path: _read_qualified_source(source_0560, "0.56.0", path)
        for path in STARTUP_SOURCE_PATHS_0560
    }
    _assert_reload_event_runtime_order(
        startup_source_bytes_0560,
        complex_source_bytes,
    )
    modifier_source = complex_source_bytes[
        Path("src/config/lua/bindings/LuaBindingsToplevel.cpp")
    ].decode("utf-8")
    modifier_aliases = extract_modifier_aliases(modifier_source)
    if modifier_aliases != TAGGED_MODIFIER_ALIASES:
        raise ValueError(
            "tagged Lua modifier inventory changed; review aliases and canonical "
            f"persisted names before updating: {modifier_aliases!r}"
        )
    schema_modifiers = (
        schema_documents["config.schema.json"]
        .get("$defs", {})
        .get("modifiers", {})
        .get("items", {})
        .get("enum")
    )
    if schema_modifiers != list(CANONICAL_MODIFIERS):
        raise ValueError(
            "config schema modifier inventory does not match the reviewed tagged "
            f"Lua aliases: expected {list(CANONICAL_MODIFIERS)!r}, "
            f"found {schema_modifiers!r}"
        )
    fullscreen_source = complex_source_bytes[
        Path("src/managers/fullscreen/FullscreenController.hpp")
    ].decode("utf-8")
    fullscreen_modes = extract_fullscreen_modes(fullscreen_source)
    if fullscreen_modes != TAGGED_FULLSCREEN_MODES:
        raise ValueError(
            "tagged fullscreen mode inventory changed; review persisted matcher "
            f"and dispatcher bounds before updating: {fullscreen_modes!r}"
        )
    fullscreen_schema = (
        schema_documents["config.schema.json"]
        .get("$defs", {})
        .get("fullscreenMode")
    )
    expected_fullscreen_schema = {
        "type": "integer",
        "minimum": min(TAGGED_FULLSCREEN_MODES.values()),
        "maximum": max(TAGGED_FULLSCREEN_MODES.values()),
    }
    if fullscreen_schema != expected_fullscreen_schema:
        raise ValueError(
            "config schema fullscreen mode definition does not match the tagged "
            f"enum: expected {expected_fullscreen_schema!r}, "
            f"found {fullscreen_schema!r}"
        )
    _assert_gesture_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_inherited_default_contract(
        sources["0.56.1"][1],
        complex_source_bytes,
    )
    _assert_animation_style_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_monitor_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_dispatcher_action_bounds(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_action_invocation_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_key_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_window_selector_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_workspace_selector_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_window_effect_contract(
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )
    _assert_numeric_safety_contract(
        sources["0.56.1"][1],
        complex_source_bytes,
        schema_documents["config.schema.json"],
    )

    fixture_055 = _source_document("0.55.0", *sources["0.55.0"])
    fixture_056 = _source_document("0.56.1", *sources["0.56.1"])
    delta = _delta_document(fixture_055, fixture_056)
    if len(delta["added"]) != 12 or delta["removed"]:
        raise ValueError(
            "reviewed 0.55.0 -> 0.56.1 boundary changed: expected 12 additions and no removals"
        )

    added_in_056 = set(delta["added"])
    options_056 = sources["0.56.1"][1]
    catalog = {
        "contractVersion": 1,
        "hyprland": {
            "major": 0,
            "minor": 56,
            "reviewedVersion": "0.56.1",
            "reviewedTag": QUALIFIED_SOURCES["0.56.1"]["tag"],
            "reviewedCommit": QUALIFIED_SOURCES["0.56.1"]["commit"],
            "repository": REPOSITORY,
            "minimumPatch": 0,
            "maximumPatch": None,
        },
        "options": [
            catalog_record(option, added_in_056)
            for option in sorted(options_056, key=lambda item: item.path)
        ],
        "complexSurfaces": _complex_surfaces(),
        "compatibility": {
            "minimumSupported": "0.55.0",
            "fullyQualified": ["0.56.x"],
            "olderMinor": "migration",
            "newerMinor": "read-only",
            "unknownMajor": "unsupported",
        },
    }
    non_writable = [
        option["path"] for option in catalog["options"] if not option["writable"]
    ]
    if non_writable != [
        "input:scroll_points",
        "input:tablet:output",
        "input:touchdevice:output",
        "scrolling:explicit_column_widths",
    ]:
        raise ValueError(f"reviewed non-writable scalar inventory changed: {non_writable}")
    accel_profile = next(
        option for option in catalog["options"] if option["path"] == "input:accel_profile"
    )
    if any(
        choice["value"] == "custom"
        for choice in accel_profile["constraints"].get("choices", [])
    ):
        raise ValueError("scalar accel profile exposes an untyped custom curve")
    catalog_bytes = _json_bytes(catalog)
    dispatcher_source = complex_source_bytes[DISPATCHER_SOURCE_PATH]
    config_schema = (
        output_root / "interfaces/hyprland/v1/config.schema.json"
    ).read_bytes()
    action_catalog = _action_catalog(dispatcher_source, config_schema)
    action_catalog_digest = _sha256(
        _canonical_json_bytes(action_catalog) + b"\n" + config_schema
    )

    defaults = {
        "formatVersion": 1,
        "revision": "0",
        "targetHyprland": "0.56.1",
        "catalogDigest": _sha256(_canonical_json_bytes(catalog)),
        "actionCatalogDigest": action_catalog_digest,
        "overrides": {},
        "monitors": [],
        "devices": [],
        "curves": [],
        "animations": [],
        "gestures": [],
        "workspaceRules": [],
        "windowRules": [],
        "layerRules": [],
        "submaps": [],
        "bindings": [],
        "permissions": [],
        "environment": [],
    }

    source_manifest = {
        "formatVersion": 1,
        "repository": REPOSITORY,
        "reviewedOn": REVIEWED_ON,
        "versionFiles": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": "VERSION",
                "sha256": _sha256(version_file_bytes[version]),
            }
            for version in ("0.55.0", "0.56.0", "0.56.1")
        ],
        "sources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": REGISTRY_PATH.as_posix(),
                "sha256": _sha256(sources[version][0]),
                "optionCount": len(sources[version][1]),
            }
            for version in ("0.55.0", "0.56.1")
        ],
        "complexSources": [
            {
                "version": "0.56.1",
                "tag": QUALIFIED_SOURCES["0.56.1"]["tag"],
                "commit": QUALIFIED_SOURCES["0.56.1"]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(complex_source_bytes[path]),
            }
            for path in sorted(COMPLEX_SOURCE_PATHS)
        ],
        "startupSources": [
            {
                "version": "0.56.0",
                "tag": QUALIFIED_SOURCES["0.56.0"]["tag"],
                "commit": QUALIFIED_SOURCES["0.56.0"]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(startup_source_bytes_0560[path]),
            }
            for path in sorted(STARTUP_SOURCE_PATHS_0560)
        ],
        "documentation": [
            f"{WIKI_ROOT}/Configuring/Start/",
            f"{WIKI_ROOT}/Configuring/Basics/Variables/",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Using-hyprctl/",
            f"{WIKI_ROOT}/Configuring/Basics/Monitors/",
            f"{WIKI_ROOT}/Configuring/Basics/Binds/",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Animations/",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Gestures/",
            f"{WIKI_ROOT}/Configuring/Basics/Workspace-Rules/",
            f"{WIKI_ROOT}/Configuring/Basics/Window-Rules/",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Permissions/",
            f"{WIKI_ROOT}/Configuring/Advanced-and-Cool/Environment-variables/",
        ],
    }

    # A deterministic, non-activatable fixture exercises the installed
    # generation-manifest schema.  Its single file is the empty byte string;
    # production generations replace every fixture-only identity field.
    generation_payload = {
        "formatVersion": 1,
        "contractVersion": 1,
        "snapshotDigest": _sha256(_canonical_json_bytes(defaults)),
        "catalogDigest": defaults["catalogDigest"],
        "actionCatalogDigest": defaults["actionCatalogDigest"],
        "revision": defaults["revision"],
        "targetHyprland": defaults["targetHyprland"],
        "compatibleHyprland": {
            "major": 0,
            "minor": 56,
            "reviewedVersion": "0.56.1",
            "minimumPatch": 0,
            "maximumPatch": None,
        },
        "rendererVersion": 1,
        "activationNonce": "0123456789abcdef0123456789abcdef",
        "createdAt": "2026-08-09T00:00:00Z",
        "entrypoint": "hyprland.lua",
        "files": {
            "hyprland.lua": {
                "sha256": _sha256(b""),
                "size": 0,
            }
        },
    }
    generation_manifest_fixture = {
        "formatVersion": generation_payload["formatVersion"],
        "contractVersion": generation_payload["contractVersion"],
        "generation": _sha256(_canonical_json_bytes(generation_payload)),
        **{
            key: value
            for key, value in generation_payload.items()
            if key not in ("formatVersion", "contractVersion")
        },
    }

    def assert_versioned_wiki_urls(value: Any) -> None:
        if isinstance(value, dict):
            for child in value.values():
                assert_versioned_wiki_urls(child)
        elif isinstance(value, list):
            for child in value:
                assert_versioned_wiki_urls(child)
        elif isinstance(value, str) and value.startswith("https://wiki.hypr.land/"):
            if not value.startswith(f"{WIKI_ROOT}/"):
                raise ValueError(f"unversioned or wrong-version Hyprland documentation URL: {value}")

    assert_versioned_wiki_urls(catalog)
    assert_versioned_wiki_urls(action_catalog)
    assert_versioned_wiki_urls(source_manifest)

    return {
        Path("data/hyprland/config-catalog-v1.json"): catalog_bytes,
        Path("data/hyprland/action-catalog-v1.json"): _json_bytes(action_catalog),
        Path("data/defaults/hyprland.json"): _json_bytes(defaults),
        Path("tests/fixtures/hyprland/v0.55.0.scalar-options.json"): _json_bytes(fixture_055),
        Path("tests/fixtures/hyprland/v0.56.1.scalar-options.json"): _json_bytes(fixture_056),
        Path("tests/fixtures/hyprland/v0.55.0-to-v0.56.1.delta.json"): _json_bytes(delta),
        Path("tests/fixtures/hyprland/source-manifest.json"): _json_bytes(source_manifest),
        Path("tests/fixtures/hyprland/generation-manifest.json"): _json_bytes(
            generation_manifest_fixture
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-055", type=Path, required=True)
    parser.add_argument("--source-0560", type=Path, required=True)
    parser.add_argument("--source-056", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, default=Path.cwd())
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare generated bytes with checked-in files instead of writing",
    )
    arguments = parser.parse_args()

    try:
        documents = build_documents(
            arguments.source_055,
            arguments.source_0560,
            arguments.source_056,
            arguments.output_root,
        )
    except (OSError, UnicodeError, ValueError) as error:
        print(f"extract_contract.py: {error}", file=sys.stderr)
        return 2

    stale: list[Path] = []
    for relative_path, expected in documents.items():
        destination = arguments.output_root / relative_path
        if arguments.check:
            try:
                actual = destination.read_bytes()
            except OSError:
                stale.append(relative_path)
                continue
            if actual != expected:
                stale.append(relative_path)
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(expected)

    if stale:
        for path in stale:
            print(f"stale generated contract file: {path}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
