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
        ShellHealthWarning.qml
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
