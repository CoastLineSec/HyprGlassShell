set(HYPRSHELLD_CONFIG_CLIENT_PROXY_SOURCES)

qt_add_dbus_interface(
    HYPRSHELLD_CONFIG_CLIENT_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Config1.xml"
    config1_interface
)

qt_add_qml_module(
    hyprshelld-client
    SHARED
    URI HyprShelld.Client
    SOURCES
        config_client.cpp
        config_client.h
        ${HYPRSHELLD_CONFIG_CLIENT_PROXY_SOURCES}
)

target_include_directories(
    hyprshelld-client
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-client
    PUBLIC
        Qt6::Core
        Qt6::DBus
        Qt6::Qml
)
