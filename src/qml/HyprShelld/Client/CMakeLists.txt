set(HYPRSHELLD_CONFIG_CLIENT_PROXY_SOURCES)

set(HYPRSHELLD_COORDINATOR_CLIENT_PROXY_SOURCES)

qt_add_dbus_interface(
    HYPRSHELLD_COORDINATOR_CLIENT_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Coordinator1.xml"
    coordinator1_interface
)

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
        component_config_client.cpp
        component_config_client.h
        component_manager_client.cpp
        component_manager_client.h
        component_runtime_client.cpp
        component_runtime_client.h
        compositor_client.cpp
        compositor_client.h
        input_device_projection.cpp
        input_device_projection.h
        keyboard_shortcut_reference_model.cpp
        keyboard_shortcut_reference_model.h
        compositor_option_catalog.cpp
        compositor_option_catalog.h
        compositor_snapshot_editor.cpp
        compositor_snapshot_editor.h
        coordinator_client.cpp
        coordinator_client.h
        config_client.cpp
        config_client.h
        shell_runtime_status.cpp
        shell_runtime_status.h
        ${HYPRSHELLD_COORDINATOR_CLIENT_PROXY_SOURCES}
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
        hyprshelld-component-core
        hyprshelld-hyprland-core
)

qt_query_qml_module(
    hyprshelld-client
    PLUGIN_TARGET module_plugin
    TARGET_PATH module_target_path
    QMLDIR module_qmldir
    TYPEINFO module_typeinfo
)

set(module_install_dir "${QT6_INSTALL_QML}/${module_target_path}")

if(IS_ABSOLUTE "${module_install_dir}")
    set(module_install_absolute "${module_install_dir}")
else()
    set(module_install_absolute "${CMAKE_INSTALL_PREFIX}/${module_install_dir}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(library_install_absolute "${CMAKE_INSTALL_LIBDIR}")
else()
    set(library_install_absolute "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()

file(
    RELATIVE_PATH module_library_path
    "${module_install_absolute}"
    "${library_install_absolute}"
)

set_target_properties(
    "${module_plugin}"
    PROPERTIES
        INSTALL_RPATH "\$ORIGIN/${module_library_path}"
)

install(
    TARGETS hyprshelld-client
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
    TARGETS "${module_plugin}"
    ARCHIVE DESTINATION "${module_install_dir}"
    LIBRARY DESTINATION "${module_install_dir}"
    RUNTIME DESTINATION "${module_install_dir}"
)

install(
    FILES
        "${module_qmldir}"
        "${module_typeinfo}"
    DESTINATION "${module_install_dir}"
)
