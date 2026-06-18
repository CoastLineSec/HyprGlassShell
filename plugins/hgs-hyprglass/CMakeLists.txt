cmake_minimum_required(VERSION 3.16)
project(hgs-hyprglass VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json.hpp not found. Install nlohmann-json to build hgs-hyprglass.")
endif()

add_library(hgs-hyprglass SHARED
    src/main.cpp
)

target_include_directories(hgs-hyprglass PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
target_link_libraries(hgs-hyprglass PRIVATE PkgConfig::HYPRLAND)
target_compile_options(hgs-hyprglass PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(hgs-hyprglass PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install(TARGETS hgs-hyprglass LIBRARY DESTINATION lib/hyprland)
