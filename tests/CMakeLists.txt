qt_add_executable(
    hyprshelld-toolchain-test
    qt_toolchain_test.cpp
)

target_link_libraries(
    hyprshelld-toolchain-test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME toolchain
    COMMAND hyprshelld-toolchain-test
)

add_test(
    NAME quickshell-version
    COMMAND "${QUICKSHELL_EXECUTABLE}" --version
)

set_tests_properties(
    quickshell-version
    PROPERTIES
        PASS_REGULAR_EXPRESSION
            "^Quickshell 0\\.3\\.0 \\(revision .*, distributed by Arch Linux\\)"
)

set(
    HYPRSHELLD_SCRIPT_MODEL_TEST_ROOT
    "${PROJECT_BINARY_DIR}/.qsm"
)
foreach(directory config state cache data runtime)
    file(
        MAKE_DIRECTORY
        "${HYPRSHELLD_SCRIPT_MODEL_TEST_ROOT}/${directory}"
    )
endforeach()
file(
    CHMOD
    "${HYPRSHELLD_SCRIPT_MODEL_TEST_ROOT}/runtime"
    PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
)

add_test(
    NAME quickshell-script-model
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQUICKSHELL_EXECUTABLE=${QUICKSHELL_EXECUTABLE}"
        "-DQML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
        "-DTEST_BUILD_ROOT=${PROJECT_BINARY_DIR}"
        "-DTEST_ROOT=${HYPRSHELLD_SCRIPT_MODEL_TEST_ROOT}"
        "-DTEST_QML=${CMAKE_CURRENT_SOURCE_DIR}/quickshell/scriptmodel/shell.qml"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/run_quickshell_script_model.cmake"
)

set_tests_properties(
    quickshell-script-model
    PROPERTIES
        PASS_REGULAR_EXPRESSION "SCRIPT_MODEL_RECONCILIATION_PASS"
        FAIL_REGULAR_EXPRESSION "SCRIPT_MODEL_RECONCILIATION_FAIL"
        TIMEOUT 15
        RUN_SERIAL TRUE
)

set(
    HYPRSHELLD_COMPONENT_HOSTS_TEST_ROOT
    "${PROJECT_BINARY_DIR}/.qch"
)

add_test(
    NAME quickshell-component-hosts
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQUICKSHELL_EXECUTABLE=${QUICKSHELL_EXECUTABLE}"
        "-DQML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
        "-DSHARED_UI_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/qml/HyprShelld/UI"
        "-DSURFACED_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/surfaced"
        "-DTEST_BUILD_ROOT=${PROJECT_BINARY_DIR}"
        "-DTEST_ROOT=${HYPRSHELLD_COMPONENT_HOSTS_TEST_ROOT}"
        "-DTEST_QML=${CMAKE_CURRENT_SOURCE_DIR}/quickshell/component-hosts/shell.qml"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/run_quickshell_component_hosts.cmake"
)

set_tests_properties(
    quickshell-component-hosts
    PROPERTIES
        PASS_REGULAR_EXPRESSION "COMPONENT_HOSTS_PASS"
        FAIL_REGULAR_EXPRESSION "COMPONENT_HOSTS_FAIL"
        TIMEOUT 15
        RUN_SERIAL TRUE
)

set(
    HYPRSHELLD_HYPRLAND_SOURCE_TEST_ROOT
    "${PROJECT_BINARY_DIR}/.hws"
)

add_test(
    NAME quickshell-hyprland-source
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQUICKSHELL_EXECUTABLE=${QUICKSHELL_EXECUTABLE}"
        "-DQML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
        "-DSURFACED_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/surfaced"
        "-DTEST_BUILD_ROOT=${PROJECT_BINARY_DIR}"
        "-DTEST_ROOT=${HYPRSHELLD_HYPRLAND_SOURCE_TEST_ROOT}"
        "-DTEST_QML=${CMAKE_CURRENT_SOURCE_DIR}/quickshell/hyprland-source-shell.qml"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/run_quickshell_hyprland_source.cmake"
)

set_tests_properties(
    quickshell-hyprland-source
    PROPERTIES
        PASS_REGULAR_EXPRESSION "HYPRLAND_SOURCE_PASS"
        FAIL_REGULAR_EXPRESSION "HYPRLAND_SOURCE_FAIL"
        TIMEOUT 25
        RUN_SERIAL TRUE
)

set(
    HYPRSHELLD_QUALIFICATION_QML
    "${CMAKE_CURRENT_SOURCE_DIR}/quickshell/qualification/shell.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/quickshell/qualification/QualificationSurface.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/quickshell/qualification/QualificationBar.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/quickshell/qualification/QualificationPopout.qml"
)

add_test(
    NAME quickshell-qml-lint
    COMMAND
        "$<TARGET_FILE:Qt6::qmllint>"
        -W
        0
        ${HYPRSHELLD_QUALIFICATION_QML}
)

add_test(
    NAME shared-qml-lint
    COMMAND
        "$<TARGET_FILE:Qt6::qmllint>"
        -W
        0
        --bare
        -I
        "${QT_QML_OUTPUT_DIRECTORY}"
        -I
        "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
        "${QT_QML_OUTPUT_DIRECTORY}/HyprShelld/UI/Bar.qml"
        "${QT_QML_OUTPUT_DIRECTORY}/HyprShelld/UI/BarHeightControl.qml"
        "${QT_QML_OUTPUT_DIRECTORY}/HyprShelld/UI/ShellTheme.qml"
        "${QT_QML_OUTPUT_DIRECTORY}/HyprShelld/UI/WorkspaceSwitcher.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/Main.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/AppearancePage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/AppearancePreview.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/BarPreview.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/BarSettingsPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/BindingEditor.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/BindingsPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/CompositorRecoveryDialog.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/ComponentPill.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/ComponentReviewDialog.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/ComponentsPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/DisplayAdoptionDialog.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/DisplaysPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/DisplaySettingsCard.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/DisplayTopologyPreview.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/EnvironmentVariablesPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/GenericComponentSettings.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/GestureEditor.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/GestureSummaryList.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/InputPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/InputDevicesPane.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/InputDeviceEditor.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/InputDeviceOverrideRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/InputDevicePreview.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/InputDevicesPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/HyprlandCatalogPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/HyprlandCategoryGraphic.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/HyprlandOptionRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/HyprlandOverviewPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/KeyboardShortcutsPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/PermissionsPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/RuleDecimalField.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/RuleOptionalField.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/RuleSafeIntegerField.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/RuleSummaryList.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/RulesPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/SchemaSettingRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/SettingsDecimalRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/SettingsSelectRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/SettingsSliderRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/SettingsSpinBoxRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/SettingsToggleRow.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/ShellHealthWarning.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/ThemeModeSelector.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/WindowsLayoutPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/WorkspaceRuleEditor.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/WorkspaceRuleSummaryList.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/WorkspacesPage.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/WorkspaceSwitcherSettings.qml"
)

