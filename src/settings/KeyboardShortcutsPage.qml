pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI
import HyprShelld.Client

Page {
    id: root

    property bool referenceAvailable:
        KeyboardShortcutReferenceModel.available
    property string referenceErrorMessage:
        KeyboardShortcutReferenceModel.errorMessage
    property string referenceSourceDigest:
        KeyboardShortcutReferenceModel.sourceDigest
    property string referenceArtifactDigest:
        KeyboardShortcutReferenceModel.artifactDigest
    property int referenceRowCount:
        KeyboardShortcutReferenceModel.rowCount
    property var sourceRows: KeyboardShortcutReferenceModel.rows
    property string searchQuery: ""
    property real contentTopMargin: 28

    readonly property real horizontalPageMargin: width < 520 ? 12 : 28
    readonly property var filteredRows:
        filteredShortcutRows(sourceRows, searchQuery)
    readonly property int visibleRowCount: filteredRows.length

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

    function optionValue(row, name) {
        if (!row || typeof row !== "object")
            return undefined;
        const options = row.options;
        if (!options || typeof options !== "object"
                || Array.isArray(options)) {
            return undefined;
        }
        return Object.prototype.hasOwnProperty.call(options, name)
            ? options[name] : undefined;
    }

    function escapedLegacyString(value) {
        return JSON.stringify(String(value));
    }

    function optionsText(row) {
        const entries = [];
        const locked = optionValue(row, "locked");
        const repeating = optionValue(row, "repeating");
        const mouse = optionValue(row, "mouse");
        const description = optionValue(row, "description");
        if (locked !== undefined)
            entries.push("locked = " + String(locked));
        if (repeating !== undefined)
            entries.push("repeating = " + String(repeating));
        if (mouse !== undefined)
            entries.push("mouse = " + String(mouse));
        if (description !== undefined) {
            entries.push(
                "description = " + escapedLegacyString(description)
            );
        }
        return entries.length > 0
            ? "{ " + entries.join(", ") + " }" : "{}";
    }

    function searchableText(row) {
        if (!row || typeof row !== "object")
            return "";
        return [
            row.ordinal,
            row.sourceLine,
            row.section,
            row.chord,
            row.action,
            optionsText(row),
            row.sourceText
        ].join("\n").toLocaleLowerCase();
    }

    function filteredShortcutRows(rows, query) {
        const values = listValue(rows);
        const needle = String(query || "").trim().toLocaleLowerCase();
        if (needle.length === 0)
            return values;
        return values.filter(row => searchableText(row).includes(needle));
    }

    background: Rectangle {
        color: "transparent"
    }

    ColumnLayout {
        anchors {
            fill: parent
            leftMargin: root.horizontalPageMargin
            rightMargin: root.horizontalPageMargin
            topMargin: root.contentTopMargin
            bottomMargin: 20
        }
        spacing: 12

        Label {
            objectName: "keyboardShortcutsTitle"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: qsTr("Keyboard Shortcuts")
            textFormat: Text.PlainText
            color: root.palette.text
            font.pixelSize: root.width < 520 ? 24 : 30
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Heading
        }

        Rectangle {
            objectName: "legacyShortcutNotice"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            implicitHeight: legacyNoticeContent.implicitHeight + 24
            radius: 12
            color: ShellTheme.warningContainer
            border.width: 1
            border.color: ShellTheme.warningOutline
            Accessible.role: Accessible.StaticText
            Accessible.name: legacyNoticeTitle.text
                + ". " + legacyNoticeBody.text

            ColumnLayout {
                id: legacyNoticeContent

                anchors {
                    fill: parent
                    margins: 12
                }
                spacing: 5

                Label {
                    id: legacyNoticeTitle

                    objectName: "legacyShortcutNoticeTitle"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("Legacy reference — not current keybindings")
                    textFormat: Text.PlainText
                    color: ShellTheme.onWarningContainer
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Accessible.ignored: true
                }

                Label {
                    id: legacyNoticeBody

                    objectName: "legacyShortcutNoticeBody"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("This immutable page reproduces the pinned old keybinds.lua in source order. It does not read your active Hyprland configuration and cannot edit, activate, repair, or replace any shortcut.")
                    textFormat: Text.PlainText
                    color: ShellTheme.onWarningContainer
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Accessible.ignored: true
                }
            }
        }

        Label {
            objectName: "keyboardShortcutReferenceError"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: !root.referenceAvailable
            text: root.referenceErrorMessage.length > 0
                ? qsTr("The pinned legacy shortcut reference could not be verified: %1")
                    .arg(root.referenceErrorMessage)
                : qsTr("The pinned legacy shortcut reference is unavailable.")
            textFormat: Text.PlainText
            color: ShellTheme.onErrorContainer
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
        }

        TextField {
            id: shortcutSearch

            objectName: "keyboardShortcutSearch"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 44
            visible: root.referenceAvailable
            text: root.searchQuery
            placeholderText: qsTr("Search legacy chords, actions, sections, or options")
            selectByMouse: true
            leftPadding: 14
            rightPadding: 14
            Accessible.name: qsTr("Search the legacy shortcut reference")
            Accessible.description:
                qsTr("Filters this read-only reference without changing any shortcut.")

            onTextEdited: root.searchQuery = text
        }

        Label {
            objectName: "keyboardShortcutResultCount"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: root.referenceAvailable
            text: qsTr("%1 of %2 legacy rows")
                .arg(root.visibleRowCount)
                .arg(root.referenceRowCount)
            textFormat: Text.PlainText
            color: root.palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
        }

        ListView {
            id: shortcutList

            objectName: "keyboardShortcutList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            visible: root.referenceAvailable
            clip: true
            spacing: 10
            model: root.filteredRows
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Pinned legacy keyboard shortcuts")
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: shortcutCard

                required property var modelData

                readonly property var shortcut: modelData
                readonly property int shortcutOrdinal:
                    Number(shortcut.ordinal)
                readonly property string shortcutOptionsText:
                    root.optionsText(shortcut)

                objectName: "keyboardShortcutRow-" + shortcutOrdinal
                width: ListView.view.width
                implicitHeight: shortcutContent.implicitHeight + 24
                radius: 12
                color: ShellTheme.card
                border.width: 1
                border.color: ShellTheme.outline
                Accessible.role: Accessible.ListItem
                Accessible.name: qsTr("Legacy shortcut %1: %2. Action: %3. Options: %4")
                    .arg(shortcutOrdinal)
                    .arg(String(shortcut.chord))
                    .arg(String(shortcut.action))
                    .arg(shortcutOptionsText)
                    + qsTr(". Reference only; not editable or active.")

                ColumnLayout {
                    id: shortcutContent

                    anchors {
                        fill: parent
                        margins: 12
                    }
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 8

                        Label {
                            objectName: "keyboardShortcutOrdinal"
                            text: qsTr("#%1 · line %2")
                                .arg(shortcutCard.shortcutOrdinal)
                                .arg(String(shortcutCard.shortcut.sourceLine))
                            textFormat: Text.PlainText
                            color: root.palette.highlight
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            Accessible.ignored: true
                        }

                        Label {
                            objectName: "keyboardShortcutSection"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            horizontalAlignment: Text.AlignRight
                            text: String(shortcutCard.shortcut.section)
                            textFormat: Text.PlainText
                            color: root.palette.placeholderText
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Accessible.ignored: true
                        }
                    }

                    Label {
                        objectName: "keyboardShortcutChord"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: String(shortcutCard.shortcut.chord)
                        textFormat: Text.PlainText
                        color: root.palette.text
                        font.family: "monospace"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.WrapAnywhere
                        Accessible.ignored: true
                    }

                    Label {
                        objectName: "keyboardShortcutAction"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Action: %1")
                            .arg(String(shortcutCard.shortcut.action))
                        textFormat: Text.PlainText
                        color: ShellTheme.onSurface
                        font.family: "monospace"
                        font.pixelSize: 12
                        wrapMode: Text.WrapAnywhere
                        Accessible.ignored: true
                    }

                    Label {
                        objectName: "keyboardShortcutOptions"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Options: %1")
                            .arg(shortcutCard.shortcutOptionsText)
                        textFormat: Text.PlainText
                        color: ShellTheme.onSurfaceMuted
                        font.family: "monospace"
                        font.pixelSize: 12
                        wrapMode: Text.WrapAnywhere
                        Accessible.ignored: true
                    }

                    Label {
                        objectName: "keyboardShortcutReferenceState"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Reference only — not editable or active")
                        textFormat: Text.PlainText
                        color: ShellTheme.onWarningContainer
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        Accessible.ignored: true
                    }
                }
            }

            Label {
                objectName: "keyboardShortcutEmptyState"
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                visible: shortcutList.count === 0
                text: qsTr("No legacy shortcut rows match this search.")
                textFormat: Text.PlainText
                color: root.palette.placeholderText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                Accessible.role: Accessible.StaticText
            }
        }
    }
}
