set(hyprshelld_systemd_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/systemd")
set(hyprshelld_systemd_binary_dir "${CMAKE_CURRENT_BINARY_DIR}/systemd")
file(MAKE_DIRECTORY "${hyprshelld_systemd_binary_dir}")

set(
    HYPRSHELLD_SYSTEMD_USER_UNIT_DIR
    "lib/systemd/user"
    CACHE PATH
    "Installation directory for systemd user units"
)

if(IS_ABSOLUTE "${QT6_INSTALL_QML}")
    set(HYPRSHELLD_QML_INSTALL_DIR "${QT6_INSTALL_QML}")
else()
    set(
        HYPRSHELLD_QML_INSTALL_DIR
        "${CMAKE_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
    )
endif()

set(
    hyprshelld_systemd_unit_sources
    hyprshelld.target
    session-hyprshelld.slice
    hyprshelld.service.in
    hyprshelld-configd.service.in
    hyprshelld-surfaced.service.in
)

set(hyprshelld_systemd_units)

foreach(unit_source IN LISTS hyprshelld_systemd_unit_sources)
    string(REGEX REPLACE "\\.in$" "" unit_name "${unit_source}")
    set(unit_output "${hyprshelld_systemd_binary_dir}/${unit_name}")

    if(unit_source MATCHES "\\.in$")
        configure_file(
            "${hyprshelld_systemd_source_dir}/${unit_source}"
            "${unit_output}"
            @ONLY
        )
    else()
        configure_file(
            "${hyprshelld_systemd_source_dir}/${unit_source}"
            "${unit_output}"
            COPYONLY
        )
    endif()

    list(APPEND hyprshelld_systemd_units "${unit_output}")
endforeach()

install(
    FILES ${hyprshelld_systemd_units}
    DESTINATION "${HYPRSHELLD_SYSTEMD_USER_UNIT_DIR}"
)

set(hyprshelld_dbus_binary_dir "${CMAKE_CURRENT_BINARY_DIR}/dbus")
file(MAKE_DIRECTORY "${hyprshelld_dbus_binary_dir}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/dbus/org.hyprshelld.Config1.service.in"
    "${hyprshelld_dbus_binary_dir}/org.hyprshelld.Config1.service"
    @ONLY
)

install(
    FILES "${hyprshelld_dbus_binary_dir}/org.hyprshelld.Config1.service"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services"
)
