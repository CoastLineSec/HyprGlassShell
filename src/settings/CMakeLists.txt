qt_add_executable(
    hyprshelld-settings
    main.cpp
)

qt_add_qml_module(
    hyprshelld-settings
    URI HyprShelld.Settings
    QML_FILES
        Main.qml
        BarPreview.qml
        BarSettingsPage.qml
        ComponentPill.qml
        ComponentReviewDialog.qml
        ComponentsPage.qml
        GenericComponentSettings.qml
        SchemaSettingRow.qml
        ShellHealthWarning.qml
        WorkspaceSwitcherSettings.qml
)

target_link_libraries(
    hyprshelld-settings
    PRIVATE
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        hyprshelld-client
        hyprshelld-ui
)

target_compile_definitions(
    hyprshelld-settings
    PRIVATE
        HYPRSHELLD_SETTINGS_DESKTOP_ID="${HYPRSHELLD_SETTINGS_DESKTOP_ID}"
)

file(
    RELATIVE_PATH settings_library_path
    "${CMAKE_INSTALL_FULL_BINDIR}"
    "${CMAKE_INSTALL_FULL_LIBDIR}"
)

set_target_properties(
    hyprshelld-settings
    PROPERTIES
        INSTALL_RPATH "\$ORIGIN/${settings_library_path}"
)

install(
    TARGETS hyprshelld-settings
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
