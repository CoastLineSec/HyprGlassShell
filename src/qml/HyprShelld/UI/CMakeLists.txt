set_source_files_properties(
    ShellTheme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(
    hyprshelld-ui
    SHARED
    URI HyprShelld.UI
    QML_FILES
        ShellTheme.qml
        Bar.qml
        BarHeightControl.qml
        WorkspaceSwitcher.qml
)

target_link_libraries(
    hyprshelld-ui
    PUBLIC
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

qt_query_qml_module(
    hyprshelld-ui
    PLUGIN_TARGET module_plugin
    TARGET_PATH module_target_path
    QMLDIR module_qmldir
    TYPEINFO module_typeinfo
    QML_FILES module_qml_files
    QML_FILES_DEPLOY_PATHS module_qml_deploy_paths
)

set(module_install_dir "${QT6_INSTALL_QML}/${module_target_path}")

if(IS_ABSOLUTE "${module_install_dir}")
    set(module_install_absolute "${module_install_dir}")
else()
    set(module_install_absolute "${CMAKE_INSTALL_PREFIX}/${module_install_dir}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(library_install_absolute "${CMAKE_INSTALL_LIBDIR}")
else()
    set(library_install_absolute "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()

file(
    RELATIVE_PATH module_library_path
    "${module_install_absolute}"
    "${library_install_absolute}"
)

set_target_properties(
    "${module_plugin}"
    PROPERTIES
        INSTALL_RPATH "\$ORIGIN/${module_library_path}"
)

install(
    TARGETS hyprshelld-ui
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
    TARGETS "${module_plugin}"
    ARCHIVE DESTINATION "${module_install_dir}"
    LIBRARY DESTINATION "${module_install_dir}"
    RUNTIME DESTINATION "${module_install_dir}"
)

install(
    FILES
        "${module_qmldir}"
        "${module_typeinfo}"
    DESTINATION "${module_install_dir}"
)

list(LENGTH module_qml_files module_qml_file_count)
if(module_qml_file_count GREATER 0)
    math(EXPR module_qml_last_index "${module_qml_file_count} - 1")
    foreach(index RANGE 0 ${module_qml_last_index})
        list(GET module_qml_files ${index} source_file)
        list(GET module_qml_deploy_paths ${index} deploy_path)
        get_filename_component(destination_name "${deploy_path}" NAME)
        get_filename_component(destination_dir "${deploy_path}" DIRECTORY)

        install(
            FILES "${source_file}"
            DESTINATION "${module_install_dir}/${destination_dir}"
            RENAME "${destination_name}"
        )
    endforeach()
endif()
