qt_add_library(
    hyprshelld-configd-core
    STATIC
        config_service.cpp
        config_service.h
        config_store.cpp
        config_store.h
)

target_include_directories(
    hyprshelld-configd-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-configd-core
    PUBLIC
        Qt6::Core
        Qt6::DBus
)

set(HYPRSHELLD_CONFIGD_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_CONFIGD_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Config1.xml"
    config_service.h
    HyprShelld::ConfigService
    config1_adaptor
    Config1Adaptor
)

qt_add_executable(
    hyprshelld-configd
    main.cpp
    ${HYPRSHELLD_CONFIGD_ADAPTOR_SOURCES}
)

target_include_directories(
    hyprshelld-configd
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    hyprshelld-configd
    PRIVATE
        Qt6::Core
        Qt6::DBus
        hyprshelld-configd-core
)
