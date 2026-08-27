pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Page {
    id: root

    property var allOptions: []

    signal openCategoryRequested(string categoryId)
    signal openSurfaceRequested(string pageId)

    readonly property string reviewedHyprlandVersion: "0.56.2"
    readonly property int reviewedOptionCount: 353
    readonly property int loadedCatalogOptionCount: root.normalizedCatalogOptions().length
    readonly property real coverageFraction: Math.min(1, root.loadedCatalogOptionCount / root.reviewedOptionCount)
    readonly property bool coverageComplete: root.loadedCatalogOptionCount >= root.reviewedOptionCount
    readonly property int cardColumnCount: width >= 760 ? 2 : 1
    readonly property bool wideHero: width >= 650
    readonly property color coverageColor: {
        if (root.coverageComplete)
            return ShellTheme.success;
        if (root.loadedCatalogOptionCount > 0)
            return ShellTheme.warning;
        return root.palette.placeholderText;
    }
    readonly property string coverageSummary: {
        if (root.coverageComplete) {
            return qsTr("%1 of %2 reviewed options are available").arg(root.loadedCatalogOptionCount).arg(root.reviewedOptionCount);
        }
        if (root.loadedCatalogOptionCount > 0) {
            return qsTr("%1 of %2 reviewed options are connected").arg(root.loadedCatalogOptionCount).arg(root.reviewedOptionCount);
        }
        return qsTr("Waiting for the trusted option catalog · %1 options reviewed").arg(root.reviewedOptionCount);
    }

    readonly property var categories: [
        {
            id: "appearance",
            title: qsTr("Appearance & Motion"),
            description: qsTr("Borders, corners, blur, shadows, opacity, cursor behavior and compositor animation."),
            modules: ["animations", "cursor", "decoration"],
            moduleSummary: qsTr("Animation · Cursor · Decoration"),
            pageId: "appearance",
            pageLabel: qsTr("Appearance"),
            routeType: "category",
            graphic: "appearance",
            accent: "#9f8cff"
        },
        {
            id: "input",
            title: qsTr("Input & Gestures"),
            description: qsTr("Keyboard, pointer, touchpad, tablet and gesture behavior, including device-specific controls."),
            modules: ["gestures", "input"],
            moduleSummary: qsTr("Input · Gestures"),
            pageId: "input",
            pageLabel: qsTr("Input"),
            routeType: "category",
            graphic: "input",
            accent: "#5eb8ff"
        },
        {
            id: "devices",
            title: qsTr("Per-device Input"),
            description: qsTr("Create ordered keyboard, pointer, touchpad, touch, tablet and switch overrides for named devices."),
            modules: ["device", "devices"],
            moduleSummary: qsTr("Structured device profiles"),
            pageId: "input-devices",
            pageLabel: qsTr("Device Profiles"),
            routeType: "surface",
            graphic: "input",
            accent: "#58c7d9"
        },
        {
            id: "windows",
            title: qsTr("Windows & Layouts"),
            description: qsTr("Focus, snapping, window groups and the dwindle, master, scrolling and monocle layouts."),
            modules: ["dwindle", "general", "group", "layout", "master", "scrolling"],
            moduleSummary: qsTr("General · Layout engines · Groups"),
            pageId: "windows",
            pageLabel: qsTr("Windows"),
            routeType: "category",
            graphic: "windows",
            accent: "#7c91ff"
        },
        {
            id: "displays",
            title: qsTr("Displays"),
            description: qsTr("Arrange outputs and control mode, refresh rate, scale, transform, mirroring, VRR and HDR."),
            modules: ["display", "displays", "monitor", "monitors"],
            moduleSummary: qsTr("Structured output profiles"),
            pageId: "displays",
            pageLabel: qsTr("Displays"),
            routeType: "surface",
            graphic: "displays",
            accent: "#47c8c2"
        },
        {
            id: "workspaces",
            title: qsTr("Workspaces"),
            description: qsTr("Persistent workspaces, monitor placement, per-workspace layouts, gaps and creation actions."),
            modules: ["workspace", "workspaces"],
            moduleSummary: qsTr("Structured workspace rules"),
            pageId: "workspaces",
            pageLabel: qsTr("Workspaces"),
            routeType: "surface",
            graphic: "workspaces",
            accent: "#63cc8a"
        },
        {
            id: "rules",
            title: qsTr("Rules"),
            description: qsTr("Match windows and layers, then control placement, appearance, focus, capture and behavior."),
            modules: ["layer-rule", "layer-rules", "rule", "rules", "window-rule", "window-rules"],
            moduleSummary: qsTr("Window rules · Layer rules"),
            pageId: "rules",
            pageLabel: qsTr("Rules"),
            routeType: "surface",
            graphic: "rules",
            accent: "#f28ab2"
        },
        {
            id: "shortcuts",
            title: qsTr("Shortcuts & Submaps"),
            description: qsTr("Browse compositor actions, understand bind behavior and organize keyboard shortcut modes."),
            modules: ["bind", "binds", "submap", "submaps"],
            moduleSummary: qsTr("Binds · Action reference · Submaps"),
            pageId: "bindings",
            pageLabel: qsTr("Shortcuts & Submaps"),
            routeType: "category",
            graphic: "shortcuts",
            accent: "#e9b65f"
        },
        {
            id: "system",
            title: qsTr("System & Compatibility"),
            description: qsTr("Rendering, XWayland, OpenGL, hardware quirks, diagnostics and experimental behavior."),
            modules: ["debug", "experimental", "misc", "opengl", "quirks", "render", "xwayland"],
            moduleSummary: qsTr("Render · XWayland · Diagnostics"),
            pageId: "advanced",
            pageLabel: qsTr("Advanced"),
            routeType: "category",
            graphic: "system",
            accent: "#a8b4c8"
        },
        {
            id: "session",
            title: qsTr("Session & Security"),
            description: qsTr("Session-lock safeguards, capture permissions, startup policy and Hyprland ecosystem controls."),
            modules: ["ecosystem"],
            moduleSummary: qsTr("Session · Permissions · Ecosystem"),
            pageId: "advanced",
            pageLabel: qsTr("Advanced"),
            routeType: "category",
            graphic: "security",
            accent: "#ee8c72"
        },
        {
            id: "environment",
            title: qsTr("Environment"),
            description: qsTr("Manage ordered Hyprland-owned environment variables with exact Lua previews and explicit session activation."),
            modules: ["environment"],
            moduleSummary: qsTr("Structured environment variables"),
            pageId: "environment",
            pageLabel: qsTr("Environment"),
            routeType: "surface",
            graphic: "system",
            accent: "#69b7a8"
        },
        {
            id: "permissions",
            title: qsTr("Permissions"),
            description: qsTr("Review ordered screencopy, cursor, plugin, keyboard and input-capture policies by executable pattern."),
            modules: ["permission", "permissions"],
            moduleSummary: qsTr("Structured security policy"),
            pageId: "permissions",
            pageLabel: qsTr("Permissions"),
            routeType: "surface",
            graphic: "security",
            accent: "#ec7a8f"
        }
    ]

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

    function sourceCatalogOptions() {
        if (root.allOptions && typeof root.allOptions.length === "number") {
            return root.listValue(root.allOptions);
        }
        if (!root.allOptions || typeof root.allOptions !== "object")
            return [];
        if (root.allOptions.options && typeof root.allOptions.options.length === "number") {
            return root.listValue(root.allOptions.options);
        }

        const flattened = [];
        for (const key of Object.keys(root.allOptions)) {
            const group = root.allOptions[key];
            if (group && typeof group.length === "number")
                flattened.push(...root.listValue(group));
        }
        return flattened;
    }

    function normalizedCatalogOptions() {
        const source = root.sourceCatalogOptions();
        const result = [];
        const seen = Object.create(null);
        for (let index = 0; index < source.length; ++index) {
            const option = source[index];
            if (!option || typeof option !== "object")
                continue;
            const id = typeof option.id === "string" ? option.id : "";
            const module = root.moduleForOption(option);
            const identity = id.length > 0 ? "id:" + id : "anonymous:" + module + ":" + index;
            if (seen[identity])
                continue;
            seen[identity] = true;
            result.push(option);
        }
        return result;
    }

    function moduleForOption(option) {
        if (!option || typeof option !== "object")
            return "";
        if (typeof option.module === "string" && option.module.length > 0)
            return option.module.toLowerCase();
        if (typeof option.catalogModule === "string" && option.catalogModule.length > 0) {
            return option.catalogModule.toLowerCase();
        }
        if (typeof option.id === "string") {
            const idMatch = /^hyprland\.([^.]+)(?:\.|$)/.exec(option.id);
            if (idMatch)
                return idMatch[1].toLowerCase();
        }
        if (typeof option.path === "string") {
            const separator = option.path.indexOf(":");
            if (separator > 0)
                return option.path.slice(0, separator).toLowerCase();
        }
        return "";
    }

    function optionCountForModules(modules) {
        const moduleList = root.listValue(modules);
        if (moduleList.length === 0)
            return 0;
        const accepted = Object.create(null);
        for (const module of moduleList)
            accepted[String(module).toLowerCase()] = true;

        let count = 0;
        for (const option of root.normalizedCatalogOptions()) {
            if (accepted[root.moduleForOption(option)] === true)
                ++count;
        }
        return count;
    }

    function categoryById(categoryId) {
        for (const category of root.categories) {
            if (category.id === categoryId)
                return category;
        }
        return null;
    }

    function optionCountForCategory(categoryId) {
        const category = root.categoryById(categoryId);
        return category ? root.optionCountForModules(category.modules) : 0;
    }

    function activateCategory(categoryId) {
        const category = root.categoryById(categoryId);
        if (!category)
            return false;
        if (category.routeType === "surface")
            root.openSurfaceRequested(category.pageId);
        else
            root.openCategoryRequested(category.id);
        return true;
    }

    function optionCountLabel(count) {
        return count === 1 ? qsTr("1 catalog option") : qsTr("%1 catalog options").arg(count);
    }

    function translucent(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha);
    }

    background: Rectangle {
        color: root.palette.window
    }

    ScrollView {
        id: overviewScroll

        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: overviewScroll.availableWidth
            spacing: 18

            Rectangle {
                id: hero

                objectName: "hyprlandOverviewHero"
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 28
                implicitHeight: heroGrid.implicitHeight + 44
                radius: 20
                color: root.palette.base
                border.color: root.palette.mid

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: root.translucent(root.palette.highlight, 0.22)
                    }
                    GradientStop {
                        position: 0.58
                        color: root.translucent(root.palette.highlight, 0.07)
                    }
                    GradientStop {
                        position: 1
                        color: root.palette.base
                    }
                }

                GridLayout {
                    id: heroGrid

                    anchors {
                        fill: parent
                        margins: 22
                    }
                    columns: root.wideHero ? 2 : 1
                    columnSpacing: 26
                    rowSpacing: 18

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 7

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("HYPRLAND")
                            color: root.palette.highlight
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            font.letterSpacing: 1.8
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Shape the compositor around you")
                            color: root.palette.text
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Settings are organized by what they change, while the trusted catalog keeps every value tied to its generated Lua module.")
                            color: root.palette.placeholderText
                            font.pixelSize: 14
                            lineHeight: 1.22
                            wrapMode: Text.WordWrap
                        }
                    }

                    HyprlandCategoryGraphic {
                        Layout.preferredWidth: root.wideHero ? 176 : 150
                        Layout.preferredHeight: root.wideHero ? 124 : 104
                        Layout.alignment: root.wideHero ? Qt.AlignRight | Qt.AlignVCenter : Qt.AlignHCenter
                        kind: "windows"
                        accentColor: root.palette.highlight
                    }
                }
            }

            Rectangle {
                id: coverageCard

                objectName: "hyprlandCoverageStatusArea"
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: coverageLayout.implicitHeight + 40
                radius: 16
                color: root.palette.base
                border.color: root.palette.mid
                Accessible.role: Accessible.Note
                Accessible.name: qsTr("Hyprland current-wiki coverage")
                Accessible.description: root.coverageSummary

                ColumnLayout {
                    id: coverageLayout

                    anchors {
                        fill: parent
                        margins: 20
                    }
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 11
                            color: root.translucent(root.coverageColor, 0.16)
                            border.color: root.translucent(root.coverageColor, 0.58)

                            Label {
                                anchors.centerIn: parent
                                text: root.coverageComplete ? "✓" : "i"
                                color: root.coverageColor
                                font.pixelSize: 17
                                font.weight: Font.Bold
                                Accessible.ignored: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Hyprland %1 · current-wiki coverage").arg(root.reviewedHyprlandVersion)
                                color: root.palette.text
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }

                            Label {
                                id: coverageSummaryLabel

                                objectName: "hyprlandCoverageSummary"
                                Layout.fillWidth: true
                                text: root.coverageSummary
                                color: root.coverageColor
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                        }

                        Rectangle {
                            visible: root.wideHero
                            Layout.preferredWidth: versionLabel.implicitWidth + 18
                            Layout.preferredHeight: 28
                            radius: 14
                            color: root.translucent(root.palette.highlight, 0.14)
                            border.color: root.translucent(root.palette.highlight, 0.38)

                            Label {
                                id: versionLabel

                                anchors.centerIn: parent
                                text: qsTr("Reviewed v%1").arg(root.reviewedHyprlandVersion)
                                color: root.palette.highlight
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        radius: 3
                        color: root.palette.button

                        Rectangle {
                            width: parent.width * root.coverageFraction
                            height: parent.height
                            radius: parent.radius
                            color: root.coverageColor

                            Behavior on width {
                                NumberAnimation {
                                    duration: 180
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Counts below follow generated Lua module ownership. Display profiles, workspace and window rules, and shortcut actions use dedicated structured editors beyond the scalar option catalog.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        lineHeight: 1.18
                        wrapMode: Text.WordWrap
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Explore Hyprland")
                    color: root.palette.text
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Choose a destination to open its current Settings surface.")
                    color: root.palette.placeholderText
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            GridLayout {
                id: categoryGrid

                objectName: "hyprlandCategoryGrid"
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                columns: root.cardColumnCount
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: root.categories

                    AbstractButton {
                        id: categoryCard

                        required property var modelData
                        readonly property int optionCount: root.optionCountForModules(modelData.modules)
                        readonly property color categoryAccent: modelData.accent
                        readonly property string coverageLabel: modelData.routeType === "surface" && optionCount === 0 ? qsTr("Structured editor") : root.optionCountLabel(optionCount)

                        objectName: "hyprlandCategoryCard-" + modelData.id
                        Layout.fillWidth: true
                        Layout.preferredWidth: 340
                        Layout.minimumHeight: 230
                        Layout.preferredHeight: 230
                        focusPolicy: Qt.StrongFocus
                        hoverEnabled: true
                        leftPadding: 18
                        rightPadding: 18
                        topPadding: 17
                        bottomPadding: 15
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title
                        Accessible.description: coverageLabel + ". " + modelData.description

                        onClicked: root.activateCategory(modelData.id)

                        background: Rectangle {
                            radius: 18
                            color: categoryCard.hovered ? root.translucent(categoryCard.categoryAccent, 0.09) : root.palette.base
                            border.width: categoryCard.activeFocus ? 2 : 1
                            border.color: categoryCard.activeFocus
                                ? root.palette.highlight
                                : categoryCard.hovered
                                    ? root.translucent(
                                          categoryCard.categoryAccent, 0.62
                                      )
                                    : root.palette.mid

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    top: parent.top
                                    bottom: parent.bottom
                                }
                                width: categoryCard.hovered ? 4 : 2
                                radius: 2
                                color: categoryCard.categoryAccent
                                opacity: categoryCard.hovered ? 0.9 : 0.48

                                Behavior on width {
                                    NumberAnimation {
                                        duration: 120
                                    }
                                }
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                        }

                        contentItem: ColumnLayout {
                            spacing: 11

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 14

                                HyprlandCategoryGraphic {
                                    Layout.preferredWidth: 126
                                    Layout.preferredHeight: 88
                                    kind: categoryCard.modelData.graphic
                                    accentColor: categoryCard.categoryAccent
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 7

                                    Label {
                                        Layout.fillWidth: true
                                        text: categoryCard.modelData.title
                                        color: root.palette.text
                                        font.pixelSize: 17
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.WordWrap
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: countLabel.implicitWidth + 16
                                        Layout.preferredHeight: 25
                                        radius: 12
                                        color: root.translucent(categoryCard.categoryAccent, 0.14)
                                        border.color: root.translucent(categoryCard.categoryAccent, 0.35)

                                        Label {
                                            id: countLabel

                                            anchors.centerIn: parent
                                            text: categoryCard.coverageLabel
                                            color: root.palette.highlight
                                            font.pixelSize: 10
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: categoryCard.modelData.description
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                lineHeight: 1.17
                                wrapMode: Text.WordWrap
                                elide: Text.ElideRight
                                maximumLineCount: 3
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: categoryCard.modelData.moduleSummary
                                    color: root.palette.placeholderText
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: categoryCard.modelData.routeType === "surface" ? qsTr("Open %1").arg(categoryCard.modelData.pageLabel) : qsTr("Explore category")
                                    color: root.palette.highlight
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }

                                Item {
                                    Layout.preferredWidth: 12
                                    Layout.preferredHeight: 12

                                    Rectangle {
                                        x: 1
                                        y: 5
                                        width: 9
                                        height: 2
                                        radius: 1
                                        color: root.palette.highlight
                                    }

                                    Rectangle {
                                        x: 6
                                        y: 2
                                        width: 7
                                        height: 2
                                        radius: 1
                                        rotation: 45
                                        color: root.palette.highlight
                                    }

                                    Rectangle {
                                        x: 6
                                        y: 8
                                        width: 7
                                        height: 2
                                        radius: 1
                                        rotation: -45
                                        color: root.palette.highlight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
            }
        }
    }
}
