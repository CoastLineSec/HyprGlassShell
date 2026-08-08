foreach(required IN ITEMS
    CMAKE_EXECUTABLE
    DBUS_RUN_SESSION
    BUSCTL
    SOURCE_ACTIVATION_FILE
    FIXTURE_DIRECTORY
    INSTALL_CONFIGD_EXECUTABLE
    BUILD_CONFIGD_EXECUTABLE
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
set(expected "Exec=${INSTALL_CONFIGD_EXECUTABLE}")
set(replacement "Exec=${BUILD_CONFIGD_EXECUTABLE}")
string(FIND "${activation}" "${expected}" position)
if(position EQUAL -1)
    message(FATAL_ERROR
        "Config1 activation file does not contain the packaged Exec contract: ${expected}"
    )
endif()
string(REPLACE "${expected}" "${replacement}" activation "${activation}")
file(WRITE
    "${service_directory}/org.hyprshelld.Config1.service"
    "${activation}"
)

set(response_file "${FIXTURE_DIRECTORY}/response.txt")
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
        "${response_file}"
        "${stopped_file}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 15
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Private-bus Config1 activation failed\n"
        "stdout:\n${output}\n"
        "stderr:\n${error}"
    )
endif()
if(NOT EXISTS "${response_file}")
    message(FATAL_ERROR "Private-bus activation did not produce a response")
endif()
file(READ "${response_file}" response)
string(STRIP "${response}" response)
if(NOT response STREQUAL "u 40")
    message(FATAL_ERROR "Unexpected activated BarHeight response: ${response}")
endif()
if(NOT EXISTS "${stopped_file}")
    message(FATAL_ERROR "Activated configd did not terminate with the private bus")
endif()
if(NOT EXISTS "${config_home}/hyprshelld/settings.json")
    message(FATAL_ERROR "Activated configd did not create its isolated settings file")
endif()
if(NOT EXISTS "${state_home}/hyprshelld/settings.last-good.json")
    message(FATAL_ERROR "Activated configd did not create its isolated recovery file")
endif()
