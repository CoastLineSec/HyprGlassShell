foreach(required IN ITEMS
    CMAKE_EXECUTABLE
    DBUS_RUN_SESSION
    BUSCTL
    SOURCE_ACTIVATION_FILE
    FIXTURE_DIRECTORY
    INSTALL_COMPONENTD_EXECUTABLE
    TEST_COMPONENTD_EXECUTABLE
    ACTIVATION_DRIVER
)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing required argument: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${FIXTURE_DIRECTORY}")
set(data_home "${FIXTURE_DIRECTORY}/data")
set(config_home "${FIXTURE_DIRECTORY}/config")
set(state_home "${FIXTURE_DIRECTORY}/state")
set(system_data "${FIXTURE_DIRECTORY}/system-data")
set(service_directory "${data_home}/dbus-1/services")
file(MAKE_DIRECTORY
    "${service_directory}"
    "${config_home}"
    "${state_home}"
    "${system_data}"
)

file(READ "${SOURCE_ACTIVATION_FILE}" activation)
set(expected "Exec=${INSTALL_COMPONENTD_EXECUTABLE}")
set(replacement "Exec=${TEST_COMPONENTD_EXECUTABLE}")
string(FIND "${activation}" "${expected}" position)
if(position EQUAL -1)
    message(FATAL_ERROR
        "ComponentManager1 activation file does not contain the packaged Exec contract: ${expected}"
    )
endif()
string(REPLACE "${expected}" "${replacement}" activation "${activation}")
file(WRITE
    "${service_directory}/org.hyprshelld.ComponentManager1.service"
    "${activation}"
)

set(list_response_file "${FIXTURE_DIRECTORY}/list-response.txt")
set(property_response_file "${FIXTURE_DIRECTORY}/property-response.txt")
set(stopped_file "${FIXTURE_DIRECTORY}/stopped.txt")

execute_process(
    COMMAND
        "${CMAKE_EXECUTABLE}"
        -E
        env
        "XDG_DATA_HOME=${data_home}"
        "XDG_DATA_DIRS=${system_data}"
        "XDG_CONFIG_HOME=${config_home}"
        "XDG_STATE_HOME=${state_home}"
        "${DBUS_RUN_SESSION}"
        --
        /bin/sh
        "${ACTIVATION_DRIVER}"
        "${BUSCTL}"
        "${list_response_file}"
        "${property_response_file}"
        "${stopped_file}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 15
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Private-bus ComponentManager1 activation failed\n"
        "stdout:\n${output}\n"
        "stderr:\n${error}"
    )
endif()

foreach(response_file IN ITEMS
    "${list_response_file}"
    "${property_response_file}"
)
    if(NOT EXISTS "${response_file}")
        message(FATAL_ERROR
            "Private-bus activation did not produce: ${response_file}"
        )
    endif()
endforeach()

file(READ "${list_response_file}" list_response)
string(STRIP "${list_response}" list_response)
if(NOT list_response MATCHES
    "^ass 1 \"io\\.github\\.coastlinesec\\.hyprshelld\\.workspace-switcher\" \"([0-9a-f]+)\"$"
)
    message(FATAL_ERROR
        "Unexpected activated ListComponents response: ${list_response}"
    )
endif()
set(list_digest "${CMAKE_MATCH_1}")
string(LENGTH "${list_digest}" list_digest_length)
if(NOT list_digest_length EQUAL 64)
    message(FATAL_ERROR
        "Activated catalog digest is not an exact SHA-256: ${list_digest}"
    )
endif()

file(READ "${property_response_file}" property_response)
string(STRIP "${property_response}" property_response)
if(NOT property_response MATCHES "^s \"([0-9a-f]+)\"$")
    message(FATAL_ERROR
        "Unexpected activated CatalogDigest response: ${property_response}"
    )
endif()
if(NOT CMAKE_MATCH_1 STREQUAL list_digest)
    message(FATAL_ERROR
        "Activated catalog digest changed between the typed list and property replies"
    )
endif()

if(NOT EXISTS "${stopped_file}")
    message(FATAL_ERROR
        "Activated componentd did not terminate with the private bus"
    )
endif()
