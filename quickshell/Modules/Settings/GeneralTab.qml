pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Widgets

// macOS-style "General" page: a list of sub-pages. Selecting one drills into a detail
// page with a back-bar (back button far left + centered title) and the page below it.
Item {
    id: generalTab

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    // "" = the list; otherwise the open sub-page id.
    property string sub: ""

    readonly property var items: [
        {
            "sid": "about",
            "label": I18n.tr("About"),
            "icon": "info"
        },
        {
            "sid": "update",
            "label": I18n.tr("Software Update"),
            "icon": "system_update"
        },
        {
            "sid": "language",
            "label": I18n.tr("Language & Region"),
            "icon": "language"
        },
        {
            "sid": "greeter",
            "label": I18n.tr("Greeter Login"),
            "icon": "login"
        },
        {
            "sid": "datetime",
            "label": I18n.tr("Date & Time"),
            "icon": "schedule"
        },
        {
            "sid": "storage",
            "label": I18n.tr("Storage"),
            "icon": "storage"
        }
    ]

    function labelFor(sid) {
        for (var i = 0; i < items.length; i++) {
            if (items[i].sid === sid)
                return items[i].label;
        }
        return "";
    }

    // Reset to the list whenever the General tab is (re)shown.
    onVisibleChanged: {
        if (!visible)
            sub = "";
    }

    // ===== LIST VIEW =====
    HGSFlickable {
        anchors.fill: parent
        visible: generalTab.sub === ""
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
                model: generalTab.items

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
                        onClicked: generalTab.sub = itemRow.modelData.sid
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
        visible: generalTab.sub !== ""

        // Back-bar: back button (far left) + centered category title.
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
                onClicked: generalTab.sub = ""
            }

            StyledText {
                anchors.centerIn: parent
                text: generalTab.labelFor(generalTab.sub)
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

        // Page content under the bar — each sub-page is a full settings tab.
        Item {
            anchors.top: backBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.spacingS
            clip: true

            Loader {
                anchors.fill: parent
                active: generalTab.sub === "about"
                visible: active
                sourceComponent: AboutTab {}
            }
            Loader {
                anchors.fill: parent
                active: generalTab.sub === "update"
                visible: active
                sourceComponent: SystemUpdaterTab {}
            }
            Loader {
                anchors.fill: parent
                active: generalTab.sub === "language"
                visible: active
                sourceComponent: LocaleTab {}
            }
            Loader {
                anchors.fill: parent
                active: generalTab.sub === "greeter"
                visible: active
                sourceComponent: GreeterTab {}
            }
            Loader {
                anchors.fill: parent
                active: generalTab.sub === "datetime"
                visible: active
                sourceComponent: DateTimePage {}
            }
            Loader {
                anchors.fill: parent
                active: generalTab.sub === "storage"
                visible: active
                sourceComponent: StoragePage {}
            }
        }
    }
}
