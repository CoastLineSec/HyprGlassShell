import QtQuick
import QtTest
import "../../src/surfaced/HyprlandWorkspaceProtocol.js" as Protocol
import "../../src/surfaced/WorkspaceProjection.js" as Projection

TestCase {
    name: "HyprlandWorkspaceProtocol"

    function monitor(id, name, activeId, activeName, focused) {
        return {
            id: id,
            name: name,
            focused: focused,
            activeWorkspace: {
                id: activeId,
                name: activeName
            },
            specialWorkspace: {
                id: 0,
                name: ""
            }
        };
    }

    function workspace(
        id,
        name,
        monitorName,
        monitorId,
        windows,
        persistent,
        hasFullscreen
    ) {
        return {
            id: id,
            name: name,
            monitor: monitorName,
            monitorID: monitorId,
            windows: windows,
            hasfullscreen: hasFullscreen === true,
            ispersistent: persistent === true,
            lastwindow: "0x0"
        };
    }

    function client(
        address,
        workspaceId,
        workspaceName,
        monitorId,
        focusHistoryId
    ) {
        return {
            address: address,
            workspace: {
                id: workspaceId,
                name: workspaceName
            },
            monitor: monitorId,
            mapped: true,
            hidden: false,
            visible: true,
            floating: false,
            fullscreen: 0,
            fullscreenHandler: "default",
            class: "org.example.Editor",
            initialClass: "org.example.Editor",
            title: "Editor",
            focusHistoryID: focusHistoryId
        };
    }

    function build(workspaces, monitors, clients, urgency, revision) {
        return Protocol.buildSnapshot(
            JSON.stringify(workspaces),
            JSON.stringify(monitors),
            JSON.stringify(clients),
            urgency || {},
            revision || 1
        );
    }

    function twoWorkspaceState(clients) {
        return build(
            [
                workspace(1, "1", "DP-4", 0, 0, true),
                workspace(2, "writing", "DP-4", 0, 1)
            ],
            [monitor(0, "DP-4", 1, "1", true)],
            clients || []
        );
    }

    function test_statusDialectIsAuthoritativeAndStrict() {
        let result = Protocol.parseStatus(
            '{"configProvider":"lua"}'
        );
        verify(result.ok);
        compare(result.configProvider, "lua");
        compare(result.usingLua, true);
        verify(Protocol.completeResponse(
            "j/status",
            '{"configProvider":"lua"}'
        ));

        result = Protocol.parseStatus(
            '{"configProvider":"hyprlang"}'
        );
        verify(result.ok);
        compare(result.usingLua, false);

        result = Protocol.parseStatus("unknown request\n");
        verify(result.ok);
        compare(result.configProvider, "legacy");
        compare(result.usingLua, false);

        verify(!Protocol.parseStatus('{"configProvider":').ok);
        verify(!Protocol.parseStatus('{"configProvider":"future"}').ok);
        verify(!Protocol.parseStatus("[]").ok);
        verify(!Protocol.completeResponse("j/status", "unknown"));
    }

    function test_completeResponseWaitsForAtomicUtf8Json() {
        const complete = '[{"name":"écriture"}]';
        verify(!Protocol.completeArrayPayload('[{"name":"é'));
        verify(Protocol.completeArrayPayload(complete));
        verify(Protocol.completeResponse("j/workspaces", complete));
        verify(!Protocol.completeResponse("j/workspaces", "{}"));
    }

    function test_validSnapshotNormalizesAndLinksEveryRow() {
        const result = twoWorkspaceState([
            client("ABC", 2, "writing", 0, 1)
        ]);
        verify(result.ok);
        compare(result.snapshot.monitors.length, 1);
        compare(result.snapshot.workspaces.length, 2);
        compare(result.snapshot.clients.length, 1);
        compare(result.snapshot.clients[0].address, "0xabc");
        compare(result.snapshot.clients[0].workspaceId, 2);
        compare(result.snapshot.clients[0].monitorId, 0);
        compare(result.snapshot.clients[0].visible, true);
        compare(result.snapshot.clients[0].floating, false);
        compare(result.snapshot.clients[0].fullscreenMode, 0);
        compare(result.snapshot.clients[0].fullscreenHandler, "default");
        compare(result.snapshot.workspaces[0].persistent, true);
        compare(result.snapshot.workspaces[0].hasFullscreen, false);
        compare(result.snapshot.workspaces[1].persistent, false);
        compare(result.snapshot.monitors[0].specialWorkspaceId, 0);
        compare(result.snapshot.monitors[0].visibleWorkspaceId, 1);
    }

    function test_snapshotRejectsMalformedAndIncompleteRelations() {
        verify(!Protocol.buildSnapshot("[", "[]", "[]", {}, 1).ok);
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 0)],
            [],
            []
        ).ok);
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 0), {}],
            [monitor(0, "DP-4", 1, "1", true)],
            []
        ).ok);
        const invalidPersistent = workspace(1, "1", "DP-4", 0, 0);
        invalidPersistent.ispersistent = 0;
        verify(!build(
            [invalidPersistent],
            [monitor(0, "DP-4", 1, "1", true)],
            []
        ).ok);
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 0)],
            [monitor(0, "DP-4", 2, "2", true)],
            []
        ).ok);
        const missingSpecial = monitor(0, "DP-4", 1, "1", true);
        delete missingSpecial.specialWorkspace;
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 0)],
            [missingSpecial],
            []
        ).ok);
        const malformedSpecial = monitor(0, "DP-4", 1, "1", true);
        malformedSpecial.specialWorkspace = { id: -44, name: "writing" };
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 0)],
            [malformedSpecial],
            []
        ).ok);
        verify(!build(
            [workspace(1, "1", "DP-9", 9, 0)],
            [monitor(0, "DP-4", 1, "1", true)],
            []
        ).ok);
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 0)],
            [monitor(0, "DP-4", 1, "1", true)],
            [client("0xa", 2, "2", 0, 1)]
        ).ok);
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 1)],
            [monitor(0, "DP-4", 1, "1", true)],
            [client("0xa", 1, "stale-name", 0, 1)]
        ).ok);
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 1)],
            [monitor(0, "DP-4", 1, "1", true)],
            [client("0xa", 1, "1", 4, 1)]
        ).ok);

        const missingFullscreen = workspace(1, "1", "DP-4", 0, 1);
        delete missingFullscreen.hasfullscreen;
        verify(!build(
            [missingFullscreen],
            [monitor(0, "DP-4", 1, "1", true)],
            []
        ).ok);

        const malformedClient = client("0xa", 1, "1", 0, 1);
        delete malformedClient.visible;
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 1)],
            [monitor(0, "DP-4", 1, "1", true)],
            [malformedClient]
        ).ok);
        const invalidMode = client("0xa", 1, "1", 0, 1);
        invalidMode.fullscreen = 3;
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 1, false, true)],
            [monitor(0, "DP-4", 1, "1", true)],
            [invalidMode]
        ).ok);
        const unknownHandler = client("0xa", 1, "1", 0, 1);
        unknownHandler.fullscreen = 1;
        unknownHandler.fullscreenHandler = "future";
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 1, false, true)],
            [monitor(0, "DP-4", 1, "1", true)],
            [unknownHandler]
        ).ok);
        const floatingScrolling = client("0xa", 1, "1", 0, 1);
        floatingScrolling.fullscreen = 1;
        floatingScrolling.floating = true;
        floatingScrolling.fullscreenHandler = "scrolling";
        verify(!build(
            [workspace(1, "1", "DP-4", 0, 1, false, true)],
            [monitor(0, "DP-4", 1, "1", true)],
            [floatingScrolling]
        ).ok);

        const unmapped = client("0xa", 1, "stale-name", -1, 1);
        unmapped.mapped = false;
        verify(build(
            [workspace(1, "1", "DP-4", 0, 0)],
            [monitor(0, "DP-4", 1, "1", true)],
            [unmapped]
        ).ok);
    }

    function test_projectionRequiresARealMatchingActiveWorkspace() {
        const empty = Protocol.emptySnapshot();
        compare(Projection.project(
            empty,
            "DP-4"
        ).length, 0);

        const missing = {
            revision: 1,
            monitors: [{
                id: 0,
                name: "DP-4",
                focused: true,
                activeWorkspaceId: 9,
                activeWorkspaceName: "9"
            }],
            workspaces: [{
                id: 1,
                name: "1",
                monitorName: "DP-4",
                monitorId: 0,
                windowCount: 0,
                persistent: false,
                urgent: false
            }],
            clients: []
        };
        verify(!Projection.outputAvailable(missing, "DP-4"));
        compare(Projection.project(
            missing,
            "DP-4"
        ).length, 0);
    }

    function test_urgentBeforeOpenIsRetainedAndAttached() {
        const event = Protocol.parseEvent("urgent>>0xabc");
        let urgency = Protocol.applyUrgentEvent(
            {},
            event,
            Protocol.emptySnapshot()
        );
        compare(urgency["0xabc"], 3);

        let result = twoWorkspaceState([]);
        result = build(
            [
                workspace(1, "1", "DP-4", 0, 0),
                workspace(2, "writing", "DP-4", 0, 1)
            ],
            [monitor(0, "DP-4", 1, "1", true)],
            [],
            urgency
        );
        verify(result.ok);
        compare(result.urgentAddresses["0xabc"], 2);

        result = build(
            [
                workspace(1, "1", "DP-4", 0, 0),
                workspace(2, "writing", "DP-4", 0, 1)
            ],
            [monitor(0, "DP-4", 1, "1", true)],
            [client("0xabc", 2, "writing", 0, 1)],
            result.urgentAddresses
        );
        verify(result.ok);
        compare(result.snapshot.clients[0].urgent, true);
        compare(result.snapshot.workspaces[1].urgent, true);
    }

    function test_unknownUrgencyExpiresAndKnownClears() {
        let urgency = { "0xabc": 3 };
        for (let generation = 0; generation < 3; ++generation) {
            const result = twoWorkspaceState([]);
            const reconciled = Protocol.buildSnapshot(
                JSON.stringify([
                    workspace(1, "1", "DP-4", 0, 0),
                    workspace(2, "writing", "DP-4", 0, 1)
                ]),
                JSON.stringify([monitor(0, "DP-4", 1, "1", true)]),
                "[]",
                urgency,
                generation + 1
            );
            verify(result.ok);
            urgency = result.urgentAddresses;
        }
        compare(Object.keys(urgency).length, 0);

        urgency = { "0xabc": 3 };
        let snapshot = twoWorkspaceState([
            client("0xabc", 2, "writing", 0, 0)
        ]);
        verify(snapshot.ok);
        compare(Object.keys(snapshot.urgentAddresses).length, 0);

        urgency = Protocol.applyUrgentEvent(
            { "0xabc": 3 },
            Protocol.parseEvent("closewindow>>abc"),
            Protocol.emptySnapshot()
        );
        compare(Object.keys(urgency).length, 0);
    }

    function test_preexistingUrgencyIsIntentionallyUnknowable() {
        const result = twoWorkspaceState([
            client("0xabc", 2, "writing", 0, 1)
        ]);
        verify(result.ok);
        compare(result.snapshot.clients[0].urgent, false);
        compare(result.snapshot.workspaces[1].urgent, false);
    }

    function test_irrelevantEventStormCannotInvalidateState() {
        for (let index = 0; index < 100; ++index) {
            verify(!Protocol.parseEvent(
                "windowtitlev2>>abc,title " + index
            ).relevant);
            verify(!Protocol.parseEvent(
                "minimize>>abc," + (index % 2)
            ).relevant);
        }
        verify(Protocol.parseEvent("openwindow>>abc,1,app,title").relevant);
        verify(Protocol.parseEvent("activewindowv2>>abc").relevant);
    }

    function test_exactStateInvalidationEventsArePinned() {
        const relevant = [
            "workspace", "workspacev2", "focusedmon", "focusedmonv2",
            "monitoradded", "monitoraddedv2", "monitorremoved",
            "monitorremovedv2", "createworkspace", "createworkspacev2",
            "destroyworkspace", "destroyworkspacev2", "moveworkspace",
            "moveworkspacev2", "renameworkspace", "changeworkspaceid",
            "openwindow", "closewindow", "movewindow", "movewindowv2",
            "changefloatingmode", "fullscreen", "activespecial",
            "activespecialv2", "minimized", "pin", "togglegroup",
            "moveintogroup", "moveoutofgroup", "activewindow",
            "activewindowv2", "urgent", "configreloaded"
        ];
        for (const name of relevant) {
            const event = Protocol.parseEvent(name + ">>ignored,payload");
            compare(event.valid, true, name);
            compare(event.relevant, true, name);
            const malformed = Protocol.parseEvent(name);
            compare(malformed.valid, false, name);
            compare(malformed.relevant, true, name);
        }
        for (const name of [
            "lockgroups", "windowtitle", "windowtitlev2", "minimize"
        ]) {
            compare(
                Protocol.parseEvent(name + ">>ignored").relevant,
                false,
                name
            );
        }
    }

    function test_coveringModeIsStrictPerOutputAndWorkspace() {
        const regularMonitor = monitor(0, "DP-4", 1, "1", true);
        const coveredWorkspace = workspace(
            1, "1", "DP-4", 0, 2, false, true
        );
        const scrollingMax = client("0xa", 1, "1", 0, 0);
        scrollingMax.fullscreen = 1;
        scrollingMax.fullscreenHandler = "scrolling";
        const defaultFullscreen = client("0xb", 1, "1", 0, 1);
        defaultFullscreen.fullscreen = 2;
        coveredWorkspace.lastwindow = "0xa";

        let result = build(
            [coveredWorkspace],
            [regularMonitor],
            [scrollingMax, defaultFullscreen]
        );
        verify(result.ok);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            2
        );
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-9"
            ),
            0
        );

        const floatingMax = client("0xc", 1, "1", 0, 9);
        floatingMax.fullscreen = 1;
        floatingMax.floating = true;
        result = build(
            [coveredWorkspace],
            [regularMonitor],
            [scrollingMax, defaultFullscreen, floatingMax]
        );
        verify(result.ok);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            1
        );

        const uncoveredWorkspace = workspace(
            1, "1", "DP-4", 0, 1, false, false
        );
        result = build(
            [uncoveredWorkspace],
            [regularMonitor],
            [floatingMax]
        );
        verify(result.ok);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            0
        );
    }

    function test_coveringModeNeverGuessesBetweenAmbiguousClients() {
        const output = monitor(0, "DP-4", 1, "1", true);
        const covered = workspace(1, "1", "DP-4", 0, 2, false, true);
        covered.lastwindow = "0xff";
        const first = client("0xa", 1, "1", 0, 3);
        first.fullscreen = 1;
        const second = client("0xb", 1, "1", 0, 3);
        second.fullscreen = 2;

        let result = build([covered], [output], [first, second]);
        verify(result.ok);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            0
        );

        covered.lastwindow = "0xb";
        result = build([covered], [output], [first, second]);
        verify(result.ok);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            2
        );

        covered.lastwindow = "0xff";
        second.focusHistoryID = 2;
        result = build([covered], [output], [first, second]);
        verify(result.ok);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            2
        );

        second.hidden = true;
        first.visible = false;
        result = build([covered], [output], [first, second]);
        verify(result.ok);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            0
        );
    }

    function test_specialWorkspaceStrictlyWinsTheRegularWorkspace() {
        const output = monitor(0, "DP-4", 1, "1", true);
        output.specialWorkspace = { id: -44, name: "special:writing" };
        const regular = workspace(1, "1", "DP-4", 0, 1, false, true);
        const special = workspace(
            -44, "special:writing", "DP-4", 0, 1, false, true
        );
        const regularFullscreen = client("0xa", 1, "1", 0, 0);
        regularFullscreen.fullscreen = 2;
        const specialMax = client(
            "0xb", -44, "special:writing", 0, 1
        );
        specialMax.fullscreen = 1;

        const result = build(
            [regular, special],
            [output],
            [regularFullscreen, specialMax]
        );
        verify(result.ok);
        compare(result.snapshot.monitors[0].visibleWorkspaceId, -44);
        compare(
            Protocol.visibleWorkspaceCoveringMode(
                result.snapshot, "DP-4"
            ),
            1
        );
    }

    function test_dispatchCommandsAreExactAndRejectControls() {
        const workspace = { id: -44, name: 'write "docs"\\today' };
        compare(
            Protocol.workspaceDispatch(true, workspace),
            'hl.dsp.focus({ workspace = '
                + '"name:write \\"docs\\"\\\\today" })'
        );
        compare(
            Protocol.workspaceDispatch(false, { id: 2, name: "renamed" }),
            "workspace 2"
        );
        compare(
            Protocol.workspaceDispatch(true, { id: 2, name: "renamed" }),
            'hl.dsp.focus({ workspace = "2" })'
        );
        compare(
            Protocol.windowDispatch(true, "ABC"),
            'hl.dsp.focus({ window = "address:0xabc" })'
        );
        compare(
            Protocol.windowDispatch(false, "0xABC"),
            "focuswindow address:0xabc"
        );

        for (let code = 0; code <= 31; ++code) {
            compare(Protocol.workspaceDispatch(true, {
                id: -44,
                name: "bad" + String.fromCharCode(code) + "name"
            }), "");
            compare(Protocol.workspaceDispatch(false, {
                id: -44,
                name: "bad" + String.fromCharCode(code) + "name"
            }), "");
        }
        compare(Protocol.workspaceDispatch(true, {
            id: -44,
            name: "bad" + String.fromCharCode(127) + "name"
        }), "");
        compare(Protocol.workspaceDispatch(false, { id: 0, name: "0" }), "");
        compare(Protocol.windowDispatch(false, "not-an-address"), "");
    }
}
