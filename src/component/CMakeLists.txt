qt_add_library(
    hyprshelld-component-core
    STATIC
        builtin_component_defaults.cpp
        builtin_component_defaults.h
        component_configuration.cpp
        component_configuration.h
        component_contract.cpp
        component_contract.h
        settings_schema.cpp
        settings_schema.h
        strict_json.cpp
        strict_json.h
        surface_plan.cpp
        surface_plan.h
        validation_result.h
)

set_target_properties(
    hyprshelld-component-core
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
    hyprshelld-component-core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-component-core
    PUBLIC
        Qt6::Core
)
