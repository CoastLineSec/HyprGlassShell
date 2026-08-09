foreach(required IN ITEMS
    CMAKE_EXECUTABLE
    BUILD_DIRECTORY
    STAGE_DIRECTORY
    INSTALL_PREFIX
    INSTALL_BINDIR
    INSTALL_LIBDIR
    INSTALL_DATADIR
    INSTALL_QMLDIR
    INSTALL_SYSTEMD_UNIT_DIR
    QML_EXECUTABLE
    DBUS_RUN_SESSION
    DBUS_CONFIG
    MODULE_PROBE
    DESKTOP_FILE_VALIDATE
    SETTINGS_SMOKE_EXECUTABLE
    SETTINGS_DESKTOP_ID
    PROJECT_SOURCE_DIRECTORY
    PROJECT_BINARY_DIRECTORY
)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing required argument: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${STAGE_DIRECTORY}")
file(MAKE_DIRECTORY "${STAGE_DIRECTORY}")

set(install_command "${CMAKE_EXECUTABLE}" --install "${BUILD_DIRECTORY}")
if(DEFINED BUILD_CONFIG AND NOT "${BUILD_CONFIG}" STREQUAL "")
    list(APPEND install_command --config "${BUILD_CONFIG}")
endif()

execute_process(
    COMMAND
        "${CMAKE_EXECUTABLE}"
        -E
        env
        "DESTDIR=${STAGE_DIRECTORY}"
        ${install_command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 30
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Staged installation failed\nstdout:\n${output}\nstderr:\n${error}"
    )
endif()

set(bin_root "${STAGE_DIRECTORY}${INSTALL_BINDIR}")
set(lib_root "${STAGE_DIRECTORY}${INSTALL_LIBDIR}")
set(data_root "${STAGE_DIRECTORY}${INSTALL_DATADIR}")
set(qml_root "${STAGE_DIRECTORY}${INSTALL_QMLDIR}")
set(systemd_root "${STAGE_DIRECTORY}${INSTALL_SYSTEMD_UNIT_DIR}")

set(required_files
    "${bin_root}/hyprshelld"
    "${bin_root}/hyprshelld-configd"
    "${bin_root}/hyprshelld-componentd"
    "${bin_root}/hyprshelld-settings"
    "${lib_root}/libhyprshelld-client.so"
    "${lib_root}/libhyprshelld-ui.so"
    "${systemd_root}/hyprshelld.target"
    "${systemd_root}/session-hyprshelld.slice"
    "${systemd_root}/hyprshelld.service"
    "${systemd_root}/hyprshelld-configd.service"
    "${systemd_root}/hyprshelld-componentd.service"
    "${systemd_root}/hyprshelld-surfaced.service"
    "${data_root}/dbus-1/services/org.hyprshelld.Config1.service"
    "${data_root}/dbus-1/services/org.hyprshelld.ComponentManager1.service"
    "${data_root}/applications/${SETTINGS_DESKTOP_ID}.desktop"
    "${data_root}/hyprshelld/components/io.github.coastlinesec.hyprshelld.workspace-switcher/manifest.json"
    "${data_root}/hyprshelld/components/io.github.coastlinesec.hyprshelld.workspace-switcher/settings.schema.json"
    "${data_root}/hyprshelld/defaults/components.json"
    "${data_root}/hyprshelld/dbus/org.hyprshelld.ComponentRuntime1.xml"
    "${data_root}/hyprshelld/schemas/components/v1/manifest.schema.json"
    "${data_root}/hyprshelld/schemas/components/v1/integrity.schema.json"
    "${data_root}/hyprshelld/schemas/components/v1/settings.schema.json"
    "${data_root}/hyprshelld/schemas/components/v1/surface-plan.schema.json"
    "${data_root}/hyprshelld/surfaced/shell.qml"
    "${data_root}/hyprshelld/surfaced/BarSurface.qml"
    "${data_root}/hyprshelld/surfaced/HyprlandWorkspaceSource.qml"
    "${data_root}/hyprshelld/surfaced/HyprlandSocketRequest.qml"
    "${data_root}/hyprshelld/surfaced/HyprlandEventStream.qml"
    "${data_root}/hyprshelld/surfaced/HyprlandWorkspaceProtocol.js"
    "${data_root}/hyprshelld/surfaced/WorkspaceProjection.js"
    "${data_root}/hyprshelld/surfaced/components/BarComponentHost.qml"
    "${data_root}/hyprshelld/surfaced/components/BuiltinComponentFactory.qml"
    "${data_root}/hyprshelld/surfaced/components/WorkspaceSwitcherComponent.qml"
    "${qml_root}/HyprShelld/Client/qmldir"
    "${qml_root}/HyprShelld/Client/hyprshelld-client.qmltypes"
    "${qml_root}/HyprShelld/Client/libhyprshelld-clientplugin.so"
    "${qml_root}/HyprShelld/UI/qmldir"
    "${qml_root}/HyprShelld/UI/hyprshelld-ui.qmltypes"
    "${qml_root}/HyprShelld/UI/libhyprshelld-uiplugin.so"
    "${qml_root}/HyprShelld/UI/Bar.qml"
    "${qml_root}/HyprShelld/UI/BarHeightControl.qml"
    "${qml_root}/HyprShelld/UI/WorkspaceSwitcher.qml"
)

foreach(path IN LISTS required_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Staged installation is missing: ${path}")
    endif()
endforeach()

set(
    settings_desktop_file
    "${data_root}/applications/${SETTINGS_DESKTOP_ID}.desktop"
)

execute_process(
    COMMAND "${DESKTOP_FILE_VALIDATE}" "${settings_desktop_file}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 10
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "The staged Settings desktop entry is invalid\n"
        "stdout:\n${output}\n"
        "stderr:\n${error}"
    )
endif()

file(READ "${settings_desktop_file}" settings_desktop_contents)
set(settings_desktop_contents "\n${settings_desktop_contents}")

foreach(required_line IN ITEMS
    "[Desktop Entry]"
    "Type=Application"
    "Name=HyprShelld Settings"
    "Exec=hyprshelld-settings"
    "TryExec=hyprshelld-settings"
    "Icon=preferences-system"
    "Terminal=false"
    "Categories=Settings;DesktopSettings;Qt;"
    "StartupNotify=true"
    "StartupWMClass=${SETTINGS_DESKTOP_ID}"
)
    string(FIND "${settings_desktop_contents}" "\n${required_line}\n" line_index)
    if(line_index EQUAL -1)
        message(FATAL_ERROR
            "The staged Settings desktop entry is missing: ${required_line}"
        )
    endif()
endforeach()

file(
    GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${bin_root}/hyprshelld-settings"
    RESOLVED_DEPENDENCIES_VAR settings_resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR settings_unresolved_dependencies
)

if(settings_unresolved_dependencies)
    message(FATAL_ERROR
        "The staged Settings application has unresolved dependencies: "
        "${settings_unresolved_dependencies}"
    )
endif()

set(settings_expected_product_dependencies
    "${lib_root}/libhyprshelld-client.so"
    "${lib_root}/libhyprshelld-ui.so"
)

cmake_path(NORMAL_PATH STAGE_DIRECTORY OUTPUT_VARIABLE normalized_stage_directory)
set(settings_normalized_dependencies)

foreach(dependency IN LISTS settings_resolved_dependencies)
    cmake_path(NORMAL_PATH dependency OUTPUT_VARIABLE normalized_dependency)
    list(APPEND settings_normalized_dependencies "${normalized_dependency}")

    string(FIND
        "${normalized_dependency}"
        "${normalized_stage_directory}/"
        stage_index
    )
    string(FIND
        "${normalized_dependency}"
        "${PROJECT_SOURCE_DIRECTORY}/"
        source_index
    )
    string(FIND
        "${normalized_dependency}"
        "${PROJECT_BINARY_DIRECTORY}/"
        binary_index
    )
    if(
        NOT stage_index EQUAL 0
        AND (NOT source_index EQUAL -1 OR NOT binary_index EQUAL -1)
    )
        message(FATAL_ERROR
            "The staged Settings application resolves a development dependency: "
            "${normalized_dependency}"
        )
    endif()
endforeach()

foreach(expected_dependency IN LISTS settings_expected_product_dependencies)
    cmake_path(
        NORMAL_PATH expected_dependency
        OUTPUT_VARIABLE normalized_expected_dependency
    )
    list(
        FIND
        settings_normalized_dependencies
        "${normalized_expected_dependency}"
        dependency_index
    )
    if(dependency_index EQUAL -1)
        message(FATAL_ERROR
            "The staged Settings application did not resolve its staged library: "
            "${normalized_expected_dependency}"
        )
    endif()
endforeach()

set(probe_config "${STAGE_DIRECTORY}/probe-config")
set(probe_state "${STAGE_DIRECTORY}/probe-state")
set(probe_data "${STAGE_DIRECTORY}/probe-data")
set(probe_system_data "${STAGE_DIRECTORY}/probe-system-data")
file(MAKE_DIRECTORY
    "${probe_config}"
    "${probe_state}"
    "${probe_data}"
    "${probe_system_data}"
)
execute_process(
    COMMAND
        "${CMAKE_EXECUTABLE}"
        -E
        env
        --unset=LD_LIBRARY_PATH
        --unset=QML2_IMPORT_PATH
        "QML_IMPORT_PATH=${qml_root}"
        "QT_QPA_PLATFORM=offscreen"
        "QT_FATAL_WARNINGS=1"
        "XDG_CONFIG_HOME=${probe_config}"
        "XDG_STATE_HOME=${probe_state}"
        "XDG_DATA_HOME=${probe_data}"
        "XDG_DATA_DIRS=${probe_system_data}"
        "${DBUS_RUN_SESSION}"
        "--config-file=${DBUS_CONFIG}"
        --
        "${QML_EXECUTABLE}"
        -import
        "${qml_root}"
        -input
        "${MODULE_PROBE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 15
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Staged QML modules could not be loaded externally\n"
        "stdout:\n${output}\n"
        "stderr:\n${error}"
    )
endif()

execute_process(
    COMMAND
        "${CMAKE_EXECUTABLE}"
        -E
        env
        --unset=LD_LIBRARY_PATH
        --unset=QML_IMPORT_PATH
        --unset=QML2_IMPORT_PATH
        --unset=QT_PLUGIN_PATH
        --unset=QT_QPA_PLATFORM_PLUGIN_PATH
        "HYPRSHELLD_SETTINGS_EXECUTABLE=${bin_root}/hyprshelld-settings"
        "XDG_DATA_HOME=${probe_data}"
        "XDG_DATA_DIRS=${probe_system_data}"
        "${DBUS_RUN_SESSION}"
        "--config-file=${DBUS_CONFIG}"
        --
        "${SETTINGS_SMOKE_EXECUTABLE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 15
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "The staged Settings application could not be launched\n"
        "stdout:\n${output}\n"
        "stderr:\n${error}"
    )
endif()
