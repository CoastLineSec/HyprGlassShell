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
            }
        };
    }

    function workspace(id, name, monitorName, monitorId, windows, persistent) {
        return {
            id: id,
            name: name,
            monitor: monitorName,
            monitorID: monitorId,
            windows: windows,
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
        compare(result.snapshot.workspaces[0].persistent, true);
        compare(result.snapshot.workspaces[1].persistent, false);
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
