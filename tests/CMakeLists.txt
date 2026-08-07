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

find_program(
    HYPRSHELLD_QMLLINT_EXECUTABLE
    NAMES qmllint
    REQUIRED
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
        "${HYPRSHELLD_QMLLINT_EXECUTABLE}"
        ${HYPRSHELLD_QUALIFICATION_QML}
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
