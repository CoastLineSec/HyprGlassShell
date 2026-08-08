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
        -I
        "${QT_QML_OUTPUT_DIRECTORY}"
        "${PROJECT_SOURCE_DIR}/src/qml/HyprShelld/UI/Bar.qml"
        "${PROJECT_SOURCE_DIR}/src/qml/HyprShelld/UI/BarHeightControl.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/Main.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/BarPreview.qml"
        "${PROJECT_SOURCE_DIR}/src/settings/BarSettingsPage.qml"
)

add_test(
    NAME surfaced-qml-lint
    COMMAND
        "$<TARGET_FILE:Qt6::qmllint>"
        -W
        0
        -I
        "${QT_QML_OUTPUT_DIRECTORY}"
        "${PROJECT_SOURCE_DIR}/src/surfaced/shell.qml"
        "${PROJECT_SOURCE_DIR}/src/surfaced/BarSurface.qml"
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

qt_add_executable(
    hyprshelld-dbus-contract-test
    dbus_contract_test.cpp
    ${HYPRSHELLD_COORDINATOR_PROXY_SOURCES}
    ${HYPRSHELLD_CONFIG_PROXY_SOURCES}
)

target_compile_definitions(
    hyprshelld-dbus-contract-test
    PRIVATE
        HYPRSHELLD_COORDINATOR_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Coordinator1.xml"
        HYPRSHELLD_CONFIG_XML="${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Config1.xml"
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

find_program(
    HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE
    NAMES dbus-run-session
    REQUIRED
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

add_test(
    NAME qml-modules
    COMMAND
        "${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
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
        HYPRSHELLD_INSTALL_COORDINATOR="${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld"
        HYPRSHELLD_INSTALL_CONFIGD="${CMAKE_INSTALL_FULL_BINDIR}/hyprshelld-configd"
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
        "-DINSTALL_DATADIR=${CMAKE_INSTALL_FULL_DATADIR}"
        "-DINSTALL_QMLDIR=${HYPRSHELLD_INSTALL_QML_DIRECTORY}"
        "-DINSTALL_SYSTEMD_UNIT_DIR=${HYPRSHELLD_INSTALL_SYSTEMD_UNIT_DIRECTORY}"
        "-DQML_EXECUTABLE=$<TARGET_FILE:Qt6::qmltestrunner>"
        "-DDBUS_RUN_SESSION=${HYPRSHELLD_DBUS_RUN_SESSION_EXECUTABLE}"
        "-DMODULE_PROBE=${CMAKE_CURRENT_SOURCE_DIR}/staged_module_probe.qml"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/verify_staged_install.cmake"
)

set_tests_properties(
    staged-install
    PROPERTIES
        TIMEOUT 45
        RUN_SERIAL TRUE
)
