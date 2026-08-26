pkg_check_modules(
    HyprlandXkbcommon
    REQUIRED
    IMPORTED_TARGET
    xkbcommon>=1.0
)
pkg_check_modules(
    HyprlandRe2
    REQUIRED
    IMPORTED_TARGET
    re2>=11.0
)

qt_add_library(
    hyprshelld-hyprland-core
    STATIC
        catalog.cpp
        catalog.h
        action_catalog.cpp
        action_catalog.h
        desired_state.cpp
        desired_state.h
        monitor_profile.cpp
        monitor_profile.h
        input_device_inventory.cpp
        input_device_inventory.h
        default_keybindings.cpp
        default_keybindings.h
        keyboard_shortcut_reference.cpp
        keyboard_shortcut_reference.h
        json_support.cpp
        json_support.h
        validation.h
)

qt_add_resources(
    hyprshelld-hyprland-core
    hyprshelld_keyboard_shortcut_reference
    PREFIX
        "/hyprshelld/hyprland"
    BASE
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/keyboard-shortcuts-reference-v1.json"
)

set_target_properties(
    hyprshelld-hyprland-core
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
    hyprshelld-hyprland-core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-hyprland-core
    PUBLIC
        Qt6::Core
    PRIVATE
        hyprshelld-component-core
        PkgConfig::HyprlandXkbcommon
        PkgConfig::HyprlandRe2
)
