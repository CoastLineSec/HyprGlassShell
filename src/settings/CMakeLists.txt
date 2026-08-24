qt_add_executable(
    hyprshelld-settings
    main.cpp
)

qt_add_qml_module(
    hyprshelld-settings
    URI HyprShelld.Settings
    QML_FILES
        Main.qml
        AdvancedPage.qml
        AnimationCollectionsSummary.qml
        AnimationCurveEditor.qml
        AnimationRuleEditor.qml
        AppearancePage.qml
        AppearancePreview.qml
        BarPreview.qml
        BarSettingsPage.qml
        BindingEditor.qml
        BindingsPage.qml
        CompositorRecoveryDialog.qml
        ComponentPill.qml
        ComponentReviewDialog.qml
        ComponentsPage.qml
        DisplayAdoptionDialog.qml
        DisplaysPage.qml
        DisplaySettingsCard.qml
        DisplayTopologyPreview.qml
        EnvironmentVariablesPage.qml
        GenericComponentSettings.qml
        GestureEditor.qml
        GestureSummaryList.qml
        InputDevicesPane.qml
        InputDeviceEditor.qml
        InputDeviceOverrideRow.qml
        InputDevicePreview.qml
        InputDevicesPage.qml
        InputPage.qml
        HyprlandCatalogPage.qml
        HyprlandCategoryGraphic.qml
        HyprlandOptionRow.qml
        HyprlandOverviewPage.qml
        KeyboardShortcutsPage.qml
        PermissionsPage.qml
        RuleDecimalField.qml
        RuleOptionalField.qml
        RuleSafeIntegerField.qml
        RuleSummaryList.qml
        RulesPage.qml
        SchemaSettingRow.qml
        SettingsDecimalRow.qml
        SettingsSelectRow.qml
        SettingsSliderRow.qml
        SettingsSpinBoxRow.qml
        SettingsToggleRow.qml
        ShellHealthWarning.qml
        WorkspaceSwitcherSettings.qml
        WorkspaceRuleEditor.qml
        WorkspaceRuleSummaryList.qml
        WindowsLayoutPage.qml
        WorkspacesPage.qml
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
