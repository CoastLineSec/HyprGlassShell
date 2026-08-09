import QtQuick
import QtQuick.Window
import QtTest
import HyprShelld.UI
import "../../src/surfaced/WorkspaceProjection.js" as WorkspaceProjection

TestCase {
    id: testCase

    name: "Workspaces"
    when: windowShown

    readonly property var sampleWorkspaces: [
        {
            key: "workspace:1",
            workspaceId: 1,
            name: "1",
            numberLabel: "1",
            active: false,
            urgent: false,
            occupied: false,
            applications: []
        },
        {
            key: "workspace:2",
            workspaceId: 2,
            name: "writing",
            numberLabel: "2",
            active: true,
            urgent: false,
            occupied: true,
            applications: [
                {
                    key: "browser",
                    itemKey: "browser:0xa",
                    activationKey: "0xa",
                    label: "Browser",
                    iconSource: "",
                    fallbackInitial: "B",
                    active: true,
                    count: 1,
                    activatable: true
                },
                {
                    key: "editor",
                    itemKey: "editor:0xb",
                    activationKey: "0xb",
                    label: "Editor",
                    iconSource: "",
                    fallbackInitial: "E",
                    active: false,
                    count: 1,
                    activatable: true
                },
                {
                    key: "files",
                    itemKey: "files:0xc",
                    activationKey: "0xc",
                    label: "Files",
                    iconSource: "",
                    fallbackInitial: "F",
                    active: false,
                    count: 1,
                    activatable: true
                },
                {
                    key: "mail",
                    itemKey: "mail:0xd",
                    activationKey: "0xd",
                    label: "Mail",
                    iconSource: "",
                    fallbackInitial: "M",
                    active: false,
                    count: 1,
                    activatable: true
                }
            ]
        },
        {
            key: "workspace:-44",
            workspaceId: -44,
            name: "chat",
            numberLabel: "chat",
            active: false,
            urgent: true,
            occupied: true,
            applications: [
                {
                    key: "chat",
                    itemKey: "chat",
                    activationKey: "0xe",
                    label: "Chat",
                    iconSource: "",
                    fallbackInitial: "C",
                    active: false,
                    count: 2,
                    activatable: true
                }
            ]
        }
    ]

    SignalSpy {
        id: requestSpy
        signalName: "workspaceRequested"
    }

    SignalSpy {
        id: applicationSpy
        signalName: "applicationRequested"
    }

    SignalSpy {
        id: stepSpy
        signalName: "workspaceStepRequested"
    }

    Component {
        id: switcherWindowComponent

        Window {
            width: 520
            height: 100
            visible: true

            property alias switcher: workspaceSwitcher

            WorkspaceSwitcher {
                id: workspaceSwitcher

                x: 10
                y: 24
                width: 500
                height: 40
                workspaces: testCase.sampleWorkspaces
                available: true
                outputName: "DP-4"
                showApplications: true
                maximumApplications: 2
                scrollMode: "disabled"
                keyboardNavigationEnabled: true
                animationsEnabled: false
            }
        }
    }

    Component {
        id: wheelPassThroughWindowComponent

        Window {
            width: 520
            height: 100
            visible: true

            property int parentWheelCount: 0
            property alias switcher: workspaceSwitcher

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: wheel => {
                    ++parentWheelCount;
                    wheel.accepted = true;
                }
            }

            WorkspaceSwitcher {
                id: workspaceSwitcher

                x: 10
                y: 24
                width: 500
                height: 40
                workspaces: testCase.sampleWorkspaces
                available: true
                outputName: "Preview"
                showApplications: true
                maximumApplications: 2
                scrollMode: "normal"
                interactive: false
                keyboardNavigationEnabled: false
                animationsEnabled: false
            }
        }
    }

    Component {
        id: projectionHarnessComponent

        QtObject {
            property var sourceSnapshot: testCase.workspaceSnapshot(
                [
                    testCase.rawMonitor(0, "DP-4", 2, "2", true),
                    testCase.rawMonitor(1, "HDMI-A-2", 3, "3", false)
                ],
                [
                    testCase.rawWorkspace(2, "2", "DP-4", 0, 0, false),
                    testCase.rawWorkspace(3, "3", "HDMI-A-2", 1, 0, false),
                    testCase.rawWorkspace(1, "1", "DP-4", 0, 0, false)
                ],
                []
            )
            readonly property var entries: WorkspaceProjection.project(
                sourceSnapshot,
                "DP-4",
                {
                    resolveApplication: rawId => ({
                        key: String(rawId).toLowerCase(),
                        label: "Editor",
                        iconSource: "",
                        fallbackInitial: "E"
                    })
                }
            )
        }
    }

    function cleanup() {
        requestSpy.target = null;
        requestSpy.clear();
        applicationSpy.target = null;
        applicationSpy.clear();
        stepSpy.target = null;
        stepSpy.clear();
    }

    function rawMonitor(id, name, activeId, activeName, focused) {
        return {
            id: id,
            name: name,
            focused: focused,
            activeWorkspaceId: activeId,
            activeWorkspaceName: activeName
        };
    }

    function rawWorkspace(id, name, outputName, monitorId, windows, urgent) {
        return {
            id: id,
            name: name,
            monitorName: outputName,
            monitorId: monitorId,
            windowCount: windows,
            lastWindowAddress: "",
            urgent: urgent
        };
    }

    function rawClient(address, workspaceId, applicationId, active) {
        return {
            address: address,
            workspaceId: workspaceId,
            workspaceName: String(workspaceId),
            monitorId: 0,
            applicationId: applicationId,
            initialApplicationId: applicationId,
            title: applicationId,
            mapped: true,
            hidden: false,
            focusHistoryId: active ? 0 : 1,
            active: active,
            urgent: false
        };
    }

    function workspaceSnapshot(monitors, workspaces, clients) {
        return {
            revision: 1,
            monitors: monitors,
            workspaces: workspaces,
            clients: clients
        };
    }

    function iconWorkspace(iconSource) {
        return [{
            key: "workspace:2",
            workspaceId: 2,
            name: "2",
            numberLabel: "2",
            active: true,
            urgent: false,
            occupied: true,
            applications: [{
                key: "browser",
                itemKey: "browser:0xa",
                activationKey: "0xa",
                label: "Browser",
                iconSource: iconSource,
                fallbackInitial: "B",
                active: true,
                count: 1,
                activatable: true
            }]
        }];
    }

    function test_projectionFiltersOrdersAndRetainsNamedWorkspaces() {
        const sourceWorkspaces = [
            rawWorkspace(-1337, "name:writing", "DP-4", 0, 0, true),
            rawWorkspace(2, "2", "DP-4", 0, 0, false),
            rawWorkspace(-99, "special:scratch", "DP-4", 0, 0, false),
            rawWorkspace(3, "3", "HDMI-A-2", 1, 0, false),
            rawWorkspace(-44, "name:chat", "DP-4", 0, 0, false),
            rawWorkspace(1, "1", "DP-4", 0, 0, false)
        ];
        const source = workspaceSnapshot(
            [
                rawMonitor(0, "DP-4", 2, "2", true),
                rawMonitor(1, "HDMI-A-2", 3, "3", false)
            ],
            sourceWorkspaces,
            []
        );
        const sourceOrder = sourceWorkspaces.map(workspace => workspace.id);
        const projected = WorkspaceProjection.project(source, "DP-4");

        compare(projected.length, 4);
        compare(projected[0].workspaceId, 1);
        compare(projected[1].workspaceId, 2);
        compare(projected[1].active, true);
        compare(projected[2].workspaceId, -44);
        compare(projected[2].name, "chat");
        compare(projected[2].numberLabel, "chat");
        compare(projected[3].workspaceId, -1337);
        compare(projected[3].name, "writing");
        compare(projected[3].urgent, true);
        compare(
            sourceWorkspaces.map(workspace => workspace.id),
            sourceOrder
        );
        compare(WorkspaceProjection.find(source, "DP-4", -1337).name,
            "name:writing");
        compare(WorkspaceProjection.find(source, "DP-4", -99), null);
        compare(WorkspaceProjection.project(source, "missing").length, 0);
    }

    function test_numericLookingNamedWorkspaceKeepsNamedSemantics() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        switcher.showIdentifiers = true;
        switcher.showNames = true;
        switcher.showApplications = false;
        switcher.workspaces = [{
            key: "workspace:-44",
            workspaceId: -44,
            name: "123",
            numberLabel: "123",
            active: true,
            urgent: false,
            occupied: false,
            applications: []
        }];
        waitForRendering(switcher);

        const indicator = findChild(switcher, "workspaceIndicator--44");
        const identifier = findChild(switcher, "workspaceIdentifier--44");
        const nameLabel = findChild(switcher, "workspaceLabel--44");
        verify(indicator !== null);
        verify(identifier !== null);
        verify(nameLabel !== null);
        compare(indicator.circleIdentifier, "1");
        compare(indicator.nameLabel, "123");
        compare(identifier.text, "1");
        compare(identifier.visible, true);
        compare(nameLabel.text, "123");
        compare(nameLabel.visible, true);
    }

    function test_projectionGroupsInactiveAppsButKeepsActiveWindows() {
        const clients = [
            rawClient("0xa", 2, "browser", false),
            rawClient("0xb", 2, "browser", true),
            rawClient("0xc", 2, "terminal", false)
        ];
        const resolver = rawId => ({
            key: rawId === "browser" ? "org.example.browser" : rawId,
            label: rawId === "browser" ? "Browser" : "Terminal",
            iconSource: "",
            fallbackInitial: rawId === "browser" ? "B" : "T"
        });

        const inactive = WorkspaceProjection.projectApplications(
            clients,
            false,
            resolver
        );
        compare(inactive.length, 2);
        compare(inactive[0].key, "org.example.browser");
        compare(inactive[0].count, 2);
        compare(inactive[0].activationKey, "0xb");
        compare(inactive[0].active, true);
        compare(inactive[1].fallbackInitial, "T");
        compare(inactive[1].activatable, true);

        const active = WorkspaceProjection.projectApplications(
            clients,
            true,
            resolver
        );
        compare(active.length, 3);
        compare(active[0].activationKey, "0xb");
        compare(active[0].active, true);
        compare(active[1].activationKey, "0xa");
        compare(active[0].count, 1);
        compare(active[1].count, 1);
        verify(active[0].itemKey !== active[1].itemKey);
    }

    function test_activeApplicationIsVisibleWhenItStartedBeyondTheCap() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        const applications = WorkspaceProjection.projectApplications(
            [
                rawClient("0x1", 9, "editor", false),
                rawClient("0x2", 9, "files", false),
                rawClient("0x3", 9, "browser", true)
            ],
            true,
            rawId => ({
                key: rawId,
                label: rawId,
                iconSource: "",
                fallbackInitial: rawId.charAt(0).toUpperCase()
            })
        );
        compare(applications[0].activationKey, "0x3");

        switcher.maximumApplications = 1;
        switcher.workspaces = [{
            key: "workspace:9",
            workspaceId: 9,
            name: "9",
            numberLabel: "9",
            active: true,
            urgent: false,
            occupied: true,
            applications: applications
        }];
        waitForRendering(switcher);

        const visibleApplication = findChild(
            switcher,
            "workspaceApplication-9-0"
        );
        const overflow = findChild(
            switcher,
            "workspaceApplicationOverflow-9"
        );
        verify(visibleApplication !== null);
        verify(overflow !== null);
        compare(visibleApplication.activationKey, "0x3");
        compare(visibleApplication.Accessible.selected, true);
        verify(overflow.Accessible.name.includes("2 more"));
    }

    function test_realEmptyWorkspacesRemainAndProjectionNeverPads() {
        const source = workspaceSnapshot(
            [rawMonitor(0, "DP-4", 1, "1", true)],
            [
                rawWorkspace(2, "2", "DP-4", 0, 0, false),
                rawWorkspace(3, "3", "DP-4", 0, 1, false),
                rawWorkspace(1, "1", "DP-4", 0, 0, false),
                rawWorkspace(8, "8", "DP-4", 0, 0, false)
            ],
            [rawClient("0xd", 3, "files", false)]
        );

        const allEntries = WorkspaceProjection.project(source, "DP-4");
        compare(allEntries.length, 4);
        compare(allEntries[3].workspaceId, 8);
        compare(allEntries[3].occupied, false);
        verify(!("placeholder" in allEntries[3]));

        const entries = WorkspaceProjection.project(
            source,
            "DP-4",
            { occupiedOnly: true }
        );
        compare(entries.length, 2);
        compare(entries[0].workspaceId, 1);
        compare(entries[0].active, true);
        compare(entries[1].workspaceId, 3);
        compare(entries[1].occupied, true);
        verify(!entries.some(entry => String(entry.key)
            .startsWith("placeholder:")));
        compare(WorkspaceProjection.find(source, "DP-4", null), null);

        const hiddenApplications = WorkspaceProjection.project(
            source,
            "DP-4",
            { showApplications: false }
        );
        compare(hiddenApplications[2].workspaceId, 3);
        compare(hiddenApplications[2].occupied, true);
        compare(hiddenApplications[2].applications.length, 0);

        const incomplete = workspaceSnapshot(
            [rawMonitor(0, "DP-4", 99, "99", true)],
            [rawWorkspace(1, "1", "DP-4", 0, 0, false)],
            []
        );
        compare(WorkspaceProjection.outputAvailable(incomplete, "DP-4"),
            false);
        compare(WorkspaceProjection.project(
            incomplete,
            "DP-4"
        ).length, 0);
    }

    function test_projectionTracksAtomicSnapshotReplacements() {
        const harness = createTemporaryObject(
            projectionHarnessComponent,
            this
        );
        verify(harness !== null);
        compare(harness.entries.length, 2);
        compare(harness.entries[1].active, true);

        harness.sourceSnapshot = workspaceSnapshot(
            [rawMonitor(0, "DP-4", 1, "name:writing", true)],
            [
                rawWorkspace(1, "name:writing", "DP-4", 0, 1, false),
                rawWorkspace(2, "2", "DP-4", 0, 0, true)
            ],
            [rawClient("0x100", 1, "Editor", true)]
        );
        tryVerify(function() {
            return harness.entries[0].active
                && !harness.entries[1].active
                && harness.entries[1].urgent;
        });
        compare(harness.entries[0].name, "writing");
        compare(harness.entries[0].applications.length, 1);
        compare(harness.entries[0].applications[0].fallbackInitial, "E");
        compare(harness.entries[0].applications[0].active, true);

        harness.sourceSnapshot = workspaceSnapshot(
            [rawMonitor(0, "DP-4", 2, "2", true)],
            [rawWorkspace(2, "2", "DP-4", 0, 0, false)],
            []
        );
        tryVerify(function() {
            return harness.entries.length === 1;
        });
        compare(harness.entries[0].workspaceId, 2);
    }

    /*
        The remaining tests exercise the transport-free shared renderer.
    */

    function test_rendererShowsStatesLabelsAppsAndOverflow() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        const occupiedEntry = Object.assign(
            {},
            testCase.sampleWorkspaces[0],
            {
                key: "workspace:5",
                workspaceId: 5,
                name: "5",
                numberLabel: "5",
                occupied: true
            }
        );
        switcher.workspaces = testCase.sampleWorkspaces.concat([
            occupiedEntry
        ]);
        waitForRendering(switcher);

        const firstIndicator = findChild(switcher, "workspaceIndicator-1");
        const currentIndicator = findChild(switcher, "workspaceIndicator-2");
        const firstCircle = findChild(switcher, "workspaceCircle-1");
        const currentCircle = findChild(switcher, "workspaceCircle-2");
        const urgentCircle = findChild(switcher, "workspaceCircle--44");
        const occupiedCircle = findChild(switcher, "workspaceCircle-5");
        const firstIdentifier = findChild(
            switcher,
            "workspaceIdentifier-1"
        );
        const currentIdentifier = findChild(
            switcher,
            "workspaceIdentifier-2"
        );
        const namedIdentifier = findChild(
            switcher,
            "workspaceIdentifier--44"
        );
        const currentLabel = findChild(switcher, "workspaceLabel-2");
        const firstLabel = findChild(switcher, "workspaceLabel-1");
        const current = findChild(switcher, "workspaceButton-2");
        const urgent = findChild(switcher, "workspaceButton--44");
        const urgentMarker = findChild(
            switcher,
            "workspaceUrgentMarker--44"
        );
        const activeApp = findChild(switcher, "workspaceApplication-2-0");
        const fallback = findChild(
            switcher,
            "workspaceApplicationFallback-2-0"
        );
        const overflow = findChild(
            switcher,
            "workspaceApplicationOverflow-2"
        );
        const groupedCount = findChild(
            switcher,
            "workspaceApplicationCount--44-0"
        );
        const activeApplicationMarker = findChild(
            switcher,
            "workspaceApplicationActiveMarker-2-0"
        );
        verify(firstIndicator !== null);
        verify(currentIndicator !== null);
        verify(firstCircle !== null);
        verify(currentCircle !== null);
        verify(urgentCircle !== null);
        verify(occupiedCircle !== null);
        verify(firstIdentifier !== null);
        verify(currentIdentifier !== null);
        verify(namedIdentifier !== null);
        verify(currentLabel !== null);
        verify(firstLabel !== null);
        verify(current !== null);
        verify(urgent !== null);
        verify(urgentMarker !== null);
        verify(activeApp !== null);
        verify(fallback !== null);
        verify(overflow !== null);
        verify(groupedCount !== null);
        verify(activeApplicationMarker !== null);
        compare(switcher.showIdentifiers, true);
        compare(switcher.showNames, false);
        compare(switcher.transitionDuration, 150);
        switcher.height = 50;
        verify(findChild(switcher, "workspaceRail") === null);
        verify(findChild(switcher, "workspaceSelectionLens") === null);
        compare(firstIndicator.width, switcher.workspaceHitCellWidth);
        verify(currentIndicator.width > switcher.workspaceHitCellWidth);
        compare(firstCircle.width, switcher.inactiveCircleSize);
        compare(firstCircle.height, switcher.inactiveCircleSize);
        compare(firstCircle.color, "#00000000");
        compare(firstCircle.border.width, 1);
        compare(firstCircle.border.color, switcher.emptyRingColor);
        compare(currentCircle.width, switcher.activeCircleSize);
        compare(currentCircle.height, switcher.activeCircleSize);
        compare(currentCircle.color, switcher.activeFillColor);
        compare(currentCircle.border.width, 1);
        compare(currentCircle.border.color, switcher.activeEdgeColor);
        compare(urgentCircle.width, switcher.inactiveCircleSize);
        compare(urgentCircle.color, "#00000000");
        compare(urgentCircle.border.width, 2);
        compare(urgentCircle.border.color, switcher.urgentColor);
        compare(occupiedCircle.width, switcher.inactiveCircleSize);
        compare(occupiedCircle.color, "#00000000");
        compare(occupiedCircle.border.width, 1.5);
        compare(occupiedCircle.border.color, switcher.occupiedRingColor);
        compare(firstIdentifier.text, "1");
        compare(currentIdentifier.text, "2");
        compare(namedIdentifier.text, "C");
        compare(firstIdentifier.visible, true);
        compare(currentIndicator.nameLabel, "");
        compare(currentLabel.visible, false);
        compare(firstLabel.visible, false);
        compare(current.Accessible.selected, true);
        verify(current.Accessible.description.includes("Current"));
        compare(urgent.Accessible.selected, false);
        verify(urgent.Accessible.description.includes("attention"));
        compare(urgentMarker.visible, true);
        verify(current.Accessible.name.includes("DP-4"));
        compare(activeApp.Accessible.selected, true);
        compare(fallback.visible, true);
        compare(fallback.text, "B");
        compare(activeApplicationMarker.visible, true);
        compare(overflow.visible, true);
        verify(overflow.Accessible.name.includes("2 more"));
        compare(groupedCount.visible, true);
        compare(currentIndicator.applicationOverflow, 2);
        const currentAccessibleName = current.Accessible.name;
        const urgentAccessibleName = urgent.Accessible.name;

        switcher.showApplications = false;
        compare(currentIndicator.applicationOverflow, 0);
        compare(currentIndicator.width, switcher.workspaceHitCellWidth);
        compare(firstIndicator.width, switcher.workspaceHitCellWidth);
        compare(currentCircle.width, switcher.activeCircleSize);

        const firstIndicatorBefore = firstIndicator;
        const currentIndicatorBefore = currentIndicator;
        const firstCircleX = firstCircle.x;
        const currentCircleX = currentCircle.x;

        switcher.showIdentifiers = false;
        compare(firstIdentifier.visible, false);
        compare(currentIdentifier.visible, false);
        compare(namedIdentifier.visible, false);
        compare(currentIndicator.nameLabel, "");
        compare(firstCircle.width, switcher.inactiveCircleSize);
        compare(currentCircle.width, switcher.activeCircleSize);
        compare(firstCircle.color, "#00000000");
        compare(currentCircle.color, switcher.activeFillColor);
        compare(firstCircle.x, firstCircleX);
        compare(currentCircle.x, currentCircleX);
        compare(
            findChild(switcher, "workspaceIndicator-1"),
            firstIndicatorBefore
        );
        compare(
            findChild(switcher, "workspaceIndicator-2"),
            currentIndicatorBefore
        );
        compare(current.Accessible.name, currentAccessibleName);
        compare(urgent.Accessible.name, urgentAccessibleName);

        switcher.showNames = true;
        compare(currentIndicator.nameLabel, "writing");
        compare(currentLabel.visible, true);
        compare(firstLabel.visible, false);
        compare(currentIdentifier.visible, false);
        compare(namedIdentifier.visible, false);
        compare(currentIdentifier.text, "");
        verify(currentLabel.width <= 88);
        compare(urgent.parent.nameLabel, "chat");
        compare(
            findChild(switcher, "workspaceIndicator-2"),
            currentIndicatorBefore
        );

        switcher.showIdentifiers = true;
        compare(firstIdentifier.visible, true);
        compare(currentIdentifier.visible, true);
        compare(namedIdentifier.visible, true);
        compare(currentIdentifier.text, "2");
        compare(namedIdentifier.text, "C");
        compare(currentLabel.visible, true);
        compare(firstCircle.x, firstCircleX);
        compare(currentCircle.x, currentCircleX);

        switcher.showApplications = true;
        const restoredActiveApp = findChild(
            switcher,
            "workspaceApplication-2-0"
        );
        compare(currentLabel.visible, true);
        verify(restoredActiveApp !== null);
        compare(restoredActiveApp.visible, true);
        tryVerify(function() {
            return restoredActiveApp.x
                >= currentLabel.x + currentLabel.width;
        });
        compare(
            findChild(switcher, "workspaceIndicator-2"),
            currentIndicatorBefore
        );
    }

    function test_applicationIconRendering_data() {
        return [
            {
                tag: "empty source",
                source: "",
                expectedStatus: Image.Null,
                expectedIconVisible: false
            },
            {
                tag: "invalid source",
                source: "file:///__hyprshelld_missing_icon__/missing.svg",
                expectedStatus: Image.Error,
                expectedIconVisible: false
            },
            {
                tag: "valid data URI",
                source: "data:image/svg+xml,%3Csvg%20xmlns="
                    + "'http://www.w3.org/2000/svg'%20viewBox="
                    + "'0%200%201%201'%3E%3Crect%20width='1'%20height="
                    + "'1'%20fill='%23ffffff'/%3E%3C/svg%3E",
                expectedStatus: Image.Ready,
                expectedIconVisible: true
            }
        ];
    }

    function test_applicationIconRendering(data) {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        switcher.workspaces = iconWorkspace(data.source);
        waitForRendering(switcher);

        const icon = findChild(
            switcher,
            "workspaceApplicationIcon-2-0"
        );
        const fallback = findChild(
            switcher,
            "workspaceApplicationFallback-2-0"
        );
        verify(icon !== null);
        verify(fallback !== null);
        tryCompare(icon, "status", data.expectedStatus);
        compare(icon.visible, data.expectedIconVisible);
        compare(fallback.visible, !data.expectedIconVisible);
        compare(fallback.text, "B");
    }

    function test_fullHeightPointerAndKeyboardWorkspaceHitboxes() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        waitForRendering(switcher);
        const first = findChild(switcher, "workspaceButton-1");
        const current = findChild(switcher, "workspaceButton-2");
        const currentApplication = findChild(
            switcher,
            "workspaceApplication-2-0"
        );
        const currentApplicationIcon = findChild(
            switcher,
            "workspaceApplicationIcon-2-0"
        );
        const firstCircle = findChild(switcher, "workspaceCircle-1");
        const currentCircle = findChild(switcher, "workspaceCircle-2");
        verify(first !== null);
        verify(current !== null);
        verify(currentApplication !== null);
        verify(currentApplicationIcon !== null);
        verify(firstCircle !== null);
        verify(currentCircle !== null);
        compare(first.focusPolicy, Qt.StrongFocus);

        for (const height of [24, 40, 50, 96]) {
            switcher.height = height;
            compare(first.height, height);
            compare(current.height, height);
            verify(first.width >= 32);
            verify(current.width >= 32);
            compare(firstCircle.width, 18);
            compare(firstCircle.height, 18);
            compare(currentCircle.width, 24);
            compare(currentCircle.height, 24);
            verify(currentApplication.width >= 24);
            verify(currentApplication.height >= 24);
            verify(currentApplicationIcon.width >= 12);
            verify(currentApplicationIcon.width <= 18);
            compare(
                currentApplicationIcon.height,
                currentApplicationIcon.width
            );
        }

        requestSpy.target = switcher;
        requestSpy.clear();
        mouseClick(first, first.width / 2, first.height / 2, Qt.LeftButton);
        compare(requestSpy.count, 1);
        compare(requestSpy.signalArguments[0][0], 1);

        testWindow.requestActivate();
        first.forceActiveFocus();
        tryCompare(first, "activeFocus", true);
        keyClick(Qt.Key_Space);
        keyClick(Qt.Key_Return);
        keyClick(Qt.Key_Enter);
        compare(requestSpy.count, 4);

        mouseClick(
            current,
            current.width / 2,
            current.height / 2,
            Qt.LeftButton
        );
        compare(requestSpy.count, 4);
    }

    function test_applicationClickUsesDedicatedSignalOnlyWhenCurrent() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        waitForRendering(switcher);
        const activeApp = findChild(switcher, "workspaceApplication-2-0");
        const inactiveApp = findChild(
            switcher,
            "workspaceApplication--44-0"
        );
        verify(activeApp !== null);
        verify(inactiveApp !== null);
        compare(activeApp.enabled, true);
        compare(inactiveApp.enabled, false);
        compare(inactiveApp.Accessible.ignored, true);

        applicationSpy.target = switcher;
        applicationSpy.clear();
        requestSpy.target = switcher;
        requestSpy.clear();
        mouseClick(
            activeApp,
            activeApp.width / 2,
            activeApp.height / 2,
            Qt.LeftButton
        );
        compare(applicationSpy.count, 1);
        compare(applicationSpy.signalArguments[0][0], 2);
        compare(applicationSpy.signalArguments[0][1], "0xa");
        compare(requestSpy.count, 0);

        applicationSpy.clear();
        requestSpy.clear();
        const inactiveAppCenter = inactiveApp.mapToItem(
            switcher,
            inactiveApp.width / 2,
            inactiveApp.height / 2
        );
        mouseClick(
            switcher,
            inactiveAppCenter.x,
            inactiveAppCenter.y,
            Qt.LeftButton
        );
        compare(applicationSpy.count, 0);
        compare(requestSpy.count, 1);
        compare(requestSpy.signalArguments[0][0], -44);
    }

    function test_pointerOnlyAndInvalidEntriesHaveNoVisualGeometry() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        waitForRendering(switcher);
        switcher.keyboardNavigationEnabled = false;
        const first = findChild(switcher, "workspaceButton-1");
        verify(first !== null);
        compare(first.focusPolicy, Qt.NoFocus);

        const realOnlyWidth = findChild(
            switcher,
            "workspaceFlickable"
        ).contentWidth;
        const entriesWithInvalidData = testCase.sampleWorkspaces.concat([{
            key: "invalid:null-workspace",
            workspaceId: null,
            name: "",
            numberLabel: "",
            active: false,
            urgent: false,
            occupied: false,
            applications: []
        }]);
        switcher.workspaces = entriesWithInvalidData;
        tryVerify(function() {
            return Math.abs(findChild(
                switcher,
                "workspaceFlickable"
            ).contentWidth - realOnlyWidth) < 0.5;
        });
        verify(findChild(switcher, "workspacePlaceholderButton-3")
            === null);
        verify(findChild(switcher, "workspacePlaceholderMarker-3")
            === null);
        verify(findChild(switcher, "workspacePlaceholderPill-3")
            === null);
        verify(findChild(switcher, "workspaceIndicator-0") === null);
    }

    function test_wheelDirectionThresholdCooldownAndNoWrapHelper() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        stepSpy.target = switcher;
        stepSpy.clear();

        compare(switcher.wheelDirectionForDelta(120), 0);
        switcher.scrollMode = "normal";
        compare(switcher.wheelDirectionForDelta(120), -1);
        compare(switcher.wheelDirectionForDelta(-120), 1);
        compare(switcher.submitWheelDelta(60, false), false);
        compare(switcher.submitWheelDelta(60, false), true);
        compare(stepSpy.count, 1);
        compare(stepSpy.signalArguments[0][0], -1);
        compare(switcher.submitWheelDelta(-120, false), false);
        wait(130);

        switcher.scrollMode = "reversed";
        compare(switcher.wheelDirectionForDelta(120), 1);
        compare(switcher.submitWheelDelta(120, false), true);
        compare(stepSpy.count, 2);
        compare(stepSpy.signalArguments[1][0], 1);

        compare(
            WorkspaceProjection.adjacentWorkspaceId(
                testCase.sampleWorkspaces,
                1,
                -1
            ),
            null
        );
        compare(
            WorkspaceProjection.adjacentWorkspaceId(
                testCase.sampleWorkspaces,
                1,
                1
            ),
            2
        );
        compare(
            WorkspaceProjection.adjacentWorkspaceId(
                testCase.sampleWorkspaces,
                -44,
                1
            ),
            null
        );
    }

    function test_noninteractiveOrUnavailableSwitcherPassesWheelToParent() {
        const testWindow = createTemporaryObject(
            wheelPassThroughWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        waitForRendering(switcher);
        const wheelArea = findChild(switcher, "workspaceWheelArea");
        verify(wheelArea !== null);
        compare(wheelArea.enabled, false);

        stepSpy.target = switcher;
        stepSpy.clear();
        mouseWheel(
            switcher,
            switcher.width / 2,
            switcher.height / 2,
            0,
            -120
        );
        tryCompare(testWindow, "parentWheelCount", 1);
        compare(stepSpy.count, 0);

        switcher.interactive = true;
        switcher.available = false;
        compare(wheelArea.enabled, false);
        mouseWheel(
            switcher,
            switcher.width / 2,
            switcher.height / 2,
            0,
            -120
        );
        tryCompare(testWindow, "parentWheelCount", 2);
        compare(stepSpy.count, 0);
    }

    function test_flickableKeepsOverflowReachableAndBounded() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        switcher.width = 70;
        switcher.showApplications = false;
        switcher.showNames = true;
        waitForRendering(switcher);
        const flickable = findChild(switcher, "workspaceFlickable");
        const last = findChild(switcher, "workspaceIndicator--44");
        verify(flickable !== null);
        verify(last !== null);
        verify(flickable.contentWidth > flickable.width);
        compare(flickable.interactive, true);
        compare(flickable.boundsBehavior, Flickable.StopAtBounds);

        flickable.contentX = flickable.contentWidth - flickable.width;
        tryCompare(
            flickable,
            "contentX",
            flickable.contentWidth - flickable.width
        );
        const lastPosition = last.mapToItem(
            flickable,
            last.width,
            last.height / 2
        );
        verify(lastPosition.x <= flickable.width + 1);

        switcher.workspaces = testCase.sampleWorkspaces.map(entry => {
            const copy = Object.assign({}, entry);
            copy.active = entry.workspaceId === -44;
            return copy;
        });
        tryVerify(function() {
            return flickable.contentX > 0;
        });
        const activeLast = findChild(switcher, "workspaceIndicator--44");
        verify(activeLast !== null);
        const activeLastPosition = activeLast.mapToItem(
            flickable,
            activeLast.width,
            activeLast.height / 2
        );
        verify(activeLastPosition.x <= flickable.width + 1);
    }

    function test_activeIndicatorStaysVisibleDuringAnimatedExpansion() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        switcher.width = 74;
        switcher.showApplications = false;
        switcher.showIdentifiers = false;
        switcher.showNames = false;
        switcher.animationsEnabled = false;
        switcher.workspaces = testCase.sampleWorkspaces.map(entry => {
            const copy = Object.assign({}, entry);
            copy.active = entry.workspaceId === -44;
            return copy;
        });
        waitForRendering(switcher);

        const flickable = findChild(switcher, "workspaceFlickable");
        const active = findChild(switcher, "workspaceIndicator--44");
        verify(flickable !== null);
        verify(active !== null);
        tryVerify(function() {
            const right = active.mapToItem(
                flickable,
                active.width,
                active.height / 2
            ).x;
            return right <= flickable.width + 1;
        });
        const compactWidth = active.width;

        switcher.animationsEnabled = true;
        switcher.showIdentifiers = true;
        switcher.showNames = true;
        switcher.showApplications = true;
        wait(60);
        verify(active.width > compactWidth);
        let rightEdge = active.mapToItem(
            flickable,
            active.width,
            active.height / 2
        ).x;
        if (active.width < flickable.width) {
            verify(rightEdge <= flickable.width + 1);
        } else {
            const maximumContentX = Math.max(
                0,
                flickable.contentWidth - flickable.width
            );
            const activeContentLeft = active.mapToItem(
                flickable,
                0,
                active.height / 2
            ).x + flickable.contentX;
            const expectedContentX = Math.max(
                0,
                Math.min(maximumContentX, activeContentLeft)
            );
            verify(Math.abs(
                flickable.contentX - expectedContentX
            ) < 1);
        }

        wait(170);
        rightEdge = active.mapToItem(
            flickable,
            active.width,
            active.height / 2
        ).x;
        if (active.width < flickable.width)
            verify(rightEdge <= flickable.width + 1);
        verify(
            flickable.contentX
                <= flickable.contentWidth - flickable.width + 1
        );
    }

    function test_oversizedActiveIndicatorUsesStableClampedAlignment() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        switcher.width = 42;
        switcher.showApplications = false;
        switcher.showIdentifiers = false;
        switcher.showNames = false;
        switcher.animationsEnabled = false;
        switcher.workspaces = testCase.sampleWorkspaces.map(entry => {
            const copy = Object.assign({}, entry);
            copy.active = entry.workspaceId === -44;
            return copy;
        });
        waitForRendering(switcher);

        const flickable = findChild(switcher, "workspaceFlickable");
        const active = findChild(switcher, "workspaceIndicator--44");
        verify(flickable !== null);
        verify(active !== null);
        const initialX = active.x;

        switcher.animationsEnabled = true;
        switcher.showIdentifiers = true;
        switcher.showNames = true;
        switcher.showApplications = true;
        wait(240);
        verify(active.width > flickable.width);
        verify(active.x > initialX);

        const maximumContentX = Math.max(
            0,
            flickable.contentWidth - flickable.width
        );
        const activeContentLeft = active.mapToItem(
            flickable,
            0,
            active.height / 2
        ).x + flickable.contentX;
        const expectedContentX = Math.max(
            0,
            Math.min(maximumContentX, activeContentLeft)
        );
        tryVerify(function() {
            return Math.abs(flickable.contentX - expectedContentX) < 1;
        });
        verify(flickable.contentX >= 0);
        verify(flickable.contentX <= maximumContentX + 1);

        const stableContentX = flickable.contentX;
        wait(200);
        verify(Math.abs(flickable.contentX - stableContentX) < 0.5);
    }

    function test_unavailableStateIsVisibleAndInert() {
        const testWindow = createTemporaryObject(
            switcherWindowComponent,
            this
        );
        verify(testWindow !== null);
        const switcher = testWindow.switcher;
        waitForRendering(switcher);
        switcher.available = false;
        const unavailable = findChild(
            switcher,
            "workspaceUnavailableLabel"
        );
        verify(unavailable !== null);
        compare(unavailable.visible, true);
        verify(unavailable.Accessible.name.includes("DP-4"));
        tryVerify(function() {
            return findChild(switcher, "workspaceButton-1") === null;
        });
    }

}
