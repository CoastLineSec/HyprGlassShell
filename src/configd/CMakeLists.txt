qt_add_library(
    hyprshelld-configd-core
    STATIC
        component_catalog_client.cpp
        component_catalog_client.h
        component_config_service.cpp
        component_config_service.h
        component_store.cpp
        component_store.h
        config_service.cpp
        config_service.h
        config_store.cpp
        config_store.h
        legacy_workspace_settings.h
)

target_include_directories(
    hyprshelld-configd-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-configd-core
    PUBLIC
        Qt6::Core
        Qt6::DBus
        hyprshelld-component-core
)

set(HYPRSHELLD_CONFIGD_ADAPTOR_SOURCES)
set(HYPRSHELLD_COMPONENT_CONFIG_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_CONFIGD_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Config1.xml"
    config_service.h
    HyprShelld::ConfigService
    config1_adaptor
    Config1Adaptor
)

qt_add_dbus_adaptor(
    HYPRSHELLD_COMPONENT_CONFIG_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentConfig1.xml"
    component_config_service.h
    HyprShelld::ComponentConfigService
    component_config1_adaptor
    ComponentConfig1Adaptor
)

qt_add_executable(
    hyprshelld-configd
    main.cpp
    ${HYPRSHELLD_CONFIGD_ADAPTOR_SOURCES}
    ${HYPRSHELLD_COMPONENT_CONFIG_ADAPTOR_SOURCES}
)

target_compile_definitions(
    hyprshelld-configd
    PRIVATE
        HYPRSHELLD_COMPONENT_DEFAULTS_FILE="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/defaults/components.json"
)

target_include_directories(
    hyprshelld-configd
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    hyprshelld-configd
    PRIVATE
        Qt6::Core
        Qt6::DBus
        hyprshelld-configd-core
)

install(
    TARGETS hyprshelld-configd
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
