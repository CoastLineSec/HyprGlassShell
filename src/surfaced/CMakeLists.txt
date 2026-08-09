install(
    FILES
        shell.qml
        BarSurface.qml
        HyprlandWorkspaceSource.qml
        HyprlandSocketRequest.qml
        HyprlandEventStream.qml
        HyprlandWorkspaceProtocol.js
        WorkspaceProjection.js
    DESTINATION "${CMAKE_INSTALL_DATADIR}/hyprshelld/surfaced"
)

install(
    FILES
        components/BarComponentHost.qml
        components/ComponentFactory.qml
        components/BuiltinComponentFactory.qml
        components/DeclarativeComponentFactory.qml
        components/WorkspaceSwitcherComponent.qml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/hyprshelld/surfaced/components"
)
