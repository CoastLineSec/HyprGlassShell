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
    "${lib_root}/libhyprshelld-client.so"
    "${lib_root}/libhyprshelld-ui.so"
    "${systemd_root}/hyprshelld.target"
    "${systemd_root}/session-hyprshelld.slice"
    "${systemd_root}/hyprshelld.service"
    "${systemd_root}/hyprshelld-configd.service"
    "${systemd_root}/hyprshelld-surfaced.service"
    "${data_root}/dbus-1/services/org.hyprshelld.Config1.service"
    "${data_root}/hyprshelld/surfaced/shell.qml"
    "${data_root}/hyprshelld/surfaced/BarSurface.qml"
    "${qml_root}/HyprShelld/Client/qmldir"
    "${qml_root}/HyprShelld/Client/hyprshelld-client.qmltypes"
    "${qml_root}/HyprShelld/Client/libhyprshelld-clientplugin.so"
    "${qml_root}/HyprShelld/UI/qmldir"
    "${qml_root}/HyprShelld/UI/hyprshelld-ui.qmltypes"
    "${qml_root}/HyprShelld/UI/libhyprshelld-uiplugin.so"
    "${qml_root}/HyprShelld/UI/Bar.qml"
    "${qml_root}/HyprShelld/UI/BarHeightControl.qml"
)

foreach(path IN LISTS required_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Staged installation is missing: ${path}")
    endif()
endforeach()

if(EXISTS "${bin_root}/hyprshelld-settings")
    message(FATAL_ERROR "Staged runtime unexpectedly includes hyprshelld-settings")
endif()

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
