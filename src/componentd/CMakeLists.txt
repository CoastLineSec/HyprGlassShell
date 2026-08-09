qt_add_library(
    hyprshelld-componentd-core
    STATIC
        component_manager_service.cpp
        component_manager_service.h
        system_catalog.cpp
        system_catalog.h
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
