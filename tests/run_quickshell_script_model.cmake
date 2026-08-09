foreach(required_variable
        QUICKSHELL_EXECUTABLE
        QML_IMPORT_PATH
        TEST_BUILD_ROOT
        TEST_ROOT
        TEST_QML)
    if(NOT DEFINED ${required_variable}
            OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${required_variable}")
    endif()
endforeach()

get_filename_component(test_root_name "${TEST_ROOT}" NAME)
get_filename_component(test_root_parent "${TEST_ROOT}" DIRECTORY)
if(NOT test_root_name STREQUAL ".qsm"
        OR NOT test_root_parent STREQUAL TEST_BUILD_ROOT)
    message(FATAL_ERROR "Refusing unsafe test root: ${TEST_ROOT}")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
foreach(directory config state cache data runtime)
    file(MAKE_DIRECTORY "${TEST_ROOT}/${directory}")
endforeach()
file(
    CHMOD
    "${TEST_ROOT}/runtime"
    PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
)

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "QML_IMPORT_PATH=${QML_IMPORT_PATH}"
        "QML2_IMPORT_PATH=${QML_IMPORT_PATH}"
        "XDG_CONFIG_HOME=${TEST_ROOT}/config"
        "XDG_STATE_HOME=${TEST_ROOT}/state"
        "XDG_CACHE_HOME=${TEST_ROOT}/cache"
        "XDG_DATA_HOME=${TEST_ROOT}/data"
        "XDG_RUNTIME_DIR=${TEST_ROOT}/runtime"
        "QS_DISABLE_CRASH_HANDLER=1"
        "${QUICKSHELL_EXECUTABLE}"
        --no-color
        -p
        "${TEST_QML}"
    RESULT_VARIABLE quickshell_result
    OUTPUT_VARIABLE quickshell_output
    ERROR_VARIABLE quickshell_error
    TIMEOUT 12
)

set(quickshell_log "${quickshell_output}${quickshell_error}")
file(REMOVE_RECURSE "${TEST_ROOT}")
message("${quickshell_log}")

if(NOT "${quickshell_result}" STREQUAL "0")
    message(FATAL_ERROR
        "Quickshell ScriptModel harness exited with ${quickshell_result}"
    )
endif()
if(quickshell_log MATCHES "SCRIPT_MODEL_RECONCILIATION_FAIL")
    message(FATAL_ERROR "Quickshell ScriptModel harness reported failure")
endif()
if(NOT quickshell_log MATCHES "SCRIPT_MODEL_RECONCILIATION_PASS")
    message(FATAL_ERROR "Quickshell ScriptModel harness did not pass")
endif()
