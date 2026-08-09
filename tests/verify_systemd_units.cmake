foreach(required IN ITEMS
    SYSTEMD_ANALYZE
    SOURCE_UNIT_DIRECTORY
    VERIFY_DIRECTORY
    COORDINATOR_EXECUTABLE
    CONFIGD_EXECUTABLE
    COMPONENTD_EXECUTABLE
    COMPOSITORD_EXECUTABLE
    QUICKSHELL_EXECUTABLE
    SURFACED_DIRECTORY
)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing required argument: ${required}")
    endif()
endforeach()

set(units
    hyprshelld.target
    session-hyprshelld.slice
    session-hyprshelld-components.slice
    hyprshelld.service
    hyprshelld-configd.service
    hyprshelld-componentd.service
    hyprshelld-compositord.service
    hyprshelld-surfaced.service
)

file(REMOVE_RECURSE "${VERIFY_DIRECTORY}")
file(MAKE_DIRECTORY "${VERIFY_DIRECTORY}")

foreach(unit IN LISTS units)
    set(source "${SOURCE_UNIT_DIRECTORY}/${unit}")
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "Generated unit is missing: ${source}")
    endif()
    file(COPY "${source}" DESTINATION "${VERIFY_DIRECTORY}")
endforeach()

function(quoted_systemd_argument output value)
    string(REPLACE "\\" "\\\\" escaped "${value}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    set(${output} "\"${escaped}\"" PARENT_SCOPE)
endfunction()

quoted_systemd_argument(coordinator "${COORDINATOR_EXECUTABLE}")
quoted_systemd_argument(configd "${CONFIGD_EXECUTABLE}")
quoted_systemd_argument(componentd "${COMPONENTD_EXECUTABLE}")
quoted_systemd_argument(compositord "${COMPOSITORD_EXECUTABLE}")
quoted_systemd_argument(quickshell "${QUICKSHELL_EXECUTABLE}")
quoted_systemd_argument(surfaced "${SURFACED_DIRECTORY}")

foreach(service IN ITEMS
    hyprshelld
    hyprshelld-configd
    hyprshelld-componentd
    hyprshelld-compositord
    hyprshelld-surfaced
)
    file(MAKE_DIRECTORY "${VERIFY_DIRECTORY}/${service}.service.d")
endforeach()

file(WRITE "${VERIFY_DIRECTORY}/hyprshelld.service.d/verify.conf"
    "[Service]\nExecStart=\nExecStart=${coordinator}\n"
)
file(WRITE "${VERIFY_DIRECTORY}/hyprshelld-configd.service.d/verify.conf"
    "[Service]\nExecStart=\nExecStart=${configd}\n"
)
file(WRITE "${VERIFY_DIRECTORY}/hyprshelld-componentd.service.d/verify.conf"
    "[Service]\nExecStart=\nExecStart=${componentd}\n"
)
file(WRITE "${VERIFY_DIRECTORY}/hyprshelld-compositord.service.d/verify.conf"
    "[Service]\nExecStart=\nExecStart=${compositord}\n"
)
file(WRITE "${VERIFY_DIRECTORY}/hyprshelld-surfaced.service.d/verify.conf"
    "[Service]\nExecStart=\nExecStart=${quickshell} --path ${surfaced}\n"
)

set(unit_paths)
foreach(unit IN LISTS units)
    list(APPEND unit_paths "${VERIFY_DIRECTORY}/${unit}")
endforeach()

execute_process(
    COMMAND
        "${SYSTEMD_ANALYZE}"
        --user
        --recursive-errors=yes
        --man=no
        --generators=no
        verify
        ${unit_paths}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 20
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "systemd-analyze rejected the generated user units\n"
        "stdout:\n${output}\n"
        "stderr:\n${error}"
    )
endif()

string(STRIP "${output}" output)
string(STRIP "${error}" error)
if(NOT output STREQUAL "" OR NOT error STREQUAL "")
    message(FATAL_ERROR
        "systemd-analyze produced unexpected diagnostics\n"
        "stdout:\n${output}\n"
        "stderr:\n${error}"
    )
endif()
