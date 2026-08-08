qt_add_qml_module(
    hyprshelld-ui
    SHARED
    URI HyprShelld.UI
    QML_FILES
        Bar.qml
        BarHeightControl.qml
)

target_link_libraries(
    hyprshelld-ui
    PUBLIC
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
