qt_add_library(
    hyprshelld-componentd-core
    STATIC
        component_inspection_sessions.cpp
        component_inspection_sessions.h
        component_inspector_launcher.cpp
        component_inspector_launcher.h
        component_manager_service.cpp
        component_manager_service.h
        system_catalog.cpp
        system_catalog.h
        user_package_store.cpp
        user_package_store.h
)

target_include_directories(
    hyprshelld-componentd-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-componentd-core
    PUBLIC
        Qt6::Core
        Qt6::DBus
        hyprshelld-component-core
)

if(BUILD_TESTING)
    qt_add_executable(
        hyprshelld-component-inspection-sessions-test
        component_inspection_sessions_test.cpp
    )

    target_link_libraries(
        hyprshelld-component-inspection-sessions-test
        PRIVATE
            Qt6::Core
            Qt6::DBus
            Qt6::Test
            hyprshelld-componentd-core
    )

    target_compile_definitions(
        hyprshelld-component-inspection-sessions-test
        PRIVATE
            HYPRSHELLD_TEST_COMPONENT_INSPECTOR_EXECUTABLE="$<TARGET_FILE:hyprshelld-component-inspector>"
    )
    add_dependencies(
        hyprshelld-component-inspection-sessions-test
        hyprshelld-component-inspector
    )

    add_test(
        NAME component-inspection-sessions
        COMMAND hyprshelld-component-inspection-sessions-test
    )
    set_tests_properties(
        component-inspection-sessions
        PROPERTIES TIMEOUT 30
    )
endif()

set(HYPRSHELLD_COMPONENT_MANAGER_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_COMPONENT_MANAGER_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentManager1.xml"
    component_manager_service.h
    HyprShelld::ComponentManagerService
    component_manager1_adaptor
    ComponentManager1Adaptor
)

qt_add_executable(
    hyprshelld-componentd
    main.cpp
    ${HYPRSHELLD_COMPONENT_MANAGER_ADAPTOR_SOURCES}
)

target_include_directories(
    hyprshelld-componentd
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions(
    hyprshelld-componentd
    PRIVATE
        HYPRSHELLD_COMPONENT_INSPECTOR_EXECUTABLE="${CMAKE_INSTALL_FULL_LIBEXECDIR}/hyprshelld/hyprshelld-component-inspector"
        HYPRSHELLD_SYSTEM_COMPONENT_DIR="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/components"
)

target_link_libraries(
    hyprshelld-componentd
    PRIVATE
        Qt6::Core
        Qt6::DBus
        hyprshelld-componentd-core
)

install(
    TARGETS hyprshelld-componentd
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
