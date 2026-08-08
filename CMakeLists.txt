cmake_minimum_required(VERSION 3.24)

project(
    HyprShelld
    DESCRIPTION "A cohesive desktop environment for Hyprland"
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(
    HYPRSHELLD_SETTINGS_DESKTOP_ID
    "io.github.CoastLineSec.HyprShelld.Settings"
)

find_package(
    Qt6 6.8 REQUIRED
    COMPONENTS
        Core
        DBus
        Gui
        Qml
        Quick
        QuickControls2
        Test
)

qt_standard_project_setup(REQUIRES 6.8)

set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/qml")

find_program(
    QUICKSHELL_EXECUTABLE
    NAMES quickshell qs
    REQUIRED
)

include(CTest)

add_subdirectory(src)
add_subdirectory(data)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
