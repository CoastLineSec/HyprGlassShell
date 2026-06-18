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

string(TIMESTAMP HGS_HYPRGLASS_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
execute_process(
    COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-parse --short=12 HEAD
    OUTPUT_VARIABLE HGS_HYPRGLASS_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT HGS_HYPRGLASS_GIT_COMMIT)
    set(HGS_HYPRGLASS_GIT_COMMIT "unknown")
endif()
set(HGS_HYPRGLASS_BUILD_ID "${HGS_HYPRGLASS_BUILD_TIME}-${HGS_HYPRGLASS_GIT_COMMIT}")

add_library(hgs-hyprglass SHARED
    src/main.cpp
)

target_include_directories(hgs-hyprglass PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
target_link_libraries(hgs-hyprglass PRIVATE PkgConfig::HYPRLAND)
target_compile_options(hgs-hyprglass PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(hgs-hyprglass PRIVATE
    HGS_HYPRGLASS_PLUGIN_VERSION="${PROJECT_VERSION}"
    HGS_HYPRGLASS_BUILD_ID="${HGS_HYPRGLASS_BUILD_ID}"
    HGS_HYPRGLASS_BUILD_TIME="${HGS_HYPRGLASS_BUILD_TIME}"
    HGS_HYPRGLASS_GIT_COMMIT="${HGS_HYPRGLASS_GIT_COMMIT}"
    HGS_HYPRGLASS_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)
set_target_properties(hgs-hyprglass PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install(TARGETS hgs-hyprglass LIBRARY DESTINATION lib/hyprland)
