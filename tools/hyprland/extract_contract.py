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
import subprocess
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
MONITOR_QUERY_SOURCE_PATHS = (
    Path("src/debug/HyprCtl.cpp"),
    Path("src/output/Monitor.cpp"),
)
MAXIMIZE_SOURCE_PATHS = (
    Path("src/desktop/Workspace.cpp"),
    Path("src/config/shared/workspace/WorkspaceRuleManager.cpp"),
    Path("src/config/shared/workspace/WorkspaceRule.cpp"),
    Path("src/layout/space/Space.cpp"),
    Path("src/managers/fullscreen/FullscreenController.hpp"),
    Path("src/managers/fullscreen/FullscreenController.cpp"),
    Path("src/managers/fullscreen/handler/FullscreenHandler.cpp"),
)
GROUP_BEHAVIOR_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        Path("src/desktop/view/Window.cpp"),
        Path("src/desktop/view/Group.cpp"),
        Path("src/layout/supplementary/DragController.cpp"),
        Path("src/render/decorations/CHyprGroupBarDecoration.cpp"),
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/Compositor.cpp"),
    ),
    "0.56.1": (
        Path("src/desktop/view/Window.cpp"),
        Path("src/desktop/view/Group.cpp"),
        Path("src/layout/supplementary/DragController.cpp"),
        Path("src/render/decorations/CHyprGroupBarDecoration.cpp"),
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/desktop/state/GlobalWindowController.cpp"),
    ),
}
APPEARANCE_BEHAVIOR_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        REGISTRY_PATH,
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/Compositor.cpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/render/Renderer.cpp"),
        Path("src/render/OpenGL.cpp"),
        Path("src/render/pass/Pass.cpp"),
        Path("src/render/ShaderLoader.hpp"),
        Path("src/render/Shader.cpp"),
        Path("src/render/GLRenderer.cpp"),
        Path("src/render/ElementRenderer.cpp"),
        Path("src/render/gl/GLElementRenderer.cpp"),
        Path("src/render/pass/PreBlurElement.hpp"),
        Path("src/render/pass/PreBlurElement.cpp"),
        Path("src/render/shaders/glsl/blurprepare.frag"),
        Path("src/render/shaders/glsl/blurprepare.glsl"),
        Path("src/render/shaders/glsl/blurfinish.frag"),
        Path("src/render/shaders/glsl/blurFinish.glsl"),
        Path("src/render/shaders/glsl/blur1.frag"),
        Path("src/render/shaders/glsl/blur1.glsl"),
        Path("src/render/shaders/glsl/gain.glsl"),
        Path("src/desktop/Workspace.cpp"),
        Path("src/render/decorations/CHyprBorderDecoration.cpp"),
        Path("src/render/decorations/DecorationPositioner.cpp"),
        Path("src/render/decorations/CHyprDropShadowDecoration.cpp"),
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.hpp"),
        Path("src/desktop/view/Window.hpp"),
        Path("src/render/Renderer.hpp"),
        Path("src/render/OpenGL.hpp"),
        Path("src/render/decorations/CHyprDropShadowDecoration.hpp"),
        Path("src/render/pass/BorderPassElement.hpp"),
        Path("src/render/pass/RectPassElement.hpp"),
        Path("src/render/pass/SurfacePassElement.hpp"),
        Path("src/render/pass/TexPassElement.hpp"),
        Path("src/render/shaders/glsl/border.frag"),
        Path("src/render/shaders/glsl/border.glsl"),
        Path("src/render/shaders/glsl/ext.frag"),
        Path("src/render/shaders/glsl/quad.frag"),
        Path("src/render/shaders/glsl/rounding.glsl"),
        Path("src/render/shaders/glsl/shadow.frag"),
        Path("src/render/shaders/glsl/shadow.glsl"),
        Path("src/render/shaders/glsl/surface.frag"),
        Path("src/render/decorations/CHyprInnerGlowDecoration.cpp"),
        Path("src/render/shaders/glsl/inner_glow.frag"),
        Path("src/render/shaders/glsl/inner_glow.glsl"),
        Path("src/protocols/OutputManagement.cpp"),
        Path("src/config/shared/monitor/MonitorRuleManager.cpp"),
        Path("src/helpers/Monitor.cpp"),
    ),
    "0.56.1": (
        REGISTRY_PATH,
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/desktop/state/GlobalWindowController.cpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/render/Renderer.cpp"),
        Path("src/output/Monitor.cpp"),
        Path("src/desktop/state/LayerFadeout.cpp"),
        Path("src/desktop/state/WindowFadeout.cpp"),
        Path("src/render/OpenGL.cpp"),
        Path("src/render/pass/Pass.cpp"),
        Path("src/desktop/state/PopupFadeout.cpp"),
        Path("src/render/ShaderLoader.hpp"),
        Path("src/render/Shader.cpp"),
        Path("src/render/GLRenderer.cpp"),
        Path("src/render/ElementRenderer.cpp"),
        Path("src/render/gl/GLElementRenderer.cpp"),
        Path("src/render/pass/PreBlurElement.hpp"),
        Path("src/render/pass/PreBlurElement.cpp"),
        Path("src/render/shaders/glsl/blurprepare.frag"),
        Path("src/render/shaders/glsl/blurprepare.glsl"),
        Path("src/render/shaders/glsl/blurfinish.frag"),
        Path("src/render/shaders/glsl/blurFinish.glsl"),
        Path("src/render/shaders/glsl/blur1.frag"),
        Path("src/render/shaders/glsl/blur1.glsl"),
        Path("src/render/shaders/glsl/gain.glsl"),
        Path("src/desktop/Workspace.cpp"),
        Path("src/render/decorations/CHyprBorderDecoration.cpp"),
        Path("src/render/decorations/DecorationPositioner.cpp"),
        Path("src/render/decorations/CHyprDropShadowDecoration.cpp"),
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.hpp"),
        Path("src/desktop/view/Window.hpp"),
        Path("src/render/Renderer.hpp"),
        Path("src/render/OpenGL.hpp"),
        Path("src/render/decorations/CHyprDropShadowDecoration.hpp"),
        Path("src/render/pass/BorderPassElement.hpp"),
        Path("src/render/pass/RectPassElement.hpp"),
        Path("src/render/pass/SurfacePassElement.hpp"),
        Path("src/render/pass/TexPassElement.hpp"),
        Path("src/render/shaders/glsl/border.frag"),
        Path("src/render/shaders/glsl/border.glsl"),
        Path("src/render/shaders/glsl/ext.frag"),
        Path("src/render/shaders/glsl/quad.frag"),
        Path("src/render/shaders/glsl/rounding.glsl"),
        Path("src/render/shaders/glsl/shadow.frag"),
        Path("src/render/shaders/glsl/shadow.glsl"),
        Path("src/render/shaders/glsl/surface.frag"),
        Path("src/render/decorations/CHyprInnerGlowDecoration.cpp"),
        Path("src/render/shaders/glsl/inner_glow.frag"),
        Path("src/render/shaders/glsl/inner_glow.glsl"),
        Path("src/protocols/OutputManagement.cpp"),
        Path("src/config/shared/monitor/MonitorRuleManager.cpp"),
    ),
}
ADVANCED_RUNTIME_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        REGISTRY_PATH,
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/managers/SessionLockManager.cpp"),
        Path("src/render/Renderer.cpp"),
        Path("src/render/ElementRenderer.cpp"),
        Path("src/render/pass/TexPassElement.hpp"),
        Path("src/render/pass/TexPassElement.cpp"),
        Path("src/render/gl/GLElementRenderer.cpp"),
        Path("src/render/OpenGL.cpp"),
        Path("src/render/pass/SurfacePassElement.hpp"),
        Path("src/render/pass/SurfacePassElement.cpp"),
        Path("src/render/shaders/glsl/surface.frag"),
        Path("src/helpers/Monitor.cpp"),
        Path("src/managers/screenshare/ScreenshareSession.cpp"),
        Path("src/helpers/cm/ColorManagement.hpp"),
        Path("src/render/Framebuffer.cpp"),
        Path("src/helpers/MonitorResources.cpp"),
    ),
    "0.56.0": (
        REGISTRY_PATH,
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/protocols/InputCapture.cpp"),
    ),
    "0.56.1": (
        REGISTRY_PATH,
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/protocols/InputCapture.cpp"),
        Path("src/managers/SessionLockManager.cpp"),
        Path("src/render/Renderer.cpp"),
        Path("src/render/ElementRenderer.cpp"),
        Path("src/render/pass/TexPassElement.hpp"),
        Path("src/render/pass/TexPassElement.cpp"),
        Path("src/render/gl/GLElementRenderer.cpp"),
        Path("src/render/OpenGL.cpp"),
        Path("src/render/pass/SurfacePassElement.hpp"),
        Path("src/render/pass/SurfacePassElement.cpp"),
        Path("src/render/shaders/glsl/surface.frag"),
        Path("src/output/Monitor.cpp"),
        Path("src/managers/screenshare/ScreenshareSession.cpp"),
        Path("src/helpers/cm/ColorManagement.hpp"),
        Path("src/render/Framebuffer.cpp"),
        Path("src/output/MonitorResources.cpp"),
    ),
}
WINDOW_BEHAVIOR_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        REGISTRY_PATH,
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/Compositor.cpp"),
        Path("src/managers/ANRManager.cpp"),
        Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
        Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"),
        Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"),
        Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/desktop/state/FocusState.cpp"),
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"),
        Path("src/layout/target/WindowTarget.cpp"),
    ),
    "0.56.1": (
        REGISTRY_PATH,
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/desktop/state/WindowQuery.cpp"),
        Path("src/managers/fullscreen/FullscreenController.cpp"),
        Path("src/managers/ANRManager.cpp"),
        Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
        Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"),
        Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"),
        Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/desktop/state/FocusState.cpp"),
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"),
        Path("src/layout/target/WindowTarget.cpp"),
    ),
}
GROUP_BAR_SOURCE_PATHS = (
    REGISTRY_PATH,
    Path("src/desktop/view/Group.cpp"),
    Path("src/render/decorations/CHyprGroupBarDecoration.cpp"),
    Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
    Path("src/config/lua/types/LuaConfigFontWeight.cpp"),
)
WORKSPACE_BEHAVIOR_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        REGISTRY_PATH,
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/desktop/history/WorkspaceHistoryTracker.cpp"),
        Path("src/desktop/history/WorkspaceHistoryTracker.hpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/Compositor.cpp"),
    ),
    "0.56.1": (
        REGISTRY_PATH,
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/desktop/history/WorkspaceHistoryTracker.cpp"),
        Path("src/desktop/history/WorkspaceHistoryTracker.hpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/state/WorkspacePlacementController.cpp"),
        Path("src/pointer/PointerController.cpp"),
    ),
}
BINDING_RUNTIME_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    version: (
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/lua/bindings/LuaBindingsRegistration.cpp"),
        Path("src/config/lua/bindings/LuaBindingsToplevel.cpp"),
        Path("src/config/shared/actions/ConfigActions.hpp"),
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/managers/KeybindManager.cpp"),
        Path("src/debug/HyprCtl.cpp"),
    )
    for version in ("0.55.0", "0.56.1")
}
MISC_EXCLUSION_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    version: (
        REGISTRY_PATH,
        Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
        Path("src/layout/target/WindowTarget.cpp"),
    )
    for version in ("0.55.0", "0.56.1")
}
MISC_EXCLUSION_OPTION_PATHS = (
    "misc:animate_manual_resizes",
    "misc:animate_mouse_windowdragging",
    "misc:layers_hog_keyboard_focus",
)
MISC_EXCLUSION_EXPECTED_OCCURRENCES = {
    version: {
        "misc:animate_manual_resizes": {
            REGISTRY_PATH: 1,
            Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"): 1,
        },
        "misc:animate_mouse_windowdragging": {REGISTRY_PATH: 1},
        "misc:layers_hog_keyboard_focus": {REGISTRY_PATH: 1},
    }
    for version in MISC_EXCLUSION_SOURCE_PATHS
}
INPUT_BEHAVIOR_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        REGISTRY_PATH,
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/managers/input/InputManager.hpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/managers/input/Tablets.cpp"),
        Path("src/protocols/PrimarySelection.cpp"),
        Path("src/render/Renderer.cpp"),
        Path("src/render/Renderer.hpp"),
        Path("src/managers/input/Touch.cpp"),
        Path("src/devices/Mouse.cpp"),
        Path("src/managers/PointerManager.hpp"),
        Path("src/managers/PointerManager.cpp"),
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/desktop/view/Window.hpp"),
        Path("src/Compositor.cpp"),
        Path("src/Compositor.hpp"),
    ),
    "0.56.1": (
        REGISTRY_PATH,
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/managers/input/InputManager.hpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/managers/input/Tablets.cpp"),
        Path("src/protocols/PrimarySelection.cpp"),
        Path("src/render/Renderer.cpp"),
        Path("src/render/Renderer.hpp"),
        Path("src/managers/input/Touch.cpp"),
        Path("src/devices/Mouse.cpp"),
        Path("src/pointer/PointerManager.hpp"),
        Path("src/pointer/PointerManager.cpp"),
        Path("src/config/shared/actions/ConfigActions.cpp"),
        Path("src/desktop/view/Window.cpp"),
        Path("src/desktop/view/Window.hpp"),
        Path("src/pointer/PointerController.cpp"),
        Path("src/pointer/PointerController.hpp"),
    ),
}
INPUT_DEVICE_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        Path("src/debug/HyprCtl.cpp"),
        Path("src/helpers/MiscFunctions.cpp"),
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/managers/input/InputManager.hpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/managers/input/Tablets.cpp"),
        Path("src/devices/IHID.cpp"),
        Path("src/devices/IPointer.cpp"),
        Path("src/devices/IKeyboard.cpp"),
        Path("src/devices/VirtualKeyboard.cpp"),
        Path("src/devices/VirtualPointer.cpp"),
        Path("src/protocols/VirtualKeyboard.cpp"),
        Path("src/protocols/VirtualPointer.cpp"),
        Path("src/Compositor.cpp"),
        Path("src/managers/PointerManager.cpp"),
    ),
    "0.56.1": (
        Path("src/debug/HyprCtl.cpp"),
        Path("src/helpers/MiscFunctions.cpp"),
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"),
        Path("src/managers/input/InputManager.hpp"),
        Path("src/managers/input/InputManager.cpp"),
        Path("src/managers/input/Tablets.cpp"),
        Path("src/devices/IHID.cpp"),
        Path("src/devices/IPointer.cpp"),
        Path("src/devices/IKeyboard.cpp"),
        Path("src/devices/VirtualKeyboard.cpp"),
        Path("src/devices/VirtualPointer.cpp"),
        Path("src/protocols/VirtualKeyboard.cpp"),
        Path("src/protocols/VirtualPointer.cpp"),
        Path("src/Compositor.cpp"),
        Path("src/pointer/PointerManager.cpp"),
    ),
}
GESTURE_SOURCE_PATHS = (
    Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
    Path("src/config/lua/ConfigManager.cpp"),
    Path("src/managers/input/trackpad/GestureTypes.hpp"),
    Path("src/managers/input/trackpad/TrackpadGestures.cpp"),
    Path("src/managers/input/trackpad/TrackpadGestures.hpp"),
    Path("src/managers/input/trackpad/gestures/ITrackpadGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"),
    Path("src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/ResizeGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/MoveGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/CloseGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/ScrollMoveGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/FloatGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/FloatGesture.hpp"),
    Path("src/managers/input/trackpad/gestures/FullscreenGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/FullscreenGesture.hpp"),
    Path("src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"),
    Path("src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"),
)
GESTURE_ACTION_SOURCE_PATHS = {
    "workspace": Path(
        "src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.cpp"
    ),
    "resize": Path("src/managers/input/trackpad/gestures/ResizeGesture.cpp"),
    "move": Path("src/managers/input/trackpad/gestures/MoveGesture.cpp"),
    "close": Path("src/managers/input/trackpad/gestures/CloseGesture.cpp"),
    "scrollMove": Path(
        "src/managers/input/trackpad/gestures/ScrollMoveGesture.cpp"
    ),
    "special": Path(
        "src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"
    ),
    "float": Path("src/managers/input/trackpad/gestures/FloatGesture.cpp"),
    "fullscreen": Path(
        "src/managers/input/trackpad/gestures/FullscreenGesture.cpp"
    ),
    "cursorZoom": Path(
        "src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"
    ),
}
INPUT_BEHAVIOR_OPTION_PATHS = (
    "input:force_no_accel",
    "input:rotation",
    "input:touchdevice:enabled",
    "input:touchdevice:transform",
    "input:tablet:region_position",
    "input:tablet:absolute_region_position",
    "input:tablet:region_size",
    "input:tablet:relative_input",
    "input:tablet:left_handed",
    "input:tablet:transform",
    "misc:middle_click_paste",
    "cursor:hide_on_key_press",
    "cursor:hide_on_touch",
    "cursor:hide_on_tablet",
    "cursor:inactive_timeout",
    "cursor:hotspot_padding",
    "cursor:no_warps",
    "cursor:persistent_warps",
    "cursor:warp_back_after_non_mouse_input",
    "input:follow_mouse_threshold",
    "input:resolve_binds_by_sym",
)
ADVANCED_RUNTIME_OPTION_PATHS = (
    "misc:allow_session_lock_restore",
    "misc:lockdead_screen_delay",
    "misc:disable_scale_notification",
    "misc:render_unfocused_fps",
    "misc:screencopy_force_8b",
    "input-capture:capture_modifiers",
    "input-capture:enforce_barriers",
    "misc:disable_hyprland_logo",
    "misc:disable_splash_rendering",
    "misc:session_lock_xray",
    "misc:session_lock_blur",
    "xwayland:use_nearest_neighbor",
    "render:expand_undersized_textures",
    "render:direct_scanout",
    "render:fp16_sdr_tf",
    "render:xp_mode",
)
ADVANCED_RENDER_OPTION_PATHS = ADVANCED_RUNTIME_OPTION_PATHS[-9:]
ADVANCED_RENDER_OPTION_SINCE = (
    ("misc:disable_hyprland_logo", "0.55.0"),
    ("misc:disable_splash_rendering", "0.55.0"),
    ("misc:session_lock_xray", "0.55.0"),
    ("misc:session_lock_blur", "0.56.0"),
    ("xwayland:use_nearest_neighbor", "0.55.0"),
    ("render:expand_undersized_textures", "0.55.0"),
    ("render:direct_scanout", "0.55.0"),
    ("render:fp16_sdr_tf", "0.55.0"),
    ("render:xp_mode", "0.55.0"),
)
WORKSPACE_BEHAVIOR_OPTION_PATHS = (
    "binds:allow_workspace_cycles",
    "binds:hide_special_on_workspace_change",
    "binds:workspace_back_and_forth",
    "binds:workspace_center_on",
    "cursor:warp_on_change_workspace",
    "cursor:warp_on_toggle_special",
)
WORKSPACE_BEHAVIOR_INTERACTION_OPTION_PATHS = (
    "cursor:no_warps",
    "cursor:persistent_warps",
)
ANIMATION_SOURCE_PATHS: dict[str, tuple[Path, ...]] = {
    "0.55.0": (
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/shared/animation/AnimationTree.cpp"),
        Path("src/managers/animation/AnimationManager.hpp"),
        Path("src/managers/animation/AnimationManager.cpp"),
        Path("src/managers/animation/DesktopAnimationManager.cpp"),
        Path("src/desktop/view/Window.cpp"),
    ),
    "0.56.1": (
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        Path("src/config/lua/ConfigManager.cpp"),
        Path("src/config/shared/animation/AnimationTree.cpp"),
        Path("src/animation/AnimationManager.hpp"),
        Path("src/animation/AnimationManager.cpp"),
        Path("src/desktop/view/animationControllers/WindowAnimationController.cpp"),
        Path("src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"),
        Path("src/animation/WorkspaceAnimationController.cpp"),
        Path("src/desktop/view/Window.cpp"),
    ),
}
FLAKE_LOCK_PATH = Path("flake.lock")
HYPRUTILS_ANIMATION_SOURCE_PATHS = (
    Path("include/hyprutils/animation/AnimationManager.hpp"),
    Path("src/animation/AnimationManager.cpp"),
    Path("src/animation/AnimatedVariable.cpp"),
)
ANIMATION_DEPENDENCY_SOURCES = {
    "0.55.0": {
        "repository": "https://github.com/hyprwm/hyprutils",
        "revision": "a2dbd8a4cc51f7cbe4224732668392bb1aa79df2",
        "hashes": {
            HYPRUTILS_ANIMATION_SOURCE_PATHS[0]: "472747d817e167041c51bb7f4853aca3895450ac2b38464fc60a42acecb9e3c4",
            HYPRUTILS_ANIMATION_SOURCE_PATHS[1]: "cd46df7bd7f8bfb193ace37b32370c99056e9c633cb88793045c32d6d4cdb097",
            HYPRUTILS_ANIMATION_SOURCE_PATHS[2]: "e7e6184fa21c03be8bc4248553fed06e588f6600f8969795f4dfb595f09e7622",
        },
    },
    "0.56.1": {
        "repository": "https://github.com/hyprwm/hyprutils",
        "revision": "5f03477ab3a005ff27c527486f551883535aea2f",
        "hashes": {
            HYPRUTILS_ANIMATION_SOURCE_PATHS[0]: "472747d817e167041c51bb7f4853aca3895450ac2b38464fc60a42acecb9e3c4",
            HYPRUTILS_ANIMATION_SOURCE_PATHS[1]: "cd46df7bd7f8bfb193ace37b32370c99056e9c633cb88793045c32d6d4cdb097",
            HYPRUTILS_ANIMATION_SOURCE_PATHS[2]: "e9aa6712d9987e9415a0644a7e9521fd3813992f32043650ffdbb7f008d7c16a",
        },
    },
}
HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS = (
    Path("include/hyprutils/math/Vector2D.hpp"),
    Path("src/math/Box.cpp"),
)
INPUT_BEHAVIOR_DEPENDENCY_SOURCES = {
    "0.55.0": {
        "repository": "https://github.com/hyprwm/hyprutils",
        "revision": "a2dbd8a4cc51f7cbe4224732668392bb1aa79df2",
        "hashes": {
            HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS[0]: "26079ea62f7a4eca1e3792e7a37c2ca6d1736e3ec879dd35997d29758c9098aa",
            HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS[1]: "2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c",
        },
    },
    "0.56.1": {
        "repository": "https://github.com/hyprwm/hyprutils",
        "revision": "5f03477ab3a005ff27c527486f551883535aea2f",
        "hashes": {
            HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS[0]: "26079ea62f7a4eca1e3792e7a37c2ca6d1736e3ec879dd35997d29758c9098aa",
            HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS[1]: "2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c",
        },
    },
}
GROUP_BAR_OPTION_PATHS = (
    "group:groupbar:enabled",
    "group:groupbar:disable_when_only",
    "group:groupbar:font_family",
    "group:groupbar:font_weight_active",
    "group:groupbar:font_weight_inactive",
    "group:groupbar:font_size",
    "group:groupbar:gradients",
    "group:groupbar:height",
    "group:groupbar:indicator_gap",
    "group:groupbar:indicator_height",
    "group:groupbar:stacked",
    "group:groupbar:priority",
    "group:groupbar:render_titles",
    "group:groupbar:scrolling",
    "group:groupbar:middle_click_close",
    "group:groupbar:rounding",
    "group:groupbar:rounding_power",
    "group:groupbar:gradient_rounding",
    "group:groupbar:gradient_rounding_power",
    "group:groupbar:round_only_edges",
    "group:groupbar:gradient_round_only_edges",
    "group:groupbar:gaps_out",
    "group:groupbar:gaps_in",
    "group:groupbar:keep_upper_gap",
    "group:groupbar:text_offset",
    "group:groupbar:text_padding",
    "group:groupbar:blur",
)
GROUP_BAR_REGISTRY_PATHS_0561 = (
    "group:groupbar:enabled",
    "group:groupbar:disable_when_only",
    "group:groupbar:font_family",
    "group:groupbar:font_weight_active",
    "group:groupbar:font_weight_inactive",
    "group:groupbar:font_size",
    "group:groupbar:gradients",
    "group:groupbar:height",
    "group:groupbar:indicator_gap",
    "group:groupbar:indicator_height",
    "group:groupbar:stacked",
    "group:groupbar:priority",
    "group:groupbar:render_titles",
    "group:groupbar:scrolling",
    "group:groupbar:middle_click_close",
    "group:groupbar:rounding",
    "group:groupbar:rounding_power",
    "group:groupbar:gradient_rounding",
    "group:groupbar:gradient_rounding_power",
    "group:groupbar:round_only_edges",
    "group:groupbar:gradient_round_only_edges",
    "group:groupbar:text_color",
    "group:groupbar:text_color_inactive",
    "group:groupbar:text_color_locked_active",
    "group:groupbar:text_color_locked_inactive",
    "group:groupbar:col.active",
    "group:groupbar:col.inactive",
    "group:groupbar:col.locked_active",
    "group:groupbar:col.locked_inactive",
    "group:groupbar:gaps_out",
    "group:groupbar:gaps_in",
    "group:groupbar:keep_upper_gap",
    "group:groupbar:text_offset",
    "group:groupbar:text_padding",
    "group:groupbar:blur",
)
GROUP_BEHAVIOR_OPTION_PATHS = (
    "group:auto_group",
    "group:insert_after_current",
    "group:focus_removed_window",
    "group:drag_into_group",
    "group:merge_groups_on_drag",
    "group:merge_groups_on_groupbar",
    "group:merge_floated_into_tiled_on_groupbar",
    "group:group_on_movetoworkspace",
)
WINDOW_BEHAVIOR_OPTION_PATHS = (
    "binds:allow_pin_fullscreen",
    "binds:focus_preferred_method",
    "binds:ignore_group_lock",
    "binds:movefocus_cycles_fullscreen",
    "binds:movefocus_cycles_groupfirst",
    "binds:window_direction_monitor_fallback",
    "misc:enable_anr_dialog",
    "misc:anr_missed_pings",
    "misc:size_limits_tiled",
    "misc:always_follow_on_dnd",
    "misc:enable_swallow",
    "misc:swallow_regex",
    "misc:swallow_exception_regex",
    "misc:focus_on_activate",
    "misc:mouse_move_focuses_monitor",
    "misc:on_focus_under_fullscreen",
    "misc:exit_window_retains_fullscreen",
)
MONITOR_QUERY_JSON_FIELDS = (
    "id",
    "name",
    "description",
    "make",
    "model",
    "serial",
    "width",
    "height",
    "physicalWidth",
    "physicalHeight",
    "refreshRate",
    "x",
    "y",
    "activeWorkspace",
    "specialWorkspace",
    "reserved",
    "scale",
    "transform",
    "focused",
    "dpmsStatus",
    "vrr",
    "solitary",
    "solitaryBlockedBy",
    "activelyTearing",
    "tearingBlockedBy",
    "directScanoutTo",
    "directScanoutBlockedBy",
    "disabled",
    "currentFormat",
    "mirrorOf",
    "availableModes",
    "colorManagementPreset",
    "sdrBrightness",
    "sdrSaturation",
    "sdrMinLuminance",
    "sdrMaxLuminance",
    "hardwareCursorsInUse",
)
REPOSITORY = "https://github.com/hyprwm/Hyprland"
REVIEWED_ON = "2026-08-20"
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

# Dormant contract-v2 qualification is intentionally kept separate from the
# v1 tables above.  In particular, adding 0.56.2 to QUALIFIED_SOURCES would
# change the closed v1 hash-table invariant and therefore make a v2 rotation
# capable of perturbing the shipping v1 generator.
V2_PREDECESSOR = {
    "version": "0.56.1",
    "tag": "v0.56.1",
    "commit": "5c9377c15f85c50648f35ca5a213754f95b93ca0",
    "tree": "3dd768e2339d34931da46bc001773356808883f8",
}
V2_UPSTREAM = {
    "version": "0.56.2",
    "tag": "v0.56.2",
    "commit": "efb50993780079460b0cbed1363e2166a2de1d9f",
    "tree": "a0a517a96596fd50c829f0c65eaaa30b52b62fd1",
    "versionPath": "VERSION",
    "versionSha256": "83983962d9161a0ec80cc585a3d3a1deb1f7e6c5ed8a5ba806f7dc338ebf790f",
}
V2_HYPRUTILS = {
    "hyprlandVersion": "0.56.2",
    "lockPath": "flake.lock",
    "lockSha256": "ba52e6238661708fdf94d693d7290698206ada41f1d6ca93053f553fe2beef0f",
    "repository": "https://github.com/hyprwm/hyprutils",
    "revision": "5a7b8cf221914ce4714407950e4ffbdddcd8b66f",
    "tree": "24edbdcc602db7da9af36aeb7b5de8b357ac7ec3",
    "paths": (
        (
            "include/hyprutils/math/Vector2D.hpp",
            "967bd9d7e12efb8f68694760f6d28b8b3c4810055b966b8c61e2ecebb6de2ddb",
        ),
        (
            "src/math/Box.cpp",
            "2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c",
        ),
        (
            "include/hyprutils/animation/AnimationManager.hpp",
            "6b07e7f1b25d19935081bc329a4face09b69a38cf97b9d9cd9bef9575783f847",
        ),
        (
            "src/animation/AnimationManager.cpp",
            "cd46df7bd7f8bfb193ace37b32370c99056e9c633cb88793045c32d6d4cdb097",
        ),
        (
            "src/animation/AnimatedVariable.cpp",
            "e9aa6712d9987e9415a0644a7e9521fd3813992f32043650ffdbb7f008d7c16a",
        ),
    ),
}
V2_PATCH = {
    "id": "protected-f1-float-gaps",
    "fileName": "hyprland-0.56.2-protected-f1-float-gaps.patch",
    "targetPath": "src/config/lua/bindings/LuaBindingsConfigRules.cpp",
    "patchSha256": "ceae6095bd0dd5352ffa35819348e494d3bb17ec133ab0fd3906cbdd1f0f3242",
    "preimageSha256": "157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9",
    "postimageSha256": "d9607574beee09177aee9acc72f9fe15d96fe9f22bf8bd700b10938203fb11a4",
    "hunkSha256": "1b98a3173032283ff7cf5c526e12e5c2bdfc62df6712624aef57de548dadaf14",
}
V2_DOMAIN_ORDER = (
    "advanced-runtime",
    "animation",
    "appearance",
    "bindings",
    "complex-config",
    "dependency",
    "gestures",
    "group",
    "input",
    "input-device",
    "maximize",
    "monitor",
    "observation",
    "release",
    "renderer",
    "scalar-options",
    "startup",
    "test",
    "tooling",
    "window",
    "workspace",
)

# The name-status ledger is frozen in path order.  Digests are independently
# recomputed from the exact predecessor/upstream trees at generation time.
V2_CHANGED_PATH_REVIEW = (
    ("M", "VERSION", "release-metadata", "Bumps the exact upstream release identity from 0.56.1 to 0.56.2."),
    ("M", "flake.lock", "dependency-lock", "Rotates the pinned upstream dependency closure, including Hyprutils."),
    ("M", "hyprctl/src/Strings.hpp", "documentation-tooling", "Corrects generated hyprctl documentation strings; no managed action or option is added."),
    ("M", "hyprtester/plugin/src/main.cpp", "test-evidence", "Extends upstream key-symbol test support for the dispatcher state-resolution change."),
    ("M", "hyprtester/src/tests/main/dwindle.cpp", "test-evidence", "Adds upstream coverage for directional focus seeking around layout-managed fullscreen windows."),
    ("M", "hyprtester/src/tests/main/keybinds.cpp", "test-evidence", "Adds upstream coverage for fresh key-symbol state and group resolution."),
    ("A", "hyprtester/src/tests/main/monitor_rules.cpp", "test-evidence", "Adds upstream coverage for mirror rechecks during monitor-rule application."),
    ("M", "hyprtester/src/tests/main/scroll.cpp", "test-evidence", "Adds upstream coverage for scrolling-layout fullscreen transitions and focus-fit placement."),
    ("M", "hyprtester/src/tests/main/workspaces.cpp", "test-evidence", "Adds upstream coverage for independent workspace float_gaps behavior."),
    ("M", "meta/generateLuaStubs.py", "documentation-tooling", "Adds the missing Lua stub generation surface; no managed runtime action is added."),
    ("M", "src/config/ConfigValue.cpp", "managed-config-runtime", "Removes duplicate cache insertion that could invalidate a config value and crash."),
    ("M", "src/config/lua/bindings/LuaBindingsDispatchers.cpp", "managed-action-runtime", "Resolves a key symbol from a fresh XKB state and effective layout group without inheriting the live modifier mask."),
    ("M", "src/config/shared/actions/ConfigActions.cpp", "managed-action-runtime", "Allows directional focus queries to traverse layout-managed fullscreen state."),
    ("M", "src/config/shared/monitor/MonitorRuleManager.cpp", "managed-monitor-runtime", "Rechecks mirror relationships when ensuring monitor-rule application."),
    ("M", "src/config/shared/workspace/WorkspaceRule.hpp", "managed-workspace-runtime", "Stops implicitly initializing float_gaps from gaps_out in upstream workspace-rule state."),
    ("M", "src/debug/HyprCtl.cpp", "managed-observation-runtime", "Preserves monitor scale and SDR values without integer rounding or truncation in JSON observations."),
    ("M", "src/desktop/state/ViewHitTester.cpp", "reviewed-desktop-runtime", "Routes exclusive-layer popup hit testing through the reviewed desktop hit-test path."),
    ("M", "src/desktop/state/ViewHitTester.hpp", "reviewed-desktop-runtime", "Declares the exclusive-layer popup hit-test helper used by the reviewed implementation."),
    ("M", "src/desktop/state/WindowQuery.cpp", "reviewed-desktop-runtime", "Adjusts directional window seeking around layout-managed fullscreen windows."),
    ("M", "src/desktop/view/Window.cpp", "reviewed-desktop-runtime", "Corrects floating-window render ordering when another window is fullscreen."),
    ("M", "src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp", "reviewed-layout-runtime", "Restores reviewed scrolling-layout fullscreen behavior and focus-fit placement."),
    ("M", "src/layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.cpp", "reviewed-layout-runtime", "Corrects scrolling-layout fullscreen entry, exit, and focus-fit placement."),
    ("M", "src/layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.hpp", "reviewed-layout-runtime", "Adds the scrolling fullscreen handler state needed for reviewed restoration."),
    ("M", "src/layout/space/Space.cpp", "managed-workspace-runtime", "Consumes workspace float_gaps independently from gaps_out."),
    ("M", "src/managers/fullscreen/FullscreenController.cpp", "reviewed-layout-runtime", "Allows layout-managed fullscreen state to participate in directional focus and scrolling restoration."),
    ("M", "src/managers/input/InputManager.cpp", "reviewed-input-runtime", "Routes exclusive popup input and IME stacking through the reviewed desktop ordering."),
    ("M", "src/managers/input/InputMethodPopup.cpp", "reviewed-input-runtime", "Places IME popups in the reviewed top-level desktop render ordering."),
    ("M", "src/managers/input/InputMethodPopup.hpp", "reviewed-input-runtime", "Carries the IME popup state required by the reviewed render ordering."),
    ("M", "src/managers/input/InputMethodRelay.cpp", "reviewed-input-runtime", "Updates IME relay ownership for the reviewed popup ordering."),
    ("M", "src/managers/screenshare/ScreenshareFrame.cpp", "reviewed-render-runtime", "Includes the software cursor in toplevel capture when required."),
    ("M", "src/pointer/PointerManager.cpp", "reviewed-render-runtime", "Exposes and renders the software cursor for the reviewed screenshare path."),
    ("M", "src/pointer/PointerManager.hpp", "reviewed-render-runtime", "Declares the software-cursor state used by the reviewed screenshare path."),
    ("M", "src/render/OpenGL.cpp", "reviewed-render-runtime", "Invalidates depth alongside color for the reviewed render path."),
    ("M", "src/render/Renderer.cpp", "reviewed-render-runtime", "Renders IME popups above reviewed desktop components."),
    ("M", "src/render/Renderer.hpp", "reviewed-render-runtime", "Declares the renderer entrypoint used for reviewed IME popup ordering."),
)

# A VERSION string and option count are not provenance: a caller can edit a
# source file while leaving both unchanged.  These immutable digests are the
# reviewed bytes at the exact tags/commits above.  The extractor refuses to
# generate or check artifacts until every source file used to qualify the
# contract matches its pin, so generated manifests can never stamp a trusted
# commit over caller-supplied content.
QUALIFIED_SOURCE_HASHES: dict[str, dict[Path, str]] = {
    "0.55.0": {
        Path("VERSION"): "ac6c7168f4989720dc4505c85ea89d7cfdbd57f14f903bb8689cd23c992ed19e",
        FLAKE_LOCK_PATH: "0978f7573968480e977764eb309a4426018afd53e60b7636752bc05e3b77956d",
        REGISTRY_PATH: "290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce",
        Path("src/managers/SessionLockManager.cpp"): "7bcea37c191c54401e743b718ea6475183a9dbcc33123bb6e26d88693db1227c",
        Path("src/helpers/Monitor.cpp"): "c84ad7cafd85bdc8192dd30d8441f8d417f7d379d9deee4ee71ef6666ef26a0f",
        Path("src/managers/screenshare/ScreenshareSession.cpp"): "865998b9669353b1653b8f0f78583931d017331c53f71b0956474bb3bd3d6f10",
        Path("src/helpers/cm/ColorManagement.hpp"): "a3eaf1dffb8bef9d18ce0be5bc5b270b9d07f6e605f7161b6c2c7bcd91dfa434",
        Path("src/render/Framebuffer.cpp"): "80ba41c93bb068a85ca94be6f95a6bd43b33f2da52618f2c7fb78c229f787648",
        Path("src/helpers/MonitorResources.cpp"): "18ff58ce9ed2268f361a49f22c7e5d9b951234b0cab5335accf628b6ac92bb4c",
        Path("src/desktop/view/Window.cpp"): "ec00fc5ca125163eadaa267eac73032de28d9d7f32a4b03ae4498a89427b2a7e",
        Path("src/desktop/view/Window.hpp"): "c52d94684d4dfab1fe80df565a08b560c3fd8e8ca32571436c8fa6f569f76e42",
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.hpp"): "30a8f21c37fbb2207ba9647d2432916b3f3ba1214a8788fc9a9c599a6cdd931e",
        Path("src/render/Renderer.cpp"): "67febb2393cdad2671d172dbc84f230a735f579411ef0ee4f441421ef318cbf7",
        Path("src/render/Renderer.hpp"): "196404adaf7c2af5f9f7ad0383c95c0a89d1d5c4e560941f38ea10b960650d24",
        Path("src/render/ShaderLoader.hpp"): "8e35c2300fa597bdad275dbb1e2c14067a6440d4b4032e0881cdc4765190fb93",
        Path("src/render/Shader.cpp"): "2e98805b5082cb48e88fa364c3703087f574cb781903de07d833758581684f78",
        Path("src/render/GLRenderer.cpp"): "a2f5aac8e77ec4a96a87b038d36f72e3b84af3325f27cf9cb1d8e2d6991773c3",
        Path("src/render/ElementRenderer.cpp"): "5dbe22ebdb367af0d6e3e4fd566c4532b98d949007668249c957de075a28a24e",
        Path("src/render/OpenGL.hpp"): "f1d33ee262601cc3a2d5df0d694b7352de5f401d8063a487e16a092f255c5e3e",
        Path("src/render/decorations/CHyprDropShadowDecoration.hpp"): "aaa43c9c130461a7558f17b78a302ebd9409137d79aebe34417f9fe7c12905e6",
        Path("src/render/pass/BorderPassElement.hpp"): "0aafff3cbfc3290bd725a6b2ea2f4a10f7f47f5505108ce36569395e7fc53efd",
        Path("src/render/pass/RectPassElement.hpp"): "29b59dc0f46df48ea6624a1bec7705f359ddb99390458a9d5c8914c63fcb3569",
        Path("src/render/pass/TexPassElement.hpp"): "d95d048fc0ec7ad06cce2cb1799b1cf46e3b08d632cae3429866f8428ad4d4e2",
        Path("src/render/pass/TexPassElement.cpp"): "b3b482a7d4c121a0163ff3120642a6490d237d84812e7d01db82a09fbc250b71",
        Path("src/render/gl/GLElementRenderer.cpp"): "1c14d08921b57a051fba09fd272d0efb5b4dd10452b7e8bd675ba548214a0391",
        Path("src/render/OpenGL.cpp"): "2b1245e9207db6e33191a509fb45c1cdf1f1380146e55d61b56ea920857ea1e3",
        Path("src/render/pass/Pass.cpp"): "e78eb11477d673ade97ad7a12486ebd97366708bd5e634b5a7f6901fe649c9e1",
        Path("src/render/pass/PreBlurElement.hpp"): "50675b156230bc29e1ef11844f360cc5e01d0b514ee10ae817a4a70f3ae38528",
        Path("src/render/pass/PreBlurElement.cpp"): "308c85d990a3cbe1458aee1990fa91ca76cc6118fde9b3a961e42d40282c77d5",
        Path("src/render/pass/SurfacePassElement.hpp"): "e5fe6f6213f21197c23773caef98742eb3d27377ff7342f1a75b0c020cc04717",
        Path("src/render/pass/SurfacePassElement.cpp"): "c5e3729400ca779e73b516ea9c9367962d5358c28bb4fc396e348ad7aa4d0f46",
        Path("src/render/shaders/glsl/surface.frag"): "6830e2db330843fcb4917fa4ed7118767999ca25b4840d135d4d8cea8143bb48",
        Path("src/render/shaders/glsl/border.frag"): "c477ed2719e7d4889eb1e7b40dae897da64b9c2b515892f1996303de85556ef3",
        Path("src/render/shaders/glsl/border.glsl"): "e476ed34604e3e3cebbac55e8d0fb628d8f297eb643db304abd317d79869d112",
        Path("src/render/shaders/glsl/ext.frag"): "d2164a4529ecbef68ec8ce5f6d459a71762bbe6d9b5d118ddbc478d9e750a3c7",
        Path("src/render/shaders/glsl/quad.frag"): "28862349478c2ea5d1122aeef9b38e79e230ce358a2c659ec7f074b73b84afee",
        Path("src/render/shaders/glsl/rounding.glsl"): "67efb089576ed2b8ddb327369e75049cccc822564e0e5d845d9c6f325f4de9dc",
        Path("src/render/shaders/glsl/shadow.frag"): "16d1f5c7efcde9d31a4690a2c25e2db6b89a61ab97ed0f97b467a4097715ef60",
        Path("src/render/shaders/glsl/shadow.glsl"): "6f925478b303f8a467c926cb33fb5f6f197e22bfb5819665cf3cf95f785abdb1",
        Path("src/render/decorations/CHyprInnerGlowDecoration.cpp"): "0d8fcb1608d6f355e8a8dae63b2dd715c89e7f565a0611e4794982d97d895938",
        Path("src/render/shaders/glsl/inner_glow.frag"): "d595247c788e9193926b7826224bf4bb49f34579d2fded34c708ebd39bfd27e1",
        Path("src/render/shaders/glsl/inner_glow.glsl"): "3428e751180963f3c93423ca0cf29898aca481e8869be93a7f080b67d18517dd",
        Path("src/protocols/OutputManagement.cpp"): "619e3854259769282c2aa07496158af0a80d4cfa340e67bf937a958e097ba96f",
        Path("src/config/shared/monitor/MonitorRuleManager.cpp"): "fac5ef95c741c8fb6b46802d3711f579cba23b5e2f44d051e5e3a0ed568e8b14",
        Path("src/render/shaders/glsl/blurprepare.frag"): "e870f47f282e21cc5a8534f1b62ebf6c5dc96fb7ab53e3db0582ec80b9b5d1c3",
        Path("src/render/shaders/glsl/blurprepare.glsl"): "219d76ec643ce3055d295850f54365412ba2eb4180015c59e1764807ba64a56d",
        Path("src/render/shaders/glsl/blurfinish.frag"): "77a16587e972ced698da747f29a1c0fd91a70f79b9db75beeab3916391710748",
        Path("src/render/shaders/glsl/blurFinish.glsl"): "ab3df5ec456f60c83a3af85796816b5d99351cca309eaf7023eddf0dd695c731",
        Path("src/render/shaders/glsl/blur1.frag"): "ea384ff735b47417aac1873579582dca3a8e57e9bbde0b643e6f15b2ffae3c68",
        Path("src/render/shaders/glsl/blur1.glsl"): "ff0985d2b95f6d927d39a2058379283ac6118ab75b47b90a536fa4da4cc0fd4d",
        Path("src/render/shaders/glsl/gain.glsl"): "fae6204d8e51d368534b23988e5de84c5d3bd2e25a8d95c5266a9d0bbc0c4bfb",
        Path("src/desktop/Workspace.cpp"): "5e7d585513e7d4c79c93edc5303d578ae4cbdb337a2128f37358f754bac0c5f6",
        Path("src/render/decorations/CHyprBorderDecoration.cpp"): "b03a9c649c158403f88da98fc686514e587cd4a1bf13eb8bd143f1e5edab8d74",
        Path("src/render/decorations/DecorationPositioner.cpp"): "aea200570600718a05977daf1beb2a2188058e7f0290e36a9ab5b0029b47629d",
        Path("src/render/decorations/CHyprDropShadowDecoration.cpp"): "d2bf3906c2d63633200b222e03c1f664a8bb4f90fb81f46500af3f18335d56cb",
        Path("src/desktop/state/FocusState.cpp"): "677c243ada837fda14aed11c55b435de0fd877549709158dda1dfb2c8530dc43",
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"): "35e330cba1af5e07968f9b968c2719a46aa28728537e3d0e97d13f6ab4dcb247",
        Path("src/layout/target/WindowTarget.cpp"): "33f6f6dcbfb4e140f2cce04a8e06b8042c8e780617960618b023f9a9591b9a9d",
        Path("src/desktop/view/Group.cpp"): "b0f9210c858bcaf74c8f5a44f7f40abfe03af8a096e3fb158afd2346be8b0eed",
        Path("src/layout/supplementary/DragController.cpp"): "0d1ddc01f506cdbf3dcec5ea4e111c61286d2ce4359100847270f82569b3505f",
        Path("src/render/decorations/CHyprGroupBarDecoration.cpp"): "3d687c43d5414fb6ad617465f06bb87cda8dc6c2e791dbb0e28de72de1cf4c68",
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): "6ee949391fa74f713d00b718e0e498fbbc1cc58e2931e1737c4c865fe8f6c679",
        Path("src/config/lua/types/LuaConfigFontWeight.cpp"): "da094c9f2e041586b9138280339dfdb24b78712b324462722e86186c8b274c05",
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): "2693dd89945b35c650b0bcc91d8da3441e690ca2a4f20705cc9be713e9314c94",
        Path("src/config/lua/ConfigManager.cpp"): "204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642",
        Path("src/config/lua/bindings/LuaBindingsRegistration.cpp"): "c0e0533d07bc75b48ba469df2523b3f1335bcea17922ac0dae01aed81487e5d3",
        Path("src/config/lua/bindings/LuaBindingsToplevel.cpp"): "bf1f9e9bdd94a2403ecab41ceea6ffb6feecd3083715ec4e8086e84089e9a006",
        Path("src/config/shared/actions/ConfigActions.hpp"): "14b6d07d7720e45e099e469ed53ac2db1584e408705869b71a92a14876767b2c",
        Path("src/config/shared/animation/AnimationTree.cpp"): "f264fc3fe7de93237af06122600949984064e75e240d195ce07113d526edcf06",
        Path("src/managers/animation/AnimationManager.hpp"): "601532457f29435879665f6ecd92adfb86f9f8e16f7fe48abd068f041e2edb14",
        Path("src/managers/animation/AnimationManager.cpp"): "92702589150c0264d109d5ae74915a778368d471eb935ae4ae7a6de0d083f596",
        Path("src/managers/animation/DesktopAnimationManager.cpp"): "437b5a34fa6b462e68890679abf38f7d806ebc56f9fb6ebc814bfc40e44c3ad6",
        Path("src/config/shared/actions/ConfigActions.cpp"): "2b2b09799b4cca634d544d4103f07b82ef4b6c377131c858d2707a5e0494fe07",
        Path("src/managers/KeybindManager.cpp"): "2fde298fd690c5c3b72741be1c4b11cba220996649ba560e3112a81112531491",
        Path("src/desktop/history/WorkspaceHistoryTracker.cpp"): "628b33ca5fcfdb20e21daed845732ef812eb77bc6e0dddfd6db55cb9f41ca989",
        Path("src/desktop/history/WorkspaceHistoryTracker.hpp"): "89afe0b8578d193b6636af422911922a39b7bfdf130bfb286d0708d4a489d009",
        Path("src/Compositor.cpp"): "a639c01f628e50d765856001c567aa8f344e6a620f69fca994e4436b3a090125",
        Path("src/managers/ANRManager.cpp"): "67276cc0d2dccf516a1075112a5eb3592871c6d88d8e7665a4503194b4ae558b",
        Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"): "d5ec852d1b26fb55574ae184b6bfc86dd2a047f8c7d4c8c7e4748277c0e841cb",
        Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"): "7a95cbac2074f7cde31e3181e84bf146337dea586fce56900eb1fb55af5864c0",
        Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"): "f1f95b5e53f048ef52f6a87be7ab3040da74ce6361dee17bd5d97ef583ee2f0f",
        Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"): "00d5563d7c49f7df81378496e6e35a95356bac3f4369d5de7782ae7012f2990f",
        Path("src/debug/HyprCtl.cpp"): "a88c1517da9fa2934d740dd1fd9cbca4d5d0bdd5b569a63bdba64e5e36c2d886",
        Path("src/helpers/MiscFunctions.cpp"): "b11ed4a52bebcdcfd4f934b94b2a5f324bf66b2b035c5d7f119c6c027800f8aa",
        Path("src/managers/input/InputManager.hpp"): "c4479cdf4b1dc6a5f1234d86a034fb5f1f517a140afb33dc5995c65d20ac0dbf",
        Path("src/managers/input/InputManager.cpp"): "410836e5695062779cf525ed35b53900d199490f0db5eeba85f12fb894053835",
        Path("src/managers/input/Tablets.cpp"): "b875f1c65775ead92dcaae26612290e5b1728895f659f933de283086b8fdb683",
        Path("src/managers/input/Touch.cpp"): "c7cc52a8da21286035b144366ad82aafd93610d4ae5827f0b4246034acd6549c",
        Path("src/devices/Mouse.cpp"): "aebdf0fd1765d25d943cc79f4e5bbdce09e0c437ae3e9cdc0985195df54a5470",
        Path("src/devices/IHID.cpp"): "a58a4b44e5947e82ab29cab414d92511f953a0feb87b662516496a869f1c7ee3",
        Path("src/devices/IPointer.cpp"): "ffb4ca03e3d4cdd0d7f615d27c0c01b42d360206e471bc66d50322f87ac36ccc",
        Path("src/devices/IKeyboard.cpp"): "f1bccbbb227ac7808f25a941601eba327a498f3556ba12514196035eac5d55cb",
        Path("src/devices/VirtualKeyboard.cpp"): "bc6d984a4c9e62313501c1cdb4ace3659c16d3b77e029a3c082a77f21500ed86",
        Path("src/devices/VirtualPointer.cpp"): "b1425a95edb425a229ffd61b266f9c02c8882a5b3dee6cd4d3b6d0e5427b516f",
        Path("src/protocols/PrimarySelection.cpp"): "f4989a7770e13d351a7db362d0539e8d96d23b9f7c524d278d1e529f68e0391b",
        Path("src/protocols/VirtualKeyboard.cpp"): "43cbf7b39bf0910df6d933cef95f029fa336d5b432b0202280e6fc40d8db2f50",
        Path("src/protocols/VirtualPointer.cpp"): "eb6d8ea90209d6fe1899ae075465cbd8b0b6be62d301c06c493212a801e308bb",
        Path("src/managers/PointerManager.cpp"): "3bdbb256f39b8a892f70ff6795a366068997e335c1258dc6baf3dd8dcd34fcb3",
        Path("src/managers/PointerManager.hpp"): "c2a0a22603230fcaaed62afade6f0dbc8a6e8381906b8a5063e65651cad43718",
        Path("src/Compositor.hpp"): "472006be35fdf03c09566e4a371a05b8ba2e39f9a4dd13eb7313e24d2575f4f2",
        Path("src/managers/input/trackpad/GestureTypes.hpp"): "87ef4e338afd1ba8f16f2d4a9c03dfccedef3da0f5cdb4464da0c3ec903f23c1",
        Path("src/managers/input/trackpad/TrackpadGestures.cpp"): "5e23524d8a6a0778fc8199173978488e4b54e70611ec354b036a99ae6b9610b7",
        Path("src/managers/input/trackpad/TrackpadGestures.hpp"): "f0e2a20af8b5c7e41d1c7a7e3bf650b9a6ac89f883adeefca0f39ff060cdce09",
        Path("src/managers/input/trackpad/gestures/ITrackpadGesture.cpp"): "c2ce8e076a7bc326316d3c9ef0221e5ad7699fe87669fa9188a2fc872c1ebf5e",
        Path("src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"): "3e89e749eeebee1050907830432221963e4ad9c2e7b76cebf784ef993e445890",
        Path("src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.cpp"): "fc54ad123f0406bcb9b8e42b8241c4a208b377c107ad7820f0b26ada58cd3d54",
        Path("src/managers/input/trackpad/gestures/ResizeGesture.cpp"): "97c13e3889e744b5b3617f16620033fc0d4ce43cceeb014cdfcf7f5b85e7adc1",
        Path("src/managers/input/trackpad/gestures/MoveGesture.cpp"): "2fee93dc6b90c8638f00062685dd0185de07a4f3701cc3d6a78064da50fe3fe0",
        Path("src/managers/input/trackpad/gestures/CloseGesture.cpp"): "1963b7c441bab5a9ce9e7605f16c5d3289c927208cc11c1f3895aecd31116d85",
        Path("src/managers/input/trackpad/gestures/ScrollMoveGesture.cpp"): "e78caa506e078c6710338f9e2419c9af22668fbcb5ed078ac7e0256bb8906517",
        Path("src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"): "5a8080a4c4d2f81824bf37192f450bf48deba2de37a0fe39e80e86d8af80ef42",
        Path("src/managers/input/trackpad/gestures/FloatGesture.cpp"): "92d3eed9c053b2da300f67aeed974ab21b59068a2dd27aeedce6a41548c1acf1",
        Path("src/managers/input/trackpad/gestures/FloatGesture.hpp"): "dfc7559d79b9109ed36f202483d4bcd64d1f08b4e2daef182fd44d5433b0e307",
        Path("src/managers/input/trackpad/gestures/FullscreenGesture.cpp"): "c3c20c47e1421e567bf9ad9376a37062b2863409f3bbd6046b3e55272b4e9417",
        Path("src/managers/input/trackpad/gestures/FullscreenGesture.hpp"): "97121208913016b8f423a830e046b2704c473c466e8fb0a2cca6b301e32a5f6a",
        Path("src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"): "c8acc40765d52a0581a72536ae5489aeb43a9d275f4c7ccd8c36346e8b4f9002",
        Path("src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"): "a070dfb875fdb80559c6278afe1ed9c6d83237c9fc58d356639c7d9c17b6d779",
    },
    "0.56.1": {
        Path("VERSION"): "2aaeb543208a766598b45262f3eabb0600c2a6055350ab8be22b5bf944a484e9",
        FLAKE_LOCK_PATH: "a44dd68728466027d72e434856186f56610c46ed0ea3826b370827a23c77b74e",
        REGISTRY_PATH: "a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754",
        Path("src/managers/SessionLockManager.cpp"): "221e45b09e75ced356bf229169c207f43b4adf8e93147288c47de0cd04e9a9c0",
        Path("src/managers/screenshare/ScreenshareSession.cpp"): "b6b2ce5d682080131f374510e7cecae54925356bea541ef99feeff3aba394c99",
        Path("src/helpers/cm/ColorManagement.hpp"): "c9b4823032e12cb45907aac714a19c5de89e5565a773b4eccbecd83ddb5d4de6",
        Path("src/render/Framebuffer.cpp"): "80ba41c93bb068a85ca94be6f95a6bd43b33f2da52618f2c7fb78c229f787648",
        Path("src/output/MonitorResources.cpp"): "90d39fd2c43852611b4f90ca14cc2d213f97c4770123021d2733cd1970488b8b",
        Path("src/Compositor.cpp"): "74833ecbf0e2b6f8ad84345ac0716a3295a0e347420a188be0ab4f6a684af7c0",
        Path("src/animation/AnimationManager.cpp"): "8c37cd0d1e972e8789468fdf063456a6a40ed17d482b898b0639a6d2a1fa7985",
        Path("src/animation/AnimationManager.hpp"): "e7834aef3b0f3259a109e5407c27bd31d5ad8ed05b698e4726e5972bacd479fa",
        Path("src/animation/WorkspaceAnimationController.cpp"): "0698720a19698186197a0f1c98893b839502ad454d751d3e590f2eb0ae2b5e5b",
        Path("src/config/lua/ConfigManager.cpp"): "94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502",
        Path("src/config/lua/types/LuaConfigFontWeight.cpp"): "da094c9f2e041586b9138280339dfdb24b78712b324462722e86186c8b274c05",
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): "157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9",
        DISPATCHER_SOURCE_PATH: "76488e1f4893fcf835c13ed98e51ab4d1c72d76a12c753eb0ad3a2237bf95223",
        Path("src/config/lua/bindings/LuaBindingsInternal.cpp"): "5f6534641d58073bbcbd3e004b168659d497072596cf10c9f2b189c55c2233e9",
        Path("src/config/lua/bindings/LuaBindingsInternal.hpp"): "83630401a5b3d2cd99ec42705a7b70b0fdd73ddb3989c561a4316d15d72f0edd",
        Path("src/config/lua/bindings/LuaBindingsToplevel.cpp"): "706b29eb52de087c1d6e64770cf85fb3ebc0f2fbe9ddcff086905499bcd332f5",
        Path("src/config/lua/bindings/LuaBindingsRegistration.cpp"): "377e617ca0400722dea581e1ca740500a6057cff3842998c8f8dd0a218f77770",
        Path("src/config/shared/actions/ConfigActions.hpp"): "746c6f517620eb039afbfbb9a4a0e2d5ba1c5eb117bcc8f396173f63091b7073",
        Path("src/config/shared/actions/ConfigActions.cpp"): "29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab",
        Path("src/config/shared/animation/AnimationTree.cpp"): "313ed20167618dfe163fceee8753e9a49d8ef356bc3f580963e07039839e7bae",
        Path("src/config/shared/monitor/Parser.cpp"): "616468fd0576d8b201ef00cd69289234f0ffb1c3137ae2a0ac954fabe37dd589",
        Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): "65f01a1a50ef05ecb1630f8fd30bbbac91029b29f6b4b6bc3f353d0bb4b2817b",
        Path("src/config/shared/Types.hpp"): "337556a3fbedda3e34f31f600f4a44d4e85f53a04220f3c90d8394f98d1eb638",
        Path("src/desktop/rule/Rule.cpp"): "257687efd814cb714024c14e2952adb26e1e7a1f5907d20aa6aaea1e4c098e2e",
        Path("src/desktop/rule/windowRule/WindowRule.cpp"): "7034c3325fee476cb501266af67999534ca36cf2841d83c9940db96d116d1f78",
        Path("src/desktop/rule/windowRule/WindowRule.hpp"): "12cf28cc2aab0126a52ed61b7dcabd77ec6aebb643846883cd4d439e1603853a",
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"): "fec33f9a0bc279ffc94ca27a6e9843964840d6269bc28d330d566e05fed3307b",
        Path("src/desktop/state/FocusState.cpp"): "9b971362bba97f200f0ef786237deed6b7b863066590d243cac11787e2a56ebd",
        Path("src/desktop/state/WindowQuery.cpp"): "5eede566e031769aa13247830f68a10077c78f4db864fdf17dcfa58f222a9beb",
        Path("src/desktop/state/ViewQuery.cpp"): "10acb6240350d3fcb07544ab34279bd050984f02b1087d2d4a1febe4d2cbf1cc",
        Path("src/desktop/state/ViewQuery.hpp"): "e7037e695a48e3b856f2c42285225db03f62aa41b4f694539db4b65be5736a7e",
        Path("src/desktop/types/OverridableVar.hpp"): "4d111c95b15133ca8f6f8a579fe36f9652a7298d031c45d84d83576670c06501",
        Path("src/desktop/view/Window.cpp"): "4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f",
        Path("src/desktop/view/Window.hpp"): "1f60b8ff0bc6b5a48af5c03cb2cac1108db677efc70b4424b56ef6f1f2d1cc81",
        Path("src/desktop/rule/windowRule/WindowRuleApplicator.hpp"): "334e1254a66e2df8ef243544e3ff99db398ed5ec6e48996d496d05ff7bf50f44",
        Path("src/render/Renderer.cpp"): "4c3b2a4d42fbb1021a7cbb5013e826b926a9756ecde40ef5e9acdc3a9a16c4a7",
        Path("src/render/Renderer.hpp"): "0787f259d788e979094ebb5f3b40140c642118c634356a431e2aca17ecf4d5a2",
        Path("src/render/ShaderLoader.hpp"): "7f3c109c8ac1c044c0b48b56e98b977ad29b6138fed9f45041163a83eea23663",
        Path("src/render/Shader.cpp"): "dcf574e0b64c246d12fe03bff6af4c9631b95baa18d56e481d6d37ffa7b64f64",
        Path("src/render/GLRenderer.cpp"): "2072807043b5d53a0910ebcc3936e69905abdf5905457d363f70d8c1043a4a81",
        Path("src/render/ElementRenderer.cpp"): "32f79c359a5ae6a265f7b2feee5c190d643371dd984d2bdda6a4ef203ccec828",
        Path("src/render/OpenGL.hpp"): "e08060838c88d6bf650fd2e2b695acbda48e10f04a317a397d35f2f1dff9c813",
        Path("src/render/decorations/CHyprDropShadowDecoration.hpp"): "013d55c2923012159989fc06cecd655ed75e51578c6f086254e5d840238c33bc",
        Path("src/render/pass/BorderPassElement.hpp"): "0aafff3cbfc3290bd725a6b2ea2f4a10f7f47f5505108ce36569395e7fc53efd",
        Path("src/render/pass/RectPassElement.hpp"): "29b59dc0f46df48ea6624a1bec7705f359ddb99390458a9d5c8914c63fcb3569",
        Path("src/render/pass/TexPassElement.hpp"): "9f3a858292d9b2892df0f8b64ee605d2d4322cb57dd3bb5b3ed12e1f0638de3a",
        Path("src/render/pass/TexPassElement.cpp"): "60dcded5bef532e26ee1d4e57d1cd0d653abea406114d71c72d9e49ae83f526e",
        Path("src/render/gl/GLElementRenderer.cpp"): "d7db3c34fbd94c3acdef3dc821d906a48583ac188704ec1943959df1c88239f8",
        Path("src/desktop/state/LayerFadeout.cpp"): "913541971166a171d5c922b63fbbda3be51afe2c25c43f54df97cb7fa5b9b254",
        Path("src/desktop/state/WindowFadeout.cpp"): "510e3f19d645585e55ced3fb090037c7e4f003e67fdba1b65dcbb5ff92ff03fe",
        Path("src/render/OpenGL.cpp"): "6b58dcffd17364f4f98719f3b3dab342dff1e2ca0b922a65cc2bfb664f81ea16",
        Path("src/render/pass/Pass.cpp"): "529c5c9d55dd707c4bc98da38832b0006880d3948592678a31db62e876e34820",
        Path("src/render/pass/PreBlurElement.hpp"): "50675b156230bc29e1ef11844f360cc5e01d0b514ee10ae817a4a70f3ae38528",
        Path("src/render/pass/PreBlurElement.cpp"): "308c85d990a3cbe1458aee1990fa91ca76cc6118fde9b3a961e42d40282c77d5",
        Path("src/render/pass/SurfacePassElement.hpp"): "74bd3d56f5d6e75b43fa9d8291141869849341f4b5ec9d10f6c7ed3f5b55c2d5",
        Path("src/render/pass/SurfacePassElement.cpp"): "ea559690633f8f2e0da7709cb3c00f12b54bc20fbe631437633744676eff24f8",
        Path("src/render/shaders/glsl/surface.frag"): "26fb659bf56460aa051b28ee8b3ce9fed4ced34fcc836aede291058dd8195970",
        Path("src/render/shaders/glsl/border.frag"): "bbcd85dd46b97d995418aedc6bb42e0f7e2500a855a777452f2ecb61598d8c49",
        Path("src/render/shaders/glsl/border.glsl"): "730875c35be7bc002940f8db0b363a65cbc6659f7630a21eb0a46410d1fa619a",
        Path("src/render/shaders/glsl/ext.frag"): "d2164a4529ecbef68ec8ce5f6d459a71762bbe6d9b5d118ddbc478d9e750a3c7",
        Path("src/render/shaders/glsl/quad.frag"): "28862349478c2ea5d1122aeef9b38e79e230ce358a2c659ec7f074b73b84afee",
        Path("src/render/shaders/glsl/rounding.glsl"): "67efb089576ed2b8ddb327369e75049cccc822564e0e5d845d9c6f325f4de9dc",
        Path("src/render/shaders/glsl/shadow.frag"): "2f2c44a1d3a0733fc1557c3789bae26188410ba6e177baf1648976fb824b4c14",
        Path("src/render/shaders/glsl/shadow.glsl"): "c4ca78af1d094eb0f996a06ac14bdda4c45a8d993da392b1b505ed513714e15d",
        Path("src/render/decorations/CHyprInnerGlowDecoration.cpp"): "c857934b45dfe308c56f0a45defd25d9b7e0c1a07e05ff52255c9c9441986c69",
        Path("src/render/shaders/glsl/inner_glow.frag"): "60b5bf2771382af07a19cee5e66fe909030bbe087217f9d9baaf122bcd395e02",
        Path("src/render/shaders/glsl/inner_glow.glsl"): "7858f0226aebfabdc78e6eaae3cba524ec3173b2d59a021d5cdc0bb54a66293b",
        Path("src/protocols/OutputManagement.cpp"): "148f12d73ced4e045bc864ebc3cc5022e3681b82bbec9936f2cd8b32d0bbd588",
        Path("src/config/shared/monitor/MonitorRuleManager.cpp"): "8946d306e66afc42c1d3b49fbbadd5ccf931c630896bda8430e6aaf7b3a7ea65",
        Path("src/render/shaders/glsl/blurprepare.frag"): "e870f47f282e21cc5a8534f1b62ebf6c5dc96fb7ab53e3db0582ec80b9b5d1c3",
        Path("src/render/shaders/glsl/blurprepare.glsl"): "1e359628ecdae3bd769e6a2c00b13d09df9319c88442730f5d8510c78feaeb36",
        Path("src/render/shaders/glsl/blurfinish.frag"): "77a16587e972ced698da747f29a1c0fd91a70f79b9db75beeab3916391710748",
        Path("src/render/shaders/glsl/blurFinish.glsl"): "f3c1f38430bb362552584c8f8ad5b52ac2838b18d576eb1d87fdbbbba6258c27",
        Path("src/render/shaders/glsl/blur1.frag"): "ea384ff735b47417aac1873579582dca3a8e57e9bbde0b643e6f15b2ffae3c68",
        Path("src/render/shaders/glsl/blur1.glsl"): "ff0985d2b95f6d927d39a2058379283ac6118ab75b47b90a536fa4da4cc0fd4d",
        Path("src/render/shaders/glsl/gain.glsl"): "fae6204d8e51d368534b23988e5de84c5d3bd2e25a8d95c5266a9d0bbc0c4bfb",
        Path("src/desktop/state/PopupFadeout.cpp"): "d3498272769a78ef6f700f858f454c1bed056425df23a2c3003a6fb43b08879f",
        Path("src/desktop/history/WorkspaceHistoryTracker.cpp"): "33a0cf8d26540870b66d6aacb6fe7eaad0c767e8b21184016c12de5135cea678",
        Path("src/desktop/history/WorkspaceHistoryTracker.hpp"): "89afe0b8578d193b6636af422911922a39b7bfdf130bfb286d0708d4a489d009",
        Path("src/desktop/view/Group.cpp"): "f34fec0891e69e0a1b67901e8d1b6b1a1ae8a47eccc92313c2b41660961cd385",
        Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"): "218c4d4ba7e7b34d1113da818347e260868451de0e5a6d699934871febc03b32",
        Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"): "ad3494f7292fbd717c1e9ab608e0b2fbd488cc85e88763317c563162c9e978df",
        Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"): "bfec45e79d26bb21dea2c61809ac37baffffa48b6e0202a56e7522b22a1815f3",
        Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"): "4a5735c12b4e2f1b1d29fc655b75c9bd04ff278138dea9013a3cb569566306fb",
        Path("src/layout/target/WindowTarget.cpp"): "8bcaf37000cf55e1d2084bf732f0589b4ab49d36c778d8d4efa98949dd14734d",
        Path("src/layout/supplementary/DragController.cpp"): "02560e7a38902cd400c3fa1b229eb6ba2c9494750413116b9277f5e8a818b62c",
        Path("src/desktop/state/GlobalWindowController.cpp"): "669cb209f2e4efb2b248bbbc00ef8cef84a4638017a30dc085c8f85fdb2d65f8",
        Path("src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"): "22b223f086b454c54f475e7f16b2d499d92cfd071f2083ea58a9f6a4412f0b1d",
        Path("src/desktop/view/animationControllers/WindowAnimationController.cpp"): "9a8c3c2961a1a55fca7b8783e1c3b808b424291e832bf4cdf88ed7a73e5bab08",
        Path("src/devices/IPointer.hpp"): "bf4b2309526ca44ac30d2c3d3ad6a66cc7bf8d7a398b22a8fdde58c0f0ab1310",
        Path("src/helpers/CMType.cpp"): "b3df474366f4c2e4dae85beae34eef900c5390faaf2683db2e14c919abdd8e00",
        Path("src/helpers/MiscFunctions.cpp"): "065418241a3b40e21273bca1fd29036221942554cddb61e08422d86f0a13c1d9",
        Path("src/helpers/MiscFunctions.hpp"): "084844f04b5be8ca1e9d7c6e49f94a4b6fc9cd3e387a060d6f70dcc0b68c7c79",
        Path("src/helpers/TransferFunction.cpp"): "503eafd06a295b1ecbf0506d552db337f79498180f183c0bbc7a12f8855baeaf",
        Path("src/main.cpp"): "98e5752cd485378c58c2a8cb0ba89265eaea27144925ea059e5564917dd3b645",
        Path("src/debug/HyprCtl.cpp"): "7b96515a4cf13333ca71549053e76fcdd9cf815b18e4ae530dfff169af3ff1d1",
        Path("src/output/Monitor.cpp"): "9cf88e154eb5dae676c79d37b5b055ca6134838857cecdbb89a3b747a6821927",
        Path("src/desktop/Workspace.cpp"): "e6c8e44d9f8211a8f56b65b433b5f5e4c3e6565479ecb2d749bc02cf4e926ca9",
        Path("src/render/decorations/CHyprBorderDecoration.cpp"): "23ef95cfd33ec3116dbcc0de0a521809a8752df80d83df926ae2771631f48b46",
        Path("src/render/decorations/DecorationPositioner.cpp"): "2435b97c90534fd3cf8a859f8c23d31a9ba89e4b5b7eed529e10bc82f7676b6c",
        Path("src/render/decorations/CHyprDropShadowDecoration.cpp"): "f83870a33d8d7b4d28f2c03ecdf0673d4bfef2192da189a1fe03684c9a6fce41",
        Path("src/config/shared/workspace/WorkspaceRuleManager.cpp"): "06e8dcbaea83e51710bd372a8f264ad3d07bbe061b9720bbe456811451c02d10",
        Path("src/config/shared/workspace/WorkspaceRule.cpp"): "96e1cc448847b03e7f48dc7e1c2bc4ed6c192fcf7c11c608b33f5760f7788f96",
        Path("src/layout/space/Space.cpp"): "8ddcd4dd0a90bd59d4fa21f95f53444419188846a3a217284f0d6b008875d376",
        Path("src/managers/fullscreen/FullscreenController.cpp"): "581d92ef70588fce181b4b87a04e37f6de7b4777c24ad7fee34b21f941b706b0",
        Path("src/managers/ANRManager.cpp"): "842b795210ea83c735cb9a75c5e2f104507fa9285a460730dcd809e87892803e",
        Path("src/managers/fullscreen/handler/FullscreenHandler.cpp"): "5bfed3aa05f2e6f013e7776efaa754e3e107aa7688eb5d523bdbdfa87f51bc85",
        Path("src/managers/fullscreen/FullscreenController.hpp"): "7f3585f23e4d756f3f165670a604de39717eb3fd704114e2a230bdd6ffba2378",
        Path("src/managers/KeybindManager.cpp"): "8d8f35fc84c4a2f8de63ac2bf6f6531c651c6fa6d37b1aac7c8fc5d9342d4e40",
        Path("src/managers/input/InputManager.cpp"): "07de27ef0f4c9a5c3bf14f42c07af29574d428a7b02412f785f90db30b03125e",
        Path("src/managers/input/InputManager.hpp"): "bb8ceaf61e274bedae10555173760055e632e0ff2ac050636e06c29dffcb188d",
        Path("src/managers/input/Tablets.cpp"): "ad276a2e23f8792ff5f2fb43944a4c3c40e7218e77f66047dd754417a6bbc90f",
        Path("src/managers/input/Touch.cpp"): "69a5459b254751a28d2f6df1afb03781ea9b4086a43078d8275c9b165f5e3f7e",
        Path("src/devices/Mouse.cpp"): "aebdf0fd1765d25d943cc79f4e5bbdce09e0c437ae3e9cdc0985195df54a5470",
        Path("src/managers/input/trackpad/TrackpadGestures.cpp"): "5e23524d8a6a0778fc8199173978488e4b54e70611ec354b036a99ae6b9610b7",
        Path("src/managers/input/trackpad/GestureTypes.hpp"): "87ef4e338afd1ba8f16f2d4a9c03dfccedef3da0f5cdb4464da0c3ec903f23c1",
        Path("src/managers/input/trackpad/TrackpadGestures.hpp"): "f0e2a20af8b5c7e41d1c7a7e3bf650b9a6ac89f883adeefca0f39ff060cdce09",
        Path("src/managers/input/trackpad/gestures/ITrackpadGesture.cpp"): "c2ce8e076a7bc326316d3c9ef0221e5ad7699fe87669fa9188a2fc872c1ebf5e",
        Path("src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"): "3e89e749eeebee1050907830432221963e4ad9c2e7b76cebf784ef993e445890",
        Path("src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.cpp"): "2f12d2b682af060e23a8d4e0aee1ee1366cd9dce9bf05f8e287b52740cb14ad0",
        Path("src/managers/input/trackpad/gestures/ResizeGesture.cpp"): "9a0b156e42f0938614395a198b382a3388056b2b211c1679658f24686d9a372a",
        Path("src/managers/input/trackpad/gestures/MoveGesture.cpp"): "617e22260d144832639fa811e24025c52b7404955b1f7879f5d0af38e44f4f9c",
        Path("src/managers/input/trackpad/gestures/CloseGesture.cpp"): "9da24b306cd803a5517cee5527ae8a1f3ee3d313b09876d43434b6be7607ef32",
        Path("src/managers/input/trackpad/gestures/ScrollMoveGesture.cpp"): "f3c1434ac10e187102a3c5c5bfff759f04d990f0cc82fd46914a8244aac500ed",
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
        Path("src/protocols/PrimarySelection.cpp"): "f4989a7770e13d351a7db362d0539e8d96d23b9f7c524d278d1e529f68e0391b",
        Path("src/devices/IHID.cpp"): "a58a4b44e5947e82ab29cab414d92511f953a0feb87b662516496a869f1c7ee3",
        Path("src/devices/IPointer.cpp"): "ffb4ca03e3d4cdd0d7f615d27c0c01b42d360206e471bc66d50322f87ac36ccc",
        Path("src/devices/IKeyboard.cpp"): "8a800efe9baa7f375a3781744372eecd98a88be2c4bedaae96c90a1102a1aa3d",
        Path("src/devices/VirtualKeyboard.cpp"): "bc6d984a4c9e62313501c1cdb4ace3659c16d3b77e029a3c082a77f21500ed86",
        Path("src/devices/VirtualPointer.cpp"): "b1425a95edb425a229ffd61b266f9c02c8882a5b3dee6cd4d3b6d0e5427b516f",
        Path("src/protocols/VirtualKeyboard.cpp"): "43cbf7b39bf0910df6d933cef95f029fa336d5b432b0202280e6fc40d8db2f50",
        Path("src/protocols/VirtualPointer.cpp"): "a07b12a920fe46beab3840245b818b474a376aa736b566069014d8eb4fd82fb1",
        Path("src/protocols/InputCapture.cpp"): "d034a7f2dd7c5010bfb52cc4db7717119a6c1482efe2dfeeae9da2fadab1a0fb",
        Path("src/pointer/PointerManager.cpp"): "e7fb86bc7cd0420e0a225dfc8ca43da561638416380d398f67a200849e738981",
        Path("src/pointer/PointerManager.hpp"): "a18050d377c96b6bcee1d3d591b5573515473a507aaa464de611e29124bfbf88",
        Path("src/pointer/PointerController.cpp"): "4aef766cf4205222ef143b1432d2598c7beab1527aee5e19aa58f17a1890d899",
        Path("src/pointer/PointerController.hpp"): "260fa1c172d0f469d420dc1b7e5311c0dfec0777fa6a6cf08af69ea854fdbb55",
        Path("src/render/decorations/CHyprGroupBarDecoration.cpp"): "39cb87fc2b28c81433bfd34d3900e58c2c58f4f336be728e07461a8f16c095e6",
        Path("src/state/MonitorQueryCore.cpp"): "19e76f03b679d151afa0d55a03c42071ef30578f889df1d15167126cc7c350bc",
        Path("src/state/MonitorQueryCore.hpp"): "829581d4aeb084cdd30de9c8c8e310ead38d357061df4e82a8279d634870e0f4",
        Path("src/state/WorkspaceQueryCore.cpp"): "b810515fb0720d1fe6b3e3e1e5d5ebfa57e5503e83840337a614b0862860b3d7",
        Path("src/state/WorkspaceQueryCore.hpp"): "698a1814b9d47ef08fe8ed54e723a839d73b9191335f3ba879f2e8389b3025a2",
        Path("src/state/WorkspacePlacementController.cpp"): "cea25fc71ef2d54e2a4eec3e6d04fa7abfbdd25bdafbe2d742026e37dd438420",
    },
    "0.56.0": {
        Path("VERSION"): "3fea81d177087f5d3380893d95b86573a803b34ed45419ec381bdd776f526cee",
        REGISTRY_PATH: "a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754",
        Path("src/Compositor.cpp"): "0c08837447683a23a62aefbfcc8332881e7f8e49d3a87f41aba856df29cca9fb",
        Path("src/config/lua/ConfigManager.cpp"): "bf295818d6ad5a1f01aa708a6843a968b9cbb14228482421bbe0e4e5b26600ed",
        Path("src/config/shared/actions/ConfigActions.cpp"): "29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab",
        Path("src/main.cpp"): "98e5752cd485378c58c2a8cb0ba89265eaea27144925ea059e5564917dd3b645",
        Path("src/debug/HyprCtl.cpp"): "17dddd63fca2d367f81eec5f0b6785cc7c971998fab5b786f908905b2327743d",
        Path("src/output/Monitor.cpp"): "5f6dc48a7cb6cda7b1c0859cbce72023c0102d865d7a67f5210b59587f2b5801",
        Path("src/desktop/Workspace.cpp"): "413bc18a0d17b1bafc27f956a7103b301fa382088449bbb2422e123255e9fcec",
        Path("src/config/shared/workspace/WorkspaceRuleManager.cpp"): "06e8dcbaea83e51710bd372a8f264ad3d07bbe061b9720bbe456811451c02d10",
        Path("src/config/shared/workspace/WorkspaceRule.cpp"): "96e1cc448847b03e7f48dc7e1c2bc4ed6c192fcf7c11c608b33f5760f7788f96",
        Path("src/layout/space/Space.cpp"): "8ddcd4dd0a90bd59d4fa21f95f53444419188846a3a217284f0d6b008875d376",
        Path("src/managers/fullscreen/FullscreenController.hpp"): "7f3585f23e4d756f3f165670a604de39717eb3fd704114e2a230bdd6ffba2378",
        Path("src/managers/fullscreen/FullscreenController.cpp"): "581d92ef70588fce181b4b87a04e37f6de7b4777c24ad7fee34b21f941b706b0",
        Path("src/managers/fullscreen/handler/FullscreenHandler.cpp"): "5bfed3aa05f2e6f013e7776efaa754e3e107aa7688eb5d523bdbdfa87f51bc85",
        Path("src/managers/input/InputManager.cpp"): "030d4f734e1303b5ae92c5bbfbdf8ece1bc7debecbee7abaf28f1e53f6fca966",
        Path("src/protocols/InputCapture.cpp"): "d034a7f2dd7c5010bfb52cc4db7717119a6c1482efe2dfeeae9da2fadab1a0fb",
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

ANIMATION_TREE_NODES: dict[str, tuple[tuple[str, str | None], ...]] = {
    "0.55.0": (
        ("__internal_fadeCTM", None), ("global", None),
        ("windows", "global"), ("layers", "global"), ("fade", "global"),
        ("border", "global"), ("borderangle", "global"),
        ("workspaces", "global"), ("zoomFactor", "global"),
        ("monitorAdded", "global"), ("layersIn", "layers"),
        ("layersOut", "layers"), ("windowsIn", "windows"),
        ("windowsOut", "windows"), ("windowsMove", "windows"),
        ("fadeIn", "fade"), ("fadeOut", "fade"),
        ("fadeSwitch", "fade"), ("fadeShadow", "fade"),
        ("fadeGlow", "fade"), ("fadeDim", "fade"),
        ("fadeLayers", "fade"), ("fadeLayersIn", "fadeLayers"),
        ("fadeLayersOut", "fadeLayers"), ("fadePopups", "fade"),
        ("fadePopupsIn", "fadePopups"),
        ("fadePopupsOut", "fadePopups"), ("fadeDpms", "fade"),
        ("workspacesIn", "workspaces"),
        ("workspacesOut", "workspaces"),
        ("specialWorkspace", "workspaces"),
        ("specialWorkspaceIn", "specialWorkspace"),
        ("specialWorkspaceOut", "specialWorkspace"),
    ),
    "0.56.1": (
        ("__internal_fadeCTM", None), ("global", None),
        ("windows", "global"), ("layers", "global"), ("fade", "global"),
        ("border", "global"), ("borderangle", "global"),
        ("shadowangle", "global"), ("glowangle", "global"),
        ("workspaces", "global"), ("zoomFactor", "global"),
        ("monitorAdded", "global"), ("layersIn", "layers"),
        ("layersOut", "layers"), ("windowsIn", "windows"),
        ("windowsOut", "windows"), ("windowsMove", "windows"),
        ("fadeIn", "fade"), ("fadeOut", "fade"),
        ("fadeSwitch", "fade"), ("fadeShadow", "fade"),
        ("fadeGlow", "fade"), ("fadeDim", "fade"),
        ("fadeLayers", "fade"), ("fadeLayersIn", "fadeLayers"),
        ("fadeLayersOut", "fadeLayers"), ("fadePopups", "fade"),
        ("fadePopupsIn", "fadePopups"),
        ("fadePopupsOut", "fadePopups"), ("fadeDpms", "fade"),
        ("workspacesIn", "workspaces"),
        ("workspacesOut", "workspaces"),
        ("specialWorkspace", "workspaces"),
        ("specialWorkspaceIn", "specialWorkspace"),
        ("specialWorkspaceOut", "specialWorkspace"),
    ),
}

ANIMATION_TREE_DEFAULTS: dict[str, tuple[tuple[str, bool, str, str], ...]] = {
    "0.55.0": (
        ("global", True, "8.f", "default"),
        ("__internal_fadeCTM", True, "5.f", "linear"),
        ("borderangle", False, "1", "default"),
    ),
    "0.56.1": (
        ("global", True, "8.f", "default"),
        ("__internal_fadeCTM", True, "5.f", "linear"),
        ("borderangle", False, "1", "default"),
        ("shadowangle", False, "1", "default"),
        ("glowangle", False, "1", "default"),
    ),
}

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

TAGGED_GESTURE_ACTION_SPELLINGS = (
    "workspace", "resize", "move", "special", "close", "float",
    "fullscreen", "cursor_zoom", "cursorZoom", "scroll_move", "unset",
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


def _assert_monitor_query_contract(
    monitor_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify the filtered monitor, workspace, and client wire contract."""
    semantic_signatures: dict[str, tuple[str, ...]] = {}
    for version in ("0.56.0", "0.56.1"):
        sources = monitor_sources[version]
        hyprctl = sources[Path("src/debug/HyprCtl.cpp")].decode("utf-8")
        monitor = sources[Path("src/output/Monitor.cpp")].decode("utf-8")
        normalized_hyprctl = re.sub(r"\s+", " ", hyprctl)
        normalized_monitor = re.sub(r"\s+", " ", monitor)

        json_template = re.search(
            r'R"#\(\{\{\n(?P<body>.*?)\n\}\},\)#"',
            hyprctl,
            flags=re.DOTALL,
        )
        if json_template is None:
            raise ValueError(
                f"Hyprland {version} monitor JSON template is missing"
            )
        fields = tuple(
            re.findall(r'^ {4}"([A-Za-z][A-Za-z0-9]*)":',
                       json_template.group("body"), flags=re.MULTILINE)
        )
        if fields != MONITOR_QUERY_JSON_FIELDS:
            raise ValueError(
                f"Hyprland {version} monitor JSON field inventory changed: "
                f"{fields!r}"
            )

        required_hyprctl_fragments = (
            '"refreshRate": {:.5f}',
            '"reserved": [{}, {}, {}, {}]',
            '"scale": {:.2f}',
            'result += std::format("\\"{}x{}@{:.2f}Hz\\",", '
            'm->pixelSize.x, m->pixelSize.y, m->refreshRate / 1000.0);',
            'escapeJSONStrings(m->m_name), '
            'escapeJSONStrings(m->m_shortDescription), '
            'escapeJSONStrings(m->m_output->make)',
            'sc<int>(m->m_reservedArea.left()), '
            'sc<int>(m->m_reservedArea.top()), '
            'sc<int>(m->m_reservedArea.right()), '
            'sc<int>(m->m_reservedArea.bottom())',
            '(m->m_output->state->state().adaptiveSync ? "true" : "false")',
            '(m->m_enabled ? "false" : "true")',
            'm->m_mirrorOf ? std::format("{}", m->m_mirrorOf->m_id) : "none"',
            'case DRM_FORMAT_XRGB2101010: return "XRGB2101010";',
            'case DRM_FORMAT_XBGR2101010: return "XBGR2101010";',
            'case DRM_FORMAT_XRGB8888: return "XRGB8888";',
            'case DRM_FORMAT_XBGR8888: return "XBGR8888";',
            'return "Invalid";',
            'if (vars.size() == 2 && vars[1] == "all") allMonitors = true;',
            'allMonitors ? State::monitorState()->allMonitors() '
            ': State::monitorState()->monitors()',
            '"mapped": {}, "hidden": {}, "visible": {}, "acceptsInput": {},',
            '"workspace": {{ "id": {}, "name": "{}" }}, "floating": {}, "monitor": {},',
            '"fullscreen": {}, "fullscreenClient": {}, "fullscreenHandler": "{}",',
            '"focusHistoryID": {},',
            '(w->m_isMapped ? "true" : "false"), '
            '(w->isHidden() ? "true" : "false"), '
            '(w->visible() ? "true" : "false")',
            '(sc<int>(w->m_isFloating) == 1 ? "true" : "false")',
            'sc<uint8_t>(Fullscreen::controller()->getFullscreenModes(w).internal)',
            'sc<uint8_t>(Fullscreen::controller()->getFullscreenModes(w).client)',
            'escapeJSONStrings(Fullscreen::controller()->getFullscreenHandlerNameAsString(w))',
            'getFocusHistoryID(w)',
            '"hasfullscreen": {}, "lastwindow": "0x{:x}",',
            'Fullscreen::controller()->hasFullscreen(w) ? "true" : "false"',
            'rc<uintptr_t>(PLASTW.get())',
        )
        missing_hyprctl = tuple(
            fragment for fragment in required_hyprctl_fragments
            if fragment not in normalized_hyprctl
        )
        if missing_hyprctl:
            raise ValueError(
                f"Hyprland {version} monitor query semantics changed near "
                f"{missing_hyprctl!r}"
            )

        required_monitor_fragments = (
            'm_shortDescription = trim(std::format("{} {} {}", '
            'm_output->make, m_output->model, m_output->serial));',
            "std::erase(m_shortDescription, ',');",
        )
        missing_monitor = tuple(
            fragment for fragment in required_monitor_fragments
            if fragment not in normalized_monitor
        )
        if missing_monitor:
            raise ValueError(
                f"Hyprland {version} monitor identity semantics changed near "
                f"{missing_monitor!r}"
            )

        semantic_signatures[version] = (
            *fields,
            *required_hyprctl_fragments,
            *required_monitor_fragments,
        )

    if semantic_signatures["0.56.0"] != semantic_signatures["0.56.1"]:
        raise ValueError(
            "Hyprland 0.56.0 and 0.56.1 monitor query contracts diverged"
        )


def _require_ordered_cpp_fragments(
    source: str,
    version: str,
    path: Path,
    fragments: tuple[str, ...],
    contract: str,
) -> None:
    """Require reviewed C++ statements in order after whitespace normalization."""
    normalized = re.sub(r"\s+", " ", source)
    offset = 0
    for fragment in fragments:
        found = normalized.find(fragment, offset)
        if found < 0:
            raise ValueError(
                f"Hyprland {version} {contract} semantics changed in {path} "
                f"near {fragment!r}"
            )
        offset = found + len(fragment)


def _maximize_contract_requirements() -> dict[Path, tuple[str, ...]]:
    return {
        Path("src/desktop/Workspace.cpp"): (
            "if (cur == 'f') {",
            'if (!prop.starts_with("f[") || !prop.ends_with("]")) {',
            "FSSTATE = std::stoi(prop);",
            "case 0: // fullscreen full",
            "getFullscreenModes(m_self.lock()).internal != Fullscreen::FSMODE_FULLSCREEN",
            "case 1: // maximized",
            "getFullscreenModes(m_self.lock()).internal != Fullscreen::FSMODE_MAXIMIZED",
        ),
        Path("src/config/shared/workspace/WorkspaceRuleManager.cpp"): (
            "CWorkspaceRule mergedRule;",
            "for (auto const& rule : m_rules) {",
            "if (!workspace->matchesStaticSelector(rule->m_workspaceString))",
            "mergedRule.mergeLeft(*rule);",
            "return mergedRule;",
        ),
        Path("src/config/shared/workspace/WorkspaceRule.cpp"): (
            "void CWorkspaceRule::mergeLeft(const CWorkspaceRule& other) {",
            "if (other.m_gapsOut.has_value()) m_gapsOut = other.m_gapsOut;",
        ),
        Path("src/layout/space/Space.cpp"): (
            "auto gapsOut = WORKSPACERULE.m_gapsOut.value_or(*PGAPSOUT);",
            "auto gapsFloat = WORKSPACERULE.m_gapsOut.value_or(*PFLOATGAPS);",
            "reservedFloatGaps.applyip(floatWorkArea);",
            "reservedGaps.applyip(workArea);",
            "m_workArea = workArea;",
            "m_floatingWorkArea = floatWorkArea;",
            "return floating ? m_floatingWorkArea : m_workArea;",
        ),
        Path("src/managers/fullscreen/FullscreenController.hpp"): (
            "enum eFullscreenMode : int8_t { FSMODE_NONE = 0, FSMODE_MAXIMIZED, FSMODE_FULLSCREEN, };",
            "struct SFullscreenMode { eFullscreenMode internal = FSMODE_NONE; eFullscreenMode client = FSMODE_NONE; };",
        ),
        Path("src/managers/fullscreen/FullscreenController.cpp"): (
            "return hasFullscreen(activeWorkspace) && getFullscreenModes(activeWorkspace).internal == FSMODE_FULLSCREEN;",
            'case FULLSCREEN_HANDLER_DEFAULT: return "default";',
            'case FULLSCREEN_HANDLER_SCROLLING: return "scrolling";',
            'g_pEventManager->postEvent(SHyprIPCEvent{.event = "fullscreen",',
        ),
        Path("src/managers/fullscreen/handler/FullscreenHandler.cpp"): (
            "if (TARGET_INTERNAL_MODE == FSMODE_FULLSCREEN) {",
            "const CBox MONBOX = MONITOR->logicalBox();",
            "LAYOUT_TARGET->setPositionGlobal(MONBOX);",
            "} else if (TARGET_INTERNAL_MODE == FSMODE_MAXIMIZED) {",
            "const CBox WORKAREA = WORKSPACE->m_space->workArea(target->floating());",
            "LAYOUT_TARGET->setPositionGlobal(WORKAREA);",
            "if (TARGET_INTERNAL_MODE == FSMODE_FULLSCREEN) {",
            "const CBox MONBOX = MONITOR->logicalBox();",
            "LAYOUT_TARGET->setPositionGlobal(MONBOX);",
            "} else if (TARGET_INTERNAL_MODE == FSMODE_MAXIMIZED) {",
            "const auto WORK_AREA = WORKSPACE->m_space->workArea(FS_TARGET->floating());",
            "LAYOUT_TARGET->setPositionGlobal(WORKSPACE->m_space->workArea(FS_TARGET->floating()));",
        ),
    }


def _assert_maximize_contract(
    maximize_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify the protected `f[1]` rule and maximize geometry for both patches."""
    semantic_signatures: dict[str, tuple[tuple[str, tuple[str, ...]], ...]] = {}
    requirements = _maximize_contract_requirements()

    if set(requirements) != set(MAXIMIZE_SOURCE_PATHS):
        raise ValueError("maximize semantic assertion inventory is incomplete")

    for version in ("0.56.0", "0.56.1"):
        sources = maximize_sources.get(version, {})
        if set(sources) != set(MAXIMIZE_SOURCE_PATHS):
            raise ValueError(
                f"Hyprland {version} maximize source inventory is incomplete"
            )
        for path in MAXIMIZE_SOURCE_PATHS:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                requirements[path],
                "maximize",
            )
        semantic_signatures[version] = tuple(
            (path.as_posix(), requirements[path])
            for path in MAXIMIZE_SOURCE_PATHS
        )

    if semantic_signatures["0.56.0"] != semantic_signatures["0.56.1"]:
        raise ValueError(
            "Hyprland 0.56.0 and 0.56.1 maximize contracts diverged"
        )


def _group_behavior_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    common_window = (
        'static auto PAUTOGROUP = CConfigValue<Config::INTEGER>("group:auto_group");',
        "if (*PAUTOGROUP // auto_group enabled",
        "&& canBeGroupedInto(Desktop::focusState()->window()->m_group)",
        "Desktop::focusState()->window()->m_group->add(m_self.lock());",
        'static auto ALLOWGROUPMERGE = CConfigValue<Config::INTEGER>("group:merge_groups_on_drag");',
        "g_layoutManager->dragController()->wasDraggingWindow() && isGroup && !sc<bool>(*ALLOWGROUPMERGE)",
        "&& !disallowDragIntoGroup;",
    )
    common_drag = (
        'static auto PDRAGINTOGROUP = CConfigValue<Config::INTEGER>("group:drag_into_group");',
        "pWindow->m_group && DRAGGING_WINDOW->canBeGroupedInto(pWindow->m_group) && *PDRAGINTOGROUP == 1 && !FLOATEDINTOTILED",
        "pWindow->m_group->add(DRAGGING_WINDOW);",
    )
    common_groupbar = (
        'static auto PDRAGINTOGROUP = CConfigValue<Config::INTEGER>("group:drag_into_group");',
        'static auto PMERGEFLOATEDINTOTILEDONGROUPBAR = CConfigValue<Config::INTEGER>("group:merge_floated_into_tiled_on_groupbar");',
        'static auto PMERGEGROUPSONGROUPBAR = CConfigValue<Config::INTEGER>("group:merge_groups_on_groupbar");',
        "(*PDRAGINTOGROUP != 1 && *PDRAGINTOGROUP != 2)",
        "(FLOATEDINTOTILED && !*PMERGEFLOATEDINTOTILEDONGROUPBAR)",
        "(!*PMERGEGROUPSONGROUPBAR && pDraggedWindow->m_group)",
        "m_window->m_group->add(pDraggedWindow);",
    )

    return {
        "0.55.0": {
            Path("src/desktop/view/Window.cpp"): common_window,
            Path("src/desktop/view/Group.cpp"): (
                'static auto INSERT_AFTER_CURRENT = CConfigValue<Config::INTEGER>("group:insert_after_current");',
                "if (*INSERT_AFTER_CURRENT) {",
                "m_windows.insert(m_windows.begin() + m_current + 1, w);",
                "m_current++;",
                "} else {",
                "m_windows.emplace_back(w);",
                "m_current = m_windows.size() - 1;",
            ),
            Path("src/layout/supplementary/DragController.cpp"): common_drag,
            Path("src/render/decorations/CHyprGroupBarDecoration.cpp"): common_groupbar,
            Path("src/config/shared/actions/ConfigActions.cpp"): (
                "ActionResult Actions::moveToWorkspace(PHLWORKSPACE ws, bool silent, std::optional<PHLWINDOW> w) {",
                "if (silent) {",
                "g_pCompositor->moveWindowToWorkspaceSafe(window, ws);",
                "} else {",
                "g_pCompositor->moveWindowToWorkspaceSafe(window, ws);",
                'static auto BFOCUSREMOVEDWINDOW = CConfigValue<Config::INTEGER>("group:focus_removed_window");',
                "pWindow->m_group->remove(pWindow, direction);",
                "if (*BFOCUSREMOVEDWINDOW || !group) {",
                "Desktop::focusState()->fullWindowFocus(pWindow, Desktop::FOCUS_REASON_KEYBIND);",
                "} else {",
                "Desktop::focusState()->fullWindowFocus(group->current(), Desktop::FOCUS_REASON_KEYBIND);",
            ),
            Path("src/Compositor.cpp"): (
                "void CCompositor::moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) {",
                "const PHLWINDOW pFirstWindowOnWorkspace = pWorkspace->getFirstWindow();",
                "const int visibleWindowsOnWorkspace = pWorkspace->getWindows(true, std::nullopt, true);",
                "pWindow->moveToWorkspace(pWorkspace);",
                'static auto PGROUPONMOVETOWORKSPACE = CConfigValue<Config::INTEGER>("group:group_on_movetoworkspace");',
                "if (*PGROUPONMOVETOWORKSPACE && visibleWindowsOnWorkspace == 1 && pFirstWindowOnWorkspace && pFirstWindowOnWorkspace != pWindow && pFirstWindowOnWorkspace->m_group &&",
                "pWindow->canBeGroupedInto(pFirstWindowOnWorkspace->m_group)) {",
                "pFirstWindowOnWorkspace->m_group->add(pWindow);",
            ),
        },
        "0.56.1": {
            Path("src/desktop/view/Window.cpp"): common_window,
            Path("src/desktop/view/Group.cpp"): (
                'static auto INSERT_AFTER_CURRENT = CConfigValue<Config::INTEGER>("group:insert_after_current");',
                "if (index) {",
                "} else if (*INSERT_AFTER_CURRENT) {",
                "m_windows.insert(m_windows.begin() + m_current + 1, w);",
                "m_current++;",
                "} else {",
                "m_windows.emplace_back(w);",
                "m_current = m_windows.size() - 1;",
            ),
            Path("src/layout/supplementary/DragController.cpp"): common_drag,
            Path("src/render/decorations/CHyprGroupBarDecoration.cpp"): common_groupbar,
            Path("src/config/shared/actions/ConfigActions.cpp"): (
                "ActionResult Actions::moveToWorkspace(PHLWORKSPACE ws, bool silent, std::optional<PHLWINDOW> w) {",
                "if (silent) {",
                "Desktop::globalWindowController()->moveWindowToWorkspace(window, ws);",
                "} else {",
                "Desktop::globalWindowController()->moveWindowToWorkspace(window, ws);",
                'static auto BFOCUSREMOVEDWINDOW = CConfigValue<Config::INTEGER>("group:focus_removed_window");',
                "pWindow->m_group->remove(pWindow, direction);",
                "if (*BFOCUSREMOVEDWINDOW || !group) {",
                "Desktop::focusState()->fullWindowFocus(pWindow, Desktop::FOCUS_REASON_KEYBIND);",
                "} else {",
                "Desktop::focusState()->fullWindowFocus(group->current(), Desktop::FOCUS_REASON_KEYBIND);",
            ),
            Path("src/desktop/state/GlobalWindowController.cpp"): (
                "void CGlobalWindowController::moveWindowToWorkspace(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) const {",
                "const PHLWINDOW pFirstWindowOnWorkspace = pWorkspace->getFirstWindow();",
                "const int visibleWindowsOnWorkspace = pWorkspace->getWindowCount(true, std::nullopt, true);",
                "pWindow->moveToWorkspace(pWorkspace);",
                'static auto PGROUPONMOVETOWORKSPACE = CConfigValue<Config::INTEGER>("group:group_on_movetoworkspace");',
                "if (*PGROUPONMOVETOWORKSPACE && visibleWindowsOnWorkspace == 1 && pFirstWindowOnWorkspace && pFirstWindowOnWorkspace != pWindow && pFirstWindowOnWorkspace->m_group &&",
                "pWindow->canBeGroupedInto(pFirstWindowOnWorkspace->m_group)) {",
                "pFirstWindowOnWorkspace->m_group->add(pWindow);",
            ),
        },
    }


def _assert_group_behavior_contract(
    group_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify the authored group behavior and its cross-option dependencies."""
    requirements_by_version = _group_behavior_contract_requirements()
    if set(requirements_by_version) != set(GROUP_BEHAVIOR_SOURCE_PATHS):
        raise ValueError("window-group behavior patch inventory is incomplete")

    for version, expected_paths in GROUP_BEHAVIOR_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} window-group semantic inventory is incomplete"
            )
        sources = group_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} window-group source inventory is incomplete"
            )
        reviewed_fragments = " ".join(
            fragment
            for path in expected_paths
            for fragment in requirements[path]
        )
        for option_path in GROUP_BEHAVIOR_OPTION_PATHS:
            if f'"{option_path}"' not in reviewed_fragments:
                raise ValueError(
                    f"Hyprland {version} has no semantic assertion for {option_path}"
                )
        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                requirements[path],
                "window-group behavior",
            )


def _appearance_behavior_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    reload_requirements = (
        "// phase 2: syntax is valid, reset and load.",
        "for (const auto& v : m_configValues) {",
        "v.second->reset();",
        'if (guardedPCall(0, 0, 1, LUA_TIMEOUT_CONFIG_RELOAD_MS, "config reload") != LUA_OK) {',
        "postConfigReload();",
        "void CConfigManager::postConfigReload() {",
        "Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);",
    )
    window_prefix = (
        "void CWindow::updateDecorationValues() {",
        'static auto PINACTIVEALPHA = CConfigValue<Config::FLOAT>("decoration:inactive_opacity");',
        'static auto PACTIVEALPHA = CConfigValue<Config::FLOAT>("decoration:active_opacity");',
        'static auto PFULLSCREENALPHA = CConfigValue<Config::FLOAT>("decoration:fullscreen_opacity");',
        'static auto PGLOW = CConfigValue<Config::INTEGER>("decoration:glow:enabled");',
        'static auto PDIMSTRENGTH = CConfigValue<Config::FLOAT>("decoration:dim_strength");',
        'static auto PDIMENABLED = CConfigValue<Config::INTEGER>("decoration:dim_inactive");',
        'static auto PDIMMODAL = CConfigValue<Config::INTEGER>("decoration:dim_modal");',
    )
    window_dim = (
        "float goalDim = 1.F;",
        "if (m_self == Desktop::focusState()->window() || m_ruleApplicator->noDim().valueOrDefault() || !*PDIMENABLED)",
        "goalDim = 0;",
        "else",
        "goalDim = *PDIMSTRENGTH;",
        "if (IS_SHADOWED_BY_MODAL && *PDIMMODAL)",
        "goalDim += (1.F - goalDim) / 2.F;",
        "*m_dimPercent = goalDim;",
    )
    rounding_window_requirements = (
        "bool CWindow::isInCurvedCorner(double x, double y) {",
        "const int ROUNDING      = rounding();",
        "const int ROUNDINGPOWER = roundingPower();",
        "if (getRealBorderSize() >= ROUNDING)",
        "return false;",
        "return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);",
        "return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);",
        "return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);",
        "return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);",
        "float CWindow::rounding() {",
        'static auto PROUNDING      = CConfigValue<Config::INTEGER>("decoration:rounding");',
        'static auto PROUNDINGPOWER = CConfigValue<Config::FLOAT>("decoration:rounding_power");',
        "float       roundingPower = m_ruleApplicator->roundingPower().valueOr(*PROUNDINGPOWER);",
        "float       rounding      = m_ruleApplicator->rounding().valueOr(*PROUNDING) * (roundingPower / 2.0); /* Make perceived roundness consistent. */",
        "return rounding;",
        "float CWindow::roundingPower() {",
        'static auto PROUNDINGPOWER = CConfigValue<Config::FLOAT>("decoration:rounding_power");',
        "return m_ruleApplicator->roundingPower().valueOr(std::clamp(*PROUNDINGPOWER, 1.F, 10.F));",
    )
    window_map_prefix = (
        "void CWindow::mapWindow() {",
        'static auto PINACTIVEALPHA = CConfigValue<Config::FLOAT>("decoration:inactive_opacity");',
        'static auto PACTIVEALPHA = CConfigValue<Config::FLOAT>("decoration:active_opacity");',
        'static auto PDIMSTRENGTH = CConfigValue<Config::FLOAT>("decoration:dim_strength");',
    )
    window_map_values = (
        "alpha(WINDOW_ALPHA_ACTIVE)->setValueAndWarp(*PACTIVEALPHA); m_dimPercent->setValueAndWarp(m_ruleApplicator->noDim().valueOrDefault() ? 0.f : *PDIMSTRENGTH); } else { alpha(WINDOW_ALPHA_ACTIVE)->setValueAndWarp(*PINACTIVEALPHA); m_dimPercent->setValueAndWarp(0); }",
    )
    renderer_mapped_popup = (
        "data.xray = shouldUseNewBlurOptimizations(nullptr, pWindow);",
        'static CConfigValue PBLURIGNOREA = CConfigValue<Config::FLOAT>("decoration:blur:popups_ignorealpha");',
        "renderdata.blur = shouldBlur(pWindow->m_popupHead);",
        "if (renderdata.blur) {",
        "renderdata.discardMode |= DISCARD_ALPHA;",
        "renderdata.discardOpacity = *PBLURIGNOREA;",
    )
    renderer_preblur = (
        "bool IHyprRenderer::preBlurQueued(PHLMONITORREF pMonitor) {",
        'static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");',
        'static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");',
        "if (!pMonitor)",
        "return false;",
        "return m_renderData.pMonitor->m_blurFBDirty && *PBLURNEWOPTIMIZE && *PBLUR && m_renderData.pMonitor->m_blurFBShouldRender;",
    )
    renderer_ime = (
        "void IHyprRenderer::renderIMEPopup(CInputPopup* pPopup, PHLMONITOR pMonitor, const Time::steady_tp& time) {",
        'static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");',
        'static auto PBLURIMES = CConfigValue<Config::INTEGER>("decoration:blur:input_methods");',
        'static auto PBLURIGNOREA = CConfigValue<Config::FLOAT>("decoration:blur:input_methods_ignorealpha");',
        "renderdata.blur = *PBLURIMES && *PBLUR;",
        "if (renderdata.blur) {",
        "renderdata.discardMode |= DISCARD_ALPHA;",
        "renderdata.discardOpacity = *PBLURIGNOREA;",
    )
    renderer_optimization = (
        "bool IHyprRenderer::shouldUseNewBlurOptimizations(PHLLS pLayer, PHLWINDOW pWindow) {",
        'static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");',
        'static auto PBLURXRAY = CConfigValue<Config::INTEGER>("decoration:blur:xray");',
        "if (!getBlurTexture(m_renderData.pMonitor))",
        "return false;",
        "if (pWindow && pWindow->m_ruleApplicator->xray().hasValue() && !pWindow->m_ruleApplicator->xray().valueOrDefault())",
        "return false;",
        "if (pLayer && pLayer->m_ruleApplicator->xray().valueOrDefault() == 0)",
        "return false;",
        "if ((*PBLURNEWOPTIMIZE && pWindow && !pWindow->m_isFloating && !pWindow->onSpecialWorkspace()) || *PBLURXRAY)",
        "return true;",
        "if ((pLayer && pLayer->m_ruleApplicator->xray().valueOrDefault() == 1) || (pWindow && pWindow->m_ruleApplicator->xray().valueOrDefault()))",
        "return true;",
    )
    renderer_popup_gate = (
        "bool IHyprRenderer::shouldBlur(WP<Desktop::View::CPopup> p) {",
        'static CConfigValue PBLURPOPUPS = CConfigValue<Config::INTEGER>("decoration:blur:popups");',
        'static CConfigValue PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");',
        "return *PBLURPOPUPS && *PBLUR;",
    )
    renderer_cache_queue = (
        "if (preBlurQueued(pMonitor))",
        "m_renderPass.add(makeUnique<CPreBlurElement>());",
    )
    renderer_cache_recompute = (
        "SP<ITexture> IHyprRenderer::blurMainFramebuffer(float a, CRegion* originalDamage) {",
        "auto guard = bindTempFB(m_renderData.currentFB);",
        "return blurFramebuffer(m_renderData.currentFB, a, originalDamage);",
        "void IHyprRenderer::preBlurForCurrentMonitor(CRegion* fakeDamage) {",
        "const auto blurredTex = blurMainFramebuffer(1, fakeDamage);",
        "auto guard = bindTempFB(m_renderData.pMonitor->resources()->m_blurFB);",
        "const auto SAVE_TRANSFORM = blurredTex->m_transform;",
        "blurredTex->m_transform = Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform));",
        "draw(CClearPassElement::SClearData{{0, 0, 0, 0}});",
        "pushMonitorTransformEnabled(true);",
        ".tex = blurredTex,",
        ".box = CBox{0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y},",
        ".damage = *fakeDamage,",
        "*fakeDamage);",
        "popMonitorTransformEnabled();",
        "blurredTex->m_transform = SAVE_TRANSFORM;",
    )
    renderer_055 = (
        "void IHyprRenderer::renderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor, const Time::steady_tp& time, bool decorate, eRenderPassMode mode, bool ignorePosition, bool standalone) {",
        'static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");',
        "renderdata.rounding      = standalone || renderdata.dontRound ? 0 : pWindow->rounding() * pMonitor->m_scale;",
        "renderdata.roundingPower = standalone || renderdata.dontRound ? 2.0f : pWindow->roundingPower();",
        "if (*PDIMAROUND && pWindow->m_ruleApplicator->dimAround().valueOrDefault() && !m_bRenderingSnapshot && mode != RENDER_PASS_POPUP) {",
        "data.color = CHyprColor(0, 0, 0, *PDIMAROUND * fullAlpha);",
        *renderer_mapped_popup,
        *renderer_preblur,
        "void IHyprRenderer::renderLayer(PHLLS pLayer, PHLMONITOR pMonitor, const Time::steady_tp& time, bool popups, bool lockscreen) {",
        'static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");',
        "if (*PDIMAROUND && pLayer->m_ruleApplicator->dimAround().valueOrDefault() && !m_bRenderingSnapshot && !popups) {",
        "data.color = CHyprColor(0, 0, 0, *PDIMAROUND * pLayer->m_alpha->value());",
        *renderer_ime,
        "void IHyprRenderer::renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time, const Vector2D& translate, const float& scale) {",
        'static auto PDIMSPECIAL = CConfigValue<Config::FLOAT>("decoration:dim_special");',
        'static auto PBLURSPECIAL = CConfigValue<Config::INTEGER>("decoration:blur:special");',
        'static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");',
        *renderer_cache_queue,
        "if UNLIKELY (pMonitor->m_specialFade->value() != 0.F) {",
        "if (*PDIMSPECIAL != 0.f) {",
        "data.color = CHyprColor(0, 0, 0, *PDIMSPECIAL * (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS));",
        "if (*PBLURSPECIAL && *PBLUR) {",
        "data.blur = true;",
        "data.blurA = (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS);",
        *renderer_optimization,
        *renderer_cache_recompute,
        "void IHyprRenderer::renderSnapshot(PHLWINDOW pWindow) {",
        'static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");',
        "if (*PDIMAROUND && pWindow->m_ruleApplicator->dimAround().valueOrDefault()) {",
        "CHyprColor(0, 0, 0, *PDIMAROUND * pWindow->alphaValue(WINDOW_ALPHA_FADE) * pWindow->alphaValue(WINDOW_ALPHA_FULLSCREEN) * pWindow->alphaValue(WINDOW_ALPHA_LAYOUT));",
        "data.round         = pWindow->rounding();",
        "data.roundingPower = pWindow->roundingPower();",
        "void IHyprRenderer::renderSnapshot(WP<Desktop::View::CPopup> popup) {",
        'static CConfigValue PBLURIGNOREA = CConfigValue<Config::FLOAT>("decoration:blur:popups_ignorealpha");',
        "const bool SHOULD_BLUR = shouldBlur(popup);",
        "data.blur = SHOULD_BLUR;",
        "data.blockBlurOptimization = SHOULD_BLUR;",
        "if (SHOULD_BLUR) {",
        "if (const auto PLAYER = popup->layerOwner(); PLAYER && PLAYER->m_ruleApplicator->ignoreAlpha().hasValue())",
        "data.ignoreAlpha = std::max(PLAYER->m_ruleApplicator->ignoreAlpha().valueOrDefault(), 0.01F);",
        "else",
        "data.ignoreAlpha = std::max(*PBLURIGNOREA, 0.01F);",
        *renderer_popup_gate,
    )
    renderer_056 = (
        "void IHyprRenderer::renderWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor, const Time::steady_tp& time, bool decorate, eRenderPassMode mode, bool ignorePosition, bool standalone) {",
        'static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");',
        "renderdata.rounding      = standalone || renderdata.dontRound ? 0 : pWindow->rounding() * pMonitor->m_scale;",
        "renderdata.roundingPower = standalone || renderdata.dontRound ? 2.0f : pWindow->roundingPower();",
        "if (*PDIMAROUND && pWindow->m_ruleApplicator->dimAround().valueOrDefault() && !m_bRenderingSnapshot && mode != RENDER_PASS_POPUP) {",
        "data.color = CHyprColor(0, 0, 0, *PDIMAROUND * fullAlpha);",
        *renderer_mapped_popup[:1],
        ".blurRoundingPower = renderdata.roundingPower,",
        *renderer_mapped_popup[1:],
        *renderer_preblur,
        "void IHyprRenderer::renderLayer(PHLLS pLayer, PHLMONITOR pMonitor, const Time::steady_tp& time, bool popups, bool lockscreen) {",
        'static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");',
        "if (*PDIMAROUND && pLayer->m_ruleApplicator->dimAround().valueOrDefault() && !m_bRenderingSnapshot && !popups) {",
        "data.color = CHyprColor(0, 0, 0, *PDIMAROUND * pLayer->alpha()[LS_ALPHA_FADE]->value());",
        *renderer_ime,
        "void IHyprRenderer::renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time, const Vector2D& translate, const float& scale) {",
        *renderer_cache_queue,
        "if UNLIKELY (pMonitor->m_specialDim->value() != 0.F) {",
        "data.color = CHyprColor(0, 0, 0, pMonitor->m_specialDim->value());",
        "if UNLIKELY (pMonitor->m_specialBlur->value() != 0.F) {",
        "data.blur = true;",
        "data.blurA = pMonitor->m_specialBlur->value();",
        *renderer_optimization,
        *renderer_cache_recompute,
        "if (EFFECTS.preBlur) {",
        "data.round         = EFFECTS.preBlur->round;",
        "data.roundingPower = EFFECTS.preBlur->roundingPower;",
        *renderer_popup_gate,
    )
    opengl_shader_inventory = (
        '"gain.glsl",',
        '"blurprepare.glsl",',
        '"blur1.glsl",',
        '"blurFinish.glsl",',
        "// order matters, see ePreparedFragmentShader",
        "const std::array<std::string, SH_FRAG_LAST> FRAG_SHADERS = {",
        '"ext.frag", "blur1.frag", "blur2.frag", "blurprepare.frag",',
        '"blurfinish.frag", "shadow.frag",',
        "auto shaderLoader = makeUnique<CShaderLoader>(SHADER_INCLUDES, FRAG_SHADERS, path);",
    )
    opengl_prefix = (
        *opengl_shader_inventory,
        "void CHyprOpenGLImpl::renderRectWithDamageInternal(const CBox& box, const CHyprColor& col, const SRectRenderData& data) {",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);",
        "void CHyprOpenGLImpl::renderTexture(SP<ITexture> tex, const CBox& box, STextureRenderData data) {",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);",
        "SP<IFramebuffer> CHyprOpenGLImpl::blurFramebufferWithDamage(float a, CRegion* originalDamage, CGLFramebuffer& source) {",
        'static auto PBLURSIZE = CConfigValue<Config::INTEGER>("decoration:blur:size");',
        'static auto PBLURPASSES = CConfigValue<Config::INTEGER>("decoration:blur:passes");',
        'static auto PBLURVIBRANCY = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy");',
        'static auto PBLURVIBRANCYDARKNESS = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy_darkness");',
        "const auto BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));",
        "damage.expand(std::clamp(*PBLURSIZE, sc<int64_t>(1), sc<int64_t>(40)) * pow(2, BLUR_PASSES));",
        'static auto PBLURCONTRAST = CConfigValue<Config::FLOAT>("decoration:blur:contrast");',
        'static auto PBLURBRIGHTNESS = CConfigValue<Config::FLOAT>("decoration:blur:brightness");',
        "shader = useShader(getShaderVariant(SH_FRAG_BLURPREPARE, SH_FEAT_CM));",
        "passCMUniforms(shader, g_pHyprRenderer->workBufferImageDescription(), getDefaultImageDescription()",
        "shader = useShader(getShaderVariant(SH_FRAG_BLURPREPARE));",
        "shader->setUniformFloat(SHADER_CONTRAST, *PBLURCONTRAST);",
        "shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);",
        "auto drawPass = [&](WP<CShader> shader, ePreparedFragmentShader frag, CRegion* pDamage) {",
        "shader->setUniformFloat(SHADER_RADIUS, *PBLURSIZE * a);",
        "if (frag == SH_FRAG_BLUR1) {",
        "shader->setUniformInt(SHADER_PASSES, BLUR_PASSES);",
        "shader->setUniformFloat(SHADER_VIBRANCY, *PBLURVIBRANCY);",
        "shader->setUniformFloat(SHADER_VIBRANCY_DARKNESS, *PBLURVIBRANCYDARKNESS);",
        "auto shader = useShader(getShaderVariant(SH_FRAG_BLUR1));",
        "for (auto i = 1; i <= BLUR_PASSES; ++i) {",
        "tempDamage = damage.copy().scale(1.f / (1 << i));",
        "drawPass(shader, SH_FRAG_BLUR1, &tempDamage);",
        "shader = useShader(getShaderVariant(SH_FRAG_BLUR2));",
        "for (auto i = BLUR_PASSES - 1; i >= 0; --i) {",
        "tempDamage = damage.copy().scale(1.f / (1 << i));",
        "drawPass(shader, SH_FRAG_BLUR2, &tempDamage);",
        'static auto PBLURNOISE = CConfigValue<Config::FLOAT>("decoration:blur:noise");',
        'static auto PBLURBRIGHTNESS = CConfigValue<Config::FLOAT>("decoration:blur:brightness");',
        "shader = useShader(getShaderVariant(SH_FRAG_BLURFINISH, SH_FEAT_CM));",
        "passCMUniforms(shader, getDefaultImageDescription(), g_pHyprRenderer->workBufferImageDescription()",
        "shader = useShader(getShaderVariant(SH_FRAG_BLURFINISH));",
        "shader->setUniformFloat(SHADER_NOISE, *PBLURNOISE);",
        "shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);",
        "void CHyprOpenGLImpl::preRender(PHLMONITOR pMonitor) {",
        'static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");',
        'static auto PBLURXRAY = CConfigValue<Config::INTEGER>("decoration:blur:xray");',
        'static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");',
        "if (!*PBLURNEWOPTIMIZE || !pMonitor->m_blurFBDirty || !*PBLUR)",
        "return;",
    )
    opengl_suffix = (
        "void CHyprOpenGLImpl::renderTextureWithBlurInternal(SP<ITexture> tex, const CBox& box, const STextureRenderData& data) {",
        ".roundingPower               = data.roundingPower,",
        'static auto PBLURIGNOREOPACITY = CConfigValue<Config::INTEGER>("decoration:blur:ignore_opacity");',
        ".a = (*PBLURIGNOREOPACITY ? data.blurA : data.a * data.blurA) * data.overallA,",
        ".roundingPower  = data.roundingPower,",
        ".roundingPower  = data.roundingPower,",
    )
    opengl_rounding_tail_055 = (
        "void CHyprOpenGLImpl::renderBorder(const CBox& box, const Config::CGradientValueData& grad, SBorderRenderData data) {",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);",
        "void CHyprOpenGLImpl::renderBorder(const CBox& box, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2, float lerp, SBorderRenderData data) {",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);",
        "void CHyprOpenGLImpl::renderRoundedShadow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color, float a) {",
        'static auto PSHADOWPOWER = CConfigValue<Config::INTEGER>("decoration:shadow:render_power");',
        "const auto  SHADOWPOWER = std::clamp(sc<int>(*PSHADOWPOWER), 1, 4);",
        "const auto TOPLEFT = Vector2D(range + round, range + round);",
        "const auto BOTTOMRIGHT = Vector2D(newBox.width - (range + round), newBox.height - (range + round));",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);",
        "shader->setUniformFloat(SHADER_RANGE, range);",
        "shader->setUniformFloat(SHADER_SHADOW_POWER, SHADOWPOWER);",
        "void CHyprOpenGLImpl::renderInnerGlow(const CBox& box, int round, float roundingPower, int range, const CHyprColor& color, int glowPower, float a) {",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);",
        "shader->setUniformFloat(SHADER_RANGE, range);",
        "shader->setUniformFloat(SHADER_SHADOW_POWER, glowPower);",
    )
    opengl_rounding_tail_056 = (
        "void CHyprOpenGLImpl::renderBorder(const CBox& box, const Config::CGradientValueData& grad, SBorderRenderData data) {",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);",
        "void CHyprOpenGLImpl::renderBorder(const CBox& box, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2, float lerp, SBorderRenderData data) {",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, data.roundingPower);",
        "void CHyprOpenGLImpl::renderRoundedShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a) {",
        "renderRoundedShadow(box, round, roundingPower, range, grad, Config::CGradientValueData{}, 0.f, a);",
        "void CHyprOpenGLImpl::renderRoundedShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,",
        'static auto PSHADOWPOWER = CConfigValue<Config::INTEGER>("decoration:shadow:render_power");',
        "const auto  SHADOWPOWER = std::clamp(sc<int>(*PSHADOWPOWER), 1, 4);",
        "const auto TOPLEFT = Vector2D(range + round, range + round);",
        "const auto BOTTOMRIGHT = Vector2D(newBox.width - (range + round), newBox.height - (range + round));",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);",
        "shader->setUniformFloat(SHADER_RANGE, range);",
        "shader->setUniformFloat(SHADER_SHADOW_POWER, SHADOWPOWER);",
        "void CHyprOpenGLImpl::renderInnerGlow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, int glowPower, float a) {",
        "renderInnerGlow(box, round, roundingPower, range, grad, Config::CGradientValueData{}, 0.f, glowPower, a);",
        "void CHyprOpenGLImpl::renderInnerGlow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,",
        'static auto PGLOWPOWER = CConfigValue<Config::INTEGER>("decoration:glow:render_power");',
        "const auto  GLOWPOWER = std::clamp(sc<int>(*PGLOWPOWER), 1, 4);",
        "shader->setUniformFloat(SHADER_ROUNDING_POWER, roundingPower);",
        "shader->setUniformFloat(SHADER_RANGE, range);",
        "shader->setUniformFloat(SHADER_SHADOW_POWER, GLOWPOWER);",
    )
    opengl_055 = (
        *opengl_prefix,
        "for (auto const& w : g_pCompositor->m_windows) {",
        "if (w->m_workspace == pMonitor->m_activeWorkspace && w->visible() && w->m_isMapped && (!w->m_isFloating || *PBLURXRAY)) {",
        "if (!hasWindows)",
        "return;",
        "g_pHyprRenderer->damageMonitor(pMonitor);",
        "pMonitor->m_blurFBShouldRender = true;",
        *opengl_suffix,
        *opengl_rounding_tail_055,
    )
    opengl_056 = (
        *opengl_prefix,
        "for (auto const& w : Desktop::windowState()->windows()) {",
        "if (w->m_workspace == pMonitor->m_activeWorkspace && w->visible() && w->m_isMapped && (!w->m_isFloating || *PBLURXRAY)) {",
        "if (!hasWindows)",
        "return;",
        "g_pHyprRenderer->damageMonitor(pMonitor);",
        "pMonitor->m_blurFBShouldRender = true;",
        *opengl_suffix,
        *opengl_rounding_tail_056,
    )
    pass_tail = (
        "float CRenderPass::oneBlurRadius() {",
        'static auto PBLURSIZE = CConfigValue<Config::INTEGER>("decoration:blur:size");',
        'static auto PBLURPASSES = CConfigValue<Config::INTEGER>("decoration:blur:passes");',
        "const auto BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));",
        "return std::clamp(*PBLURSIZE, sc<int64_t>(1), sc<int64_t>(40)) * pow(2, BLUR_PASSES);",
    )
    pass_055 = (
        "if (newDamage.empty() && !el->element->undiscardable()) {",
        "el->element->needsLiveBlurCached = el->element->needsLiveBlur();",
        "el->element->needsPrecomputeBlurCached = el->element->needsPrecomputeBlur();",
        "if (el->element->disableSimplification())",
        "g_pHyprRenderer->draw(el->element, el->elementDamage);",
        *pass_tail,
    )
    pass_056 = (
        "if (newDamage.empty() && !el.element->undiscardable()) {",
        "el.element->needsLiveBlurCached = el.element->needsLiveBlur();",
        "el.element->needsPrecomputeBlurCached = el.element->needsPrecomputeBlur();",
        "if (el.element->disableSimplification())",
        "g_pHyprRenderer->draw(el.element, el.elementDamage);",
        *pass_tail,
    )
    shader_loader_requirements = (
        "SH_FRAG_EXT,",
        "SH_FRAG_BLUR1,",
        "SH_FRAG_BLUR2,",
        "SH_FRAG_BLURPREPARE,",
        "SH_FRAG_BLURFINISH,",
        "SH_FRAG_SHADOW,",
    )
    shader_loader_rounding_055 = (
        "SH_FEAT_ROUNDING = (1 << 3), // uniforms: radius, roundingPower, topLeft, fullSize; condition: radius > 0",
    )
    shader_loader_rounding_056 = (
        "SH_FEAT_ROUNDING        = (1 << 3),  // uniforms: radius, roundingPower, topLeft, fullSize; condition: radius > 0",
    )
    shader_uniform_requirements = (
        'm_uniformLocations[SHADER_ROUNDING_POWER]            = getUniform("roundingPower");',
        'm_uniformLocations[SHADER_RANGE]                     = getUniform("range");',
        'm_uniformLocations[SHADER_SHADOW_POWER]              = getUniform("shadowPower");',
        'm_uniformLocations[SHADER_CONTRAST] = getUniform("contrast");',
        'm_uniformLocations[SHADER_VIBRANCY] = getUniform("vibrancy");',
        'm_uniformLocations[SHADER_VIBRANCY_DARKNESS] = getUniform("vibrancy_darkness");',
        'm_uniformLocations[SHADER_BRIGHTNESS] = getUniform("brightness");',
        'm_uniformLocations[SHADER_NOISE] = getUniform("noise");',
    )
    gl_renderer_requirements = (
        "SP<ITexture> CHyprGLRenderer::blurFramebuffer(SP<IFramebuffer> source, float a, CRegion* originalDamage) {",
        "auto src = GLFB(source);",
        "return g_pHyprOpenGL->blurFramebufferWithDamage(a, originalDamage, *src)->getTexture();",
    )
    gl_renderer_shadow_055 = (
        "void CHyprGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) {",
        "g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, color, a);",
    )
    gl_renderer_shadow_056 = (
        "void CHyprGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) {",
        "g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, color, a);",
        "void CHyprGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,",
        "g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, grad1, grad2, lerp, a);",
    )
    element_renderer_requirements = (
        "void IElementRenderer::drawElement(WP<IPassElement> element, const CRegion& damage) {",
        "case EK_PRE_BLUR: drawPreBlur(dynamicPointerCast<CPreBlurElement>(element), damage); break;",
        "void IElementRenderer::drawPreBlur(WP<CPreBlurElement> element, const CRegion& damage) {",
        "CRegion fakeDamage{0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};",
        "draw(element, fakeDamage);",
        "m_renderData.pMonitor->m_blurFBDirty = false;",
        "m_renderData.pMonitor->m_blurFBShouldRender = false;",
        "float rounding      = m_data.rounding;",
        "float roundingPower = m_data.roundingPower;",
        "if (m_data.dontRound) {",
        "rounding      = 0;",
        "roundingPower = 2.0f;",
        ".roundingPower         = roundingPower,",
        ".roundingPower  = roundingPower,",
        ".roundingPower         = roundingPower,",
        ".roundingPower  = roundingPower,",
    )
    element_renderer_rounding_056 = (
        ".roundingPower = element->m_data.blurRoundingPower,",
    )
    gl_element_renderer_requirements = (
        "void CGLElementRenderer::draw(WP<CBorderPassElement> element, const CRegion& damage) {",
        "{.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound}",
        "{.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound}",
        "void CGLElementRenderer::draw(WP<CPreBlurElement> element, const CRegion& damage) {",
        "auto dmg = damage;",
        "g_pHyprRenderer->preBlurForCurrentMonitor(&dmg);",
        "void CGLElementRenderer::draw(WP<CRectPassElement> element, const CRegion& damage) {",
        "{.damage = &damage, .round = m_data.round, .roundingPower = m_data.roundingPower}",
        "{.round = m_data.round, .roundingPower = m_data.roundingPower, .blur = true, .blurA = m_data.blurA, .xray = m_data.xray}",
        "void CGLElementRenderer::draw(WP<CShadowPassElement> element, const CRegion& damage) {",
        "m_data.deco->render(g_pHyprRenderer->m_renderData.pMonitor.lock(), m_data.a);",
        "void CGLElementRenderer::draw(WP<CInnerGlowPassElement> element, const CRegion& damage) {",
        "void CGLElementRenderer::draw(WP<CTexPassElement> element, const CRegion& damage) {",
        ".roundingPower  = m_data.roundingPower,",
    )
    preblur_header_requirements = (
        "class CPreBlurElement : public IPassElement {",
        'virtual const char* passName() { return "CPreBlurElement"; }',
        "virtual ePassElementType type() { return EK_PRE_BLUR; };",
    )
    preblur_implementation_requirements = (
        "CPreBlurElement::CPreBlurElement() = default;",
        "bool CPreBlurElement::needsLiveBlur() { return false; }",
        "bool CPreBlurElement::needsPrecomputeBlur() { return false; }",
        "bool CPreBlurElement::disableSimplification() { return true; }",
        "bool CPreBlurElement::undiscardable() { return true; }",
    )
    blurprepare_wrapper_requirements = (
        "uniform float contrast;",
        "uniform float brightness;",
        '#include "blurprepare.glsl"',
        "blurPrepare(texture(tex, v_texcoord), contrast, brightness",
        "sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange, srcRefLuminance, sdrBrightnessMultiplier",
    )
    blurprepare_common_requirements = (
        '#include "gain.glsl"',
        "vec4 blurPrepare(vec4 pixColor, float contrast, float brightness",
        "if (sourceTF == CM_TRANSFER_FUNCTION_ST2084_PQ) {",
        "pixColor.rgb /= sdrBrightnessMultiplier;",
        "pixColor.rgb = convertMatrix * toLinearRGB(pixColor.rgb, sourceTF);",
        "pixColor = fromLinearNit(pixColor, targetTF, dstTFRange);",
        "if (contrast != 1.0)",
        "pixColor.rgb = gain(pixColor.rgb, contrast);",
        "pixColor.rgb *= max(1.0, brightness);",
        "return pixColor;",
    )
    blurfinish_wrapper_requirements = (
        "uniform float noise;",
        "uniform float brightness;",
        '#include "blurFinish.glsl"',
        "vec4 pixColor = texture(tex, v_texcoord);",
        "blurFinish(pixColor, v_texcoord, noise, brightness",
        "sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange",
    )
    blurfinish_common_requirements = (
        "float hash(vec2 p) {",
        "vec3 p3 = fract(vec3(p.xyx) * 1689.1984);",
        "p3 += dot(p3, p3.yzx + 33.33);",
        "return fract((p3.x + p3.y) * p3.z);",
        "vec4 blurFinish(vec4 pixColor, vec2 v_texcoord, float noise, float brightness",
        "float noiseHash = hash(v_texcoord);",
        "float noiseAmount = noiseHash - 0.5;",
        "pixColor.rgb += noiseAmount * noise;",
        "pixColor.rgb *= min(1.0, brightness);",
    )
    blur1_wrapper_requirements = (
        "uniform int passes;",
        "uniform float vibrancy;",
        "uniform float vibrancy_darkness;",
        '#include "blur1.glsl"',
        "fragColor = blur1(v_texcoord, tex, radius, halfpixel, passes, vibrancy, vibrancy_darkness);",
    )
    blur1_requirements = (
        "const float Pr = 0.299;",
        "const float Pg = 0.587;",
        "const float Pb = 0.114;",
        "const float a = 0.93;",
        "const float b = 0.11;",
        "const float c = 0.66;",
        "float doubleCircleSigmoid(float x, float a) {",
        "a = clamp(a, 0.0, 1.0);",
        "if (x <= a) {",
        "y = a - sqrt(a * a - x * x);",
        "y = a + sqrt(pow(1. - a, 2.) - pow(x - 1., 2.));",
        "return y;",
        "vec3 rgb2hsl(vec3 col) {",
        "float minc = min(col.r, min(col.g, col.b));",
        "float maxc = max(col.r, max(col.g, col.b));",
        "float delta = maxc - minc;",
        "float lum = (minc + maxc) * 0.5;",
        "float mul = (lum < 0.5) ? (lum) : (1.0 - lum);",
        "sat = delta / (mul * 2.0);",
        "vec3 adds = vec3(0.0, 2.0, 4.0) + vec3(green - blue, blue - red, red - green) / delta;",
        "hue += dot(adds, masks);",
        "hue /= 6.0;",
        "return vec3(hue, sat, lum);",
        "vec3 hsl2rgb(vec3 col) {",
        "xt = min(xt, 1.0);",
        "float sat2 = 2.0 * sat;",
        "float satinv = 1.0 - sat;",
        "float luminv = 1.0 - lum;",
        "float lum2m1 = (2.0 * lum) - 1.0;",
        "vec3 ct = (sat2 * xt) + satinv;",
        "if (lum >= 0.5)",
        "rgb = (luminv * ct) + lum2m1;",
        "rgb = lum * ct;",
        "return rgb;",
        "vec4 blur1(vec2 v_texcoord, sampler2D tex, float radius, vec2 halfpixel, int passes, float vibrancy, float vibrancy_darkness) {",
        "vec4 color = sum / 8.0;",
        "if (vibrancy == 0.0) {",
        "return color;",
        "float vibrancy_darkness1 = 1.0 - vibrancy_darkness;",
        "vec3 hsl = rgb2hsl(color.rgb);",
        "float perceivedBrightness = doubleCircleSigmoid(sqrt(color.r * color.r * Pr + color.g * color.g * Pg + color.b * color.b * Pb), 0.8 * vibrancy_darkness1);",
        "float b1 = b * vibrancy_darkness1;",
        "float boostBase = hsl[1] > 0.0 ? smoothstep(b1 - c * 0.5, b1 + c * 0.5, 1.0 - (pow(1.0 - hsl[1] * cos(a), 2.0) + pow(1.0 - perceivedBrightness * sin(a), 2.0))) : 0.0;",
        "float saturation = clamp(hsl[1] + (boostBase * vibrancy) / float(passes), 0.0, 1.0);",
        "vec3 newColor = hsl2rgb(vec3(hsl[0], saturation, hsl[2]));",
        "return vec4(newColor, color[3]);",
    )
    gain_requirements = (
        "vec3 gain(vec3 src, float k) {",
        "vec3 x = clamp(src, 0.0, 1.0);",
        "vec3 t = step(0.5, x);",
        "vec3 y = mix(x, 1.0 - x, t);",
        "vec3 a = 0.5 * pow(2.0 * y, vec3(k));",
        "return mix(a, 1.0 - a, t);",
    )
    window_rule_rounding_requirements = (
        'DEFINE_PROP(Config::FLOAT, roundingPower, {std::string("decoration:rounding_power")}, WINDOW_RULE_EFFECT_ROUNDING_POWER)',
    )
    window_header_055 = (
        "float                      rounding();",
        "float                      roundingPower();",
    )
    window_header_056 = (
        "float                             rounding();",
        "float                             roundingPower();",
    )
    renderer_header_055 = (
        "drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) = 0;",
    )
    renderer_header_056 = (
        "drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) = 0;",
        "drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,",
        "drawGlow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a)",
    )
    opengl_header_common = (
        "struct SRectRenderData {",
        "float          roundingPower = 2.F;",
        "struct STextureRenderData {",
        "float                  roundingPower = 2.F;",
        "struct SBorderRenderData {",
        "float roundingPower = 2.F;",
    )
    opengl_header_055 = (
        *opengl_header_common,
        "renderRoundedShadow(const CBox&, int round, float roundingPower, int range, const CHyprColor& color, float a = 1.0);",
        "renderInnerGlow(const CBox&, int round, float roundingPower, int range, const CHyprColor& color, int glowPower, float a = 1.0);",
    )
    opengl_header_056 = (
        *opengl_header_common,
        "renderRoundedShadow(const CBox&, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a = 1.0);",
        "renderRoundedShadow(const CBox&, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,",
        "renderInnerGlow(const CBox&, int round, float roundingPower, int range, const Config::CGradientValueData& color, int glowPower, float a = 1.0);",
    )
    shadow_header_055 = (
        "struct SShadowRenderData {",
        "float rounding      = 0;",
        "float roundingPower = 0;",
        "int   size          = 0;",
        "drawShadowInternal(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a);",
    )
    shadow_header_056 = (
        "struct SShadowRenderData {",
        "float rounding      = 0;",
        "float roundingPower = 0;",
        "int   size          = 0;",
        "drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a);",
        "drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,",
    )
    border_pass_header = (
        "struct SBorderData {",
        "float                      roundingPower = 2.F;",
    )
    rect_pass_header = (
        "struct SRectData {",
        "float      roundingPower = 2.0f;",
    )
    surface_pass_header = (
        "struct SRenderData {",
        "float                  roundingPower = 2.0F;",
    )
    tex_pass_header = (
        "struct SRenderData {",
        "float                  roundingPower       = 2.0f;",
    )
    border_fragment_requirements = (
        "uniform float radius;",
        "uniform float roundingPower;",
        '#include "rounding.glsl"',
        "getBorder(v_texcoord, alpha, fullSizeUntransformed, radiusOuter, thick, radius, roundingPower, topLeft, fullSize, gradientLength, gradient, angle, gradient2Length,",
    )
    border_shader_requirements = (
        "getBorder(vec2 v_texcoord, float alpha, vec2 fullSizeUntransformed, float radiusOuter, float thick, float radius, float roundingPower, vec2 topLeft, vec2 fullSize,",
        "if (min(pixCoord.x, pixCoord.y) > 0.0 && radius > 0.0) {",
        "float dist      = pow(pow(pixCoord.x, roundingPower) + pow(pixCoord.y, roundingPower), 1.0 / roundingPower);",
        "float distOuter = pow(pow(pixCoordOuter.x, roundingPower) + pow(pixCoordOuter.y, roundingPower), 1.0 / roundingPower);",
        "float normalized = smoothstep(0.0, 1.0, (dist - radius + thick + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));",
        "float normalized = 1.0 - smoothstep(0.0, 1.0, (distOuter - radiusOuter + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));",
    )
    ext_fragment_requirements = (
        "uniform float              radius;",
        "uniform float              roundingPower;",
        "if (radius > 0.0)",
        "pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);",
    )
    quad_fragment_requirements = (
        "uniform float radius;",
        "uniform float roundingPower;",
        "pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);",
        "mirrorColor = rounding(colorSRGB, radius, roundingPower, topLeft, fullSize);",
    )
    rounding_shader_requirements = (
        "float distanceWithRounding(vec2 coords, float roundingPower) {",
        "return pow(pow(coords.x, roundingPower) + pow(coords.y, roundingPower), 1.0 / roundingPower);",
        "vec4 rounding(vec4 color, float radius, float roundingPower, vec2 topLeft, vec2 fullSize) {",
        "float dist = distanceWithRounding(pixCoord, roundingPower);",
        "if (dist > radius + SMOOTHING_CONSTANT)",
        "discard;",
        "float normalized = 1.0 - smoothstep(0.0, 1.0, (dist - radius + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));",
        "color *= normalized;",
        "return color;",
    )
    shadow_fragment_requirements = (
        "uniform float radius;",
        "uniform float roundingPower;",
        "uniform float range;",
        "uniform float shadowPower;",
        "getShadow(pixColor, colorSRGB, v_texcoord, radius, roundingPower, topLeft, fullSize, range, shadowPower, bottomRight, windowTopLeft, windowBottomRight, thick",
    )
    shadow_shader_requirements = (
        '#include "rounding.glsl"',
        "float pixAlphaRoundedDistance(float distanceToCorner, float radius, float range, float shadowPower) {",
        "if (distanceToCorner > radius) {",
        "return 0.0;",
        "if (distanceToCorner > radius - range) {",
        "return pow((range - (distanceToCorner - radius + range)) / range, shadowPower);",
        "return 1.0;",
        "float modifiedLength(vec2 a, float roundingPower) {",
        "return pow(pow(abs(a.x), roundingPower) + pow(abs(a.y), roundingPower), 1.0 / roundingPower);",
        "bool pointInRoundedRect(vec2 pixCoord, vec2 tl, vec2 br, float radius, float roundingPower) {",
        "return distanceWithRounding(delta, roundingPower) <= radius;",
        "getShadow(vec4 pixColor, vec4 colorSRGB, vec2 v_texcoord, float borderRadius, float roundingPower, vec2 topLeft, vec2 fullSize, float range, float shadowPower,",
        "float radius        = range + borderRadius;",
        "pixAlphaRoundedDistance(modifiedLength(pixCoord - topLeft, roundingPower), radius, range, shadowPower);",
        "pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(topLeft[0], bottomRight[1]), roundingPower), radius, range, shadowPower);",
        "pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(bottomRight[0], topLeft[1]), roundingPower), radius, range, shadowPower);",
        "pixAlphaRoundedDistance(modifiedLength(pixCoord - bottomRight, roundingPower), radius, range, shadowPower);",
        "if (!done) {",
        "float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));",
        "if (smallest < range) {",
        "pixColor[3] = pixColor[3] * pow((smallest / range), shadowPower);",
        "if (pointInRoundedRect(pixCoord, windowTopLeft, windowBottomRight, windowRadius, roundingPower))",
        "pixColor[3] = 0.0;",
    )
    surface_fragment_055 = (
        "uniform float radius;",
        "uniform float roundingPower;",
        "pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);",
        "mirrorColor = rounding(mirrorColor, radius, roundingPower, topLeft, fullSize);",
    )
    surface_fragment_056 = (
        "uniform float radius;",
        "uniform float roundingPower;",
        "const float radius        = 0.0;",
        "const float roundingPower = 2.0;",
        "pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);",
        "mirrorColor = rounding(mirrorColor, radius, roundingPower, topLeft, fullSize);",
    )
    window_decoration_construction = (
        "pWindow->addWindowDeco(makeUnique<CHyprDropShadowDecoration>(pWindow));",
        "pWindow->addWindowDeco(makeUnique<CHyprBorderDecoration>(pWindow));",
        "pWindow->addWindowDeco(makeUnique<CHyprDropShadowDecoration>(pWindow));",
        "pWindow->addWindowDeco(makeUnique<CHyprBorderDecoration>(pWindow));",
    )
    window_decoration_update_055 = (
        "void CWindow::updateWindowDecos() {",
        "if (!m_isMapped || isHidden())",
        "return;",
        "g_pDecorationPositioner->onWindowUpdate(m_self.lock());",
        "m_decosToRemove.clear();",
        "std::vector<IHyprWindowDecoration*> decos;",
        "decos.reserve(m_windowDecorations.size());",
        "for (auto const& wd : m_windowDecorations) {",
        "decos.push_back(wd.get());",
        "for (auto const& wd : decos) {",
        "if (std::ranges::find_if(m_windowDecorations, [wd](const auto& other) { return other.get() == wd; }) == m_windowDecorations.end())",
        "continue;",
        "wd->updateWindow(m_self.lock());",
    )
    window_decoration_update_056 = (
        "void CWindow::updateWindowDecos() {",
        "if (!m_isMapped || isHidden())",
        "return;",
        "g_pDecorationPositioner->onWindowUpdate(m_self.lock());",
        "m_decosToRemove.clear();",
        "const auto updateDecos = [this](const auto& decos) {",
        "for (auto const& wd : decos) {",
        "if (std::ranges::find_if(m_windowDecorations, [wd](const auto& other) { return other.get() == wd; }) == m_windowDecorations.end())",
        "continue;",
        "wd->updateWindow(m_self.lock());",
        "constexpr size_t INLINE_DECOS = 4;",
        "std::array<IHyprWindowDecoration*, INLINE_DECOS> inlineDecos = {};",
        "if (m_windowDecorations.size() <= inlineDecos.size()) {",
        "std::ranges::transform(m_windowDecorations, inlineDecos.begin(), [](const auto& deco) { return deco.get(); });",
        "updateDecos(std::span{inlineDecos}.first(m_windowDecorations.size()));",
        "return;",
        "std::vector<IHyprWindowDecoration*> decos;",
        "decos.reserve(m_windowDecorations.size());",
        "std::ranges::transform(m_windowDecorations, std::back_inserter(decos), [](const auto& deco) { return deco.get(); });",
        "updateDecos(decos);",
    )
    workspace_update_055 = (
        "void CWorkspace::updateWindowDecos() {",
        "for (auto const& w : g_pCompositor->m_windows) {",
        "if (w->m_workspace != m_self)",
        "continue;",
        "w->updateWindowDecos();",
    )
    workspace_update_056 = (
        "void CWorkspace::updateWindowDecos() {",
        "for (auto const& w : Desktop::windowState()->windows()) {",
        "if (w->m_workspace != m_self)",
        "continue;",
        "w->updateWindowDecos();",
    )
    border_positioning_prefix = (
        "SDecorationPositioningInfo CHyprBorderDecoration::getPositioningInfo() {",
        "const auto BORDERSIZE = m_window->getRealBorderSize();",
        "m_extents = {{BORDERSIZE, BORDERSIZE}, {BORDERSIZE, BORDERSIZE}};",
        "if (doesntWantBorders())",
        "m_extents = {{}, {}};",
        "info.priority = 10000;",
        "info.policy = DECORATION_POSITION_STICKY;",
        "info.desiredExtents = m_extents;",
        "info.reserved = true;",
        "info.edges = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;",
        "m_reportedExtents = m_extents;",
        "void CHyprBorderDecoration::draw(PHLMONITOR pMonitor, float const& a) {",
        "if (doesntWantBorders())",
        "return;",
    )
    border_damage_suffix = (
        "const auto ROUNDING = m_window->rounding();",
        "const auto BORDERSIZE = m_window->getRealBorderSize() + 1;",
        "CRegion borderRegion(GLOBAL_BOX);",
        "borderRegion.subtract(GLOBAL_BOX.copy().expand(-(BORDERSIZE + ROUNDING)));",
        "borderRegion.expand(2); // pad",
        "const CBox borderExtents = borderRegion.getExtents();",
    )
    border_flags = (
        "uint64_t CHyprBorderDecoration::getDecorationFlags() {",
        'static auto PPARTOFWINDOW = CConfigValue<Config::INTEGER>("decoration:border_part_of_window");',
        "return *PPARTOFWINDOW && !doesntWantBorders() ? DECORATION_PART_OF_MAIN_WINDOW : 0;",
        "bool CHyprBorderDecoration::doesntWantBorders() {",
        "return m_window->m_X11DoesntWantBorders || m_window->getRealBorderSize() == 0 || !m_window->m_ruleApplicator->decorate().valueOrDefault();",
    )
    border_rounding_requirements = (
        "const auto                      ROUNDINGBASE     = m_window->rounding();",
        "const auto                      ROUNDING         = ROUNDINGBASE * pMonitor->m_scale;",
        "const auto                      ROUNDINGPOWER    = m_window->roundingPower();",
        "const auto                      CORRECTIONOFFSET = (borderSize * (M_SQRT2 - 1) * std::max(2.0 - ROUNDINGPOWER, 0.0));",
        "const auto                      OUTERROUND       = ((ROUNDINGBASE + borderSize) - CORRECTIONOFFSET) * pMonitor->m_scale;",
        "data.round         = ROUNDING;",
        "data.outerRound    = OUTERROUND;",
        "data.roundingPower = ROUNDINGPOWER;",
    )
    positioner_included_box = (
        "CBox CDecorationPositioner::getBoxWithIncludedDecos(PHLWINDOW pWindow) {",
        "CBox accum = pWindow->getWindowMainSurfaceBox();",
        "const auto WIT = m_windowDatas.find(pWindow);",
        "if (WIT == m_windowDatas.end())",
        "return accum;",
        "for (auto const& data : WIT->second.positioningDatas) {",
        "if (!(data->pDecoration->getDecorationFlags() & DECORATION_PART_OF_MAIN_WINDOW))",
        "continue;",
        "if (data->positioningInfo.policy == DECORATION_POSITION_ABSOLUTE) {",
        "decoBox = pWindow->getWindowMainSurfaceBox();",
        "decoBox.addExtents(data->positioningInfo.desiredExtents);",
        "decoBox = data->lastReply.assignedGeometry;",
        "const auto EDGEPOINT = getEdgeDefinedPoint(data->positioningInfo.edges, pWindow);",
        "decoBox.translate(EDGEPOINT);",
        "if (decoBox.x < accum.x)",
        "extentsToAdd.topLeft.x = accum.x - decoBox.x;",
        "if (decoBox.y < accum.y)",
        "extentsToAdd.topLeft.y = accum.y - decoBox.y;",
        "if (decoBox.x + decoBox.w > accum.x + accum.w)",
        "extentsToAdd.bottomRight.x = (decoBox.x + decoBox.w) - (accum.x + accum.w);",
        "if (decoBox.y + decoBox.h > accum.y + accum.h)",
        "extentsToAdd.bottomRight.y = (decoBox.y + decoBox.h) - (accum.y + accum.h);",
        "accum.addExtents(extentsToAdd);",
        "return accum;",
    )
    shadow_damage_prefix = (
        "void CHyprDropShadowDecoration::damageEntire() {",
        'static auto PSHADOWS = CConfigValue<Config::INTEGER>("decoration:shadow:enabled");',
        "if (*PSHADOWS != 1)",
        "return; // disabled",
        "const auto PWINDOW = m_window.lock();",
    )
    shadow_damage_suffix = (
        "CBox shadowBox = {pos.x - m_extents.topLeft.x, pos.y - m_extents.topLeft.y, pos.x + size.x + m_extents.bottomRight.x, pos.y + size.y + m_extents.bottomRight.y};",
        "const auto PWORKSPACE = PWINDOW->m_workspace;",
        "const auto applyOffset = [&](CBox& b) {",
        "if (PWORKSPACE && PWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)",
        "b.translate(PWORKSPACE->m_renderOffset->value());",
        "b.translate(PWINDOW->m_floatingOffset);",
        "applyOffset(shadowBox);",
        "CRegion shadowRegion(shadowBox);",
    )
    shadow_update_prefix = (
        "void CHyprDropShadowDecoration::updateWindow(PHLWINDOW pWindow) {",
        "const auto PWINDOW = m_window.lock();",
    )
    shadow_rounding_setup = (
        "const auto  BORDERSIZE       = PWINDOW->getRealBorderSize();",
        "const auto  ROUNDINGBASE     = PWINDOW->rounding();",
        "const auto  ROUNDINGPOWER    = PWINDOW->roundingPower();",
        "const auto  CORRECTIONOFFSET = (BORDERSIZE * (M_SQRT2 - 1) * std::max(2.0 - ROUNDINGPOWER, 0.0));",
        "const auto  ROUNDING         = ROUNDINGBASE > 0 ? (ROUNDINGBASE + BORDERSIZE) - CORRECTIONOFFSET : 0;",
    )
    shadow_update_suffix = (
        "m_lastWindowBox = {m_lastWindowPos.x, m_lastWindowPos.y, m_lastWindowSize.x, m_lastWindowSize.y};",
        "m_lastWindowBoxWithDecos = g_pDecorationPositioner->getBoxWithIncludedDecos(pWindow);",
        "bool CHyprDropShadowDecoration::canRender(PHLMONITOR pMonitor) {",
        'static auto PSHADOWS = CConfigValue<Config::INTEGER>("decoration:shadow:enabled");',
        "if (*PSHADOWS != 1)",
        "return false; // disabled",
        "if (!validMapped(PWINDOW))",
        "return false;",
        "if (!PWINDOW->m_ruleApplicator->decorate().valueOrDefault())",
        "return false;",
        "if (PWINDOW->m_ruleApplicator->noShadow().valueOrDefault())",
        "return false;",
        "SShadowRenderData CHyprDropShadowDecoration::getRenderData(PHLMONITOR pMonitor, float const& a) {",
        "if (!canRender(pMonitor))",
        "return {};",
        'static auto PSHADOWSIZE   = CConfigValue<Config::INTEGER>("decoration:shadow:range");',
        'static auto PSHADOWSCALE  = CConfigValue<Config::FLOAT>("decoration:shadow:scale");',
        'static auto PSHADOWOFFSET = CConfigValue<Config::VEC2>("decoration:shadow:offset");',
        *shadow_rounding_setup,
        "CBox fullBox = m_lastWindowBoxWithDecos;",
        "fullBox.translate(-pMonitor->m_position + WORKSPACEOFFSET);",
        "fullBox.x -= *PSHADOWSIZE;",
        "fullBox.y -= *PSHADOWSIZE;",
        "fullBox.w += 2 * *PSHADOWSIZE;",
        "fullBox.h += 2 * *PSHADOWSIZE;",
        "const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);",
        "fullBox.scaleFromCenter(SHADOWSCALE).translate({(*PSHADOWOFFSET).x, (*PSHADOWOFFSET).y});",
        "updateWindow(PWINDOW);",
        "m_lastWindowPos += WORKSPACEOFFSET;",
        "m_extents = {",
        "m_lastWindowPos.x - fullBox.x - pMonitor->m_position.x + 2,",
        "m_lastWindowPos.y - fullBox.y - pMonitor->m_position.y + 2,",
        "fullBox.x + fullBox.width + pMonitor->m_position.x - m_lastWindowPos.x - m_lastWindowSize.x + 2,",
        "fullBox.y + fullBox.height + pMonitor->m_position.y - m_lastWindowPos.y - m_lastWindowSize.y + 2,",
        "fullBox.translate(PWINDOW->m_floatingOffset);",
        "if (fullBox.width < 1 || fullBox.height < 1)",
        "return {}; // don't draw invisible shadows",
        "fullBox.scale(pMonitor->m_scale).round();",
        ".fullBox = fullBox,",
    )
    shadow_reposition = (
        "void CHyprDropShadowDecoration::reposition() {",
        "if (m_extents != m_reportedExtents)",
        "g_pDecorationPositioner->repositionDeco(this);",
        "g_pHyprRenderer->m_renderData.currentWindow.reset();",
    )
    shadow_rounding_055 = (
        ".rounding      = ROUNDING,",
        ".roundingPower = ROUNDINGPOWER,",
        ".size          = *PSHADOWSIZE,",
        *shadow_reposition,
        "drawShadowInternal(data.fullBox, data.rounding * pMonitor->m_scale, data.roundingPower, data.size * pMonitor->m_scale, PWINDOW->m_realShadowColor->value(), a);",
        "reposition();",
        "void CHyprDropShadowDecoration::drawShadowInternal(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) {",
        'static auto PSHADOWSHARP = CConfigValue<Config::INTEGER>("decoration:shadow:sharp");',
        "if (*PSHADOWSHARP)",
        "CRectPassElement::SRectData{",
        ".box = box,",
        ".roundingPower = roundingPower,",
        "}, box);",
        "else",
        "g_pHyprRenderer->drawShadow(box, round, roundingPower, range, color, 1.F);",
    )
    shadow_rounding_056 = (
        ".rounding      = ROUNDING,",
        ".roundingPower = ROUNDINGPOWER,",
        ".size          = *PSHADOWSIZE,",
        *shadow_reposition,
        "drawShadowInternal(data.fullBox, data.rounding * pMonitor->m_scale, data.roundingPower, data.size * pMonitor->m_scale, PWINDOW->m_realShadowColorPrevious, grad,",
        "drawShadowInternal(data.fullBox, data.rounding * pMonitor->m_scale, data.roundingPower, data.size * pMonitor->m_scale, grad, a);",
        "reposition();",
        "void CHyprDropShadowDecoration::drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a) {",
        'static auto PSHADOWSHARP = CConfigValue<Config::INTEGER>("decoration:shadow:sharp");',
        "if (*PSHADOWSHARP) {",
        "CHyprColor flatColor = grad.m_colors.empty() ? CHyprColor(0, 0, 0, 0) : grad.m_colors[0];",
        "CRectPassElement::SRectData{",
        ".box = box,",
        ".roundingPower = roundingPower,",
        "}, box);",
        "} else",
        "g_pHyprRenderer->drawShadow(box, round, roundingPower, range, grad, a);",
        "void CHyprDropShadowDecoration::drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,",
        'static auto PSHADOWSHARP = CConfigValue<Config::INTEGER>("decoration:shadow:sharp");',
        "if (*PSHADOWSHARP) {",
        "CHyprColor col1 = grad1.m_colors.empty() ? CHyprColor(0, 0, 0, 0) : grad1.m_colors[0];",
        "CHyprColor col2 = grad2.m_colors.empty() ? col1 : grad2.m_colors[0];",
        ".box = box,",
        ".roundingPower = roundingPower,",
        "}, box);",
        "} else",
        "g_pHyprRenderer->drawShadow(box, round, roundingPower, range, grad1, grad2, lerp, a);",
    )
    glow_decoration_055 = (
        "void CHyprInnerGlowDecoration::render(PHLMONITOR pMonitor, float const& a) {",
        'static auto PGLOW      = CConfigValue<Hyprlang::INT>("decoration:glow:enabled");',
        'static auto PGLOWRANGE = CConfigValue<Hyprlang::INT>("decoration:glow:range");',
        'static auto PGLOWPOWER = CConfigValue<Hyprlang::INT>("decoration:glow:render_power");',
        "if (!*PGLOW)",
        "return;",
        "const int   GLOWSIZE  = *PGLOWRANGE;",
        "const float GLOWPOWER = *PGLOWPOWER;",
        "Render::GL::g_pHyprOpenGL->renderInnerGlow(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, GLOWCOLOR, GLOWPOWER, a);",
    )
    glow_decoration_056 = (
        "void CHyprInnerGlowDecoration::render(PHLMONITOR pMonitor, float const& a) {",
        'static auto PGLOW = CConfigValue<Config::INTEGER>("decoration:glow:enabled");',
        "if (!*PGLOW || !visible())",
        "return;",
        'static auto PGLOWSIZE = CConfigValue<Config::INTEGER>("decoration:glow:range");',
        "const auto  GLOWSIZE  = sc<int>(*PGLOWSIZE);",
        "drawGlowInternal(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, PWINDOW->m_realGlowColorPrevious, grad,",
        "drawGlowInternal(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, grad, a);",
        "void CHyprInnerGlowDecoration::drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a) {",
        "g_pHyprRenderer->drawGlow(box, round, roundingPower, range, grad, a);",
        "void CHyprInnerGlowDecoration::drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,",
        "g_pHyprRenderer->drawGlow(box, round, roundingPower, range, grad1, grad2, lerp, a);",
        "bool CHyprInnerGlowDecoration::visible() {",
        'static auto PENABLED = CConfigValue<Config::INTEGER>("decoration:glow:enabled");',
        "return *PENABLED && m_window->m_ruleApplicator->decorate().valueOrDefault();",
    )
    glow_fragment_055 = (
        "uniform float range;",
        "uniform float shadowPower;",
        '#include "inner_glow.glsl"',
        "void main() {",
        "getInnerGlow(pixColor, colorSRGB, v_texcoord, radius, roundingPower, topLeft, fullSize, range, shadowPower, bottomRight);",
    )
    glow_fragment_056 = (
        "uniform float range;",
        "uniform float shadowPower;",
        '#include "inner_glow.glsl"',
        "void main() {",
        "getInnerGlow(pixColor, colorSRGB, v_texcoord, radius, roundingPower, topLeft, fullSize, range, shadowPower, bottomRight,",
        "gradientLength, gradient, angle, gradient2Length, gradient2, angle2, gradientLerp, alpha);",
    )
    glow_shader_requirements = (
        "float innerGlowAlpha(float distFromEdge, float range, float glowPower) {",
        "if (distFromEdge >= range)",
        "return 0.0;",
        "if (distFromEdge <= 0.0)",
        "return 1.0;",
        "return pow(1.0 - distFromEdge / range, glowPower);",
        "float innerGlowSmin(float a, float b, float k) {",
        "float h = max(k - abs(a - b), 0.0) / k;",
        "float k = range;",
        "float distFromEdge = innerGlowSmin(innerGlowSmin(distT, distB, k), innerGlowSmin(distL, distR, k), k);",
        "pixColor[3] = pixColor[3] * innerGlowAlpha(distFromEdge, range, glowPower);",
    )
    output_scale_requirements = (
        "if (head->m_state.committedProperties & eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_SCALE) {",
        "newState.scale = head->m_state.scale;",
        "newState.committedProperties |= eWlrOutputCommittedProperties::OUTPUT_HEAD_COMMITTED_SCALE;",
        "Config::monitorRuleMgr()->scheduleReload();",
        "m_owner->m_monitorStates[PMONITOR->m_name] = newState;",
        "m_resource->setSetScale([this](CZwlrOutputConfigurationHeadV1* r, wl_fixed_t scale_) {",
        "double scale = wl_fixed_to_double(scale_);",
        "if (scale < 0.1 || scale > 10.0) {",
        'm_resource->error(ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_ERROR_INVALID_SCALE, "Invalid scale");',
        "m_state.committedProperties |= OUTPUT_HEAD_COMMITTED_SCALE;",
        "m_state.scale = scale;",
    )
    monitor_rule_requirements_055 = (
        "CMonitorRuleManager::CMonitorRuleManager() {",
        "if (m_reloadScheduled)",
        "performMonitorReload();",
        "void CMonitorRuleManager::add(CMonitorRule&& x) {",
        "scheduleReload();",
        "CMonitorRule CMonitorRuleManager::get(const PHLMONITOR PMONITOR) {",
        "if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_SCALE) {",
        "rule.m_scale = CONFIG->scale;",
        "void CMonitorRuleManager::scheduleReload() {",
        "m_reloadScheduled = true;",
        "void CMonitorRuleManager::performMonitorReload() {",
        "auto rule = get(m);",
        "if (!m->applyMonitorRule(Config::CMonitorRule{rule})) {",
    )
    monitor_rule_requirements_056 = (
        "CMonitorRuleManager::CMonitorRuleManager() {",
        "if (m_reloadScheduled)",
        "ensureMonitorStatus();",
        "void CMonitorRuleManager::add(CMonitorRule&& x) {",
        "scheduleReload();",
        "CMonitorRule CMonitorRuleManager::get(const PHLMONITOR PMONITOR) {",
        "if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_SCALE) {",
        "rule.m_scale = CONFIG->scale;",
        "void CMonitorRuleManager::scheduleReload() {",
        "m_reloadScheduled = true;",
        "void CMonitorRuleManager::ensureMonitorStatus() {",
        "auto rule = get(m);",
        "auto cmp = rule.compare(m->m_activeMonitorRule);",
        "if (cmp == COMPARISON_SOFT_MISMATCH) {",
        "m->applyMonitorRuleSoft(Config::CMonitorRule{rule});",
        "if (!m->applyMonitorRule(Config::CMonitorRule{rule})) {",
    )
    monitor_scale_requirements_055 = (
        "bool CMonitor::applyMonitorRule(Config::CMonitorRule&& pMonitorRule, bool force) {",
        "m_activeMonitorRule = std::move(pMonitorRule);",
        "const auto RULE = &m_activeMonitorRule;",
        "if (RULE->m_scale > 0.1)",
        "m_scale = RULE->m_scale;",
        "else {",
        "const auto DEFAULTSCALE = getDefaultScale();",
        "m_scale                 = DEFAULTSCALE;",
        "m_setScale     = m_scale;",
    )
    monitor_scale_requirements_056 = (
        "bool CMonitor::applyMonitorRule(Config::CMonitorRule&& pMonitorRule) {",
        "const auto RULE = &pMonitorRule;",
        "const bool autoScale = RULE->m_scale <= 0.1;",
        "if (autoScale)",
        "m_scale = getDefaultScale();",
        "else",
        "m_scale = RULE->m_scale;",
        "m_setScale = m_scale;",
        "applyMonitorRuleSoft(std::move(pMonitorRule));",
    )
    return {
        "0.55.0": {
            REGISTRY_PATH: (
                'MS<Float>("decoration:rounding_power", "rounding power of corners (2 is a circle)", 2, {.min = 2, .max = 10}),',
                'MS<Float>("decoration:active_opacity", "opacity of active windows.", 1, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:inactive_opacity", "opacity of inactive windows.", 1, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:fullscreen_opacity", "opacity of fullscreen windows.", 1, {.min = 0, .max = 1}),',
                'MS<Int>("decoration:shadow:range", "Shadow range (size) in layout px", 4, {.min = 0, .max = 100}),',
                'MS<Int>("decoration:shadow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3, {.min = 1, .max = 4}),',
                'MS<Bool>("decoration:shadow:sharp", "whether the shadow should be sharp or not.", false),',
                'MS<Vec2>("decoration:shadow:offset", "shadow\'s rendering offset.", Config::VEC2{}, {.validator = vec2Range(-250, -250, 250, 250)}),',
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1}),',
                'MS<Bool>("decoration:glow:enabled", "enable inner glow on windows", false),',
                'MS<Int>("decoration:glow:range", "glow range (size) in layout px", 10, {.min = 0, .max = 100}),',
                'MS<Int>("decoration:glow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3, {.min = 1, .max = 4}),',
                'MS<Bool>("decoration:dim_modal", "enables dimming of parents of modal windows", true),',
                'MS<Bool>("decoration:dim_inactive", "enables dimming of inactive windows", false),',
                'MS<Float>("decoration:dim_strength", "how much inactive windows should be dimmed", 0.5, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:dim_special", "how much to dim the rest of the screen by when a special workspace is open.", 0.2, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:dim_around", "how much the dimaround window rule should dim by.", 0.4, {.min = 0, .max = 1}),',
                'MS<Bool>("decoration:border_part_of_window", "whether the border should be treated as a part of the window.", true),',
                'MS<Int>("decoration:blur:size", "blur size (distance)", 8, {.min = 0, .max = 100}),',
                'MS<Int>("decoration:blur:passes", "the amount of passes to perform", 1, {.min = 0, .max = 10}),',
                'MS<Bool>("decoration:blur:ignore_opacity", "make the blur layer ignore the opacity of the window", true),',
                'MS<Bool>("decoration:blur:new_optimizations", "whether to enable further optimizations to the blur.", true),',
                'MS<Bool>("decoration:blur:xray", "if enabled, floating windows will ignore tiled windows in their blur.", false),',
                'MS<Float>("decoration:blur:noise", "how much noise to apply.", 0.0117, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:blur:contrast", "contrast modulation for blur.", 0.8916, {.min = 0, .max = 2}),',
                'MS<Float>("decoration:blur:brightness", "brightness modulation for blur.", 1, {.min = 0, .max = 2}),',
                'MS<Float>("decoration:blur:vibrancy", "Increase saturation of blurred colors.", 0.1696, {.min = 0, .max = 1}),',
                'MS<Float>("decoration:blur:vibrancy_darkness", "How strong the effect of vibrancy is on dark areas.", 0, {.min = 0, .max = 1}),',
                'MS<Bool>("decoration:blur:special", "whether to blur behind the special workspace (note: expensive)", false),',
                'MS<Bool>("decoration:blur:popups", "whether to blur popups (e.g. right-click menus)", false),',
                'MS<Float>("decoration:blur:popups_ignorealpha", "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur.", 0.2, {.min = 0, .max = 1}),',
                'MS<Bool>("decoration:blur:input_methods", "whether to blur input methods (e.g. fcitx5)", false),',
                'MS<Float>("decoration:blur:input_methods_ignorealpha", "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur.", 0.2, {.min = 0, .max = 1}),',
            ),
            Path("src/config/lua/ConfigManager.cpp"): reload_requirements,
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): (
                "if (m_propsTripped & REFRESH_BLUR_FB) { for (auto const& m : g_pCompositor->m_monitors) { if (m) m->m_blurFBDirty = true; } }",
                "if (m_propsTripped & REFRESH_WINDOW_STATES) {",
                "Desktop::Rule::ruleEngine()->updateAllRules();",
                "for (const auto& ws : g_pCompositor->getWorkspaces()) {",
                "if (!ws)",
                "continue;",
                "ws->updateWindows();",
                "ws->updateWindowData();",
                "ws->updateWindowDecos();",
                "g_pCompositor->updateAllWindowsAnimatedDecorationValues();",
                "if (m_propsTripped & REFRESH_LAYOUTS) {",
                "Layout::Supplementary::algoMatcher()->updateWorkspaceLayouts();",
                "g_layoutManager->recalculateMonitor(m);",
                "g_pHyprRenderer->damageMonitor(m);",
            ),
            Path("src/Compositor.cpp"): (
                "void CCompositor::updateAllWindowsAnimatedDecorationValues() {",
                "for (auto const& w : m_windows) {",
                "if (!w->m_isMapped)",
                "continue;",
                "w->updateDecorationValues();",
            ),
            Path("src/desktop/view/Window.cpp"): (
                *window_decoration_construction,
                *window_decoration_update_055,
                *rounding_window_requirements,
                *window_prefix,
                "if (isEffectiveInternalFSMode(FSMODE_FULLSCREEN)) {",
                "*alpha(WINDOW_ALPHA_ACTIVE) = m_ruleApplicator->alphaFullscreen().valueOrDefault().applyAlpha(*PFULLSCREENALPHA);",
                "} else { if (m_self == Desktop::focusState()->window()) *alpha(WINDOW_ALPHA_ACTIVE) = m_ruleApplicator->alpha().valueOrDefault().applyAlpha(*PACTIVEALPHA); else *alpha(WINDOW_ALPHA_ACTIVE) = m_ruleApplicator->alphaInactive().valueOrDefault().applyAlpha(*PINACTIVEALPHA); }",
                *window_dim,
                "updateWindowDecos();",
                *window_map_prefix,
                "if (!m_ruleApplicator->noFocus().valueOrDefault() && !m_noInitialFocus && (!isX11OverrideRedirect() || (m_isX11 && m_xwaylandSurface->wantsFocus())) && !workspaceSilent && (!PFORCEFOCUS || PFORCEFOCUS == m_self.lock()) && !g_pInputManager->isConstrained()) {",
                *window_map_values,
            ),
            Path("src/render/Renderer.cpp"): renderer_055,
            Path("src/render/OpenGL.cpp"): opengl_055,
            Path("src/render/pass/Pass.cpp"): pass_055,
            Path("src/render/ShaderLoader.hpp"): (
                *shader_loader_rounding_055,
                *shader_loader_requirements,
            ),
            Path("src/render/Shader.cpp"): shader_uniform_requirements,
            Path("src/render/GLRenderer.cpp"): (
                *gl_renderer_shadow_055,
                *gl_renderer_requirements,
            ),
            Path("src/render/ElementRenderer.cpp"): element_renderer_requirements,
            Path("src/render/gl/GLElementRenderer.cpp"): gl_element_renderer_requirements,
            Path("src/render/pass/PreBlurElement.hpp"): preblur_header_requirements,
            Path("src/render/pass/PreBlurElement.cpp"): preblur_implementation_requirements,
            Path("src/render/shaders/glsl/blurprepare.frag"): blurprepare_wrapper_requirements,
            Path("src/render/shaders/glsl/blurprepare.glsl"): (
                *blurprepare_common_requirements[:5],
                "pixColor = toNit(pixColor, vec2(srcTFRange[0], srcRefLuminance));",
                *blurprepare_common_requirements[5:],
            ),
            Path("src/render/shaders/glsl/blurfinish.frag"): blurfinish_wrapper_requirements,
            Path("src/render/shaders/glsl/blurFinish.glsl"): (
                *blurfinish_common_requirements,
                "if (targetTF == CM_TRANSFER_FUNCTION_EXT_LINEAR) {",
                "pixColor = doColorManagement(pixColor, 1.0, sourceTF, targetTF, convertMatrix, srcTFRange, vec2(0.0, 160.0));",
                "pixColor = doColorManagement(pixColor, 1.0, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange);",
                "return pixColor;",
            ),
            Path("src/render/shaders/glsl/blur1.frag"): blur1_wrapper_requirements,
            Path("src/render/shaders/glsl/blur1.glsl"): blur1_requirements,
            Path("src/render/shaders/glsl/gain.glsl"): gain_requirements,
            Path("src/desktop/Workspace.cpp"): workspace_update_055,
            Path("src/render/decorations/CHyprBorderDecoration.cpp"): (
                *border_positioning_prefix,
                *border_rounding_requirements,
                "void CHyprBorderDecoration::damageEntire() {",
                "if (!validMapped(m_window) || m_window->m_fullscreenState.internal == FSMODE_FULLSCREEN)",
                "return;",
                "const auto GLOBAL_BOX = assignedBoxGlobal();",
                *border_damage_suffix,
                "for (auto const& m : g_pCompositor->m_monitors) {",
                "const CBox monitorBox = {m->m_position, m->m_size};",
                "if (borderExtents.intersection(monitorBox).empty())",
                "continue;",
                "if (!g_pHyprRenderer->shouldRenderWindow(m_window.lock(), m)) {",
                "const CRegion monitorRegion(monitorBox);",
                "borderRegion.subtract(monitorRegion);",
                "g_pHyprRenderer->damageRegion(borderRegion);",
                *border_flags,
            ),
            Path("src/render/decorations/DecorationPositioner.cpp"): positioner_included_box,
            Path("src/render/decorations/CHyprDropShadowDecoration.cpp"): (
                *shadow_damage_prefix,
                "const auto pos = PWINDOW->m_realPosition->value();",
                "const auto size = PWINDOW->m_realSize->value();",
                *shadow_damage_suffix,
                "for (auto const& m : g_pCompositor->m_monitors) {",
                "if (!g_pHyprRenderer->shouldRenderWindow(PWINDOW, m)) {",
                "const CRegion monitorRegion({m->m_position, m->m_size});",
                "shadowRegion.subtract(monitorRegion);",
                "g_pHyprRenderer->damageRegion(shadowRegion);",
                *shadow_update_prefix,
                "m_lastWindowPos = PWINDOW->m_realPosition->value();",
                "m_lastWindowSize = PWINDOW->m_realSize->value();",
                *shadow_update_suffix,
                *shadow_rounding_055,
            ),
            Path("src/desktop/rule/windowRule/WindowRuleApplicator.hpp"): window_rule_rounding_requirements,
            Path("src/desktop/view/Window.hpp"): window_header_055,
            Path("src/render/Renderer.hpp"): renderer_header_055,
            Path("src/render/OpenGL.hpp"): opengl_header_055,
            Path("src/render/decorations/CHyprDropShadowDecoration.hpp"): shadow_header_055,
            Path("src/render/pass/BorderPassElement.hpp"): border_pass_header,
            Path("src/render/pass/RectPassElement.hpp"): rect_pass_header,
            Path("src/render/pass/SurfacePassElement.hpp"): surface_pass_header,
            Path("src/render/pass/TexPassElement.hpp"): tex_pass_header,
            Path("src/render/shaders/glsl/border.frag"): border_fragment_requirements,
            Path("src/render/shaders/glsl/border.glsl"): border_shader_requirements,
            Path("src/render/shaders/glsl/ext.frag"): ext_fragment_requirements,
            Path("src/render/shaders/glsl/quad.frag"): quad_fragment_requirements,
            Path("src/render/shaders/glsl/rounding.glsl"): rounding_shader_requirements,
            Path("src/render/shaders/glsl/shadow.frag"): shadow_fragment_requirements,
            Path("src/render/shaders/glsl/shadow.glsl"): shadow_shader_requirements,
            Path("src/render/shaders/glsl/surface.frag"): surface_fragment_055,
            Path("src/render/decorations/CHyprInnerGlowDecoration.cpp"): glow_decoration_055,
            Path("src/render/shaders/glsl/inner_glow.frag"): glow_fragment_055,
            Path("src/render/shaders/glsl/inner_glow.glsl"): glow_shader_requirements,
            Path("src/protocols/OutputManagement.cpp"): output_scale_requirements,
            Path("src/config/shared/monitor/MonitorRuleManager.cpp"): monitor_rule_requirements_055,
            Path("src/helpers/Monitor.cpp"): monitor_scale_requirements_055,
        },
        "0.56.1": {
            REGISTRY_PATH: (
                'MS<Float>("decoration:rounding_power", "rounding power of corners (2 is a circle)", 2,',
                "{.min = 2, .max = 10, .refresh = Supplementary::REFRESH_WINDOW_STATES | Supplementary::REFRESH_BLUR_FB}),",
                'MS<Float>("decoration:active_opacity", "opacity of active windows.", 1, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Float>("decoration:inactive_opacity", "opacity of inactive windows.", 1, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Float>("decoration:fullscreen_opacity", "opacity of fullscreen windows.", 1, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Int>("decoration:shadow:range", "Shadow range (size) in layout px", 4, {.min = 0, .max = 100, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Int>("decoration:shadow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3, {.min = 1, .max = 4, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Bool>("decoration:shadow:sharp", "whether the shadow should be sharp or not.", false, {.refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Vec2>("decoration:shadow:offset", "shadow\'s rendering offset.", Config::VEC2{},',
                "{.validator = vec2Range(-250, -250, 250, 250), .refresh = Supplementary::REFRESH_WINDOW_STATES}),",
                'MS<Float>("decoration:shadow:scale", "shadow\'s scale.", 1, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Bool>("decoration:glow:enabled", "enable inner glow on windows", false, {.refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Int>("decoration:glow:range", "glow range (size) in layout px", 10, {.min = 0, .max = 100, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Int>("decoration:glow:render_power", "in what power to render the falloff (more power, the faster the falloff)", 3,',
                "{.min = 1, .max = 4, .refresh = Supplementary::REFRESH_WINDOW_STATES}),",
                'MS<Bool>("decoration:dim_modal", "enables dimming of parents of modal windows", true, {.refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Bool>("decoration:dim_inactive", "enables dimming of inactive windows", false, {.refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Float>("decoration:dim_strength", "how much inactive windows should be dimmed", 0.5, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Float>("decoration:dim_special", "how much to dim the rest of the screen by when a special workspace is open.", 0.2,',
                "{.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),",
                'MS<Float>("decoration:dim_around", "how much the dimaround window rule should dim by.", 0.4, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Bool>("decoration:border_part_of_window", "whether the border should be treated as a part of the window.", true),',
                'MS<Int>("decoration:blur:size", "blur size (distance)", 8, {.min = 0, .max = 100, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Int>("decoration:blur:passes", "the amount of passes to perform", 1, {.min = 0, .max = 10, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Bool>("decoration:blur:ignore_opacity", "make the blur layer ignore the opacity of the window", true, {.refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Bool>("decoration:blur:new_optimizations", "whether to enable further optimizations to the blur.", true, {.refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Bool>("decoration:blur:xray", "if enabled, floating windows will ignore tiled windows in their blur.", false, {.refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Float>("decoration:blur:noise", "how much noise to apply.", 0.0117, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Float>("decoration:blur:contrast", "contrast modulation for blur.", 0.8916, {.min = 0, .max = 2, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Float>("decoration:blur:brightness", "brightness modulation for blur.", 1, {.min = 0, .max = 2, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Float>("decoration:blur:vibrancy", "Increase saturation of blurred colors.", 0.1696, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Float>("decoration:blur:vibrancy_darkness", "How strong the effect of vibrancy is on dark areas.", 0, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Bool>("decoration:blur:special", "whether to blur behind the special workspace (note: expensive)", false, {.refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Bool>("decoration:blur:popups", "whether to blur popups (e.g. right-click menus)", false, {.refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Float>("decoration:blur:popups_ignorealpha", "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur.", 0.2, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Bool>("decoration:blur:input_methods", "whether to blur input methods (e.g. fcitx5)", false, {.refresh = Supplementary::REFRESH_BLUR_FB}),',
                'MS<Float>("decoration:blur:input_methods_ignorealpha", "works like ignorealpha in layer rules. If pixel opacity is below set value, will not blur.", 0.2, {.min = 0, .max = 1, .refresh = Supplementary::REFRESH_BLUR_FB}),',
            ),
            Path("src/config/lua/ConfigManager.cpp"): reload_requirements,
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): (
                "if (m_propsTripped & REFRESH_BLUR_FB) { for (auto const& m : State::monitorState()->monitors()) { if (!m) continue; m->m_blurFBDirty = true; m->m_forceFullFrames = 2; m->scheduleFrame(); } }",
                "if (m_propsTripped & REFRESH_WINDOW_STATES) {",
                "Desktop::Rule::ruleEngine()->updateAllRules();",
                "for (auto const& w : Desktop::windowState()->windows())",
                "w->uncacheWindowDecos();",
                "for (const auto& ws : State::workspaceState()->workspaces()) {",
                "if (!ws)",
                "continue;",
                "ws->updateWindows();",
                "ws->updateWindowData();",
                "ws->updateWindowDecos();",
                "Desktop::globalWindowController()->updateAllWindowsDecorations();",
                "for (auto const& m : State::monitorState()->monitors()) {",
                "if (!m)",
                "continue;",
                "m->m_forceFullFrames = 2;",
                "g_pHyprRenderer->damageMonitor(m);",
                "m->scheduleFrame();",
            ),
            Path("src/desktop/state/GlobalWindowController.cpp"): (
                "void CGlobalWindowController::updateAllWindowsDecorations() const {",
                "for (auto const& w : Desktop::windowState()->windows()) {",
                "if (!w->m_isMapped)",
                "continue;",
                "w->updateDecorationValues();",
            ),
            Path("src/desktop/view/Window.cpp"): (
                *window_decoration_construction,
                *window_decoration_update_056,
                *rounding_window_requirements,
                *window_prefix,
                "if (Fullscreen::controller()->getFullscreenModes(m_self.lock()).internal == Fullscreen::FSMODE_FULLSCREEN) {",
                "*alpha(WINDOW_ALPHA_ACTIVE) = m_ruleApplicator->alphaFullscreen().valueOrDefault().applyAlpha(*PFULLSCREENALPHA);",
                "} else { if (m_self == Desktop::focusState()->window()) *alpha(WINDOW_ALPHA_ACTIVE) = m_ruleApplicator->alpha().valueOrDefault().applyAlpha(*PACTIVEALPHA); else *alpha(WINDOW_ALPHA_ACTIVE) = m_ruleApplicator->alphaInactive().valueOrDefault().applyAlpha(*PINACTIVEALPHA); }",
                *window_dim,
                "updateWindowDecos();",
                *window_map_prefix,
                "if (!m_ruleApplicator->noFocus().valueOrDefault() && !m_noInitialFocus && (!isX11OverrideRedirect() || (m_isX11 && m_xwaylandSurface->wantsFocus())) && !workspaceSilent && !monitorSilent && (!PFORCEFOCUS || PFORCEFOCUS == m_self.lock()) && !g_pInputManager->isConstrained()) {",
                *window_map_values,
            ),
            Path("src/render/Renderer.cpp"): renderer_056,
            Path("src/output/Monitor.cpp"): (
                *monitor_scale_requirements_056,
                "void CMonitor::setSpecialWorkspaceVisualState(bool active) {",
                'static auto PDIMSPECIAL = CConfigValue<Config::FLOAT>("decoration:dim_special");',
                'static auto PBLURSPECIAL = CConfigValue<Config::INTEGER>("decoration:blur:special");',
                'static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");',
                'const auto ANIM = active ? "specialWorkspaceIn" : "specialWorkspaceOut";',
                "m_specialDim->setConfig(Config::animationTree()->getAnimationPropertyConfig(ANIM));",
                "*m_specialDim = active ? *PDIMSPECIAL : 0.F;",
                "m_specialBlur->setConfig(Config::animationTree()->getAnimationPropertyConfig(ANIM));",
                "*m_specialBlur = active && *PBLURSPECIAL && *PBLUR ? 1.F : 0.F;",
                "void CMonitor::setSpecialWorkspace(const PHLWORKSPACE& pWorkspace) {",
                "setSpecialWorkspaceVisualState(!!pWorkspace);",
                "PMONITOR->setSpecialWorkspaceVisualState(false);",
            ),
            Path("src/desktop/state/LayerFadeout.cpp"): (
                "SP<CLayerFadeout> CLayerFadeout::create(PHLLS layer, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {",
                'static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");',
                "if (*PDIMAROUND && layer->m_ruleApplicator->dimAround().valueOrDefault())",
                "fadeout->m_effects.dimAroundAlpha = *PDIMAROUND;",
            ),
            Path("src/desktop/state/WindowFadeout.cpp"): (
                "SP<CWindowFadeout> CWindowFadeout::create(PHLWINDOW window, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {",
                "fadeout->m_rounding      = window->rounding();",
                "fadeout->m_roundingPower = window->roundingPower();",
                'static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");',
                "if (*PDIMAROUND && window->m_ruleApplicator->dimAround().valueOrDefault())",
                "fadeout->m_effects.dimAroundAlpha = *PDIMAROUND;",
                "if (m_blur && MONITOR) {",
                "effects.preBlur = SFadeoutPreBlur{",
                ".round         = m_rounding,",
                ".roundingPower = m_roundingPower,",
            ),
            Path("src/render/OpenGL.cpp"): opengl_056,
            Path("src/render/pass/Pass.cpp"): pass_056,
            Path("src/desktop/state/PopupFadeout.cpp"): (
                "static bool shouldBlurPopup() {",
                'static CConfigValue PBLURPOPUPS = CConfigValue<Config::INTEGER>("decoration:blur:popups");',
                'static CConfigValue PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");',
                "return *PBLURPOPUPS && *PBLUR;",
                "SP<CPopupFadeout> CPopupFadeout::create(SP<CPopup> popup, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {",
                'static CConfigValue PBLURIGNOREA = CConfigValue<Config::FLOAT>("decoration:blur:popups_ignorealpha");',
                "if (shouldBlurPopup()) {",
                "fadeout->m_effects.textureBlur.enabled = true;",
                "fadeout->m_effects.textureBlur.blockBlurOptimization = true;",
                "if (const auto PLAYER = popup->layerOwner(); PLAYER && PLAYER->m_ruleApplicator->ignoreAlpha().hasValue())",
                "fadeout->m_effects.textureBlur.ignoreAlpha = std::max(PLAYER->m_ruleApplicator->ignoreAlpha().valueOrDefault(), 0.01F);",
                "else",
                "fadeout->m_effects.textureBlur.ignoreAlpha = std::max(*PBLURIGNOREA, 0.01F);",
            ),
            Path("src/render/ShaderLoader.hpp"): (
                *shader_loader_rounding_056,
                *shader_loader_requirements,
            ),
            Path("src/render/Shader.cpp"): shader_uniform_requirements,
            Path("src/render/GLRenderer.cpp"): (
                *gl_renderer_shadow_056,
                *gl_renderer_requirements,
            ),
            Path("src/render/ElementRenderer.cpp"): (
                *element_renderer_requirements,
                *element_renderer_rounding_056,
            ),
            Path("src/render/gl/GLElementRenderer.cpp"): gl_element_renderer_requirements,
            Path("src/render/pass/PreBlurElement.hpp"): preblur_header_requirements,
            Path("src/render/pass/PreBlurElement.cpp"): preblur_implementation_requirements,
            Path("src/render/shaders/glsl/blurprepare.frag"): blurprepare_wrapper_requirements,
            Path("src/render/shaders/glsl/blurprepare.glsl"): (
                *blurprepare_common_requirements[:5],
                "pixColor = toNit(pixColor, srcTFRange);",
                *blurprepare_common_requirements[5:],
            ),
            Path("src/render/shaders/glsl/blurfinish.frag"): blurfinish_wrapper_requirements,
            Path("src/render/shaders/glsl/blurFinish.glsl"): (
                *blurfinish_common_requirements,
                "pixColor = doColorManagement(pixColor, 1.0, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange);",
                "return pixColor;",
            ),
            Path("src/render/shaders/glsl/blur1.frag"): blur1_wrapper_requirements,
            Path("src/render/shaders/glsl/blur1.glsl"): blur1_requirements,
            Path("src/render/shaders/glsl/gain.glsl"): gain_requirements,
            Path("src/desktop/Workspace.cpp"): workspace_update_056,
            Path("src/render/decorations/CHyprBorderDecoration.cpp"): (
                *border_positioning_prefix,
                *border_rounding_requirements,
                "void CHyprBorderDecoration::damageEntire() {",
                "if (!validMapped(m_window) || Fullscreen::controller()->getFullscreenModes(m_window.lock()).internal == Fullscreen::FSMODE_FULLSCREEN)",
                "return;",
                "const auto GLOBAL_BOX = assignedBoxGlobal();",
                "if (GLOBAL_BOX.w <= 0 || GLOBAL_BOX.h <= 0)",
                "return;",
                *border_damage_suffix,
                "for (auto const& m : State::monitorState()->monitors()) {",
                "const CBox monitorBox = {m->m_position, m->m_size};",
                "if (borderExtents.intersection(monitorBox).empty())",
                "continue;",
                "if (!g_pHyprRenderer->shouldRenderWindow(m_window.lock(), m)) {",
                "const CRegion monitorRegion(monitorBox);",
                "borderRegion.subtract(monitorRegion);",
                "g_pHyprRenderer->damageRegion(borderRegion);",
                *border_flags,
            ),
            Path("src/render/decorations/DecorationPositioner.cpp"): positioner_included_box,
            Path("src/render/decorations/CHyprDropShadowDecoration.cpp"): (
                *shadow_damage_prefix,
                "const auto pos = PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);",
                "const auto size = PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);",
                *shadow_damage_suffix,
                "for (auto const& m : State::monitorState()->monitors()) {",
                "if (!g_pHyprRenderer->shouldRenderWindow(PWINDOW, m)) {",
                "const CRegion monitorRegion({m->m_position, m->m_size});",
                "shadowRegion.subtract(monitorRegion);",
                "g_pHyprRenderer->damageRegion(shadowRegion);",
                *shadow_update_prefix,
                "m_lastWindowPos = PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);",
                "m_lastWindowSize = PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);",
                *shadow_update_suffix,
                *shadow_rounding_056,
            ),
            Path("src/desktop/rule/windowRule/WindowRuleApplicator.hpp"): window_rule_rounding_requirements,
            Path("src/desktop/view/Window.hpp"): window_header_056,
            Path("src/render/Renderer.hpp"): renderer_header_056,
            Path("src/render/OpenGL.hpp"): opengl_header_056,
            Path("src/render/decorations/CHyprDropShadowDecoration.hpp"): shadow_header_056,
            Path("src/render/pass/BorderPassElement.hpp"): border_pass_header,
            Path("src/render/pass/RectPassElement.hpp"): rect_pass_header,
            Path("src/render/pass/SurfacePassElement.hpp"): surface_pass_header,
            Path("src/render/pass/TexPassElement.hpp"): tex_pass_header,
            Path("src/render/shaders/glsl/border.frag"): border_fragment_requirements,
            Path("src/render/shaders/glsl/border.glsl"): border_shader_requirements,
            Path("src/render/shaders/glsl/ext.frag"): ext_fragment_requirements,
            Path("src/render/shaders/glsl/quad.frag"): quad_fragment_requirements,
            Path("src/render/shaders/glsl/rounding.glsl"): rounding_shader_requirements,
            Path("src/render/shaders/glsl/shadow.frag"): shadow_fragment_requirements,
            Path("src/render/shaders/glsl/shadow.glsl"): shadow_shader_requirements,
            Path("src/render/shaders/glsl/surface.frag"): surface_fragment_056,
            Path("src/render/decorations/CHyprInnerGlowDecoration.cpp"): glow_decoration_056,
            Path("src/render/shaders/glsl/inner_glow.frag"): glow_fragment_056,
            Path("src/render/shaders/glsl/inner_glow.glsl"): glow_shader_requirements,
            Path("src/protocols/OutputManagement.cpp"): output_scale_requirements,
            Path("src/config/shared/monitor/MonitorRuleManager.cpp"): monitor_rule_requirements_056,
        },
    }


def _assert_appearance_behavior_contract(
    appearance_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify reviewed opacity, dimming, blur, rounding, shadow, and glow behavior."""
    requirements_by_version = _appearance_behavior_contract_requirements()
    if set(requirements_by_version) != set(APPEARANCE_BEHAVIOR_SOURCE_PATHS):
        raise ValueError("appearance behavior patch inventory is incomplete")

    for version, expected_paths in APPEARANCE_BEHAVIOR_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} appearance behavior semantic inventory is incomplete"
            )
        sources = appearance_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} appearance behavior source inventory is incomplete"
            )

        registry = sources[REGISTRY_PATH].decode("utf-8")
        option_paths = (
            "decoration:rounding_power",
            "decoration:active_opacity",
            "decoration:inactive_opacity",
            "decoration:fullscreen_opacity",
            "decoration:shadow:range",
            "decoration:shadow:render_power",
            "decoration:shadow:sharp",
            "decoration:shadow:offset",
            "decoration:shadow:scale",
            "decoration:glow:enabled",
            "decoration:glow:range",
            "decoration:glow:render_power",
            "decoration:dim_modal",
            "decoration:dim_inactive",
            "decoration:dim_strength",
            "decoration:dim_special",
            "decoration:dim_around",
            "decoration:border_part_of_window",
            "decoration:blur:size",
            "decoration:blur:passes",
            "decoration:blur:ignore_opacity",
            "decoration:blur:new_optimizations",
            "decoration:blur:xray",
            "decoration:blur:noise",
            "decoration:blur:contrast",
            "decoration:blur:brightness",
            "decoration:blur:vibrancy",
            "decoration:blur:vibrancy_darkness",
            "decoration:blur:special",
            "decoration:blur:popups",
            "decoration:blur:popups_ignorealpha",
            "decoration:blur:input_methods",
            "decoration:blur:input_methods_ignorealpha",
        )
        for option_path in option_paths:
            if registry.count(f'"{option_path}"') != 1:
                raise ValueError(
                    f"Hyprland {version} appearance registry count changed in "
                    f"{REGISTRY_PATH} for {option_path}"
                )

        border_path = Path(
            "src/render/decorations/CHyprBorderDecoration.cpp"
        )
        positioner_path = Path(
            "src/render/decorations/DecorationPositioner.cpp"
        )
        shadow_path = Path(
            "src/render/decorations/CHyprDropShadowDecoration.cpp"
        )
        workspace_path = Path("src/desktop/Workspace.cpp")
        border = sources[border_path].decode("utf-8")
        positioner = sources[positioner_path].decode("utf-8")
        shadow = sources[shadow_path].decode("utf-8")
        workspace = sources[workspace_path].decode("utf-8")
        border_option = '"decoration:border_part_of_window"'

        if border.count(border_option) != 1:
            raise ValueError(
                f"Hyprland {version} appearance border-part consumer count "
                f"changed in {border_path}"
            )
        for path in expected_paths:
            if path in (REGISTRY_PATH, border_path):
                continue
            if border_option in sources[path].decode("utf-8"):
                raise ValueError(
                    f"Hyprland {version} appearance border-part literal "
                    f"consumer count changed in {path}"
                )
        if (
            border.count("DECORATION_PART_OF_MAIN_WINDOW") != 1
            or positioner.count("DECORATION_PART_OF_MAIN_WINDOW") != 1
        ):
            raise ValueError(
                f"Hyprland {version} appearance border-part flag count changed "
                f"in {border_path} or {positioner_path}"
            )
        if border.count("doesntWantBorders()") != 4:
            raise ValueError(
                f"Hyprland {version} appearance border exclusion count "
                f"changed in {border_path}"
            )
        if (
            positioner.count("getBoxWithIncludedDecos") != 1
            or shadow.count("getBoxWithIncludedDecos") != 1
            or shadow.count("m_lastWindowBoxWithDecos") != 2
        ):
            raise ValueError(
                f"Hyprland {version} appearance included-decoration cache "
                f"count changed in {positioner_path} or {shadow_path}"
            )
        if workspace.count("updateWindowDecos") != 2:
            raise ValueError(
                f"Hyprland {version} appearance workspace refresh count "
                f"changed in {workspace_path}"
            )

        window_path = Path("src/desktop/view/Window.cpp")
        window = sources[window_path].decode("utf-8")
        rounding_option = '"decoration:rounding_power"'
        rounding_literal_counts = {
            REGISTRY_PATH: 1,
            Path(
                "src/desktop/rule/windowRule/WindowRuleApplicator.hpp"
            ): 1,
            window_path: 2,
        }
        for path in expected_paths:
            expected_count = rounding_literal_counts.get(path, 0)
            if sources[path].decode("utf-8").count(rounding_option) != expected_count:
                raise ValueError(
                    f"Hyprland {version} appearance rounding-power literal "
                    f"consumer count changed in {path}"
                )
        rounding_window_counts = {
            "const int ROUNDINGPOWER = roundingPower();": 1,
            "std::pow(": 12,
            "(roundingPower / 2.0)": 1,
            "std::clamp(*PROUNDINGPOWER, 1.F, 10.F)": 1,
        }
        for fragment, expected_count in rounding_window_counts.items():
            if window.count(fragment) != expected_count:
                raise ValueError(
                    f"Hyprland {version} appearance rounding-power structural "
                    f"count changed in {window_path}"
                )
        if (
            window.count(
                "pWindow->addWindowDeco(makeUnique<CHyprDropShadowDecoration>(pWindow));"
            )
            != 2
            or window.count(
                "pWindow->addWindowDeco(makeUnique<CHyprBorderDecoration>(pWindow));"
            )
            != 2
            or window.count("wd->updateWindow(m_self.lock());") != 1
        ):
            raise ValueError(
                f"Hyprland {version} appearance window-decoration chain "
                f"count changed in {window_path}"
            )
        expected_window_uses = {
            "decoration:active_opacity": 2,
            "decoration:inactive_opacity": 2,
            "decoration:fullscreen_opacity": 1,
            "decoration:dim_modal": 1,
            "decoration:dim_inactive": 1,
            "decoration:dim_strength": 2,
        }
        for option_path, expected_count in expected_window_uses.items():
            if window.count(f'"{option_path}"') != expected_count:
                raise ValueError(
                    f"Hyprland {version} appearance runtime gate count changed in "
                    f"{window_path} for {option_path}"
                )

        renderer_path = Path("src/render/Renderer.cpp")
        renderer = sources[renderer_path].decode("utf-8")
        expected_live_rounding_uses = 2 if version == "0.55.0" else 1
        if (
            renderer.count("pWindow->roundingPower()")
            != expected_live_rounding_uses
            or renderer.count(
                "renderdata.roundingPower = standalone || "
                "renderdata.dontRound ? 2.0f : pWindow->roundingPower();"
            )
            != 1
        ):
            raise ValueError(
                f"Hyprland {version} appearance rounding-power renderer "
                f"count changed in {renderer_path}"
            )
        rounding_shader_inverse_counts = {
            Path("src/render/shaders/glsl/border.glsl"): 2,
            Path("src/render/shaders/glsl/rounding.glsl"): 1,
            Path("src/render/shaders/glsl/shadow.glsl"): 1,
        }
        for path, expected_count in rounding_shader_inverse_counts.items():
            if (
                sources[path]
                .decode("utf-8")
                .count("1.0 / roundingPower")
                != expected_count
            ):
                raise ValueError(
                    f"Hyprland {version} appearance rounding-power shader "
                    f"count changed in {path}"
                )
        expected_renderer_uses = {
            "decoration:dim_around": 3 if version == "0.55.0" else 2,
            "decoration:dim_special": 1 if version == "0.55.0" else 0,
        }
        for option_path, expected_count in expected_renderer_uses.items():
            if renderer.count(f'"{option_path}"') != expected_count:
                raise ValueError(
                    f"Hyprland {version} appearance renderer count changed in "
                    f"{renderer_path} for {option_path}"
                )

        shadow_runtime_counts = {
            "decoration:shadow:range": (shadow_path, 1),
            "decoration:shadow:render_power": (
                Path("src/render/OpenGL.cpp"),
                1,
            ),
            "decoration:shadow:sharp": (
                shadow_path,
                1 if version == "0.55.0" else 2,
            ),
            "decoration:shadow:offset": (shadow_path, 1),
            "decoration:shadow:scale": (shadow_path, 1),
        }
        for path in expected_paths:
            if path == REGISTRY_PATH:
                continue
            source = sources[path].decode("utf-8")
            for option_path, (consumer_path, consumer_count) in (
                shadow_runtime_counts.items()
            ):
                expected_count = consumer_count if path == consumer_path else 0
                if source.count(f'"{option_path}"') != expected_count:
                    raise ValueError(
                        f"Hyprland {version} appearance shadow-rendering literal "
                        f"consumer count changed in {path} for {option_path}"
                    )

        glow_path = Path(
            "src/render/decorations/CHyprInnerGlowDecoration.cpp"
        )
        glow_fragment_path = Path("src/render/shaders/glsl/inner_glow.frag")
        glow_shader_path = Path("src/render/shaders/glsl/inner_glow.glsl")
        output_management_path = Path("src/protocols/OutputManagement.cpp")
        monitor_rule_path = Path(
            "src/config/shared/monitor/MonitorRuleManager.cpp"
        )
        monitor_scale_path = (
            Path("src/helpers/Monitor.cpp")
            if version == "0.55.0"
            else Path("src/output/Monitor.cpp")
        )
        glow_runtime_counts = {
            "decoration:glow:enabled": (
                glow_path,
                1 if version == "0.55.0" else 2,
            ),
            "decoration:glow:range": (glow_path, 1),
            "decoration:glow:render_power": (
                glow_path
                if version == "0.55.0"
                else Path("src/render/OpenGL.cpp"),
                1,
            ),
        }
        window_path = Path("src/desktop/view/Window.cpp")
        for path in expected_paths:
            if path == REGISTRY_PATH:
                continue
            source = sources[path].decode("utf-8")
            for option_path, (consumer_path, consumer_count) in (
                glow_runtime_counts.items()
            ):
                expected_count = consumer_count if path == consumer_path else 0
                if (
                    option_path == "decoration:glow:enabled"
                    and path == window_path
                ):
                    expected_count = 1
                if source.count(f'"{option_path}"') != expected_count:
                    raise ValueError(
                        f"Hyprland {version} appearance glow literal consumer "
                        f"count changed in {path} for {option_path}"
                    )

        glow = sources[glow_path].decode("utf-8")
        glow_fragment = sources[glow_fragment_path].decode("utf-8")
        glow_shader = sources[glow_shader_path].decode("utf-8")
        output_management = sources[output_management_path].decode("utf-8")
        monitor_rule = sources[monitor_rule_path].decode("utf-8")
        monitor_scale = sources[monitor_scale_path].decode("utf-8")
        expected_scaled_calls = 1 if version == "0.55.0" else 2
        if glow.count("GLOWSIZE * pMonitor->m_scale") != expected_scaled_calls:
            raise ValueError(
                f"Hyprland {version} appearance glow scaled-integer transport "
                f"count changed in {glow_path}"
            )
        if (
            glow_fragment.count('#include "inner_glow.glsl"') != 1
            or glow_fragment.count("getInnerGlow(") != 1
        ):
            raise ValueError(
                f"Hyprland {version} appearance glow wrapper count changed in "
                f"{glow_fragment_path}"
            )
        if (
            glow_shader.count("distFromEdge / range") != 1
            or glow_shader.count("max(k - abs(a - b), 0.0) / k") != 1
            or glow_shader.count("float k = range;") != 1
        ):
            raise ValueError(
                f"Hyprland {version} appearance glow shader division count "
                f"changed in {glow_shader_path}"
            )
        if output_management.count("if (scale < 0.1 || scale > 10.0)") != 1:
            raise ValueError(
                f"Hyprland {version} appearance output-scale admission count "
                f"changed in {output_management_path}"
            )
        if monitor_rule.count("rule.m_scale = CONFIG->scale;") != 1:
            raise ValueError(
                f"Hyprland {version} appearance monitor-rule scale count changed "
                f"in {monitor_rule_path}"
            )
        if monitor_scale.count("m_scale = RULE->m_scale;") != 1:
            raise ValueError(
                f"Hyprland {version} appearance monitor scale-consumer count "
                f"changed in {monitor_scale_path}"
            )

        shadow_offset_structure_counts = {
            "fullBox.scaleFromCenter(SHADOWSCALE).translate({(*PSHADOWOFFSET).x, (*PSHADOWOFFSET).y});": 1,
            "if (fullBox.width < 1 || fullBox.height < 1)": 1,
            "fullBox.scale(pMonitor->m_scale).round();": 1,
            "g_pDecorationPositioner->repositionDeco(this);": 1,
            "reposition();": 1,
        }
        for fragment, expected_count in shadow_offset_structure_counts.items():
            if shadow.count(fragment) != expected_count:
                raise ValueError(
                    f"Hyprland {version} appearance shadow-offset structural "
                    f"count changed in {shadow_path}"
                )

        expected_blur_runtime_uses: dict[Path, dict[str, int]] = {
            renderer_path: {
                "decoration:blur:new_optimizations": 2,
                "decoration:blur:xray": 1,
                "decoration:blur:special": 1 if version == "0.55.0" else 0,
                "decoration:blur:popups": 1,
                "decoration:blur:popups_ignorealpha": (
                    2 if version == "0.55.0" else 1
                ),
                "decoration:blur:input_methods": 1,
                "decoration:blur:input_methods_ignorealpha": 1,
            },
            Path("src/render/OpenGL.cpp"): {
                "decoration:blur:size": 1,
                "decoration:blur:passes": 1,
                "decoration:blur:ignore_opacity": 1,
                "decoration:blur:new_optimizations": 1,
                "decoration:blur:xray": 1,
                "decoration:blur:brightness": 2,
                "decoration:blur:contrast": 1,
                "decoration:blur:noise": 1,
                "decoration:blur:vibrancy": 1,
                "decoration:blur:vibrancy_darkness": 1,
            },
            Path("src/render/pass/Pass.cpp"): {
                "decoration:blur:size": 1,
                "decoration:blur:passes": 1,
            },
        }
        if version == "0.56.1":
            expected_blur_runtime_uses.update(
                {
                    Path("src/output/Monitor.cpp"): {
                        "decoration:blur:special": 1,
                    },
                    Path("src/desktop/state/PopupFadeout.cpp"): {
                        "decoration:blur:popups": 1,
                        "decoration:blur:popups_ignorealpha": 1,
                    },
                }
            )
        for path, expected_counts in expected_blur_runtime_uses.items():
            source = sources[path].decode("utf-8")
            for option_path, expected_count in expected_counts.items():
                if source.count(f'"{option_path}"') != expected_count:
                    raise ValueError(
                        f"Hyprland {version} appearance blur runtime count "
                        f"changed in {path} for {option_path}"
                    )

        modulation_runtime_counts = {
            "decoration:blur:brightness": 2,
            "decoration:blur:contrast": 1,
            "decoration:blur:noise": 1,
            "decoration:blur:vibrancy": 1,
            "decoration:blur:vibrancy_darkness": 1,
        }
        for path in expected_paths:
            if path == REGISTRY_PATH:
                continue
            source = sources[path].decode("utf-8")
            for option_path, opengl_count in modulation_runtime_counts.items():
                expected_count = (
                    opengl_count
                    if path == Path("src/render/OpenGL.cpp")
                    else 0
                )
                if source.count(f'"{option_path}"') != expected_count:
                    raise ValueError(
                        f"Hyprland {version} appearance blur-modulation literal "
                        f"consumer count changed in {path} for {option_path}"
                    )

        refresher_path = Path(
            "src/config/supplementary/propRefresher/PropRefresher.cpp"
        )
        refresher = sources[refresher_path].decode("utf-8")
        if (
            refresher.count("REFRESH_BLUR_FB") != 1
            or refresher.count("m_blurFBDirty") != 1
        ):
            raise ValueError(
                f"Hyprland {version} appearance blur refresh count changed in "
                f"{refresher_path}"
            )

        if version == "0.56.1":
            monitor_path = Path("src/output/Monitor.cpp")
            monitor = sources[monitor_path].decode("utf-8")
            if monitor.count('"decoration:dim_special"') != 1:
                raise ValueError(
                    "Hyprland 0.56.1 appearance special-dim transition count "
                    f"changed in {monitor_path}"
                )
            if monitor.count("setSpecialWorkspaceVisualState") != 3:
                raise ValueError(
                    "Hyprland 0.56.1 appearance special-dim transition cache "
                    f"changed in {monitor_path}"
                )
            if renderer.count("m_specialDim") != 2:
                raise ValueError(
                    "Hyprland 0.56.1 appearance cached special-dim renderer "
                    f"changed in {renderer_path}"
                )
            if renderer.count("m_specialBlur") != 2:
                raise ValueError(
                    "Hyprland 0.56.1 appearance cached special-blur renderer "
                    f"changed in {renderer_path}"
                )
            if "setSpecialWorkspaceVisualState" in sources[refresher_path].decode(
                "utf-8"
            ):
                raise ValueError(
                    "Hyprland 0.56.1 appearance special-dim transition cache "
                    f"changed in {refresher_path}"
                )

            for fadeout_path in (
                Path("src/desktop/state/LayerFadeout.cpp"),
                Path("src/desktop/state/WindowFadeout.cpp"),
            ):
                fadeout = sources[fadeout_path].decode("utf-8")
                if fadeout.count('"decoration:dim_around"') != 1:
                    raise ValueError(
                        "Hyprland 0.56.1 appearance fadeout dim-around count "
                        f"changed in {fadeout_path}"
                    )

            window_fadeout_path = Path("src/desktop/state/WindowFadeout.cpp")
            window_fadeout = sources[window_fadeout_path].decode("utf-8")
            if (
                window_fadeout.count(
                    "m_roundingPower = window->roundingPower();"
                )
                != 1
                or window_fadeout.count(
                    ".roundingPower = m_roundingPower,"
                )
                != 1
            ):
                raise ValueError(
                    "Hyprland 0.56.1 appearance rounding-power fadeout cache "
                    f"count changed in {window_fadeout_path}"
                )

        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(
                    re.sub(r"\s+", " ", fragment)
                    for fragment in requirements[path]
                ),
                "appearance behavior",
            )


def _advanced_runtime_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    registry_prefix = (
        'MS<Bool>("misc:disable_hyprland_logo", "disables the random Hyprland logo / anime girl background. :(", false),',
        'MS<Bool>("misc:disable_splash_rendering", "disables the Hyprland splash rendering.", false),',
        'MS<Bool>("misc:allow_session_lock_restore", "if true, will allow you to restart a lockscreen app in case it crashes.", false),',
        'MS<Bool>("misc:session_lock_xray", "keep rendering workspaces below your lockscreen", false),',
    )
    registry_tail = (
        'MS<Int>("misc:render_unfocused_fps", "the maximum limit for renderunfocused windows\' fps in the background", 15, {.min = 1, .max = 120}),',
        'MS<Int>("misc:lockdead_screen_delay", "the delay in ms after the lockdead screen appears.", 1000, {.min = 0, .max = 5000}),',
        'MS<Bool>("misc:screencopy_force_8b", "forces 8 bit screencopy", true),',
        'MS<Bool>("misc:disable_scale_notification", "disables notification popup when a monitor fails to set a suitable scale", false),',
    )
    registry_xwayland = (
        'MS<Bool>("xwayland:use_nearest_neighbor", "uses the nearest neighbor filtering for xwayland apps, making them pixelated rather than blurry", true),',
    )
    registry_direct_scanout = (
        'MS<Int>("render:direct_scanout", "Enables direct scanout.", 0, {.min = 0, .max = 2, .map = OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}}),',
    )
    registry_expand_undersized_textures = (
        'MS<Bool>("render:expand_undersized_textures", "Whether to expand textures that have not yet resized to be larger.", true),',
    )
    registry_xp_mode = (
        'MS<Bool>("render:xp_mode", "Disable back buffer and bottom layer rendering.", false),',
    )
    registry_fp16 = (
        'MS<Int>("render:use_fp16", "Use experimental internal FP16 buffer.", 2, {.min = 0, .max = 2, .map = OptionMap{{"disable", 0}, {"enable", 1}, {"auto", 2}}}),',
        'MS<Int>("render:fp16_sdr_tf", "Internal workbuffer transfer function for fp16 in SDR mode", 0, {.min = 0, .max = 1, .map = OptionMap{{"monitor", 0}, {"linear", 1}}}),',
    )
    registry_input_capture = (
        'MS<Bool>("input-capture:capture_modifiers", "If enabled, modifiers are also captured and sent to the program", false),',
        'MS<Bool>("input-capture:enforce_barriers", "If enabled, throw a wayland error when a invalid barrier is received", true),',
    )
    reload_requirements_055 = (
        "for (const auto& v : m_configValues) {",
        "v.second->reset();",
        "void CConfigManager::postConfigReload() {",
        "handlePluginLoads();",
        "Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);",
        "Event::bus()->m_events.config.reloaded.emit();",
    )
    reload_requirements_056 = (
        "for (const auto& v : m_configValues) {",
        "v.second->reset();",
        "v.second->resetSetByUser();",
        "void CConfigManager::postConfigReload() {",
        "handlePluginLoads();",
        "Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);",
        "Event::bus()->m_events.config.reloaded.emit();",
    )
    input_capture_modifiers_prefix = (
        "void CInputManager::onKeyboardMod(SP<IKeyboard> pKeyboard) {",
        'static auto PSENDMOD = CConfigValue<Hyprlang::INT>("input-capture:capture_modifiers");',
        "if (!pKeyboard->m_enabled) return;",
        "const bool DISALLOWACTION = pKeyboard->isVirtual() && shouldIgnoreVirtualKeyboard(pKeyboard);",
        "const auto IME = m_relay.m_inputMethod.lock();",
        "const bool HASIME = IME && IME->hasGrab();",
        "const bool USEIME = HASIME && !DISALLOWACTION;",
        "auto MODS = pKeyboard->m_modifiersState;",
        "if (*PSENDMOD) {",
        "PROTO::inputCapture->modifiers(MODS.depressed, MODS.latched, MODS.locked, MODS.group);",
    )
    input_capture_modifiers_suffix = (
        "if (USEIME || !HASIME) {",
        "const auto ALLMODS = shareModsFromAllKBs(MODS.depressed);",
        "MODS.depressed = ALLMODS;",
        "m_lastMods = MODS.depressed; // for hyprland keybinds use; not for sending to seat",
        "if (USEIME) {",
        "IME->setKeyboard(pKeyboard);",
        "IME->sendMods(MODS.depressed, MODS.latched, MODS.locked, MODS.group);",
        "g_pSeatManager->setKeyboard(pKeyboard);",
        "g_pSeatManager->sendKeyboardMods(MODS.depressed, MODS.latched, MODS.locked, MODS.group);",
        "updateKeyboardsLeds(pKeyboard);",
    )
    input_capture_modifiers_0560 = (
        *input_capture_modifiers_prefix,
        "if (PROTO::inputCapture->isCaptured()) return;",
        *input_capture_modifiers_suffix,
    )
    input_capture_modifiers_0561 = (
        *input_capture_modifiers_prefix,
        "if (PROTO::inputCapture->isCaptured()) { m_lastMods = shareModsFromAllKBs(MODS.depressed); return; }",
        *input_capture_modifiers_suffix,
    )
    input_capture_barriers = (
        "static eValidResult isBarrierValidAgainstMonitor(int x1, int y1, int x2, int y2, PHLMONITOR monitor) {",
        "int mx1 = monitor->m_position.x;",
        "int my1 = monitor->m_position.y;",
        "const int width = static_cast<int>(monitor->m_size.x);",
        "const int height = static_cast<int>(monitor->m_size.y);",
        "int mx2 = mx1 + width - 1;",
        "int my2 = my1 + height - 1;",
        "if (x1 == x2) {",
        "if (x1 != mx1 && x1 != mx2 + 1)",
        "if (y1 != my1 || y2 != my2) {",
        "if ((my1 <= y1 && y1 <= my2) || (my1 <= y2 && y2 <= my2))",
        "if (y1 != my1 && y1 != my2 + 1)",
        "if (x1 != mx1 || x2 != mx2) {",
        "if ((mx1 <= x1 && x1 <= mx2) || (mx1 <= x2 && x2 <= mx2))",
        "return VALID;",
        "static bool isBarrierValid(int x1, int y1, int x2, int y2) {",
        "if (x1 != x2 && y1 != y2)",
        "if (x1 == x2 && y1 == y2)",
        "if (x1 > x2)",
        "std::swap(x1, x2);",
        "if (y1 > y2)",
        "std::swap(y1, y2);",
        "int valid = 0;",
        "int partial = 0;",
        "for (auto& o : State::monitorState()->monitors()) {",
        "switch (isBarrierValidAgainstMonitor(x1, y1, x2, y2, o)) {",
        "case VALID: valid++; break;",
        "case PARTIAL: partial++; break;",
        "case INVALID: break;",
        "return valid == 1 && partial == 0;",
        "void CInputCaptureResource::onAddBarrier(uint32_t zoneSet, uint32_t id, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {",
        'static auto PENFORCEBARRIERS = CConfigValue<Hyprlang::INT>("input-capture:enforce_barriers");',
        "const int32_t sx1 = static_cast<int32_t>(x1);",
        "const int32_t sy1 = static_cast<int32_t>(y1);",
        "const int32_t sx2 = static_cast<int32_t>(x2);",
        "const int32_t sy2 = static_cast<int32_t>(y2);",
        "bool valid = isBarrierValid(sx1, sy1, sx2, sy2);",
        "if (!valid) {",
        'Log::logger->log(Log::INFO, "[input-capture]({}) Barrier {} is invalid [{}, {}], [{}, {}]", m_sessionId.c_str(), id, sx1, sy1, sx2, sy2);',
        "if (*PENFORCEBARRIERS) {",
        'm_resource->error(HYPRLAND_INPUT_CAPTURE_V1_ERROR_INVALID_BARRIER, "The barrier id " + std::to_string(id) + " is invalid");',
        "return;",
        'Log::logger->log(Log::INFO, "[input-capture]({}) Barrier {} [{}, {}], [{}, {}] added", m_sessionId.c_str(), id, sx1, sy1, sx2, sy2);',
        "PROTO::inputCapture->addBarrier({.sessionId = m_sessionId, .id = id, .x1 = sx1, .y1 = sy1, .x2 = sx2, .y2 = sy2});",
        "void CInputCaptureResource::modifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group) {",
        "m_eis->sendModifiers(modsDepressed, modsLatched, modsLocked, group);",
        "bool CInputCaptureProtocol::isCaptured() {",
        "return active != nullptr;",
        "void CInputCaptureProtocol::modifiers(uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {",
        "if (active)",
        "active->modifiers(mods_depressed, mods_latched, mods_locked, group);",
    )
    session_lock_common = (
        "void CSessionLockManager::onNewSessionLock(",
        'static auto PALLOWRELOCK = CConfigValue<Config::INTEGER>("misc:allow_session_lock_restore");',
    )
    session_lock_tail = (
        "pLock->sendDenied();",
        "if (m_sessionLock && !clientDenied() && !clientLocked())",
        "m_sessionLock->lockTimer.reset();",
        "bool CSessionLockManager::shallConsiderLockMissing() {",
        "if (!m_sessionLock)",
        "return true;",
        'static auto LOCKDEAD_SCREEN_DELAY = CConfigValue<Config::INTEGER>("misc:lockdead_screen_delay");',
        "return m_sessionLock->lockTimer.getMillis() > *LOCKDEAD_SCREEN_DELAY;",
    )
    renderer_prefix = (
        "Event::bus()->m_events.window.updateRules.listen(",
        "if (window->m_ruleApplicator->renderUnfocused().valueOrDefault())",
        "addWindowToRenderUnfocused(window);",
        "m_renderUnfocusedTimer = makeShared<CEventLoopTimer>(",
        'static auto PFPS = CConfigValue<Config::INTEGER>("misc:render_unfocused_fps");',
        "if (m_renderUnfocused.empty())",
        "for (auto& w : m_renderUnfocused)",
        "if (!w->wlSurface() || !w->wlSurface()->resource() || shouldRenderWindow(w.lock()))",
    )
    renderer_after_timer = (
        "if (dirty)",
        "std::erase_if(m_renderUnfocused, [](const auto& e) { return !e || !e->m_ruleApplicator->renderUnfocused().valueOr(false); });",
        "if (!m_renderUnfocused.empty())",
        "m_renderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));",
    )
    renderer_xwayland = (
        'static auto PXWLUSENN = CConfigValue<Config::INTEGER>("xwayland:use_nearest_neighbor");',
        "if ((pWindow->m_isX11 && *PXWLUSENN) || pWindow->m_ruleApplicator->nearestNeighbor().valueOrDefault())",
        "renderdata.useNearestNeighbor = true;",
    )
    renderer_xwayland_tail_055 = (
        "m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));",
        "renderdata.useNearestNeighbor = false;",
    )
    renderer_xwayland_tail_056 = (
        "addPassElement(makeUnique<CSurfacePassElement>(renderdata));",
        "renderdata.useNearestNeighbor = false;",
    )
    renderer_workspace_xray = (
        "void IHyprRenderer::renderAllClientsForWorkspace(",
        'static auto PXPMODE = CConfigValue<Config::INTEGER>("render:xp_mode");',
        'static auto PSESSIONLOCKXRAY = CConfigValue<Config::INTEGER>("misc:session_lock_xray");',
        "if UNLIKELY (g_pSessionLockManager->isSessionLocked() && !*PSESSIONLOCKXRAY) {",
        "if (g_pSessionLockManager->shallConsiderLockMissing() || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied())",
        "return;",
    )
    renderer_xp_no_workspace_055 = (
        "if UNLIKELY (!pWorkspace) { // allow rendering without a workspace. "
        "In this case, just render layers. renderBackground(pMonitor); for "
        "(auto const& ls : pMonitor->m_layerSurfaceLayers["
        "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { renderLayer(ls.lock(), "
        "pMonitor, time); } Event::bus()->m_events.render.stage.emit("
        "RENDER_POST_WALLPAPER); for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) "
        "{ renderLayer(ls.lock(), pMonitor, time); } for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) { "
        "renderLayer(ls.lock(), pMonitor, time); } for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) "
        "{ renderLayer(ls.lock(), pMonitor, time); } return; }"
    )
    renderer_xp_active_workspace_055 = (
        "if LIKELY (!*PXPMODE) { renderBackground(pMonitor); for (auto "
        "const& ls : pMonitor->m_layerSurfaceLayers["
        "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { renderLayer(ls.lock(), "
        "pMonitor, time); } Event::bus()->m_events.render.stage.emit("
        "RENDER_POST_WALLPAPER); for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) "
        "{ renderLayer(ls.lock(), pMonitor, time); } }"
    )
    renderer_xp_no_workspace_056 = (
        "if UNLIKELY (!pWorkspace) { // allow rendering without a workspace. "
        "In this case, just render layers. renderBackground(pMonitor); for "
        "(auto const& ls : pMonitor->m_layerSurfaceLayers["
        "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { renderLayer(ls.lock(), "
        "pMonitor, time); } renderFadeouts(pMonitor, "
        "Desktop::FADEOUT_PLANE_LAYER_BACKGROUND); Event::bus()->m_events."
        "render.stage.emit(RENDER_POST_WALLPAPER); for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) "
        "{ renderLayer(ls.lock(), pMonitor, time); } renderFadeouts(pMonitor, "
        "Desktop::FADEOUT_PLANE_LAYER_BOTTOM); for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) { "
        "renderLayer(ls.lock(), pMonitor, time); } renderFadeouts(pMonitor, "
        "Desktop::FADEOUT_PLANE_LAYER_TOP); for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) "
        "{ renderLayer(ls.lock(), pMonitor, time); } renderFadeouts(pMonitor, "
        "Desktop::FADEOUT_PLANE_LAYER_OVERLAY); return; }"
    )
    renderer_xp_active_workspace_056 = (
        "if LIKELY (!*PXPMODE) { renderBackground(pMonitor); for (auto "
        "const& ls : pMonitor->m_layerSurfaceLayers["
        "ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) { renderLayer(ls.lock(), "
        "pMonitor, time); } renderFadeouts(pMonitor, "
        "Desktop::FADEOUT_PLANE_LAYER_BACKGROUND); Event::bus()->m_events."
        "render.stage.emit(RENDER_POST_WALLPAPER); for (auto const& ls : "
        "pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) "
        "{ renderLayer(ls.lock(), pMonitor, time); } renderFadeouts(pMonitor, "
        "Desktop::FADEOUT_PLANE_LAYER_BOTTOM); }"
    )
    renderer_background = (
        "void IHyprRenderer::renderBackground(PHLMONITOR pMonitor) {",
        'static auto PRENDERTEX = CConfigValue<Config::INTEGER>("misc:disable_hyprland_logo");',
        'static auto PBACKGROUNDCOLOR = CConfigValue<Config::INTEGER>("misc:background_color");',
        'static auto PNOSPLASH = CConfigValue<Config::INTEGER>("misc:disable_splash_rendering");',
        "if (*PRENDERTEX /* inverted cfg flag */ || pMonitor->m_backgroundOpacity->isBeingAnimated())",
        "if (!*PRENDERTEX) {",
        "if (!pMonitor->m_background) pMonitor->m_background = getBackground(pMonitor);",
        "if (!pMonitor->m_background) m_renderPass.add(makeUnique<CClearPassElement>(CClearPassElement::SClearData{CHyprColor(*PBACKGROUNDCOLOR)}));",
        "data.tex = pMonitor->m_background;",
        "if (!*PNOSPLASH) {",
        "if (!pMonitor->m_splash) pMonitor->m_splash = renderSplash(",
        "if (pMonitor->m_splash) {",
        "data.tex = pMonitor->m_splash;",
        "void IHyprRenderer::requestBackgroundResource() {",
        'static auto PNOWALLPAPER = CConfigValue<Config::INTEGER>("misc:disable_hyprland_logo");',
        "if (*PNOWALLPAPER) return;",
    )
    renderer_lockscreen = (
        "void IHyprRenderer::renderLockscreen(",
        "const bool RENDERPRIMER = g_pSessionLockManager->shallConsiderLockMissing() || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied();",
        "if (RENDERPRIMER) renderSessionLockPrimer(pMonitor);",
        "const bool RENDERLOCKMISSING = (PSLS.expired() || g_pSessionLockManager->clientDenied()) && g_pSessionLockManager->shallConsiderLockMissing();",
        "if (RENDERLOCKMISSING)",
        "renderSessionLockMissing(pMonitor);",
        "void IHyprRenderer::renderSessionLockPrimer(PHLMONITOR pMonitor) {",
        'static auto PSESSIONLOCKXRAY = CConfigValue<Config::INTEGER>("misc:session_lock_xray");',
        "if (*PSESSIONLOCKXRAY) return;",
        "data.color = CHyprColor(0, 0, 0, 1.f);",
        "data.box = CBox{{}, pMonitor->m_pixelSize};",
        "m_renderPass.add(makeUnique<CRectPassElement>(data));",
    )
    renderer_resource_bridge_055 = (
        "bool IHyprRenderer::beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode, SP<IHLBuffer> buffer, SP<IFramebuffer> fb, bool simple) {",
        "m_renderData.pMonitor = pMonitor;",
        "const bool HAS_MIRROR_FB = g_pHyprRenderer->m_renderData.pMonitor->resources()->hasMirrorFB();",
    )
    renderer_resource_bridge_056 = (
        "bool IHyprRenderer::beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode, SP<IHLBuffer> buffer, SP<IFramebuffer> fb, bool simple) {",
        "m_renderData.pMonitor = pMonitor;",
        "const auto RESOURCES = g_pHyprRenderer->m_renderData.pMonitor->resources();",
    )
    renderer_direct_scanout_055 = (
        "const bool canAttemptDirectScanout = pMonitor->canAttemptDirectScanoutFast();",
        "if (canAttemptDirectScanout) {",
        "if (pMonitor->attemptDirectScanout()) {",
        "if (!pMonitor->m_directScanoutIsActive) {",
        "pMonitor->m_previousFSWindow.reset(); // recalc fs settings",
        "pMonitor->m_directScanoutIsActive = true;",
        "handleFullscreenSettings(pMonitor);",
        "return;",
        "} else if (!pMonitor->m_lastScanout.expired() || pMonitor->m_directScanoutIsActive)",
        "pMonitor->handleDSleave();",
    )
    renderer_direct_scanout_056 = (
        "const bool canAttemptDirectScanout = pMonitor->canAttemptDirectScanoutFast();",
        "if (canAttemptDirectScanout) {",
        "if (pMonitor->attemptDirectScanout()) {",
        "if (!pMonitor->needsACopyFB())",
        "pMonitor->resources()->markMirrorFBStale();",
        "if (!pMonitor->m_directScanoutIsActive) {",
        "pMonitor->m_previousFSWindow.reset(); // recalc fs settings",
        "pMonitor->m_directScanoutIsActive = true;",
        "handleFullscreenSettings(pMonitor);",
        "return;",
        "} else if (!pMonitor->m_lastScanout.expired() || pMonitor->m_directScanoutIsActive)",
        "pMonitor->handleDSleave();",
    )
    renderer_frame_and_damage = (
        "renderWorkspace(pMonitor, pMonitor->m_activeWorkspace, NOW, renderBox);",
        "renderLockscreen(pMonitor, NOW, renderBox);",
        "void IHyprRenderer::damageMonitor(PHLMONITOR pMonitor) {",
        "CBox damageBox = {0, 0, INT16_MAX, INT16_MAX};",
        "pMonitor->addDamage(damageBox);",
    )
    renderer_unfocused_tail = (
        "void IHyprRenderer::addWindowToRenderUnfocused(",
        'static auto PFPS = CConfigValue<Config::INTEGER>("misc:render_unfocused_fps");',
        "if (*PFPS <= 0)",
        "return;",
        "if (std::ranges::find(m_renderUnfocused, window) != m_renderUnfocused.end())",
        "m_renderUnfocused.emplace_back(window);",
        "if (!m_renderUnfocusedTimer->armed())",
        "m_renderUnfocusedTimer->updateTimeout(std::chrono::milliseconds(1000 / *PFPS));",
    )
    renderer_work_buffer_description = (
        "NColorManagement::PImageDescription IHyprRenderer::workBufferImageDescription() {",
        "if (!m_renderData.pMonitor)",
        "return LINEAR_IMAGE_DESCRIPTION;",
        "return m_renderData.pMonitor->workBufferImageDescription();",
    )
    element_renderer_surface_uv = (
        "static std::optional<Vector2D> getSurfaceExpectedSize(",
        "if (pSurface->m_current.viewport.hasDestination)",
        "return (pSurface->m_current.viewport.destination * pMonitor->m_scale).round();",
        "if (pSurface->m_current.viewport.hasSource)",
        "return (pSurface->m_current.viewport.source.size() * pMonitor->m_scale).round();",
        "if (WINDOW_SIZE_MISALIGN)",
        "return (pSurface->m_current.size * pMonitor->m_scale).round();",
        "if (CAN_USE_WINDOW)",
        "return (pWindow->getReportedSize() * pMonitor->m_scale).round();",
        "void IElementRenderer::calculateUVForSurface(",
        "if (!pWindow || !pWindow->m_isX11) {",
        'static auto PEXPANDEDGES = CConfigValue<Hyprlang::INT>("render:expand_undersized_textures");',
        "if (pSurface->m_current.viewport.hasSource) {",
        "if (projSize != Vector2D{} && fixMisalignedFSV1) {",
        "const Vector2D PIXELASUV = Vector2D{1, 1} / pSurface->m_current.bufferSize;",
        "const Vector2D MISALIGNMENT = (uvBR - uvTL) * BUFFER_SIZE - projSize;",
        "if (MISALIGNMENT != Vector2D{})",
        "uvBR -= MISALIGNMENT * PIXELASUV;",
        "} else { // if the surface is smaller than our viewport, extend its edges.",
        "const auto MONITOR_WL_SCALE = std::ceil(pMonitor->m_scale);",
        "const bool SCALE_UNAWARE = pMonitor->m_scale != 1.f && (MONITOR_WL_SCALE == pSurface->m_current.scale || !pSurface->m_current.viewport.hasDestination);",
        "const auto EXPECTED_SIZE = getSurfaceExpectedSize(pWindow, pSurface, pMonitor, main).value_or((projSize * pMonitor->m_scale).round());",
        "const auto RATIO = projSize / EXPECTED_SIZE;",
        "if (!SCALE_UNAWARE || MONITOR_WL_SCALE == 1) {",
        "if (*PEXPANDEDGES && !SCALE_UNAWARE && (RATIO.x > 1 || RATIO.y > 1)) {",
        "const auto FIX = RATIO.clamp(Vector2D{1, 1}, Vector2D{1000000, 1000000});",
        "uvBR = uvBR * FIX;",
        "m_renderData.primarySurfaceUVTopLeft = uvTL; m_renderData.primarySurfaceUVBottomRight = uvBR; if (m_renderData.primarySurfaceUVTopLeft == Vector2D() && m_renderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) { // No special UV mods needed m_renderData.primarySurfaceUVTopLeft = Vector2D(-1, -1); m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1); } if (!main || !pWindow) return;",
        "} else { m_renderData.primarySurfaceUVTopLeft = Vector2D(-1, -1); m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1); } }",
    )
    element_renderer = (
        *element_renderer_surface_uv,
        "void IElementRenderer::drawSurface(WP<CSurfacePassElement> element, const CRegion& damage) {",
        "const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier : 1.F);",
        "const float OVERALL_ALPHA = PSURFACE ? PSURFACE->m_overallOpacity : 1.F;",
        "const bool BLUR = m_data.blur && (!TEXTURE->m_opaque || ALPHA < 1.F || OVERALL_ALPHA < 1.F);",
        "calculateUVForSurface(m_data.pWindow, m_data.surface, m_data.pMonitor->m_self.lock(), m_data.mainSurface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);",
        "if (m_data.surfaceCounter == 0 && !m_data.popup) {",
        "if (BLUR) drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{",
        ".blur = true,",
        ".blockBlurOptimization = m_data.blockBlurOptimization,",
        ".allowCustomUV = true,",
        ".discardMode = m_data.discardMode,",
        ".discardOpacity = m_data.discardOpacity,",
        "void IElementRenderer::preDrawSurface(WP<CSurfacePassElement> element, const CRegion& damage) {",
        "m_renderData.useNearestNeighbor = element->m_data.useNearestNeighbor;",
        "drawSurface(element, damage);",
        "m_renderData.useNearestNeighbor = false;",
    )
    tex_pass_declaration = (
        "struct SRenderData {",
        "bool discardActive = false;",
        "bool allowCustomUV = false;",
        "SP<CWLSurfaceResource> surface = nullptr;",
    )
    tex_pass_copy = (
        "CTexPassElement::CTexPassElement(const SRenderData& data) : m_data(data) {",
        "CTexPassElement::CTexPassElement(CTexPassElement::SRenderData&& data) : m_data(std::move(data)) {",
    )
    gl_element_renderer_055 = (
        "void CGLElementRenderer::draw(WP<CTexPassElement> element, const CRegion& damage) {",
        "const auto m_data = element->m_data;",
        "g_pHyprOpenGL->renderTexture(",
        ".allowCustomUV = m_data.allowCustomUV,",
        ".primarySurfaceUVTopLeft = g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft,",
        ".primarySurfaceUVBottomRight = g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight,",
    )
    gl_element_renderer_056 = (
        "void CGLElementRenderer::draw(WP<CTexPassElement> element, const CRegion& damage) {",
        "const auto& m_data = element->m_data;",
        "g_pHyprOpenGL->renderTexture(",
        ".allowCustomUV = m_data.allowCustomUV,",
        ".primarySurfaceUVTopLeft = g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft,",
        ".primarySurfaceUVBottomRight = g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight,",
    )
    surface_pass = (
        "CSurfacePassElement::CSurfacePassElement(const CSurfacePassElement::SRenderData& data_) : m_data(data_) {",
        "bool CSurfacePassElement::needsLiveBlur() { auto PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface); const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier * PSURFACE->m_overallOpacity : 1.F); const bool BLUR = m_data.blur && (!m_data.texture || !m_data.texture->m_opaque || ALPHA < 1.F); if (!m_data.pLS && !m_data.pWindow) return BLUR;",
        "bool CSurfacePassElement::needsPrecomputeBlur() { auto PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface); const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier * PSURFACE->m_overallOpacity : 1.F); const bool BLUR = m_data.blur && (!m_data.texture || !m_data.texture->m_opaque || ALPHA < 1.F); if (!m_data.pLS && !m_data.pWindow) return BLUR;",
    )
    opengl_common = (
        "if (data.discardActive) {",
        "shader->setUniformInt(SHADER_DISCARD_OPAQUE, !!(data.discardMode & DISCARD_OPAQUE));",
        "if (g_pHyprRenderer->m_renderData.useNearestNeighbor) {",
        "tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);",
        "tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);",
        "} else {",
        "tex->setTexParameter(GL_TEXTURE_MAG_FILTER, tex->magFilter);",
        "tex->setTexParameter(GL_TEXTURE_MIN_FILTER, tex->minFilter);",
    )
    opengl_custom_uv_055 = (
        "if (data.allowCustomUV && data.primarySurfaceUVTopLeft != Vector2D(-1, -1)) {",
        "const float u0 = data.primarySurfaceUVTopLeft.x;",
        "const float v0 = data.primarySurfaceUVTopLeft.y;",
        "const float u1 = data.primarySurfaceUVBottomRight.x;",
        "const float v1 = data.primarySurfaceUVBottomRight.y;",
        "verts[0].u = u0;",
        "verts[0].v = v0;",
        "verts[1].u = u0;",
        "verts[1].v = v1;",
        "verts[2].u = u1;",
        "verts[2].v = v0;",
        "verts[3].u = u1;",
        "verts[3].v = v1;",
    )
    opengl_custom_uv_056 = (
        "const bool CUSTOMUV = data.allowCustomUV && data.primarySurfaceUVTopLeft != Vector2D(-1, -1);",
        "if (CUSTOMUV || shader->usesCustomUV()) {",
        "if (CUSTOMUV) {",
        "const float u0 = data.primarySurfaceUVTopLeft.x;",
        "const float v0 = data.primarySurfaceUVTopLeft.y;",
        "const float u1 = data.primarySurfaceUVBottomRight.x;",
        "const float v1 = data.primarySurfaceUVBottomRight.y;",
        "verts[0].u = u0;",
        "verts[0].v = v0;",
        "verts[1].u = u0;",
        "verts[1].v = v1;",
        "verts[2].u = u1;",
        "verts[2].v = v0;",
        "verts[3].u = u1;",
        "verts[3].v = v1;",
        "shader->setUsesCustomUV(CUSTOMUV);",
    )
    opengl_blur = (
        "void CHyprOpenGLImpl::renderTextureWithBlurInternal(",
        "const auto NEEDS_STENCIL = data.discardMode != 0 && (!data.blockBlurOptimization || (data.discardMode & DISCARD_ALPHA));",
        "if (NEEDS_STENCIL) {",
        "glStencilFunc(GL_ALWAYS, 1, 0xFF);",
        ".discardActive = true,",
        ".discardMode = data.discardMode,",
        ".discardOpacity = data.discardOpacity,",
        "}); // discard opaque and alpha < discardOpacity",
        "glStencilFunc(GL_EQUAL, 1, 0xFF);",
        "renderTextureInternal(data.blurredBG, box,",
    )
    opengl_work_buffer_description = (
        "WP<CShader> CHyprOpenGLImpl::renderToFBInternal(",
        "const auto WORK_BUFFER_IMAGE_DESCRIPTION = g_pHyprRenderer->m_renderData.pMonitor->workBufferImageDescription();",
        "const auto SOURCE_IMAGE_DESCRIPTION = [&] {",
        "if (tex->m_imageDescription)",
        "return tex->m_imageDescription;",
        "if (surface.valid() && surface->m_colorManagement.valid())",
        "return CImageDescription::from(surface->m_colorManagement->imageDescription());",
        "if (data.cmBackToSRGB)",
        "return tex->m_imageDescription ? tex->m_imageDescription : WORK_BUFFER_IMAGE_DESCRIPTION;",
        "if (data.finalMonitorCM) // NOLINTNEXTLINE",
        "return WORK_BUFFER_IMAGE_DESCRIPTION;",
        "return getDefaultImageDescription();",
        "const auto TARGET_IMAGE_DESCRIPTION = [&] {",
        "if (g_pHyprRenderer->m_renderData.currentFB->imageDescription())",
        "return g_pHyprRenderer->m_renderData.currentFB->imageDescription();",
        "if (data.cmBackToSRGB)",
        "return getDefaultImageDescription();",
        "if (data.finalMonitorCM)",
        "return g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription;",
        "return WORK_BUFFER_IMAGE_DESCRIPTION;",
        "const bool skipCM = !*PENABLECM || !m_cmSupported /* CM unsupported or disabled */",
        "|| g_pHyprRenderer->m_renderData.pMonitor->doesNoShaderCM() /* no shader needed */",
        "|| !SOURCE_IMAGE_DESCRIPTION->needsCM(TARGET_IMAGE_DESCRIPTION) /* Source and target have matching image descriptions */",
    )
    surface_shader = (
        "uniform bool discardOpaque;",
        "#if USE_DISCARD && !USE_BLUR if (discardOpaque && pixColor.a * alpha == 1.0) discard;",
        "#if USE_BLUR",
        "pixColor = mix(pixColor, vec4(mix(texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb, pixColor.rgb, pixColor.a), 1.0), discardAlpha && (pixColor.a <= discardAlphaValue) ? 0.0 : 1.0);",
    )
    monitor_scale = (
        'Log::logger->log(Log::ERR, "Invalid scale passed to monitor, {} found suggestion {}", m_scale, searchScale);',
        'static auto PDISABLENOTIFICATION = CConfigValue<Config::INTEGER>("misc:disable_scale_notification");',
        "if (!*PDISABLENOTIFICATION) {",
        "Notification::overlay()->addNotification(",
        "m_scale = searchScale;",
    )
    monitor_read_format = (
        "uint32_t CMonitor::getPreferredReadFormat() {",
        'static const auto PFORCE8BIT = CConfigValue<Config::INTEGER>("misc:screencopy_force_8b");',
        "auto monFmt = m_output->state->state().drmFormat;",
        "if (*PFORCE8BIT)",
        "if (monFmt == DRM_FORMAT_BGRA1010102 || monFmt == DRM_FORMAT_ARGB2101010 || monFmt == DRM_FORMAT_XRGB2101010 || monFmt == DRM_FORMAT_BGRX1010102 || monFmt == DRM_FORMAT_XBGR2101010)",
        "monFmt = DRM_FORMAT_XRGB8888;",
        "return monFmt;",
    )
    monitor_choose_tf = (
        "static NColorManagement::eTransferFunction chooseTF(NTransferFunction::eTF tf) {",
        "const auto sdrEOTF = NTransferFunction::fromConfig();",
        "case NTransferFunction::TF_GAMMA22:",
        "case NTransferFunction::TF_FORCED_GAMMA22: return NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22;",
        "case NTransferFunction::TF_DEFAULT:",
        "case NTransferFunction::TF_SRGB: return NColorManagement::CM_TRANSFER_FUNCTION_SRGB;",
        "case NTransferFunction::TF_AUTO: // use global setting",
        "case NTransferFunction::TF_AUTO: return NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22;",
        "default: return chooseTF(sdrEOTF);",
    )
    monitor_use_fp16 = (
        "bool CMonitor::useFP16() {",
        'static const auto PFP16 = CConfigValue<Hyprlang::INT>("render:use_fp16");',
        "auto isSRGB = [this] {",
        "if (m_imageDescription->value().transferFunction != CM_TRANSFER_FUNCTION_SRGB && m_imageDescription->value().transferFunction != CM_TRANSFER_FUNCTION_GAMMA22)",
        "return false;",
        "if (m_imageDescription->value().primariesNamed != CM_PRIMARIES_SRGB)",
        "return false;",
        "return true;",
        "// Auto: use FP16 if the monitor is not sRGB",
        "bool shouldUse = *PFP16 == 1 || (*PFP16 == 2 && !isSRGB());",
        "static bool usedBefore = shouldUse;",
        "if (usedBefore != shouldUse) {",
        "usedBefore = shouldUse;",
        "m_blurFBDirty = true;",
        "return shouldUse;",
    )
    monitor_work_buffer_prefix = (
        "PImageDescription CMonitor::workBufferImageDescription() {",
        'static const auto PFP16TF = CConfigValue<Hyprlang::INT>("render:fp16_sdr_tf");',
        "if (!useFP16() && !m_imageDescription->value().icc.present)",
        "return m_imageDescription;",
        "const auto& value = m_imageDescription->value();",
        "const bool isHDRLikeTF = value.transferFunction == CM_TRANSFER_FUNCTION_ST2084_PQ || value.transferFunction == CM_TRANSFER_FUNCTION_HLG || value.transferFunction == CM_TRANSFER_FUNCTION_EXT_LINEAR;",
        "const auto& cached = m_cachedInternalDescription->value();",
    )
    monitor_work_buffer_suffix = (
        "if (cached.transferFunction != LINEAR_IMAGE_DESCRIPTION->value().transferFunction || cached.luminances != value.luminances)",
        "m_cachedInternalDescription = LINEAR_IMAGE_DESCRIPTION->with(value.luminances);",
        "return m_cachedInternalDescription;",
        "// SDR",
        "if (cached.transferFunction != chooseTF(m_sdrEotf))",
        "m_cachedInternalDescription = CImageDescription::from(SImageDescription{",
        ".transferFunction = chooseTF(m_sdrEotf),",
        ".primariesNameSet = true,",
        "// render:keep_unmodified_copy and other conditions that trigger MRT for screen sharing expect a work buffer with sRGB primaries",
        ".primariesNamed = NColorManagement::CM_PRIMARIES_SRGB,",
        ".primaries = NColorPrimaries::BT709,",
        "return m_cachedInternalDescription;",
        "WP<CMonitorResources> CMonitor::resources() {",
        "const auto DRM_FORMAT = useFP16() ? DRM_FORMAT_ABGR16161616F : m_output->state->state().drmFormat;",
        "const auto DESC = workBufferImageDescription();",
        "if (!m_resources || m_resources->m_drmFormat != DRM_FORMAT || m_resources->m_size != m_pixelSize)",
        "m_resources = makeUnique<CMonitorResources>(m_self, DRM_FORMAT, m_pixelSize, DESC);",
        "if (m_resources->m_imageDescription != DESC)",
        "m_resources->setImageDescription(DESC);",
        "return m_resources;",
    )
    color_management_linear_055 = (
        "static const auto LINEAR_IMAGE_DESCRIPTION = CImageDescription::from(SImageDescription{",
        ".transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_EXT_LINEAR,",
        ".primariesNameSet = true,",
        ".primariesNamed = NColorManagement::CM_PRIMARIES_SRGB,",
        ".primaries = NColorPrimaries::BT709,",
        ".luminances = {.min = 0, .max = 10000, .reference = 80},",
    )
    color_management_linear_056 = (
        "inline const auto LINEAR_IMAGE_DESCRIPTION = CImageDescription::from(SImageDescription{",
        ".transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_EXT_LINEAR,",
        ".primariesNameSet = true,",
        ".primariesNamed = NColorManagement::CM_PRIMARIES_SRGB,",
        ".primaries = NColorPrimaries::BT709,",
        ".luminances = {.min = 0, .max = 10000, .reference = 80},",
    )
    framebuffer_image_description = (
        "NColorManagement::PImageDescription IFramebuffer::imageDescription() {",
        "return m_tex ? m_tex->m_imageDescription : m_imageDescription;",
        "void IFramebuffer::setImageDescription(NColorManagement::PImageDescription desc) {",
        "m_imageDescription = desc;",
        "if (m_tex)",
        "m_tex->m_imageDescription = desc;",
        "else",
        'Log::logger->log(Log::TRACE, "CM: FIXME no framebuffer texture");',
    )
    monitor_resources_common = (
        "CMonitorResources::CMonitorResources(WP<CMonitor> monitor, DRMFormat format, Vector2D size, NColorManagement::PImageDescription imageDescription) :",
        "m_imageDescription(imageDescription) {",
        "initFB(m_blurFB);",
        "void CMonitorResources::initFB(SP<Render::IFramebuffer> fb) {",
        "fb->addStencil(m_stencilTex);",
        "fb->alloc(m_size.x, m_size.y, m_drmFormat);",
        "fb->setImageDescription(m_imageDescription);",
        "void CMonitorResources::setImageDescription(NColorManagement::PImageDescription imageDescription) {",
        "if (m_imageDescription == imageDescription)",
        "return;",
        "m_imageDescription = imageDescription;",
        "m_blurFB->setImageDescription(imageDescription);",
        "for (const auto& res : m_workBuffers)",
        "res.buffer->setImageDescription(imageDescription);",
        "if (m_monitorMirrorFB)",
        "m_monitorMirrorFB->setImageDescription(getMirrorTexImageDescription());",
        "if (m_mirrorTex)",
        "m_mirrorTex->m_imageDescription = getMirrorTexImageDescription();",
    )
    monitor_resources_reuse = (
        "SP<Render::IFramebuffer> CMonitorResources::getUnusedWorkBuffer() {",
        "std::erase_if(m_workBuffers, [](const auto& res) { return res.lastUsed.getSeconds() >= MAX_UNUSED_SECONDS; });",
        "auto found = std::ranges::find_if(m_workBuffers, [](const auto& res) { return res.buffer.strongRef() < 2; });",
        "if (found != m_workBuffers.end()) {",
        "found->lastUsed.reset();",
        "return found->buffer;",
        "if (m_workBuffers.size() >= MAX_WORK_BUFFERS)",
        "return nullptr;",
        'auto& res = m_workBuffers.emplace_back(g_pHyprRenderer->createFB(std::format("Monitor {} workbuffer", m_monitor->m_name)));',
        "initFB(res.buffer);",
        "res.lastUsed.reset();",
        "return res.buffer;",
    )
    monitor_damage_055 = (
        "void CMonitor::addDamage(const CBox& box) {",
        "if (m_cursorZoom->value() != 1.f && g_pCompositor->getMonitorFromCursor() == m_self) {",
        "m_damage.damageEntire();",
        "g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);",
        "return;",
        "if (m_damage.damage(box))",
        "g_pCompositor->scheduleFrameForMonitor(m_self.lock(), Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);",
    )
    monitor_damage_056 = (
        "void CMonitor::addDamage(const CBox& box) {",
        "if (m_cursorZoom->value() != 1.f && State::monitorState()->query().vec(Pointer::mgr()->position()).run() == m_self) {",
        "m_damage.damageEntire();",
        "scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);",
        "return;",
        "if (m_damage.damage(box))",
        "scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);",
    )
    monitor_direct_scanout_055 = (
        "uint32_t CMonitor::isSolitaryBlocked(bool full) {",
        "const auto PWORKSPACE = m_activeWorkspace;",
        "if (!PWORKSPACE) { reasons |= SC_WORKSPACE; return reasons; }",
        "if (!inFullscreenMode()) { reasons |= SC_WINDOWED; if (!full) return reasons; }",
        "if (m_activeSpecialWorkspace) { reasons |= SC_SPECIAL; if (!full) return reasons; }",
        "if (Notification::overlay()->hasAny()) { reasons |= SC_NOTIFICATION; if (!full) return reasons; }",
        "if (ErrorOverlay::overlay()->active() && Desktop::focusState()->monitor() == m_self) { reasons |= SC_ERRORBAR; if (!full) return reasons; }",
        "if (g_pSessionLockManager->isSessionLocked()) { reasons |= SC_LOCK; if (!full) return reasons; }",
        "if (PROTO::data->dndActive()) { reasons |= SC_DND; if (!full) return reasons; }",
        "if (PWORKSPACE->m_alpha->value() != 1.f) { reasons |= SC_ALPHA; if (!full) return reasons; }",
        "if (PWORKSPACE->m_renderOffset->value() != Vector2D{}) { reasons |= SC_OFFSET; if (!full) return reasons; }",
        "const auto PCANDIDATE = getFullscreenWindow();",
        "if (!PCANDIDATE) { reasons |= SC_CANDIDATE; return reasons; }",
        "if (!PCANDIDATE->opaque()) { reasons |= SC_OPAQUE; if (!full) return reasons; }",
        "if (PCANDIDATE->m_realSize->value() != m_size || PCANDIDATE->m_realPosition->value() != m_position || PCANDIDATE->m_realPosition->isBeingAnimated() || PCANDIDATE->m_realSize->isBeingAnimated()) { reasons |= SC_TRANSFORM; if (!full) return reasons; }",
        "if (!m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY].empty()) { reasons |= SC_OVERLAYS; if (!full) return reasons; }",
        "for (auto const& topls : m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) { if (topls->m_alpha->value() != 0.f) { reasons |= SC_OVERLAYS; if (!full) return reasons; } }",
        "for (auto const& w : g_pCompositor->m_windows) {",
        "if (w == PCANDIDATE || (!w->m_isMapped && !w->m_fadingOut) || !w->visible()) continue;",
        "if (w->workspaceID() == PCANDIDATE->workspaceID() && w->m_isFloating && w->isAllowedOverFullscreen() && w->visibleOnMonitor(m_self.lock())) { reasons |= SC_FLOAT; if (!full) return reasons; }",
        "for (auto const& ws : g_pCompositor->getWorkspaces()) {",
        "if (ws->m_alpha->value() <= 0.F || !ws->m_isSpecialWorkspace || ws->m_monitor != m_self) continue;",
        "reasons |= SC_WORKSPACES;",
        "if (!PCANDIDATE->getSolitaryResource()) reasons |= SC_SURFACES;",
        "void CMonitor::recheckSolitary() {",
        "m_solitaryClient.reset(); // reset it, if we find one it will be set.",
        "if (isSolitaryBlocked()) return;",
        "m_solitaryClient = getFullscreenWindow();",
        "uint16_t CMonitor::isDSBlocked(bool full) {",
        'static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");',
        'static auto PNONSHADER = CConfigValue<Config::INTEGER>("render:non_shader_cm");',
        "if (!canAttemptDirectScanoutFast()) { reasons |= DS_BLOCK_CANDIDATE; if (!full) return reasons; }",
        "if (*PDIRECTSCANOUT == 0) { reasons |= DS_BLOCK_USER; if (!full) return reasons; } if (*PDIRECTSCANOUT == 2) { const auto FSWINDOW = getFullscreenWindow(); if (!PWORKSPACE || !inFullscreenMode() || !FSWINDOW) { reasons |= DS_BLOCK_WINDOWED; if (!full) return reasons; } else if (FSWINDOW->getContentType() != CONTENT_TYPE_GAME) { reasons |= DS_BLOCK_CONTENT; if (!full) return reasons; } } if (!m_mirrors.empty() || isMirror()) {",
        "reasons |= DS_BLOCK_MIRROR;",
        "if (g_pHyprRenderer->m_directScanoutBlocked) { reasons |= DS_BLOCK_RECORD; if (!full) return reasons; }",
        "if (g_pPointerManager->softwareLockedFor(m_self.lock())) { reasons |= DS_BLOCK_SW; if (!full) return reasons; }",
        "const auto PCANDIDATE = m_solitaryClient.lock();",
        "if (!PCANDIDATE) { reasons |= DS_BLOCK_CANDIDATE; return reasons; }",
        "const auto PSURFACE = PCANDIDATE->getSolitaryResource();",
        "if (!PSURFACE || !PSURFACE->m_current.texture || !PSURFACE->m_current.buffer) { reasons |= DS_BLOCK_SURFACE; return reasons; }",
        "if (PSURFACE->m_current.bufferSize != m_pixelSize || PSURFACE->m_current.transform != m_transform) { reasons |= DS_BLOCK_TRANSFORM; if (!full) return reasons; }",
        "const auto params = PSURFACE->m_current.buffer->dmabuf();",
        "if (!params.success || !PSURFACE->m_current.texture->isDMA() /* dmabuf */) { reasons |= DS_BLOCK_DMA; if (!full) return reasons; }",
        "const bool surfaceIsHDR = PSURFACE->m_colorManagement.valid() && PSURFACE->m_colorManagement->isHDR();",
        "const bool surfaceIsScRGB = surfaceIsHDR && PSURFACE->m_colorManagement->isWindowsScRGB();",
        "if (surfaceIsScRGB)",
        "reasons |= DS_BLOCK_CM; // block scRGB",
        "else if (*PNONSHADER != CM_NS_IGNORE) {",
        "if (!surfaceIsHDR && needsCM() && !canNoShaderCM(true)) reasons |= DS_BLOCK_CM; // block SDR that needs CM while non-shader CM isn't available",
        "else if (surfaceIsHDR && !inHDR()) reasons |= DS_BLOCK_CM; // block HDR while monitor isn't in HDR mode",
        "bool CMonitor::attemptDirectScanout() {",
        "const auto blockedReason = isDSBlocked();",
        "if (blockedReason) return false;",
        "const auto PCANDIDATE = m_solitaryClient.lock();",
        "const auto PSURFACE = PCANDIDATE->getSolitaryResource();",
        "auto PBUFFER = PSURFACE->m_current.buffer.m_buffer;",
        "const bool NEEDS_TEST = !m_lastScanout || m_drmFormat != params.format; // do not retest while it's active",
        "m_output->state->setBuffer(PBUFFER);",
        "if (NEEDS_TEST && !m_state.test()) {",
        'Log::logger->log(Log::TRACE, "attemptDirectScanout: failed basic test");',
        "m_drmFormat = PREV_FORMAT; } return false; }",
        "PSURFACE->presentFeedback(Time::steadyNow(), m_self.lock());",
        "bool ok = m_output->commit();",
        'if (!ok) { Log::logger->log(Log::TRACE, "attemptDirectScanout: failed to scanout surface");',
        "m_lastScanout.reset(); return false; }",
        "if (m_lastScanout.expired()) { m_lastScanout = PCANDIDATE;",
        'Log::logger->log(Log::DEBUG, "Entered a direct scanout to {:x}: \\\"{}\\\"", rc<uintptr_t>(PCANDIDATE.get()), PCANDIDATE->m_title);',
        "void CMonitor::handleDSleave() {",
        "m_lastScanout.reset();",
        "m_previousFSWindow.reset(); // recalc fs settings",
        "m_directScanoutIsActive = false;",
        "m_drmFormat = m_prevDrmFormat;",
        "m_blurFBDirty = true;",
        "bool CMonitor::canAttemptDirectScanoutFast() const {",
        "return !m_solitaryClient.expired() || !m_lastScanout.expired() || m_directScanoutIsActive;",
    )
    monitor_direct_scanout_056 = (
        "uint32_t CMonitor::isSolitaryBlocked(bool full) {",
        "const auto PWORKSPACE = m_activeWorkspace;",
        "if (!PWORKSPACE) { reasons |= SC_WORKSPACE; return reasons; }",
        "if (Fullscreen::controller()->getFullscreenModes(m_self.lock()).internal != Fullscreen::FSMODE_FULLSCREEN) { reasons |= SC_WINDOWED; if (!full) return reasons; }",
        "if (m_activeSpecialWorkspace) { reasons |= SC_SPECIAL; if (!full) return reasons; }",
        "if (Notification::overlay()->hasAny()) { reasons |= SC_NOTIFICATION; if (!full) return reasons; }",
        "if (ErrorOverlay::overlay()->active() && Desktop::focusState()->monitor() == m_self) { reasons |= SC_ERRORBAR; if (!full) return reasons; }",
        "if (g_pSessionLockManager->isSessionLocked()) { reasons |= SC_LOCK; if (!full) return reasons; }",
        "if (PROTO::data->dndActive()) { reasons |= SC_DND; if (!full) return reasons; }",
        "if (PWORKSPACE->m_alpha->value() != 1.f) { reasons |= SC_ALPHA; if (!full) return reasons; }",
        "if (PWORKSPACE->m_renderOffset->value() != Vector2D{}) { reasons |= SC_OFFSET; if (!full) return reasons; }",
        "const auto PCANDIDATE = Fullscreen::controller()->getFullscreenWindow(m_self.lock());",
        "if (!PCANDIDATE) { reasons |= SC_CANDIDATE; return reasons; }",
        "if (!PCANDIDATE->opaque()) { reasons |= SC_OPAQUE; if (!full) return reasons; }",
        "if (PCANDIDATE->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) != m_size || PCANDIDATE->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) != m_position || PCANDIDATE->positionAnimation()->isBeingAnimated() || PCANDIDATE->sizeAnimation()->isBeingAnimated()) { reasons |= SC_TRANSFORM; if (!full) return reasons; }",
        "if (!m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY].empty()) { reasons |= SC_OVERLAYS; if (!full) return reasons; }",
        "for (auto const& topls : m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) { if (topls->alpha()[LS_ALPHA_FADE]->value() != 0.F) { reasons |= SC_OVERLAYS; if (!full) return reasons; } }",
        "for (auto const& fadeout : Desktop::fadingOutState()->fadeouts()) {",
        "if (!fadeout || fadeout->monitor() != m_self) continue;",
        "reasons |= SC_FADEOUT;",
        "for (auto const& w : Desktop::windowState()->windows()) {",
        "if (w == PCANDIDATE || !w->m_isMapped || !w->visible()) continue;",
        "if (w->workspaceID() == PCANDIDATE->workspaceID() && w->m_isFloating && w->isAllowedOverFullscreen() && w->visibleOnMonitor(m_self.lock())) { reasons |= SC_FLOAT; if (!full) return reasons; }",
        "for (auto const& ws : State::workspaceState()->workspaces()) {",
        "if (ws->m_alpha->value() <= 0.F || !ws->m_isSpecialWorkspace || ws->m_monitor != m_self) continue;",
        "reasons |= SC_WORKSPACES;",
        "if (!PCANDIDATE->getSolitaryResource()) reasons |= SC_SURFACES;",
        "void CMonitor::recheckSolitary() {",
        "m_solitaryClient.reset(); // reset it, if we find one it will be set.",
        "if (isSolitaryBlocked()) return;",
        "m_solitaryClient = Fullscreen::controller()->getFullscreenWindow(m_self.lock());",
        "uint16_t CMonitor::isDSBlocked(bool full) {",
        'static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");',
        'static auto PNONSHADER = CConfigValue<Config::INTEGER>("render:non_shader_cm");',
        "if (!canAttemptDirectScanoutFast()) { reasons |= DS_BLOCK_CANDIDATE; if (!full) return reasons; }",
        "if (*PDIRECTSCANOUT == 0) { reasons |= DS_BLOCK_USER; if (!full) return reasons; } if (*PDIRECTSCANOUT == 2) { const auto FSWINDOW = Fullscreen::controller()->getFullscreenWindow(m_self.lock()); if (!PWORKSPACE || !FSWINDOW || Fullscreen::controller()->getFullscreenModes(FSWINDOW).internal != Fullscreen::FSMODE_FULLSCREEN) { reasons |= DS_BLOCK_WINDOWED; if (!full) return reasons; } else if (FSWINDOW->getContentType() != CONTENT_TYPE_GAME) { reasons |= DS_BLOCK_CONTENT; if (!full) return reasons; } } if (!m_mirrors.empty() || isMirror()) {",
        "reasons |= DS_BLOCK_MIRROR;",
        "if (g_pHyprRenderer->m_directScanoutBlocked) { reasons |= DS_BLOCK_RECORD; if (!full) return reasons; }",
        "if (Pointer::mgr()->softwareLockedFor(m_self.lock())) { reasons |= DS_BLOCK_SW; if (!full) return reasons; }",
        "const auto PCANDIDATE = m_solitaryClient.lock();",
        "if (!PCANDIDATE) { reasons |= DS_BLOCK_CANDIDATE; return reasons; }",
        "const auto PSURFACE = PCANDIDATE->getSolitaryResource();",
        "if (!PSURFACE || !PSURFACE->m_current.texture || !PSURFACE->m_current.buffer) { reasons |= DS_BLOCK_SURFACE; return reasons; }",
        "if (PSURFACE->m_current.bufferSize != m_pixelSize || PSURFACE->m_current.transform != m_transform) { reasons |= DS_BLOCK_TRANSFORM; if (!full) return reasons; }",
        "const auto params = PSURFACE->m_current.buffer->dmabuf();",
        "if (!params.success || !PSURFACE->m_current.texture->isDMA() /* dmabuf */) { reasons |= DS_BLOCK_DMA; if (!full) return reasons; }",
        "const bool surfaceIsHDR = PSURFACE->m_colorManagement.valid() && PSURFACE->m_colorManagement->isHDR();",
        "const bool surfaceIsScRGB = surfaceIsHDR && PSURFACE->m_colorManagement->isWindowsScRGB();",
        "if (surfaceIsScRGB)",
        "reasons |= DS_BLOCK_CM; // block scRGB",
        "else if (*PNONSHADER != CM_NS_IGNORE) {",
        "if (!surfaceIsHDR && needsCM() && !canNoShaderCM(true)) reasons |= DS_BLOCK_CM; // block SDR that needs CM while non-shader CM isn't available",
        "else if (surfaceIsHDR && !inHDR()) reasons |= DS_BLOCK_CM; // block HDR while monitor isn't in HDR mode",
        "bool CMonitor::attemptDirectScanout() {",
        "const auto blockedReason = isDSBlocked();",
        "if (blockedReason) return false;",
        "const auto PCANDIDATE = m_solitaryClient.lock();",
        "const auto PSURFACE = PCANDIDATE->getSolitaryResource();",
        "auto PBUFFER = PSURFACE->m_current.buffer.m_buffer;",
        "const bool NEEDS_TEST = !m_lastScanout || m_drmFormat != params.format; // do not retest while it's active",
        "m_output->state->setBuffer(PBUFFER);",
        "if (NEEDS_TEST && !m_state.test()) {",
        'Log::logger->log(Log::TRACE, "attemptDirectScanout: failed basic test");',
        "return false; }",
        "PSURFACE->presentFeedback(Time::steadyNow(), m_self.lock());",
        "bool ok = m_output->commit();",
        'if (!ok) { Log::logger->log(Log::TRACE, "attemptDirectScanout: failed to scanout surface");',
        "m_lastScanout.reset(); return false; }",
        "scanoutCommitted = true;",
        "if (m_lastScanout.expired()) { m_lastScanout = PCANDIDATE;",
        'Log::logger->log(Log::DEBUG, "Entered a direct scanout to {:x}: \\\"{}\\\"", rc<uintptr_t>(PCANDIDATE.get()), PCANDIDATE->m_title);',
        "void CMonitor::handleDSleave() {",
        "m_lastScanout.reset();",
        "m_previousFSWindow.reset(); // recalc fs settings",
        "m_directScanoutIsActive = false;",
        "m_drmFormat = m_prevDrmFormat;",
        "m_blurFBDirty = true;",
        "bool CMonitor::canAttemptDirectScanoutFast() const {",
        "return !m_solitaryClient.expired() || !m_lastScanout.expired() || m_directScanoutIsActive;",
    )
    screenshare = (
        "void CScreenshareSession::init() {",
        "m_listeners.monitorModeChanged = monitor()->m_events.modeChanged.listen(",
        "calculateConstraints();",
        "m_events.constraintsChanged.emit();",
        "calculateConstraints();",
        "void CScreenshareSession::calculateConstraints() {",
        "m_formats.clear();",
        "m_formats.push_back(NFormatUtils::alphaFormat(PMONITOR->getPreferredReadFormat()));",
        "m_formats.push_back(PMONITOR->getPreferredReadFormat());",
        "for (auto& format : m_formats) {",
        "if (format == DRM_FORMAT_XRGB2101010 || format == DRM_FORMAT_ARGB2101010)",
        "format = DRM_FORMAT_XBGR2101010;",
        "UP<CScreenshareFrame> CScreenshareSession::nextFrame(bool overlayCursor) {",
        "Screenshare::mgr()->m_pendingFrames.emplace_back(frame);",
        "g_pHyprRenderer->m_directScanoutBlocked = true;",
    )
    return {
        "0.55.0": {
            REGISTRY_PATH: (
                *registry_prefix,
                *registry_tail,
                *registry_xwayland,
                *registry_direct_scanout,
                *registry_expand_undersized_textures,
                *registry_xp_mode,
                *registry_fp16,
            ),
            Path("src/config/lua/ConfigManager.cpp"): reload_requirements_055,
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): (
                "void CPropRefresher::scheduleRefresh(PropRefreshBits prop) {",
                "m_propsTripped |= prop;",
                "if (!m_scheduled) {",
                "if (m_propsTripped & REFRESH_BLUR_FB) { for (auto const& m : g_pCompositor->m_monitors) { if (m) m->m_blurFBDirty = true; } }",
                "if (m_propsTripped & REFRESH_LAYOUTS) {",
                "for (auto const& m : g_pCompositor->m_monitors) { g_layoutManager->recalculateMonitor(m); g_pHyprRenderer->damageMonitor(m); }",
                "m_propsTripped = 0;",
            ),
            Path("src/managers/SessionLockManager.cpp"): (
                *session_lock_common,
                "if (PROTO::sessionLock->isLocked() && !*PALLOWRELOCK) {",
                *session_lock_tail,
            ),
            Path("src/render/Renderer.cpp"): (
                *renderer_prefix,
                "w->wlSurface()->resource()->frame(Time::steadyNow());",
                "FEEDBACK->discarded();",
                *renderer_after_timer,
                *renderer_xwayland,
                *renderer_xwayland_tail_055,
                "void IHyprRenderer::renderSessionLockSurface(",
                "CSurfacePassElement::SRenderData renderdata = {pMonitor, time, pMonitor->m_position, pMonitor->m_position};",
                "renderdata.blur = false;",
                "m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));",
                *renderer_workspace_xray,
                renderer_xp_no_workspace_055,
                renderer_xp_active_workspace_055,
                *renderer_background,
                *renderer_lockscreen,
                *renderer_resource_bridge_055,
                *renderer_direct_scanout_055,
                *renderer_frame_and_damage,
                *renderer_unfocused_tail,
                *renderer_work_buffer_description,
            ),
            Path("src/render/ElementRenderer.cpp"): element_renderer,
            Path("src/render/pass/TexPassElement.hpp"): tex_pass_declaration,
            Path("src/render/pass/TexPassElement.cpp"): tex_pass_copy,
            Path("src/render/gl/GLElementRenderer.cpp"): gl_element_renderer_055,
            Path("src/render/OpenGL.cpp"): (
                "void CHyprOpenGLImpl::renderTexture(",
                "if (data.blur) renderTextureWithBlurInternal(tex, box, data);",
                *opengl_work_buffer_description,
                *opengl_common,
                *opengl_custom_uv_055,
                *opengl_blur,
            ),
            Path("src/render/pass/SurfacePassElement.hpp"): (
                "bool blur = false;",
                "uint32_t discardMode = DISCARD_OPAQUE;",
                "float discardOpacity = 0.f;",
                "bool useNearestNeighbor = false;",
            ),
            Path("src/render/pass/SurfacePassElement.cpp"): surface_pass,
            Path("src/render/shaders/glsl/surface.frag"): surface_shader,
            Path("src/helpers/Monitor.cpp"): (
                *monitor_choose_tf,
                *monitor_scale,
                *monitor_damage_055,
                *monitor_direct_scanout_055,
                *monitor_read_format,
                *monitor_use_fp16,
                *monitor_work_buffer_prefix,
                "if (isHDRLikeTF || value.windowsScRGB || *PFP16TF != 0) {",
                *monitor_work_buffer_suffix,
            ),
            Path("src/managers/screenshare/ScreenshareSession.cpp"): screenshare,
            Path("src/helpers/cm/ColorManagement.hpp"): color_management_linear_055,
            Path("src/render/Framebuffer.cpp"): framebuffer_image_description,
            Path("src/helpers/MonitorResources.cpp"): (
                *monitor_resources_common,
                *monitor_resources_reuse,
            ),
        },
        "0.56.0": {
            REGISTRY_PATH: registry_input_capture,
            Path("src/config/lua/ConfigManager.cpp"): reload_requirements_056,
            Path("src/managers/input/InputManager.cpp"): input_capture_modifiers_0560,
            Path("src/protocols/InputCapture.cpp"): input_capture_barriers,
        },
        "0.56.1": {
            REGISTRY_PATH: (
                *registry_prefix,
                'MS<Bool>("misc:session_lock_blur", "Enable blur for lockscreen. You probably want to enable `session_lock_xray`.", false),',
                *registry_tail,
                *registry_xwayland,
                *registry_direct_scanout,
                *registry_expand_undersized_textures,
                *registry_xp_mode,
                *registry_fp16,
                *registry_input_capture,
            ),
            Path("src/config/lua/ConfigManager.cpp"): reload_requirements_056,
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): (
                "void CPropRefresher::scheduleRefresh(PropRefreshBits prop) {",
                "m_propsTripped |= prop;",
                "if (!m_scheduled && g_pEventLoopManager) {",
                "refreshProp(true);",
                "void CPropRefresher::refreshProp(const bool execdAsScheduled) {",
                "if (m_propsTripped & REFRESH_BLUR_FB) { for (auto const& m : State::monitorState()->monitors()) { if (!m) continue; m->m_blurFBDirty = true; m->m_forceFullFrames = 2; m->scheduleFrame(); } }",
                "if (m_propsTripped & REFRESH_WINDOW_STATES) {",
                "g_pHyprRenderer->damageMonitor(m);",
                "m->scheduleFrame();",
                "m_propsTripped = 0;",
            ),
            Path("src/managers/input/InputManager.cpp"): input_capture_modifiers_0561,
            Path("src/protocols/InputCapture.cpp"): input_capture_barriers,
            Path("src/managers/SessionLockManager.cpp"): (
                *session_lock_common,
                "if (PROTO::sessionLock->isLocked() && !*PALLOWRELOCK && g_pCompositor->m_startLockedCommand.empty()) {",
                *session_lock_tail,
            ),
            Path("src/render/Renderer.cpp"): (
                *renderer_prefix,
                "w->wlSurface()->resource()->breadthfirst(",
                "surf->m_stateQueue.unlockFirst(LOCK_REASON_FENCE | LOCK_REASON_FIFO | LOCK_REASON_TIMER);",
                "surf->presentFeedback(Time::steadyNow(), Desktop::focusState()->monitor(), true);",
                *renderer_after_timer,
                *renderer_xwayland,
                *renderer_xwayland_tail_056,
                "void IHyprRenderer::renderSessionLockSurface(",
                'static auto PSESSIONLOCKXRAY = CConfigValue<Config::BOOL>("misc:session_lock_xray");',
                'static auto PSESSIONLOCKBLUR = CConfigValue<Config::BOOL>("misc:session_lock_blur");',
                "CSurfacePassElement::SRenderData renderdata = {pMonitor, time, pMonitor->m_position, pMonitor->m_position};",
                "renderdata.blur = *PSESSIONLOCKBLUR && *PSESSIONLOCKXRAY;",
                "m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));",
                *renderer_workspace_xray,
                renderer_xp_no_workspace_056,
                renderer_xp_active_workspace_056,
                *renderer_background,
                *renderer_lockscreen,
                *renderer_resource_bridge_056,
                *renderer_direct_scanout_056,
                *renderer_frame_and_damage,
                *renderer_unfocused_tail,
                *renderer_work_buffer_description,
            ),
            Path("src/render/ElementRenderer.cpp"): element_renderer,
            Path("src/render/pass/TexPassElement.hpp"): tex_pass_declaration,
            Path("src/render/pass/TexPassElement.cpp"): tex_pass_copy,
            Path("src/render/gl/GLElementRenderer.cpp"): gl_element_renderer_056,
            Path("src/render/OpenGL.cpp"): (
                "void CHyprOpenGLImpl::renderTexture(",
                "if (data.blur && !data.forceBlurBlend) renderTextureWithBlurInternal(tex, box, data);",
                *opengl_work_buffer_description,
                *opengl_common,
                *opengl_custom_uv_056,
                *opengl_blur,
            ),
            Path("src/render/pass/SurfacePassElement.hpp"): (
                "bool blur = false;",
                "uint8_t discardMode = DISCARD_OPAQUE;",
                "float discardOpacity = 0.f;",
                "bool useNearestNeighbor = false;",
            ),
            Path("src/render/pass/SurfacePassElement.cpp"): surface_pass,
            Path("src/render/shaders/glsl/surface.frag"): surface_shader,
            Path("src/output/Monitor.cpp"): (
                *monitor_choose_tf,
                *monitor_scale,
                *monitor_damage_056,
                *monitor_direct_scanout_056,
                *monitor_read_format,
                *monitor_use_fp16,
                *monitor_work_buffer_prefix,
                "if (isHDRLikeTF || *PFP16TF != 0) {",
                *monitor_work_buffer_suffix,
            ),
            Path("src/managers/screenshare/ScreenshareSession.cpp"): screenshare,
            Path("src/helpers/cm/ColorManagement.hpp"): color_management_linear_056,
            Path("src/render/Framebuffer.cpp"): framebuffer_image_description,
            Path("src/output/MonitorResources.cpp"): (
                *monitor_resources_common,
                "invalidateMirrorFB();",
                *monitor_resources_reuse,
            ),
        },
    }


def _assert_advanced_runtime_contract(
    versioned_sources: dict[str, dict[Path, bytes]],
) -> None:
    requirements = _advanced_runtime_contract_requirements()
    if tuple(versioned_sources) != tuple(ADVANCED_RUNTIME_SOURCE_PATHS):
        raise ValueError("advanced runtime source version inventory changed")
    if tuple(requirements) != tuple(ADVANCED_RUNTIME_SOURCE_PATHS):
        raise ValueError("advanced runtime requirement version inventory changed")

    reviewed_fragments = " ".join(
        fragment
        for version_requirements in requirements.values()
        for fragments in version_requirements.values()
        for fragment in fragments
    )
    for option_path in ADVANCED_RUNTIME_OPTION_PATHS:
        if f'"{option_path}"' not in reviewed_fragments:
            raise ValueError(
                f"advanced runtime has no semantic assertion for {option_path}"
            )

    for version, expected_paths in ADVANCED_RUNTIME_SOURCE_PATHS.items():
        sources = versioned_sources.get(version)
        if not isinstance(sources, dict) or tuple(sources) != expected_paths:
            raise ValueError(
                f"Hyprland {version} advanced runtime source inventory/order changed"
            )
        version_requirements = requirements[version]
        if tuple(version_requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} advanced runtime requirement order changed"
            )

        monitor_path = (
            Path("src/helpers/Monitor.cpp")
            if version == "0.55.0"
            else Path("src/output/Monitor.cpp")
        )
        runtime_counts = {path: {} for path in expected_paths}
        runtime_counts[REGISTRY_PATH] = {
            "misc:allow_session_lock_restore": 1,
            "misc:lockdead_screen_delay": 1,
            "misc:disable_scale_notification": 1,
            "misc:render_unfocused_fps": 1,
            "misc:screencopy_force_8b": 1,
            "input-capture:capture_modifiers": 0 if version == "0.55.0" else 1,
            "input-capture:enforce_barriers": 0 if version == "0.55.0" else 1,
            "misc:disable_hyprland_logo": 1,
            "misc:disable_splash_rendering": 1,
            "misc:session_lock_xray": 1,
            "misc:session_lock_blur": 0 if version == "0.55.0" else 1,
            "xwayland:use_nearest_neighbor": 1,
            "render:expand_undersized_textures": 1,
            "render:direct_scanout": 1,
            "render:fp16_sdr_tf": 1,
            "render:xp_mode": 1,
            "render:use_fp16": 1,
        }
        session_lock_path = Path("src/managers/SessionLockManager.cpp")
        if session_lock_path in runtime_counts:
            runtime_counts[session_lock_path] = {
                "misc:allow_session_lock_restore": 1,
                "misc:lockdead_screen_delay": 1,
            }
        renderer_path = Path("src/render/Renderer.cpp")
        if renderer_path in runtime_counts:
            runtime_counts[renderer_path] = {
                "misc:render_unfocused_fps": 2,
                "misc:disable_hyprland_logo": 2,
                "misc:disable_splash_rendering": 1,
                "misc:session_lock_xray": 2 if version == "0.55.0" else 3,
                "misc:session_lock_blur": 0 if version == "0.55.0" else 1,
                "xwayland:use_nearest_neighbor": 1,
                "render:xp_mode": 1,
            }
        element_renderer_path = Path("src/render/ElementRenderer.cpp")
        if element_renderer_path in runtime_counts:
            runtime_counts[element_renderer_path] = {
                "render:expand_undersized_textures": 1,
            }
        if monitor_path in runtime_counts:
            runtime_counts[monitor_path] = {
                "misc:disable_scale_notification": 1,
                "misc:screencopy_force_8b": 1,
                "render:direct_scanout": 1,
                "render:fp16_sdr_tf": 1,
                "render:use_fp16": 1,
            }
        input_manager_path = Path("src/managers/input/InputManager.cpp")
        if input_manager_path in runtime_counts:
            runtime_counts[input_manager_path] = {
                "input-capture:capture_modifiers": 1,
            }
        input_capture_path = Path("src/protocols/InputCapture.cpp")
        if input_capture_path in runtime_counts:
            runtime_counts[input_capture_path] = {
                "input-capture:enforce_barriers": 1,
            }
        for path in expected_paths:
            source = sources[path].decode("utf-8")
            expected_counts = runtime_counts[path]
            for option_path in (*ADVANCED_RUNTIME_OPTION_PATHS, "render:use_fp16"):
                expected_count = expected_counts.get(option_path, 0)
                if source.count(f'"{option_path}"') != expected_count:
                    raise ValueError(
                        f"Hyprland {version} advanced runtime count changed in "
                        f"{path} for {option_path}"
                    )
            _require_ordered_cpp_fragments(
                source,
                version,
                path,
                version_requirements[path],
                "advanced runtime",
            )


def _assert_advanced_render_catalog(catalog: dict[str, Any]) -> None:
    options = catalog.get("options")
    if not isinstance(options, list):
        raise ValueError("advanced render catalog has no option inventory")

    by_path = {
        option.get("path"): option
        for option in options
        if isinstance(option, dict)
        and option.get("path") in ADVANCED_RENDER_OPTION_PATHS
    }
    actual = tuple(
        (
            path,
            by_path.get(path, {}).get("type"),
            by_path.get(path, {}).get("default"),
            by_path.get(path, {}).get("control"),
            by_path.get(path, {}).get("constraints"),
            by_path.get(path, {}).get("risk"),
            by_path.get(path, {}).get("applyMode"),
            by_path.get(path, {}).get("since"),
        )
        for path in ADVANCED_RENDER_OPTION_PATHS
    )
    expected = tuple(
        (
            path,
            (
                "enum"
                if path in ("render:direct_scanout", "render:fp16_sdr_tf")
                else "boolean"
            ),
            (
                0
                if path in ("render:direct_scanout", "render:fp16_sdr_tf")
                else path
                in (
                    "xwayland:use_nearest_neighbor",
                    "render:expand_undersized_textures",
                )
            ),
            (
                "select"
                if path in ("render:direct_scanout", "render:fp16_sdr_tf")
                else "toggle"
            ),
            (
                {
                    "min": 0,
                    "max": 2,
                    "choices": [
                        {"label": "disable", "value": 0},
                        {"label": "enable", "value": 1},
                        {"label": "auto", "value": 2},
                    ],
                }
                if path == "render:direct_scanout"
                else (
                    {
                        "min": 0,
                        "max": 1,
                        "choices": [
                            {"label": "monitor", "value": 0},
                            {"label": "linear", "value": 1},
                        ],
                    }
                    if path == "render:fp16_sdr_tf"
                    else {}
                )
            ),
            "safe" if path.startswith("misc:") else "caution",
            "reload",
            since,
        )
        for path, since in ADVANCED_RENDER_OPTION_SINCE
    )
    if actual != expected:
        raise ValueError(
            "advanced render option type/default/control/constraints/risk/"
            "reload/version contract changed: "
            f"expected {expected!r}, found {actual!r}"
        )


def _window_behavior_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    registry = (
        'MS<Bool>("misc:always_follow_on_dnd", "Will make mouse focus follow the mouse when drag and dropping.", true),',
        'MS<Bool>("misc:enable_swallow", "Enable window swallowing", false),',
        'MS<String>("misc:swallow_regex", "The class regex to be used for windows that should be swallowed.", STRVAL_EMPTY),',
        'MS<String>("misc:swallow_exception_regex", "The title regex to be used for windows that should not be swallowed.", STRVAL_EMPTY),',
        'MS<Bool>("misc:focus_on_activate", "Whether Hyprland should focus an app that requests to be focused.", false),',
        'MS<Bool>("misc:mouse_move_focuses_monitor", "Whether mouse moving into a different monitor should focus it", true),',
        'MS<Int>("misc:on_focus_under_fullscreen", "if there is a fullscreen or maximized window, decide whether a tiled window requested to focus should replace it.", 2,',
        '{.min = 0, .max = 2, .map = OptionMap{{"ignore", 0}, {"take_over", 1}, {"exit_fullscreen", 2}}}),',
        'MS<Bool>("misc:exit_window_retains_fullscreen", "if true, closing a fullscreen window makes the next focused window fullscreen", false),',
        'MS<Bool>("misc:enable_anr_dialog", "whether to enable the ANR (app not responding) dialog when your apps hang", true),',
        'MS<Int>("misc:anr_missed_pings", "number of missed pings before showing the ANR dialog", 5, {.min = 1, .max = 20}),',
        'MS<Bool>("misc:size_limits_tiled", "whether to apply minsize and maxsize rules to tiled windows", false),',
        'MS<Int>("binds:focus_preferred_method", "sets the preferred focus finding method when using focuswindow/movewindow/etc with a direction.", 0, {.min = 0, .max = 1}),',
        'MS<Bool>("binds:ignore_group_lock", "If enabled, dispatchers like moveintogroup, moveoutofgroup and movewindoworgroup will ignore lock per group.", false),',
        'MS<Bool>("binds:movefocus_cycles_fullscreen", "If enabled, when on a fullscreen window, movefocus will cycle fullscreen.", false),',
        'MS<Bool>("binds:movefocus_cycles_groupfirst", "If enabled, when in a grouped window, movefocus will cycle windows in the groups first.", false),',
        'MS<Bool>("binds:window_direction_monitor_fallback", "If enabled, moving a window or focus over the edge of a monitor with a direction will move it to the next monitor.", true),',
        'MS<Bool>("binds:allow_pin_fullscreen", "Allows fullscreen to pinned windows, and restore their pinned status afterwards", false),',
    )
    group_lock_actions = (
        "ActionResult Actions::moveIntoGroup(Math::eDirection direction, std::optional<PHLWINDOW> w) {",
        'static auto PIGNOREGROUPLOCK = CConfigValue<Config::INTEGER>("binds:ignore_group_lock");',
        "if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked)",
        "if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->m_group->locked() || (window->m_group && window->m_group->locked())))",
        "ActionResult Actions::moveOutOfGroup(Math::eDirection direction, std::optional<PHLWINDOW> w) {",
        'static auto PIGNOREGROUPLOCK = CConfigValue<Config::INTEGER>("binds:ignore_group_lock");',
        "if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked)",
        "ActionResult Actions::moveWindowOrGroup(Math::eDirection direction, std::optional<PHLWINDOW> w) {",
        'static auto PIGNOREGROUPLOCK = CConfigValue<Config::INTEGER>("binds:ignore_group_lock");',
        "if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked) {",
        "if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->m_group->locked() || ISWINDOWGROUPLOCKED || ISWINDOWGROUPDENIED)) {",
        "if ((!*PIGNOREGROUPLOCK && ISWINDOWGROUPLOCKED) || !ISWINDOWGROUP || (ISWINDOWGROUPSINGLE && window->m_groupRules & Desktop::View::GROUP_SET_ALWAYS)) {",
        "} else if ((*PIGNOREGROUPLOCK || !ISWINDOWGROUPLOCKED) && ISWINDOWGROUP) {",
        "ActionResult Actions::moveIntoOrCreateGroup(Math::eDirection dir, std::optional<PHLWINDOW> w) {",
        'static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");',
        "if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked)",
        "if (!*PIGNOREGROUPLOCK && (GROUP->locked() || (PWINDOW->m_group && PWINDOW->m_group->locked())))",
    )
    actions_055 = (
        "ActionResult Actions::moveFocus(Math::eDirection dir) {",
        'static auto PFULLCYCLE = CConfigValue<Config::INTEGER>("binds:movefocus_cycles_fullscreen");',
        'static auto PGROUPCYCLE = CConfigValue<Config::INTEGER>("binds:movefocus_cycles_groupfirst");',
        'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
        "if (!PLASTWINDOW || !PLASTWINDOW->aliveAndVisible()) {",
        "if (*PMONITORFALLBACK)",
        "tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(dir));",
        "const auto PWINDOWTOCHANGETO = *PFULLCYCLE && PLASTWINDOW->isFullscreen() ?",
        "g_pCompositor->getWindowCycle(PLASTWINDOW, true, {}, false, dir != Math::DIRECTION_DOWN && dir != Math::DIRECTION_RIGHT, true) :",
        "g_pCompositor->getWindowInDirection(PLASTWINDOW, dir);",
        "if (*PGROUPCYCLE && PLASTWINDOW->m_group) {",
        "auto isTheOnlyGroupOnWs = !PWINDOWTOCHANGETO && g_pCompositor->m_monitors.size() == 1;",
        "PLASTWINDOW->m_group->moveCurrent(false);",
        "PLASTWINDOW->m_group->moveCurrent(true);",
        "switchToWindow(PWINDOWTOCHANGETO, *PFULLCYCLE && PLASTWINDOW->isFullscreen());",
        "if (*PMONITORFALLBACK && tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(dir)))",
        *group_lock_actions,
    )
    actions_056 = (
        "ActionResult Actions::moveFocus(Math::eDirection dir) {",
        'static auto PFULLCYCLE = CConfigValue<Config::INTEGER>("binds:movefocus_cycles_fullscreen");',
        'static auto PGROUPCYCLE = CConfigValue<Config::INTEGER>("binds:movefocus_cycles_groupfirst");',
        'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
        "if (!PLASTWINDOW || !PLASTWINDOW->aliveAndVisible()) {",
        "if (*PMONITORFALLBACK)",
        "tryMoveFocusToMonitor(State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).inDirection(dir).run());",
        "const auto PWINDOWTOCHANGETO = *PFULLCYCLE && Fullscreen::controller()->isFullscreen(PLASTWINDOW) && !Fullscreen::controller()->layoutManagedFS(PLASTWINDOW) ?",
        "Desktop::windowState()->query().cycle(PLASTWINDOW,",
        ".allowFullscreenBlocked = true}) :",
        "Desktop::windowState()->query().inDirection(PLASTWINDOW, dir);",
        "if (*PGROUPCYCLE && PLASTWINDOW->m_group) {",
        "auto isTheOnlyGroupOnWs = !PWINDOWTOCHANGETO && State::monitorState()->monitors().size() == 1;",
        "PLASTWINDOW->m_group->moveCurrent(false);",
        "PLASTWINDOW->m_group->moveCurrent(true);",
        "switchToWindow(PWINDOWTOCHANGETO, *PFULLCYCLE && Fullscreen::controller()->isFullscreen(PLASTWINDOW) && !Fullscreen::controller()->layoutManagedFS(PLASTWINDOW));",
        "if (*PMONITORFALLBACK && tryMoveFocusToMonitor(State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).inDirection(dir).run()))",
        *group_lock_actions,
    )
    focus_055 = (
        "PHLWINDOW CCompositor::getWindowInDirection(const CBox& box, PHLWORKSPACE pWorkspace, Math::eDirection dir, bool floatingPreference, PHLWINDOW ignoreWindow, bool useVectorAngles) {",
        'static auto PMETHOD = CConfigValue<Config::INTEGER>("binds:focus_preferred_method");',
        'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
        "if (!*PMONITORFALLBACK && pWorkspace->m_monitor != w->m_monitor)",
        "if (*PMETHOD == 0 /* history */) {",
        "const auto& HISTORY = Desktop::History::windowTracker()->fullHistory();",
        "if (windowIDX > leaderValue) {",
        "} else /* length */ {",
        "if (intersectLength > leaderValue) {",
        "if (!*PMONITORFALLBACK && pWorkspace->m_monitor != w->m_monitor)",
    )
    focus_056 = (
        "PHLWINDOW CWindowQuery::inDirection(const SWindowDirectionQuery& query) const {",
        'static auto PMETHOD = CConfigValue<Config::INTEGER>("binds:focus_preferred_method");',
        'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
        "if (!*PMONITORFALLBACK && query.workspace->m_monitor != w->m_monitor)",
        "if (*PMETHOD == 0 /* history */) {",
        "const auto& HISTORY = Desktop::History::windowTracker()->fullHistory();",
        "if (windowIDX > leaderValue || OVERRIDE_MIN_REQ) {",
        "} else /* length */ {",
        "if (intersectLength > leaderValue || OVERRIDE_MIN_REQ) {",
        "if (!*PMONITORFALLBACK && query.workspace->m_monitor != w->m_monitor)",
    )
    pin_055 = (
        "void CCompositor::setWindowFullscreenState(const PHLWINDOW PWINDOW, Desktop::View::SFullscreenState state) {",
        'static auto PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");',
        "if (*PALLOWPINFULLSCREEN && !PWINDOW->m_pinFullscreened && !PWINDOW->isFullscreen() && PWINDOW->m_pinned) {",
        "PWINDOW->m_pinned = false;",
        "PWINDOW->m_pinFullscreened = true;",
        "const bool CHANGEINTERNAL = !PWINDOW->m_pinned && PWINDOW->m_fullscreenState.internal != state.internal;",
        "if (*PALLOWPINFULLSCREEN && PWINDOW->m_pinFullscreened && PWINDOW->isFullscreen() && !PWINDOW->m_pinned && state.internal == FSMODE_NONE) {",
        "PWINDOW->m_pinned = true;",
        "PWINDOW->m_pinFullscreened = false;",
    )
    pin_056 = (
        "Handling Pinned windows - allow_pin_fullscreen",
        'static auto PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");',
        "if (*PALLOWPINFULLSCREEN && !window->m_pinFullscreened && window->m_pinned && !WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC)",
        "pinnedWindowRequetsInternalFS = true;",
        "if (*PALLOWPINFULLSCREEN && window->m_pinFullscreened && WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC && !window->m_pinned && targetInternalMode == FSMODE_NONE)",
        "pinnedWindowRequetsInternalFS = false;",
        "window->m_pinned = false;",
        "window->m_pinFullscreened = true;",
        "window->m_pinned = true;",
        "window->m_pinFullscreened = false;",
        "} else if (!(*PALLOWPINFULLSCREEN)) {",
        "targetInternalMode = FSMODE_NONE;",
    )
    anr = (
        'if (!NFsUtils::executableExistsInPath("hyprland-dialog")) {',
        'Log::logger->log(Log::ERR, "hyprland-dialog missing from PATH, cannot start ANRManager");',
        "return;",
        "m_active = true;",
        "void CANRManager::onTick() {",
        'static auto PENABLEANR    = CConfigValue<Config::INTEGER>("misc:enable_anr_dialog");',
        'static auto PANRTHRESHOLD = CConfigValue<Config::INTEGER>("misc:anr_missed_pings");',
        "if (!*PENABLEANR) {",
        "m_timer->updateTimeout(TIMER_TIMEOUT * 10);",
        "return;",
        "if (data->missedResponses >= *PANRTHRESHOLD) {",
        "if (!data->isRunning() && !data->dialogSaidWait) {",
        "data->runDialog(firstWindow->m_title, firstWindow->m_class, data->getPID());",
        "if (data->missedResponses == 0)",
        "data->dialogSaidWait = false;",
        "data->missedResponses++;",
        "void CANRManager::onResponse(SP<CANRManager::SANRData> data) {",
        "data->missedResponses = 0;",
        "if (data->isRunning())",
        "data->killDialog();",
        "bool CANRManager::isNotResponding(SP<CANRManager::SANRData> data) {",
        'static auto PANRTHRESHOLD = CConfigValue<Config::INTEGER>("misc:anr_missed_pings");',
        "return data->missedResponses > *PANRTHRESHOLD;",
    )
    input_behavior = (
        "void CInputManager::mouseMoveUnified(uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos) {",
        'static auto PFOLLOWONDND          = CConfigValue<Config::INTEGER>("misc:always_follow_on_dnd");',
        'static auto PMOUSEFOCUSMON        = CConfigValue<Config::INTEGER>("misc:mouse_move_focuses_monitor");',
        "const auto  FOLLOWMOUSE = *PFOLLOWONDND && PROTO::data->dndActive() ? 1 : *PFOLLOWMOUSE;",
        "if (PMONITOR != Desktop::focusState()->monitor() && (*PMOUSEFOCUSMON || refocus) && m_forcedFocus.expired())",
        "Desktop::focusState()->rawMonitorFocus(PMONITOR);",
    )
    window_behavior_055 = (
        "void CWindow::activate(bool force) {",
        'static auto PFOCUSONACTIVATE = CConfigValue<Config::INTEGER>("misc:focus_on_activate");',
        "m_isUrgent = true;",
        "if (!force &&",
        "(!m_ruleApplicator->focusOnActivate().valueOr(*PFOCUSONACTIVATE) || (m_suppressedEvents & SUPPRESS_ACTIVATE_FOCUSONLY) || (m_suppressedEvents & SUPPRESS_ACTIVATE)))",
        "if (m_isFloating)",
        "g_pCompositor->changeWindowZOrder(m_self.lock(), true);",
        "Desktop::focusState()->fullWindowFocus(m_self.lock(), FOCUS_REASON_DESKTOP_STATE_CHANGE);",
        "warpCursor();",
        "PHLWINDOW CWindow::getSwallower() {",
        'static auto PSWALLOWREGEX   = CConfigValue<std::string>("misc:swallow_regex");',
        'static auto PSWALLOWEXREGEX = CConfigValue<std::string>("misc:swallow_exception_regex");',
        'static auto PSWALLOW        = CConfigValue<Config::INTEGER>("misc:enable_swallow");',
        "if (!*PSWALLOW || std::string{*PSWALLOWREGEX} == STRVAL_EMPTY || (*PSWALLOWREGEX).empty())",
        "for (size_t i = 0; i < 25; ++i) {",
        "currentPid = getPPIDof(currentPid);",
        "for (auto const& w : g_pCompositor->m_windows) {",
        "if (!w->m_isMapped || !w->acceptsInput())",
        "if (w->getPID() == currentPid)",
        "candidates.push_back(w);",
        "if (!(*PSWALLOWREGEX).empty())",
        "std::erase_if(candidates, [&](const auto& other) { return !RE2::FullMatch(other->m_class, *PSWALLOWREGEX); });",
        "if (candidates.empty())",
        "if (!(*PSWALLOWEXREGEX).empty())",
        "std::erase_if(candidates, [&](const auto& other) { return RE2::FullMatch(other->m_title, *PSWALLOWEXREGEX); });",
        "if (candidates.empty())",
        "if (candidates.size() == 1)",
        "return candidates[0];",
        "for (auto const& w : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {",
        "if (std::ranges::find(candidates.begin(), candidates.end(), w.lock()) != candidates.end())",
        "return w.lock();",
        "return candidates[0];",
        "void CWindow::mapWindow() {",
        'static auto PNEWTAKESOVERFS    = CConfigValue<Config::INTEGER>("misc:on_focus_under_fullscreen");',
        "const auto SWALLOWER = getSwallower();",
        "m_swallowed          = SWALLOWER;",
        "if (m_swallowed)",
        "m_swallowed->m_currentlySwallowed = true;",
        "if (m_workspace->m_hasFullscreenWindow && !requestedInternalFSMode.has_value() && !requestedClientFSMode.has_value() && !m_isFloating) {",
        "if (*PNEWTAKESOVERFS == 0)",
        "m_noInitialFocus = true;",
        "else if (*PNEWTAKESOVERFS == 1)",
        "requestedInternalFSMode = m_workspace->m_fullscreenMode;",
        "else if (*PNEWTAKESOVERFS == 2)",
        "g_pCompositor->setWindowFullscreenInternal(m_workspace->getFullscreenWindow(), FSMODE_NONE);",
        "if (SWALLOWER) {",
        "g_layoutManager->removeTarget(SWALLOWER->layoutTarget());",
        "SWALLOWER->setHidden(true);",
        "void CWindow::unmapWindow() {",
        'static auto PEXITRETAINSFS = CConfigValue<Config::INTEGER>("misc:exit_window_retains_fullscreen");',
        "const auto  CURRENTWINDOWFSSTATE = isFullscreen();",
        "const auto  CURRENTFSMODE        = m_fullscreenState.internal;",
        "if (valid(m_swallowed)) {",
        "if (m_swallowed->m_currentlySwallowed) {",
        "m_swallowed->m_currentlySwallowed = false;",
        "m_swallowed->setHidden(false);",
        "g_layoutManager->newTarget(m_swallowed->layoutTarget(), m_workspace->m_space);",
        "m_swallowed->m_groupSwallowed = false;",
        "m_swallowed.reset();",
        "if (m_self.lock() == Desktop::focusState()->window()) {",
        "wasLastWindow = true;",
        "PHLWINDOW   candidate    = nextInGroup;",
        "if ((*PEXITRETAINSFS || candidate == nextInGroup) && CURRENTWINDOWFSSTATE)",
        "g_pCompositor->setWindowFullscreenInternal(candidate, CURRENTFSMODE);",
    )
    window_behavior_056 = (
        "void CWindow::activate(bool force) {",
        'static auto PFOCUSONACTIVATE = CConfigValue<Config::INTEGER>("misc:focus_on_activate");',
        "m_isUrgent = true;",
        "if (!force &&",
        "(!m_ruleApplicator->focusOnActivate().valueOr(*PFOCUSONACTIVATE) || (m_suppressedEvents & SUPPRESS_ACTIVATE_FOCUSONLY) || (m_suppressedEvents & SUPPRESS_ACTIVATE)))",
        "if (m_isFloating)",
        "Desktop::windowState()->raise(m_self.lock());",
        "Desktop::focusState()->fullWindowFocus(m_self.lock(), FOCUS_REASON_DESKTOP_STATE_CHANGE);",
        "warpCursor();",
        "PHLWINDOW CWindow::getSwallowee() {",
        'static auto PSWALLOWREGEX   = CConfigValue<std::string>("misc:swallow_regex");',
        'static auto PSWALLOWEXREGEX = CConfigValue<std::string>("misc:swallow_exception_regex");',
        'static auto PSWALLOW        = CConfigValue<Config::INTEGER>("misc:enable_swallow");',
        "if (!*PSWALLOW || std::string{*PSWALLOWREGEX} == STRVAL_EMPTY || (*PSWALLOWREGEX).empty())",
        "for (size_t i = 0; i < 25; ++i) {",
        "currentPid = getPPIDof(currentPid);",
        "for (auto const& w : Desktop::windowState()->windows()) {",
        "if (!w->m_isMapped || !w->acceptsInput())",
        "if (w->getPID() == currentPid)",
        "candidates.push_back(w);",
        "if (!(*PSWALLOWREGEX).empty())",
        "std::erase_if(candidates, [&](const auto& other) { return !RE2::FullMatch(other->m_class, *PSWALLOWREGEX); });",
        "if (candidates.empty())",
        "if (!(*PSWALLOWEXREGEX).empty())",
        "std::erase_if(candidates, [&](const auto& other) { return RE2::FullMatch(other->m_title, *PSWALLOWEXREGEX); });",
        "if (candidates.empty())",
        "if (candidates.size() == 1)",
        "return candidates[0];",
        "for (auto const& w : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {",
        "if (std::ranges::find(candidates.begin(), candidates.end(), w.lock()) != candidates.end())",
        "return w.lock();",
        "return candidates[0];",
        "void CWindow::mapWindow() {",
        'static auto PNEWTAKESOVERFS    = CConfigValue<Config::INTEGER>("misc:on_focus_under_fullscreen");',
        "const auto SWALLOWEE = getSwallowee();",
        "if (SWALLOWEE && !SWALLOWEE->m_hasSwallower) {",
        "SWALLOWEE->m_currentlySwallowed = true;",
        "SWALLOWEE->m_hasSwallower       = true;",
        "m_swallowee                     = SWALLOWEE;",
        "if (Fullscreen::controller()->hasFullscreen(m_workspace) && !requestedInternalFSMode.has_value() && !requestedClientFSMode.has_value() && !m_isFloating) {",
        "if (*PNEWTAKESOVERFS == 0)",
        "m_noInitialFocus = true;",
        "else if (*PNEWTAKESOVERFS == 1)",
        "requestedInternalFSMode = Fullscreen::controller()->getFullscreenModes(m_workspace).internal;",
        "else if (*PNEWTAKESOVERFS == 2)",
        "Fullscreen::controller()->setFullscreenMode(Fullscreen::controller()->getFullscreenWindow(m_workspace), Fullscreen::FSMODE_NONE, std::nullopt);",
        "if (m_swallowee) {",
        "g_layoutManager->removeTarget(SWALLOWEE->layoutTarget());",
        "SWALLOWEE->setHidden(true);",
        "void CWindow::unmapWindow() {",
        'static auto PEXITRETAINSFS = CConfigValue<Config::INTEGER>("misc:exit_window_retains_fullscreen");',
        "const bool  IS_CURRENT_WINDOW_FS      = Fullscreen::controller()->isFullscreen(m_self.lock());",
        "const auto  CURRENT_WINDOW_FS_MODES   = Fullscreen::controller()->getFullscreenModes(m_self.lock());",
        "const bool  CURRENT_FS_LAYOUT_HANDLED = IS_CURRENT_WINDOW_FS ? Fullscreen::controller()->layoutManagedFS(m_self.lock()) : false;",
        "if (const auto SWALLOWEE = m_swallowee.lock()) {",
        "if (SWALLOWEE->m_isMapped && SWALLOWEE->m_currentlySwallowed) {",
        "SWALLOWEE->m_currentlySwallowed = false;",
        "SWALLOWEE->setHidden(false);",
        "g_layoutManager->newTarget(SWALLOWEE->layoutTarget(), m_workspace->m_space);",
        "SWALLOWEE->m_groupSwallowed = false;",
        "SWALLOWEE->m_hasSwallower   = false;",
        "m_swallowee.reset();",
        "if (m_self.lock() == Desktop::focusState()->window()) {",
        "wasLastWindow = true;",
        "PHLWINDOW   candidate    = nextInGroup;",
        "if ((*PEXITRETAINSFS || candidate == nextInGroup) && IS_CURRENT_WINDOW_FS)",
        "Fullscreen::controller()->setFullscreenMode(candidate, CURRENT_WINDOW_FS_MODES.internal, std::nullopt, CURRENT_FS_LAYOUT_HANDLED);",
    )
    focus_under_fullscreen_055 = (
        "static SFullscreenWorkspaceFocusResult onFullscreenWorkspaceFocusWindow(PHLWINDOW pWindow, bool forceFSCycle) {",
        "const auto FSWINDOW = pWindow->m_workspace->getFullscreenWindow();",
        "const auto FSMODE   = pWindow->m_workspace->m_fullscreenMode;",
        "if (pWindow->m_isFloating) {",
        "return {};",
        'static auto PONFOCUSUNDERFS = CConfigValue<Config::INTEGER>("misc:on_focus_under_fullscreen");',
        "switch (*PONFOCUSUNDERFS) {",
        "case 0:",
        "return {.overrideFocusWindow = FSWINDOW};",
        "case 2:",
        "if (!forceFSCycle) {",
        "g_pCompositor->setWindowFullscreenInternal(FSWINDOW, FSMODE_NONE);",
        "case 1:",
        "g_pCompositor->setWindowFullscreenInternal(FSWINDOW, FSMODE_NONE);",
        "g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE);",
        'default: Log::logger->log(Log::ERR, "Invalid misc:on_focus_under_fullscreen mode: {}", *PONFOCUSUNDERFS); break;',
        "void CFocusState::fullWindowFocus(PHLWINDOW pWindow, eFocusReason reason, SP<CWLSurfaceResource> surface, bool forceFSCycle) {",
        "const auto CURRENT_FS_MODE = pWindow->m_workspace->m_hasFullscreenWindow ? pWindow->m_workspace->m_fullscreenMode : FSMODE_NONE;",
        "if (CURRENT_FS_MODE != FSMODE_NONE) {",
        "const auto RESULT = onFullscreenWorkspaceFocusWindow(pWindow, forceFSCycle);",
        "if (RESULT.overrideFocusWindow)",
        "pWindow = RESULT.overrideFocusWindow;",
    )
    focus_under_fullscreen_056 = (
        "static SFullscreenWorkspaceFocusResult onFullscreenWorkspaceFocusWindow(PHLWINDOW pWindow, bool forceFSCycle) {",
        "const auto FSWINDOW        = Fullscreen::controller()->getFullscreenWindow(pWindow->m_workspace);",
        "const auto FSMODE_INTERNAL = Fullscreen::controller()->getFullscreenModes(pWindow->m_workspace).internal;",
        "const auto LAYOUT_HANDLED  = Fullscreen::controller()->layoutManagedFS(FSWINDOW);",
        "if (pWindow->m_isFloating) {",
        "return {};",
        'static auto PONFOCUSUNDERFS = CConfigValue<Config::INTEGER>("misc:on_focus_under_fullscreen");',
        "switch (*PONFOCUSUNDERFS) {",
        "case 0:",
        "return {.overrideFocusWindow = FSWINDOW};",
        "case 2:",
        "if (!forceFSCycle) {",
        "Fullscreen::controller()->setFullscreenMode(FSWINDOW, Fullscreen::FSMODE_NONE);",
        "case 1:",
        "Fullscreen::controller()->setFullscreenMode(FSWINDOW, Fullscreen::FSMODE_NONE);",
        "Fullscreen::controller()->setFullscreenMode(pWindow, FSMODE_INTERNAL, std::nullopt, LAYOUT_HANDLED);",
        'default: Log::logger->log(Log::ERR, "Invalid misc:on_focus_under_fullscreen mode: {}", *PONFOCUSUNDERFS); break;',
        "void CFocusState::fullWindowFocus(PHLWINDOW pWindow, eFocusReason reason, SP<CWLSurfaceResource> surface, bool forceFSCycle) {",
        "const auto FSWINDOW = Fullscreen::controller()->getFullscreenWindow(pWindow->m_workspace);",
        "if (FSWINDOW && !Fullscreen::controller()->layoutManagedFS(FSWINDOW)) {",
        "const auto RESULT = onFullscreenWorkspaceFocusWindow(pWindow, forceFSCycle);",
        "if (RESULT.overrideFocusWindow)",
        "pWindow = RESULT.overrideFocusWindow;",
    )
    size_rule_behavior = (
        "case WINDOW_RULE_EFFECT_MAX_SIZE: {",
        'static auto PCLAMP_TILED = CConfigValue<Config::INTEGER>("misc:size_limits_tiled");',
        "if (*PCLAMP_TILED || m_window->m_isFloating)",
        "m_window->clampWindowSize(std::nullopt, m_maxSize.first.value());",
        "case WINDOW_RULE_EFFECT_MIN_SIZE: {",
        'static auto PCLAMP_TILED = CConfigValue<Config::INTEGER>("misc:size_limits_tiled");',
        "if (*PCLAMP_TILED || m_window->m_isFloating)",
        "m_window->clampWindowSize(m_minSize.first.value(), std::nullopt);",
    )
    tiled_target_size_055 = (
        'static auto PCLAMP_TILED = CConfigValue<Config::INTEGER>("misc:size_limits_tiled");',
        "if (*PCLAMP_TILED) {",
        "Vector2D minSize = m_window->m_ruleApplicator->minSize().valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});",
        "Vector2D maxSize = m_window->isFullscreen() ? Vector2D{INFINITY, INFINITY} : m_window->m_ruleApplicator->maxSize().valueOr(Vector2D{INFINITY, INFINITY});",
        "calcSize         = calcSize.clamp(minSize, maxSize);",
        "calcPos += (availableSpace - calcSize) / 2.0;",
        "calcPos.x = std::clamp(calcPos.x, MONITOR_WORKAREA.x, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w - calcSize.x);",
        "calcPos.y = std::clamp(calcPos.y, MONITOR_WORKAREA.y, MONITOR_WORKAREA.y + MONITOR_WORKAREA.h - calcSize.y);",
    )
    tiled_target_size_056 = (
        'static auto PCLAMP_TILED = CConfigValue<Config::INTEGER>("misc:size_limits_tiled");',
        "if (*PCLAMP_TILED) {",
        "Vector2D minSize = m_window->m_ruleApplicator->minSize().valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});",
        "Fullscreen::controller()->isFullscreen(m_window.lock()) ? Vector2D{INFINITY, INFINITY} : m_window->m_ruleApplicator->maxSize().valueOr(Vector2D{INFINITY, INFINITY});",
        "calcSize = calcSize.clamp(minSize, maxSize);",
        "calcPos += (availableSpace - calcSize) / 2.0;",
        "calcPos.x = std::clamp(calcPos.x, MONITOR_WORKAREA.x, std::max(MONITOR_WORKAREA.x, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w - calcSize.x));",
        "calcPos.y = std::clamp(calcPos.y, MONITOR_WORKAREA.y, std::max(MONITOR_WORKAREA.y, MONITOR_WORKAREA.y + MONITOR_WORKAREA.h - calcSize.y));",
    )

    return {
        "0.55.0": {
            REGISTRY_PATH: registry,
            Path("src/config/shared/actions/ConfigActions.cpp"): actions_055,
            Path("src/Compositor.cpp"): (*focus_055, *pin_055),
            Path("src/managers/ANRManager.cpp"): anr,
            Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"): (
                "void CDwindleAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "const auto PMONITORFOCAL = g_pCompositor->getMonitorFromVector(FOCAL_POINT.value_or(t->position().middle()));",
                "if (PMONITORFOCAL != m_parent->space()->workspace()->m_monitor && !*PMONITORFALLBACK)",
                "if (PMONITORFOCAL != m_parent->space()->workspace()->m_monitor) {",
                "t->assignToSpace(PMONITORFOCAL->m_activeWorkspace->m_space, FOCAL_POINT);",
            ),
            Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"): (
                "void CMasterAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "const auto PMONINDIR = g_pCompositor->getMonitorInDirection(t->space()->workspace()->m_monitor.lock(), dir);",
                "if (t->window()->m_workspace != targetWs) {",
                "if (!*PMONITORFALLBACK)",
                "t->assignToSpace(targetWs->m_space, focalPointForDir(t, dir));",
            ),
            Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"): (
                "void CScrollingAlgorithm::moveTargetTo(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "if (!commenceDir()) {",
                "if (!*PMONITORFALLBACK)",
                "const auto MONINDIR = g_pCompositor->getMonitorInDirection(m_parent->space()->workspace()->m_monitor.lock(), dir);",
                "t->assignToSpace(MONINDIR->m_activeWorkspace->m_space, focalPointForDir(t, dir));",
            ),
            Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"): (
                "void CMonocleAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "if (!*PMONITORFALLBACK)",
                "const auto PMONINDIR = g_pCompositor->getMonitorInDirection(PMONITOR, dir);",
                "if (PMONINDIR && PMONINDIR != PMONITOR) {",
                "t->assignToSpace(TARGETWS->m_space, focalPointForDir(t, dir));",
            ),
            Path("src/managers/input/InputManager.cpp"): input_behavior,
            Path("src/desktop/view/Window.cpp"): window_behavior_055,
            Path("src/desktop/state/FocusState.cpp"): focus_under_fullscreen_055,
            Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"): size_rule_behavior,
            Path("src/layout/target/WindowTarget.cpp"): tiled_target_size_055,
        },
        "0.56.1": {
            REGISTRY_PATH: registry,
            Path("src/config/shared/actions/ConfigActions.cpp"): actions_056,
            Path("src/desktop/state/WindowQuery.cpp"): focus_056,
            Path("src/managers/fullscreen/FullscreenController.cpp"): pin_056,
            Path("src/managers/ANRManager.cpp"): anr,
            Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"): (
                "void CDwindleAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "const auto PMONITORFOCAL = State::monitorState()->query().vec(FOCAL_POINT.value_or(t->position().middle())).run();",
                "if (PMONITORFOCAL != m_parent->space()->workspace()->m_monitor && !*PMONITORFALLBACK)",
                "if (PMONITORFOCAL != m_parent->space()->workspace()->m_monitor) {",
                "t->assignToSpace(PMONITORFOCAL->m_activeWorkspace->m_space, FOCAL_POINT);",
            ),
            Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"): (
                "void CMasterAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "const auto PMONINDIR = State::monitorState()->query().relativeTo(t->space()->workspace()->m_monitor.lock()).inDirection(dir).run();",
                "if (t->window()->m_workspace != targetWs) {",
                "if (!*PMONITORFALLBACK)",
                "t->assignToSpace(targetWs->m_space, focalPointForDir(t, dir));",
            ),
            Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"): (
                "void CScrollingAlgorithm::moveTargetTo(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "if (!commenceDir()) {",
                "if (!*PMONITORFALLBACK)",
                "const auto MONINDIR = State::monitorState()->query().relativeTo(m_parent->space()->workspace()->m_monitor.lock()).inDirection(dir).run();",
                "t->assignToSpace(MONINDIR->m_activeWorkspace->m_space, focalPointForDir(t, dir));",
            ),
            Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"): (
                "void CMonocleAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {",
                'static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");',
                "if (!*PMONITORFALLBACK)",
                "const auto PMONINDIR = State::monitorState()->query().relativeTo(PMONITOR).inDirection(dir).run();",
                "if (PMONINDIR && PMONINDIR != PMONITOR) {",
                "t->assignToSpace(TARGETWS->m_space, focalPointForDir(t, dir));",
            ),
            Path("src/managers/input/InputManager.cpp"): input_behavior,
            Path("src/desktop/view/Window.cpp"): window_behavior_056,
            Path("src/desktop/state/FocusState.cpp"): focus_under_fullscreen_056,
            Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp"): size_rule_behavior,
            Path("src/layout/target/WindowTarget.cpp"): tiled_target_size_056,
        },
    }


def _assert_window_behavior_contract(
    window_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify authored focus, group-lock, fullscreen, and monitor-edge behavior."""
    requirements_by_version = _window_behavior_contract_requirements()
    if set(requirements_by_version) != set(WINDOW_BEHAVIOR_SOURCE_PATHS):
        raise ValueError("window behavior patch inventory is incomplete")

    for version, expected_paths in WINDOW_BEHAVIOR_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} window behavior semantic inventory is incomplete"
            )
        sources = window_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} window behavior source inventory is incomplete"
            )

        registry = sources[REGISTRY_PATH].decode("utf-8")
        for option_path in WINDOW_BEHAVIOR_OPTION_PATHS:
            if registry.count(f'"{option_path}"') != 1:
                raise ValueError(
                    f"Hyprland {version} window registry count changed in "
                    f"{REGISTRY_PATH} for {option_path}"
                )

        actions_path = Path("src/config/shared/actions/ConfigActions.cpp")
        focus_path = (
            Path("src/Compositor.cpp")
            if version == "0.55.0"
            else Path("src/desktop/state/WindowQuery.cpp")
        )
        fullscreen_path = (
            Path("src/Compositor.cpp")
            if version == "0.55.0"
            else Path("src/managers/fullscreen/FullscreenController.cpp")
        )
        exact_runtime_uses: dict[Path, dict[str, int]] = {
            actions_path: {
                "binds:ignore_group_lock": 4,
                "binds:movefocus_cycles_fullscreen": 1,
                "binds:movefocus_cycles_groupfirst": 1,
                "binds:window_direction_monitor_fallback": 1,
            },
            focus_path: {
                "binds:focus_preferred_method": 1,
                "binds:window_direction_monitor_fallback": 1,
            },
        }
        exact_runtime_uses.setdefault(fullscreen_path, {})[
            "binds:allow_pin_fullscreen"
        ] = 1
        exact_runtime_uses[Path("src/managers/ANRManager.cpp")] = {
            "misc:enable_anr_dialog": 1,
            "misc:anr_missed_pings": 2,
        }
        exact_runtime_uses[Path("src/managers/input/InputManager.cpp")] = {
            "misc:always_follow_on_dnd": 1,
            "misc:mouse_move_focuses_monitor": 1,
        }
        exact_runtime_uses[Path("src/desktop/view/Window.cpp")] = {
            "misc:focus_on_activate": 1,
            "misc:on_focus_under_fullscreen": 1,
            "misc:exit_window_retains_fullscreen": 1,
            "misc:enable_swallow": 1,
            "misc:swallow_regex": 1,
            "misc:swallow_exception_regex": 1,
        }
        exact_runtime_uses[Path("src/desktop/state/FocusState.cpp")] = {
            "misc:on_focus_under_fullscreen": 1,
        }
        exact_runtime_uses[
            Path("src/desktop/rule/windowRule/WindowRuleApplicator.cpp")
        ] = {
            "misc:size_limits_tiled": 2,
        }
        exact_runtime_uses[Path("src/layout/target/WindowTarget.cpp")] = {
            "misc:size_limits_tiled": 1,
        }
        for path in (
            Path("src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
            Path("src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"),
            Path("src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"),
            Path("src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"),
        ):
            exact_runtime_uses[path] = {
                "binds:window_direction_monitor_fallback": 1,
            }
        for path, option_counts in exact_runtime_uses.items():
            source = sources[path].decode("utf-8")
            for option_path, expected_count in option_counts.items():
                if source.count(f'"{option_path}"') != expected_count:
                    raise ValueError(
                        f"Hyprland {version} window behavior gate count changed "
                        f"in {path} for {option_path}"
                    )

        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(
                    re.sub(r"\s+", " ", fragment)
                    for fragment in requirements[path]
                ),
                "window behavior",
            )


def _workspace_behavior_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    registry = (
        'MS<Bool>("binds:workspace_back_and_forth", "If enabled, an attempt to switch to the currently focused workspace will instead switch to the previous workspace.", false),',
        'MS<Bool>("binds:hide_special_on_workspace_change", "If enabled, changing the active workspace will hide the special workspace on the monitor.", false),',
        'MS<Bool>("binds:allow_workspace_cycles", "If enabled, workspaces don\'t forget their previous workspace.", false),',
        'MS<Int>("binds:workspace_center_on", "Whether switching workspaces should center the cursor on the workspace (0) or on the last active window (1)", 1,',
        "{.min = 0, .max = 1}),",
        'MS<Bool>("cursor:no_warps", "if true, will not warp the cursor in many cases", false),',
        'MS<Bool>("cursor:persistent_warps", "When a window is refocused, the cursor returns to its last position relative to that window.", false),',
        'MS<Int>("cursor:warp_on_change_workspace", "Move the cursor to the last focused window after changing the workspace.", 0,',
        '{.min = 0, .max = 2, .map = OptionMap{{"disable", 0}, {"enable", 1}, {"force", 2}}}),',
        'MS<Int>("cursor:warp_on_toggle_special", "Move the cursor to the last focused window when toggling a special workspace.", 0,',
        '{.min = 0, .max = 2, .map = OptionMap{{"disable", 0}, {"enable", 1}, {"force", 2}}}),',
    )
    common_actions = (
        "ActionResult Actions::changeWorkspace(PHLWORKSPACE ws) {",
        'static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("binds:hide_special_on_workspace_change");',
        'static auto PWORKSPACECENTERON = CConfigValue<Config::INTEGER>("binds:workspace_center_on");',
        "if (ws->m_isSpecialWorkspace) {",
        "PMONITOR->setSpecialWorkspace(ws);",
        "return {};",
        "if (*PHIDESPECIALONWORKSPACECHANGE)",
        "PMONITORWORKSPACEOWNER->setSpecialWorkspace(nullptr);",
        "if (PMONITOR != PMONITORWORKSPACEOWNER) {",
        "Vector2D middle = PMONITORWORKSPACEOWNER->middle();",
        "auto pWindow = ws->getFocusCandidate();",
        "if (*PWORKSPACECENTERON == 1)",
        "middle = pWindow->middle();",
    )
    common_actions_after_cross_monitor = (
        'const static auto PWARPONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("cursor:warp_on_change_workspace");',
        "if (*PWARPONWORKSPACECHANGE > 0) {",
        "auto PLAST = ws->getLastFocusedWindow();",
        "PLAST->warpCursor(*PWARPONWORKSPACECHANGE == 2);",
        "static PHLWORKSPACE resolveWorkspaceForChange(const std::string& args) {",
        'static auto PBACKANDFORTH = CConfigValue<Config::INTEGER>("binds:workspace_back_and_forth");',
        'if (args.starts_with("previous")) {',
        "Desktop::History::workspaceTracker()->previousWorkspaceIDName(PCURRENTWORKSPACE",
        "if (workspaceToChangeTo == PCURRENTWORKSPACE->m_id && (*PBACKANDFORTH || EXPLICITPREVIOUS)) {",
        "Desktop::History::workspaceTracker()->previousWorkspaceIDName(PCURRENTWORKSPACE",
        "ActionResult Actions::toggleSpecial(PHLWORKSPACE special) {",
        "if (requestedWorkspaceIsAlreadyOpen && specialOpenOnMonitor == special->m_id) {",
        "focusedWorkspace = PMONITOR->m_activeWorkspace;",
        "PMONITOR->setSpecialWorkspace(special);",
        "focusedWorkspace = special;",
        'const static auto PWARPONTOGGLESPECIAL = CConfigValue<Config::INTEGER>("cursor:warp_on_toggle_special");',
        "if (*PWARPONTOGGLESPECIAL > 0) {",
        "auto PLAST = focusedWorkspace->getLastFocusedWindow();",
        "PLAST->warpCursor(*PWARPONTOGGLESPECIAL == 2);",
    )
    history = (
        "void CWorkspaceHistoryTracker::track(PHLWORKSPACE ws) {",
        'static auto PALLOWWORKSPACECYCLES = CConfigValue<Config::INTEGER>("binds:allow_workspace_cycles");',
        "if (!m_history.empty() && m_history.front().workspace == ws && !*PALLOWWORKSPACECYCLES)",
        "return;",
        "std::erase_if(m_history, [&](const auto& entry) { return entry.workspace == ws; });",
        "m_history.push_front(SHistoryEntry{.workspace = ws, .monitor = ws->m_monitor, .name = ws->m_name, .id = ws->m_id});",
        "const CWorkspaceHistoryTracker::SHistoryEntry CWorkspaceHistoryTracker::previousWorkspace(PHLWORKSPACE ws) {",
        "if (it != m_history.end() && std::next(it) != m_history.end())",
        "return *std::next(it);",
        "const CWorkspaceHistoryTracker::SHistoryEntry CWorkspaceHistoryTracker::previousWorkspace(PHLWORKSPACE ws, PHLMONITOR restrict) {",
        "if (it->monitor == restrict)",
        "return *it;",
    )
    history_header = (
        "struct SHistoryEntry {",
        "PHLWORKSPACEREF workspace;",
        "PHLMONITORREF monitor;",
        "WORKSPACEID id = WORKSPACE_INVALID;",
        "const SHistoryEntry previousWorkspace(PHLWORKSPACE ws);",
        "SWorkspaceIDName previousWorkspaceIDName(PHLWORKSPACE ws);",
        "const SHistoryEntry previousWorkspace(PHLWORKSPACE ws, PHLMONITOR restrict);",
        "SWorkspaceIDName previousWorkspaceIDName(PHLWORKSPACE ws, PHLMONITOR restrict);",
        "std::deque<SHistoryEntry> m_history;",
        "void track(PHLWORKSPACE w);",
    )
    common_placement = (
        'static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("binds:hide_special_on_workspace_change");',
        "const bool SWITCHINGISACTIVE = POLDMON ? POLDMON->m_activeWorkspace == pWorkspace : false;",
        "if (SWITCHINGISACTIVE && POLDMON == Desktop::focusState()->monitor()) {",
        "if (*PHIDESPECIALONWORKSPACECHANGE)",
        "pMonitor->setSpecialWorkspace(nullptr);",
        "Desktop::focusState()->rawMonitorFocus(pMonitor);",
        "pMonitor->m_activeWorkspace = pWorkspace;",
    )
    return {
        "0.55.0": {
            REGISTRY_PATH: registry,
            Path("src/config/shared/actions/ConfigActions.cpp"): (
                *common_actions,
                "g_pCompositor->warpCursorTo(middle);",
                *common_actions_after_cross_monitor,
            ),
            Path("src/desktop/history/WorkspaceHistoryTracker.cpp"): history,
            Path("src/desktop/history/WorkspaceHistoryTracker.hpp"): history_header,
            Path("src/desktop/view/Window.cpp"): (
                "void CWindow::warpCursor(bool force) {",
                'static auto PERSISTENTWARPS = CConfigValue<Config::INTEGER>("cursor:persistent_warps");',
                "const auto coords = m_relativeCursorCoordsOnLastWarp;",
                "m_relativeCursorCoordsOnLastWarp.x = -1;",
                "if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < m_size)",
                "g_pCompositor->warpCursorTo(m_position + coords, force);",
                "else",
                "g_pCompositor->warpCursorTo(middle(), force);",
            ),
            Path("src/Compositor.cpp"): (
                "void CCompositor::moveWorkspaceToMonitor(PHLWORKSPACE pWorkspace, PHLMONITOR pMonitor, bool noWarpCursor) {",
                *common_placement,
                "void CCompositor::warpCursorTo(const Vector2D& pos, bool force) {",
                'static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps");',
                "if (*PNOWARPS && !force) {",
                "Desktop::focusState()->rawMonitorFocus(PMONITORNEW);",
                "return;",
                "g_pPointerManager->warpTo(pos);",
            ),
        },
        "0.56.1": {
            REGISTRY_PATH: registry,
            Path("src/config/shared/actions/ConfigActions.cpp"): (
                *common_actions,
                "Pointer::pointerController()->warpTo(middle);",
                *common_actions_after_cross_monitor,
            ),
            Path("src/desktop/history/WorkspaceHistoryTracker.cpp"): history,
            Path("src/desktop/history/WorkspaceHistoryTracker.hpp"): history_header,
            Path("src/desktop/view/Window.cpp"): (
                "void CWindow::warpCursor(bool force) {",
                'static auto PERSISTENTWARPS = CConfigValue<Config::INTEGER>("cursor:persistent_warps");',
                "const auto coords = m_relativeCursorCoordsOnLastWarp;",
                "m_relativeCursorCoordsOnLastWarp.x = -1;",
                "const auto BOX = layoutBox();",
                "if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < BOX.size())",
                "Pointer::pointerController()->warpTo(BOX.pos() + coords, force);",
                "else",
                "Pointer::pointerController()->warpTo(middle(), force);",
            ),
            Path("src/state/WorkspacePlacementController.cpp"): (
                "void CWorkspacePlacementController::moveWorkspaceToMonitor(PHLWORKSPACE pWorkspace, PHLMONITOR pMonitor, bool noWarpCursor) const {",
                *common_placement,
            ),
            Path("src/pointer/PointerController.cpp"): (
                "void CPointerController::warpTo(const Vector2D& pos, bool force) const {",
                'static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps");',
                "if (*PNOWARPS && !force) {",
                "Desktop::focusState()->rawMonitorFocus(PMONITORNEW);",
                "return;",
                "Pointer::mgr()->warpTo(pos);",
            ),
        },
    }


def _assert_workspace_behavior_contract(
    workspace_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify workspace switching, history, and cursor-placement behavior."""
    requirements_by_version = _workspace_behavior_contract_requirements()
    if set(requirements_by_version) != set(WORKSPACE_BEHAVIOR_SOURCE_PATHS):
        raise ValueError("workspace behavior patch inventory is incomplete")

    for version, expected_paths in WORKSPACE_BEHAVIOR_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} workspace behavior semantic inventory is incomplete"
            )
        sources = workspace_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} workspace behavior source inventory is incomplete"
            )
        reviewed_fragments = " ".join(
            fragment
            for path in expected_paths
            for fragment in requirements[path]
        )
        for option_path in (
            *WORKSPACE_BEHAVIOR_OPTION_PATHS,
            *WORKSPACE_BEHAVIOR_INTERACTION_OPTION_PATHS,
        ):
            if f'"{option_path}"' not in reviewed_fragments:
                raise ValueError(
                    f"Hyprland {version} has no workspace behavior assertion for {option_path}"
                )

        registry = sources[REGISTRY_PATH].decode("utf-8")
        for option_path in (
            *WORKSPACE_BEHAVIOR_OPTION_PATHS,
            *WORKSPACE_BEHAVIOR_INTERACTION_OPTION_PATHS,
        ):
            if registry.count(f'"{option_path}"') != 1:
                raise ValueError(
                    f"Hyprland {version} workspace registry count changed in "
                    f"{REGISTRY_PATH} for {option_path}"
                )
        placement_path = (
            Path("src/Compositor.cpp")
            if version == "0.55.0"
            else Path("src/state/WorkspacePlacementController.cpp")
        )
        warp_gate_path = (
            Path("src/Compositor.cpp")
            if version == "0.55.0"
            else Path("src/pointer/PointerController.cpp")
        )
        exact_uses = {
            Path("src/config/shared/actions/ConfigActions.cpp"): (
                "binds:hide_special_on_workspace_change",
                "binds:workspace_back_and_forth",
                "binds:workspace_center_on",
                "cursor:warp_on_change_workspace",
                "cursor:warp_on_toggle_special",
            ),
            Path("src/desktop/history/WorkspaceHistoryTracker.cpp"): (
                "binds:allow_workspace_cycles",
            ),
            Path("src/desktop/view/Window.cpp"): (
                "cursor:persistent_warps",
            ),
        }
        exact_uses[placement_path] = (
            *exact_uses.get(placement_path, ()),
            "binds:hide_special_on_workspace_change",
        )
        exact_uses[warp_gate_path] = (
            *exact_uses.get(warp_gate_path, ()),
            "cursor:no_warps",
        )
        for path, option_paths in exact_uses.items():
            source = sources[path].decode("utf-8")
            for option_path in option_paths:
                if source.count(f'"{option_path}"') != 1:
                    raise ValueError(
                        f"Hyprland {version} workspace gate count changed in "
                        f"{path} for {option_path}"
                    )

        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(re.sub(r"\s+", " ", fragment) for fragment in requirements[path]),
                "workspace behavior",
            )


def _binding_runtime_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    reload_requirements = (
        "void CConfigManager::reload() {",
        "auto phase1Load = [this]() -> bool {",
        "if (luaL_loadfile(m_lua, m_mainConfigPath.c_str()) != LUA_OK) {",
        "if (!phase1Load()) {",
        "if (g_pKeybindManager) {",
        "for (const auto& kb : g_pKeybindManager->m_keybinds) {",
        'if (kb->handler == "__lua")',
        "luaL_unref(m_lua, LUA_REGISTRYINDEX, std::stoi(kb->arg));",
        "g_pKeybindManager->clearKeybinds();",
        "reinitLuaState();",
        "if (!phase1Load()) {",
        "reregisterLuaPluginFns();",
        'if (guardedPCall(0, 0, 1, LUA_TIMEOUT_CONFIG_RELOAD_MS, "config reload") != LUA_OK) {',
    )
    registration_requirements = (
        "void Internal::registerBindingsImpl(lua_State* L, CConfigManager* mgr) {",
        'g_pKeybindManager->m_dispatchers["__lua"] = [L](std::string arg) -> SDispatchResult {',
        "int ref = std::stoi(arg);",
        "lua_rawgeti(L, LUA_REGISTRYINDEX, ref);",
        'status = mgr->guardedPCall(0, 1, 0, CConfigManager::LUA_TIMEOUT_KEYBIND_CALLBACK_MS, "keybind callback");',
        "status = lua_pcall(L, 0, 1, 0);",
        "auto result = dispatchResultFromLua(L, -1);",
        "return result;",
    )
    action_header_requirements = (
        "class CActionState {",
        'std::string m_currentSubmap = ""; // current keybind submap name',
        "UP<CActionState>& state();",
    )
    action_requirements = (
        "UP<CActionState>& Actions::state() {",
        "static UP<CActionState> p = makeUnique<CActionState>();",
        "return p;",
        "ActionResult Actions::setSubmap(const std::string& submap) {",
        'if (submap == "reset" || submap.empty()) {',
        'Config::Actions::state()->m_currentSubmap = "";',
        "for (const auto& k : g_pKeybindManager->m_keybinds) {",
        "if (k->submap.name == submap) {",
        "Config::Actions::state()->m_currentSubmap = submap;",
        'return std::unexpected(std::format("Cannot set submap {}, submap doesn\'t exist (wasn\'t registered!)", submap));',
        "ActionResult Actions::cycleNext(",
    )
    symbol_resolution_requirements = (
        "void CKeybindManager::updateXKBTranslationState() {",
        "if (m_xkbTranslationState) {",
        "xkb_state_unref(m_xkbTranslationState);",
        "m_xkbTranslationState = nullptr;",
        'static auto PFILEPATH = CConfigValue<std::string>("input:kb_file");',
        'static auto PRULES = CConfigValue<std::string>("input:kb_rules");',
        'static auto PMODEL = CConfigValue<std::string>("input:kb_model");',
        'static auto PLAYOUT = CConfigValue<std::string>("input:kb_layout");',
        'static auto PVARIANT = CConfigValue<std::string>("input:kb_variant");',
        'static auto POPTIONS = CConfigValue<std::string>("input:kb_options");',
        "xkb_rule_names rules = {.rules = RULES.c_str(), .model = MODEL.c_str(), .layout = LAYOUT.c_str(), .variant = VARIANT.c_str(), .options = OPTIONS.c_str()};",
        "FILE* const KEYMAPFILE = FILEPATH.empty() ? nullptr : fopen(absolutePath(FILEPATH, Config::mgr()->currentConfigPath()).c_str(), \"r\");",
        "auto PKEYMAP = KEYMAPFILE ? xkb_keymap_new_from_file(PCONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS) :",
        "xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);",
        "if (!PKEYMAP) {",
        "memset(&rules, 0, sizeof(rules));",
        "PKEYMAP = xkb_keymap_new_from_names2(PCONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);",
        "xkb_context_unref(PCONTEXT);",
        "m_xkbTranslationState = xkb_state_new(PKEYMAP);",
        "xkb_keymap_unref(PKEYMAP);",
        "bool CKeybindManager::onKeyEvent(std::any event, SP<IKeyboard> pKeyboard) {",
        "if (!m_xkbTranslationState) {",
        "updateXKBTranslationState();",
        "if (!m_xkbTranslationState)",
        "return true;",
        "const auto KEYCODE = e.keycode + 8; // Because to xkbcommon it's +8 from libinput",
        "const xkb_keysym_t keysym = xkb_state_key_get_one_sym(pKeyboard->m_resolveBindsBySym ? pKeyboard->m_xkbSymState : m_xkbTranslationState, KEYCODE);",
        "const xkb_keysym_t internalKeysym = xkb_state_key_get_one_sym(pKeyboard->m_xkbState, KEYCODE);",
    )
    raw_keycode_requirements = (
        "} else if (k->keycode != 0) {",
        "if (key.keycode != k->keycode)",
        "continue;",
    )
    registration_path = Path(
        "src/config/lua/bindings/LuaBindingsRegistration.cpp"
    )
    toplevel_path = Path("src/config/lua/bindings/LuaBindingsToplevel.cpp")
    action_header_path = Path("src/config/shared/actions/ConfigActions.hpp")
    action_path = Path("src/config/shared/actions/ConfigActions.cpp")
    keybind_path = Path("src/managers/KeybindManager.cpp")
    hyprctl_path = Path("src/debug/HyprCtl.cpp")

    return {
        "0.55.0": {
            Path("src/config/lua/ConfigManager.cpp"): reload_requirements,
            registration_path: registration_requirements,
            toplevel_path: (
                "static int hlBind(lua_State* L) {",
                "kb.submap.name = mgr->m_currentSubmap;",
                "kb.submap.reset = mgr->m_currentSubmapReset;",
                "int ref = luaL_ref(L, LUA_REGISTRYINDEX);",
                'kb.handler = "__lua";',
                "kb.arg = std::to_string(ref);",
                "const auto BIND = g_pKeybindManager->addKeybind(kb);",
                "static int hlDefineSubmap(lua_State* L) {",
                "std::string prev = mgr->m_currentSubmap;",
                "std::string prevReset = mgr->m_currentSubmapReset;",
                "mgr->m_currentSubmap = name;",
                "mgr->m_currentSubmapReset = reset;",
                "lua_pushvalue(L, fnIdx);",
                'if (mgr->guardedPCall(0, 0, 0, CConfigManager::LUA_TIMEOUT_DISPATCH_MS, std::format("hl.define_submap(\\\"{}\\\")", name)) != LUA_OK) {',
                "mgr->m_currentSubmap = prev;",
                "mgr->m_currentSubmapReset = prevReset;",
            ),
            action_header_path: action_header_requirements,
            action_path: action_requirements,
            keybind_path: (
                *symbol_resolution_requirements,
                "SSubmap CKeybindManager::getCurrentSubmap() {",
                "return SSubmap{.name = Config::Actions::state()->m_currentSubmap};",
                "if (!IGNORECONDITIONS && ((modmask != k->modmask && !k->ignoreMods) || (k->submap.name != Config::Actions::state()->m_currentSubmap && !k->submapUniversal) || k->shadowed))",
                *raw_keycode_requirements,
                "if (found || key.submapAtPress.name != Config::Actions::state()->m_currentSubmap)",
                "void CKeybindManager::clearKeybinds() { m_keybinds.clear(); }",
            ),
            hyprctl_path: (
                "static std::string bindsRequest(eHyprCtlOutputFormat format, std::string request) {",
                '"dispatcher": "{}", "arg": "{}"',
                "escapeJSONStrings(kb->handler), escapeJSONStrings(kb->arg));",
                "std::string versionRequest(",
                'registerCommand(SHyprCtlCommand{"binds", true, bindsRequest});',
            ),
        },
        "0.56.1": {
            Path("src/config/lua/ConfigManager.cpp"): reload_requirements,
            registration_path: registration_requirements,
            toplevel_path: (
                "static int hlBind(lua_State* L) {",
                "kb.submap.name = mgr->m_currentSubmap;",
                "kb.submap.reset = mgr->m_currentSubmapReset;",
                "int ref = luaL_ref(L, LUA_REGISTRYINDEX);",
                'kb.handler = "__lua";',
                "kb.arg = std::to_string(ref);",
                "const auto BIND = g_pKeybindManager->addKeybind(kb);",
                "static int hlDefineSubmap(lua_State* L) {",
                "std::string prev = mgr->m_currentSubmap;",
                "std::string prevReset = mgr->m_currentSubmapReset;",
                "mgr->m_currentSubmap = *name;",
                "mgr->m_currentSubmapReset = reset;",
                "lua_pushvalue(L, fnIdx);",
                'if (mgr->guardedPCall(0, 0, 0, CConfigManager::LUA_TIMEOUT_DISPATCH_MS, std::format("hl.define_submap(\\\"{}\\\")", *name)) != LUA_OK) {',
                "mgr->m_currentSubmap = prev;",
                "mgr->m_currentSubmapReset = prevReset;",
            ),
            action_header_path: action_header_requirements,
            action_path: action_requirements,
            keybind_path: (
                *symbol_resolution_requirements,
                "SSubmap CKeybindManager::getCurrentSubmap() {",
                "return SSubmap{.name = Config::Actions::state()->m_currentSubmap};",
                "if (!IGNORECONDITIONS && ((modmask != k->modmask && !k->ignoreMods) || ((k->submap.name != key.submapAtPress.name) && !k->submapUniversal) || k->shadowed))",
                *raw_keycode_requirements,
                "if (found || key.submapAtPress.name != Config::Actions::state()->m_currentSubmap)",
                "void CKeybindManager::clearKeybinds() { m_keybinds.clear(); }",
            ),
            hyprctl_path: (
                "static std::string bindsRequest(eHyprCtlOutputFormat format, std::string request) {",
                '"dispatcher": "{}", "arg": "{}"',
                "escapeJSONStrings(kb->handler), escapeJSONStrings(kb->arg));",
                "std::string versionRequest(",
                'registerCommand(SHyprCtlCommand{"binds", true, bindsRequest});',
            ),
        },
    }


def _assert_binding_runtime_contract(
    binding_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify the reload and process-lifetime binding/submap boundary."""
    requirements_by_version = _binding_runtime_contract_requirements()
    if tuple(requirements_by_version) != tuple(BINDING_RUNTIME_SOURCE_PATHS):
        raise ValueError("binding runtime patch inventory is incomplete")

    assignment = "Config::Actions::state()->m_currentSubmap ="
    for version, expected_paths in BINDING_RUNTIME_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} binding runtime semantic inventory is incomplete"
            )
        sources = binding_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} binding runtime source inventory is incomplete"
            )
        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(
                    re.sub(r"\s+", " ", fragment)
                    for fragment in requirements[path]
                ),
                "binding runtime",
            )

        config_source = sources[
            Path("src/config/lua/ConfigManager.cpp")
        ].decode("utf-8")
        if config_source.count("if (!phase1Load()) {") != 2:
            raise ValueError(
                f"Hyprland {version} binding reload no longer performs both guarded loads"
            )

        actions_source = sources[
            Path("src/config/shared/actions/ConfigActions.cpp")
        ].decode("utf-8")
        set_submap_start = actions_source.find(
            "ActionResult Actions::setSubmap(const std::string& submap) {"
        )
        set_submap_end = actions_source.find(
            "ActionResult Actions::cycleNext(", set_submap_start
        )
        if (
            set_submap_start < 0
            or set_submap_end < 0
            or actions_source.count(assignment) != 2
            or assignment in actions_source[:set_submap_start]
            or assignment in actions_source[set_submap_end:]
        ):
            raise ValueError(
                f"Hyprland {version} process-lifetime submap mutation escaped setSubmap"
            )

        keybind_source = sources[
            Path("src/managers/KeybindManager.cpp")
        ].decode("utf-8")
        symbol_choice = (
            "xkb_state_key_get_one_sym(pKeyboard->m_resolveBindsBySym ? "
            "pKeyboard->m_xkbSymState : m_xkbTranslationState, KEYCODE)"
        )
        if keybind_source.count(symbol_choice) != 1:
            raise ValueError(
                f"Hyprland {version} binding symbol-state choice changed"
            )
        if keybind_source.count(
            "m_xkbTranslationState = xkb_state_new(PKEYMAP);"
        ) != 1:
            raise ValueError(
                f"Hyprland {version} global binding translation state changed"
            )
        raw_keycode_match = (
            "else if (k->keycode != 0) { "
            "if (key.keycode != k->keycode)"
        )
        if re.sub(r"\s+", " ", keybind_source).count(raw_keycode_match) != 1:
            raise ValueError(
                f"Hyprland {version} raw-keycode binding seam changed"
            )
        clear_match = re.search(
            r"void\s+CKeybindManager::clearKeybinds\(\)\s*\{([^{}]*)\}",
            keybind_source,
            re.DOTALL,
        )
        if (
            clear_match is None
            or re.sub(r"\s+", " ", clear_match.group(1)).strip()
            != "m_keybinds.clear();"
        ):
            raise ValueError(
                f"Hyprland {version} clearKeybinds no longer clears only definitions"
            )

        hyprctl_source = sources[Path("src/debug/HyprCtl.cpp")].decode("utf-8")
        binds_start = hyprctl_source.find(
            "static std::string bindsRequest(eHyprCtlOutputFormat format, std::string request) {"
        )
        binds_end = hyprctl_source.find(
            "std::string versionRequest(", binds_start
        )
        if binds_start < 0 or binds_end < 0:
            raise ValueError(
                f"Hyprland {version} j/binds implementation boundary changed"
            )
        if '"callback"' in hyprctl_source[binds_start:binds_end]:
            raise ValueError(
                f"Hyprland {version} j/binds unexpectedly claims callback semantics"
            )


def _misc_exclusion_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    registry_requirements = (
        'MS<Bool>("misc:layers_hog_keyboard_focus", "If true, will make keyboard-interactive layers keep their focus on mouse move.", true),',
        'MS<Bool>("misc:animate_manual_resizes", "If true, will animate manual window resizes/moves", false),',
        'MS<Bool>("misc:animate_mouse_windowdragging", "If true, will animate windows being dragged by mouse.", false),',
    )
    dwindle_path = Path(
        "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"
    )
    window_target_path = Path("src/layout/target/WindowTarget.cpp")
    common_dwindle_requirements = (
        "void SDwindleNodeData::recalcSizePosRecursive(bool force, bool horizontalOverride, bool verticalOverride) {",
        "if (children[0]) {",
        "children[0]->recalcSizePosRecursive(force);",
        "children[1]->recalcSizePosRecursive(force);",
        "void CDwindleAlgorithm::newTarget(SP<ITarget> target) {",
        "void CDwindleAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {",
        'static auto PANIMATE = CConfigValue<Config::INTEGER>("misc:animate_manual_resizes");',
        "PHOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);",
        "PHINNER->pParent->recalcSizePosRecursive(*PANIMATE == 0);",
        "PHOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);",
        "PVOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);",
        "PVINNER->pParent->recalcSizePosRecursive(*PANIMATE == 0);",
        "PVOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);",
        "Hyprutils::Utils::CScopeGuard x([target, this] {",
        "if (target == g_layoutManager->dragController()->target()) {",
        "for (const auto& w : m_dwindleNodesData) {",
        "if (w->isNode)",
        "continue;",
        "w->pTarget->warpPositionSize();",
        "PPARENT->recalcSizePosRecursive(*PANIMATE == 0);",
        "PPARENT->recalcSizePosRecursive(*PANIMATE == 0);",
        "PPARENT->recalcSizePosRecursive(*PANIMATE == 0);",
        "PPARENT->recalcSizePosRecursive(*PANIMATE == 0);",
        "SIDECONTAINER->recalcSizePosRecursive(*PANIMATE == 0);",
        "TOPCONTAINER->recalcSizePosRecursive(*PANIMATE == 0);",
        "if (target == g_layoutManager->dragController()->target()) {",
        "for (const auto& w : m_dwindleNodesData) {",
        "if (w->isNode)",
        "continue;",
        "w->pTarget->warpPositionSize();",
        "SP<ITarget> CDwindleAlgorithm::getNextCandidate(SP<ITarget> old) {",
    )
    leaf_requirements = {
        "0.55.0": "} else pTarget->setPositionGlobal(box);",
        "0.56.1": "} else if (!pTarget.expired()) pTarget->setPositionGlobal(box);",
    }
    target_requirements = {
        "0.55.0": (
            "void CWindowTarget::warpPositionSize() { "
            "m_window->m_realSize->warp(); "
            "m_window->m_realPosition->warp(); "
            "m_window->updateWindowDecos(); }"
        ),
        "0.56.1": (
            "void CWindowTarget::warpPositionSize() { "
            "m_window->finishAnimation(); "
            "m_window->updateWindowDecos(); }"
        ),
    }
    return {
        version: {
            REGISTRY_PATH: registry_requirements,
            dwindle_path: (
                *common_dwindle_requirements[:4],
                leaf_requirements[version],
                *common_dwindle_requirements[4:],
            ),
            window_target_path: (target_requirements[version],),
        }
        for version in MISC_EXCLUSION_SOURCE_PATHS
    }


def _assert_misc_exclusion_contract(
    misc_sources: dict[str, dict[Path, bytes]],
    global_occurrences: dict[str, dict[str, dict[Path, int]]],
) -> None:
    """Qualify three writable values deliberately excluded from Settings."""
    requirements_by_version = _misc_exclusion_contract_requirements()
    if tuple(requirements_by_version) != tuple(MISC_EXCLUSION_SOURCE_PATHS):
        raise ValueError("misc exclusion patch inventory is incomplete")
    if global_occurrences != MISC_EXCLUSION_EXPECTED_OCCURRENCES:
        raise ValueError("misc exclusion global option occurrences changed")

    dwindle_path = Path(
        "src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"
    )
    window_target_path = Path("src/layout/target/WindowTarget.cpp")
    recalc_marker = (
        "void SDwindleNodeData::recalcSizePosRecursive(bool force, "
        "bool horizontalOverride, bool verticalOverride) {"
    )
    resize_marker = (
        "void CDwindleAlgorithm::resizeTarget(const Vector2D& Δ, "
        "SP<ITarget> target, eRectCorner corner) {"
    )
    for version, expected_paths in MISC_EXCLUSION_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} misc exclusion semantic inventory is incomplete"
            )
        sources = misc_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} misc exclusion source inventory is incomplete"
            )
        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(
                    re.sub(r"\s+", " ", fragment)
                    for fragment in requirements[path]
                ),
                "misc exclusion",
            )

        registry = sources[REGISTRY_PATH].decode("utf-8")
        for option_path in MISC_EXCLUSION_OPTION_PATHS:
            if registry.count(f'"{option_path}"') != 1:
                raise ValueError(
                    f"Hyprland {version} misc exclusion registry count changed for "
                    f"{option_path}"
                )

        dwindle = re.sub(r"\s+", " ", sources[dwindle_path].decode("utf-8"))
        recalc_start = dwindle.find(recalc_marker)
        recalc_end = dwindle.find(
            "void CDwindleAlgorithm::newTarget(SP<ITarget> target) {",
            recalc_start,
        )
        if recalc_start < 0 or recalc_end < 0:
            raise ValueError(
                f"Hyprland {version} Dwindle recursive resize boundary changed"
            )
        recalc = dwindle[recalc_start:recalc_end]
        if (
            recalc.count("force") != 3
            or recalc.count("recalcSizePosRecursive(force);") != 2
            or recalc.count("pTarget->setPositionGlobal(box);") != 1
            or "setPositionGlobal(box, force)" in recalc
        ):
            raise ValueError(
                f"Hyprland {version} Dwindle force reached or escaped a resize leaf"
            )

        resize_start = dwindle.find(resize_marker)
        resize_end = dwindle.find(
            "SP<ITarget> CDwindleAlgorithm::getNextCandidate(SP<ITarget> old) {",
            resize_start,
        )
        if resize_start < 0 or resize_end < 0:
            raise ValueError(
                f"Hyprland {version} Dwindle manual-resize boundary changed"
            )
        resize = dwindle[resize_start:resize_end]
        if (
            resize.count("recalcSizePosRecursive(*PANIMATE == 0);") != 12
            or resize.count("w->pTarget->warpPositionSize();") != 2
            or "if (*PANIMATE" in resize
        ):
            raise ValueError(
                f"Hyprland {version} manual resize forwarding or snap behavior changed"
            )

        target = re.sub(
            r"\s+", " ", sources[window_target_path].decode("utf-8")
        )
        expected_target = requirements[window_target_path][0]
        if target.count(expected_target) != 1:
            raise ValueError(
                f"Hyprland {version} WindowTarget unconditional finish behavior changed"
            )


def _input_behavior_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    registry = (
        'MS<Bool>("input:resolve_binds_by_sym", "Determines how keybinds act when multiple layouts are used.", false, {.refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Bool>("input:force_no_accel", "Force no cursor acceleration.", false, {.refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Int>("input:rotation", "Sets the rotation of a device in degrees clockwise. Value is clamped to the range 0 to 359.", 0,',
        '{.min = 0, .max = 359, .refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Float>("input:follow_mouse_threshold", "The smallest distance in logical pixels the mouse needs to travel for the window under it to get focused.", 0),',
        'MS<Int>("input:touchdevice:transform", "Transform the input from touchdevices.", 0, {.min = 0, .max = 6, .refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Bool>("input:touchdevice:enabled", "Whether input is enabled for touch devices.", true, {.refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Int>("input:tablet:transform", "transform the input from tablets.", 0, {.min = 0, .max = 6, .refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Vec2>("input:tablet:region_position", "position of the mapped region in monitor layout.", Config::VEC2{},',
        '{.validator = vec2Range(-20000, -20000, 20000, 20000), .refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Bool>("input:tablet:absolute_region_position", "whether to treat the region_position as an absolute position in monitor layout.", false,',
        '{.refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Vec2>("input:tablet:region_size", "size of the mapped region.", Config::VEC2{},',
        '{.validator = vec2Range(-100, -100, 4000, 4000), .refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Bool>("input:tablet:relative_input", "whether the input should be relative", false, {.refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Bool>("input:tablet:left_handed", "if enabled, the tablet will be rotated 180 degrees", false, {.refresh = Supplementary::REFRESH_INPUT_DEVICES}),',
        'MS<Bool>("misc:middle_click_paste", "whether to enable middle-click-paste (aka primary selection)", true),',
        'MS<Int>("cursor:hotspot_padding", "the padding, in logical px, between screen edges and the cursor", 0, {.min = 0, .max = 20}),',
        'MS<Float>("cursor:inactive_timeout", "in seconds, after how many seconds of cursor\'s inactivity to hide it. Set to 0 for never.", 0, {.min = 0, .max = 20}),',
        'MS<Bool>("cursor:no_warps", "if true, will not warp the cursor in many cases", false),',
        'MS<Bool>("cursor:persistent_warps", "When a window is refocused, the cursor returns to its last position relative to that window.", false),',
        'MS<Bool>("cursor:hide_on_key_press", "Hides the cursor when you press any key until the mouse is moved.", false),',
        'MS<Bool>("cursor:hide_on_touch", "Hides the cursor when the last input was a touch input until a mouse input is done.", true),',
        'MS<Bool>("cursor:hide_on_tablet", "Hides the cursor when the last input was a tablet input until a mouse input is done.", false),',
        'MS<Bool>("cursor:warp_back_after_non_mouse_input", "warp the cursor back to where it was after using a non-mouse input to move it.", false),',
    )
    reload_reset = (
        "m_deviceConfigs.clear();",
        "reinitLuaState();",
        "for (const auto& v : m_configValues) {",
        "v.second->reset();",
    )
    device_fallback = (
        'if (guardedPCall(0, 0, 1, LUA_TIMEOUT_CONFIG_RELOAD_MS, "config reload") != LUA_OK) {',
        "ILuaConfigValue* CConfigManager::findDeviceValue(const std::string& dev, const std::string& field) {",
        "int CConfigManager::getDeviceInt(const std::string& dev, const std::string& field, const std::string& fb) {",
        "if (auto* v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field)); v && v->setByUser())",
        "return v->asInt();",
        "if (!fallback.empty() && m_configValues.contains(fallback))",
        "return m_configValues.at(fallback)->asInt();",
        "Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& field, const std::string& fb) {",
        "std::string fallback = luaConfigValueName(fb);",
        "auto toVec = [](const Config::VEC2& v) -> Vector2D { return {v.x, v.y}; };",
        "if (auto* v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field)); v && v->setByUser())",
        "return toVec(v->asVec2());",
        "if (!fallback.empty() && m_configValues.contains(fallback))",
        "return toVec(m_configValues.at(fallback)->asVec2());",
    )
    device_binding = (
        '"rotation", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 359); }},',
        '"resolve_binds_by_sym", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},',
        '"left_handed", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},',
        '"transform", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); }},',
        '"enabled", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},',
        '"region_position", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},',
        '"absolute_region_position", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},',
        '"region_size", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},',
        '"relative_input", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},',
        "auto val = UP<ILuaConfigValue>(desc->factory());",
        "auto err = val->parse(L);",
        "self->m_deviceConfigs[devName].values.insert_or_assign(key, std::move(val));",
        "Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_INPUT_DEVICES);",
    )
    input_refresh = (
        "if (m_propsTripped & REFRESH_INPUT_DEVICES) {",
        "g_pInputManager->setKeyboardLayout();",
        "g_pInputManager->setPointerConfigs();",
        "g_pInputManager->setTouchDeviceConfigs();",
        "g_pInputManager->setTabletConfigs();",
    )
    keyboard_resolution = (
        "void CInputManager::setKeyboardLayout() {",
        "for (auto const& k : m_keyboards)",
        "applyConfigToKeyboard(k);",
        "g_pKeybindManager->updateXKBTranslationState();",
        "void CInputManager::applyConfigToKeyboard(SP<IKeyboard> pKeyboard) {",
        'const auto RESOLVEBINDSBYSYM = Config::mgr()->getDeviceInt(devname, "resolve_binds_by_sym", "input:resolve_binds_by_sym");',
        "pKeyboard->m_resolveBindsBySym = RESOLVEBINDSBYSYM;",
        "try {",
        "if (NUMLOCKON == pKeyboard->m_numlockOn && REPEATDELAY == pKeyboard->m_repeatDelay && REPEATRATE == pKeyboard->m_repeatRate && RULES == pKeyboard->m_currentRules.rules &&",
        "OPTIONS == pKeyboard->m_currentRules.options && FILEPATH == pKeyboard->m_xkbFilePath) {",
        'Log::logger->log(Log::DEBUG, "Not applying config to keyboard, it did not change.");',
        "return;",
    )
    transform_matrices = (
        "// The third row is always 0 0 1 and is not expected by `libinput_device_config_calibration_set_matrix`",
        "static const float MATRICES[8][6] = {{// normal 1, 0, 0, 0, 1, 0},",
        "{// rotation 90° 0, -1, 1, 1, 0, 0},",
        "{// rotation 180° -1, 0, 1, 0, -1, 1},",
        "{// rotation 270° 0, 1, 0, -1, 0, 1},",
        "{// flipped -1, 0, 1, 0, 1, 0},",
        "{// flipped + rotation 90° 0, 1, 0, 1, 0, 0},",
        "{// flipped + rotation 180° 1, 0, 0, 0, -1, 1},",
        "{// flipped + rotation 270° 0, -1, 1, -1, 0, 1}};",
    )
    retained_input_state = (
        "CTimer m_lastCursorMovement;",
        "bool m_lastInputTouch = false;",
        "bool m_lastInputTablet = false;",
        "void mouseMoveUnified(uint32_t, bool refocus = false, bool mouse = false, std::optional<Vector2D> overridePos = std::nullopt);",
        "void recheckMouseWarpOnMouseInput();",
        "Vector2D m_lastMousePos = {};",
        "double m_mousePosDelta = 0;",
        "bool m_lastInputMouse = true;",
    )
    pointer_common = (
        "void CInputManager::onMouseMoved(IPointer::SMotionEvent e) {",
        'static auto PNOACCEL = CConfigValue<Config::INTEGER>("input:force_no_accel");',
        "Vector2D delta = e.delta;",
        "Vector2D unaccel = e.unaccel;",
        "const auto DELTA = *PNOACCEL == 1 ? unaccel : delta; if (e.mouse) recheckMouseWarpOnMouseInput();",
        "PROTO::relativePointer->sendRelativeMotion(sc<uint64_t>(e.timeMs) * 1000, delta, unaccel);",
    )
    pointer_motion_state = (
        "mouseMoveUnified(e.timeMs, false, e.mouse);",
        "m_lastCursorMovement.reset(); m_lastInputTouch = false; m_lastInputTablet = false;",
        "if (e.mouse) m_lastMousePos = getMouseCoordsInternal();",
        "void CInputManager::mouseMoveUnified(uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos) {",
        "m_lastInputMouse = mouse;",
        'static auto PFOLLOWMOUSETHRESHOLD = CConfigValue<Config::FLOAT>("input:follow_mouse_threshold");',
        "const auto FOLLOWMOUSE = *PFOLLOWONDND && PROTO::data->dndActive() ? 1 : *PFOLLOWMOUSE;",
        "if (FOLLOWMOUSE == 1 && m_lastCursorMovement.getSeconds() < 0.5)",
        "m_mousePosDelta += MOUSECOORDSFLOORED.distance(m_lastCursorPosFloored);",
        "else m_mousePosDelta = 0;",
        "Event::bus()->m_events.input.mouse.move.emit(MOUSECOORDSFLOORED, info);",
        "if (m_mousePosDelta > *PFOLLOWMOUSETHRESHOLD || refocus) {",
        "const bool hasNoFollowMouse = pFoundWindow && pFoundWindow->m_ruleApplicator->noFollowMouse().valueOrDefault();",
        "if (refocus || !hasNoFollowMouse)",
        "Desktop::focusState()->rawWindowFocus(pFoundWindow, FOCUS_REASON, foundSurface);",
    )
    pointer_buttons_055 = (
        "void CInputManager::onMouseButton(IPointer::SButtonEvent e, SP<IPointer> mouse) { Event::SCallbackInfo info; Event::bus()->m_events.input.mouse.button.emit(e, info);",
        "if (e.mouse) recheckMouseWarpOnMouseInput(); m_lastCursorMovement.reset(); if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {",
        "void CInputManager::onMouseWheel(IPointer::SAxisEvent e, SP<IPointer> pointer) {",
        "Event::bus()->m_events.input.mouse.axis.emit(e, info); if (info.cancelled) return; if (e.mouse) recheckMouseWarpOnMouseInput(); bool passEvent = g_pKeybindManager->onAxisEvent(e, pointer);",
    )
    pointer_buttons_056 = (
        "void CInputManager::onMouseButton(IPointer::SButtonEvent e, SP<IPointer> mouse) { Event::SCallbackInfo info; Event::bus()->m_events.input.mouse.button.emit(e, info);",
        "if (e.mouse) recheckMouseWarpOnMouseInput(); PROTO::inputCapture->button(e.button, e.state); if (PROTO::inputCapture->isCaptured()) return; m_lastCursorMovement.reset(); if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {",
        "void CInputManager::onMouseWheel(IPointer::SAxisEvent e, SP<IPointer> pointer) {",
        "Event::bus()->m_events.input.mouse.axis.emit(e, info); if (info.cancelled) return; if (e.mouse) recheckMouseWarpOnMouseInput(); PROTO::inputCapture->axis(e.axis, e.delta);",
        "bool passEvent = !PROTO::inputCapture->isCaptured() && g_pKeybindManager->onAxisEvent(e, pointer);",
    )
    warp_back_055 = (
        "void CInputManager::recheckMouseWarpOnMouseInput() {",
        'static auto PWARPFORNONMOUSE = CConfigValue<Config::INTEGER>("cursor:warp_back_after_non_mouse_input");',
        "if (!m_lastInputMouse && *PWARPFORNONMOUSE) g_pPointerManager->warpTo(m_lastMousePos);",
    )
    warp_back_056 = (
        "void CInputManager::recheckMouseWarpOnMouseInput() {",
        'static auto PWARPFORNONMOUSE = CConfigValue<Config::INTEGER>("cursor:warp_back_after_non_mouse_input");',
        "if (!m_lastInputMouse && *PWARPFORNONMOUSE) Pointer::mgr()->warpTo(m_lastMousePos);",
    )
    pointer_configuration = (
        "void CInputManager::setPointerConfigs() {",
        'const auto LIBINPUTSENS = std::clamp(Config::mgr()->getDeviceFloat(devname, "sensitivity", "input:sensitivity"), -1.f, 1.f);',
        "libinput_device_config_accel_set_speed(LIBINPUTDEV, LIBINPUTSENS);",
        "if (libinput_device_config_rotation_is_available(LIBINPUTDEV)) {",
        'const auto ROTATION = std::clamp(Config::mgr()->getDeviceInt(devname, "rotation", "input:rotation"), 0, 359);',
        "libinput_device_config_rotation_set_angle(LIBINPUTDEV, ROTATION);",
        'const auto ACCELPROFILE = Config::mgr()->getDeviceString(devname, "accel_profile", "input:accel_profile");',
    )
    touch_and_tablet_configuration = (
        "void CInputManager::setTouchDeviceConfigs(SP<ITouch> dev) {",
        "if (PTOUCHDEV->aq() && PTOUCHDEV->aq()->getLibinputHandle()) {",
        "const auto LIBINPUTDEV = PTOUCHDEV->aq()->getLibinputHandle();",
        'const auto ENABLED = Config::mgr()->getDeviceInt(PTOUCHDEV->m_hlName, "enabled", "input:touchdevice:enabled");',
        "const auto mode = ENABLED ? LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;",
        "if (libinput_device_config_send_events_get_mode(LIBINPUTDEV) != mode)",
        "libinput_device_config_send_events_set_mode(LIBINPUTDEV, mode);",
        "if (libinput_device_config_calibration_has_matrix(LIBINPUTDEV)) {",
        'const int ROTATION = std::clamp(Config::mgr()->getDeviceInt(PTOUCHDEV->m_hlName, "transform", "input:touchdevice:transform"), -1, 7);',
        "if (ROTATION > -1)",
        "libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);",
        "void CInputManager::setTabletConfigs() {",
        "for (auto const& t : m_tablets) {",
        "if (t->aq()->getLibinputHandle()) {",
        "const auto LIBINPUTDEV = t->aq()->getLibinputHandle();",
        'const auto RELINPUT = Config::mgr()->getDeviceInt(NAME, "relative_input", "input:tablet:relative_input");',
        "t->m_relativeInput = RELINPUT;",
        'const int ROTATION = std::clamp(Config::mgr()->getDeviceInt(NAME, "transform", "input:tablet:transform"), -1, 7);',
        "if (ROTATION > -1)",
        "libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);",
        'if (Config::mgr()->getDeviceInt(NAME, "left_handed", "input:tablet:left_handed") == 0)',
        "libinput_device_config_left_handed_set(LIBINPUTDEV, 0);",
        "else",
        "libinput_device_config_left_handed_set(LIBINPUTDEV, 1);",
        'const auto OUTPUT = Config::mgr()->getDeviceString(NAME, "output", "input:tablet:output");',
        "if (OUTPUT != STRVAL_EMPTY) {",
        "t->m_boundOutput = OUTPUT;",
        "} else t->m_boundOutput = \"\";",
        'const auto REGION_POS = Config::mgr()->getDeviceVec(NAME, "region_position", "input:tablet:region_position");',
        'const auto REGION_SIZE = Config::mgr()->getDeviceVec(NAME, "region_size", "input:tablet:region_size");',
        "t->m_boundBox = {REGION_POS, REGION_SIZE};",
        'const auto ABSOLUTE_REGION_POS = Config::mgr()->getDeviceInt(NAME, "absolute_region_position", "input:tablet:absolute_region_position");',
        "t->m_absolutePos = ABSOLUTE_REGION_POS;",
    )
    tablet_motion_055 = (
        "void CInputManager::onTabletAxis(CTablet::SAxisEvent e) {",
        "case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_MOUSE: { g_pPointerManager->move(delta); break; }",
        "if (PTAB->m_relativeInput) g_pPointerManager->move(delta); else g_pPointerManager->warpAbsolute(transformToActiveRegion({x, y}, PTAB->m_activeArea), PTAB);",
        "m_lastInputTouch = false;",
        "if (!PTOOL->m_isDown || PROTO::data->dndActive()) simulateMouseMovement();",
        "refocusTablet(PTAB, PTOOL, true);",
        "m_lastCursorMovement.reset();",
        "void CInputManager::onTabletTip(CTablet::STipEvent e) {",
        "if (PTAB->m_relativeInput) g_pPointerManager->move({0, 0}); else g_pPointerManager->warpAbsolute(transformToActiveRegion(POS, PTAB->m_activeArea), PTAB);",
        "void CInputManager::onTabletProximity(CTablet::SProximityEvent e) {",
        "if (!e.in) { m_lastInputTablet = false;",
        "} else { m_lastInputTablet = true; simulateMouseMovement();",
    )
    tablet_motion_056 = (
        "void CInputManager::onTabletAxis(CTablet::SAxisEvent e) {",
        "case Aquamarine::ITabletTool::AQ_TABLET_TOOL_TYPE_MOUSE: { Pointer::mgr()->move(delta); break; }",
        "if (PTAB->m_relativeInput) Pointer::mgr()->move(delta); else Pointer::mgr()->warpAbsolute(transformToActiveRegion({x, y}, PTAB->m_activeArea), PTAB);",
        "m_lastInputTouch = false;",
        "if (!PTOOL->m_isDown || PROTO::data->dndActive()) simulateMouseMovement();",
        "refocusTablet(PTAB, PTOOL, true);",
        "m_lastCursorMovement.reset();",
        "void CInputManager::onTabletTip(CTablet::STipEvent e) {",
        "if (PTAB->m_relativeInput) Pointer::mgr()->move({0, 0}); else Pointer::mgr()->warpAbsolute(transformToActiveRegion(POS, PTAB->m_activeArea), PTAB);",
        "void CInputManager::onTabletProximity(CTablet::SProximityEvent e) {",
        "if (!e.in) { m_lastInputTablet = false;",
        "} else { m_lastInputTablet = true; simulateMouseMovement();",
    )
    primary_selection = (
        "CPrimarySelectionDevice::CPrimarySelectionDevice(SP<CZwpPrimarySelectionDeviceV1> resource_) : m_resource(resource_) {",
        "m_resource->setSetSelection([](CZwpPrimarySelectionDeviceV1* r, wl_resource* sourceR, uint32_t serial) {",
        'static auto PPRIMARYSEL = CConfigValue<Config::INTEGER>("misc:middle_click_paste");',
        "if (!*PPRIMARYSEL) {",
        'LOGM(Log::DEBUG, "Ignoring primary selection: disabled in config");',
        "g_pSeatManager->setCurrentPrimarySelection(nullptr);",
        "return;",
        "auto source = sourceR ? CPrimarySelectionSource::fromResource(sourceR) : CSharedPointer<CPrimarySelectionSource>{};",
        "source->markUsed();",
        "g_pSeatManager->setCurrentPrimarySelection(source);",
    )
    renderer_constructor = (
        "static int cursorTicker(void* data) { g_pHyprRenderer->ensureCursorRenderingMode(); wl_event_source_timer_update(g_pHyprRenderer->m_cursorTicker, 500); return 0; }",
        "static auto P = Event::bus()->m_events.input.keyboard.key.listen([&](IKeyboard::SKeyEvent e, Event::SCallbackInfo&) { if (m_cursorHiddenConditions.hiddenOnKeyboard) return; m_cursorHiddenConditions.hiddenOnKeyboard = true; ensureCursorRenderingMode(); });",
        "static auto P2 = Event::bus()->m_events.input.mouse.move.listen([&](Vector2D pos, Event::SCallbackInfo&) {",
        "if (!m_cursorHiddenConditions.hiddenOnKeyboard && m_cursorHiddenConditions.hiddenOnTouch == g_pInputManager->m_lastInputTouch && m_cursorHiddenConditions.hiddenOnTablet == g_pInputManager->m_lastInputTablet && !m_cursorHiddenConditions.hiddenOnTimeout) return;",
        "m_cursorHiddenConditions.hiddenOnKeyboard = false; m_cursorHiddenConditions.hiddenOnTimeout = false; m_cursorHiddenConditions.hiddenOnTouch = g_pInputManager->m_lastInputTouch; m_cursorHiddenConditions.hiddenOnTablet = g_pInputManager->m_lastInputTablet; ensureCursorRenderingMode();",
        "m_cursorTicker = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, cursorTicker, nullptr); wl_event_source_timer_update(m_cursorTicker, 500);",
    )
    renderer_visibility = (
        "void IHyprRenderer::ensureCursorRenderingMode() {",
        'static auto PINVISIBLE = CConfigValue<Config::INTEGER>("cursor:invisible");',
        'static auto PCURSORTIMEOUT = CConfigValue<Config::FLOAT>("cursor:inactive_timeout");',
        'static auto PHIDEONTOUCH = CConfigValue<Config::INTEGER>("cursor:hide_on_touch");',
        'static auto PHIDEONTABLET = CConfigValue<Config::INTEGER>("cursor:hide_on_tablet");',
        'static auto PHIDEONKEY = CConfigValue<Config::INTEGER>("cursor:hide_on_key_press");',
        "if (*PCURSORTIMEOUT <= 0) m_cursorHiddenConditions.hiddenOnTimeout = false;",
        "if (*PHIDEONTOUCH == 0) m_cursorHiddenConditions.hiddenOnTouch = false;",
        "if (*PHIDEONTABLET == 0) m_cursorHiddenConditions.hiddenOnTablet = false;",
        "if (*PHIDEONKEY == 0) m_cursorHiddenConditions.hiddenOnKeyboard = false;",
        "if (*PCURSORTIMEOUT > 0) m_cursorHiddenConditions.hiddenOnTimeout = *PCURSORTIMEOUT < g_pInputManager->m_lastCursorMovement.getSeconds();",
        "m_cursorHiddenByCondition = m_cursorHiddenConditions.hiddenOnTimeout || m_cursorHiddenConditions.hiddenOnTouch || m_cursorHiddenConditions.hiddenOnTablet || m_cursorHiddenConditions.hiddenOnKeyboard;",
    )
    renderer_header = (
        "void setCursorSurface(SP<Desktop::View::CWLSurface> surf, int hotspotX, int hotspotY, bool force = false);",
        "void setCursorFromName(const std::string& name, bool force = false);",
        "bool m_cursorHidden = false; bool m_cursorHiddenByCondition = false;",
        "bool hiddenOnTouch = false; bool hiddenOnTablet = false; bool hiddenOnTimeout = false; bool hiddenOnKeyboard = false;",
    )
    touch_events = (
        "void CInputManager::onTouchDown(ITouch::SDownEvent e) { m_lastInputTouch = true;",
        "void CInputManager::onTouchUp(ITouch::SUpEvent e) { m_lastInputTouch = true;",
        "void CInputManager::onTouchMove(ITouch::SMotionEvent e) { m_lastInputTouch = true; m_lastCursorMovement.reset();",
    )
    mouse_events = (
        "m_listeners.motion = m_mouse->events.move.listen([this](const Aquamarine::IPointer::SMoveEvent& event) { m_pointerEvents.motion.emit(SMotionEvent{ .timeMs = event.timeMs, .delta = event.delta, .unaccel = event.unaccel, .mouse = true, .device = m_self.lock(),",
        "m_listeners.button = m_mouse->events.button.listen([this](const Aquamarine::IPointer::SButtonEvent& event) { m_pointerEvents.button.emit(SButtonEvent{ .timeMs = event.timeMs, .button = event.button, .state = event.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED, .mouse = true,",
        "m_listeners.axis = m_mouse->events.axis.listen([this](const Aquamarine::IPointer::SAxisEvent& event) { m_pointerEvents.axis.emit(SAxisEvent{ .timeMs = event.timeMs, .source = sc<wl_pointer_axis_source>(event.source), .axis = sc<wl_pointer_axis>(event.axis), .relativeDirection = sc<wl_pointer_axis_relative_direction>(event.direction), .delta = event.delta, .deltaDiscrete = event.discrete, .mouse = true,",
    )
    pointer_manager_header = (
        "void warpTo(const Vector2D& logical);",
        "void move(const Vector2D& deltaLogical);",
        "Vector2D closestValid(const Vector2D& pos);",
        "std::vector<CBox> monitorBoxes;",
    )
    pointer_clamp = (
        "Vector2D CPointerManager::closestValid(const Vector2D& pos) {",
        'static auto PADDING = CConfigValue<Config::INTEGER>("cursor:hotspot_padding");',
        "auto CURSOR_PADDING = std::clamp(sc<int>(*PADDING), 0, 100);",
        "CBox hotBox = {{pos.x - CURSOR_PADDING, pos.y - CURSOR_PADDING}, {2 * CURSOR_PADDING, 2 * CURSOR_PADDING}};",
        "static auto INSIDE_LAYOUT = [this](const CBox& box) -> bool { for (auto const& b : m_currentMonitorLayout.monitorBoxes) { if (box.inside(b)) return true;",
        "static auto INSIDE_LAYOUT_COORD = [this](const Vector2D& vec) -> bool { for (auto const& b : m_currentMonitorLayout.monitorBoxes) { if (b.containsPoint(vec)) return true;",
        "static auto NEAREST_LAYOUT = [this](const Vector2D& vec) -> Vector2D { Vector2D leader; float distanceSq = __FLT_MAX__;",
        "for (auto const& b : m_currentMonitorLayout.monitorBoxes) { auto p = b.closestPoint(vec); auto distSq = p.distanceSq(vec);",
        "if (distSq < distanceSq) { leader = p; distanceSq = distSq;",
        "if (INSIDE_LAYOUT(hotBox)) return pos;",
        "Vector2D leader = NEAREST_LAYOUT(pos); hotBox.x = leader.x - CURSOR_PADDING; hotBox.y = leader.y - CURSOR_PADDING;",
        "if (!INSIDE_LAYOUT_COORD(hotBox.middle() + Vector2D{CURSOR_PADDING, CURSOR_PADDING})) { auto delta = NEAREST_LAYOUT(hotBox.middle() + Vector2D{CURSOR_PADDING, CURSOR_PADDING}) - (hotBox.middle() + Vector2D{CURSOR_PADDING, CURSOR_PADDING}); hotBox.translate(delta);",
        "if (!INSIDE_LAYOUT_COORD(hotBox.middle() - Vector2D{CURSOR_PADDING, CURSOR_PADDING})) { auto delta = NEAREST_LAYOUT(hotBox.middle() - Vector2D{CURSOR_PADDING, CURSOR_PADDING}) - (hotBox.middle() - Vector2D{CURSOR_PADDING, CURSOR_PADDING}); hotBox.translate(delta);",
        "if (!INSIDE_LAYOUT_COORD(hotBox.middle() + Vector2D{CURSOR_PADDING, -CURSOR_PADDING})) { auto delta = NEAREST_LAYOUT(hotBox.middle() + Vector2D{CURSOR_PADDING, -CURSOR_PADDING}) - (hotBox.middle() + Vector2D{CURSOR_PADDING, -CURSOR_PADDING}); hotBox.translate(delta);",
        "if (!INSIDE_LAYOUT_COORD(hotBox.middle() + Vector2D{-CURSOR_PADDING, CURSOR_PADDING})) { auto delta = NEAREST_LAYOUT(hotBox.middle() + Vector2D{-CURSOR_PADDING, CURSOR_PADDING}) - (hotBox.middle() + Vector2D{-CURSOR_PADDING, CURSOR_PADDING}); hotBox.translate(delta);",
        "return hotBox.middle();",
    )
    pointer_routing_055 = (
        "void CPointerManager::warpTo(const Vector2D& logical) { damageIfSoftware(); m_pointerPos = closestValid(logical);",
        "void CPointerManager::move(const Vector2D& deltaLogical) { const auto oldPos = m_pointerPos; auto newPos = oldPos + Vector2D{std::isnan(deltaLogical.x) ? 0.0 : deltaLogical.x, std::isnan(deltaLogical.y) ? 0.0 : deltaLogical.y}; warpTo(newPos);",
        "void CPointerManager::warpAbsolute(Vector2D abs, SP<IHID> dev) { if (!dev) return;",
        "const auto& MONITORS = g_pCompositor->m_monitors;",
        "Vector2D topLeft = MONITORS.at(0)->m_position, bottomRight = MONITORS.at(0)->m_position + MONITORS.at(0)->m_size;",
        "for (size_t i = 1; i < MONITORS.size(); ++i) {",
        "const auto EXTENT = MONITORS[i]->logicalBox().extent();",
        "const auto POS = MONITORS[i]->logicalBox().pos();",
        "if (EXTENT.x > bottomRight.x) bottomRight.x = EXTENT.x; if (EXTENT.y > bottomRight.y) bottomRight.y = EXTENT.y; if (POS.x < topLeft.x) topLeft.x = POS.x; if (POS.y < topLeft.y) topLeft.y = POS.y;",
        "CBox mappedArea = {topLeft, bottomRight - topLeft};",
        "auto outputMappedArea = [&mappedArea](const std::string& output) {",
        'if (output == "current") {',
        "if (const auto PLASTMONITOR = Desktop::focusState()->monitor(); PLASTMONITOR) return PLASTMONITOR->logicalBox();",
        "} else if (const auto PMONITOR = g_pCompositor->getMonitorFromString(output); PMONITOR) return PMONITOR->logicalBox();",
        "case HID_TYPE_TABLET: { CTablet* TAB = rc<CTablet*>(dev.get());",
        "if (!TAB->m_boundOutput.empty()) { mappedArea = outputMappedArea(TAB->m_boundOutput); mappedArea.translate(TAB->m_boundBox.pos());",
        "} else if (TAB->m_absolutePos) { mappedArea.x = TAB->m_boundBox.x; mappedArea.y = TAB->m_boundBox.y;",
        "} else mappedArea.translate(TAB->m_boundBox.pos());",
        "if (!TAB->m_boundBox.empty()) { mappedArea.w = TAB->m_boundBox.w; mappedArea.h = TAB->m_boundBox.h;",
        "m_pointerPos = mappedArea.pos() + mappedArea.size() * abs;",
        "void CPointerManager::onMonitorLayoutChange() { m_currentMonitorLayout.monitorBoxes.clear(); for (auto const& m : g_pCompositor->m_monitors) { if (m->isMirror() || !m->m_enabled || !m->m_output) continue; m_currentMonitorLayout.monitorBoxes.emplace_back(m->m_position, m->m_size);",
        "damageIfSoftware(); m_pointerPos = closestValid(m_pointerPos); updateCursorBackend(); recheckEnteredOutputs();",
    )
    pointer_routing_056 = (
        "void CPointerManager::warpTo(const Vector2D& logical) { damageIfSoftware(); m_pointerPos = closestValid(logical);",
        "void CPointerManager::move(const Vector2D& deltaLogical) { const auto oldPos = m_pointerPos; auto newPos = oldPos + Vector2D{std::isnan(deltaLogical.x) ? 0.0 : deltaLogical.x, std::isnan(deltaLogical.y) ? 0.0 : deltaLogical.y};",
        "if (!g_pInputManager->isLocked()) PROTO::inputCapture->motion(newPos, deltaLogical); if (PROTO::inputCapture->isCaptured()) return; warpTo(newPos);",
        "void CPointerManager::warpAbsolute(Vector2D abs, SP<IHID> dev) { if (!dev || State::monitorState()->monitors().empty()) return;",
        "const auto& MONITORS = State::monitorState()->monitors();",
        "Vector2D topLeft = MONITORS.at(0)->m_position, bottomRight = MONITORS.at(0)->m_position + MONITORS.at(0)->m_size;",
        "for (size_t i = 1; i < MONITORS.size(); ++i) {",
        "const auto EXTENT = MONITORS[i]->logicalBox().extent();",
        "const auto POS = MONITORS[i]->logicalBox().pos();",
        "if (EXTENT.x > bottomRight.x) bottomRight.x = EXTENT.x; if (EXTENT.y > bottomRight.y) bottomRight.y = EXTENT.y; if (POS.x < topLeft.x) topLeft.x = POS.x; if (POS.y < topLeft.y) topLeft.y = POS.y;",
        "CBox mappedArea = {topLeft, bottomRight - topLeft};",
        "auto outputMappedArea = [&mappedArea](const std::string& output) {",
        'if (output == "current") {',
        "if (const auto PLASTMONITOR = Desktop::focusState()->monitor(); PLASTMONITOR) return PLASTMONITOR->logicalBox();",
        "} else if (const auto PMONITOR = State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).configString(output).run(); PMONITOR) return PMONITOR->logicalBox();",
        "case HID_TYPE_TABLET: { CTablet* TAB = rc<CTablet*>(dev.get());",
        "if (!TAB->m_boundOutput.empty()) { mappedArea = outputMappedArea(TAB->m_boundOutput); mappedArea.translate(TAB->m_boundBox.pos());",
        "} else if (TAB->m_absolutePos) { mappedArea.x = TAB->m_boundBox.x; mappedArea.y = TAB->m_boundBox.y;",
        "} else mappedArea.translate(TAB->m_boundBox.pos());",
        "if (!TAB->m_boundBox.empty()) { mappedArea.w = TAB->m_boundBox.w; mappedArea.h = TAB->m_boundBox.h;",
        "m_pointerPos = mappedArea.pos() + mappedArea.size() * abs;",
        "void CPointerManager::onMonitorLayoutChange() { m_currentMonitorLayout.monitorBoxes.clear(); for (auto const& m : State::monitorState()->monitors()) { if (m->isMirror() || !m->m_enabled || !m->m_output) continue; m_currentMonitorLayout.monitorBoxes.emplace_back(m->m_position, m->m_size);",
        "damageIfSoftware(); m_pointerPos = closestValid(m_pointerPos); updateCursorBackend(); recheckEnteredOutputs();",
    )
    config_actions_055 = (
        'static void updateRelativeCursorCoords() { static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps"); if (*PNOWARPS) return; if (Desktop::focusState()->window()) Desktop::focusState()->window()->m_relativeCursorCoordsOnLastWarp = g_pInputManager->getMouseCoordsInternal() - Desktop::focusState()->window()->m_position;',
        "updateRelativeCursorCoords(); Desktop::focusState()->fullWindowFocus(PWINDOWTOCHANGETO, Desktop::FOCUS_REASON_SWITCH_TO_WINDOW_SOFT, nullptr, forceFSCycle); PWINDOWTOCHANGETO->warpCursor();",
        'else if (PROP == "no_follow_mouse") parsePropTrivial(PWINDOW->m_ruleApplicator->noFollowMouse(), VAL);',
        "ActionResult Actions::moveCursor(const Vector2D& pos) { g_pCompositor->warpCursorTo(pos, true); g_pInputManager->simulateMouseMovement();",
    )
    config_actions_056 = (
        'static void updateRelativeCursorCoords() { static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps"); if (*PNOWARPS) return; if (Desktop::focusState()->window()) Desktop::focusState()->window()->m_relativeCursorCoordsOnLastWarp = g_pInputManager->getMouseCoordsInternal() - Desktop::focusState()->window()->layoutBox().pos();',
        "updateRelativeCursorCoords(); Desktop::focusState()->fullWindowFocus(PWINDOWTOCHANGETO, Desktop::FOCUS_REASON_SWITCH_TO_WINDOW_SOFT, nullptr, forceFSCycle); PWINDOWTOCHANGETO->warpCursor();",
        'else if (PROP == "no_follow_mouse") parsePropTrivial(PWINDOW->m_ruleApplicator->noFollowMouse(), VAL);',
        "ActionResult Actions::moveCursor(const Vector2D& pos) { Pointer::pointerController()->warpTo(pos, true); g_pInputManager->simulateMouseMovement();",
    )
    window_header = (
        "m_relativeCursorCoordsOnLastWarp = Vector2D(-1, -1);",
        "warpCursor(bool force = false);",
    )
    window_warp_055 = (
        'void CWindow::warpCursor(bool force) { static auto PERSISTENTWARPS = CConfigValue<Config::INTEGER>("cursor:persistent_warps");',
        "const auto coords = m_relativeCursorCoordsOnLastWarp;",
        "m_relativeCursorCoordsOnLastWarp.x = -1;",
        "if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < m_size)",
        "g_pCompositor->warpCursorTo(m_position + coords, force);",
        "else",
        "g_pCompositor->warpCursorTo(middle(), force);",
    )
    window_warp_056 = (
        'void CWindow::warpCursor(bool force) { static auto PERSISTENTWARPS = CConfigValue<Config::INTEGER>("cursor:persistent_warps");',
        "const auto coords = m_relativeCursorCoordsOnLastWarp;",
        "m_relativeCursorCoordsOnLastWarp.x = -1;",
        "const auto BOX = layoutBox();",
        "if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < BOX.size())",
        "Pointer::pointerController()->warpTo(BOX.pos() + coords, force);",
        "else",
        "Pointer::pointerController()->warpTo(middle(), force);",
    )
    no_warps_055 = (
        "void CCompositor::warpCursorTo(const Vector2D& pos, bool force) {",
        'static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps");',
        "if (*PNOWARPS && !force) {",
        "const auto PMONITORNEW = getMonitorFromVector(pos);",
        "Desktop::focusState()->rawMonitorFocus(PMONITORNEW);",
        "return;",
        "g_pPointerManager->warpTo(pos);",
        "const auto PMONITORNEW = getMonitorFromVector(pos);",
        "Desktop::focusState()->rawMonitorFocus(PMONITORNEW);",
    )
    no_warps_056 = (
        "void CPointerController::warpTo(const Vector2D& pos, bool force) const {",
        'static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps");',
        "if (*PNOWARPS && !force) {",
        "const auto PMONITORNEW = State::monitorState()->query().vec(pos).run();",
        "Desktop::focusState()->rawMonitorFocus(PMONITORNEW);",
        "return;",
        "Pointer::mgr()->warpTo(pos);",
        "const auto PMONITORNEW = State::monitorState()->query().vec(pos).run();",
        "Desktop::focusState()->rawMonitorFocus(PMONITORNEW);",
    )

    return {
        "0.55.0": {
            REGISTRY_PATH: registry,
            Path("src/config/lua/ConfigManager.cpp"): (
                *reload_reset,
                *device_fallback,
            ),
            Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): device_binding,
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): input_refresh,
            Path("src/managers/input/InputManager.hpp"): (
                *transform_matrices,
                *retained_input_state,
            ),
            Path("src/managers/input/InputManager.cpp"): (
                *pointer_common,
                "g_pPointerManager->move(DELTA);",
                *pointer_motion_state,
                *pointer_buttons_055,
                *keyboard_resolution,
                *pointer_configuration,
                *touch_and_tablet_configuration,
                *warp_back_055,
            ),
            Path("src/managers/input/Tablets.cpp"): tablet_motion_055,
            Path("src/protocols/PrimarySelection.cpp"): primary_selection,
            Path("src/render/Renderer.cpp"): (
                *renderer_constructor,
                *renderer_visibility,
                "const bool HIDE = m_cursorHiddenByCondition || (*PINVISIBLE != 0);",
                "for (auto const& m : g_pCompositor->m_monitors) { if (!g_pPointerManager->softwareLockedFor(m)) continue; g_pPointerManager->damageCursor(m, m->shouldSkipScheduleFrameOnMouseEvent());",
                "setCursorHidden(HIDE);",
                "void IHyprRenderer::setCursorHidden(bool hide) { if (hide == m_cursorHidden) return; m_cursorHidden = hide;",
                "if (hide) { g_pPointerManager->resetCursorImage(); return;",
                'if (m_lastCursorData.surf.has_value()) setCursorSurface(m_lastCursorData.surf.value(), m_lastCursorData.hotspotX, m_lastCursorData.hotspotY, true); else if (!m_lastCursorData.name.empty()) setCursorFromName(m_lastCursorData.name, true); else setCursorFromName("left_ptr", true);',
            ),
            Path("src/render/Renderer.hpp"): renderer_header,
            Path("src/managers/input/Touch.cpp"): touch_events,
            Path("src/devices/Mouse.cpp"): mouse_events,
            Path("src/managers/PointerManager.hpp"): pointer_manager_header,
            Path("src/managers/PointerManager.cpp"): (
                *pointer_clamp,
                *pointer_routing_055,
            ),
            Path("src/config/shared/actions/ConfigActions.cpp"): config_actions_055,
            Path("src/desktop/view/Window.cpp"): window_warp_055,
            Path("src/desktop/view/Window.hpp"): window_header,
            Path("src/Compositor.cpp"): no_warps_055,
            Path("src/Compositor.hpp"): (
                "warpCursorTo(const Vector2D&, bool force = false);",
            ),
        },
        "0.56.1": {
            REGISTRY_PATH: registry,
            Path("src/config/lua/ConfigManager.cpp"): (
                *reload_reset,
                "v.second->resetSetByUser();",
                *device_fallback,
            ),
            Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): device_binding,
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): input_refresh,
            Path("src/managers/input/InputManager.hpp"): (
                *transform_matrices,
                *retained_input_state,
            ),
            Path("src/managers/input/InputManager.cpp"): (
                *pointer_common,
                "Pointer::mgr()->move(DELTA);",
                "if (PROTO::inputCapture->isCaptured()) return;",
                *pointer_motion_state,
                *pointer_buttons_056,
                *keyboard_resolution,
                *pointer_configuration,
                *touch_and_tablet_configuration,
                *warp_back_056,
            ),
            Path("src/managers/input/Tablets.cpp"): tablet_motion_056,
            Path("src/protocols/PrimarySelection.cpp"): primary_selection,
            Path("src/render/Renderer.cpp"): (
                *renderer_constructor,
                *renderer_visibility,
                "const bool HIDE = m_cursorHiddenByCondition || (*PINVISIBLE != 0) || PROTO::inputCapture->isCaptured();",
                "for (auto const& m : State::monitorState()->monitors()) { if (!Pointer::mgr()->softwareLockedFor(m)) continue; Pointer::mgr()->damageCursor(m, m->shouldSkipScheduleFrameOnMouseEvent());",
                "setCursorHidden(HIDE);",
                "void IHyprRenderer::setCursorHidden(bool hide) { if (hide == m_cursorHidden) return; m_cursorHidden = hide;",
                "if (hide) { Pointer::mgr()->resetCursorImage(); return;",
                'if (m_lastCursorData.surf.has_value()) setCursorSurface(m_lastCursorData.surf.value(), m_lastCursorData.hotspotX, m_lastCursorData.hotspotY, true); else if (!m_lastCursorData.name.empty()) setCursorFromName(m_lastCursorData.name, true); else setCursorFromName("left_ptr", true);',
            ),
            Path("src/render/Renderer.hpp"): renderer_header,
            Path("src/managers/input/Touch.cpp"): touch_events,
            Path("src/devices/Mouse.cpp"): mouse_events,
            Path("src/pointer/PointerManager.hpp"): pointer_manager_header,
            Path("src/pointer/PointerManager.cpp"): (
                *pointer_clamp,
                *pointer_routing_056,
            ),
            Path("src/config/shared/actions/ConfigActions.cpp"): config_actions_056,
            Path("src/desktop/view/Window.cpp"): window_warp_056,
            Path("src/desktop/view/Window.hpp"): window_header,
            Path("src/pointer/PointerController.cpp"): no_warps_056,
            Path("src/pointer/PointerController.hpp"): (
                "warpTo(const Vector2D& point, bool force = false) const;",
            ),
        },
    }


def _assert_input_behavior_contract(
    input_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify global input behavior, device fallback, and live request gates."""
    requirements_by_version = _input_behavior_contract_requirements()
    if set(requirements_by_version) != {"0.55.0", "0.56.1"}:
        raise ValueError("input behavior patch inventory is incomplete")

    for version, expected_paths in INPUT_BEHAVIOR_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} input behavior semantic inventory is incomplete"
            )
        sources = input_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} input behavior source inventory is incomplete"
            )
        reviewed_fragments = " ".join(
            fragment
            for path in expected_paths
            for fragment in requirements[path]
        )
        registry_source = sources[REGISTRY_PATH].decode("utf-8")
        for option_path in INPUT_BEHAVIOR_OPTION_PATHS:
            if f'"{option_path}"' not in reviewed_fragments:
                raise ValueError(
                    f"Hyprland {version} has no input behavior assertion for {option_path}"
                )
            if registry_source.count(f'"{option_path}"') != 1:
                raise ValueError(
                    f"Hyprland {version} input registry count changed in "
                    f"{REGISTRY_PATH} for {option_path}"
                )
        exact_runtime_uses = {
            Path("src/managers/input/InputManager.cpp"): (
                "input:force_no_accel",
                "input:follow_mouse_threshold",
                "input:resolve_binds_by_sym",
                "input:rotation",
                "input:touchdevice:enabled",
                "input:touchdevice:transform",
                "input:tablet:region_position",
                "input:tablet:absolute_region_position",
                "input:tablet:region_size",
                "input:tablet:relative_input",
                "input:tablet:left_handed",
                "input:tablet:transform",
                "cursor:warp_back_after_non_mouse_input",
            ),
            Path("src/render/Renderer.cpp"): (
                "cursor:hide_on_key_press",
                "cursor:hide_on_touch",
                "cursor:hide_on_tablet",
                "cursor:inactive_timeout",
            ),
            (
                Path("src/managers/PointerManager.cpp")
                if version == "0.55.0"
                else Path("src/pointer/PointerManager.cpp")
            ): ("cursor:hotspot_padding",),
            Path("src/desktop/view/Window.cpp"): ("cursor:persistent_warps",),
        }
        for path, option_paths in exact_runtime_uses.items():
            source = sources[path].decode("utf-8")
            for option_path in option_paths:
                if source.count(f'"{option_path}"') != 1:
                    raise ValueError(
                        f"Hyprland {version} input runtime gate count changed in "
                        f"{path} for {option_path}"
                    )
        no_warps_path = (
            Path("src/Compositor.cpp")
            if version == "0.55.0"
            else Path("src/pointer/PointerController.cpp")
        )
        if sources[no_warps_path].decode("utf-8").count('"cursor:no_warps"') != 1:
            raise ValueError(
                f"Hyprland {version} input runtime gate count changed in "
                f"{no_warps_path} for cursor:no_warps"
            )
        input_manager_path = Path("src/managers/input/InputManager.cpp")
        input_manager_source = re.sub(
            r"\s+", " ", sources[input_manager_path].decode("utf-8")
        )
        wheel_start = input_manager_source.find(
            "void CInputManager::onMouseWheel(IPointer::SAxisEvent"
        )
        wheel_end = input_manager_source.find(
            "void CInputManager::", wheel_start + 1
        )
        if wheel_start < 0 or wheel_end < 0:
            raise ValueError(
                f"Hyprland {version} input wheel boundary changed in {input_manager_path}"
            )
        if "m_lastCursorMovement.reset();" in input_manager_source[
            wheel_start:wheel_end
        ]:
            raise ValueError(
                f"Hyprland {version} wheel unexpectedly resets cursor timeout in "
                f"{input_manager_path}"
            )
        primary_source = sources[
            Path("src/protocols/PrimarySelection.cpp")
        ].decode("utf-8")
        if primary_source.count('"misc:middle_click_paste"') != 1:
            raise ValueError(
                f"Hyprland {version} primary-selection gate count changed in PrimarySelection.cpp"
            )
        device_binding_path = Path(
            "src/config/lua/bindings/LuaBindingsConfigRules.cpp"
        )
        device_binding_source = sources[device_binding_path].decode("utf-8")
        resolve_device_field = (
            '"resolve_binds_by_sym", []() -> ILuaConfigValue* '
            "{ return new CLuaConfigBool(false); }},"
        )
        if device_binding_source.count(resolve_device_field) != 1:
            raise ValueError(
                f"Hyprland {version} resolve_binds_by_sym device Bool factory "
                f"changed in {device_binding_path}"
            )
        if device_binding_source.count(
            "Supplementary::refresher()->scheduleRefresh("
            "Supplementary::REFRESH_INPUT_DEVICES);"
        ) != 1:
            raise ValueError(
                f"Hyprland {version} resolve_binds_by_sym refresh scheduling "
                f"changed in {device_binding_path}"
            )
        refresher_path = Path(
            "src/config/supplementary/propRefresher/PropRefresher.cpp"
        )
        if sources[refresher_path].decode("utf-8").count(
            "g_pInputManager->setKeyboardLayout();"
        ) != 1:
            raise ValueError(
                f"Hyprland {version} resolve_binds_by_sym refresh execution "
                f"changed in {refresher_path}"
            )
        manager_path = Path("src/managers/input/InputManager.cpp")
        manager_source = re.sub(
            r"\s+", " ", sources[manager_path].decode("utf-8")
        )
        resolve_fallback = (
            'Config::mgr()->getDeviceInt(devname, "resolve_binds_by_sym", '
            '"input:resolve_binds_by_sym")'
        )
        resolve_assignment = (
            "pKeyboard->m_resolveBindsBySym = RESOLVEBINDSBYSYM;"
        )
        unchanged_keymap = (
            'Log::logger->log(Log::DEBUG, "Not applying config to keyboard, '
            'it did not change.");'
        )
        if manager_source.count(resolve_fallback) != 1:
            raise ValueError(
                f"Hyprland {version} resolve_binds_by_sym device fallback changed "
                f"in {manager_path}"
            )
        if (
            manager_source.count(resolve_assignment) != 1
            or manager_source.find(resolve_assignment)
            > manager_source.find(unchanged_keymap)
        ):
            raise ValueError(
                f"Hyprland {version} resolve_binds_by_sym assignment no longer "
                f"precedes the unchanged-keymap return in {manager_path}"
            )
        if manager_source.count(
            "g_pKeybindManager->updateXKBTranslationState();"
        ) != 1:
            raise ValueError(
                f"Hyprland {version} global XKB translation refresh changed in "
                f"{manager_path}"
            )
        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(re.sub(r"\s+", " ", fragment) for fragment in requirements[path]),
                "input behavior",
            )


def _input_device_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    discovery_wire = (
        "static std::string devicesRequest(eHyprCtlOutputFormat format, std::string request) {",
        "if (format == eHyprCtlOutputFormat::FORMAT_JSON) {",
        'result += "\\\"mice\\\": [\\n";',
        "for (auto const& m : g_pInputManager->m_pointers) {",
        '"address": "0x{:x}", "name": "{}", "defaultSpeed": {:.5f}, "scrollFactor": {:.2f}',
        'result += "\\\"keyboards\\\": [\\n";',
        "for (auto const& k : g_pInputManager->m_keyboards) {",
        'const auto KI = INDEX_OPT.has_value() ? std::to_string(INDEX_OPT.value()) : "none";',
        '"address": "0x{:x}", "name": "{}", "rules": "{}", "model": "{}", "layout": "{}", "variant": "{}", "options": "{}", "active_layout_index": {}, "active_keymap": "{}", "capsLock": {}, "numLock": {}, "main": {}',
        'result += "\\\"tablets\\\": [\\n";',
        "for (auto const& d : g_pInputManager->m_tabletPads) {",
        '"address": "0x{:x}", "type": "tabletPad", "belongsTo": {{ "address": "0x{:x}", "name": "{}" }}',
        "for (auto const& d : g_pInputManager->m_tablets) {",
        '"address": "0x{:x}", "name": "{}"',
        "for (auto const& d : g_pInputManager->m_tabletTools) {",
        '"address": "0x{:x}", "type": "tabletTool"',
        'result += "\\\"touch\\\": [\\n";',
        "for (auto const& d : g_pInputManager->m_touches) {",
        '"address": "0x{:x}", "name": "{}"',
        'result += "\\\"switches\\\": [\\n";',
        "for (auto const& d : g_pInputManager->m_switches) {",
        '"address": "0x{:x}", "name": "{}"',
        "} else {",
    )
    name_normalization = (
        "std::string deviceNameToInternalString(const std::string& in) {",
        "auto result = in | std::views::transform([](unsigned char ch) -> char {",
        "case ' ': case '\\n': case ',': return '-';",
        "default: return sc<char>(std::tolower(ch));",
        "return result | std::ranges::to<std::string>();",
    )
    config_lifecycle = (
        "m_deviceConfigs.clear();",
        'if (guardedPCall(0, 0, 1, LUA_TIMEOUT_CONFIG_RELOAD_MS, "config reload") != LUA_OK) {',
        "postConfigReload();",
        "void CConfigManager::postConfigReload() {",
        "Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);",
        "Event::bus()->m_events.config.reloaded.emit();",
        "static std::string normalizeDeviceName(const std::string& dev) {",
        "std::ranges::replace(copy, ' ', '-');",
        "int CConfigManager::getDeviceInt(const std::string& dev, const std::string& field, const std::string& fb) {",
        "if (auto* v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field)); v && v->setByUser()) return v->asInt();",
        "if (!fallback.empty() && m_configValues.contains(fallback)) return m_configValues.at(fallback)->asInt();",
        "float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& field, const std::string& fb) {",
        "Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& field, const std::string& fb) {",
        "std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& field, const std::string& fb) {",
        "bool CConfigManager::deviceConfigExplicitlySet(const std::string& dev, const std::string& field) {",
        "bool CConfigManager::deviceConfigExists(const std::string& dev) {",
    )
    device_binding = (
        '{"resolve_binds_by_sym", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }},',
        '{"transform", []() -> ILuaConfigValue* { return new CLuaConfigInt(-1); }},',
        '{"output", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }},',
        '{"enabled", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }},',
        '{"active_area_position", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},',
        '{"active_area_size", []() -> ILuaConfigValue* { return new CLuaConfigVec2({0, 0}); }},',
        "static int hlDevice(lua_State* L) {",
        'lua_getfield(L, 1, "name");',
        "std::string devName = lua_tostring(L, -1);",
        "std::ranges::replace(devName, ' ', '-');",
        "self->m_deviceConfigs[devName].values.insert_or_assign(key, std::move(val));",
        "Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_INPUT_DEVICES);",
    )
    input_header = (
        "void newVirtualKeyboard(SP<CVirtualKeyboardV1Resource>);",
        "void newVirtualMouse(SP<CVirtualPointerV1Resource>);",
        "void setKeyboardLayout();",
        "void setPointerConfigs();",
        "void setTouchDeviceConfigs(SP<ITouch> dev = nullptr);",
        "void setTabletConfigs();",
        "std::vector<SP<IKeyboard>> m_keyboards;",
        "std::vector<SP<IPointer>> m_pointers;",
        "std::vector<SP<ITouch>> m_touches;",
        "std::vector<SP<CTablet>> m_tablets;",
        "std::vector<SP<CTabletTool>> m_tabletTools;",
        "std::vector<SP<CTabletPad>> m_tabletPads;",
        "std::vector<WP<IHID>> m_hids;",
        "std::list<SSwitchDevice> m_switches;",
        "std::string getNameForNewDevice(std::string);",
    )
    input_manager_prefix = (
        "m_listeners.newVirtualKeyboard = PROTO::virtualKeyboard->m_events.newKeyboard.listen([this](const auto& keyboard) {",
        "newVirtualKeyboard(keyboard);",
        "m_listeners.newVirtualMouse = PROTO::virtualPointer->m_events.newPointer.listen([this](const auto& mouse) {",
        "newVirtualMouse(mouse);",
        "void CInputManager::newVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keyboard) {",
        "const auto PNEWKEYBOARD = m_keyboards.emplace_back(CVirtualKeyboard::create(keyboard));",
        "setupKeyboard(PNEWKEYBOARD);",
        "void CInputManager::setupKeyboard(SP<IKeyboard> keeb) {",
        "m_hids.emplace_back(keeb);",
        "keeb->m_hlName = getNameForNewDevice(keeb->m_deviceName);",
        "disableAllKeyboards(false);",
        "applyConfigToKeyboard(keeb);",
        "void CInputManager::setKeyboardLayout() {",
        "for (auto const& k : m_keyboards) applyConfigToKeyboard(k);",
        "g_pKeybindManager->updateXKBTranslationState();",
        "void CInputManager::applyConfigToKeyboard(SP<IKeyboard> pKeyboard) {",
        "const auto HASCONFIG = Config::mgr()->deviceConfigExists(devname);",
        'const auto RESOLVEBINDSBYSYM = Config::mgr()->getDeviceInt(devname, "resolve_binds_by_sym", "input:resolve_binds_by_sym");',
        'const auto ENABLED = HASCONFIG && Config::mgr()->deviceConfigExplicitlySet(devname, "enabled") ? Config::mgr()->getDeviceInt(devname, "enabled") : true; const auto ALLOWBINDS',
        "pKeyboard->m_enabled = ENABLED;",
        "pKeyboard->m_resolveBindsBySym = RESOLVEBINDSBYSYM;",
        "try {",
        "if (NUMLOCKON == pKeyboard->m_numlockOn && REPEATDELAY == pKeyboard->m_repeatDelay && REPEATRATE == pKeyboard->m_repeatRate && RULES == pKeyboard->m_currentRules.rules &&",
        "OPTIONS == pKeyboard->m_currentRules.options && FILEPATH == pKeyboard->m_xkbFilePath) {",
        'Log::logger->log(Log::DEBUG, "Not applying config to keyboard, it did not change.");',
        "return;",
        "pKeyboard->setKeymap(IKeyboard::SStringRuleNames{LAYOUT, MODEL, VARIANT, OPTIONS, RULES});",
        "void CInputManager::newVirtualMouse(SP<CVirtualPointerV1Resource> mouse) {",
        "const auto PMOUSE = m_pointers.emplace_back(CVirtualPointer::create(mouse));",
        "setupMouse(PMOUSE);",
        "void CInputManager::setupMouse(SP<IPointer> mauz) {",
        "m_hids.emplace_back(mauz);",
        "mauz->m_hlName = getNameForNewDevice(mauz->m_deviceName);",
    )
    input_manager_suffix = (
        "mauz->m_connected = true;",
        "setPointerConfigs();",
        "void CInputManager::setPointerConfigs() {",
        "const auto HASCONFIG = Config::mgr()->deviceConfigExists(devname);",
        "if (HASCONFIG) {",
        'const auto ENABLED = HASCONFIG && Config::mgr()->deviceConfigExplicitlySet(devname, "enabled") ? Config::mgr()->getDeviceInt(devname, "enabled") : true; if (ENABLED && !m->m_connected) {',
        "m->m_connected = true;",
        "} else if (!ENABLED && m->m_connected) {",
        "m->m_connected = false;",
        "static void removeFromHIDs(WP<IHID> hid) {",
        "void CInputManager::newTouchDevice(SP<Aquamarine::ITouch> pDevice) {",
        "const auto PNEWDEV = m_touches.emplace_back(CTouchDevice::create(pDevice));",
        "PNEWDEV->m_hlName = getNameForNewDevice(PNEWDEV->m_deviceName);",
        "setTouchDeviceConfigs(PNEWDEV);",
        "void CInputManager::setTouchDeviceConfigs(SP<ITouch> dev) {",
        'const auto ENABLED = Config::mgr()->getDeviceInt(PTOUCHDEV->m_hlName, "enabled", "input:touchdevice:enabled");',
        "const auto mode = ENABLED ? LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;",
        'const int ROTATION = std::clamp(Config::mgr()->getDeviceInt(PTOUCHDEV->m_hlName, "transform", "input:touchdevice:transform"), -1, 7);',
        "if (ROTATION > -1) libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);",
        "void CInputManager::setTabletConfigs() {",
        'const int ROTATION = std::clamp(Config::mgr()->getDeviceInt(NAME, "transform", "input:tablet:transform"), -1, 7);',
        "if (ROTATION > -1) libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);",
        'const auto ACTIVE_AREA_SIZE = Config::mgr()->getDeviceVec(NAME, "active_area_size", "input:tablet:active_area_size");',
        "if (ACTIVE_AREA_SIZE.x != 0 || ACTIVE_AREA_SIZE.y != 0) {",
        "t->m_activeArea = CBox{ACTIVE_AREA_POS.x / effectivePhysicalSize.x, ACTIVE_AREA_POS.y / effectivePhysicalSize.y,",
        "void CInputManager::newSwitch(SP<Aquamarine::ISwitch> pDevice) {",
        "std::string CInputManager::getNameForNewDevice(std::string internalName) {",
        "auto proposedNewName = deviceNameToInternalString(internalName);",
        'auto makeNewName = [&]() { return (proposedNewName.empty() ? "unknown-device" : proposedNewName) + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); };',
        "while (std::ranges::find_if(m_hids, [&](const auto& other) { return other->m_hlName == makeNewName(); }) != m_hids.end()) dupeno++;",
        "return makeNewName();",
    )
    tablets_prefix = (
        "void CInputManager::newTablet(SP<Aquamarine::ITablet> pDevice) {",
        "const auto PNEWTABLET = m_tablets.emplace_back(CTablet::create(pDevice));",
        "m_hids.emplace_back(PNEWTABLET);",
        "PNEWTABLET->m_hlName = g_pInputManager->getNameForNewDevice(pDevice->getName());",
    )
    tablets_suffix = (
        "setTabletConfigs();",
        "SP<CTabletTool> CInputManager::ensureTabletToolPresent(SP<Aquamarine::ITabletTool> pTool) {",
        "const auto PTOOL = m_tabletTools.emplace_back(CTabletTool::create(pTool));",
        "m_hids.emplace_back(PTOOL);",
        "PTOOL->m_hlName = g_pInputManager->getNameForNewDevice(pTool->getName());",
        "void CInputManager::newTabletPad(SP<Aquamarine::ITabletPad> pDevice) {",
        "const auto PNEWPAD = m_tabletPads.emplace_back(CTabletPad::create(pDevice));",
        "m_hids.emplace_back(PNEWPAD);",
        "PNEWPAD->m_hlName = g_pInputManager->getNameForNewDevice(pDevice->getName());",
        "PNEWPAD->m_padEvents.attach.listenStatic([pad = PNEWPAD.get()](const SP<CTabletTool>& tool) { pad->m_parent = tool; });",
    )
    hid_type = (
        "eHIDType IHID::getType() {",
        "return HID_TYPE_UNKNOWN;",
    )
    pointer_type = (
        "uint32_t IPointer::getCapabilities() {",
        "return HID_INPUT_CAPABILITY_POINTER;",
        "eHIDType IPointer::getType() {",
        "return HID_TYPE_POINTER;",
    )
    keyboard_type_and_keymap = (
        "uint32_t IKeyboard::getCapabilities() {",
        "return HID_INPUT_CAPABILITY_KEYBOARD;",
        "eHIDType IKeyboard::getType() {",
        "return HID_TYPE_KEYBOARD;",
        "void IKeyboard::setKeymap(const SStringRuleNames& rules) {",
        "if (m_keymapOverridden) {",
        'Log::logger->log(Log::DEBUG, "Ignoring setKeymap: keymap is overridden");',
        "m_currentRules = rules;",
        "if (!m_xkbFilePath.empty()) {",
        "m_xkbKeymap = xkb_keymap_new_from_file(CONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);",
        "if (!m_xkbKeymap) m_xkbKeymap = xkb_keymap_new_from_names2(CONTEXT, &XKBRULES, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);",
        'm_currentRules.layout = "us";',
        "void IKeyboard::updateXKBTranslationState(xkb_keymap* const keymap) {",
        "if (m_xkbSymState)",
        "xkb_state_unref(m_xkbSymState);",
        "m_xkbSymState = nullptr;",
        "if (keymap) {",
        "m_xkbSymState = xkb_state_new(keymap);",
        "if (xkb_state_layout_index_is_active(STATE, i, XKB_STATE_LAYOUT_EFFECTIVE) == 1) {",
        "m_xkbSymState = xkb_state_new(KEYMAP);",
        "m_xkbSymState = xkb_state_new(NEWKEYMAP);",
        "void IKeyboard::updateModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {",
        "if (m_xkbSymState)",
        "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, group);",
        "void IKeyboard::updateXkbStateWithKey(uint32_t xkbKey, bool pressed) {",
        "if (m_xkbSymState)",
        "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, m_modifiersState.group);",
    )
    virtual_keyboard_device = (
        "SP<CVirtualKeyboard> CVirtualKeyboard::create(SP<CVirtualKeyboardV1Resource> keeb) {",
        "CVirtualKeyboard::CVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keeb_) : m_keyboard(keeb_) {",
        "m_listeners.keymap = keeb_->m_events.keymap.listen([this](const SKeymapEvent& event) {",
        "m_xkbKeymap = xkb_keymap_ref(event.keymap);",
        "m_keymapOverridden = true;",
        "m_deviceName = keeb_->m_name;",
        'const auto SHARESTATES = Config::mgr()->getDeviceInt(m_deviceName, "share_states", "input:virtualkeyboard:share_states");',
        "m_shareStates = SHARESTATES != 0;",
        "m_shareStatesAuto = SHARESTATES == 2;",
        "bool CVirtualKeyboard::isVirtual() {",
        "return true;",
    )
    virtual_pointer_device = (
        "SP<CVirtualPointer> CVirtualPointer::create(SP<CVirtualPointerV1Resource> resource) {",
        "CVirtualPointer::CVirtualPointer(SP<CVirtualPointerV1Resource> resource) : m_pointer(resource) {",
        'm_boundOutput = resource->m_boundOutput ? resource->m_boundOutput->m_name : "";',
        "m_deviceName = m_pointer->m_name;",
        "bool CVirtualPointer::isVirtual() {",
        "return true;",
        "SP<Aquamarine::IPointer> CVirtualPointer::aq() {",
        "return nullptr;",
    )
    virtual_keyboard_protocol = (
        "static std::string virtualKeyboardNameForWlClient(wl_client* client) {",
        'std::string name = "hl-virtual-keyboard";',
        'static auto PVKNAMEPROC = CConfigValue<Config::INTEGER>("misc:name_vk_after_proc");',
        "CVirtualKeyboardV1Resource::CVirtualKeyboardV1Resource(SP<CZwpVirtualKeyboardV1> resource_) : m_resource(resource_) {",
        "m_resource->setKeymap([this](CZwpVirtualKeyboardV1* r, uint32_t fmt, int32_t fd, uint32_t len) {",
        "auto xkbKeymap = xkb_keymap_new_from_string(xkbContext, sc<const char*>(keymapData), XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);",
        "m_events.keymap.emit(IKeyboard::SKeymapEvent{",
        "m_hasKeymap = true;",
        "m_name = virtualKeyboardNameForWlClient(resource_->client());",
        "void CVirtualKeyboardV1Resource::destroy() {",
        'const auto RELEASEPRESSED = Config::mgr()->getDeviceInt(m_name, "release_pressed_on_close", "input:virtualkeyboard:release_pressed_on_close");',
        "void CVirtualKeyboardProtocol::onCreateKeeb(CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id) {",
        "const auto RESOURCE = m_keyboards.emplace_back(makeShared<CVirtualKeyboardV1Resource>(makeShared<CZwpVirtualKeyboardV1>(pMgr->client(), pMgr->version(), id)));",
        "m_events.newKeyboard.emit(RESOURCE);",
    )
    virtual_pointer_protocol = (
        "CVirtualPointerV1Resource::CVirtualPointerV1Resource(SP<CZwlrVirtualPointerV1> resource_, PHLMONITORREF boundOutput_) : m_boundOutput(boundOutput_), m_resource(resource_) {",
        "void CVirtualPointerProtocol::onCreatePointer(CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, uint32_t id, PHLMONITORREF output) {",
        "const auto RESOURCE = m_pointers.emplace_back(makeShared<CVirtualPointerV1Resource>(makeShared<CZwlrVirtualPointerV1>(pMgr->client(), pMgr->version(), id), output));",
        "m_events.newPointer.emit(RESOURCE);",
    )
    compositor_devices = (
        "m_aqBackend->events.newPointer.listenStatic([](const SP<Aquamarine::IPointer>& dev) {",
        "g_pInputManager->newMouse(dev);",
        "m_aqBackend->events.newKeyboard.listenStatic([](const SP<Aquamarine::IKeyboard>& dev) {",
        "g_pInputManager->newKeyboard(dev);",
        "m_aqBackend->events.newTouch.listenStatic([](const SP<Aquamarine::ITouch>& dev) {",
        "g_pInputManager->newTouchDevice(dev);",
        "m_aqBackend->events.newSwitch.listenStatic([](const SP<Aquamarine::ISwitch>& dev) {",
        "g_pInputManager->newSwitch(dev);",
        "m_aqBackend->events.newTablet.listenStatic([](const SP<Aquamarine::ITablet>& dev) {",
        "g_pInputManager->newTablet(dev);",
        "m_aqBackend->events.newTabletPad.listenStatic([](const SP<Aquamarine::ITabletPad>& dev) {",
        "g_pInputManager->newTabletPad(dev);",
    )
    pointer_manager = (
        "void CPointerManager::attachPointer(SP<IPointer> pointer) {",
        "auto listener = m_pointerListeners.emplace_back(makeShared<SPointerListener>());",
        "void CPointerManager::attachTouch(SP<ITouch> touch) {",
        "auto listener = m_touchListeners.emplace_back(makeShared<STouchListener>());",
        "void CPointerManager::attachTablet(SP<CTablet> tablet) {",
        "auto listener = m_tabletListeners.emplace_back(makeShared<STabletListener>());",
        "void CPointerManager::detachPointer(SP<IPointer> pointer) {",
        "std::erase_if(m_pointerListeners, [pointer](const auto& e) { return e->pointer.expired() || e->pointer == pointer; });",
        "void CPointerManager::detachTouch(SP<ITouch> touch) {",
        "std::erase_if(m_touchListeners, [touch](const auto& e) { return e->touch.expired() || e->touch == touch; });",
        "void CPointerManager::detachTablet(SP<CTablet> tablet) {",
        "std::erase_if(m_tabletListeners, [tablet](const auto& e) { return e->tablet.expired() || e->tablet == tablet; });",
    )

    requirements: dict[str, dict[Path, tuple[str, ...]]] = {}
    for version in ("0.55.0", "0.56.1"):
        pointer_namespace = "g_pPointerManager" if version == "0.55.0" else "Pointer::mgr()"
        refresher_tail = () if version == "0.55.0" else ("g_pInputManager->setTabletToolConfigs();",)
        pointer_mode = () if version == "0.55.0" else (
            "const auto mode = ENABLED ? LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;",
            "libinput_device_config_send_events_set_mode(LIBINPUTDEV, mode);",
        )
        tablet_tool_config = () if version == "0.55.0" else (
            "void CInputManager::setTabletToolConfigs() {",
        )
        tablet_attach = f"{pointer_namespace}->attachTablet(PNEWTABLET);"
        pointer_path = INPUT_DEVICE_SOURCE_PATHS[version][-1]
        requirements[version] = {
            Path("src/debug/HyprCtl.cpp"): discovery_wire,
            Path("src/helpers/MiscFunctions.cpp"): name_normalization,
            Path("src/config/lua/ConfigManager.cpp"): config_lifecycle,
            Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): device_binding,
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): (
                "if (m_propsTripped & REFRESH_INPUT_DEVICES) {",
                "g_pInputManager->setKeyboardLayout();",
                "g_pInputManager->setPointerConfigs();",
                "g_pInputManager->setTouchDeviceConfigs();",
                "g_pInputManager->setTabletConfigs();",
                *refresher_tail,
            ),
            Path("src/managers/input/InputManager.hpp"): input_header,
            Path("src/managers/input/InputManager.cpp"): (
                *input_manager_prefix,
                f"{pointer_namespace}->attachPointer(mauz);",
                *input_manager_suffix[:6],
                f"{pointer_namespace}->attachPointer(m);",
                *input_manager_suffix[6:8],
                f"{pointer_namespace}->detachPointer(m);",
                *input_manager_suffix[8:9],
                *pointer_mode,
                *input_manager_suffix[9:25],
                *tablet_tool_config,
                *input_manager_suffix[25:],
            ),
            Path("src/managers/input/Tablets.cpp"): (
                *tablets_prefix,
                tablet_attach,
                *tablets_suffix,
            ),
            Path("src/devices/IHID.cpp"): hid_type,
            Path("src/devices/IPointer.cpp"): pointer_type,
            Path("src/devices/IKeyboard.cpp"): keyboard_type_and_keymap,
            Path("src/devices/VirtualKeyboard.cpp"): virtual_keyboard_device,
            Path("src/devices/VirtualPointer.cpp"): virtual_pointer_device,
            Path("src/protocols/VirtualKeyboard.cpp"): virtual_keyboard_protocol,
            Path("src/protocols/VirtualPointer.cpp"): virtual_pointer_protocol,
            Path("src/Compositor.cpp"): compositor_devices,
            pointer_path: pointer_manager,
        }
    return requirements


def _normalized_cpp_slice(source: str, start: str, end: str) -> str:
    normalized = re.sub(r"\s+", " ", source)
    begin = normalized.find(start)
    finish = normalized.find(end, begin + len(start)) if begin >= 0 else -1
    if begin < 0 or finish < 0:
        raise ValueError(f"reviewed C++ slice changed between {start!r} and {end!r}")
    return normalized[begin:finish]


def _assert_input_device_contract(
    input_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify discovery identity and the generic-device Restart boundary."""
    requirements_by_version = _input_device_contract_requirements()
    if set(requirements_by_version) != set(INPUT_DEVICE_SOURCE_PATHS):
        raise ValueError("input-device version inventory is incomplete")

    for version, expected_paths in INPUT_DEVICE_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} input-device semantic inventory is incomplete"
            )
        sources = input_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} input-device source inventory is incomplete"
            )
        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(re.sub(r"\s+", " ", fragment) for fragment in requirements[path]),
                "input-device",
            )

        keyboard_path = Path("src/devices/IKeyboard.cpp")
        keyboard_source = sources[keyboard_path].decode("utf-8")
        if len(
            re.findall(
                r"m_xkbSymState\s*=\s*xkb_state_new\(", keyboard_source
            )
        ) != 3:
            raise ValueError(
                f"Hyprland {version} per-keyboard symbol-state creation "
                f"changed in {keyboard_path}"
            )
        for group_source in ("group", "m_modifiersState.group"):
            update = (
                "xkb_state_update_mask(m_xkbSymState, 0, 0, 0, 0, 0, "
                f"{group_source});"
            )
            if keyboard_source.count(update) != 1:
                raise ValueError(
                    f"Hyprland {version} per-keyboard active-group update "
                    f"changed in {keyboard_path}"
                )

        hyprctl_path = Path("src/debug/HyprCtl.cpp")
        hyprctl_source = sources[hyprctl_path].decode("utf-8")
        try:
            wire = _normalized_cpp_slice(
                hyprctl_source,
                "static std::string devicesRequest",
                " } else {",
            )
        except ValueError as error:
            raise ValueError(
                f"Hyprland {version} input-device wire changed in {hyprctl_path}: {error}"
            ) from error
        expected_wire_counts = {
            '\\"mice\\"': 1,
            '\\"keyboards\\"': 1,
            '\\"tablets\\"': 1,
            '\\"touch\\"': 1,
            '\\"switches\\"': 1,
            '"address"': 8,
            '"name"': 6,
            '"type"': 2,
            '"belongsTo"': 1,
            '"active_layout_index"': 1,
            '"active_keymap"': 1,
        }
        actual_wire_counts = {
            token: wire.count(token) for token in expected_wire_counts
        }
        if actual_wire_counts != expected_wire_counts:
            raise ValueError(
                f"Hyprland {version} input-device wire field/count inventory changed "
                f"in {hyprctl_path}: {actual_wire_counts!r}"
            )
        forbidden_wire_fields = (
            '"enabled"',
            '"isVirtual"',
            '"virtual"',
            '"touchpad"',
            '"capabilities"',
            '"count"',
        )
        if any(field in wire for field in forbidden_wire_fields):
            raise ValueError(
                f"Hyprland {version} input-device omission contract changed in {hyprctl_path}"
            )

        input_path = Path("src/managers/input/InputManager.cpp")
        input_source = sources[input_path].decode("utf-8")
        try:
            pointer_config = _normalized_cpp_slice(
                input_source,
                "void CInputManager::setPointerConfigs()",
                "static void removeFromHIDs",
            )
            touch_config = _normalized_cpp_slice(
                input_source,
                "void CInputManager::setTouchDeviceConfigs",
                "void CInputManager::setTabletConfigs",
            )
            tablet_end = (
                "void CInputManager::newSwitch"
                if version == "0.55.0"
                else "void CInputManager::setTabletToolConfigs"
            )
            tablet_config = _normalized_cpp_slice(
                input_source,
                "void CInputManager::setTabletConfigs",
                tablet_end,
            )
        except ValueError as error:
            raise ValueError(
                f"Hyprland {version} input-device reset seam changed in {input_path}: {error}"
            ) from error
        if "if (!HASCONFIG" in pointer_config or "else if (!HASCONFIG" in pointer_config:
            raise ValueError(
                f"Hyprland {version} pointer no-config detach seam changed in {input_path}"
            )
        if any(
            fragment in touch_config
            for fragment in (
                "libinput_device_config_calibration_get_default_matrix",
                "libinput_device_config_calibration_set_matrix(LIBINPUTDEV, IDENTITY",
            )
        ):
            raise ValueError(
                f"Hyprland {version} touch transform reset seam changed in {input_path}"
            )
        if any(
            fragment in tablet_config
            for fragment in (
                '"enabled"',
                "libinput_device_config_calibration_get_default_matrix",
                "libinput_device_config_calibration_set_matrix(LIBINPUTDEV, IDENTITY",
                "t->m_activeArea = {}",
            )
        ):
            raise ValueError(
                f"Hyprland {version} tablet reset seam changed in {input_path}"
            )

        virtual_pointer_path = Path("src/protocols/VirtualPointer.cpp")
        virtual_pointer_source = re.sub(
            r"\s+", " ", sources[virtual_pointer_path].decode("utf-8")
        )
        if "m_name =" in virtual_pointer_source:
            raise ValueError(
                f"Hyprland {version} virtual-pointer name timing changed in "
                f"{virtual_pointer_path}"
            )


def _gesture_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    lua_binding = (
        "static int hlGesture(lua_State* L) {",
        "CLuaConfigInt fingersParser(0, 2, 9);",
        'Internal::parseTableField(L, 1, "fingers", fingersParser);',
        "size_t fingerCount = fingersParser.parsed();",
        'Internal::parseTableField(L, 1, "direction", dirParser);',
        "const auto direction = g_pTrackpadGestures->dirForString(dirParser.parsed());",
        "if (direction == TRACKPAD_GESTURE_DIR_NONE)",
        'GET_ACTION_STRING(zoomLevel, "zoom_level");',
        'GET_ACTION_STRING(workspaceName, "workspace_name");',
        'GET_ACTION_STRING(mode, "mode");',
        'lua_getfield(L, 1, "mods");',
        "modMask = g_pKeybindManager->stringToModMask(modsParser.parsed());",
        'lua_getfield(L, 1, "scale");',
        "CLuaConfigFloat scaleParser(1.F, 0.1F, 10.F);",
        "deltaScale = scaleParser.parsed();",
        'lua_getfield(L, 1, "disable_inhibit");',
        "CLuaConfigBool disableInhibitParser(false);",
        "disableInhibit = disableInhibitParser.parsed();",
        'Internal::parseTableField(L, 1, "action", actionParser);',
        'if (action == "workspace")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CWorkspaceSwipeGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "resize")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CResizeTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "move")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CMoveTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "special")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CSpecialWorkspaceGesture>(workspaceName), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "close")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CCloseTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "float")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CFloatTrackpadGesture>(mode), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "fullscreen")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CFullscreenTrackpadGesture>(mode), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "cursor_zoom" || action == "cursorZoom")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CCursorZoomTrackpadGesture>(zoomLevel, mode), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "scroll_move")',
        "result = g_pTrackpadGestures->addGesture(makeUnique<CScrollMoveTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'else if (action == "unset")',
        "result = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale, disableInhibit);",
        'return Internal::configError(L, std::format("hl.gesture: unknown action \\"{}\\"", action));',
        "if (!result)",
        'return Internal::configError(L, std::format("hl.gesture: {}", result.error()));',
    )
    config_reload = (
        "void CConfigManager::reload() {",
        "if (!phase1Load()) {",
        "// phase 2: syntax is valid, reset and load.",
        "g_pTrackpadGestures->clearGestures();",
        "reinitLuaState();",
        "if (!phase1Load()) {",
        'if (guardedPCall(0, 0, 1, LUA_TIMEOUT_CONFIG_RELOAD_MS, "config reload") != LUA_OK) {',
        "postConfigReload();",
    )
    gesture_types = (
        "enum eTrackpadGestureDirection : uint8_t {",
        "TRACKPAD_GESTURE_DIR_NONE = 0,",
        "TRACKPAD_GESTURE_DIR_SWIPE,",
        "TRACKPAD_GESTURE_DIR_LEFT,",
        "TRACKPAD_GESTURE_DIR_RIGHT,",
        "TRACKPAD_GESTURE_DIR_UP,",
        "TRACKPAD_GESTURE_DIR_DOWN,",
        "TRACKPAD_GESTURE_DIR_VERTICAL,",
        "TRACKPAD_GESTURE_DIR_HORIZONTAL,",
        "TRACKPAD_GESTURE_DIR_PINCH,",
        "TRACKPAD_GESTURE_DIR_PINCH_OUT,",
        "TRACKPAD_GESTURE_DIR_PINCH_IN,",
    )
    registry_header = (
        "void clearGestures();",
        "std::expected<void, std::string> addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, uint32_t modMask, float deltaScale, bool disableInhibit);",
        "std::expected<void, std::string> removeGesture(size_t fingerCount, eTrackpadGestureDirection direction, uint32_t modMask, float deltaScale, bool disableInhibit);",
        "struct SGestureData {",
        "size_t fingerCount = 0;",
        "uint32_t modMask = 0;",
        "eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;",
        "float deltaScale = 1.F;",
        "bool disableInhibit = false;",
        "eTrackpadGestureDirection currentDirection = TRACKPAD_GESTURE_DIR_NONE;",
        "std::vector<SP<SGestureData>> m_gestures;",
    )
    registry_runtime = (
        "void CTrackpadGestures::clearGestures() {",
        "m_gestures.clear();",
        "eTrackpadGestureDirection CTrackpadGestures::dirForString(const std::string_view& s) {",
        'if (lc == "swipe")',
        'if (lc == "left" || lc == "l")',
        'if (lc == "right" || lc == "r")',
        'if (lc == "up" || lc == "u" || lc == "top" || lc == "t")',
        'if (lc == "down" || lc == "d" || lc == "bottom" || lc == "b")',
        'if (lc == "horizontal" || lc == "horiz")',
        'if (lc == "vertical" || lc == "vert")',
        'if (lc == "pinch")',
        'if (lc == "pinchin" || lc == "zoomin")',
        'if (lc == "pinchout" || lc == "zoomout")',
        "std::expected<void, std::string> CTrackpadGestures::addGesture",
        "for (const auto& g : m_gestures) {",
        "if (g->fingerCount != fingerCount)",
        "if (g->modMask != modMask)",
        "if (g->direction == axis || g->direction == direction ||",
        'std::format("Gesture will be overshadowed by a previous gesture. Previous {} shadows new {}",',
        "m_gestures.emplace_back(makeShared<CTrackpadGestures::SGestureData>(std::move(gesture), fingerCount, modMask, direction, deltaScale, disableInhibit));",
        "std::expected<void, std::string> CTrackpadGestures::removeGesture",
        "return g->fingerCount == fingerCount && g->direction == direction && g->modMask == modMask && g->deltaScale == deltaScale && g->disableInhibit == disableInhibit;",
        'return std::unexpected("Can\'t remove a non-existent gesture");',
        "std::erase(m_gestures, *IT);",
        "void CTrackpadGestures::gestureBegin(const IPointer::SSwipeBeginEvent& e) {",
        "void CTrackpadGestures::gestureUpdate(const IPointer::SSwipeUpdateEvent& e) {",
        'CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");',
        "if (PROTO::shortcutsInhibit->isInhibited() && !*PDISABLEINHIBIT && !g->disableInhibit)",
        "g->currentDirection = g->gesture->isDirectionSensitive() ? g->direction : direction;",
        "m_activeGesture->gesture->begin({.swipe = &e, .direction = direction, .scale = g->deltaScale});",
        "m_activeGesture->gesture->update({.swipe = &e, .direction = m_activeGesture->currentDirection, .scale = m_activeGesture->deltaScale});",
        "m_activeGesture->gesture->end({.swipe = &e, .direction = m_activeGesture->direction, .scale = m_activeGesture->deltaScale});",
        "void CTrackpadGestures::gestureBegin(const IPointer::SPinchBeginEvent& e) {",
        "void CTrackpadGestures::gestureUpdate(const IPointer::SPinchUpdateEvent& e) {",
        'CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");',
        "if (PROTO::shortcutsInhibit->isInhibited() && !*PDISABLEINHIBIT && !g->disableInhibit)",
        "g->currentDirection = g->gesture->isDirectionSensitive() ? g->direction : direction;",
        "m_activeGesture->gesture->begin({.pinch = &e, .direction = direction});",
        "m_activeGesture->gesture->update({.pinch = &e, .direction = m_activeGesture->currentDirection});",
        "m_activeGesture->gesture->end({.pinch = &e, .direction = m_activeGesture->direction});",
    )
    base_header = (
        "struct STrackpadGestureBegin {",
        "eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;",
        "float scale = 1.F;",
        "struct STrackpadGestureUpdate {",
        "eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;",
        "float scale = 1.F;",
        "struct STrackpadGestureEnd {",
        "eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;",
        "float scale = 1.F;",
        "float m_lastPinchScale = 1.F, m_scale = 1.F;",
    )
    base_runtime = (
        "void ITrackpadGesture::begin(const STrackpadGestureBegin& e) {",
        "m_lastPinchScale = 1.F;",
        "m_scale = e.scale;",
        "float ITrackpadGesture::distance(const STrackpadGestureBegin& e) {",
        "if (e.direction == TRACKPAD_GESTURE_DIR_LEFT || e.direction == TRACKPAD_GESTURE_DIR_RIGHT || e.direction == TRACKPAD_GESTURE_DIR_HORIZONTAL)",
        "if (e.direction == TRACKPAD_GESTURE_DIR_UP || e.direction == TRACKPAD_GESTURE_DIR_DOWN || e.direction == TRACKPAD_GESTURE_DIR_VERTICAL)",
        "if (e.direction == TRACKPAD_GESTURE_DIR_SWIPE)",
        "if (e.direction == TRACKPAD_GESTURE_DIR_PINCH || e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN || e.direction == TRACKPAD_GESTURE_DIR_PINCH_OUT) {",
        "m_lastPinchScale = e.pinch->scale;",
        "return m_scale * ((e.direction == TRACKPAD_GESTURE_DIR_PINCH_IN ? -\u0394 : \u0394 * PINCH_DELTA_SCALE_OUT_ADD) * PINCH_DELTA_SCALE);",
        "float ITrackpadGesture::distance(const STrackpadGestureUpdate& e) {",
        ".scale = e.scale,",
        "bool ITrackpadGesture::isDirectionSensitive() {",
        "return false;",
    )
    workspace_action = (
        "void CWorkspaceSwipeGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        "g_pUnifiedWorkspaceSwipe->begin();",
        "void CWorkspaceSwipeGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "const float DELTA = distance(e);",
        "g_pUnifiedWorkspaceSwipe->update(D);",
        "void CWorkspaceSwipeGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
        "g_pUnifiedWorkspaceSwipe->end();",
        "bool CWorkspaceSwipeGesture::isDirectionSensitive() {",
        "return true;",
    )
    resize_action = (
        "void CResizeTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        "m_window = Desktop::focusState()->window();",
        "void CResizeTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "g_layoutManager->resizeTarget((e.swipe ? e.swipe->delta : e.pinch->delta), m_window->layoutTarget(),",
        "void CResizeTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
        "m_window.reset();",
    )
    move_action = (
        "void CMoveTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        "void CMoveTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "const auto DELTA = e.swipe ? e.swipe->delta : e.pinch->delta;",
        "g_layoutManager->moveTarget(DELTA, m_window->layoutTarget());",
        "m_lastDelta += DELTA;",
        "void CMoveTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
        'g_layoutManager->moveInDirection(m_window->layoutTarget(), m_lastDelta.x > 0 ? "r" : "l");',
        'g_layoutManager->moveInDirection(m_window->layoutTarget(), m_lastDelta.y > 0 ? "b" : "t");',
        "m_window.reset();",
    )
    close_action = (
        "void CCloseTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        "void CCloseTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "m_lastDelta += distance(e);",
        "void CCloseTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
        'CConfigValue<Config::INTEGER>("gestures:close_max_timeout");',
        "const auto COMPLETION = std::clamp(m_lastDelta / MAX_DISTANCE, 0.F, 1.F);",
        "Desktop::focusState()->window()->sendClose();",
        "trackpadCloseTimers.emplace_back(timer);",
    )
    scroll_move_action = (
        "static float deltaForUpdate(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "if (!e.swipe)",
        "return 0.F;",
        "void CScrollMoveTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        "void CScrollMoveTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "const float DELTA = deltaForUpdate(e);",
        "if (DELTA == 0.F || PRIMARY <= 0.0)",
        "return;",
        "SCROLLING->moveTapeNormalized(NORMALIZED_DELTA);",
        "void CScrollMoveTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
    )
    special_action = (
        "CSpecialWorkspaceGesture::CSpecialWorkspaceGesture(const std::string& workspaceName) : m_specialWorkspaceName(workspaceName) {",
        "void CSpecialWorkspaceGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        '"special:" + m_specialWorkspaceName',
        '"special:" + m_specialWorkspaceName',
        "m_monitor->setSpecialWorkspace(WS);",
        "void CSpecialWorkspaceGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "m_lastDelta += distance(e);",
        "void CSpecialWorkspaceGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
        "m_monitor->setSpecialWorkspace(nullptr);",
    )
    float_action = (
        "CFloatTrackpadGesture::CFloatTrackpadGesture(const std::string_view& data) {",
        'if (lc.starts_with("float"))',
        "m_mode = FLOAT_MODE_FLOAT;",
        'else if (lc.starts_with("tile"))',
        "m_mode = FLOAT_MODE_TILE;",
        "m_mode = FLOAT_MODE_TOGGLE;",
        "void CFloatTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "g_layoutManager->changeFloatingMode(m_window->layoutTarget());",
        "void CFloatTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "m_lastDelta += distance(e);",
        "void CFloatTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
        "g_layoutManager->changeFloatingMode(m_window->layoutTarget());",
    )
    float_header = (
        "enum eMode : uint8_t {",
        "FLOAT_MODE_TOGGLE = 0,",
        "FLOAT_MODE_FLOAT,",
        "FLOAT_MODE_TILE,",
        "eMode m_mode = FLOAT_MODE_TOGGLE;",
    )
    fullscreen_action_common = (
        "CFullscreenTrackpadGesture::CFullscreenTrackpadGesture(const std::string_view& mode) {",
        'if (lc.starts_with("fullscreen"))',
        "m_mode = MODE_FULLSCREEN;",
        'else if (lc.starts_with("maximize"))',
        "m_mode = MODE_MAXIMIZE;",
        "m_mode = MODE_FULLSCREEN;",
        "void CFullscreenTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        "void CFullscreenTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "m_lastDelta += distance(e);",
        "void CFullscreenTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
    )
    cursor_zoom_action_common = (
        "CCursorZoomTrackpadGesture::CCursorZoomTrackpadGesture(const std::string& first, const std::string& second) {",
        'if (second == "mult")',
        "m_mode = MODE_MULT;",
        'else if (second == "live")',
        "m_mode = MODE_LIVE;",
        "void CCursorZoomTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {",
        "ITrackpadGesture::begin(e);",
        "if (m_mode == MODE_LIVE) {",
        "if (!e.pinch)",
        "return;",
        "void CCursorZoomTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {",
        "if (m_mode != MODE_LIVE || !m_monitor || !e.pinch)",
        "return;",
        "static_cast<float>(e.pinch->scale)",
        "void CCursorZoomTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {",
        "if (m_mode != MODE_LIVE || !m_monitor)",
    )
    cursor_zoom_header_common = (
        "enum eMode : uint8_t {",
        "MODE_TOGGLE = 0,",
        "MODE_MULT,",
        "MODE_LIVE,",
        "eMode m_mode = MODE_TOGGLE;",
    )

    common = {
        Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): lua_binding,
        Path("src/config/lua/ConfigManager.cpp"): config_reload,
        Path("src/managers/input/trackpad/GestureTypes.hpp"): gesture_types,
        Path("src/managers/input/trackpad/TrackpadGestures.cpp"): registry_runtime,
        Path("src/managers/input/trackpad/TrackpadGestures.hpp"): registry_header,
        Path("src/managers/input/trackpad/gestures/ITrackpadGesture.cpp"): base_runtime,
        Path("src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"): base_header,
        GESTURE_ACTION_SOURCE_PATHS["workspace"]: workspace_action,
        GESTURE_ACTION_SOURCE_PATHS["resize"]: resize_action,
        GESTURE_ACTION_SOURCE_PATHS["move"]: move_action,
        GESTURE_ACTION_SOURCE_PATHS["close"]: close_action,
        GESTURE_ACTION_SOURCE_PATHS["scrollMove"]: scroll_move_action,
        GESTURE_ACTION_SOURCE_PATHS["special"]: special_action,
        GESTURE_ACTION_SOURCE_PATHS["float"]: float_action,
        Path("src/managers/input/trackpad/gestures/FloatGesture.hpp"): float_header,
    }
    return {
        "0.55.0": {
            **common,
            GESTURE_ACTION_SOURCE_PATHS["fullscreen"]: (
                *fullscreen_action_common[:8],
                "g_pCompositor->setWindowFullscreenInternal",
                *fullscreen_action_common[8:],
                "eFullscreenMode CFullscreenTrackpadGesture::fsModeForMode(eMode mode) {",
                "case MODE_FULLSCREEN: return FSMODE_FULLSCREEN;",
                "case MODE_MAXIMIZE: return FSMODE_MAXIMIZED;",
            ),
            Path("src/managers/input/trackpad/gestures/FullscreenGesture.hpp"): (
                "enum eMode : uint8_t {",
                "MODE_FULLSCREEN = 0,",
                "MODE_MAXIMIZE,",
                "eMode m_mode = MODE_FULLSCREEN;",
                "eFullscreenMode m_originalMode = FSMODE_NONE;",
                "eFullscreenMode fsModeForMode(eMode mode);",
            ),
            GESTURE_ACTION_SOURCE_PATHS["cursorZoom"]: (
                *cursor_zoom_action_common[:10],
                "m_zoomBegin = std::clamp(PMONITOR->m_cursorZoom->value(), 1.0F, 100.0F);",
                *cursor_zoom_action_common[10:],
            ),
            Path("src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"): (
                "inline static bool m_zoomed = false;",
                *cursor_zoom_header_common,
            ),
        },
        "0.56.1": {
            **common,
            GESTURE_ACTION_SOURCE_PATHS["fullscreen"]: (
                *fullscreen_action_common[:8],
                "Fullscreen::controller()->setFullscreenMode",
                *fullscreen_action_common[8:],
                "Fullscreen::eFullscreenMode CFullscreenTrackpadGesture::fsModeForMode(eMode mode) {",
                "case MODE_FULLSCREEN: return Fullscreen::FSMODE_FULLSCREEN;",
                "case MODE_MAXIMIZE: return Fullscreen::FSMODE_MAXIMIZED;",
            ),
            Path("src/managers/input/trackpad/gestures/FullscreenGesture.hpp"): (
                "enum eMode : uint8_t {",
                "MODE_FULLSCREEN = 0,",
                "MODE_MAXIMIZE,",
                "eMode m_mode = MODE_FULLSCREEN;",
                "Fullscreen::eFullscreenMode m_originalMode = Fullscreen::FSMODE_NONE;",
                "Fullscreen::eFullscreenMode fsModeForMode(eMode mode);",
            ),
            GESTURE_ACTION_SOURCE_PATHS["cursorZoom"]: (
                *cursor_zoom_action_common[:10],
                "m_zoomBegin = std::clamp(PMONITOR->m_cursorZoom->goal(), 1.0F, 100.0F);",
                *cursor_zoom_action_common[10:],
            ),
            Path("src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"): (
                "bool m_zoomed = false;",
                *cursor_zoom_header_common,
            ),
        },
    }


def extract_gesture_action_ids(source: str) -> tuple[str, ...]:
    """Extract and canonicalize the closed hl.gesture string-action branches."""
    spellings: list[str] = []
    for match in re.finditer(
        r'(?:if|else if)\s*\(action == "([^"]+)"'
        r'(?:\s*\|\|\s*action == "([^"]+)")?\s*\)',
        source,
    ):
        spellings.extend(value for value in match.groups() if value is not None)
    if tuple(spellings) != TAGGED_GESTURE_ACTION_SPELLINGS:
        raise ValueError(
            "tagged Lua gesture action spellings changed: "
            f"expected {TAGGED_GESTURE_ACTION_SPELLINGS!r}, found {tuple(spellings)!r}"
        )

    canonical: list[str] = []
    for spelling in spellings:
        pieces = spelling.split("_")
        action = pieces[0] + "".join(piece[:1].upper() + piece[1:] for piece in pieces[1:])
        if action not in canonical:
            canonical.append(action)
    return tuple(canonical)


def _assert_gesture_runtime_contract(
    gesture_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify ordered gesture registration, dispatch, and runtime semantics."""
    requirements_by_version = _gesture_contract_requirements()
    if set(requirements_by_version) != {"0.55.0", "0.56.1"}:
        raise ValueError("gesture version inventory is incomplete")
    if set(gesture_sources) != {"0.55.0", "0.56.1"}:
        raise ValueError("gesture source version inventory is incomplete")

    expected_action_ids = tuple(action for action, _ in GESTURE_ACTIONS)
    executable_action_ids = set(expected_action_ids) - {"unset"}
    if set(GESTURE_ACTION_SOURCE_PATHS) != executable_action_ids:
        raise ValueError("gesture executable-action source inventory is incomplete")

    for version in ("0.55.0", "0.56.1"):
        requirements = requirements_by_version[version]
        if tuple(requirements) != GESTURE_SOURCE_PATHS:
            raise ValueError(
                f"Hyprland {version} gesture semantic inventory is incomplete"
            )
        sources = gesture_sources.get(version, {})
        if set(sources) != set(GESTURE_SOURCE_PATHS):
            raise ValueError(
                f"Hyprland {version} gesture source inventory is incomplete"
            )

        lua_path = Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp")
        try:
            action_ids = extract_gesture_action_ids(
                sources[lua_path].decode("utf-8")
            )
        except ValueError as error:
            raise ValueError(
                f"Hyprland {version} gesture action semantics changed in "
                f"{lua_path}: {error}"
            ) from error
        if action_ids != expected_action_ids:
            raise ValueError(
                f"Hyprland {version} canonical gesture action inventory changed: "
                f"expected {expected_action_ids!r}, found {action_ids!r}"
            )

        config_path = Path("src/config/lua/ConfigManager.cpp")
        config_source = sources[config_path].decode("utf-8")
        if config_source.count("g_pTrackpadGestures->clearGestures();") != 1:
            raise ValueError(
                f"Hyprland {version} gesture reload clear changed in {config_path.name}"
            )

        registry_path = Path("src/managers/input/trackpad/TrackpadGestures.cpp")
        registry_source = re.sub(
            r"\s+", " ", sources[registry_path].decode("utf-8")
        )
        if registry_source.count("m_gestures.clear();") != 1:
            raise ValueError(
                f"Hyprland {version} gesture registry clear changed in {registry_path.name}"
            )
        pinch_start = registry_source.find(
            "void CTrackpadGestures::gestureBegin(const IPointer::SPinchBeginEvent& e)"
        )
        if pinch_start < 0 or "deltaScale" in registry_source[pinch_start:]:
            raise ValueError(
                f"Hyprland {version} pinch scale transport changed in {registry_path.name}"
            )
        swipe_start = registry_source.find(
            "void CTrackpadGestures::gestureBegin(const IPointer::SSwipeBeginEvent& e)"
        )
        swipe_source = registry_source[swipe_start:pinch_start]
        if swipe_start < 0 or swipe_source.count(".scale =") != 3:
            raise ValueError(
                f"Hyprland {version} swipe scale transport changed in {registry_path.name}"
            )
        inhibit_gate = (
            "if (PROTO::shortcutsInhibit->isInhibited() && "
            "!*PDISABLEINHIBIT && !g->disableInhibit)"
        )
        if registry_source.count(inhibit_gate) != 2:
            raise ValueError(
                f"Hyprland {version} gesture inhibit gates changed in {registry_path.name}"
            )

        scroll_path = GESTURE_ACTION_SOURCE_PATHS["scrollMove"]
        scroll_source = re.sub(
            r"\s+", " ", sources[scroll_path].decode("utf-8")
        )
        delta_start = scroll_source.find(
            "static float deltaForUpdate(const ITrackpadGesture::STrackpadGestureUpdate& e)"
        )
        delta_end = scroll_source.find(
            "void CScrollMoveTrackpadGesture::begin", delta_start
        )
        if delta_start < 0 or delta_end <= delta_start:
            raise ValueError(
                f"Hyprland {version} scrollMove delta boundary changed in {scroll_path.name}"
            )
        delta_body = scroll_source[delta_start:delta_end]
        if "if (!e.swipe) return 0.F;" not in delta_body or "e.pinch" in delta_body:
            raise ValueError(
                f"Hyprland {version} scrollMove pinch no-op changed in {scroll_path.name}"
            )

        cursor_path = GESTURE_ACTION_SOURCE_PATHS["cursorZoom"]
        cursor_source = re.sub(
            r"\s+", " ", sources[cursor_path].decode("utf-8")
        )
        if cursor_source.count("if (!e.pinch) return;") != 1 or (
            "if (m_mode != MODE_LIVE || !m_monitor || !e.pinch) return;"
            not in cursor_source
        ):
            raise ValueError(
                f"Hyprland {version} live cursorZoom non-pinch no-op changed in "
                f"{cursor_path.name}"
            )

        for path in GESTURE_SOURCE_PATHS:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(
                    re.sub(r"\s+", " ", fragment)
                    for fragment in requirements[path]
                ),
                "gesture",
            )


def _group_bar_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    registry_common = (
        'MS<String>("group:groupbar:font_family", "font used to display groupbar titles", "[[EMPTY]]"),',
        'MS<FontWeight>("group:groupbar:font_weight_active", "weight of the font used to display active groupbar titles"),',
        'MS<FontWeight>("group:groupbar:font_weight_inactive", "weight of the font used to display inactive groupbar titles"),',
        'MS<Int>("group:groupbar:font_size", "font size of groupbar title", 8, {.min = 2, .max = 64}),',
        'MS<Bool>("group:groupbar:gradients", "enables gradients", false),',
        'MS<Int>("group:groupbar:height", "height of the groupbar", 14, {.min = 1, .max = 64}),',
        'MS<Int>("group:groupbar:indicator_gap", "height of the gap between the groupbar indicator and title", 0, {.min = 0, .max = 64}),',
        'MS<Int>("group:groupbar:indicator_height", "height of the groupbar indicator", 3, {.min = 1, .max = 64}),',
        'MS<Bool>("group:groupbar:stacked", "render the groupbar as a vertical stack", false),',
        'MS<Int>("group:groupbar:priority", "sets the decoration priority for groupbars", 3, {.min = 0, .max = 6}),',
        'MS<Bool>("group:groupbar:render_titles", "whether to render titles in the group bar decoration", true),',
        'MS<Bool>("group:groupbar:scrolling", "whether scrolling in the groupbar changes group active window", true),',
        'MS<Bool>("group:groupbar:middle_click_close", "whether middle clicking the groupbar closes the clicked window", true),',
        'MS<Int>("group:groupbar:rounding", "how much to round the groupbar", 1, {.min = 0, .max = 20}),',
        'MS<Float>("group:groupbar:rounding_power", "rounding power of groupbar corners (2 is a circle)", 2, {.min = 2, .max = 10}),',
        'MS<Int>("group:groupbar:gradient_rounding", "how much to round the groupbar gradient", 2, {.min = 0, .max = 20}),',
        'MS<Float>("group:groupbar:gradient_rounding_power", "rounding power of groupbar gradient corners (2 is a circle)", 2, {.min = 2, .max = 10}),',
        'MS<Bool>("group:groupbar:round_only_edges", "if yes, will only round at the groupbar edges", true),',
        'MS<Bool>("group:groupbar:gradient_round_only_edges", "if yes, will only round at the groupbar gradient edges", true),',
        'MS<Color>("group:groupbar:text_color", "color for window titles in the groupbar", 0xffffffff),',
        'MS<Color>("group:groupbar:text_color_inactive", "color for inactive windows\' titles in the groupbar", -1),',
        'MS<Color>("group:groupbar:text_color_locked_active", "color for the active window\'s title in a locked group", -1),',
        'MS<Color>("group:groupbar:text_color_locked_inactive", "color for inactive windows\' titles in locked groups", -1),',
        'MS<Gradient>("group:groupbar:col.active", "active group border color", 0x66ffff00),',
        'MS<Gradient>("group:groupbar:col.inactive", "inactive (out of focus) group border color", 0x66777700),',
        'MS<Gradient>("group:groupbar:col.locked_active", "active locked group border color", 0x66ff5500),',
        'MS<Gradient>("group:groupbar:col.locked_inactive", "inactive locked group border color", 0x66775500),',
        'MS<Int>("group:groupbar:gaps_out", "gap between gradients and window", 2, {.min = 0, .max = 20}),',
        'MS<Int>("group:groupbar:gaps_in", "gap between gradients", 2, {.min = 0, .max = 20}),',
        'MS<Bool>("group:groupbar:keep_upper_gap", "keep an upper gap above gradient", true),',
        'MS<Int>("group:groupbar:text_offset", "set an offset for a text", 0, {.min = -20, .max = 20}),',
        'MS<Int>("group:groupbar:text_padding", "set horizontal padding for a text", 0, {.min = 0, .max = 22}),',
        'MS<Bool>("group:groupbar:blur", "enable background blur for groupbars", false),',
    )
    group_lifecycle = (
        "void CGroup::applyWindowDecosAndUpdates(PHLWINDOW x) {",
        "x->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(x));",
        "x->updateWindowDecos();",
        "x->updateDecorationValues();",
        "void CGroup::removeWindowDecos(PHLWINDOW x) {",
        "x->removeWindowDeco(x->getDecorationByType(DECORATION_GROUPBAR));",
        "x->updateWindowDecos();",
        "x->updateDecorationValues();",
    )
    decoration_runtime = (
        "SDecorationPositioningInfo CHyprGroupBarDecoration::getPositioningInfo() {",
        'CConfigValue<Config::INTEGER>("group:groupbar:height")',
        'CConfigValue<Config::INTEGER>("group:groupbar:indicator_gap")',
        'CConfigValue<Config::INTEGER>("group:groupbar:indicator_height")',
        'CConfigValue<Config::INTEGER>("group:groupbar:render_titles")',
        'CConfigValue<Config::INTEGER>("group:groupbar:gradients")',
        'CConfigValue<Config::INTEGER>("group:groupbar:priority")',
        'CConfigValue<Config::INTEGER>("group:groupbar:stacked")',
        'CConfigValue<Config::INTEGER>("group:groupbar:gaps_out")',
        'CConfigValue<Config::INTEGER>("group:groupbar:keep_upper_gap")',
        "info.policy   = DECORATION_POSITION_STICKY;",
        "info.edges    = DECORATION_EDGE_TOP;",
        "info.priority = *PPRIORITY;",
        "info.reserved = true;",
        "if (visible()) {",
        "if (*PSTACKED) {",
        "const auto ONEBARHEIGHT = *POUTERGAP + *PINDICATORHEIGHT + *PINDICATORGAP + (*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0);",
        "info.desiredExtents     = {{0, (ONEBARHEIGHT * m_dwGroupMembers.size()) + (*PKEEPUPPERGAP * *POUTERGAP)}, {0, 0}};",
        "info.desiredExtents = {{0, *POUTERGAP * (1 + *PKEEPUPPERGAP) + *PINDICATORHEIGHT + *PINDICATORGAP + (*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0)}, {0, 0}};",
        "info.desiredExtents = {{0, 0}, {0, 0}};",
        "void CHyprGroupBarDecoration::draw(PHLMONITOR pMonitor, float const& a) {",
        "if (!VISIBLE)",
        "return;",
        'static auto PRENDERTITLES              = CConfigValue<Config::INTEGER>("group:groupbar:render_titles");',
        'static auto PTITLEFONTSIZE             = CConfigValue<Config::INTEGER>("group:groupbar:font_size");',
        'static auto PROUNDING                  = CConfigValue<Config::INTEGER>("group:groupbar:rounding");',
        'static auto PROUNDINGPOWER             = CConfigValue<Config::FLOAT>("group:groupbar:rounding_power");',
        'static auto PGRADIENTROUNDING          = CConfigValue<Config::INTEGER>("group:groupbar:gradient_rounding");',
        'static auto PGRADIENTROUNDINGPOWER     = CConfigValue<Config::FLOAT>("group:groupbar:gradient_rounding_power");',
        'static auto PGRADIENTROUNDINGONLYEDGES = CConfigValue<Config::INTEGER>("group:groupbar:gradient_round_only_edges");',
        'static auto PROUNDONLYEDGES            = CConfigValue<Config::INTEGER>("group:groupbar:round_only_edges");',
        'static auto PINNERGAP                  = CConfigValue<Config::INTEGER>("group:groupbar:gaps_in");',
        'static auto PTEXTOFFSET                = CConfigValue<Config::INTEGER>("group:groupbar:text_offset");',
        'static auto PTEXTPADDING               = CConfigValue<Config::INTEGER>("group:groupbar:text_padding");',
        'static auto PBLUR                      = CConfigValue<Config::INTEGER>("group:groupbar:blur");',
        "const auto  ONEBARHEIGHT = *POUTERGAP + *PINDICATORHEIGHT + *PINDICATORGAP + (*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0);",
        "m_barWidth               = *PSTACKED ? ASSIGNEDBOX.w : (ASSIGNEDBOX.w - *PINNERGAP * (barsToDraw - 1)) / barsToDraw;",
        "m_barHeight              = *PSTACKED ? ((ASSIGNEDBOX.h - *POUTERGAP * *PKEEPUPPERGAP) - *POUTERGAP * (barsToDraw)) / barsToDraw : ASSIGNEDBOX.h - *POUTERGAP * *PKEEPUPPERGAP;",
        "const auto DESIREDHEIGHT = *PSTACKED ? (ONEBARHEIGHT * m_dwGroupMembers.size()) + *POUTERGAP * *PKEEPUPPERGAP : *POUTERGAP * (1 + *PKEEPUPPERGAP) + ONEBARHEIGHT;",
        "bool  blur = *PBLUR != 0;",
        "const auto WINDOWINDEX = *PSTACKED ? m_dwGroupMembers.size() - i - 1 : i;",
        "CHyprColor        color = m_dwGroupMembers[WINDOWINDEX].lock() == Desktop::focusState()->window() ? PCOLACTIVE->m_colors[0] : PCOLINACTIVE->m_colors[0];",
        "rectdata.blur  = blur;",
        "if (*PROUNDING) {",
        "rectdata.roundingPower = *PROUNDINGPOWER;",
        "if (*PROUNDONLYEDGES && barsToDraw > 1) {",
        "(*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0)};",
        "if (*PGRADIENTS) {",
        "data.blur = blur;",
        "if (*PGRADIENTROUNDING) {",
        "data.roundingPower = *PGRADIENTROUNDINGPOWER;",
        "if (*PGRADIENTROUNDINGONLYEDGES && barsToDraw > 1) {",
        "if (*PRENDERTITLES) {",
        "Vector2D{(m_barWidth - (*PTEXTPADDING * 2)) * pMonitor->m_scale, (*PTITLEFONTSIZE + 2L * BAR_TEXT_PAD) * pMonitor->m_scale}",
        "rect.y += std::ceil(((rect.height - titleTex->m_size.y) / 2.0) - (*PTEXTOFFSET * pMonitor->m_scale));",
        "rect.x += std::round((((m_barWidth + *PTEXTPADDING) * pMonitor->m_scale) / 2.0) - ((titleTex->m_size.x + *PTEXTPADDING) / 2.0));",
        "if (*PSTACKED)",
        "yoff += ONEBARHEIGHT;",
        "xoff += *PINNERGAP + m_barWidth;",
        "if (*PRENDERTITLES)",
        "invalidateTextures();",
        'static auto      FALLBACKFONT             = CConfigValue<std::string>("misc:font_family");',
        'static auto      PTITLEFONTFAMILY         = CConfigValue<std::string>("group:groupbar:font_family");',
        'static auto      PTITLEFONTWEIGHTACTIVE   = CConfigValue<Config::IComplexConfigValue>("group:groupbar:font_weight_active");',
        'static auto      PTITLEFONTWEIGHTINACTIVE = CConfigValue<Config::IComplexConfigValue>("group:groupbar:font_weight_inactive");',
        "const auto       FONTFAMILY = *PTITLEFONTFAMILY != STRVAL_EMPTY ? *PTITLEFONTFAMILY : *FALLBACKFONT;",
        "#define RENDER_TEXT(color, weight) g_pHyprRenderer->renderText(pWindow->m_title, (color), *PTITLEFONTSIZE* monitorScale, false, FONTFAMILY, bufferSize.x - 2, (weight));",
        "m_texActive         = RENDER_TEXT(COLORACTIVE, FONTWEIGHTACTIVE->m_value);",
        "m_texInactive       = RENDER_TEXT(COLORINACTIVE, FONTWEIGHTINACTIVE->m_value);",
        "void refreshGroupBarGradients() {",
        'CConfigValue<Config::IComplexConfigValue>("group:groupbar:col.active")',
        'CConfigValue<Config::IComplexConfigValue>("group:groupbar:col.inactive")',
        'CConfigValue<Config::IComplexConfigValue>("group:groupbar:col.locked_active")',
        'CConfigValue<Config::IComplexConfigValue>("group:groupbar:col.locked_inactive")',
        "if (!*PENABLED || !*PGRADIENTS)",
        "m_tGradientActive         = renderGradient(GROUPCOLACTIVE);",
        "bool CHyprGroupBarDecoration::onMouseButtonOnDeco",
        'static auto PMIDDLECLICKCLOSE = CConfigValue<Config::INTEGER>("group:groupbar:middle_click_close");',
        "if (e.button == 274) {",
        "if (!*PMIDDLECLICKCLOSE)",
        "if (e.state == WL_POINTER_BUTTON_STATE_PRESSED)",
        "else if (e.state == WL_POINTER_BUTTON_STATE_RELEASED && pressedCursorPos == pos)",
        "g_pXWaylandManager->sendCloseWindow(m_window->m_group->fromIndex(WINDOWINDEX));",
        "bool CHyprGroupBarDecoration::onScrollOnDeco",
        'static auto PGROUPBARSCROLLING = CConfigValue<Config::INTEGER>("group:groupbar:scrolling");',
        "if (!*PGROUPBARSCROLLING || !m_window->m_group)",
        "m_window->m_group->moveCurrent(true);",
        "m_window->m_group->moveCurrent(false);",
        "uint64_t CHyprGroupBarDecoration::getDecorationFlags() {",
        "return DECORATION_ALLOWS_MOUSE_INPUT;",
    )
    font_weight_runtime = (
        "SParseError CLuaConfigFontWeight::parse(lua_State* s) {",
        "if (lua_isinteger(s, -1) || lua_isnumber(s, -1)) {",
        "int64_t v = sc<int64_t>(lua_tonumber(s, -1));",
        "if (v < 0)",
        "m_data.m_value = v;",
        "m_bSetByUser   = true;",
        "if (lua_isstring(s, -1)) {",
        "auto it = CFontWeightConfigValueData::WEIGHTS.find(lc);",
        "if (Hyprutils::String::isNumber(str))",
        'return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "font weight type requires a number or a weight name string"};',
        "void CLuaConfigFontWeight::push(lua_State* s) {",
        "lua_pushinteger(s, m_data.m_value);",
    )

    return {
        "0.55.0": {
            REGISTRY_PATH: (
                'MS<Bool>("group:groupbar:enabled", "enables groupbars", true),',
                *registry_common,
            ),
            Path("src/desktop/view/Group.cpp"): group_lifecycle,
            Path("src/render/decorations/CHyprGroupBarDecoration.cpp"): (
                'static auto PGRADIENTS = CConfigValue<Config::INTEGER>("group:groupbar:enabled");',
                'static auto PENABLED   = CConfigValue<Config::INTEGER>("group:groupbar:gradients");',
                "if (*PENABLED && *PGRADIENTS)",
                *decoration_runtime,
                "bool CHyprGroupBarDecoration::visible() {",
                'static auto PENABLED = CConfigValue<Config::INTEGER>("group:groupbar:enabled");',
                "return *PENABLED && m_window->m_ruleApplicator->decorate().valueOrDefault();",
            ),
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): (
                "if (m_propsTripped & REFRESH_WINDOW_STATES) {",
                "Desktop::Rule::ruleEngine()->updateAllRules();",
                "ws->updateWindows();",
                "ws->updateWindowData();",
                "ws->updateWindowDecos();",
                "g_pCompositor->updateAllWindowsAnimatedDecorationValues();",
                "if (m_propsTripped & REFRESH_GRADIENTS_GROUPBAR)",
                "refreshGroupBarGradients();",
            ),
            Path("src/config/lua/types/LuaConfigFontWeight.cpp"): font_weight_runtime,
        },
        "0.56.1": {
            REGISTRY_PATH: (
                'MS<Bool>("group:groupbar:enabled", "enables groupbars", true, {.refresh = Supplementary::REFRESH_WINDOW_STATES}),',
                'MS<Bool>("group:groupbar:disable_when_only", "disable if contains single window. Considered only if enabled == true", false,',
                "{.refresh = Supplementary::REFRESH_WINDOW_STATES}),",
                *registry_common,
            ),
            Path("src/desktop/view/Group.cpp"): group_lifecycle,
            Path("src/render/decorations/CHyprGroupBarDecoration.cpp"): (
                'static auto PENABLED   = CConfigValue<Config::INTEGER>("group:groupbar:enabled");',
                'static auto PGRADIENTS = CConfigValue<Config::INTEGER>("group:groupbar:gradients");',
                "if (*PENABLED && *PGRADIENTS)",
                *decoration_runtime,
                "bool CHyprGroupBarDecoration::visible() {",
                'static auto PENABLED = CConfigValue<Config::BOOL>("group:groupbar:enabled");',
                'static auto PDISABLE = CConfigValue<Config::BOOL>("group:groupbar:disable_when_only");',
                "return *PENABLED && (!*PDISABLE || m_dwGroupMembers.size() > 1) && m_window->m_ruleApplicator->decorate().valueOrDefault();",
            ),
            Path("src/config/supplementary/propRefresher/PropRefresher.cpp"): (
                "if (m_propsTripped & REFRESH_WINDOW_STATES) {",
                "Desktop::Rule::ruleEngine()->updateAllRules();",
                "for (auto const& w : Desktop::windowState()->windows())",
                "w->uncacheWindowDecos();",
                "ws->updateWindows();",
                "ws->updateWindowData();",
                "ws->updateWindowDecos();",
                "Desktop::globalWindowController()->updateAllWindowsDecorations();",
                "if (m_propsTripped & REFRESH_GRADIENTS_GROUPBAR)",
                "refreshGroupBarGradients();",
            ),
            Path("src/config/lua/types/LuaConfigFontWeight.cpp"): font_weight_runtime,
        },
    }


def _assert_group_bar_contract(
    group_bar_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify groupbar visibility, geometry, interaction, and typography."""
    requirements_by_version = _group_bar_contract_requirements()
    if set(requirements_by_version) != {"0.55.0", "0.56.1"}:
        raise ValueError("groupbar patch inventory is incomplete")

    for version in ("0.55.0", "0.56.1"):
        requirements = requirements_by_version[version]
        if tuple(requirements) != GROUP_BAR_SOURCE_PATHS:
            raise ValueError(
                f"Hyprland {version} groupbar semantic inventory is incomplete"
            )
        sources = group_bar_sources.get(version, {})
        if set(sources) != set(GROUP_BAR_SOURCE_PATHS):
            raise ValueError(
                f"Hyprland {version} groupbar source inventory is incomplete"
            )
        expected_registry_paths = GROUP_BAR_REGISTRY_PATHS_0561
        if version == "0.55.0":
            expected_registry_paths = tuple(
                path
                for path in GROUP_BAR_REGISTRY_PATHS_0561
                if path != "group:groupbar:disable_when_only"
            )
        try:
            registry_options = extract_raw_options(
                sources[REGISTRY_PATH].decode("utf-8")
            )
        except ValueError as error:
            raise ValueError(
                f"Hyprland {version} groupbar registry changed in "
                f"{REGISTRY_PATH.name}: {error}"
            ) from error
        registry_paths = tuple(
            option.path
            for option in registry_options
            if option.path.startswith("group:groupbar:")
        )
        if registry_paths != expected_registry_paths:
            raise ValueError(
                f"Hyprland {version} groupbar registry inventory changed in "
                f"{REGISTRY_PATH.name}"
            )
        reviewed_fragments = " ".join(
            fragment
            for path in GROUP_BAR_SOURCE_PATHS
            for fragment in requirements[path]
        )
        for option_path in GROUP_BAR_OPTION_PATHS:
            if version == "0.55.0" and option_path == "group:groupbar:disable_when_only":
                continue
            if f'"{option_path}"' not in reviewed_fragments:
                raise ValueError(
                    f"Hyprland {version} has no groupbar assertion for {option_path}"
                )
        for path in GROUP_BAR_SOURCE_PATHS:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(
                    re.sub(r"\s+", " ", fragment)
                    for fragment in requirements[path]
                ),
                "groupbar",
            )


def _animation_contract_requirements(
) -> dict[str, dict[Path, tuple[str, ...]]]:
    lua_common = (
        "static int hlCurve(lua_State* L) {",
        'CLuaConfigString nameParser("");',
        'auto typeErr = Internal::parseTableField(L, 2, "type", typeParser);',
        'if (curveType == "bezier") {',
        'lua_getfield(L, 2, "points");',
        "if (luaL_len(L, pointsIdx) != 2) {",
        "if (!lua_istable(L, -1) || luaL_len(L, -1) != 2) {",
        "CLuaConfigFloat coordParser(0.F, -1.F, 2.F);",
    )
    lua_spring = (
        '} else if (curveType == "spring") {',
        'lua_getfield(L, 2, "stiffness");',
        "if (curve.stiffness <= 0.5F)",
        'lua_getfield(L, 2, "dampening");',
        "if (curve.damping <= 0.5F)",
        'lua_getfield(L, 2, "mass");',
        "if (curve.mass <= 0.5F)",
    )
    lua_animation = (
        "static int hlAnimation(lua_State* L) {",
        'auto leafErr = Internal::parseTableField(L, 1, "leaf", leafParser);',
        "if (!Config::animationTree()->nodeExists(leaf))",
        'auto enabledErr = Internal::parseTableField(L, 1, "enabled", enabledParser);',
        'Config::animationTree()->setConfigForNode(leaf, false, 1, "default");',
        "CLuaConfigFloat speedParser(0.F, 0.F, 100.F);",
        "if (speed <= 0)",
        'if (Internal::hasTableField(L, 1, "bezier")) {',
    )
    lua_refs = (
        'curveName = "spring:" + springName;',
        'lua_getfield(L, 1, "style");',
        "if (!style.empty()) {",
        "Config::animationTree()->setConfigForNode(leaf, true, speed, curveName, style);",
        'Internal::setFn(L, "curve", hlCurve);',
        'Internal::setFn(L, "animation", hlAnimation);',
    )
    manager_style_common = (
        "std::string CHyprAnimationManager::styleValidInConfigVar(const std::string& config, const std::string& style) {",
        'if (config.starts_with("window")) {',
        'if (style.starts_with("slide") || style == "gnome" || style == "gnomed")',
        'else if (style.starts_with("popin")) {',
        '} else if (config.starts_with("workspaces") || config.starts_with("specialWorkspace")) {',
        'if (style == "slide" || style == "slidevert" || style == "fade")',
        'else if (style.starts_with("slide")) {',
    )
    manager_header = (
        "class CHyprAnimationManager : public Hyprutils::Animation::CAnimationManager {",
        "using SAnimationPropertyConfig = Hyprutils::Animation::SAnimationPropertyConfig;",
        "std::string styleValidInConfigVar(const std::string&, const std::string&);",
    )
    lua_reload = (
        "void CConfigManager::reload() {",
        "Config::animationTree()->reset();",
        "reinitLuaState();",
        'if (guardedPCall(0, 0, 1, LUA_TIMEOUT_CONFIG_RELOAD_MS, "config reload") != LUA_OK) {',
        "postConfigReload();",
    )

    lua_055 = (
        *lua_common,
        "g_pAnimationManager->addBezierWithName(name, Vector2D(coords[0], coords[1]), Vector2D(coords[2], coords[3]));",
        *lua_spring,
        "g_pAnimationManager->addSpringWithName(name, curve);",
        *lua_animation,
        "if (!g_pAnimationManager->bezierExists(bezierName))",
        '} else if (Internal::hasTableField(L, 1, "spring")) {',
        "if (!g_pAnimationManager->springExists(springName))",
        *lua_refs[:-3],
        "auto err = g_pAnimationManager->styleValidInConfigVar(leaf, style);",
        *lua_refs[-3:],
    )
    lua_056 = (
        *lua_common,
        "Animation::mgr()->addBezierWithName(name, Vector2D(coords[0], coords[1]), Vector2D(coords[2], coords[3]));",
        *lua_spring,
        "Animation::mgr()->addSpringWithName(name, curve);",
        *lua_animation,
        "if (!Animation::mgr()->bezierExists(bezierName))",
        '} else if (Internal::hasTableField(L, 1, "spring")) {',
        "if (!Animation::mgr()->springExists(springName))",
        *lua_refs[:-3],
        "auto err = Animation::mgr()->styleValidInConfigVar(leaf, style);",
        *lua_refs[-3:],
    )

    return {
        "0.55.0": {
            Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): lua_055,
            Path("src/config/lua/ConfigManager.cpp"): lua_reload,
            Path("src/config/shared/animation/AnimationTree.cpp"): (
                "void CAnimationTreeController::reset() {",
                'm_animationTree.createNode("__internal_fadeCTM");',
                'm_animationTree.createNode("specialWorkspaceOut", "specialWorkspace");',
                'm_animationTree.setConfigForNode("global", 1, 8.f, "default");',
                'm_animationTree.setConfigForNode("__internal_fadeCTM", 1, 5.f, "linear");',
                "void CAnimationTreeController::setConfigForNode(const std::string& name, bool enabled, float speed, const std::string& bezier, const std::string& style) {",
                "m_animationTree.setConfigForNode(name, enabled, speed, bezier, style);",
                "bool CAnimationTreeController::nodeExists(const std::string& name) {",
                "return m_animationTree.nodeExists(name);",
            ),
            Path("src/managers/animation/AnimationManager.hpp"): manager_header,
            Path("src/managers/animation/AnimationManager.cpp"): (
                "CHyprAnimationManager::CHyprAnimationManager() {",
                'addBezierWithName("linear", Vector2D(0.0, 0.0), Vector2D(1.0, 1.0));',
                *manager_style_common,
                '} else if (config == "borderangle") {',
                'if (style == "loop" || style == "once")',
                '} else if (config.starts_with("layers")) {',
                'if (style.empty() || style == "fade" || style == "slide")',
                'else if (style.starts_with("popin")) {',
            ),
            Path("src/managers/animation/DesktopAnimationManager.cpp"): (
                'pWindow->m_realPosition->setConfig(Config::animationTree()->getAnimationPropertyConfig("windowsIn"));',
                'if (STYLE.starts_with("slide")) {',
                'else if (STYLE == "gnomed" || STYLE == "gnome")',
                "animationPopin(pWindow, CLOSE, minPerc / 100.f);",
                'const auto ANIMSTYLE = ls->m_ruleApplicator->animationStyle().valueOr(ls->m_realPosition->getStyle());',
                'if (ANIMSTYLE.starts_with("slide")) {',
                '} else if (ANIMSTYLE.starts_with("popin")) {',
                "const auto ANIMSTYLE = style.value_or(ws->m_alpha->getStyle());",
                'bool vert = ANIMSTYLE.starts_with("slidevert") || ANIMSTYLE.starts_with("slidefadevert");',
                'if (ANIMSTYLE.starts_with("slidefade")) {',
                '} else if (ANIMSTYLE == "fade") {',
            ),
            Path("src/desktop/view/Window.cpp"): (
                "void CWindow::onBorderAngleAnimEnd(WP<CBaseAnimatedVariable> pav) {",
                'if (pav->getStyle() != "loop" || !pav->enabled())',
                "PANIMVAR->setValueAndWarp(0);",
                "*PANIMVAR = 1.f;",
            ),
        },
        "0.56.1": {
            Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp"): lua_056,
            Path("src/config/lua/ConfigManager.cpp"): lua_reload,
            Path("src/config/shared/animation/AnimationTree.cpp"): (
                "void CAnimationTreeController::reset() {",
                'm_animationTree.createNode("__internal_fadeCTM");',
                'm_animationTree.createNode("specialWorkspaceOut", "specialWorkspace");',
                'm_animationTree.setConfigForNode("global", 1, 8.f, "default");',
                'm_animationTree.setConfigForNode("__internal_fadeCTM", 1, 5.f, "linear");',
                "void CAnimationTreeController::setConfigForNode(const std::string& name, bool enabled, float speed, const std::string& bezier, const std::string& style) {",
                "m_animationTree.setConfigForNode(name, enabled, speed, bezier, style);",
                "bool CAnimationTreeController::nodeExists(const std::string& name) {",
                "return m_animationTree.nodeExists(name);",
            ),
            Path("src/animation/AnimationManager.hpp"): manager_header,
            Path("src/animation/AnimationManager.cpp"): (
                "CHyprAnimationManager::CHyprAnimationManager() {",
                'addBezierWithName("linear", Vector2D(0.0, 0.0), Vector2D(1.0, 1.0));',
                *manager_style_common,
                '} else if (config.ends_with("angle")) {',
                'if (style == "loop" || style == "once")',
                '} else if (config.starts_with("layers")) {',
                'if (style.empty() || style == "fade" || style.starts_with("slide"))',
                'else if (style.starts_with("popin")) {',
            ),
            Path("src/desktop/view/animationControllers/WindowAnimationController.cpp"): (
                "static void applyWindowStyle(Animation::SViewAnimationContext& ctx, CWindow* window, const bool close) {",
                "std::string animStyle = window->positionAnimation()->getStyle();",
                'if (STYLE.starts_with("slide")) {',
                'else if (STYLE == "gnomed" || STYLE == "gnome")',
                "applyPopin(ctx, close, percentageFromStyle(STYLE));",
                'if (animList[0] == "slide")',
                'else if (animList[0] == "gnomed" || animList[0] == "gnome")',
                "applyPopin(ctx, close, percentageFromStyle(animStyle, true));",
            ),
            Path("src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"): (
                "Animation::SViewAnimationContext CLayerSurfaceAnimationController::animateIn() const {",
                "const auto ANIMSTYLE = m_parent->m_ruleApplicator->animationStyle().valueOr(m_parent->positionAnimation()->getStyle());",
                'if (ANIMSTYLE.starts_with("slide"))',
                'else if (ANIMSTYLE.starts_with("popin"))',
                "Animation::SViewAnimationContext CLayerSurfaceAnimationController::animateOut() const {",
                "const auto ANIMSTYLE = m_parent->m_ruleApplicator->animationStyle().valueOr(m_parent->positionAnimation()->getStyle());",
                'if (ANIMSTYLE.starts_with("slide"))',
                'else if (ANIMSTYLE.starts_with("popin"))',
            ),
            Path("src/animation/WorkspaceAnimationController.cpp"): (
                "void Animation::Workspace::startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left, bool instant, std::optional<std::string> style) {",
                "const auto ANIMSTYLE = style.value_or(ws->m_alpha->getStyle());",
                'bool vert = ANIMSTYLE.starts_with("slidevert") || ANIMSTYLE.starts_with("slidefadevert");',
                'if (ANIMSTYLE.starts_with("slidefade")) {',
                '} else if (ANIMSTYLE == "fade") {',
            ),
            Path("src/desktop/view/Window.cpp"): (
                "void CWindow::onBorderAngleAnimEnd(WP<CBaseAnimatedVariable> pav) {",
                'if (pav->getStyle() != "loop" || !pav->enabled())',
                "void CWindow::onShadowAngleAnimEnd(WP<CBaseAnimatedVariable> pav) {",
                'if (pav->getStyle() != "loop" || !pav->enabled())',
                "void CWindow::onGlowAngleAnimEnd(WP<CBaseAnimatedVariable> pav) {",
                'if (pav->getStyle() != "loop" || !pav->enabled())',
            ),
        },
    }


def _extract_animation_tree_nodes(source: str) -> tuple[tuple[str, str | None], ...]:
    normalized = re.sub(r"\s+", " ", source)
    return tuple(
        (match.group(1), match.group(2))
        for match in re.finditer(
            r'm_animationTree\.createNode\("([^"]+)"(?:, "([^"]+)")?\);',
            normalized,
        )
    )


def _extract_animation_tree_defaults(
    source: str,
) -> tuple[tuple[str, bool, str, str], ...]:
    normalized = re.sub(r"\s+", " ", source)
    return tuple(
        (match.group(1), match.group(2) == "1", match.group(3).strip(), match.group(4))
        for match in re.finditer(
            r'm_animationTree\.setConfigForNode\("([^"]+)", ([01]), ([^,]+), "([^"]+)"\);',
            normalized,
        )
    )


def _assert_animation_contract(
    animation_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify the coupled curve/animation grammar and runtime consumers."""
    requirements_by_version = _animation_contract_requirements()
    if set(requirements_by_version) != set(ANIMATION_SOURCE_PATHS):
        raise ValueError("animation patch inventory is incomplete")

    for version, expected_paths in ANIMATION_SOURCE_PATHS.items():
        requirements = requirements_by_version[version]
        if tuple(requirements) != expected_paths:
            raise ValueError(
                f"Hyprland {version} animation semantic inventory is incomplete"
            )
        sources = animation_sources.get(version, {})
        if set(sources) != set(expected_paths):
            raise ValueError(
                f"Hyprland {version} animation source inventory is incomplete"
            )

        tree_path = Path("src/config/shared/animation/AnimationTree.cpp")
        tree_source = sources[tree_path].decode("utf-8")
        actual_nodes = _extract_animation_tree_nodes(tree_source)
        if actual_nodes != ANIMATION_TREE_NODES[version]:
            raise ValueError(
                f"Hyprland {version} animation tree leaves/inheritance changed in "
                f"{tree_path.name}"
            )
        actual_defaults = _extract_animation_tree_defaults(tree_source)
        if actual_defaults != ANIMATION_TREE_DEFAULTS[version]:
            raise ValueError(
                f"Hyprland {version} animation tree built-in references changed in "
                f"{tree_path.name}"
            )

        lua_path = Path("src/config/lua/bindings/LuaBindingsConfigRules.cpp")
        lua_source = sources[lua_path].decode("utf-8")
        curve_start = lua_source.find("static int hlCurve(lua_State* L) {")
        curve_end = lua_source.find("static int hlAnimation(lua_State* L) {")
        if curve_start < 0 or curve_end <= curve_start:
            raise ValueError(
                f"Hyprland {version} curve registration boundary changed in "
                f"{lua_path.name}"
            )
        curve_body = lua_source[curve_start:curve_end]
        if (
            '"default"' in curve_body
            or '"linear"' in curve_body
            or "bezierExists(name)" in curve_body
            or "springExists(name)" in curve_body
        ):
            raise ValueError(
                f"Hyprland {version} custom curve shadowing semantics changed in "
                f"{lua_path.name}"
            )

        config_path = Path("src/config/lua/ConfigManager.cpp")
        config_source = sources[config_path].decode("utf-8")
        reload_start = config_source.find("void CConfigManager::reload() {")
        reload_end = config_source.find(
            "void CConfigManager::postConfigReload() {", reload_start
        )
        if reload_start < 0 or reload_end <= reload_start:
            raise ValueError(
                f"Hyprland {version} Lua reload boundary changed in "
                f"{config_path.name}"
            )
        reload_body = config_source[reload_start:reload_end]
        if "removeAllBeziers" in reload_body or "removeAllSprings" in reload_body:
            raise ValueError(
                f"Hyprland {version} Lua curve reset semantics changed in "
                f"{config_path.name}"
            )

        for path in expected_paths:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(re.sub(r"\s+", " ", fragment) for fragment in requirements[path]),
                "animation",
            )


def _animation_dependency_contract_requirements() -> dict[Path, tuple[str, ...]]:
    header_path, manager_path, dispatch_path = HYPRUTILS_ANIMATION_SOURCE_PATHS
    return {
        header_path: (
            "void addBezierWithName(std::string, const Math::Vector2D&, const Math::Vector2D&);",
            "void removeAllBeziers();",
            "void addSpringWithName(std::string, const SSpringCurve&);",
            "void removeAllSprings();",
            "std::unordered_map<std::string, Memory::CSharedPointer<CBezierCurve>> m_mBezierCurves;",
            "std::unordered_map<std::string, Memory::CSharedPointer<SSpringCurve>> m_mSpringCurves;",
        ),
        manager_path: (
            "const std::array<Vector2D, 2> DEFAULTBEZIERPOINTS = {Vector2D(0.0, 0.75), Vector2D(0.15, 1.0)};",
            "const SSpringCurve DEFAULTSPRING = {};",
            "CAnimationManager::CAnimationManager() {",
            'm_mBezierCurves["default"] = BEZIER;',
            'm_mSpringCurves["default"] = makeShared<SSpringCurve>(DEFAULTSPRING);',
            "void CAnimationManager::removeAllBeziers() {",
            "m_mBezierCurves.clear();",
            'm_mBezierCurves["default"] = BEZIER;',
            "void CAnimationManager::removeAllSprings() {",
            "m_mSpringCurves.clear();",
            'm_mSpringCurves["default"] = makeShared<SSpringCurve>(DEFAULTSPRING);',
            "void CAnimationManager::addBezierWithName(std::string name, const Vector2D& p1, const Vector2D& p2) {",
            "m_mBezierCurves[name] = BEZIER;",
            "void CAnimationManager::addSpringWithName(std::string name, const SSpringCurve& spring) {",
            "m_mSpringCurves[name] = makeShared<SSpringCurve>(spring);",
        ),
        dispatch_path: (
            'static constexpr std::string_view SPRINGPREFIX = "spring:";',
            "if (isSpringCurve())",
            "m_pAnimationManager->getBezier(getBezierName());",
            "if (!isSpringCurve()) {",
            "m_pAnimationManager->getBezier(getBezierName());",
            "const auto SPRINGNAME = springNameFromSpec(getBezierName());",
            "m_pAnimationManager->getSpring(std::string{SPRINGNAME});",
            "bool CBaseAnimatedVariable::isSpringCurve() const {",
            "return !springNameFromSpec(getBezierName()).empty();",
        ),
    }


def _assert_animation_dependency_contract(
    flake_locks: dict[str, bytes],
    dependency_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify the exact Hyprutils curve storage selected by each Hyprland tag."""
    if set(flake_locks) != set(ANIMATION_DEPENDENCY_SOURCES) or set(
        dependency_sources
    ) != set(ANIMATION_DEPENDENCY_SOURCES):
        raise ValueError("animation dependency source inventory is incomplete")

    for version, dependency in ANIMATION_DEPENDENCY_SOURCES.items():
        lock = _strict_json(flake_locks[version], f"Hyprland {version} flake.lock")
        if not isinstance(lock, dict):
            raise ValueError(f"Hyprland {version} flake.lock root changed")
        root_name = lock.get("root")
        nodes = lock.get("nodes")
        if (
            not isinstance(root_name, str)
            or not isinstance(nodes, dict)
            or not isinstance(nodes.get(root_name), dict)
            or nodes[root_name].get("inputs", {}).get("hyprutils") != "hyprutils"
        ):
            raise ValueError(
                f"Hyprland {version} root Hyprutils dependency binding changed"
            )
        locked = nodes.get("hyprutils", {}).get("locked")
        expected_locked = {
            "owner": "hyprwm",
            "repo": "hyprutils",
            "rev": dependency["revision"],
            "type": "github",
        }
        if not isinstance(locked, dict) or any(
            locked.get(key) != value for key, value in expected_locked.items()
        ):
            raise ValueError(
                f"Hyprland {version} pinned Hyprutils revision changed"
            )

        sources = dependency_sources[version]
        if set(sources) != set(HYPRUTILS_ANIMATION_SOURCE_PATHS):
            raise ValueError(
                f"Hyprutils for Hyprland {version} animation source inventory "
                "is incomplete"
            )
        requirements = _animation_dependency_contract_requirements()
        for path in HYPRUTILS_ANIMATION_SOURCE_PATHS:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                requirements[path],
                "Hyprutils animation",
            )


def _input_behavior_dependency_contract_requirements(
) -> dict[Path, tuple[str, ...]]:
    vector_path, box_path = HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS
    return {
        vector_path: (
            "constexpr Vector2D operator+(const Vector2D& a) const {",
            "return Vector2D(this->x + a.x, this->y + a.y);",
            "constexpr Vector2D operator*(const Vector2D& a) const {",
            "return Vector2D(this->x * a.x, this->y * a.y);",
            "constexpr bool operator<(const Vector2D& a) const {",
            "return this->x < a.x && this->y < a.y;",
        ),
        box_path: (
            "#define VECINRECT(vec, x1, y1, x2, y2) ((vec).x >= (x1) && (vec).x < (x2) && (vec).y >= (y1) && (vec).y < (y2))",
            "constexpr double HALF = 0.5;",
            "constexpr double EPSILON = 1e-9;",
            "CBox& Hyprutils::Math::CBox::translate(const Vector2D& vec) {",
            "x += vec.x; y += vec.y;",
            "return *this;",
            "Vector2D Hyprutils::Math::CBox::middle() const {",
            "return Vector2D{x + (w * HALF), y + (h * HALF)};",
            "bool Hyprutils::Math::CBox::containsPoint(const Vector2D& vec) const {",
            "return VECINRECT(vec, x, y, x + w, y + h);",
            "bool Hyprutils::Math::CBox::empty() const {",
            "return std::fabs(w) < EPSILON || std::fabs(h) < EPSILON;",
            "CBox& Hyprutils::Math::CBox::scaleFromCenter(double scale) {",
            "double oldW = w, oldH = h;",
            "w *= scale;",
            "h *= scale;",
            "x -= (w - oldW) * HALF;",
            "y -= (h - oldH) * HALF;",
            "return *this;",
            "bool Hyprutils::Math::CBox::inside(const CBox& bound) const {",
            "return bound.x < x && bound.y < y && x + w < bound.x + bound.w && y + h < bound.y + bound.h;",
            "Vector2D Hyprutils::Math::CBox::closestPoint(const Vector2D& vec) const {",
            "if (containsPoint(vec)) return vec;",
            "Vector2D nv = vec; Vector2D maxPoint = {x + w - EPSILON, y + h - EPSILON};",
            "if (x < maxPoint.x) nv.x = std::clamp(nv.x, x, maxPoint.x); else nv.x = x;",
            "if (y < maxPoint.y) nv.y = std::clamp(nv.y, y, maxPoint.y); else nv.y = y;",
            "if (std::fabs(nv.x - x) < EPSILON) nv.x = x; else if (std::fabs(nv.x - (maxPoint.x)) < EPSILON) nv.x = maxPoint.x;",
            "if (std::fabs(nv.y - y) < EPSILON) nv.y = y; else if (std::fabs(nv.y - (maxPoint.y)) < EPSILON) nv.y = maxPoint.y;",
            "return nv;",
        ),
    }


def _assert_input_behavior_dependency_contract(
    flake_locks: dict[str, bytes],
    dependency_sources: dict[str, dict[Path, bytes]],
) -> None:
    """Qualify Hyprutils geometry selected by each paired Hyprland tag."""
    if set(flake_locks) != set(INPUT_BEHAVIOR_DEPENDENCY_SOURCES) or set(
        dependency_sources
    ) != set(INPUT_BEHAVIOR_DEPENDENCY_SOURCES):
        raise ValueError("input behavior dependency source inventory is incomplete")

    requirements = _input_behavior_dependency_contract_requirements()
    if tuple(requirements) != HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS:
        raise ValueError("input behavior dependency semantic inventory is incomplete")

    for version, dependency in INPUT_BEHAVIOR_DEPENDENCY_SOURCES.items():
        lock = _strict_json(flake_locks[version], f"Hyprland {version} flake.lock")
        if not isinstance(lock, dict):
            raise ValueError(f"Hyprland {version} flake.lock root changed")
        root_name = lock.get("root")
        nodes = lock.get("nodes")
        if (
            not isinstance(root_name, str)
            or not isinstance(nodes, dict)
            or not isinstance(nodes.get(root_name), dict)
            or nodes[root_name].get("inputs", {}).get("hyprutils") != "hyprutils"
        ):
            raise ValueError(
                f"Hyprland {version} root Hyprutils dependency binding changed"
            )
        locked = nodes.get("hyprutils", {}).get("locked")
        expected_locked = {
            "owner": "hyprwm",
            "repo": "hyprutils",
            "rev": dependency["revision"],
            "type": "github",
        }
        if not isinstance(locked, dict) or any(
            locked.get(key) != value for key, value in expected_locked.items()
        ):
            raise ValueError(
                f"Hyprland {version} pinned Hyprutils revision changed"
            )

        sources = dependency_sources[version]
        if set(sources) != set(HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS):
            raise ValueError(
                f"Hyprutils for Hyprland {version} input behavior source "
                "inventory is incomplete"
            )
        for path in HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS:
            _require_ordered_cpp_fragments(
                sources[path].decode("utf-8"),
                version,
                path,
                tuple(re.sub(r"\s+", " ", fragment) for fragment in requirements[path]),
                "Hyprutils input behavior",
            )


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
    workspace_rule_branches = definitions.get("workspaceRule", {}).get("oneOf")
    if not isinstance(workspace_rule_branches, list) or len(workspace_rule_branches) != 2:
        raise ValueError("workspaceRule is not the reviewed user/protected union")
    workspace_monitor = workspace_rule_branches[0].get("properties", {}).get("monitor")
    if workspace_monitor != {
        "oneOf": [{"const": ""}, {"$ref": "#/$defs/staticMonitorSelector"}],
    }:
        raise ValueError("workspace rule monitor is not a static selector")
    protected_workspace_properties = workspace_rule_branches[1].get("properties", {})
    if protected_workspace_properties != {
        "id": {"const": "hyprshelld.internal.shared-spacing.maximized"},
        "selector": {"const": "f[1]"},
        "enabled": {"const": True},
        "monitor": {"const": ""},
        "persistent": {"const": False},
        "isDefault": {"const": False},
        "layout": {"const": ""},
        "overrides": {"const": {"gaps_out": [0, 0, 0, 0]}},
    }:
        raise ValueError("protected maximize workspace rule schema changed")
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
        "0.55.0": {
            Path("VERSION"),
            FLAKE_LOCK_PATH,
            REGISTRY_PATH,
            *GROUP_BEHAVIOR_SOURCE_PATHS["0.55.0"],
            *APPEARANCE_BEHAVIOR_SOURCE_PATHS["0.55.0"],
            *ADVANCED_RUNTIME_SOURCE_PATHS["0.55.0"],
            *WINDOW_BEHAVIOR_SOURCE_PATHS["0.55.0"],
            *GROUP_BAR_SOURCE_PATHS,
            *WORKSPACE_BEHAVIOR_SOURCE_PATHS["0.55.0"],
            *BINDING_RUNTIME_SOURCE_PATHS["0.55.0"],
            *MISC_EXCLUSION_SOURCE_PATHS["0.55.0"],
            *INPUT_BEHAVIOR_SOURCE_PATHS["0.55.0"],
            *INPUT_DEVICE_SOURCE_PATHS["0.55.0"],
            *GESTURE_SOURCE_PATHS,
            *ANIMATION_SOURCE_PATHS["0.55.0"],
        },
        "0.56.0": {
            Path("VERSION"),
            *STARTUP_SOURCE_PATHS_0560,
            *MONITOR_QUERY_SOURCE_PATHS,
            *MAXIMIZE_SOURCE_PATHS,
            *ADVANCED_RUNTIME_SOURCE_PATHS["0.56.0"],
        },
        "0.56.1": {
            Path("VERSION"),
            FLAKE_LOCK_PATH,
            REGISTRY_PATH,
            *COMPLEX_SOURCE_PATHS,
            *MONITOR_QUERY_SOURCE_PATHS,
            *MAXIMIZE_SOURCE_PATHS,
            *GROUP_BEHAVIOR_SOURCE_PATHS["0.56.1"],
            *APPEARANCE_BEHAVIOR_SOURCE_PATHS["0.56.1"],
            *ADVANCED_RUNTIME_SOURCE_PATHS["0.56.1"],
            *WINDOW_BEHAVIOR_SOURCE_PATHS["0.56.1"],
            *GROUP_BAR_SOURCE_PATHS,
            *WORKSPACE_BEHAVIOR_SOURCE_PATHS["0.56.1"],
            *BINDING_RUNTIME_SOURCE_PATHS["0.56.1"],
            *MISC_EXCLUSION_SOURCE_PATHS["0.56.1"],
            *INPUT_BEHAVIOR_SOURCE_PATHS["0.56.1"],
            *INPUT_DEVICE_SOURCE_PATHS["0.56.1"],
            *GESTURE_SOURCE_PATHS,
            *ANIMATION_SOURCE_PATHS["0.56.1"],
        },
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

    if set(ANIMATION_DEPENDENCY_SOURCES) != {"0.55.0", "0.56.1"}:
        raise ValueError("animation dependency pin version set changed")
    for version, dependency in ANIMATION_DEPENDENCY_SOURCES.items():
        hashes = dependency.get("hashes")
        if not isinstance(hashes, dict) or set(hashes) != set(
            HYPRUTILS_ANIMATION_SOURCE_PATHS
        ):
            raise ValueError(
                f"Hyprutils for Hyprland {version} source hash coverage changed"
            )
        if not re.fullmatch(r"[0-9a-f]{40}", str(dependency.get("revision", ""))):
            raise ValueError(
                f"Hyprutils for Hyprland {version} has an invalid revision"
            )
        for path, digest in hashes.items():
            if not digest_expression.fullmatch(str(digest)):
                raise ValueError(
                    f"Hyprutils for Hyprland {version} has an invalid SHA-256 "
                    f"for {path}"
                )

    if set(INPUT_BEHAVIOR_DEPENDENCY_SOURCES) != {"0.55.0", "0.56.1"}:
        raise ValueError("input behavior dependency pin version set changed")
    for version, dependency in INPUT_BEHAVIOR_DEPENDENCY_SOURCES.items():
        hashes = dependency.get("hashes")
        if not isinstance(hashes, dict) or set(hashes) != set(
            HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS
        ):
            raise ValueError(
                f"Hyprutils for Hyprland {version} input behavior source hash "
                "coverage changed"
            )
        if not re.fullmatch(r"[0-9a-f]{40}", str(dependency.get("revision", ""))):
            raise ValueError(
                f"Hyprutils for Hyprland {version} input behavior has an "
                "invalid revision"
            )
        for path, digest in hashes.items():
            if not digest_expression.fullmatch(str(digest)):
                raise ValueError(
                    f"Hyprutils for Hyprland {version} input behavior has an "
                    f"invalid SHA-256 for {path}"
                )


def _source_tree_option_occurrences(
    source_root: Path,
    option_paths: tuple[str, ...],
) -> dict[str, dict[Path, int]]:
    """Count exact quoted option paths across the complete upstream src tree."""
    src_root = source_root / "src"
    if not src_root.is_dir():
        raise ValueError(f"Hyprland source tree has no src directory: {source_root}")
    needles = {
        option_path: f'"{option_path}"'.encode("utf-8")
        for option_path in option_paths
    }
    occurrences: dict[str, dict[Path, int]] = {
        option_path: {} for option_path in option_paths
    }
    for candidate in sorted(path for path in src_root.rglob("*") if path.is_file()):
        data = candidate.read_bytes()
        relative = candidate.relative_to(source_root)
        for option_path, needle in needles.items():
            count = data.count(needle)
            if count:
                occurrences[option_path][relative] = count
    return occurrences


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


def _read_animation_dependency_source(
    source_root: Path, version: str, path: Path
) -> bytes:
    dependency = ANIMATION_DEPENDENCY_SOURCES[version]
    expected = dependency["hashes"][path]
    data = (source_root / path).read_bytes()
    actual = _sha256(data)
    if actual != expected:
        raise ValueError(
            f"Hyprutils source provenance mismatch for Hyprland {version} at "
            f"{path}: reviewed {dependency['revision']} requires SHA-256 "
            f"{expected}, found {actual}"
        )
    return data


def _read_input_behavior_dependency_source(
    source_root: Path, version: str, path: Path
) -> bytes:
    dependency = INPUT_BEHAVIOR_DEPENDENCY_SOURCES[version]
    expected = dependency["hashes"][path]
    data = (source_root / path).read_bytes()
    actual = _sha256(data)
    if actual != expected:
        raise ValueError(
            f"Hyprutils input behavior source provenance mismatch for Hyprland "
            f"{version} at {path}: reviewed {dependency['revision']} requires "
            f"SHA-256 {expected}, found {actual}"
        )
    return data


def _json_bytes(document: Any) -> bytes:
    return (
        json.dumps(document, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    ).encode("utf-8")


def _default_state_json_bytes(document: dict[str, Any]) -> bytes:
    """Preserve the reviewed compact spelling of the protected zero-gap tuple."""
    expanded = (
        b'        "gaps_out": [\n'
        b"          0,\n"
        b"          0,\n"
        b"          0,\n"
        b"          0\n"
        b"        ]"
    )
    encoded = _json_bytes(document)
    if encoded.count(expanded) != 1:
        raise ValueError("default protected zero-gap tuple changed")
    return encoded.replace(expanded, b'        "gaps_out": [0, 0, 0, 0]', 1)


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


def _assert_canonical_json_contract() -> None:
    expected = b'{"fraction":1.25,"integer":1,"values":[0,-2,3.5]}'
    actual = _canonical_json_bytes(
        {
            "integer": 1.0,
            "fraction": 1.25,
            "values": [0.0, -2.0, 3.5],
        }
    )
    if actual != expected:
        raise ValueError(
            "canonical JSON no longer matches QJson integral-number emission"
        )


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
    required = source_schema.get("required")
    if (
        not isinstance(required, list)
        or "animationSources" not in required
        or "animationDependencySources" not in required
        or "inputBehaviorSources" not in required
        or "inputBehaviorDependencySources" not in required
        or "inputDeviceSources" not in required
        or "gestureSources" not in required
        or "workspaceBehaviorSources" not in required
        or "bindingRuntimeSources" not in required
        or "miscExclusionSources" not in required
        or "appearanceBehaviorSources" not in required
        or "advancedRuntimeSources" not in required
        or "windowBehaviorSources" not in required
    ):
        raise ValueError("source manifest focused runtime inventory is not required")
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

    monitor_property = properties.get("monitorSources", {})
    monitor_count = 2 * len(MONITOR_QUERY_SOURCE_PATHS)
    if (
        monitor_property.get("minItems") != monitor_count
        or monitor_property.get("maxItems") != monitor_count
    ):
        raise ValueError("source manifest monitor-source count is stale")
    monitor_branches = definitions.get("monitorSource", {}).get("oneOf")
    if not isinstance(monitor_branches, list):
        raise ValueError("source manifest has no closed monitor source inventory")
    actual_monitor: list[tuple[str, str, str]] = []
    for branch in monitor_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
        ):
            raise ValueError("source manifest monitor source branch is malformed")
        actual_monitor.append((version, path, digest))
    expected_monitor = [
        (version, path.as_posix(), QUALIFIED_SOURCE_HASHES[version][path])
        for version in ("0.56.0", "0.56.1")
        for path in sorted(MONITOR_QUERY_SOURCE_PATHS)
    ]
    if actual_monitor != expected_monitor:
        raise ValueError("source manifest monitor source inventory/pins are stale")

    maximize_property = properties.get("maximizeSources", {})
    maximize_count = 2 * len(MAXIMIZE_SOURCE_PATHS)
    if (
        maximize_property.get("minItems") != maximize_count
        or maximize_property.get("maxItems") != maximize_count
    ):
        raise ValueError("source manifest maximize-source count is stale")
    maximize_branches = definitions.get("maximizeSource", {}).get("oneOf")
    if not isinstance(maximize_branches, list):
        raise ValueError("source manifest has no closed maximize source inventory")
    actual_maximize: list[tuple[str, str, str]] = []
    for branch in maximize_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
        ):
            raise ValueError("source manifest maximize source branch is malformed")
        actual_maximize.append((version, path, digest))
    expected_maximize = [
        (version, path.as_posix(), QUALIFIED_SOURCE_HASHES[version][path])
        for version in ("0.56.0", "0.56.1")
        for path in MAXIMIZE_SOURCE_PATHS
    ]
    if actual_maximize != expected_maximize:
        raise ValueError("source manifest maximize source inventory/pins are stale")

    group_property = properties.get("groupBehaviorSources", {})
    group_count = sum(len(paths) for paths in GROUP_BEHAVIOR_SOURCE_PATHS.values())
    if (
        group_property.get("minItems") != group_count
        or group_property.get("maxItems") != group_count
    ):
        raise ValueError("source manifest group-behavior-source count is stale")
    group_branches = definitions.get("groupBehaviorSource", {}).get("oneOf")
    if not isinstance(group_branches, list):
        raise ValueError("source manifest has no closed group behavior inventory")
    actual_group: list[tuple[str, str, str]] = []
    for branch in group_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
        ):
            raise ValueError(
                "source manifest group behavior source branch is malformed"
            )
        actual_group.append((version, path, digest))
    expected_group = [
        (version, path.as_posix(), QUALIFIED_SOURCE_HASHES[version][path])
        for version, paths in GROUP_BEHAVIOR_SOURCE_PATHS.items()
        for path in paths
    ]
    if actual_group != expected_group:
        raise ValueError(
            "source manifest group behavior source inventory/pins are stale"
        )

    appearance_property = properties.get("appearanceBehaviorSources", {})
    appearance_count = sum(
        len(paths) for paths in APPEARANCE_BEHAVIOR_SOURCE_PATHS.values()
    )
    if (
        appearance_property.get("minItems") != appearance_count
        or appearance_property.get("maxItems") != appearance_count
        or appearance_property.get("type") != "array"
        or appearance_property.get("uniqueItems") is not True
        or appearance_property.get("items") is not False
    ):
        raise ValueError(
            "source manifest appearance-behavior-source array/count is stale"
        )
    expected_appearance_prefix = [
        {
            "allOf": [
                {"$ref": "#/$defs/appearanceBehaviorSource"},
                {
                    "properties": {
                        "version": {"const": version},
                        "path": {"const": path.as_posix()},
                    }
                },
            ]
        }
        for version, paths in APPEARANCE_BEHAVIOR_SOURCE_PATHS.items()
        for path in paths
    ]
    if appearance_property.get("prefixItems") != expected_appearance_prefix:
        raise ValueError(
            "source manifest appearance-behavior-source order is stale"
        )
    appearance_definition = definitions.get("appearanceBehaviorSource", {})
    if appearance_definition.get("additionalProperties") is not False:
        raise ValueError(
            "source manifest appearance behavior source definition is open"
        )
    if appearance_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest appearance behavior source required fields are stale"
        )
    appearance_properties = appearance_definition.get("properties", {})
    appearance_paths = appearance_properties.get("path", {}).get("enum")
    expected_appearance_paths = {
        path.as_posix()
        for paths in APPEARANCE_BEHAVIOR_SOURCE_PATHS.values()
        for path in paths
    }
    if (
        set(appearance_properties)
        != {"version", "tag", "commit", "path", "sha256"}
        or appearance_properties.get("version")
        != {"enum": list(APPEARANCE_BEHAVIOR_SOURCE_PATHS)}
        or appearance_properties.get("tag")
        != {
            "enum": [
                str(QUALIFIED_SOURCES[version]["tag"])
                for version in APPEARANCE_BEHAVIOR_SOURCE_PATHS
            ]
        }
        or appearance_properties.get("commit")
        != {"$ref": "#/$defs/commit"}
        or not isinstance(appearance_paths, list)
        or len(appearance_paths) != len(expected_appearance_paths)
        or set(appearance_paths) != expected_appearance_paths
        or appearance_properties.get("sha256")
        != {"$ref": "#/$defs/sha256"}
    ):
        raise ValueError(
            "source manifest appearance behavior source properties are stale"
        )
    appearance_branches = appearance_definition.get("oneOf")
    if not isinstance(appearance_branches, list):
        raise ValueError(
            "source manifest has no closed appearance behavior inventory"
        )
    actual_appearance: list[tuple[str, str, str, str, str]] = []
    for branch in appearance_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest appearance behavior source branch is malformed"
            )
        actual_appearance.append((version, tag, commit, path, digest))
    expected_appearance = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version, paths in APPEARANCE_BEHAVIOR_SOURCE_PATHS.items()
        for path in paths
    ]
    if actual_appearance != expected_appearance:
        raise ValueError(
            "source manifest appearance behavior source inventory/pins are stale"
        )

    advanced_property = properties.get("advancedRuntimeSources", {})
    advanced_count = sum(
        len(paths) for paths in ADVANCED_RUNTIME_SOURCE_PATHS.values()
    )
    if (
        advanced_property.get("minItems") != advanced_count
        or advanced_property.get("maxItems") != advanced_count
        or advanced_property.get("type") != "array"
        or advanced_property.get("uniqueItems") is not True
        or advanced_property.get("items")
        != {"$ref": "#/$defs/advancedRuntimeSource"}
    ):
        raise ValueError(
            "source manifest advanced-runtime-source array/count is stale"
        )
    advanced_definition = definitions.get("advancedRuntimeSource", {})
    if advanced_definition.get("additionalProperties") is not False:
        raise ValueError(
            "source manifest advanced runtime source definition is open"
        )
    if advanced_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest advanced runtime source required fields are stale"
        )
    advanced_properties = advanced_definition.get("properties", {})
    expected_advanced_versions = list(ADVANCED_RUNTIME_SOURCE_PATHS)
    expected_advanced_tags = [
        str(QUALIFIED_SOURCES[version]["tag"])
        for version in expected_advanced_versions
    ]
    advanced_paths = advanced_properties.get("path", {}).get("enum")
    expected_advanced_paths = {
        path.as_posix()
        for paths in ADVANCED_RUNTIME_SOURCE_PATHS.values()
        for path in paths
    }
    if (
        set(advanced_properties) != {"version", "tag", "commit", "path", "sha256"}
        or advanced_properties.get("version")
        != {"enum": expected_advanced_versions}
        or advanced_properties.get("tag") != {"enum": expected_advanced_tags}
        or advanced_properties.get("commit") != {"$ref": "#/$defs/commit"}
        or not isinstance(advanced_paths, list)
        or len(advanced_paths) != len(expected_advanced_paths)
        or set(advanced_paths) != expected_advanced_paths
        or advanced_properties.get("sha256") != {"$ref": "#/$defs/sha256"}
    ):
        raise ValueError(
            "source manifest advanced runtime source properties are stale"
        )
    advanced_branches = advanced_definition.get("oneOf")
    if not isinstance(advanced_branches, list):
        raise ValueError(
            "source manifest has no closed advanced runtime inventory"
        )
    actual_advanced: list[tuple[str, str, str, str, str]] = []
    for branch in advanced_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest advanced runtime source branch is malformed"
            )
        actual_advanced.append((version, tag, commit, path, digest))
    expected_advanced = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version, paths in ADVANCED_RUNTIME_SOURCE_PATHS.items()
        for path in paths
    ]
    if actual_advanced != expected_advanced:
        raise ValueError(
            "source manifest advanced runtime source inventory/pins are stale"
        )

    window_property = properties.get("windowBehaviorSources", {})
    window_count = sum(
        len(paths) for paths in WINDOW_BEHAVIOR_SOURCE_PATHS.values()
    )
    if (
        window_property.get("minItems") != window_count
        or window_property.get("maxItems") != window_count
        or window_property.get("type") != "array"
        or window_property.get("uniqueItems") is not True
        or window_property.get("items")
        != {"$ref": "#/$defs/windowBehaviorSource"}
    ):
        raise ValueError(
            "source manifest window-behavior-source array/count is stale"
        )
    window_definition = definitions.get("windowBehaviorSource", {})
    if window_definition.get("additionalProperties") is not False:
        raise ValueError("source manifest window behavior source definition is open")
    if window_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest window behavior source required fields are stale"
        )
    window_branches = window_definition.get("oneOf")
    if not isinstance(window_branches, list):
        raise ValueError("source manifest has no closed window behavior inventory")
    actual_window: list[tuple[str, str, str, str, str]] = []
    for branch in window_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest window behavior source branch is malformed"
            )
        actual_window.append((version, tag, commit, path, digest))
    expected_window = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version, paths in WINDOW_BEHAVIOR_SOURCE_PATHS.items()
        for path in paths
    ]
    if actual_window != expected_window:
        raise ValueError(
            "source manifest window behavior source inventory/pins are stale"
        )

    group_bar_property = properties.get("groupBarSources", {})
    group_bar_count = 2 * len(GROUP_BAR_SOURCE_PATHS)
    if (
        group_bar_property.get("minItems") != group_bar_count
        or group_bar_property.get("maxItems") != group_bar_count
    ):
        raise ValueError("source manifest groupbar-source count is stale")
    group_bar_branches = definitions.get("groupBarSource", {}).get("oneOf")
    if not isinstance(group_bar_branches, list):
        raise ValueError("source manifest has no closed groupbar inventory")
    actual_group_bar: list[tuple[str, str, str]] = []
    for branch in group_bar_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
        ):
            raise ValueError("source manifest groupbar source branch is malformed")
        actual_group_bar.append((version, path, digest))
    expected_group_bar = [
        (version, path.as_posix(), QUALIFIED_SOURCE_HASHES[version][path])
        for version in ("0.55.0", "0.56.1")
        for path in GROUP_BAR_SOURCE_PATHS
    ]
    if actual_group_bar != expected_group_bar:
        raise ValueError("source manifest groupbar source inventory/pins are stale")

    workspace_property = properties.get("workspaceBehaviorSources", {})
    workspace_count = sum(
        len(paths) for paths in WORKSPACE_BEHAVIOR_SOURCE_PATHS.values()
    )
    if (
        workspace_property.get("minItems") != workspace_count
        or workspace_property.get("maxItems") != workspace_count
        or workspace_property.get("type") != "array"
        or workspace_property.get("uniqueItems") is not True
        or workspace_property.get("items")
        != {"$ref": "#/$defs/workspaceBehaviorSource"}
    ):
        raise ValueError(
            "source manifest workspace-behavior-source array/count is stale"
        )
    workspace_definition = definitions.get("workspaceBehaviorSource", {})
    if workspace_definition.get("additionalProperties") is not False:
        raise ValueError("source manifest workspace behavior source definition is open")
    if workspace_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest workspace behavior source required fields are stale"
        )
    workspace_branches = workspace_definition.get("oneOf")
    if not isinstance(workspace_branches, list):
        raise ValueError("source manifest has no closed workspace behavior inventory")
    actual_workspace: list[tuple[str, str, str, str, str]] = []
    for branch in workspace_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest workspace behavior source branch is malformed"
            )
        actual_workspace.append((version, tag, commit, path, digest))
    expected_workspace = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version, paths in WORKSPACE_BEHAVIOR_SOURCE_PATHS.items()
        for path in paths
    ]
    if actual_workspace != expected_workspace:
        raise ValueError(
            "source manifest workspace behavior source inventory/pins are stale"
        )

    binding_property = properties.get("bindingRuntimeSources", {})
    binding_count = sum(
        len(paths) for paths in BINDING_RUNTIME_SOURCE_PATHS.values()
    )
    expected_binding = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version, paths in BINDING_RUNTIME_SOURCE_PATHS.items()
        for path in paths
    ]
    expected_binding_prefix = [
        {
            "allOf": [
                {"$ref": "#/$defs/bindingRuntimeSource"},
                {
                    "properties": {
                        "version": {"const": version},
                        "path": {"const": path},
                    }
                },
            ]
        }
        for version, _tag, _commit, path, _digest in expected_binding
    ]
    if (
        binding_property.get("type") != "array"
        or binding_property.get("minItems") != binding_count
        or binding_property.get("maxItems") != binding_count
        or binding_property.get("uniqueItems") is not True
        or binding_property.get("prefixItems") != expected_binding_prefix
        or binding_property.get("items") is not False
    ):
        raise ValueError(
            "source manifest binding-runtime-source array/count/order is stale"
        )
    binding_definition = definitions.get("bindingRuntimeSource", {})
    if binding_definition.get("additionalProperties") is not False:
        raise ValueError("source manifest binding runtime source definition is open")
    if binding_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest binding runtime source required fields are stale"
        )
    expected_binding_paths = [
        path.as_posix() for path in BINDING_RUNTIME_SOURCE_PATHS["0.55.0"]
    ]
    if binding_definition.get("properties") != {
        "version": {"enum": list(BINDING_RUNTIME_SOURCE_PATHS)},
        "tag": {
            "enum": [
                str(QUALIFIED_SOURCES[version]["tag"])
                for version in BINDING_RUNTIME_SOURCE_PATHS
            ]
        },
        "commit": {"$ref": "#/$defs/commit"},
        "path": {"enum": expected_binding_paths},
        "sha256": {"$ref": "#/$defs/sha256"},
    }:
        raise ValueError(
            "source manifest binding runtime source properties are stale"
        )
    binding_branches = binding_definition.get("oneOf")
    if not isinstance(binding_branches, list):
        raise ValueError("source manifest has no closed binding runtime inventory")
    actual_binding: list[tuple[str, str, str, str, str]] = []
    for branch in binding_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest binding runtime source branch is malformed"
            )
        actual_binding.append((version, tag, commit, path, digest))
    if actual_binding != expected_binding:
        raise ValueError(
            "source manifest binding runtime source inventory/pins are stale"
        )

    misc_property = properties.get("miscExclusionSources", {})
    misc_count = sum(
        len(paths) for paths in MISC_EXCLUSION_SOURCE_PATHS.values()
    )
    expected_misc = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version, paths in MISC_EXCLUSION_SOURCE_PATHS.items()
        for path in paths
    ]
    expected_misc_prefix = [
        {
            "allOf": [
                {"$ref": "#/$defs/miscExclusionSource"},
                {
                    "properties": {
                        "version": {"const": version},
                        "path": {"const": path},
                    }
                },
            ]
        }
        for version, _tag, _commit, path, _digest in expected_misc
    ]
    if (
        misc_property.get("type") != "array"
        or misc_property.get("minItems") != misc_count
        or misc_property.get("maxItems") != misc_count
        or misc_property.get("uniqueItems") is not True
        or misc_property.get("prefixItems") != expected_misc_prefix
        or misc_property.get("items") is not False
    ):
        raise ValueError(
            "source manifest misc-exclusion-source array/count/order is stale"
        )
    misc_definition = definitions.get("miscExclusionSource", {})
    if misc_definition.get("additionalProperties") is not False:
        raise ValueError("source manifest misc exclusion source definition is open")
    if misc_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest misc exclusion source required fields are stale"
        )
    expected_misc_paths = [
        path.as_posix() for path in MISC_EXCLUSION_SOURCE_PATHS["0.55.0"]
    ]
    if misc_definition.get("properties") != {
        "version": {"enum": list(MISC_EXCLUSION_SOURCE_PATHS)},
        "tag": {
            "enum": [
                str(QUALIFIED_SOURCES[version]["tag"])
                for version in MISC_EXCLUSION_SOURCE_PATHS
            ]
        },
        "commit": {"$ref": "#/$defs/commit"},
        "path": {"enum": expected_misc_paths},
        "sha256": {"$ref": "#/$defs/sha256"},
    }:
        raise ValueError(
            "source manifest misc exclusion source properties are stale"
        )
    misc_branches = misc_definition.get("oneOf")
    if not isinstance(misc_branches, list):
        raise ValueError("source manifest has no closed misc exclusion inventory")
    actual_misc: list[tuple[str, str, str, str, str]] = []
    for branch in misc_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest misc exclusion source branch is malformed"
            )
        actual_misc.append((version, tag, commit, path, digest))
    if actual_misc != expected_misc:
        raise ValueError(
            "source manifest misc exclusion source inventory/pins are stale"
        )

    input_property = properties.get("inputBehaviorSources", {})
    input_count = sum(len(paths) for paths in INPUT_BEHAVIOR_SOURCE_PATHS.values())
    expected_input = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version, paths in INPUT_BEHAVIOR_SOURCE_PATHS.items()
        for path in paths
    ]
    expected_input_prefix = [
        {
            "allOf": [
                {"$ref": "#/$defs/inputBehaviorSource"},
                {
                    "properties": {
                        "version": {"const": version},
                        "path": {"const": path},
                    }
                },
            ]
        }
        for version, _tag, _commit, path, _digest in expected_input
    ]
    if (
        input_property.get("minItems") != input_count
        or input_property.get("maxItems") != input_count
        or input_property.get("type") != "array"
        or input_property.get("uniqueItems") is not True
        or input_property.get("prefixItems") != expected_input_prefix
        or input_property.get("items") is not False
    ):
        raise ValueError(
            "source manifest input-behavior-source array/count/order is stale"
        )
    input_definition = definitions.get("inputBehaviorSource", {})
    if input_definition.get("additionalProperties") is not False:
        raise ValueError("source manifest input behavior source definition is open")
    if input_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest input behavior source required fields are stale"
        )
    expected_input_paths = {
        path.as_posix()
        for paths in INPUT_BEHAVIOR_SOURCE_PATHS.values()
        for path in paths
    }
    if input_definition.get("properties") != {
        "version": {"enum": list(INPUT_BEHAVIOR_SOURCE_PATHS)},
        "tag": {
            "enum": [
                str(QUALIFIED_SOURCES[version]["tag"])
                for version in INPUT_BEHAVIOR_SOURCE_PATHS
            ]
        },
        "commit": {"$ref": "#/$defs/commit"},
        "path": {"enum": sorted(expected_input_paths)},
        "sha256": {"$ref": "#/$defs/sha256"},
    }:
        raise ValueError(
            "source manifest input behavior source properties are stale"
        )
    input_branches = input_definition.get("oneOf")
    if not isinstance(input_branches, list):
        raise ValueError("source manifest has no closed input behavior inventory")
    actual_input: list[tuple[str, str, str, str, str]] = []
    for branch in input_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest input behavior source branch is malformed"
            )
        actual_input.append((version, tag, commit, path, digest))
    if actual_input != expected_input:
        raise ValueError(
            "source manifest input behavior source inventory/pins are stale"
        )

    input_dependency_property = properties.get(
        "inputBehaviorDependencySources", {}
    )
    input_dependency_count = len(INPUT_BEHAVIOR_DEPENDENCY_SOURCES) * (
        1 + len(HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS)
    )
    expected_input_dependency = [
        record
        for version in INPUT_BEHAVIOR_DEPENDENCY_SOURCES
        for record in (
            (
                version,
                REPOSITORY,
                str(QUALIFIED_SOURCES[version]["commit"]),
                FLAKE_LOCK_PATH.as_posix(),
                QUALIFIED_SOURCE_HASHES[version][FLAKE_LOCK_PATH],
            ),
            *(
                (
                    version,
                    str(INPUT_BEHAVIOR_DEPENDENCY_SOURCES[version]["repository"]),
                    str(INPUT_BEHAVIOR_DEPENDENCY_SOURCES[version]["revision"]),
                    path.as_posix(),
                    str(
                        INPUT_BEHAVIOR_DEPENDENCY_SOURCES[version]["hashes"][
                            path
                        ]
                    ),
                )
                for path in HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS
            ),
        )
    ]
    expected_input_dependency_prefix = [
        {
            "allOf": [
                {"$ref": "#/$defs/inputBehaviorDependencySource"},
                {
                    "properties": {
                        "hyprlandVersion": {"const": version},
                        "repository": {"const": repository},
                        "path": {"const": path},
                    }
                },
            ]
        }
        for version, repository, _revision, path, _digest in expected_input_dependency
    ]
    if (
        input_dependency_property.get("type") != "array"
        or input_dependency_property.get("minItems") != input_dependency_count
        or input_dependency_property.get("maxItems") != input_dependency_count
        or input_dependency_property.get("uniqueItems") is not True
        or input_dependency_property.get("prefixItems")
        != expected_input_dependency_prefix
        or input_dependency_property.get("items") is not False
    ):
        raise ValueError(
            "source manifest input-behavior-dependency array/count/order is stale"
        )
    input_dependency_definition = definitions.get(
        "inputBehaviorDependencySource", {}
    )
    if input_dependency_definition.get("additionalProperties") is not False:
        raise ValueError(
            "source manifest input behavior dependency definition is open"
        )
    if input_dependency_definition.get("required") != [
        "hyprlandVersion",
        "repository",
        "revision",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest input behavior dependency required fields are stale"
        )
    expected_input_dependency_paths = [
        FLAKE_LOCK_PATH.as_posix(),
        *(path.as_posix() for path in HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS),
    ]
    if input_dependency_definition.get("properties") != {
        "hyprlandVersion": {
            "enum": list(INPUT_BEHAVIOR_DEPENDENCY_SOURCES)
        },
        "repository": {
            "enum": [
                REPOSITORY,
                "https://github.com/hyprwm/hyprutils",
            ]
        },
        "revision": {"$ref": "#/$defs/commit"},
        "path": {"enum": expected_input_dependency_paths},
        "sha256": {"$ref": "#/$defs/sha256"},
    }:
        raise ValueError(
            "source manifest input behavior dependency properties are stale"
        )
    input_dependency_branches = input_dependency_definition.get("oneOf")
    if not isinstance(input_dependency_branches, list):
        raise ValueError(
            "source manifest has no closed input behavior dependency inventory"
        )
    actual_input_dependency: list[tuple[str, str, str, str, str]] = []
    for branch in input_dependency_branches:
        branch_properties = branch.get("properties", {})
        hyprland_version = branch_properties.get(
            "hyprlandVersion", {}
        ).get("const")
        repository = branch_properties.get("repository", {}).get("const")
        revision = branch_properties.get("revision", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(hyprland_version, str)
            or not isinstance(repository, str)
            or not isinstance(revision, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"hyprlandVersion", "repository", "revision", "path", "sha256"}
        ):
            raise ValueError(
                "source manifest input behavior dependency branch is malformed"
            )
        actual_input_dependency.append(
            (hyprland_version, repository, revision, path, digest)
        )
    if actual_input_dependency != expected_input_dependency:
        raise ValueError(
            "source manifest input behavior dependency inventory/pins are stale"
        )

    input_device_property = properties.get("inputDeviceSources", {})
    input_device_count = sum(
        len(paths) for paths in INPUT_DEVICE_SOURCE_PATHS.values()
    )
    if (
        input_device_property.get("minItems") != input_device_count
        or input_device_property.get("maxItems") != input_device_count
    ):
        raise ValueError("source manifest input-device-source count is stale")
    input_device_branches = definitions.get("inputDeviceSource", {}).get("oneOf")
    if not isinstance(input_device_branches, list):
        raise ValueError("source manifest has no closed input-device inventory")
    actual_input_device: list[tuple[str, str, str]] = []
    for branch in input_device_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
        ):
            raise ValueError(
                "source manifest input-device source branch is malformed"
            )
        actual_input_device.append((version, path, digest))
    expected_input_device = [
        (version, path.as_posix(), QUALIFIED_SOURCE_HASHES[version][path])
        for version, paths in INPUT_DEVICE_SOURCE_PATHS.items()
        for path in paths
    ]
    if actual_input_device != expected_input_device:
        raise ValueError(
            "source manifest input-device source inventory/pins are stale"
        )

    gesture_property = properties.get("gestureSources", {})
    gesture_count = 2 * len(GESTURE_SOURCE_PATHS)
    if (
        gesture_property.get("minItems") != gesture_count
        or gesture_property.get("maxItems") != gesture_count
    ):
        raise ValueError("source manifest gesture-source count is stale")
    if (
        gesture_property.get("type") != "array"
        or gesture_property.get("uniqueItems") is not True
        or gesture_property.get("items")
        != {"$ref": "#/$defs/gestureSource"}
    ):
        raise ValueError("source manifest gesture-source array is not closed")
    gesture_definition = definitions.get("gestureSource", {})
    if gesture_definition.get("additionalProperties") is not False:
        raise ValueError("source manifest gesture source definition is open")
    if gesture_definition.get("required") != [
        "version",
        "tag",
        "commit",
        "path",
        "sha256",
    ]:
        raise ValueError(
            "source manifest gesture source required fields are stale"
        )
    gesture_branches = gesture_definition.get("oneOf")
    if not isinstance(gesture_branches, list):
        raise ValueError("source manifest has no closed gesture inventory")
    actual_gesture: list[tuple[str, str, str, str, str]] = []
    for branch in gesture_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        tag = branch_properties.get("tag", {}).get("const")
        commit = branch_properties.get("commit", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(tag, str)
            or not isinstance(commit, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
            or set(branch_properties)
            != {"version", "tag", "commit", "path", "sha256"}
        ):
            raise ValueError("source manifest gesture source branch is malformed")
        actual_gesture.append((version, tag, commit, path, digest))
    expected_gesture = [
        (
            version,
            str(QUALIFIED_SOURCES[version]["tag"]),
            str(QUALIFIED_SOURCES[version]["commit"]),
            path.as_posix(),
            QUALIFIED_SOURCE_HASHES[version][path],
        )
        for version in ("0.55.0", "0.56.1")
        for path in GESTURE_SOURCE_PATHS
    ]
    if actual_gesture != expected_gesture:
        raise ValueError("source manifest gesture source inventory/pins are stale")

    animation_property = properties.get("animationSources", {})
    animation_count = sum(len(paths) for paths in ANIMATION_SOURCE_PATHS.values())
    if (
        animation_property.get("minItems") != animation_count
        or animation_property.get("maxItems") != animation_count
    ):
        raise ValueError("source manifest animation-source count is stale")
    animation_branches = definitions.get("animationSource", {}).get("oneOf")
    if not isinstance(animation_branches, list):
        raise ValueError("source manifest has no closed animation inventory")
    actual_animation: list[tuple[str, str, str]] = []
    for branch in animation_branches:
        branch_properties = branch.get("properties", {})
        version = branch_properties.get("version", {}).get("const")
        path = branch_properties.get("path", {}).get("const")
        digest = branch_properties.get("sha256", {}).get("const")
        if (
            not isinstance(version, str)
            or not isinstance(path, str)
            or not isinstance(digest, str)
        ):
            raise ValueError("source manifest animation source branch is malformed")
        actual_animation.append((version, path, digest))
    expected_animation = [
        (version, path.as_posix(), QUALIFIED_SOURCE_HASHES[version][path])
        for version, paths in ANIMATION_SOURCE_PATHS.items()
        for path in paths
    ]
    if actual_animation != expected_animation:
        raise ValueError("source manifest animation source inventory/pins are stale")

    dependency_property = properties.get("animationDependencySources", {})
    dependency_count = len(ANIMATION_DEPENDENCY_SOURCES) * (
        1 + len(HYPRUTILS_ANIMATION_SOURCE_PATHS)
    )
    if (
        dependency_property.get("minItems") != dependency_count
        or dependency_property.get("maxItems") != dependency_count
    ):
        raise ValueError("source manifest animation-dependency count is stale")
    dependency_branches = definitions.get(
        "animationDependencySource", {}
    ).get("oneOf")
    if not isinstance(dependency_branches, list):
        raise ValueError(
            "source manifest has no closed animation dependency inventory"
        )
    actual_dependencies: list[tuple[str, str, str, str, str]] = []
    for branch in dependency_branches:
        branch_properties = branch.get("properties", {})
        values = tuple(
            branch_properties.get(key, {}).get("const")
            for key in (
                "hyprlandVersion", "repository", "revision", "path", "sha256"
            )
        )
        if not all(isinstance(value, str) for value in values):
            raise ValueError(
                "source manifest animation dependency branch is malformed"
            )
        actual_dependencies.append(values)
    expected_dependencies = [
        record
        for version in ("0.55.0", "0.56.1")
        for record in (
            (
                version,
                REPOSITORY,
                QUALIFIED_SOURCES[version]["commit"],
                FLAKE_LOCK_PATH.as_posix(),
                QUALIFIED_SOURCE_HASHES[version][FLAKE_LOCK_PATH],
            ),
            *(
                (
                    version,
                    ANIMATION_DEPENDENCY_SOURCES[version]["repository"],
                    ANIMATION_DEPENDENCY_SOURCES[version]["revision"],
                    path.as_posix(),
                    ANIMATION_DEPENDENCY_SOURCES[version]["hashes"][path],
                )
                for path in HYPRUTILS_ANIMATION_SOURCE_PATHS
            ),
        )
    ]
    if actual_dependencies != expected_dependencies:
        raise ValueError(
            "source manifest animation dependency inventory/pins are stale"
        )

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
            "restart",
            "caution",
            "Compatibility-preserved per-device input overrides selected by a session-assigned Hyprland name; generic changes require restart until a dedicated transaction can prove them.",
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
            "restart",
            "caution",
            "Compatibility-preserved named binding submaps with an explicit reset target; bindings reference the submap name, and generic changes require restart until a dedicated receipt-bound shortcut transaction can prove them.",
            "submap",
            f"{WIKI_ROOT}/Configuring/Basics/Binds/",
        ),
        (
            "bindings",
            "binding",
            True,
            "id",
            "restart",
            "caution",
            "Compatibility-preserved normalized key chords mapped to closed semantic actions; duplicate chords are rejected, and generic changes require restart until a dedicated receipt-bound shortcut transaction can prove them.",
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
    hyprutils_055: Path,
    hyprutils_056: Path,
    output_root: Path = Path.cwd(),
) -> dict[Path, bytes]:
    _assert_canonical_json_contract()
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
    hyprutils_roots = {
        "0.55.0": hyprutils_055,
        "0.56.1": hyprutils_056,
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
    monitor_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in MONITOR_QUERY_SOURCE_PATHS
        }
        for version in ("0.56.0", "0.56.1")
    }
    maximize_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in MAXIMIZE_SOURCE_PATHS
        }
        for version in ("0.56.0", "0.56.1")
    }
    group_behavior_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in GROUP_BEHAVIOR_SOURCE_PATHS.items()
    }
    appearance_behavior_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in APPEARANCE_BEHAVIOR_SOURCE_PATHS.items()
    }
    advanced_runtime_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in ADVANCED_RUNTIME_SOURCE_PATHS.items()
    }
    window_behavior_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in WINDOW_BEHAVIOR_SOURCE_PATHS.items()
    }
    group_bar_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in GROUP_BAR_SOURCE_PATHS
        }
        for version in ("0.55.0", "0.56.1")
    }
    workspace_behavior_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in WORKSPACE_BEHAVIOR_SOURCE_PATHS.items()
    }
    binding_runtime_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in BINDING_RUNTIME_SOURCE_PATHS.items()
    }
    misc_exclusion_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in MISC_EXCLUSION_SOURCE_PATHS.items()
    }
    misc_exclusion_global_occurrences = {
        version: _source_tree_option_occurrences(
            source_roots[version], MISC_EXCLUSION_OPTION_PATHS
        )
        for version in MISC_EXCLUSION_SOURCE_PATHS
    }
    input_behavior_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in INPUT_BEHAVIOR_SOURCE_PATHS.items()
    }
    input_behavior_flake_lock_bytes = {
        version: _read_qualified_source(
            source_roots[version], version, FLAKE_LOCK_PATH
        )
        for version in INPUT_BEHAVIOR_DEPENDENCY_SOURCES
    }
    input_behavior_dependency_source_bytes = {
        version: {
            path: _read_input_behavior_dependency_source(
                hyprutils_roots[version], version, path
            )
            for path in HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS
        }
        for version in INPUT_BEHAVIOR_DEPENDENCY_SOURCES
    }
    input_device_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in INPUT_DEVICE_SOURCE_PATHS.items()
    }
    gesture_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in GESTURE_SOURCE_PATHS
        }
        for version in ("0.55.0", "0.56.1")
    }
    animation_source_bytes = {
        version: {
            path: _read_qualified_source(source_roots[version], version, path)
            for path in paths
        }
        for version, paths in ANIMATION_SOURCE_PATHS.items()
    }
    animation_flake_lock_bytes = {
        version: _read_qualified_source(
            source_roots[version], version, FLAKE_LOCK_PATH
        )
        for version in ANIMATION_DEPENDENCY_SOURCES
    }
    animation_dependency_source_bytes = {
        version: {
            path: _read_animation_dependency_source(
                hyprutils_roots[version], version, path
            )
            for path in HYPRUTILS_ANIMATION_SOURCE_PATHS
        }
        for version in ANIMATION_DEPENDENCY_SOURCES
    }
    _assert_monitor_query_contract(monitor_source_bytes)
    _assert_maximize_contract(maximize_source_bytes)
    _assert_group_behavior_contract(group_behavior_source_bytes)
    _assert_appearance_behavior_contract(appearance_behavior_source_bytes)
    _assert_advanced_runtime_contract(advanced_runtime_source_bytes)
    _assert_window_behavior_contract(window_behavior_source_bytes)
    _assert_group_bar_contract(group_bar_source_bytes)
    _assert_workspace_behavior_contract(workspace_behavior_source_bytes)
    _assert_binding_runtime_contract(binding_runtime_source_bytes)
    _assert_misc_exclusion_contract(
        misc_exclusion_source_bytes,
        misc_exclusion_global_occurrences,
    )
    _assert_input_behavior_contract(input_behavior_source_bytes)
    _assert_input_behavior_dependency_contract(
        input_behavior_flake_lock_bytes,
        input_behavior_dependency_source_bytes,
    )
    _assert_input_device_contract(input_device_source_bytes)
    _assert_gesture_runtime_contract(gesture_source_bytes)
    _assert_animation_contract(animation_source_bytes)
    _assert_animation_dependency_contract(
        animation_flake_lock_bytes, animation_dependency_source_bytes
    )
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
    _assert_advanced_render_catalog(catalog)
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
        "workspaceRules": [
            {
                "id": "hyprshelld.internal.shared-spacing.maximized",
                "selector": "f[1]",
                "enabled": True,
                "monitor": "",
                "persistent": False,
                "isDefault": False,
                "layout": "",
                "overrides": {"gaps_out": [0, 0, 0, 0]},
            }
        ],
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
        "monitorSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(monitor_source_bytes[version][path]),
            }
            for version in ("0.56.0", "0.56.1")
            for path in sorted(MONITOR_QUERY_SOURCE_PATHS)
        ],
        "maximizeSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(maximize_source_bytes[version][path]),
            }
            for version in ("0.56.0", "0.56.1")
            for path in MAXIMIZE_SOURCE_PATHS
        ],
        "groupBehaviorSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(group_behavior_source_bytes[version][path]),
            }
            for version, paths in GROUP_BEHAVIOR_SOURCE_PATHS.items()
            for path in paths
        ],
        "appearanceBehaviorSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(
                    appearance_behavior_source_bytes[version][path]
                ),
            }
            for version, paths in APPEARANCE_BEHAVIOR_SOURCE_PATHS.items()
            for path in paths
        ],
        "advancedRuntimeSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(advanced_runtime_source_bytes[version][path]),
            }
            for version, paths in ADVANCED_RUNTIME_SOURCE_PATHS.items()
            for path in paths
        ],
        "windowBehaviorSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(window_behavior_source_bytes[version][path]),
            }
            for version, paths in WINDOW_BEHAVIOR_SOURCE_PATHS.items()
            for path in paths
        ],
        "groupBarSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(group_bar_source_bytes[version][path]),
            }
            for version in ("0.55.0", "0.56.1")
            for path in GROUP_BAR_SOURCE_PATHS
        ],
        "workspaceBehaviorSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(
                    workspace_behavior_source_bytes[version][path]
                ),
            }
            for version, paths in WORKSPACE_BEHAVIOR_SOURCE_PATHS.items()
            for path in paths
        ],
        "bindingRuntimeSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(binding_runtime_source_bytes[version][path]),
            }
            for version, paths in BINDING_RUNTIME_SOURCE_PATHS.items()
            for path in paths
        ],
        "miscExclusionSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(misc_exclusion_source_bytes[version][path]),
            }
            for version, paths in MISC_EXCLUSION_SOURCE_PATHS.items()
            for path in paths
        ],
        "inputBehaviorSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(input_behavior_source_bytes[version][path]),
            }
            for version, paths in INPUT_BEHAVIOR_SOURCE_PATHS.items()
            for path in paths
        ],
        "inputBehaviorDependencySources": [
            record
            for version in ("0.55.0", "0.56.1")
            for record in (
                {
                    "hyprlandVersion": version,
                    "repository": REPOSITORY,
                    "revision": QUALIFIED_SOURCES[version]["commit"],
                    "path": FLAKE_LOCK_PATH.as_posix(),
                    "sha256": _sha256(input_behavior_flake_lock_bytes[version]),
                },
                *(
                    {
                        "hyprlandVersion": version,
                        "repository": INPUT_BEHAVIOR_DEPENDENCY_SOURCES[version][
                            "repository"
                        ],
                        "revision": INPUT_BEHAVIOR_DEPENDENCY_SOURCES[version][
                            "revision"
                        ],
                        "path": path.as_posix(),
                        "sha256": _sha256(
                            input_behavior_dependency_source_bytes[version][path]
                        ),
                    }
                    for path in HYPRUTILS_INPUT_BEHAVIOR_SOURCE_PATHS
                ),
            )
        ],
        "inputDeviceSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(input_device_source_bytes[version][path]),
            }
            for version, paths in INPUT_DEVICE_SOURCE_PATHS.items()
            for path in paths
        ],
        "gestureSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(gesture_source_bytes[version][path]),
            }
            for version in ("0.55.0", "0.56.1")
            for path in GESTURE_SOURCE_PATHS
        ],
        "animationSources": [
            {
                "version": version,
                "tag": QUALIFIED_SOURCES[version]["tag"],
                "commit": QUALIFIED_SOURCES[version]["commit"],
                "path": path.as_posix(),
                "sha256": _sha256(animation_source_bytes[version][path]),
            }
            for version, paths in ANIMATION_SOURCE_PATHS.items()
            for path in paths
        ],
        "animationDependencySources": [
            record
            for version in ("0.55.0", "0.56.1")
            for record in (
                {
                    "hyprlandVersion": version,
                    "repository": REPOSITORY,
                    "revision": QUALIFIED_SOURCES[version]["commit"],
                    "path": FLAKE_LOCK_PATH.as_posix(),
                    "sha256": _sha256(animation_flake_lock_bytes[version]),
                },
                *(
                    {
                        "hyprlandVersion": version,
                        "repository": ANIMATION_DEPENDENCY_SOURCES[version][
                            "repository"
                        ],
                        "revision": ANIMATION_DEPENDENCY_SOURCES[version][
                            "revision"
                        ],
                        "path": path.as_posix(),
                        "sha256": _sha256(
                            animation_dependency_source_bytes[version][path]
                        ),
                    }
                    for path in HYPRUTILS_ANIMATION_SOURCE_PATHS
                ),
            )
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
        Path("data/defaults/hyprland.json"): _default_state_json_bytes(defaults),
        Path("tests/fixtures/hyprland/v0.55.0.scalar-options.json"): _json_bytes(fixture_055),
        Path("tests/fixtures/hyprland/v0.56.1.scalar-options.json"): _json_bytes(fixture_056),
        Path("tests/fixtures/hyprland/v0.55.0-to-v0.56.1.delta.json"): _json_bytes(delta),
        Path("tests/fixtures/hyprland/source-manifest.json"): _json_bytes(source_manifest),
        Path("tests/fixtures/hyprland/generation-manifest.json"): _json_bytes(
            generation_manifest_fixture
        ),
    }


def _git_output(source_root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", "-C", source_root.as_posix(), *arguments],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise ValueError(
            f"cannot prove Git identity for {source_root}: {detail or 'git failed'}"
        )
    return result.stdout.rstrip("\n")


def _assert_v2_upstream_identity(source_root: Path) -> None:
    head = _git_output(source_root, "rev-parse", "HEAD^{commit}")
    tree = _git_output(source_root, "rev-parse", "HEAD^{tree}")
    if head != V2_UPSTREAM["commit"] or tree != V2_UPSTREAM["tree"]:
        raise ValueError(
            "Hyprland 0.56.2 Git identity mismatch: "
            f"expected {V2_UPSTREAM['commit']} tree {V2_UPSTREAM['tree']}, "
            f"found {head} tree {tree}"
        )
    tags = set(_git_output(source_root, "tag", "--points-at", "HEAD").splitlines())
    if V2_UPSTREAM["tag"] not in tags:
        raise ValueError(
            f"Hyprland 0.56.2 HEAD is not tagged {V2_UPSTREAM['tag']}"
        )
    status = _git_output(
        source_root,
        "status",
        "--porcelain=v1",
        "--untracked-files=all",
    )
    if status:
        raise ValueError(
            "Hyprland 0.56.2 source must be a clean Git checkout; "
            f"status was {status!r}"
        )
    version_bytes = (source_root / V2_UPSTREAM["versionPath"]).read_bytes()
    if _sha256(version_bytes) != V2_UPSTREAM["versionSha256"]:
        raise ValueError("Hyprland 0.56.2 VERSION provenance mismatch")
    if version_bytes.decode("utf-8").strip() != V2_UPSTREAM["version"]:
        raise ValueError("Hyprland 0.56.2 VERSION content mismatch")


def _v2_base_source_paths() -> set[Path]:
    return {
        Path("VERSION"),
        FLAKE_LOCK_PATH,
        REGISTRY_PATH,
        *COMPLEX_SOURCE_PATHS,
        *MONITOR_QUERY_SOURCE_PATHS,
        *MAXIMIZE_SOURCE_PATHS,
        *GROUP_BEHAVIOR_SOURCE_PATHS["0.56.1"],
        *APPEARANCE_BEHAVIOR_SOURCE_PATHS["0.56.1"],
        *ADVANCED_RUNTIME_SOURCE_PATHS["0.56.1"],
        *WINDOW_BEHAVIOR_SOURCE_PATHS["0.56.1"],
        *GROUP_BAR_SOURCE_PATHS,
        *WORKSPACE_BEHAVIOR_SOURCE_PATHS["0.56.1"],
        *BINDING_RUNTIME_SOURCE_PATHS["0.56.1"],
        *MISC_EXCLUSION_SOURCE_PATHS["0.56.1"],
        *INPUT_BEHAVIOR_SOURCE_PATHS["0.56.1"],
        *INPUT_DEVICE_SOURCE_PATHS["0.56.1"],
        *GESTURE_SOURCE_PATHS,
        *ANIMATION_SOURCE_PATHS["0.56.1"],
    }


def _v2_qualified_source_paths() -> tuple[Path, ...]:
    base = _v2_base_source_paths()
    if len(base) != 156:
        raise ValueError(
            f"reviewed Hyprland 0.56.1 closed source union has {len(base)} paths, expected 156"
        )
    changed = {Path(path) for _, path, _, _ in V2_CHANGED_PATH_REVIEW}
    paths = tuple(sorted(base | changed, key=lambda path: path.as_posix()))
    if len(paths) != 174:
        raise ValueError(
            f"reviewed Hyprland 0.56.2 closed source union has {len(paths)} paths, expected 174"
        )
    if len(V2_CHANGED_PATH_REVIEW) != 35 or tuple(
        path for _, path, _, _ in V2_CHANGED_PATH_REVIEW
    ) != tuple(sorted(path for _, path, _, _ in V2_CHANGED_PATH_REVIEW)):
        raise ValueError("reviewed Hyprland 0.56.1 -> 0.56.2 path ledger is not exact")
    return paths


def _v2_source_domains(path: Path) -> list[str]:
    domains: set[str] = set()
    memberships: tuple[tuple[str, Iterable[Path]], ...] = (
        ("advanced-runtime", ADVANCED_RUNTIME_SOURCE_PATHS["0.56.1"]),
        ("animation", ANIMATION_SOURCE_PATHS["0.56.1"]),
        ("appearance", APPEARANCE_BEHAVIOR_SOURCE_PATHS["0.56.1"]),
        ("bindings", BINDING_RUNTIME_SOURCE_PATHS["0.56.1"]),
        ("complex-config", COMPLEX_SOURCE_PATHS),
        ("gestures", GESTURE_SOURCE_PATHS),
        ("group", (*GROUP_BEHAVIOR_SOURCE_PATHS["0.56.1"], *GROUP_BAR_SOURCE_PATHS)),
        ("input", INPUT_BEHAVIOR_SOURCE_PATHS["0.56.1"]),
        ("input-device", INPUT_DEVICE_SOURCE_PATHS["0.56.1"]),
        ("maximize", MAXIMIZE_SOURCE_PATHS),
        ("monitor", MONITOR_QUERY_SOURCE_PATHS),
        ("observation", MONITOR_QUERY_SOURCE_PATHS),
        ("scalar-options", (REGISTRY_PATH, *MISC_EXCLUSION_SOURCE_PATHS["0.56.1"])),
        ("startup", STARTUP_SOURCE_PATHS_0560),
        ("window", WINDOW_BEHAVIOR_SOURCE_PATHS["0.56.1"]),
        ("workspace", WORKSPACE_BEHAVIOR_SOURCE_PATHS["0.56.1"]),
    )
    for domain, paths in memberships:
        if path in paths:
            domains.add(domain)
    if path == Path("VERSION"):
        domains.add("release")
    if path == FLAKE_LOCK_PATH:
        domains.add("dependency")

    classification = next(
        (
            item_classification
            for _, item_path, item_classification, _ in V2_CHANGED_PATH_REVIEW
            if item_path == path.as_posix()
        ),
        None,
    )
    classification_domains = {
        "release-metadata": ("release",),
        "dependency-lock": ("dependency",),
        "documentation-tooling": ("tooling",),
        "test-evidence": ("test",),
        "managed-config-runtime": ("scalar-options",),
        "managed-action-runtime": ("bindings",),
        "managed-monitor-runtime": ("monitor",),
        "managed-workspace-runtime": ("workspace",),
        "managed-observation-runtime": ("observation",),
        "reviewed-desktop-runtime": ("window",),
        "reviewed-layout-runtime": ("maximize",),
        "reviewed-input-runtime": ("input",),
        "reviewed-render-runtime": ("renderer",),
    }
    if classification is not None:
        domains.update(classification_domains[classification])
    ordered = [domain for domain in V2_DOMAIN_ORDER if domain in domains]
    if not ordered or len(ordered) != len(domains):
        raise ValueError(f"qualified source domain coverage is incomplete for {path}")
    return ordered


def _v2_effective_patch(
    source_root: Path, patch_path: Path
) -> tuple[dict[str, Any], bytes]:
    if patch_path.name != V2_PATCH["fileName"]:
        raise ValueError(
            f"protected patch filename must be {V2_PATCH['fileName']}, found {patch_path.name}"
        )
    patch_bytes = patch_path.read_bytes()
    if _sha256(patch_bytes) != V2_PATCH["patchSha256"]:
        raise ValueError("protected f[1] patch provenance mismatch")
    hunk_offset = patch_bytes.find(b"@@")
    if hunk_offset < 0:
        raise ValueError("protected f[1] patch has no unified hunk")
    expected_header = (
        b"diff --git a/src/config/lua/bindings/LuaBindingsConfigRules.cpp "
        b"b/src/config/lua/bindings/LuaBindingsConfigRules.cpp\n"
        b"--- a/src/config/lua/bindings/LuaBindingsConfigRules.cpp\n"
        b"+++ b/src/config/lua/bindings/LuaBindingsConfigRules.cpp\n"
    )
    if patch_bytes[:hunk_offset] != expected_header:
        raise ValueError("protected f[1] patch headers changed")
    exact_hunk = patch_bytes[hunk_offset:]
    if _sha256(exact_hunk) != V2_PATCH["hunkSha256"]:
        raise ValueError("protected f[1] exact hunk provenance mismatch")
    lines = exact_hunk.splitlines(keepends=True)
    if len(lines) != 14 or lines[0] != (
        b"@@ -726,6 +726,13 @@ static int hlWorkspaceRule(lua_State* L) {\n"
    ):
        raise ValueError("protected f[1] exact hunk shape changed")
    preimage_chunk = b"".join(
        line[1:] for line in lines[1:] if line.startswith((b" ", b"-"))
    )
    postimage_chunk = b"".join(
        line[1:] for line in lines[1:] if line.startswith((b" ", b"+"))
    )
    if any(not line.startswith((b" ", b"+", b"-")) for line in lines[1:]):
        raise ValueError("protected f[1] patch contains an unsupported hunk line")
    target = source_root / V2_PATCH["targetPath"]
    upstream_bytes = target.read_bytes()
    if _sha256(upstream_bytes) != V2_PATCH["preimageSha256"]:
        raise ValueError("protected f[1] patch target preimage mismatch")
    if upstream_bytes.count(preimage_chunk) != 1:
        raise ValueError("protected f[1] patch hunk does not match exactly once")
    effective_bytes = upstream_bytes.replace(preimage_chunk, postimage_chunk, 1)
    if _sha256(effective_bytes) != V2_PATCH["postimageSha256"]:
        raise ValueError("protected f[1] patch target postimage mismatch")
    record = {
        "id": V2_PATCH["id"],
        "fileName": V2_PATCH["fileName"],
        "targetPath": V2_PATCH["targetPath"],
        "patchSha256": V2_PATCH["patchSha256"],
        "preimageSha256": V2_PATCH["preimageSha256"],
        "postimageSha256": V2_PATCH["postimageSha256"],
        "hunkSha256": V2_PATCH["hunkSha256"],
        "exactHunk": exact_hunk.decode("utf-8"),
    }
    return record, effective_bytes


def _v2_scalar_document(
    registry_bytes: bytes, options: list[RawOption]
) -> dict[str, Any]:
    return {
        "formatVersion": 1,
        "hyprlandVersion": V2_UPSTREAM["version"],
        "tag": V2_UPSTREAM["tag"],
        "commit": V2_UPSTREAM["commit"],
        "source": {
            "path": REGISTRY_PATH.as_posix(),
            "sha256": _sha256(registry_bytes),
        },
        "optionCount": len(options),
        "options": [inventory_record(option) for option in options],
    }


def _v2_delta_document(
    predecessor: dict[str, Any], upstream: dict[str, Any]
) -> dict[str, Any]:
    delta = _delta_document(predecessor, upstream)
    delta["from"] = V2_PREDECESSOR["version"]
    delta["to"] = V2_UPSTREAM["version"]
    return delta


def _assert_v2_manifest_schema_receipts(
    manifest: dict[str, Any], schema: dict[str, Any]
) -> None:
    if schema.get("required") != list(manifest):
        raise ValueError("v2 source manifest top-level order/closure changed")

    def constants(property_name: str) -> list[dict[str, Any]]:
        prefix_items = schema.get("properties", {}).get(property_name, {}).get(
            "prefixItems"
        )
        if not isinstance(prefix_items, list):
            raise ValueError(f"v2 source manifest schema does not close {property_name}")
        result: list[dict[str, Any]] = []
        for item in prefix_items:
            try:
                properties = item["allOf"][1]["properties"]
                result.append(
                    {
                        key: descriptor["const"]
                        for key, descriptor in properties.items()
                    }
                )
            except (KeyError, IndexError, TypeError) as error:
                raise ValueError(
                    f"v2 source manifest schema has an open {property_name} receipt"
                ) from error
        return result

    for property_name in ("qualifiedSources", "changedPaths"):
        expected = constants(property_name)
        actual = manifest[property_name]
        if len(expected) != len(actual):
            raise ValueError(f"v2 source manifest {property_name} count changed")
        for index, (expected_record, actual_record) in enumerate(
            zip(expected, actual, strict=True)
        ):
            if any(
                actual_record.get(key) != value
                for key, value in expected_record.items()
            ):
                raise ValueError(
                    f"v2 source manifest {property_name}[{index}] receipt mismatch"
                )


def _v2_generation_manifest(
    template: dict[str, Any],
    source_manifest_digest: str,
    authority_id: str,
) -> dict[str, Any]:
    if not re.fullmatch(r"(?!0{32}$)[0-9a-f]{32}", authority_id):
        raise ValueError("v2 generation fixture authorityId is not a nonzero 128-bit ID")
    runtime_snapshot = {
        "formatVersion": template["formatVersion"],
        "authorityId": authority_id,
        **{
            key: value
            for key, value in template.items()
            if key != "formatVersion"
        },
    }
    payload = {
        "formatVersion": 2,
        "contractVersion": 2,
        "authorityId": authority_id,
        "snapshotDigest": _sha256(_canonical_json_bytes(runtime_snapshot)),
        "sourceManifestDigest": source_manifest_digest,
        "catalogDigest": template["catalogDigest"],
        "actionCatalogDigest": template["actionCatalogDigest"],
        "revision": template["revision"],
        "targetHyprland": template["targetHyprland"],
        "compatibleHyprland": {
            "major": 0,
            "minor": 56,
            "reviewedVersion": V2_UPSTREAM["version"],
            "minimumPatch": 2,
            "maximumPatch": 2,
        },
        "rendererVersion": 2,
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
    generation = _sha256(_canonical_json_bytes(payload))
    return {
        "formatVersion": payload["formatVersion"],
        "contractVersion": payload["contractVersion"],
        "authorityId": payload["authorityId"],
        "generation": generation,
        **{
            key: value
            for key, value in payload.items()
            if key not in ("formatVersion", "contractVersion", "authorityId")
        },
    }


def build_v2_documents(
    source_056: Path,
    source_0562: Path,
    hyprutils_0562: Path,
    protected_f1_patch_0562: Path,
    output_root: Path = Path.cwd(),
    v1_documents: dict[Path, bytes] | None = None,
) -> dict[Path, bytes]:
    """Build the exact, dormant 0.56.2 contract without mutating v1 output."""
    _assert_canonical_json_contract()
    _assert_v2_upstream_identity(source_0562)

    def v1_bytes(relative_path: str) -> bytes:
        path = Path(relative_path)
        if v1_documents is not None and path in v1_documents:
            return v1_documents[path]
        return (output_root / path).read_bytes()

    predecessor_registry = _read_qualified_source(
        source_056, V2_PREDECESSOR["version"], REGISTRY_PATH
    )
    upstream_registry = (source_0562 / REGISTRY_PATH).read_bytes()
    predecessor_options = extract_raw_options(predecessor_registry.decode("utf-8"))
    upstream_options = extract_raw_options(upstream_registry.decode("utf-8"))
    if len(predecessor_options) != 353 or len(upstream_options) != 353:
        raise ValueError("v2 scalar inventory must preserve exactly 353 options")
    predecessor_fixture = _strict_json(
        v1_bytes("tests/fixtures/hyprland/v0.56.1.scalar-options.json"),
        "generated v0.56.1 scalar fixture",
    )
    expected_predecessor = _source_document(
        V2_PREDECESSOR["version"], predecessor_registry, predecessor_options
    )
    if predecessor_fixture != expected_predecessor:
        raise ValueError("v1 scalar fixture drifted before v2 rotation")
    upstream_fixture = _v2_scalar_document(upstream_registry, upstream_options)
    delta = _v2_delta_document(predecessor_fixture, upstream_fixture)
    if delta["added"] or delta["removed"] or delta["changed"]:
        raise ValueError("Hyprland 0.56.1 -> 0.56.2 scalar inventory is not empty")

    patch_record, effective_patch_target = _v2_effective_patch(
        source_0562, protected_f1_patch_0562
    )
    source_paths = _v2_qualified_source_paths()
    qualified_sources: list[dict[str, Any]] = []
    for path in source_paths:
        upstream_bytes = (source_0562 / path).read_bytes()
        effective_digest = (
            _sha256(effective_patch_target)
            if path.as_posix() == V2_PATCH["targetPath"]
            else _sha256(upstream_bytes)
        )
        qualified_sources.append(
            {
                "path": path.as_posix(),
                "upstreamSha256": _sha256(upstream_bytes),
                "effectiveSha256": effective_digest,
                "domains": _v2_source_domains(path),
            }
        )

    changed_paths: list[dict[str, Any]] = []
    for status, path_string, classification, effect in V2_CHANGED_PATH_REVIEW:
        path = Path(path_string)
        preimage_digest = (
            None if status == "A" else _sha256((source_056 / path).read_bytes())
        )
        changed_paths.append(
            {
                "status": status,
                "path": path_string,
                "preimageSha256": preimage_digest,
                "postimageSha256": _sha256((source_0562 / path).read_bytes()),
                "classification": classification,
                "effect": effect,
            }
        )

    dependency_paths = []
    for path_string, expected_digest in V2_HYPRUTILS["paths"]:
        actual_digest = _sha256((hyprutils_0562 / path_string).read_bytes())
        if actual_digest != expected_digest:
            raise ValueError(
                f"Hyprutils 0.56.2 dependency provenance mismatch for {path_string}"
            )
        dependency_paths.append({"path": path_string, "sha256": actual_digest})
    lock_digest = _sha256((source_0562 / V2_HYPRUTILS["lockPath"]).read_bytes())
    if lock_digest != V2_HYPRUTILS["lockSha256"]:
        raise ValueError("Hyprland 0.56.2 dependency lock provenance mismatch")
    dependency_source = {
        "hyprlandVersion": V2_HYPRUTILS["hyprlandVersion"],
        "lockPath": V2_HYPRUTILS["lockPath"],
        "lockSha256": V2_HYPRUTILS["lockSha256"],
        "repository": V2_HYPRUTILS["repository"],
        "revision": V2_HYPRUTILS["revision"],
        "tree": V2_HYPRUTILS["tree"],
        "paths": dependency_paths,
    }
    documentation_urls = [
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
    ]
    source_manifest = {
        "formatVersion": 2,
        "repository": REPOSITORY,
        "reviewedOn": REVIEWED_ON,
        "predecessor": dict(V2_PREDECESSOR),
        "upstream": dict(V2_UPSTREAM),
        "qualifiedSources": qualified_sources,
        "dependencySources": [dependency_source],
        "changedPaths": changed_paths,
        "patches": [patch_record],
        "documentation": documentation_urls,
    }
    source_manifest_schema_path = (
        output_root / "interfaces/hyprland/v2/source-manifest.schema.json"
    )
    source_manifest_schema = _strict_json(
        source_manifest_schema_path.read_bytes(), source_manifest_schema_path.as_posix()
    )
    _assert_v2_manifest_schema_receipts(source_manifest, source_manifest_schema)
    source_manifest_digest = _sha256(_canonical_json_bytes(source_manifest))

    v1_catalog = _strict_json(
        v1_bytes("data/hyprland/config-catalog-v1.json"),
        "generated v1 config catalog",
    )
    v1_delta = _strict_json(
        v1_bytes("tests/fixtures/hyprland/v0.55.0-to-v0.56.1.delta.json"),
        "generated v1 scalar delta",
    )
    derived_options = [
        catalog_record(option, set(v1_delta["added"]))
        for option in sorted(upstream_options, key=lambda item: item.path)
    ]
    if derived_options != v1_catalog.get("options"):
        raise ValueError("v2 scalar catalog does not exactly preserve v1 semantics")
    if _complex_surfaces() != v1_catalog.get("complexSurfaces"):
        raise ValueError("v2 complex surfaces do not exactly preserve v1 semantics")
    catalog = {
        "contractVersion": 2,
        "sourceManifestDigest": source_manifest_digest,
        "hyprland": {
            "major": 0,
            "minor": 56,
            "reviewedVersion": V2_UPSTREAM["version"],
            "reviewedTag": V2_UPSTREAM["tag"],
            "reviewedCommit": V2_UPSTREAM["commit"],
            "repository": REPOSITORY,
            "minimumPatch": 2,
            "maximumPatch": 2,
        },
        "options": derived_options,
        "complexSurfaces": _complex_surfaces(),
        "compatibility": {
            "minimumSupported": V2_UPSTREAM["version"],
            "fullyQualified": [V2_UPSTREAM["version"]],
            "olderMinor": "migration",
            "newerMinor": "read-only",
            "unknownMajor": "unsupported",
        },
    }
    _assert_advanced_render_catalog(catalog)

    config_schema = (
        output_root / "interfaces/hyprland/v2/config.schema.json"
    ).read_bytes()
    dispatcher_source = (source_0562 / DISPATCHER_SOURCE_PATH).read_bytes()
    if _sha256(dispatcher_source) != (
        "a109eeb982856e0fe2ac9d88c29115a09984511787e19a20e7b4804e14a9d4de"
    ):
        raise ValueError("Hyprland 0.56.2 dispatcher source provenance mismatch")
    base_action_catalog = _action_catalog(dispatcher_source, config_schema)
    v1_action_catalog = _strict_json(
        v1_bytes("data/hyprland/action-catalog-v1.json"),
        "generated v1 action catalog",
    )
    for array_name in (
        "dispatcherActions",
        "semanticActions",
        "gestureActions",
        "excluded",
    ):
        if base_action_catalog[array_name] != v1_action_catalog[array_name]:
            raise ValueError(f"v2 {array_name} does not exactly preserve v1 semantics")
    action_catalog = {
        "contractVersion": 2,
        "hyprland": {
            "reviewedVersion": V2_UPSTREAM["version"],
            "reviewedTag": V2_UPSTREAM["tag"],
            "reviewedCommit": V2_UPSTREAM["commit"],
            "minimumPatch": 2,
            "maximumPatch": 2,
        },
        "sourceManifestDigest": source_manifest_digest,
        "configSchemaDigest": _sha256(config_schema),
        "source": {
            "repository": REPOSITORY,
            "tag": V2_UPSTREAM["tag"],
            "commit": V2_UPSTREAM["commit"],
            "path": DISPATCHER_SOURCE_PATH.as_posix(),
            "sha256": _sha256(dispatcher_source),
        },
        "dispatcherActions": base_action_catalog["dispatcherActions"],
        "semanticActions": base_action_catalog["semanticActions"],
        "gestureActions": base_action_catalog["gestureActions"],
        "excluded": base_action_catalog["excluded"],
    }

    catalog_digest = _sha256(_canonical_json_bytes(catalog))
    action_catalog_digest = _sha256(
        _canonical_json_bytes(action_catalog) + b"\n" + config_schema
    )
    template = _strict_json(
        v1_bytes("data/defaults/hyprland.json"), "generated v1 defaults"
    )
    if "authorityId" in template:
        raise ValueError("non-authoritative v2 template inherited an authorityId")
    template["formatVersion"] = 2
    template["targetHyprland"] = V2_UPSTREAM["version"]
    template["catalogDigest"] = catalog_digest
    template["actionCatalogDigest"] = action_catalog_digest
    generation_manifest = _v2_generation_manifest(
        template,
        source_manifest_digest,
        "0123456789abcdef0123456789abcdef",
    )

    return {
        Path("data/hyprland/config-catalog-v2.json"): _json_bytes(catalog),
        Path("data/hyprland/action-catalog-v2.json"): _json_bytes(action_catalog),
        Path("data/hyprland/source-manifest-v2.json"): _json_bytes(source_manifest),
        Path("data/defaults/hyprland-template.json"): _default_state_json_bytes(template),
        Path("tests/fixtures/hyprland/v0.56.2.scalar-options.json"): _json_bytes(
            upstream_fixture
        ),
        Path("tests/fixtures/hyprland/v0.56.1-to-v0.56.2.delta.json"): _json_bytes(
            delta
        ),
        Path("tests/fixtures/hyprland/generation-manifest-v2.json"): _json_bytes(
            generation_manifest
        ),
    }


def _v2_cli_inputs(
    source_0562: Path | None,
    hyprutils_0562: Path | None,
    protected_f1_patch_0562: Path | None,
) -> tuple[Path, Path, Path] | None:
    values = (source_0562, hyprutils_0562, protected_f1_patch_0562)
    if not any(value is not None for value in values):
        return None
    if not all(value is not None for value in values):
        raise ValueError(
            "--source-0562, --hyprutils-0562, and --protected-f1-patch-0562 "
            "must be supplied together"
        )
    assert source_0562 is not None
    assert hyprutils_0562 is not None
    assert protected_f1_patch_0562 is not None
    return source_0562, hyprutils_0562, protected_f1_patch_0562


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-055", type=Path, required=True)
    parser.add_argument("--source-0560", type=Path, required=True)
    parser.add_argument("--source-056", type=Path, required=True)
    parser.add_argument("--source-0562", type=Path)
    parser.add_argument("--hyprutils-055", type=Path, required=True)
    parser.add_argument("--hyprutils-056", type=Path, required=True)
    parser.add_argument("--hyprutils-0562", type=Path)
    parser.add_argument("--protected-f1-patch-0562", type=Path)
    parser.add_argument("--output-root", type=Path, default=Path.cwd())
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare generated bytes with checked-in files instead of writing",
    )
    arguments = parser.parse_args()

    try:
        v2_inputs = _v2_cli_inputs(
            arguments.source_0562,
            arguments.hyprutils_0562,
            arguments.protected_f1_patch_0562,
        )
        v1_documents = build_documents(
            arguments.source_055,
            arguments.source_0560,
            arguments.source_056,
            arguments.hyprutils_055,
            arguments.hyprutils_056,
            arguments.output_root,
        )
        documents = v1_documents
        if v2_inputs is not None:
            source_0562, hyprutils_0562, protected_f1_patch_0562 = v2_inputs
            v2_documents = build_v2_documents(
                arguments.source_056,
                source_0562,
                hyprutils_0562,
                protected_f1_patch_0562,
                arguments.output_root,
                v1_documents,
            )
            overlap = set(v1_documents) & set(v2_documents)
            if overlap:
                raise ValueError(
                    "v1/v2 generated output paths overlap: "
                    + ", ".join(path.as_posix() for path in sorted(overlap))
                )
            documents = {**v1_documents, **v2_documents}
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
