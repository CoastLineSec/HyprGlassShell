pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property var workspaces
    required property bool available
    required property string outputName
    property bool showIdentifiers: true
    property bool showNames: false
    property bool showApplications: false
    property int maximumApplications: 3
    property string scrollMode: "disabled"
    property bool interactive: true
    property bool keyboardNavigationEnabled: true
    property bool animationsEnabled: true
    property real wheelAccumulator: 0
    property bool wheelCoolingDown: false
    property Item activeIndicator: null

    signal workspaceRequested(int workspaceId)
    signal applicationRequested(int workspaceId, string activationKey)
    signal workspaceStepRequested(int direction)

    readonly property int effectiveMaximumApplications: Math.max(
        1,
        Math.min(5, maximumApplications)
    )
    // Local fallback palette for semantic workspace states. The shared theme
    // contract can supply these roles once it owns component palette values.
    readonly property color activeFillColor: "#263a59"
    readonly property color activeEdgeColor: "#668fc9"
    readonly property color strongTextColor: "#f5f7fa"
    readonly property color occupiedRingColor: "#b8aeb8c6"
    readonly property color occupiedTextColor: "#d9e1ec"
    readonly property color emptyRingColor: "#66aeb8c6"
    readonly property color emptyTextColor: "#8faeb8c6"
    readonly property color mutedTextColor: "#aeb8c6"
    readonly property color urgentColor: "#fb7185"

    readonly property real activeCircleSize: 24
    readonly property real inactiveCircleSize: 18
    readonly property real workspaceHitCellWidth: 32
    readonly property real trailingHeight: 28
    readonly property real applicationIconSize: Math.max(
        12,
        Math.min(18, trailingHeight - 8)
    )
    readonly property real applicationButtonSize: 24
    readonly property real rowHorizontalPadding: 2
    readonly property int transitionDuration: 150

    function workspaceIdentifier(entry) {
        if (!entry || !root.showIdentifiers)
            return "";

        const raw = String(
            entry.numberLabel || entry.workspaceId || ""
        ).trim();
        if (raw.length === 0)
            return "";
        const workspaceId = Number(entry.workspaceId);
        if (Number.isFinite(workspaceId) && workspaceId < 0)
            return raw.charAt(0).toUpperCase();
        if (/^-?\d+$/.test(raw))
            return raw;
        return raw.charAt(0).toUpperCase();
    }

    function workspaceNameLabel(entry) {
        if (!entry || !root.showNames)
            return "";

        const name = String(entry.name || "").trim();
        const numberLabel = String(entry.numberLabel || "").trim();
        if (name.length === 0)
            return "";
        const workspaceId = Number(entry.workspaceId);
        const numberedWorkspace = Number.isFinite(workspaceId)
            ? workspaceId >= 0
            : /^-?\d+$/.test(numberLabel);
        if (numberedWorkspace
                && name === numberLabel
                && /^-?\d+$/.test(numberLabel)) {
            return "";
        }
        return name;
    }

    function wheelDirectionForDelta(delta) {
        if (root.scrollMode === "disabled" || Number(delta) === 0)
            return 0;
        const normalDirection = Number(delta) > 0 ? -1 : 1;
        return root.scrollMode === "reversed"
            ? -normalDirection
            : normalDirection;
    }

    function submitWheelDelta(delta, pixelBased) {
        if (!root.interactive
                || root.scrollMode === "disabled"
                || root.wheelCoolingDown
                || Number(delta) === 0) {
            return false;
        }

        const threshold = pixelBased ? 180 : 120;
        root.wheelAccumulator = Math.max(
            -threshold * 2,
            Math.min(threshold * 2, root.wheelAccumulator + Number(delta))
        );
        if (Math.abs(root.wheelAccumulator) < threshold)
            return false;

        const direction = root.wheelDirectionForDelta(
            root.wheelAccumulator
        );
        root.wheelAccumulator = 0;
        if (direction === 0)
            return false;

        root.wheelCoolingDown = true;
        wheelCooldown.restart();
        root.workspaceStepRequested(direction);
        return true;
    }

    function revealActiveWorkspace() {
        Qt.callLater(root.revealActiveWorkspaceNow);
    }

    function revealActiveWorkspaceNow() {
        if (!root.available || workspaceFlickable.width <= 0)
            return;

        const indicator = root.activeIndicator;
        if (!indicator)
            return;

        const leftEdge = workspaceRow.x + indicator.x;
        const rightEdge = leftEdge + indicator.width;
        const maximumContentX = Math.max(
            0,
            workspaceFlickable.contentWidth - workspaceFlickable.width
        );
        if (indicator.width >= workspaceFlickable.width) {
            workspaceFlickable.contentX = Math.max(
                0,
                Math.min(maximumContentX, leftEdge)
            );
            return;
        }
        if (leftEdge < workspaceFlickable.contentX) {
            workspaceFlickable.contentX = Math.max(
                0,
                Math.min(maximumContentX, leftEdge)
            );
        } else if (rightEdge > workspaceFlickable.contentX
                + workspaceFlickable.width) {
            workspaceFlickable.contentX = Math.min(
                maximumContentX,
                rightEdge - workspaceFlickable.width
            );
        }
    }

    objectName: "workspaceSwitcher"
    implicitWidth: available
        ? workspaceRow.implicitWidth + rowHorizontalPadding * 2
        : unavailableLabel.implicitWidth + 12
    implicitHeight: 36
    Accessible.role: Accessible.Grouping
    Accessible.name: outputName.length > 0
        ? qsTr("Workspaces on %1").arg(outputName)
        : qsTr("Workspaces")
    Accessible.ignored: !interactive

    onWorkspacesChanged: root.revealActiveWorkspace()
    onWidthChanged: root.revealActiveWorkspace()
    onScrollModeChanged: {
        root.wheelAccumulator = 0;
        root.wheelCoolingDown = false;
        wheelCooldown.stop();
    }

    Component.onCompleted: root.revealActiveWorkspace()

    Timer {
        id: wheelCooldown

        interval: 120
        repeat: false
        onTriggered: root.wheelCoolingDown = false
    }

    Flickable {
        id: workspaceFlickable

        objectName: "workspaceFlickable"
        anchors.fill: parent
        contentWidth: workspaceRow.implicitWidth
            + root.rowHorizontalPadding * 2
        contentHeight: height
        clip: true
        interactive: root.interactive && contentWidth > width
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick

        onContentWidthChanged: root.revealActiveWorkspace()

        Item {
            id: workspaceRow

            x: root.rowHorizontalPadding
            y: (parent.height - height) / 2
            height: parent.height
            property real spacing: 2
            property int layoutRevision: 0
            implicitWidth: {
                layoutRevision;
                let width = 0;
                let visibleCount = 0;
                for (let index = 0;
                        index < workspaceRepeater.count; ++index) {
                    const indicator = workspaceRepeater.itemAt(index);
                    if (!indicator || indicator.width <= 0)
                        continue;
                    if (visibleCount > 0)
                        width += spacing;
                    width += indicator.width;
                    ++visibleCount;
                }
                return width;
            }

            function positionFor(targetIndex) {
                let position = 0;
                let visibleCount = 0;
                for (let index = 0; index < targetIndex; ++index) {
                    const indicator = workspaceRepeater.itemAt(index);
                    if (!indicator || indicator.width <= 0)
                        continue;
                    if (visibleCount > 0)
                        position += spacing;
                    position += indicator.width;
                    ++visibleCount;
                }
                if (visibleCount > 0) {
                    const target = workspaceRepeater.itemAt(targetIndex);
                    if (target && target.width > 0)
                        position += spacing;
                }
                return position;
            }

            Repeater {
                id: workspaceRepeater

                model: root.available ? root.workspaces : []

                onCountChanged: {
                    ++workspaceRow.layoutRevision;
                    root.revealActiveWorkspace();
                }

                delegate: Item {
                    id: workspaceIndicator

                    required property var modelData
                    required property int index

                    readonly property var workspaceData: modelData || ({})
                    readonly property bool validWorkspace:
                        workspaceData.workspaceId !== null
                        && workspaceData.workspaceId !== undefined
                        && Number.isFinite(
                            Number(workspaceData.workspaceId)
                        )
                    readonly property int workspaceId: validWorkspace
                        ? Number(workspaceData.workspaceId)
                        : 0
                    readonly property string workspaceName: String(
                        workspaceData.name
                            || workspaceData.numberLabel
                            || workspaceId
                    )
                    readonly property string circleIdentifier:
                        root.workspaceIdentifier(workspaceData)
                    readonly property string nameLabel:
                        root.workspaceNameLabel(workspaceData)
                    readonly property bool workspaceActive: validWorkspace
                        && Boolean(workspaceData.active)
                    readonly property bool workspaceUrgent: validWorkspace
                        && Boolean(workspaceData.urgent)
                    readonly property bool workspaceOccupied: validWorkspace
                        && Boolean(workspaceData.occupied)
                    readonly property var applications: root.showApplications
                        && validWorkspace
                        ? Array.from(workspaceData.applications || [])
                        : []
                    readonly property var visibleApplications:
                        applications.slice(
                            0,
                            root.effectiveMaximumApplications
                        )
                    readonly property int applicationOverflow: Math.max(
                        0,
                        applications.length
                            - root.effectiveMaximumApplications
                    )
                    readonly property bool hasTrailingContent: validWorkspace
                        && (nameLabel.length > 0
                            || visibleApplications.length > 0
                            || applicationOverflow > 0)
                    readonly property real trailingContentWidth:
                        hasTrailingContent
                            ? workspaceTrailingContent.implicitWidth
                            : 0
                    readonly property real targetWidth: validWorkspace
                        ? root.workspaceHitCellWidth
                            + (hasTrailingContent
                                ? trailingContentWidth + 6
                                : 0)
                        : 0

                    function claimActiveIndicator() {
                        if (!workspaceIndicator.validWorkspace)
                            return;
                        root.activeIndicator = workspaceIndicator;
                        root.revealActiveWorkspace();
                    }

                    objectName: validWorkspace
                        ? "workspaceIndicator-" + workspaceId
                        : ""
                    visible: validWorkspace
                    x: {
                        workspaceRow.layoutRevision;
                        return workspaceRow.positionFor(index);
                    }
                    width: targetWidth
                    height: root.height
                    z: 1

                    onWorkspaceActiveChanged: {
                        if (workspaceActive) {
                            claimActiveIndicator();
                        } else if (root.activeIndicator
                                === workspaceIndicator) {
                            root.activeIndicator = null;
                        }
                    }
                    onWidthChanged: {
                        ++workspaceRow.layoutRevision;
                        if (workspaceActive) {
                            root.revealActiveWorkspaceNow();
                            root.revealActiveWorkspace();
                        }
                    }
                    onXChanged: {
                        if (workspaceActive)
                            root.revealActiveWorkspace();
                    }
                    onIndexChanged: {
                        ++workspaceRow.layoutRevision;
                        if (workspaceActive)
                            root.revealActiveWorkspace();
                    }

                    Component.onCompleted: {
                        ++workspaceRow.layoutRevision;
                        if (workspaceActive)
                            claimActiveIndicator();
                    }

                    Component.onDestruction: {
                        if (root.activeIndicator === workspaceIndicator)
                            root.activeIndicator = null;
                    }

                    AbstractButton {
                        id: workspaceButton

                        objectName: workspaceIndicator.validWorkspace
                            ? "workspaceButton-"
                                + workspaceIndicator.workspaceId
                            : ""
                        anchors.fill: parent
                        enabled: root.interactive
                            && workspaceIndicator.validWorkspace
                        hoverEnabled: true
                        focusPolicy: root.keyboardNavigationEnabled
                            && enabled
                            ? (workspaceIndicator.workspaceActive
                                ? Qt.TabFocus
                                : Qt.StrongFocus)
                            : Qt.NoFocus
                        padding: 0
                        background: Item {}
                        contentItem: Item {}
                        z: 0

                        Accessible.role: Accessible.Button
                        Accessible.name: root.outputName.length > 0
                            ? qsTr("Workspace %1 on %2").arg(
                                workspaceIndicator.workspaceName
                            ).arg(root.outputName)
                            : qsTr("Workspace %1").arg(
                                workspaceIndicator.workspaceName
                            )
                        Accessible.description: {
                            if (workspaceIndicator.workspaceActive
                                    && workspaceIndicator.workspaceUrgent) {
                                return qsTr(
                                    "Current occupied workspace; needs attention"
                                );
                            }
                            if (workspaceIndicator.workspaceActive)
                                return qsTr("Current workspace");
                            if (workspaceIndicator.workspaceUrgent)
                                return qsTr("Workspace needs attention");
                            if (workspaceIndicator.workspaceOccupied)
                                return qsTr("Occupied workspace");
                            return qsTr("Switch to this workspace");
                        }
                        Accessible.selected:
                            workspaceIndicator.workspaceActive
                        Accessible.ignored: !root.interactive
                            || !workspaceIndicator.validWorkspace

                        onClicked: {
                            if (!workspaceIndicator.workspaceActive) {
                                root.workspaceRequested(
                                    workspaceIndicator.workspaceId
                                );
                            }
                        }

                        Keys.onReturnPressed: event => {
                            event.accepted = true;
                            if (root.keyboardNavigationEnabled)
                                workspaceButton.click();
                        }

                        Keys.onEnterPressed: event => {
                            event.accepted = true;
                            if (root.keyboardNavigationEnabled)
                                workspaceButton.click();
                        }
                    }

                    Rectangle {
                        id: workspaceTrailingInteraction

                        anchors.verticalCenter: parent.verticalCenter
                        x: root.workspaceHitCellWidth - 2
                        width: Math.max(0, parent.width - x)
                        height: root.trailingHeight
                        radius: height / 2
                        visible: workspaceIndicator.hasTrailingContent
                            && (workspaceButton.hovered
                                || workspaceButton.down)
                        color: workspaceButton.down
                            ? "#18ffffff"
                            : "#0cffffff"
                        z: 1
                        Accessible.ignored: true
                    }

                    Rectangle {
                        id: workspaceCircle

                        objectName: workspaceIndicator.validWorkspace
                            ? "workspaceCircle-"
                                + workspaceIndicator.workspaceId
                            : ""
                        anchors.verticalCenter: parent.verticalCenter
                        x: (root.workspaceHitCellWidth - width) / 2
                        width: workspaceIndicator.workspaceActive
                            ? root.activeCircleSize
                            : root.inactiveCircleSize
                        height: width
                        radius: width / 2
                        color: workspaceIndicator.workspaceActive
                            ? root.activeFillColor
                            : "transparent"
                        border.width: workspaceIndicator.workspaceUrgent
                            ? 2
                            : (workspaceIndicator.workspaceActive
                                ? 1
                                : (workspaceIndicator.workspaceOccupied
                                    ? 1.5
                                    : 1))
                        border.color: workspaceIndicator.workspaceUrgent
                            ? root.urgentColor
                            : (workspaceIndicator.workspaceActive
                                ? root.activeEdgeColor
                                : (workspaceIndicator.workspaceOccupied
                                    ? root.occupiedRingColor
                                    : root.emptyRingColor))
                        scale: workspaceButton.down ? 0.96 : 1
                        z: 2
                        Accessible.ignored: true

                        Behavior on width {
                            enabled: root.animationsEnabled

                            NumberAnimation {
                                duration: root.transitionDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on color {
                            enabled: root.animationsEnabled

                            ColorAnimation {
                                duration: root.transitionDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on border.width {
                            enabled: root.animationsEnabled

                            NumberAnimation {
                                duration: root.transitionDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on border.color {
                            enabled: root.animationsEnabled

                            ColorAnimation {
                                duration: root.transitionDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on scale {
                            enabled: root.animationsEnabled

                            NumberAnimation {
                                duration: 80
                                easing.type: Easing.OutCubic
                            }
                        }

                        Text {
                            id: workspaceIdentifierText

                            objectName: workspaceIndicator.validWorkspace
                                ? "workspaceIdentifier-"
                                    + workspaceIndicator.workspaceId
                                : ""
                            anchors.centerIn: parent
                            width: parent.width - 4
                            visible: workspaceIndicator.circleIdentifier
                                .length > 0
                            text: workspaceIndicator.circleIdentifier
                            color: workspaceIndicator.workspaceActive
                                ? root.strongTextColor
                                : (workspaceIndicator.workspaceUrgent
                                    ? root.urgentColor
                                    : (workspaceIndicator.workspaceOccupied
                                        ? root.occupiedTextColor
                                        : root.emptyTextColor))
                            font.pixelSize:
                                workspaceIndicator.workspaceActive ? 10 : 9
                            font.weight:
                                workspaceIndicator.workspaceActive
                                    ? Font.DemiBold
                                    : Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            maximumLineCount: 1
                            elide: Text.ElideRight
                            Accessible.ignored: true

                            Behavior on color {
                                enabled: root.animationsEnabled

                                ColorAnimation {
                                    duration: root.transitionDuration
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: workspaceButton.hovered
                                || workspaceButton.down
                            radius: width / 2
                            color: workspaceButton.down
                                ? "#18ffffff"
                                : "#0cffffff"
                            Accessible.ignored: true
                        }

                        Rectangle {
                            objectName: workspaceIndicator.validWorkspace
                                ? "workspaceUrgentMarker-"
                                    + workspaceIndicator.workspaceId
                                : ""
                            visible: workspaceIndicator.workspaceUrgent
                            anchors {
                                right: parent.right
                                rightMargin: -1
                                top: parent.top
                                topMargin: -1
                            }
                            width: 5
                            height: 5
                            radius: width / 2
                            color: root.urgentColor
                            border.width: 1
                            border.color: "#17243a"
                            Accessible.ignored: true
                        }
                    }

                    Row {
                        id: workspaceTrailingContent

                        x: root.workspaceHitCellWidth + 2
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5
                        visible: workspaceIndicator.hasTrailingContent
                        z: 3

                        Text {
                            objectName: workspaceIndicator.validWorkspace
                                ? "workspaceLabel-"
                                    + workspaceIndicator.workspaceId
                                : ""
                            visible: workspaceIndicator.nameLabel.length > 0
                            width: Math.min(implicitWidth, 88)
                            text: workspaceIndicator.nameLabel
                            color: workspaceIndicator.workspaceActive
                                ? root.strongTextColor
                                : (workspaceIndicator.workspaceUrgent
                                    ? root.urgentColor
                                    : (workspaceIndicator.workspaceOccupied
                                        ? root.occupiedTextColor
                                        : root.emptyTextColor))
                            font.pixelSize: 11
                            font.weight: workspaceIndicator.workspaceActive
                                ? Font.DemiBold
                                : Font.Medium
                            maximumLineCount: 1
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            Accessible.ignored: true

                            Behavior on color {
                                enabled: root.animationsEnabled

                                ColorAnimation {
                                    duration: root.transitionDuration
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        Repeater {
                            model: workspaceIndicator.visibleApplications

                            delegate: AbstractButton {
                                id: applicationButton

                                required property var modelData
                                required property int index

                                readonly property string activationKey: String(
                                    modelData.activationKey || ""
                                )
                                readonly property bool applicationActive:
                                    Boolean(modelData.active)
                                readonly property int applicationCount:
                                    Math.max(1, Number(modelData.count || 1))

                                objectName: "workspaceApplication-"
                                    + workspaceIndicator.workspaceId
                                    + "-" + index
                                width: root.applicationButtonSize
                                height: root.applicationButtonSize
                                enabled: root.interactive
                                    && workspaceIndicator.workspaceActive
                                    && Boolean(modelData.activatable)
                                hoverEnabled: enabled
                                focusPolicy: root.keyboardNavigationEnabled
                                    && enabled
                                    ? Qt.StrongFocus
                                    : Qt.NoFocus
                                padding: 0

                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Activate %1").arg(
                                    String(modelData.label || "application")
                                )
                                Accessible.description:
                                    applicationActive
                                        ? qsTr("Active window")
                                        : qsTr("Window on the current workspace")
                                Accessible.selected: applicationActive
                                Accessible.ignored: !enabled

                                onClicked: {
                                    root.applicationRequested(
                                        workspaceIndicator.workspaceId,
                                        applicationButton.activationKey
                                    );
                                }

                                background: Rectangle {
                                    radius: height / 2
                                    color: applicationButton.down
                                        ? "#26ffffff"
                                        : (applicationButton.hovered
                                            ? "#16ffffff"
                                            : "transparent")
                                    border.width: applicationButton.activeFocus
                                        ? 1
                                        : 0
                                    border.color: root.activeEdgeColor
                                }

                                contentItem: Item {
                                    Image {
                                        id: applicationIcon

                                        objectName: "workspaceApplicationIcon-"
                                            + workspaceIndicator.workspaceId
                                            + "-" + applicationButton.index
                                        anchors.centerIn: parent
                                        width: root.applicationIconSize
                                        height: root.applicationIconSize
                                        source: String(
                                            applicationButton.modelData
                                                .iconSource || ""
                                        )
                                        visible: source.toString().length > 0
                                            && status === Image.Ready
                                        asynchronous: true
                                        fillMode: Image.PreserveAspectFit
                                        opacity:
                                            applicationButton.applicationActive
                                                ? 1
                                                : (workspaceIndicator
                                                        .workspaceActive
                                                    ? 0.82
                                                    : 0.65)
                                        Accessible.ignored: true
                                    }

                                    Text {
                                        objectName:
                                            "workspaceApplicationFallback-"
                                            + workspaceIndicator.workspaceId
                                            + "-" + applicationButton.index
                                        anchors.centerIn: parent
                                        visible: !applicationIcon.visible
                                        text: String(
                                            applicationButton.modelData
                                                .fallbackInitial || "?"
                                        ).charAt(0).toUpperCase()
                                        color:
                                            workspaceIndicator.workspaceActive
                                                ? root.strongTextColor
                                                : root.mutedTextColor
                                        opacity:
                                            applicationButton.applicationActive
                                                ? 1
                                                : (workspaceIndicator
                                                        .workspaceActive
                                                    ? 0.82
                                                    : 0.65)
                                        font.pixelSize: Math.max(
                                            8,
                                            root.applicationIconSize - 7
                                        )
                                        font.weight: Font.Bold
                                        Accessible.ignored: true
                                    }

                                    Rectangle {
                                        objectName:
                                            "workspaceApplicationActiveMarker-"
                                            + workspaceIndicator.workspaceId
                                            + "-" + applicationButton.index
                                        visible:
                                            applicationButton.applicationActive
                                        anchors {
                                            horizontalCenter:
                                                parent.horizontalCenter
                                            bottom: parent.bottom
                                        }
                                        width: 7
                                        height: 2
                                        radius: 1
                                        color: "#a8cbff"
                                        Accessible.ignored: true
                                    }

                                    Rectangle {
                                        objectName:
                                            "workspaceApplicationCount-"
                                            + workspaceIndicator.workspaceId
                                            + "-" + applicationButton.index
                                        visible:
                                            applicationButton.applicationCount
                                                > 1
                                        anchors {
                                            right: parent.right
                                            rightMargin: -2
                                            bottom: parent.bottom
                                            bottomMargin: -2
                                        }
                                        width: 10
                                        height: 10
                                        radius: width / 2
                                        color: "#17243a"
                                        border.color: "#a8cbff"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: applicationButton
                                                .applicationCount
                                            color: root.strongTextColor
                                            font.pixelSize: 7
                                            font.weight: Font.Bold
                                            Accessible.ignored: true
                                        }

                                        Accessible.ignored: true
                                    }
                                }
                            }
                        }

                        Item {
                            objectName: "workspaceApplicationOverflow-"
                                + workspaceIndicator.workspaceId
                            visible: workspaceIndicator.applicationOverflow > 0
                            width: overflowLabel.implicitWidth + 2
                            height: root.applicationButtonSize
                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("%1 more applications").arg(
                                workspaceIndicator.applicationOverflow
                            )
                            Accessible.ignored: !visible

                            Text {
                                id: overflowLabel

                                anchors.centerIn: parent
                                text: "+"
                                    + workspaceIndicator.applicationOverflow
                                color: workspaceIndicator.workspaceActive
                                    ? root.strongTextColor
                                    : root.mutedTextColor
                                opacity: 0.78
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                                Accessible.ignored: true
                            }
                        }
                    }

                    Rectangle {
                        objectName: workspaceIndicator.validWorkspace
                            ? "workspaceFocusRing-"
                                + workspaceIndicator.workspaceId
                            : ""
                        anchors.verticalCenter: parent.verticalCenter
                        x: (root.workspaceHitCellWidth - width) / 2
                        visible: workspaceButton.activeFocus
                        width: workspaceCircle.width + 4
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: 1
                        border.color: "#a8cbff"
                        z: 5
                        Accessible.ignored: true
                    }

                    Behavior on width {
                        enabled: root.animationsEnabled

                        NumberAnimation {
                            duration: root.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }

    MouseArea {
        objectName: "workspaceWheelArea"
        anchors.fill: parent
        z: 10
        enabled: root.interactive && root.available
        acceptedButtons: Qt.NoButton

        onWheel: wheel => {
            const pixelVertical = Math.abs(wheel.pixelDelta.y);
            const pixelHorizontal = Math.abs(wheel.pixelDelta.x);
            const angleVertical = Math.abs(wheel.angleDelta.y);
            const angleHorizontal = Math.abs(wheel.angleDelta.x);
            if (pixelHorizontal > pixelVertical
                    || (pixelVertical === 0
                        && angleHorizontal > angleVertical)) {
                wheel.accepted = false;
                return;
            }

            if (root.scrollMode === "disabled") {
                wheel.accepted = false;
                return;
            }

            const pixelBased = pixelVertical > 0;
            const delta = pixelBased
                ? wheel.pixelDelta.y
                : wheel.angleDelta.y;
            root.submitWheelDelta(delta, pixelBased);
            wheel.accepted = true;
        }
    }

    Text {
        id: unavailableLabel

        objectName: "workspaceUnavailableLabel"
        anchors.centerIn: parent
        visible: !root.available
        text: qsTr("Workspaces unavailable")
        color: root.mutedTextColor
        font.pixelSize: 11
        Accessible.role: Accessible.StaticText
        Accessible.name: root.outputName.length > 0
            ? qsTr("Workspaces unavailable on %1").arg(root.outputName)
            : text
        Accessible.ignored: !root.interactive
    }
}
