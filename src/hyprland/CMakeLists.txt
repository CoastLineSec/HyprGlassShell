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
        json_support.cpp
        json_support.h
        validation.h
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
