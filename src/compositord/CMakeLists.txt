qt_add_library(
    hyprshelld-compositord-core
    STATIC
        renderer.cpp
        renderer.h
        generation.cpp
        generation.h
        store.cpp
        store.h
        transaction.cpp
        transaction.h
)

target_include_directories(
    hyprshelld-compositord-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-core
    PUBLIC
        Qt6::Core
        hyprshelld-hyprland-core
)

qt_add_library(
    hyprshelld-compositord-service
    STATIC
        activation_backend.cpp
        activation_backend.h
        entrypoint_publisher.cpp
        hyprland_ipc_runtime.cpp
        compositor_service.cpp
        compositor_service.h
        configuration_authority.h
)

target_include_directories(
    hyprshelld-compositord-service
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-service
    PUBLIC
        Qt6::Core
        Qt6::DBus
        hyprshelld-hyprland-core
)

set(HYPRSHELLD_COMPOSITOR_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_COMPOSITOR_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Compositor1.xml"
    compositor_service.h
    HyprShelld::Compositor::CompositorService
    compositor1_adaptor
    Compositor1Adaptor
)

qt_add_executable(
    hyprshelld-compositord
    main.cpp
    ${HYPRSHELLD_COMPOSITOR_ADAPTOR_SOURCES}
)

target_compile_definitions(
    hyprshelld-compositord
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/schemas/hyprland/v1/config.schema.json"
)

target_include_directories(
    hyprshelld-compositord
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    hyprshelld-compositord
    PRIVATE
        Qt6::Core
        Qt6::DBus
        hyprshelld-compositord-core
        hyprshelld-compositord-service
)

install(
    TARGETS hyprshelld-compositord
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