add_test(
    NAME surfaced-qml-lint
    COMMAND
        "$<TARGET_FILE:Qt6::qmllint>"
        -W
        0
        --bare
        -I
        "${QT_QML_OUTPUT_DIRECTORY}"
        -I
        "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
        "${PROJECT_SOURCE_DIR}/src/surfaced/shell.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/BarSurface.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/HyprlandWorkspaceSource.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/HyprlandSocketRequest.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/HyprlandEventStream.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/HyprlandWorkspaceProtocol.js"
        "${PROJECT_SOURCE_DIR}/src/surfaced/WorkspaceProjection.js"
        "${PROJECT_SOURCE_DIR}/src/surfaced/components/BarComponentHost.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/components/BuiltinComponentFactory.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/components/ComponentFactory.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/components/DeclarativeComponentFactory.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/components/WorkspaceSwitcherComponent.qml"
)

set(HYPRSHELLD_COORDINATOR_PROXY_SOURCES)

qt_add_dbus_interface(
    HYPRSHELLD_COORDINATOR_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Coordinator1.xml"
    coordinator1_interface
)

set(HYPRSHELLD_CONFIG_PROXY_SOURCES)

qt_add_dbus_interface(
    HYPRSHELLD_CONFIG_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Config1.xml"
    config1_interface
)

set(HYPRSHELLD_COMPONENT_MANAGER_PROXY_SOURCES)
set(HYPRSHELLD_COMPONENT_CONFIG_PROXY_SOURCES)
set(HYPRSHELLD_COMPONENT_RUNTIME_PROXY_SOURCES)
set(HYPRSHELLD_COMPOSITOR_PROXY_SOURCES)

qt_add_dbus_interface(
    HYPRSHELLD_COMPONENT_MANAGER_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentManager1.xml"
    component_manager1_interface
)

qt_add_dbus_interface(
    HYPRSHELLD_COMPONENT_CONFIG_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentConfig1.xml"
    component_config1_interface
)

qt_add_dbus_interface(
    HYPRSHELLD_COMPONENT_RUNTIME_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentRuntime1.xml"
    component_runtime1_interface
)

qt_add_dbus_interface(
    HYPRSHELLD_COMPOSITOR_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Compositor1.xml"
    compositor1_interface
)

set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Compositor2.xml"
    PROPERTIES
        INCLUDE "compositord/compositor2_dbus_types.h"
)

qt_add_dbus_interface(
    HYPRSHELLD_COMPOSITOR_PROXY_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Compositor2.xml"
    compositor2_interface
)

qt_add_executable(
    hyprshelld-dbus-contract-test
    dbus_contract_test.cpp
    ${HYPRSHELLD_COORDINATOR_PROXY_SOURCES}
    ${HYPRSHELLD_CONFIG_PROXY_SOURCES}
    ${HYPRSHELLD_COMPONENT_MANAGER_PROXY_SOURCES}
    ${HYPRSHELLD_COMPONENT_CONFIG_PROXY_SOURCES}
    ${HYPRSHELLD_COMPONENT_RUNTIME_PROXY_SOURCES}
    ${HYPRSHELLD_COMPOSITOR_PROXY_SOURCES}
)

