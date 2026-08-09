qt_add_library(
    hyprshelld-component-inspector-core
    STATIC
        package_inspector.cpp
        package_inspector.h
)

target_include_directories(
    hyprshelld-component-inspector-core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-component-inspector-core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        hyprshelld-component-core
    PRIVATE
        libzip::zip
)

qt_add_executable(
    hyprshelld-component-inspector
    main.cpp
)

target_link_libraries(
    hyprshelld-component-inspector
    PRIVATE
        Qt6::Core
        PkgConfig::Systemd
        hyprshelld-component-inspector-core
)

install(
    TARGETS hyprshelld-component-inspector
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/hyprshelld"
)

if(BUILD_TESTING)
    qt_add_executable(
        hyprshelld-component-inspector-test
        "${PROJECT_SOURCE_DIR}/tests/component_package_inspector_test.cpp"
    )

    target_link_libraries(
        hyprshelld-component-inspector-test
        PRIVATE
            Qt6::Core
            Qt6::Test
            libzip::zip
            hyprshelld-component-inspector-core
    )

    add_test(
        NAME component-package-inspector
        COMMAND hyprshelld-component-inspector-test
    )
endif()
