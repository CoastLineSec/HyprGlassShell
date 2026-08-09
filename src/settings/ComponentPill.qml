pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var component
    required property bool desiredEnabled
    required property bool desiredStateAvailable
    required property bool toggleEnabled
    property bool pending: false
    property string statusText: ""
    property string errorText: ""

    readonly property string componentId:
        component && typeof component.id === "string" ? component.id : ""
    readonly property bool builtIn:
        component && component.origin === "system"

    signal componentEnabledRequested(
        string componentId,
        string packageDigest,
        bool enabled
    )

    function authorText(authors) {
        if (!Array.isArray(authors))
            return "";
        return authors.map(author => author && author.name
            ? author.name : "").filter(name => name.length > 0).join(", ");
    }

    objectName: "componentPill-" + root.componentId
    padding: 18

    background: Rectangle {
        color: root.palette.base
        radius: 16
        border.color: root.palette.mid
    }

    contentItem: RowLayout {
        spacing: 18

        Rectangle {
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            radius: 13
            color: Qt.rgba(
                root.palette.highlight.r,
                root.palette.highlight.g,
                root.palette.highlight.b,
                0.16
            )

            Item {
                anchors.centerIn: parent
                width: 23
                height: 23

                Repeater {
                    model: 4

                    Rectangle {
                        required property int index

                        width: 9
                        height: 9
                        x: (index % 2) * 13
                        y: Math.floor(index / 2) * 13
                        radius: 3
                        color: index === 0
                            ? root.palette.highlight
                            : root.palette.text
                        opacity: index === 0 ? 1 : 0.72
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: root.component && root.component.name
                        ? root.component.name : qsTr("Unnamed component")
                    color: root.palette.text
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.preferredWidth: originLabel.implicitWidth + 18
                    Layout.preferredHeight: 24
                    radius: 12
                    color: root.builtIn
                        ? Qt.rgba(
                            root.palette.highlight.r,
                            root.palette.highlight.g,
                            root.palette.highlight.b,
                            0.15
                        )
                        : "#33251a"
                    border.color: root.builtIn
                        ? Qt.rgba(
                            root.palette.highlight.r,
                            root.palette.highlight.g,
                            root.palette.highlight.b,
                            0.34
                        )
                        : "#8bf6ad55"

                    Label {
                        id: originLabel

                        anchors.centerIn: parent
                        objectName: "componentOrigin-" + root.componentId
                        text: root.builtIn
                            ? qsTr("Built-in") : qsTr("Third-party")
                        color: root.builtIn
                            ? root.palette.text : "#ffd5a1"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.component && root.component.description
                    ? root.component.description : ""
                color: root.palette.placeholderText
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: {
                    const author = root.authorText(
                        root.component ? root.component.authors : []
                    );
                    const version = root.component && root.component.version
                        ? root.component.version : "";
                    if (author && version)
                        return qsTr("By %1 · Version %2").arg(author).arg(version);
                    if (author)
                        return qsTr("By %1").arg(author);
                    return version ? qsTr("Version %1").arg(version) : "";
                }
                visible: text.length > 0
                color: root.palette.placeholderText
                opacity: 0.78
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                objectName: "componentStatus-" + root.componentId
                text: root.errorText.length > 0
                    ? root.errorText : root.statusText
                visible: text.length > 0
                color: root.errorText.length > 0
                    ? "#ffb8c3" : root.palette.placeholderText
                font.pixelSize: 11
                wrapMode: Text.Wrap
                Accessible.role: root.errorText.length > 0
                    ? Accessible.AlertMessage : Accessible.StaticText
                Accessible.name: text
            }
        }

        Switch {
            id: enableControl

            objectName: "componentEnabled-" + root.componentId
            enabled: root.toggleEnabled
            text: root.pending ? qsTr("Saving…") : ""
            display: AbstractButton.TextBesideIcon
            Accessible.name: root.component && root.component.name
                ? qsTr("Enable %1").arg(root.component.name)
                : qsTr("Enable component")

            onToggled: {
                if (!root.toggleEnabled
                        || checked === root.desiredEnabled) {
                    return;
                }
                root.componentEnabledRequested(
                    root.componentId,
                    root.component.packageDigest,
                    checked
                );
            }

            Binding {
                target: enableControl
                property: "checked"
                value: {
                    const operationError = root.errorText;
                    return root.desiredStateAvailable
                        && root.desiredEnabled;
                }
                when: !root.pending
                restoreMode: Binding.RestoreNone
            }
        }
    }
}
