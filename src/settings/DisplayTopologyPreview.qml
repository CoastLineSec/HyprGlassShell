pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Control {
    id: root

    property var outputs: []
    property var topology: []
    property string selectedId: ""
    property bool interactive: true

    signal outputSelected(string id)
    signal positionRequested(string id, int x, int y)

    function topologyFor(selector) {
        if (!Array.isArray(root.topology))
            return null;
        for (const output of root.topology) {
            if (output && output.selector === selector)
                return output;
        }
        return null;
    }

    function explicitPosition(position) {
        if (typeof position !== "string")
            return null;
        const match = /^([+-]?[0-9]+)x([+-]?[0-9]+)$/.exec(position);
        if (!match)
            return null;
        return { x: Number(match[1]), y: Number(match[2]) };
    }

    function outputScale(output) {
        return output && typeof output.scale === "number"
            && isFinite(output.scale) && output.scale >= 0.25
            ? output.scale : 1;
    }

    function modeLabel(output, observed) {
        const mode = output && typeof output.mode === "string"
            ? output.mode : "preferred";
        const explicit = /^([1-9][0-9]{0,4})x([1-9][0-9]{0,4})(?:@([0-9.]+))?$/
            .exec(mode);
        if (explicit) {
            return explicit[3]
                ? qsTr("%1 × %2 at %3 Hz")
                    .arg(explicit[1]).arg(explicit[2]).arg(explicit[3])
                : qsTr("%1 × %2").arg(explicit[1]).arg(explicit[2]);
        }
        if (mode === "highrr")
            return qsTr("Highest refresh rate");
        if (mode === "highres")
            return qsTr("Highest resolution");
        if (mode === "maxwidth")
            return qsTr("Widest mode");
        return observed && observed.width && observed.height
            ? qsTr("Automatic · %1 × %2")
                .arg(observed.width).arg(observed.height)
            : qsTr("Automatic");
    }

    function buildLayout(outputs, topology, width, height) {
        const entries = [];
        let automaticX = 0;
        let minimumX = 0;
        let minimumY = 0;
        let maximumX = 1;
        let maximumY = 1;
        if (!Array.isArray(outputs))
            outputs = [];
        for (const output of outputs) {
            if (!output || typeof output !== "object")
                continue;
            const observed = root.topologyFor(output.selector) || {};
            let logicalWidth = Number(observed.width) || 1920;
            let logicalHeight = Number(observed.height) || 1080;
            const explicitMode = typeof output.mode === "string"
                ? /^([1-9][0-9]{0,4})x([1-9][0-9]{0,4})(?:@.+)?$/
                    .exec(output.mode)
                : null;
            if (explicitMode) {
                logicalWidth = Number(explicitMode[1]);
                logicalHeight = Number(explicitMode[2]);
            }
            const scale = root.outputScale(output);
            logicalWidth = Math.max(320, logicalWidth / scale);
            logicalHeight = Math.max(200, logicalHeight / scale);
            const transform = Number(output.transform) || 0;
            if (transform % 2 === 1) {
                const swap = logicalWidth;
                logicalWidth = logicalHeight;
                logicalHeight = swap;
            }
            const explicit = root.explicitPosition(output.position);
            const x = explicit ? explicit.x : automaticX;
            const y = explicit ? explicit.y : 0;
            automaticX = Math.max(automaticX, x + logicalWidth);
            entries.push({
                id: String(output.id || output.selector),
                selector: String(output.selector || ""),
                enabled: output.enabled === true,
                x: x,
                y: y,
                width: logicalWidth,
                height: logicalHeight,
                automatic: explicit === null,
                mirror: String(output.mirror || ""),
                mirrored: false,
                description: String(observed.description || ""),
                modeLabel: root.modeLabel(output, observed)
            });
        }
        const bySelector = Object.create(null);
        for (const entry of entries)
            bySelector[entry.selector] = entry;
        for (const entry of entries) {
            const mirrorTarget = entry.mirror
                ? bySelector[entry.mirror] : null;
            if (mirrorTarget) {
                entry.x = mirrorTarget.x;
                entry.y = mirrorTarget.y;
                entry.mirrored = true;
            }
            minimumX = Math.min(minimumX, entry.x);
            minimumY = Math.min(minimumY, entry.y);
            maximumX = Math.max(maximumX, entry.x + entry.width);
            maximumY = Math.max(maximumY, entry.y + entry.height);
        }
        const availableWidth = Math.max(1, width - 48);
        const availableHeight = Math.max(1, height - 64);
        const scale = Math.min(
            availableWidth / Math.max(1, maximumX - minimumX),
            availableHeight / Math.max(1, maximumY - minimumY)
        );
        return {
            entries: entries,
            minimumX: minimumX,
            minimumY: minimumY,
            scale: Math.max(0.02, Math.min(0.32, scale)),
            left: 24,
            top: 24
        };
    }

    readonly property var layout: root.buildLayout(
        root.outputs,
        root.topology,
        scene.width,
        scene.height
    )

    implicitHeight: 310
    padding: 0
    Accessible.ignored: true

    background: Rectangle {
        radius: 16
        color: root.palette.base
        border.color: root.palette.mid

        gradient: Gradient {
            GradientStop {
                position: 0
                color: Qt.rgba(
                    root.palette.highlight.r,
                    root.palette.highlight.g,
                    root.palette.highlight.b,
                    0.15
                )
            }
            GradientStop {
                position: 0.7
                color: root.palette.base
            }
        }
    }

    contentItem: Item {
        id: scene

        clip: true

        Repeater {
            model: root.layout.entries

            Rectangle {
                id: displayTile

                required property var modelData

                x: root.layout.left
                    + (modelData.x - root.layout.minimumX) * root.layout.scale
                    + (modelData.mirrored ? 12 : 0)
                y: root.layout.top
                    + (modelData.y - root.layout.minimumY) * root.layout.scale
                    + (modelData.mirrored ? 12 : 0)
                width: Math.max(92, modelData.width * root.layout.scale)
                height: Math.max(62, modelData.height * root.layout.scale)
                radius: 10
                color: modelData.enabled
                    ? Qt.rgba(
                        root.palette.highlight.r,
                        root.palette.highlight.g,
                        root.palette.highlight.b,
                        modelData.id === root.selectedId ? 0.32 : 0.18
                    )
                    : Qt.rgba(
                        root.palette.placeholderText.r,
                        root.palette.placeholderText.g,
                        root.palette.placeholderText.b,
                        0.08
                    )
                border.width: modelData.id === root.selectedId ? 3 : 1
                border.color: modelData.id === root.selectedId
                    ? root.palette.highlight : root.palette.placeholderText
                opacity: modelData.enabled ? 1 : 0.66
                z: modelData.mirrored ? 2 : 1

                Behavior on x {
                    enabled: !dragArea.drag.active
                    NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                }
                Behavior on y {
                    enabled: !dragArea.drag.active
                    NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                }

                Column {
                    anchors.centerIn: parent
                    width: parent.width - 16
                    spacing: 3

                    Label {
                        width: parent.width
                        text: displayTile.modelData.selector
                        color: root.palette.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    Label {
                        width: parent.width
                        text: !displayTile.modelData.enabled
                            ? qsTr("Disabled")
                            : displayTile.modelData.mirrored
                                ? qsTr("Mirrors %1")
                                    .arg(displayTile.modelData.mirror)
                                : displayTile.modelData.modeLabel
                        color: root.palette.placeholderText
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.bottom
                    }
                    width: Math.max(34, parent.width * 0.3)
                    height: 4
                    radius: 2
                    color: displayTile.border.color
                    opacity: 0.8
                }

                MouseArea {
                    id: dragArea

                    anchors.fill: parent
                    enabled: root.interactive && displayTile.modelData.enabled
                    cursorShape: enabled && !displayTile.modelData.mirrored
                        ? Qt.OpenHandCursor : Qt.PointingHandCursor
                    drag.target: displayTile.modelData.mirrored
                        ? null : displayTile
                    drag.axis: Drag.XAndYAxis
                    drag.minimumX: 8
                    drag.minimumY: 8
                    drag.maximumX: Math.max(8, scene.width - displayTile.width - 8)
                    drag.maximumY: Math.max(8, scene.height - displayTile.height - 14)

                    onPressed: {
                        if (!displayTile.modelData.mirrored)
                            cursorShape = Qt.ClosedHandCursor;
                        root.outputSelected(displayTile.modelData.id);
                    }
                    onReleased: {
                        if (displayTile.modelData.mirrored)
                            return;
                        cursorShape = Qt.OpenHandCursor;
                        const logicalX = Math.round(
                            (displayTile.x - root.layout.left)
                                / root.layout.scale + root.layout.minimumX
                        );
                        const logicalY = Math.round(
                            (displayTile.y - root.layout.top)
                                / root.layout.scale + root.layout.minimumY
                        );
                        root.positionRequested(
                            displayTile.modelData.id,
                            logicalX,
                            logicalY
                        );
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: root.layout.entries.length === 0
            text: qsTr("No connected displays were reported")
            color: root.palette.placeholderText
            font.pixelSize: 14
        }

        Label {
            anchors {
                left: parent.left
                bottom: parent.bottom
                margins: 12
            }
            visible: root.layout.entries.length > 0
            text: root.interactive
                ? qsTr("Drag non-mirrored displays to arrange them")
                : qsTr("Display arrangement")
            color: root.palette.placeholderText
            font.pixelSize: 11
        }
    }
}
