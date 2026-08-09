pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var review: ({})
    property string inspectionToken: ""
    property bool operationBusy: false
    property string errorText: ""

    readonly property string operation:
        review && typeof review.operation === "string"
            ? review.operation : "install"
    readonly property bool activationSupported:
        !review || review.activationSupported !== false
    readonly property var capabilities: root.listValue(
        review ? review.requestedCapabilities : null
    )
    readonly property var dependencies: root.listValue(
        review ? review.dependencies : null
    )

    signal installRequested()
    signal cancelRequested()

    function listValue(value) {
        if (Array.isArray(value))
            return value.slice();
        if (!value || typeof value.length !== "number")
            return [];
        const result = [];
        for (let index = 0; index < value.length; ++index)
            result.push(value[index]);
        return result;
    }

    function operationTitle() {
        switch (root.operation) {
        case "update": return qsTr("Review component update");
        case "reinstall": return qsTr("Review component reinstall");
        case "downgrade": return qsTr("Review component downgrade");
        default: return qsTr("Review third-party component");
        }
    }

    function actionLabel() {
        switch (root.operation) {
        case "update": return qsTr("Install update");
        case "reinstall": return qsTr("Reinstall");
        case "downgrade": return qsTr("Install older version");
        default: return qsTr("Install component");
        }
    }

    function authorText() {
        return root.listValue(
            root.review ? root.review.authors : null
        ).map(author => author && author.name
            ? author.name : "").filter(name => name.length > 0).join(", ");
    }

    objectName: "componentReviewDialog"
    title: root.operationTitle()
    modal: true
    width: Math.min(680, parent ? parent.width - 48 : 680)
    height: Math.min(720, parent ? parent.height - 48 : 720)
    closePolicy: Popup.NoAutoClose

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 14

            Frame {
                objectName: "componentUnverifiedWarning"
                Layout.fillWidth: true
                padding: 14

                background: Rectangle {
                    color: "#33251a"
                    radius: 10
                    border.color: "#8bf6ad55"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Unverified third-party code")
                        color: "#ffd5a1"
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("HyprShelld verified the package structure and integrity, not that its code is safe. Install only code you have reviewed and trust.")
                        color: "#ffd5a1"
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }
            }

            Label {
                objectName: "componentReviewName"
                Layout.fillWidth: true
                text: root.review && root.review.name
                    ? root.review.name : qsTr("Unnamed component")
                color: root.palette.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                objectName: "componentReviewDescription"
                Layout.fillWidth: true
                text: root.review && root.review.description
                    ? root.review.description : ""
                visible: text.length > 0
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 14
                rowSpacing: 7

                Label { text: qsTr("Component ID"); color: root.palette.placeholderText }
                Label {
                    objectName: "componentReviewId"
                    Layout.fillWidth: true
                    text: root.review && root.review.id ? root.review.id : ""
                    color: root.palette.text
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
                Label { text: qsTr("Version"); color: root.palette.placeholderText }
                Label {
                    objectName: "componentReviewVersion"
                    text: root.review && root.review.version
                        ? root.review.version : ""
                    color: root.palette.text
                    textFormat: Text.PlainText
                }
                Label { text: qsTr("Type"); color: root.palette.placeholderText }
                Label {
                    text: root.review && root.review.type ? root.review.type : ""
                    color: root.palette.text
                    textFormat: Text.PlainText
                }
                Label { text: qsTr("Author"); color: root.palette.placeholderText }
                Label {
                    Layout.fillWidth: true
                    text: root.authorText()
                    color: root.palette.text
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
                Label { text: qsTr("License"); color: root.palette.placeholderText }
                Label {
                    text: root.review && root.review.license
                        ? root.review.license : ""
                    color: root.palette.text
                    textFormat: Text.PlainText
                }
                Label { text: qsTr("Runtime"); color: root.palette.placeholderText }
                Label {
                    text: root.review && root.review.runtime
                            && root.review.runtime.kind
                        ? root.review.runtime.kind : ""
                    color: root.palette.text
                    textFormat: Text.PlainText
                }
                Label { text: qsTr("Package digest"); color: root.palette.placeholderText }
                Label {
                    objectName: "componentReviewDigest"
                    Layout.fillWidth: true
                    text: root.review && root.review.packageDigest
                        ? root.review.packageDigest : ""
                    color: root.palette.text
                    font.family: "monospace"
                    font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Requested capabilities")
                color: root.palette.text
                font.weight: Font.DemiBold
                textFormat: Text.PlainText
            }

            Label {
                objectName: "componentReviewCapabilities"
                Layout.fillWidth: true
                text: root.capabilities.length === 0
                    ? qsTr("None")
                    : root.capabilities.map(capability => {
                        if (!capability)
                            return "";
                        const id = capability.id || "";
                        return capability.reason
                            ? "%1 — %2".arg(id).arg(capability.reason) : id;
                    }).filter(value => value.length > 0).join("\n")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                visible: root.dependencies.length > 0
                text: qsTr("Dependencies")
                color: root.palette.text
                font.weight: Font.DemiBold
                textFormat: Text.PlainText
            }

            Label {
                objectName: "componentReviewDependencies"
                Layout.fillWidth: true
                visible: root.dependencies.length > 0
                text: root.dependencies.map(dependency => dependency
                    ? "%1 %2".arg(dependency.id || "")
                        .arg(dependency.version || "") : ""
                ).filter(value => value.trim().length > 0).join("\n")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Frame {
                objectName: "componentActivationNotice"
                Layout.fillWidth: true
                padding: 12

                background: Rectangle {
                    color: Qt.rgba(
                        root.palette.highlight.r,
                        root.palette.highlight.g,
                        root.palette.highlight.b,
                        0.09
                    )
                    radius: 10
                    border.color: root.palette.mid
                }

                Label {
                    objectName: "componentActivationNoticeText"
                    anchors.fill: parent
                    text: root.activationSupported
                        ? qsTr("Installation does not change saved enablement or placements. A new or changed version stays disabled; reinstalling the exact version you previously enabled can restore that saved state.")
                        : (root.review && root.review.compatibilityReason
                            ? qsTr("This shell cannot activate the package. Installation does not change saved state: %1").arg(root.review.compatibilityReason)
                            : qsTr("This shell cannot activate the package. Installation does not change saved enablement or placements."))
                    color: root.palette.placeholderText
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.name: text
                }
            }

            Label {
                objectName: "componentPackageError"
                Layout.fillWidth: true
                visible: root.errorText.length > 0
                text: root.errorText
                color: "#ffb8c3"
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            objectName: "cancelComponentInstallation"
            text: qsTr("Cancel")
            enabled: !root.operationBusy
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.cancelRequested()
        }

        Button {
            objectName: "confirmComponentInstallation"
            text: root.operationBusy ? qsTr("Installing…")
                : root.actionLabel()
            enabled: root.inspectionToken.length > 0
                && !root.operationBusy
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: root.installRequested()
        }
    }
}