target_include_directories(
    hyprshelld-dbus-contract-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_compile_definitions(
    hyprshelld-dbus-contract-test
    PRIVATE
        HYPRSHELLD_COORDINATOR_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Coordinator1.xml"
        HYPRSHELLD_CONFIG_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Config1.xml"
        HYPRSHELLD_COMPONENT_MANAGER_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentManager1.xml"
        HYPRSHELLD_COMPONENT_CONFIG_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentConfig1.xml"
        HYPRSHELLD_COMPONENT_RUNTIME_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentRuntime1.xml"
        HYPRSHELLD_COMPOSITOR_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Compositor1.xml"
)

target_link_libraries(
    hyprshelld-dbus-contract-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(
    NAME dbus-contract
    COMMAND hyprshelld-dbus-contract-test
)

qt_add_executable(
    hyprshelld-coordinator-policy-test
    coordinator_policy_test.cpp
)

target_link_libraries(
    hyprshelld-coordinator-policy-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-coordinator-core
)

add_test(
    NAME coordinator-policy
    COMMAND hyprshelld-coordinator-policy-test
)

qt_add_executable(
    hyprshelld-surface-plan-test
    surface_plan_test.cpp
)

target_link_libraries(
    hyprshelld-surface-plan-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-component-core
)

add_test(
    NAME surface-plan
    COMMAND hyprshelld-surface-plan-test
)

qt_add_executable(
    hyprshelld-component-plan-test
    component_plan_test.cpp
)

target_link_libraries(
    hyprshelld-component-plan-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-coordinator-core
)

add_test(
    NAME component-plan
    COMMAND hyprshelld-component-plan-test
)

qt_add_executable(
    hyprshelld-component-runtime-health-test
    component_runtime_health_test.cpp
)

target_link_libraries(
    hyprshelld-component-runtime-health-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-coordinator-core
)

add_test(
    NAME component-runtime-health
    COMMAND hyprshelld-component-runtime-health-test
)

qt_add_executable(
    hyprshelld-component-plan-source-test
    component_plan_source_test.cpp
)

target_link_libraries(
    hyprshelld-component-plan-source-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-coordinator-core
)

add_test(
    NAME component-plan-source
    COMMAND hyprshelld-component-plan-source-test
)

qt_add_executable(
    hyprshelld-component-plan-hydrator-test
    component_plan_hydrator_test.cpp
)

target_link_libraries(
    hyprshelld-component-plan-hydrator-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-coordinator-core
)

qt_add_executable(
    hyprshelld-component-contract-test
    component_contract_test.cpp
)

target_link_libraries(
    hyprshelld-component-contract-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-component-core
)

add_test(
    NAME component-contract
    COMMAND hyprshelld-component-contract-test
)

qt_add_executable(
    hyprshelld-component-settings-schema-test
    component_settings_schema_test.cpp
)

target_link_libraries(
    hyprshelld-component-settings-schema-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-component-core
)

add_test(
    NAME component-settings-schema
    COMMAND hyprshelld-component-settings-schema-test
)

qt_add_executable(
    hyprshelld-declarative-document-test
    declarative_document_test.cpp
)

target_link_libraries(
    hyprshelld-declarative-document-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-component-core
)

add_test(
    NAME declarative-document
    COMMAND hyprshelld-declarative-document-test
)

set(
    HYPRSHELLD_COMPONENT_DEFAULTS_FILE
    "${PROJECT_SOURCE_DIR}/data/defaults/components.json"
)
set(
    HYPRSHELLD_WORKSPACE_SCHEMA_FILE
    "${PROJECT_SOURCE_DIR}/data/components/io.github.coastlinesec.hyprshelld.workspace-switcher/settings.schema.json"
)

qt_add_executable(
    hyprshelld-component-configuration-test
    component_configuration_test.cpp
)

target_compile_definitions(
    hyprshelld-component-configuration-test
    PRIVATE
        HYPRSHELLD_COMPONENT_DEFAULTS_FILE="${HYPRSHELLD_COMPONENT_DEFAULTS_FILE}"
        HYPRSHELLD_WORKSPACE_SCHEMA_FILE="${HYPRSHELLD_WORKSPACE_SCHEMA_FILE}"
        HYPRSHELLD_SOURCE_COMPONENT_DIR="${PROJECT_SOURCE_DIR}/data/components"
)

target_include_directories(
    hyprshelld-component-configuration-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/configd"
)

target_link_libraries(
    hyprshelld-component-configuration-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-component-core
        hyprshelld-componentd-core
)

add_test(
    NAME component-configuration
    COMMAND hyprshelld-component-configuration-test
)

qt_add_executable(
    hyprshelld-hyprland-configuration-test
    hyprland_configuration_test.cpp
)

target_compile_definitions(
    hyprshelld-hyprland-configuration-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_SOURCE_MANIFEST_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/source-manifest-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland-template.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_CATALOG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/catalog.schema.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/action-catalog.schema.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
        HYPRSHELLD_HYPRLAND_GENERATION_MANIFEST_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/generation-manifest.schema.json"
        HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/source-manifest.schema.json"
        HYPRSHELLD_HYPRLAND_056_FIXTURE_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/v0.56.1.scalar-options.json"
        HYPRSHELLD_HYPRLAND_055_FIXTURE_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/v0.55.0.scalar-options.json"
        HYPRSHELLD_HYPRLAND_DELTA_FIXTURE_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/v0.55.0-to-v0.56.1.delta.json"
        HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/source-manifest.json"
)

target_link_libraries(
    hyprshelld-hyprland-configuration-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-hyprland-core
)

add_test(
    NAME hyprland-configuration
    COMMAND hyprshelld-hyprland-configuration-test
)

qt_add_executable(
    hyprshelld-hyprland-monitor-profile-test
    hyprland_monitor_profile_test.cpp
)

target_compile_definitions(
    hyprshelld-hyprland-monitor-profile-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
)

target_link_libraries(
    hyprshelld-hyprland-monitor-profile-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-hyprland-core
)

add_test(
    NAME hyprland-monitor-profile
    COMMAND hyprshelld-hyprland-monitor-profile-test
)

qt_add_executable(
    hyprshelld-hyprland-input-device-inventory-test
    hyprland_input_device_inventory_test.cpp
)

target_link_libraries(
    hyprshelld-hyprland-input-device-inventory-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-hyprland-core
)

add_test(
    NAME hyprland-input-device-inventory
    COMMAND hyprshelld-hyprland-input-device-inventory-test
)

qt_add_executable(
    hyprshelld-keyboard-shortcut-reference-test
    keyboard_shortcut_reference_test.cpp
)

target_compile_definitions(
    hyprshelld-keyboard-shortcut-reference-test
    PRIVATE
        HYPRSHELLD_LEGACY_SHORTCUT_REFERENCE_FILE="${PROJECT_SOURCE_DIR}/src/hyprland/resources/keyboard-shortcuts-reference-v1.json"
)

target_link_libraries(
    hyprshelld-keyboard-shortcut-reference-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-hyprland-core
)

add_test(
    NAME keyboard-shortcut-reference
    COMMAND hyprshelld-keyboard-shortcut-reference-test
)

qt_add_executable(
    hyprshelld-compositor-identity-test
    compositor_identity_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-identity-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-identity
    COMMAND hyprshelld-compositor-identity-test
)

find_program(HYPRSHELLD_NODE_EXECUTABLE NAMES node REQUIRED)

qt_add_executable(
    hyprshelld-compositor-canonical-json-test
    compositor_canonical_json_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-canonical-json-test
    PRIVATE
        HYPRSHELLD_NODE_EXECUTABLE="${HYPRSHELLD_NODE_EXECUTABLE}"
)

target_link_libraries(
    hyprshelld-compositor-canonical-json-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-canonical-json
    COMMAND hyprshelld-compositor-canonical-json-test
)

qt_add_executable(
    hyprshelld-compositor-authority-records-test
    compositor_authority_records_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-authority-records-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-authority-records
    COMMAND hyprshelld-compositor-authority-records-test
)

qt_add_executable(
    hyprshelld-compositor-authority-cas-reducer-test
    compositor_authority_cas_reducer_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-authority-cas-reducer-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-authority-cas-reducer
    COMMAND hyprshelld-compositor-authority-cas-reducer-test
)

qt_add_executable(
    hyprshelld-compositor-startup-reducer-test
    compositor_startup_reducer_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-startup-reducer-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-startup-reducer
    COMMAND hyprshelld-compositor-startup-reducer-test
)

qt_add_executable(
    hyprshelld-compositor-observed-authority-facts-test
    compositor_observed_authority_facts_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-observed-authority-facts-test
    PRIVATE
        QT_NO_DEBUG
        HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-observed-authority-facts-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-observed-authority-facts
    COMMAND hyprshelld-compositor-observed-authority-facts-test
)

qt_add_executable(
    hyprshelld-compositor-settled-v2-startup-facts-test
    compositor_settled_v2_startup_facts_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-settled-v2-startup-facts-test
    PRIVATE
        QT_NO_DEBUG
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-settled-v2-startup-facts-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-settled-v2-startup-facts
    COMMAND hyprshelld-compositor-settled-v2-startup-facts-test
)

qt_add_executable(
    hyprshelld-compositor-settled-v2-pending-observation-test
    compositor_settled_v2_pending_observation_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-settled-v2-pending-observation-test
    PRIVATE
        QT_NO_DEBUG
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-settled-v2-pending-observation-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-settled-v2-pending-observation
    COMMAND hyprshelld-compositor-settled-v2-pending-observation-test
)

qt_add_executable(
    hyprshelld-compositor-settled-v2-generation-byte-graph-test
    compositor_settled_v2_generation_byte_graph_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-settled-v2-generation-byte-graph-test
    PRIVATE
        QT_NO_DEBUG
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland-template.json"
)

target_link_libraries(
    hyprshelld-compositor-settled-v2-generation-byte-graph-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-settled-v2-generation-byte-graph
)

add_test(
    NAME compositor-settled-v2-generation-byte-graph
    COMMAND hyprshelld-compositor-settled-v2-generation-byte-graph-test
)

qt_add_executable(
    hyprshelld-compositor-ordinary-pending-record-test
    compositor_ordinary_pending_record_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-ordinary-pending-record-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-ordinary-pending-record-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-ordinary-pending-record
    COMMAND hyprshelld-compositor-ordinary-pending-record-test
)

qt_add_executable(
    hyprshelld-compositor-desired-v1-observation-test
    compositor_desired_v1_observation_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-desired-v1-observation-test
    PRIVATE
        QT_NO_DEBUG
        HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_ACTION_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-desired-v1-observation-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-desired-v1-observation
    COMMAND hyprshelld-compositor-desired-v1-observation-test
)

qt_add_executable(
    hyprshelld-compositor-desired-migration-reducer-test
    compositor_desired_migration_reducer_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-desired-migration-reducer-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_ACTION_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
        HYPRSHELLD_MIGRATION_MANIFEST_FILE="${PROJECT_SOURCE_DIR}/../restart-proof-repair-migration-manifest.md"
        HYPRSHELLD_SOURCE_MANIFEST_V2_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/source-manifest-v2.json"
)

target_link_libraries(
    hyprshelld-compositor-desired-migration-reducer-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-v2-core
)

add_test(
    NAME compositor-desired-migration-reducer
    COMMAND hyprshelld-compositor-desired-migration-reducer-test
)

qt_add_executable(
    hyprshelld-compositor-legacy-transaction-records-test
    compositor_legacy_transaction_records_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-legacy-transaction-records-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_ACTION_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-legacy-transaction-records-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-legacy-transaction-records
)

add_test(
    NAME compositor-legacy-transaction-records
    COMMAND hyprshelld-compositor-legacy-transaction-records-test
)

qt_add_executable(
    hyprshelld-compositor-legacy-entrypoint-records-test
    compositor_legacy_entrypoint_records_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-legacy-entrypoint-records-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-legacy-entrypoint-records
)

add_test(
    NAME compositor-legacy-entrypoint-records
    COMMAND hyprshelld-compositor-legacy-entrypoint-records-test
)

qt_add_executable(
    hyprshelld-compositor-dormant-generation-verifier-test
    compositor_dormant_generation_verifier_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-dormant-generation-verifier-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland-template.json"
)

target_link_libraries(
    hyprshelld-compositor-dormant-generation-verifier-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-generation-verifier
)

add_test(
    NAME compositor-dormant-generation-verifier
    COMMAND hyprshelld-compositor-dormant-generation-verifier-test
)

qt_add_executable(
    hyprshelld-compositor-dormant-generation-v1-verifier-test
    compositor_dormant_generation_v1_verifier_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-dormant-generation-v1-verifier-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_GENERATION_MANIFEST_FIXTURE_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/generation-manifest.json"
)

target_link_libraries(
    hyprshelld-compositor-dormant-generation-v1-verifier-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-dormant-generation-v1-verifier
)

add_test(
    NAME compositor-dormant-generation-v1-verifier
    COMMAND hyprshelld-compositor-dormant-generation-v1-verifier-test
)

qt_add_executable(
    hyprshelld-compositor-reachable-v1-preflight-test
    compositor_reachable_v1_preflight_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-reachable-v1-preflight-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_ACTION_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_MIGRATION_MANIFEST_FILE="${PROJECT_SOURCE_DIR}/../restart-proof-repair-migration-manifest.md"
        HYPRSHELLD_SOURCE_MANIFEST_V2_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/source-manifest-v2.json"
)

target_link_libraries(
    hyprshelld-compositor-reachable-v1-preflight-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-reachable-v1-preflight
)

add_test(
    NAME compositor-reachable-v1-preflight
    COMMAND hyprshelld-compositor-reachable-v1-preflight-test
)

# Compile the dormant capture implementation directly so its deterministic
# checkpoint seam exists only in this focused test binary. The normal dormant
# archive, production compositor, install graph, and exported ABI stay clean.
qt_add_executable(
    hyprshelld-compositor-dormant-v1-filesystem-capture-test
    compositor_dormant_v1_filesystem_capture_test.cpp
    "${PROJECT_SOURCE_DIR}/src/compositord/dormant_v1_filesystem_capture.cpp"
)

target_compile_definitions(
    hyprshelld-compositor-dormant-v1-filesystem-capture-test
    PRIVATE
        HYPRSHELLD_DORMANT_V1_CAPTURE_TEST_HOOKS=1
)

target_include_directories(
    hyprshelld-compositor-dormant-v1-filesystem-capture-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositor-dormant-v1-filesystem-capture-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-reachable-v1-preflight
)

add_test(
    NAME compositor-dormant-v1-filesystem-capture
    COMMAND hyprshelld-compositor-dormant-v1-filesystem-capture-test
)

# Compile the implementation directly so its deterministic syscall/checkpoint
# seam exists only in this focused binary. The normal dormant archive has no
# hook ABI and no production target links or installs it.
qt_add_executable(
    hyprshelld-compositor-dormant-fixed-record-capture-test
    compositor_dormant_fixed_record_capture_test.cpp
    "${PROJECT_SOURCE_DIR}/src/compositord/dormant_fixed_record_capture.cpp"
)

target_compile_definitions(
    hyprshelld-compositor-dormant-fixed-record-capture-test
    PRIVATE
        HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS=1
)

target_include_directories(
    hyprshelld-compositor-dormant-fixed-record-capture-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositor-dormant-fixed-record-capture-test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME compositor-dormant-fixed-record-capture
    COMMAND hyprshelld-compositor-dormant-fixed-record-capture-test
)

# Compile the pathless generation-tree capture directly so deterministic
# checkpoint/syscall/allocation seams exist only in this focused binary. The
# ordinary dormant archive and every production/install target stay hook-free.
qt_add_executable(
    hyprshelld-compositor-dormant-v2-generation-tree-capture-test
    compositor_dormant_v2_generation_tree_capture_test.cpp
    "${PROJECT_SOURCE_DIR}/src/compositord/dormant_v2_generation_tree_capture.cpp"
)

target_compile_definitions(
    hyprshelld-compositor-dormant-v2-generation-tree-capture-test
    PRIVATE
        HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS=1
)

target_include_directories(
    hyprshelld-compositor-dormant-v2-generation-tree-capture-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositor-dormant-v2-generation-tree-capture-test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME compositor-dormant-v2-generation-tree-capture
    COMMAND hyprshelld-compositor-dormant-v2-generation-tree-capture-test
)

qt_add_executable(
    hyprshelld-compositor-captured-observed-authority-facts-test
    compositor_captured_observed_authority_facts_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-captured-observed-authority-facts-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_V1_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-captured-observed-authority-facts-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-captured-observed-authority-facts
)

add_test(
    NAME compositor-captured-observed-authority-facts
    COMMAND hyprshelld-compositor-captured-observed-authority-facts-test
)

qt_add_executable(
    hyprshelld-compositor-captured-settled-v2-generation-byte-graph-test
    compositor_captured_settled_v2_generation_byte_graph_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-captured-settled-v2-generation-byte-graph-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-captured-settled-v2-generation-byte-graph-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-captured-settled-v2-generation-byte-graph
)

add_test(
    NAME compositor-captured-settled-v2-generation-byte-graph
    COMMAND hyprshelld-compositor-captured-settled-v2-generation-byte-graph-test
)

qt_add_executable(
    hyprshelld-compositor-activation-requirement-test
    compositor_activation_requirement_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-activation-requirement-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-activation-classifier
)

add_test(
    NAME compositor-activation-requirement
    COMMAND hyprshelld-compositor-activation-requirement-test
)

find_program(HYPRSHELLD_LUAC_EXECUTABLE NAMES luac REQUIRED)
find_program(HYPRSHELLD_LUA_EXECUTABLE NAMES lua REQUIRED)
find_program(HYPRSHELLD_HYPRLAND_EXECUTABLE NAMES Hyprland REQUIRED)

qt_add_executable(
    hyprshelld-compositor-renderer-test
    compositor_renderer_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-renderer-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v2.json"
        HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v2/config.schema.json"
        HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland-template.json"
        HYPRSHELLD_HYPRLAND_V2_SOURCE_MANIFEST_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/source-manifest-v2.json"
        HYPRSHELLD_HYPRLAND_V2_GENERATION_MANIFEST_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/generation-manifest-v2.json"
        HYPRSHELLD_LUAC_EXECUTABLE="${HYPRSHELLD_LUAC_EXECUTABLE}"
        HYPRSHELLD_LUA_EXECUTABLE="${HYPRSHELLD_LUA_EXECUTABLE}"
        HYPRSHELLD_HYPRLAND_EXECUTABLE="${HYPRSHELLD_HYPRLAND_EXECUTABLE}"
)

target_link_libraries(
    hyprshelld-compositor-renderer-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-core
)

add_test(
    NAME compositor-renderer
    COMMAND hyprshelld-compositor-renderer-test
)

qt_add_executable(
    hyprshelld-compositor-service-test
    compositor_service_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-service-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-compositord-service
)

add_test(
    NAME compositor-service
    COMMAND hyprshelld-compositor-service-test
)

find_program(
    HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE
    NAMES dbus-run-session
    REQUIRED
)

qt_add_executable(
    hyprshelld-compositor-shared-visual-dbus-test
    compositor_shared_visual_dbus_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-shared-visual-dbus-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-compositord-service
)

add_test(
    NAME compositor-shared-visual-dbus
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "$<TARGET_FILE:hyprshelld-compositor-shared-visual-dbus-test>"
)

set_tests_properties(
    compositor-shared-visual-dbus
    PROPERTIES
        TIMEOUT 30
)

qt_add_executable(
    hyprshelld-compositor-activation-backend-test
    compositor_activation_backend_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-activation-backend-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-core
        hyprshelld-compositord-service
)

add_test(
    NAME compositor-activation-backend
    COMMAND hyprshelld-compositor-activation-backend-test
)

qt_add_executable(
    hyprshelld-compositor-entrypoint-publisher-test
    compositor_entrypoint_publisher_test.cpp
)

target_link_libraries(
    hyprshelld-compositor-entrypoint-publisher-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-core
        hyprshelld-compositord-legacy-entrypoint-records
        hyprshelld-compositord-service
)

add_test(
    NAME compositor-entrypoint-publisher
    COMMAND hyprshelld-compositor-entrypoint-publisher-test
)

qt_add_executable(
    hyprshelld-hyprland-ipc-runtime-test
    hyprland_ipc_runtime_test.cpp
)

target_link_libraries(
    hyprshelld-hyprland-ipc-runtime-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-core
        hyprshelld-compositord-service
)

add_test(
    NAME hyprland-ipc-runtime
    COMMAND hyprshelld-hyprland-ipc-runtime-test
)

set_tests_properties(
    hyprland-ipc-runtime
    PROPERTIES
        TIMEOUT 30
)

qt_add_executable(
    hyprshelld-compositor-store-test
    compositor_store_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-store-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-store-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-core
)

add_test(
    NAME compositor-store
    COMMAND hyprshelld-compositor-store-test
)

# Both targets deliberately churn durable namespace state and inject failures
# on the same temporary filesystem.  Serialize them so parallel CTest runs do
# not turn host-level filesystem contention into unrelated fault-hook failures.
set_tests_properties(
    compositor-entrypoint-publisher
    compositor-store
    PROPERTIES
        RESOURCE_LOCK compositor_filesystem_fault_tests
)

qt_add_executable(
    hyprshelld-compositor-transaction-test
    compositor_transaction_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-transaction-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_PRE_SHARED_SPACING_DEFAULT_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/pre-shared-spacing-default-desired.json"
        HYPRSHELLD_HYPRLAND_PRE_BINDINGS_QUARANTINE_DEFAULT_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/hyprland/pre-bindings-quarantine-default-desired.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-transaction-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-compositord-core
        hyprshelld-compositord-legacy-transaction-records
)

add_test(
    NAME compositor-transaction
    COMMAND hyprshelld-compositor-transaction-test
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}"
        -c
        "from jsonschema import Draft202012Validator, FormatChecker"
    RESULT_VARIABLE HYPRSHELLD_JSONSCHEMA_IMPORT_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT HYPRSHELLD_JSONSCHEMA_IMPORT_RESULT EQUAL 0)
    message(
        FATAL_ERROR
        "The Python jsonschema package with Draft 2020-12 support is required when BUILD_TESTING is enabled"
    )
endif()

add_test(
    NAME hyprland-contract-schema
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/hyprland/validate_contract.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)

add_test(
    NAME hyprland-extract-contract
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/hyprland_extract_contract_test.py"
        --extractor
        "${PROJECT_SOURCE_DIR}/tools/hyprland/extract_contract.py"
)

add_test(
    NAME hyprland-extract-contract-v2
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/hyprland_extract_contract_v2_test.py"
        --extractor
        "${PROJECT_SOURCE_DIR}/tools/hyprland/extract_contract.py"
)

add_test(
    NAME hyprland-validate-contract-v2
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/hyprland_validate_contract_v2_test.py"
        --validator
        "${PROJECT_SOURCE_DIR}/tools/hyprland/validate_contract.py"
)

add_test(
    NAME hyprland-settings-coverage
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/hyprland_settings_coverage_test.py"
)

add_test(
    NAME legacy-shortcuts-source
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/legacy_shortcuts_source_test.py"
        --validator
        "${PROJECT_SOURCE_DIR}/tools/hyprland/validate_legacy_shortcuts.py"
        --source
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hyprland/legacy-keybindings.lua"
        --artifact
        "${PROJECT_SOURCE_DIR}/src/hyprland/resources/keyboard-shortcuts-reference-v1.json"
)

qt_add_executable(
    hyprshelld-component-store-test
    component_store_test.cpp
)

target_compile_definitions(
    hyprshelld-component-store-test
    PRIVATE
        HYPRSHELLD_COMPONENT_DEFAULTS_FILE="${HYPRSHELLD_COMPONENT_DEFAULTS_FILE}"
        HYPRSHELLD_WORKSPACE_SCHEMA_FILE="${HYPRSHELLD_WORKSPACE_SCHEMA_FILE}"
)

target_include_directories(
    hyprshelld-component-store-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/configd"
)

target_link_libraries(
    hyprshelld-component-store-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-configd-core
)

add_test(
    NAME component-store
    COMMAND hyprshelld-component-store-test
)

qt_add_executable(
    hyprshelld-componentd-catalog-test
    componentd_catalog_test.cpp
)

target_compile_definitions(
    hyprshelld-componentd-catalog-test
    PRIVATE
        HYPRSHELLD_SOURCE_COMPONENT_DIR="${PROJECT_SOURCE_DIR}/data/components/io.github.coastlinesec.hyprshelld.workspace-switcher"
)

target_link_libraries(
    hyprshelld-componentd-catalog-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-componentd-core
)

add_test(
    NAME componentd-catalog
    COMMAND hyprshelld-componentd-catalog-test
)

qt_add_executable(
    hyprshelld-user-package-store-test
    user_package_store_test.cpp
)

target_link_libraries(
    hyprshelld-user-package-store-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-componentd-core
)

add_test(
    NAME user-package-store
    COMMAND hyprshelld-user-package-store-test
)

qt_add_executable(
    hyprshelld-config-store-test
    config_store_test.cpp
)

target_link_libraries(
    hyprshelld-config-store-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-configd-core
)

add_test(
    NAME config-store
    COMMAND hyprshelld-config-store-test
)

add_test(
    NAME component-plan-hydrator
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        --
        "$<TARGET_FILE:hyprshelld-component-plan-hydrator-test>"
)

set_tests_properties(
    component-plan-hydrator
    PROPERTIES
        TIMEOUT 30
)

set(HYPRSHELLD_COMPONENT_CONFIG_TEST_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_COMPONENT_CONFIG_TEST_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentConfig1.xml"
    "${PROJECT_SOURCE_DIR}/src/configd/component_config_service.h"
    HyprShelld::ComponentConfigService
    component_config1_adaptor
    ComponentConfig1Adaptor
)

qt_add_executable(
    hyprshelld-component-config-dbus-test
    component_config_dbus_test.cpp
    "${PROJECT_SOURCE_DIR}/src/qml/HyprShelld/Client/component_config_client.cpp"
    "${PROJECT_SOURCE_DIR}/src/qml/HyprShelld/Client/component_config_client.h"
    ${HYPRSHELLD_COMPONENT_CONFIG_TEST_ADAPTOR_SOURCES}
)

set_target_properties(
    hyprshelld-component-config-dbus-test
    PROPERTIES
        QT_QML_MODULE_NO_IMPORT_SCAN TRUE
)

target_compile_definitions(
    hyprshelld-component-config-dbus-test
    PRIVATE
        HYPRSHELLD_COMPONENT_DEFAULTS_FILE="${HYPRSHELLD_COMPONENT_DEFAULTS_FILE}"
        HYPRSHELLD_WORKSPACE_SCHEMA_FILE="${HYPRSHELLD_WORKSPACE_SCHEMA_FILE}"
)

target_include_directories(
    hyprshelld-component-config-dbus-test
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/src/configd"
        "${PROJECT_SOURCE_DIR}/src/qml/HyprShelld/Client"
)

target_link_libraries(
    hyprshelld-component-config-dbus-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Qml
        Qt6::Test
        hyprshelld-component-core
        hyprshelld-configd-core
)

add_test(
    NAME component-config-dbus
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "$<TARGET_FILE:hyprshelld-component-config-dbus-test>"
)

set_tests_properties(
    component-config-dbus
    PROPERTIES
        TIMEOUT 30
)

qt_add_executable(
    hyprshelld-component-catalog-client-test
    component_catalog_client_test.cpp
)

target_compile_definitions(
    hyprshelld-component-catalog-client-test
    PRIVATE
        HYPRSHELLD_SOURCE_COMPONENT_DIR="${PROJECT_SOURCE_DIR}/data/components"
        HYPRSHELLD_WORKSPACE_SCHEMA_FILE="${HYPRSHELLD_WORKSPACE_SCHEMA_FILE}"
)

target_include_directories(
    hyprshelld-component-catalog-client-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/configd"
)

target_link_libraries(
    hyprshelld-component-catalog-client-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-configd-core
        hyprshelld-componentd-core
)

add_test(
    NAME component-catalog-client
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "$<TARGET_FILE:hyprshelld-component-catalog-client-test>"
)

set_tests_properties(
    component-catalog-client
    PROPERTIES
    TIMEOUT 15
)

add_executable(
    hyprshelld-component-manager-client-test
    component_manager_client_test.cpp
)

target_include_directories(
    hyprshelld-component-manager-client-test
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/qml/HyprShelld/Client"
)

target_link_libraries(
    hyprshelld-component-manager-client-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-client
)

add_test(
    NAME component-manager-client
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        --
        "$<TARGET_FILE:hyprshelld-component-manager-client-test>"
)

set_tests_properties(
    component-manager-client
    PROPERTIES
        TIMEOUT 30
)

set(HYPRSHELLD_COMPONENT_MANAGER_TEST_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_COMPONENT_MANAGER_TEST_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentManager1.xml"
    "${PROJECT_SOURCE_DIR}/src/componentd/component_manager_service.h"
    HyprShelld::ComponentManagerService
    component_manager1_adaptor
    ComponentManager1Adaptor
)

qt_add_executable(
    hyprshelld-componentd-activation-test-service
    "${PROJECT_SOURCE_DIR}/src/componentd/main.cpp"
    ${HYPRSHELLD_COMPONENT_MANAGER_TEST_ADAPTOR_SOURCES}
)

target_include_directories(
    hyprshelld-componentd-activation-test-service
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/src/componentd"
)

target_compile_definitions(
    hyprshelld-componentd-activation-test-service
    PRIVATE
        HYPRSHELLD_COMPONENT_INSPECTOR_EXECUTABLE="$<TARGET_FILE:hyprshelld-component-inspector>"
        HYPRSHELLD_SYSTEM_COMPONENT_DIR="${PROJECT_SOURCE_DIR}/data/components"
)

target_link_libraries(
    hyprshelld-componentd-activation-test-service
    PRIVATE
        Qt6::Core
        Qt6::DBus
        hyprshelld-componentd-core
)

qt_add_executable(
    hyprshelld-componentd-dbus-test
    componentd_dbus_test.cpp
    ${HYPRSHELLD_COMPONENT_MANAGER_TEST_ADAPTOR_SOURCES}
)

target_include_directories(
    hyprshelld-componentd-dbus-test
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions(
    hyprshelld-componentd-dbus-test
    PRIVATE
        HYPRSHELLD_SOURCE_COMPONENT_DIR="${PROJECT_SOURCE_DIR}/data/components/io.github.coastlinesec.hyprshelld.workspace-switcher"
)

target_link_libraries(
    hyprshelld-componentd-dbus-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-componentd-core
)

add_test(
    NAME componentd-dbus
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "$<TARGET_FILE:hyprshelld-componentd-dbus-test>"
)

set_tests_properties(
    componentd-dbus
    PROPERTIES
        TIMEOUT 30
)

qt_add_executable(
    hyprshelld-coordinator-dbus-test
    coordinator_dbus_test.cpp
    ${HYPRSHELLD_COORDINATOR_PROXY_SOURCES}
)

target_include_directories(
    hyprshelld-coordinator-dbus-test
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    hyprshelld-coordinator-dbus-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(
    NAME coordinator-dbus
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "${CMAKE_COMMAND}"
        -E
        env
        "HYPRSHELLD_COORDINATOR_EXECUTABLE=$<TARGET_FILE:hyprshelld>"
        "HYPRSHELLD_COORDINATOR1_XML=${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Coordinator1.xml"
        "$<TARGET_FILE:hyprshelld-coordinator-dbus-test>"
)

set_tests_properties(
    coordinator-dbus
    PROPERTIES
        TIMEOUT 30
)

qt_add_executable(
    hyprshelld-configd-dbus-test
    configd_dbus_test.cpp
    ${HYPRSHELLD_CONFIG_PROXY_SOURCES}
)

target_include_directories(
    hyprshelld-configd-dbus-test
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    hyprshelld-configd-dbus-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(
    NAME configd-dbus
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "${CMAKE_COMMAND}"
        -E
        env
        "HYPRSHELLD_CONFIGD_EXECUTABLE=$<TARGET_FILE:hyprshelld-configd>"
        "HYPRSHELLD_CONFIG1_XML=${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Config1.xml"
        "$<TARGET_FILE:hyprshelld-configd-dbus-test>"
)

set_tests_properties(
    configd-dbus
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    hyprshelld-config-client-test
    config_client_test.cpp
)

target_link_libraries(
    hyprshelld-config-client-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-client
)

add_test(
    NAME config-client
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        --
        "${CMAKE_COMMAND}"
        -E
        env
        "HYPRSHELLD_CONFIGD_EXECUTABLE=$<TARGET_FILE:hyprshelld-configd>"
        "$<TARGET_FILE:hyprshelld-config-client-test>"
)

set_tests_properties(
    config-client
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    hyprshelld-compositor-client-test
    compositor_client_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-client-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
)

add_executable(
    hyprshelld-compositor-settings-helpers-test
    compositor_settings_helpers_test.cpp
)

target_compile_definitions(
    hyprshelld-compositor-settings-helpers-test
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${PROJECT_SOURCE_DIR}/data/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_DEFAULTS_FILE="${PROJECT_SOURCE_DIR}/data/defaults/hyprland.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${PROJECT_SOURCE_DIR}/interfaces/hyprland/v1/config.schema.json"
)

target_link_libraries(
    hyprshelld-compositor-settings-helpers-test
    PRIVATE
        Qt6::Core
        Qt6::Test
        hyprshelld-client
        hyprshelld-hyprland-core
)

add_test(
    NAME compositor-settings-helpers
    COMMAND hyprshelld-compositor-settings-helpers-test
)

target_link_libraries(
    hyprshelld-compositor-client-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-client
        hyprshelld-hyprland-core
)

add_test(
    NAME compositor-client
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        --
        "$<TARGET_FILE:hyprshelld-compositor-client-test>"
)

set_tests_properties(
    compositor-client
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    hyprshelld-coordinator-client-test
    coordinator_client_test.cpp
)

target_link_libraries(
    hyprshelld-coordinator-client-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-client
)

add_test(
    NAME coordinator-client
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "$<TARGET_FILE:hyprshelld-coordinator-client-test>"
)

set_tests_properties(
    coordinator-client
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    hyprshelld-component-runtime-client-test
    component_runtime_client_test.cpp
)

target_link_libraries(
    hyprshelld-component-runtime-client-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-client
)

add_test(
    NAME component-runtime-client
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        --
        "$<TARGET_FILE:hyprshelld-component-runtime-client-test>"
)

set_tests_properties(
    component-runtime-client
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    hyprshelld-shell-runtime-status-test
    shell_runtime_status_test.cpp
)

target_link_libraries(
    hyprshelld-shell-runtime-status-test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        hyprshelld-client
)

add_test(
    NAME shell-runtime-status
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        --
        "$<TARGET_FILE:hyprshelld-shell-runtime-status-test>"
)

set_tests_properties(
    shell-runtime-status
    PROPERTIES
        TIMEOUT 30
)

add_test(
    NAME qml-modules
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        --
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "$<TARGET_FILE:Qt6::qmltestrunner>"
        -import
        "${QT_QML_OUTPUT_DIRECTORY}"
        -input
        "${CMAKE_CURRENT_SOURCE_DIR}/qml"
)

set_tests_properties(
    qml-modules
    PROPERTIES
        TIMEOUT 30
)

qt_add_executable(
    hyprshelld-settings-smoke-test
    settings_smoke_test.cpp
)

target_link_libraries(
    hyprshelld-settings-smoke-test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME settings-smoke
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        --
        "${CMAKE_COMMAND}"
        -E
        env
        "HYPRSHELLD_SETTINGS_EXECUTABLE=$<TARGET_FILE:hyprshelld-settings>"
        "$<TARGET_FILE:hyprshelld-settings-smoke-test>"
)

set_tests_properties(
    settings-smoke
    PROPERTIES
        TIMEOUT 30
)

set(
    HYPRSHELLD_GENERATED_SYSTEMD_DIRECTORY
    "${PROJECT_BINARY_DIR}/data/systemd"
)
set(
    HYPRSHELLD_GENERATED_CONFIG1_ACTIVATION_FILE
    "${PROJECT_BINARY_DIR}/data/dbus/org.hyprshelld.Config1.service"
)
set(
    HYPRSHELLD_GENERATED_COMPONENT_MANAGER1_ACTIVATION_FILE
    "${PROJECT_BINARY_DIR}/data/dbus/org.hyprshelld.ComponentManager1.service"
)
set(
    HYPRSHELLD_GENERATED_COMPOSITOR1_ACTIVATION_FILE
    "${PROJECT_BINARY_DIR}/data/dbus/org.hyprshelld.Compositor1.service"
)

if(IS_ABSOLUTE "${QT6_INSTALL_QML}")
    set(HYPRSHELLD_INSTALL_QML_DIRECTORY "${QT6_INSTALL_QML}")
else()
    set(
        HYPRSHELLD_INSTALL_QML_DIRECTORY
        "${CMAKE_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
    )
endif()

if(IS_ABSOLUTE "${HYPRSHELLD_SYSTEMD_USER_UNIT_DIR}")
    set(
        HYPRSHELLD_INSTALL_SYSTEMD_UNIT_DIRECTORY
        "${HYPRSHELLD_SYSTEMD_USER_UNIT_DIR}"
    )
else()
    set(
        HYPRSHELLD_INSTALL_SYSTEMD_UNIT_DIRECTORY
        "${CMAKE_INSTALL_PREFIX}/${HYPRSHELLD_SYSTEMD_USER_UNIT_DIR}"
    )
endif()

qt_add_executable(
    hyprshelld-systemd-unit-contract-test
    systemd_unit_contract_test.cpp
)

target_compile_definitions(
    hyprshelld-systemd-unit-contract-test
    PRIVATE
        HYPRSHELLD_SYSTEMD_UNIT_DIR="${HYPRSHELLD_GENERATED_SYSTEMD_DIRECTORY}"
        HYPRSHELLD_CONFIG1_ACTIVATION_FILE="${HYPRSHELLD_GENERATED_CONFIG1_ACTIVATION_FILE}"
        HYPRSHELLD_COMPONENT_MANAGER1_ACTIVATION_FILE="${HYPRSHELLD_GENERATED_COMPONENT_MANAGER1_ACTIVATION_FILE}"
        HYPRSHELLD_COMPOSITOR1_ACTIVATION_FILE="${HYPRSHELLD_GENERATED_COMPOSITOR1_ACTIVATION_FILE}"
        HYPRSHELLD_INSTALL_COORDINATOR="${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld"
        HYPRSHELLD_INSTALL_CONFIGD="${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld-configd"
        HYPRSHELLD_INSTALL_COMPONENTD="${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld-componentd"
        HYPRSHELLD_INSTALL_COMPOSITORD="${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld-compositord"
        HYPRSHELLD_INSTALL_SURFACED="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/surfaced"
        HYPRSHELLD_INSTALL_QML="${HYPRSHELLD_INSTALL_QML_DIRECTORY}"
        HYPRSHELLD_QUICKSHELL_EXECUTABLE="${QUICKSHELL_EXECUTABLE}"
)

target_link_libraries(
    hyprshelld-systemd-unit-contract-test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME systemd-unit-contract
    COMMAND hyprshelld-systemd-unit-contract-test
)

find_program(
    HYPRSHELLD_SYSTEMD_ANALYZE_EXECUTABLE
    NAMES systemd-analyze
    REQUIRED
)

add_test(
    NAME systemd-unit-syntax
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSYSTEMD_ANALYZE=${HYPRSHELLD_SYSTEMD_ANALYZE_EXECUTABLE}"
        "-DSOURCE_UNIT_DIRECTORY=${HYPRSHELLD_GENERATED_SYSTEMD_DIRECTORY}"
        "-DVERIFY_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/systemd-verify"
        "-DCOORDINATOR_EXECUTABLE=$<TARGET_FILE:hyprshelld>"
        "-DCONFIGD_EXECUTABLE=$<TARGET_FILE:hyprshelld-configd>"
        "-DCOMPONENTD_EXECUTABLE=$<TARGET_FILE:hyprshelld-componentd>"
        "-DCOMPOSITORD_EXECUTABLE=$<TARGET_FILE:hyprshelld-compositord>"
        "-DQUICKSHELL_EXECUTABLE=${QUICKSHELL_EXECUTABLE}"
        "-DSURFACED_DIRECTORY=${PROJECT_SOURCE_DIR}/src/surfaced"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/verify_systemd_units.cmake"
)

set_tests_properties(
    systemd-unit-syntax
    PROPERTIES
        TIMEOUT 30
)

find_program(
    HYPRSHELLD_BUSCTL_EXECUTABLE
    NAMES busctl
    REQUIRED
)

find_program(
    HYPRSHELLD_DESKTOP_FILE_VALIDATE_EXECUTABLE
    NAMES desktop-file-validate
    REQUIRED
)

add_test(
    NAME configd-activation
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCMAKE_EXECUTABLE=${CMAKE_COMMAND}"
        "-DDBUS_RUN_SESSION=${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "-DBUSCTL=${HYPRSHELLD_BUSCTL_EXECUTABLE}"
        "-DSOURCE_ACTIVATION_FILE=${HYPRSHELLD_GENERATED_CONFIG1_ACTIVATION_FILE}"
        "-DFIXTURE_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/configd-activation"
        "-DINSTALL_CONFIGD_EXECUTABLE=${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld-configd"
        "-DBUILD_CONFIGD_EXECUTABLE=$<TARGET_FILE:hyprshelld-configd>"
        "-DACTIVATION_DRIVER=${CMAKE_CURRENT_SOURCE_DIR}/configd_activation_driver.sh"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/verify_configd_activation.cmake"
)

set_tests_properties(
    configd-activation
    PROPERTIES
        TIMEOUT 30
)

add_test(
    NAME componentd-activation
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCMAKE_EXECUTABLE=${CMAKE_COMMAND}"
        "-DDBUS_RUN_SESSION=${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "-DBUSCTL=${HYPRSHELLD_BUSCTL_EXECUTABLE}"
        "-DSOURCE_ACTIVATION_FILE=${HYPRSHELLD_GENERATED_COMPONENT_MANAGER1_ACTIVATION_FILE}"
        "-DFIXTURE_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/componentd-activation"
        "-DINSTALL_COMPONENTD_EXECUTABLE=${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld-componentd"
        "-DTEST_COMPONENTD_EXECUTABLE=$<TARGET_FILE:hyprshelld-componentd-activation-test-service>"
        "-DACTIVATION_DRIVER=${CMAKE_CURRENT_SOURCE_DIR}/componentd_activation_driver.sh"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/verify_componentd_activation.cmake"
)

set_tests_properties(
    componentd-activation
    PROPERTIES
        TIMEOUT 30
)

add_test(
    NAME staged-install
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCMAKE_EXECUTABLE=${CMAKE_COMMAND}"
        "-DBUILD_DIRECTORY=${PROJECT_BINARY_DIR}"
        "-DBUILD_CONFIG=$<CONFIG>"
        "-DSTAGE_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/install-stage"
        "-DINSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
        "-DINSTALL_BINDIR=${CMAKE_INSTALL_FULL_BINDIR}"
        "-DINSTALL_LIBDIR=${CMAKE_INSTALL_FULL_LIBDIR}"
        "-DINSTALL_LIBEXECDIR=${CMAKE_INSTALL_FULL_LIBEXECDIR}"
        "-DINSTALL_DATADIR=${CMAKE_INSTALL_FULL_DATADIR}"
        "-DINSTALL_QMLDIR=${HYPRSHELLD_INSTALL_QML_DIRECTORY}"
        "-DINSTALL_SYSTEMD_UNIT_DIR=${HYPRSHELLD_INSTALL_SYSTEMD_UNIT_DIRECTORY}"
        "-DQML_EXECUTABLE=$<TARGET_FILE:Qt6::qmltestrunner>"
        "-DDBUS_RUN_SESSION=${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "-DDBUS_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/no_activation_session.conf"
        "-DMODULE_PROBE=${CMAKE_CURRENT_SOURCE_DIR}/staged_module_probe.qml"
        "-DDESKTOP_FILE_VALIDATE=${HYPRSHELLD_DESKTOP_FILE_VALIDATE_EXECUTABLE}"
        "-DSETTINGS_SMOKE_EXECUTABLE=$<TARGET_FILE:hyprshelld-settings-smoke-test>"
        "-DSETTINGS_DESKTOP_ID=${HYPRSHELLD_SETTINGS_DESKTOP_ID}"
        "-DPROJECT_SOURCE_DIRECTORY=${PROJECT_SOURCE_DIR}"
        "-DPROJECT_BINARY_DIRECTORY=${PROJECT_BINARY_DIR}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/verify_staged_install.cmake"
)

set_tests_properties(
    staged-install
    PROPERTIES
        TIMEOUT 45
        RUN_SERIAL TRUE
)
