pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Services
import qs.Widgets

// macOS-style "Accessibility" page: a list of category sub-pages. Selecting one
// drills into a detail page with a back-bar (back button far left + centered
// title) and the page below it. Mirrors GeneralTab.
Item {
    id: a11yTab

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    // "" = the list; otherwise the open sub-page id.
    property string sub: ""

    readonly property var items: [
        {
            "sid": "vision",
            "label": I18n.tr("Vision"),
            "icon": "visibility"
        },
        {
            "sid": "hearing",
            "label": I18n.tr("Hearing"),
            "icon": "hearing"
        },
        {
            "sid": "typing",
            "label": I18n.tr("Typing"),
            "icon": "keyboard"
        },
        {
            "sid": "pointing",
            "label": I18n.tr("Pointing & Clicking"),
            "icon": "mouse"
        },
        {
            "sid": "speech",
            "label": I18n.tr("Speech"),
            "icon": "record_voice_over"
        }
    ]

    function labelFor(sid) {
        for (var i = 0; i < items.length; i++) {
            if (items[i].sid === sid)
                return items[i].label;
        }
        return "";
    }

    // Deep-link support: when another tab links here (e.g. Appearance → Font
    // Scale), it sets SettingsSearchService.targetTabSub; open that sub-page.
    function applyPendingSub() {
        if (SettingsSearchService.targetTabSub) {
            sub = SettingsSearchService.targetTabSub;
            SettingsSearchService.targetTabSub = "";
        }
    }

    Component.onCompleted: applyPendingSub()

    onVisibleChanged: {
        if (visible)
            applyPendingSub();
        else
            sub = "";
    }

    // ===== LIST VIEW =====
    HGSFlickable {
        anchors.fill: parent
        visible: a11yTab.sub === ""
        clip: true
        contentHeight: listColumn.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: listColumn

            topPadding: Theme.spacingM
            width: Math.min(600, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingXS

            Repeater {
                model: a11yTab.items

                delegate: Rectangle {
                    id: itemRow
                    required property var modelData

                    width: parent.width
                    height: 54
                    radius: Theme.cornerRadius
                    color: rowMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceContainerHigh

                    HGSIcon {
                        id: rowIcon
                        name: itemRow.modelData.icon
                        size: Theme.iconSize
                        color: Theme.surfaceText
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacingM
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    StyledText {
                        text: itemRow.modelData.label
                        color: Theme.surfaceText
                        font.pixelSize: Theme.fontSizeMedium
                        font.weight: Font.Medium
                        anchors.left: rowIcon.right
                        anchors.leftMargin: Theme.spacingM
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    HGSIcon {
                        name: "chevron_right"
                        size: Theme.iconSize - 2
                        color: Theme.surfaceVariantText
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacingM
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: a11yTab.sub = itemRow.modelData.sid
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.shortDuration
                        }
                    }
                }
            }
        }
    }

    // ===== SUB-PAGE VIEW =====
    Item {
        anchors.fill: parent
        visible: a11yTab.sub !== ""

        Item {
            id: backBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44

            HGSActionButton {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingM
                anchors.verticalCenter: parent.verticalCenter
                circular: false
                iconName: "arrow_back"
                iconColor: Theme.surfaceText
                onClicked: a11yTab.sub = ""
            }

            StyledText {
                anchors.centerIn: parent
                text: a11yTab.labelFor(a11yTab.sub)
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Medium
                color: Theme.surfaceText
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
                opacity: 0.12
            }
        }

        Item {
            anchors.top: backBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.spacingS
            clip: true

            Loader {
                anchors.fill: parent
                active: a11yTab.sub === "vision"
                visible: active
                sourceComponent: AccessibilityVisionPage {}
            }
            Loader {
                anchors.fill: parent
                active: a11yTab.sub === "hearing"
                visible: active
                sourceComponent: AccessibilityHearingPage {}
            }
            Loader {
                anchors.fill: parent
                active: a11yTab.sub === "typing"
                visible: active
                sourceComponent: AccessibilityTypingPage {}
            }
            Loader {
                anchors.fill: parent
                active: a11yTab.sub === "pointing"
                visible: active
                sourceComponent: AccessibilityPointingPage {}
            }
            Loader {
                anchors.fill: parent
                active: a11yTab.sub === "speech"
                visible: active
                sourceComponent: AccessibilitySpeechPage {}
            }
        }
    }
}
