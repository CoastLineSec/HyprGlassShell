qt_add_library(
    hyprshelld-coordinator-core
    STATIC
        coordinator_policy.cpp
        coordinator_policy.h
        coordinator_service.cpp
        coordinator_service.h
        components/component_plan_builder.cpp
        components/component_plan_builder.h
        components/component_plan_controller.cpp
        components/component_plan_controller.h
        components/component_plan_hydrator.cpp
        components/component_plan_hydrator.h
        components/component_plan_source.cpp
        components/component_plan_source.h
        components/component_runtime_service.cpp
        components/component_runtime_service.h
        systemd_user_manager.cpp
        systemd_user_manager.h
)

target_include_directories(
    hyprshelld-coordinator-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    hyprshelld-coordinator-core
    PUBLIC
        Qt6::Core
        Qt6::DBus
        hyprshelld-component-core
)

set(HYPRSHELLD_COORDINATOR_ADAPTOR_SOURCES)
set(HYPRSHELLD_COMPONENT_RUNTIME_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_COORDINATOR_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Coordinator1.xml"
    coordinator_service.h
    HyprShelld::CoordinatorService
    coordinator1_adaptor
    Coordinator1Adaptor
)

qt_add_dbus_adaptor(
    HYPRSHELLD_COMPONENT_RUNTIME_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.ComponentRuntime1.xml"
    components/component_runtime_service.h
    HyprShelld::ComponentRuntimeService
    component_runtime1_adaptor
    ComponentRuntime1Adaptor
)

qt_add_executable(
    hyprshelld
    main.cpp
    ${HYPRSHELLD_COORDINATOR_ADAPTOR_SOURCES}
    ${HYPRSHELLD_COMPONENT_RUNTIME_ADAPTOR_SOURCES}
)

target_include_directories(
    hyprshelld
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    hyprshelld
    PRIVATE
        Qt6::Core
        Qt6::DBus
        hyprshelld-coordinator-core
)

install(
    TARGETS hyprshelld
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
