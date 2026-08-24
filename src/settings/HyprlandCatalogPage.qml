pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property string categoryId: "appearance"
    property bool serviceAvailable: false
    property bool writable: false
    property bool allOptionsAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property string revisionToken: "0"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string errorName: ""
    property string errorMessage: ""
    property var allOptions: []
    property var allValues: ({})
    property real contentTopMargin: 28

    property var synchronizedValues: ({})
    property var draftValues: ({})
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property string selectedModule: ""
    property string searchText: ""
    property string tierFilter: "all"

    signal backRequested
    signal saveRequested(var values)
    signal refreshRequested
    signal openSurfaceRequested(string pageId)

    readonly property bool revisionTokenValid: /^(0|[1-9][0-9]*)$/.test(revisionToken)
    readonly property var modules: root.modulesForCategory(categoryId)
    readonly property var categoryOptions: root.optionsForCategory()
    readonly property var filteredOptions: root.filterOptions()
    readonly property bool draftDirty: projectionInitialized && !root.valuesEqual(draftValues, synchronizedValues)
    readonly property bool controlsEnabled: serviceAvailable && writable && allOptionsAvailable && revisionTokenValid && managementState === "managed" && !busy && !externalChangeWhileEditing
    readonly property var savePatch: root.buildSavePatch()
    readonly property int savePatchCount: Object.keys(savePatch).length
    readonly property bool saveEnabled: controlsEnabled && draftDirty && savePatchCount > 0
    readonly property string guideTarget: categoryId === "appearance" ? "appearance" : categoryId === "input" ? "input" : categoryId === "windows" ? "windows" : categoryId === "shortcuts" ? "bindings" : categoryId === "system" ? "advanced" : categoryId === "session" ? "environment" : ""

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

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function canonical(value) {
        try {
            return JSON.stringify(value);
        } catch (error) {
            return "";
        }
    }

    function valuesEqual(left, right) {
        return root.canonical(left) === root.canonical(right);
    }

    function modulesForCategory(category) {
        const map = {
            appearance: ["animations", "decoration", "cursor"],
            input: ["input", "gestures"],
            windows: ["general", "layout", "dwindle", "master", "scrolling", "group"],
            shortcuts: ["binds"],
            system: ["misc", "render", "xwayland", "opengl", "quirks", "debug", "experimental"],
            session: ["ecosystem"]
        };
        return map[category] ? map[category].slice() : [];
    }

    function categoryTitle(category) {
        const titles = {
            appearance: qsTr("Appearance & Motion"),
            input: qsTr("Input & Gestures"),
            windows: qsTr("Windows & Layouts"),
            shortcuts: qsTr("Shortcuts & Submaps"),
            system: qsTr("System & Compatibility"),
            session: qsTr("Session & Security")
        };
        return titles[category] || qsTr("Hyprland Options");
    }

    function categoryDescription(category) {
        const descriptions = {
            appearance: qsTr("Window decoration, cursor behavior, opacity, blur, shadows, glow, borders, and global animation behavior."),
            input: qsTr("Keyboard, pointer, touchpad, touch, tablet, virtual keyboard, and gesture defaults."),
            windows: qsTr("Tiling engines, groups, gaps, focus, snapping, placement, resizing, and window behavior."),
            shortcuts: qsTr("Global binding behavior. Active shortcuts and submaps use the dedicated editor."),
            system: qsTr("Rendering, color management, XWayland, OpenGL, compositor diagnostics, quirks, and experimental switches."),
            session: qsTr("Permission enforcement and ecosystem notices. Environment variables and ordered permissions use dedicated editors.")
        };
        return descriptions[category] || "";
    }

    function humanize(value) {
        const text = String(value || "").replace(/[-_.:]+/g, " ").replace(/\s+/g, " ").trim();
        return text.length > 0 ? text.charAt(0).toUpperCase() + text.slice(1) : "";
    }

    function optionsForCategory() {
        const allowed = new Set(root.modules);
        return root.listValue(root.allOptions).filter(option => option && typeof option === "object" && allowed.has(String(option.module || ""))).sort((left, right) => {
            const moduleDelta = root.modules.indexOf(left.module) - root.modules.indexOf(right.module);
            if (moduleDelta !== 0)
                return moduleDelta;
            return String(left.path || left.id).localeCompare(String(right.path || right.id));
        });
    }

    function filterOptions() {
        const query = root.searchText.trim().toLowerCase();
        return root.categoryOptions.filter(option => {
            if (root.selectedModule.length > 0 && option.module !== root.selectedModule) {
                return false;
            }
            if (root.tierFilter !== "all" && option.uiTier !== root.tierFilter) {
                return false;
            }
            if (query.length === 0)
                return true;
            return String(option.id || "").toLowerCase().includes(query) || String(option.path || "").toLowerCase().includes(query) || String(option.description || "").toLowerCase().includes(query);
        });
    }

    function relevantValues(source) {
        if (!source || typeof source !== "object" || Array.isArray(source))
            return null;
        const values = {};
        for (const option of root.listValue(root.categoryOptions)) {
            if (!option || typeof option.id !== "string" || !Object.prototype.hasOwnProperty.call(source, option.id)) {
                return null;
            }
            values[option.id] = root.clone(source[option.id]);
        }
        return values;
    }

    function synchronizeProjection(force) {
        const incoming = root.relevantValues(root.allValues);
        if (incoming === null)
            return;
        if (!root.projectionInitialized || force) {
            root.synchronizedValues = root.clone(incoming);
            root.draftValues = root.clone(incoming);
            root.projectionInitialized = true;
            root.externalChangeWhileEditing = false;
            return;
        }
        if (root.valuesEqual(incoming, root.synchronizedValues))
            return;
        if (root.draftDirty) {
            root.externalChangeWhileEditing = true;
            return;
        }
        root.synchronizedValues = root.clone(incoming);
        root.draftValues = root.clone(incoming);
        root.externalChangeWhileEditing = false;
    }

    function editValue(optionId, value) {
        if (!root.controlsEnabled || root.externalChangeWhileEditing)
            return;
        const next = root.clone(root.draftValues);
        if (!next || !Object.prototype.hasOwnProperty.call(next, optionId))
            return;
        next[optionId] = root.clone(value);
        root.draftValues = next;
    }

    function buildSavePatch() {
        const patch = {};
        if (!root.projectionInitialized)
            return patch;
        for (const option of root.listValue(root.categoryOptions)) {
            if (!option || typeof option.id !== "string" || option.writable === false)
                continue;
            const id = option.id;
            if (root.canonical(root.draftValues[id]) !== root.canonical(root.synchronizedValues[id])) {
                patch[id] = root.clone(root.draftValues[id]);
            }
        }
        return patch;
    }

    function moduleCount(moduleId) {
        return root.categoryOptions.filter(option => option.module === moduleId).length;
    }

    function statusText() {
        if (!root.serviceAvailable)
            return qsTr("The compositor Settings service is unavailable.");
        if (root.managementState === "unmanaged") {
            return qsTr("HyprShelld is not managing the Hyprland Lua entrypoint yet. Review takeover from Displays before editing.");
        }
        if (root.managementState === "conflict") {
            return qsTr("The managed entrypoint changed unexpectedly. Values remain visible, but saving is locked.");
        }
        if (!root.writable)
            return qsTr("The current desired state is read-only.");
        if (!root.allOptionsAvailable) {
            return root.errorMessage.length > 0 ? root.errorMessage : qsTr("The complete Hyprland option catalog is unavailable.");
        }
        if (!root.revisionTokenValid) {
            return qsTr("The exact desired-state revision is unavailable, so edits are locked.");
        }
        if (root.externalChangeWhileEditing) {
            return qsTr("Hyprland settings changed outside this draft. Your edits are preserved; load the current revision before saving.");
        }
        if (root.busy)
            return qsTr("Saving and verifying the desired Hyprland Lua state…");
        if (root.applyState !== "current") {
            return root.requiredActivation === "restart" ? qsTr("The saved state needs a compositor restart to activate.") : root.requiredActivation === "session" ? qsTr("The saved state activates in the next session.") : qsTr("The saved state is waiting for a verified reload.");
        }
        return "";
    }

    onAllValuesChanged: root.synchronizeProjection(false)
    onAllOptionsChanged: root.synchronizeProjection(false)
    onCategoryIdChanged: {
        root.selectedModule = "";
        root.searchText = "";
        root.projectionInitialized = false;
        root.synchronizeProjection(true);
    }
    Component.onCompleted: root.synchronizeProjection(true)

    background: Rectangle {
        color: "transparent"
    }

    ColumnLayout {
        anchors {
            fill: parent
            topMargin: root.contentTopMargin
            leftMargin: 28
            rightMargin: 28
            bottomMargin: 20
        }
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ToolButton {
                objectName: "hyprlandCatalogBackButton"
                text: "‹"
                font.pixelSize: 28
                implicitWidth: 44
                implicitHeight: 44
                Accessible.name: qsTr("Back to Hyprland categories")
                onClicked: root.backRequested()
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.categoryTitle(root.categoryId)
                    color: root.palette.text
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: root.categoryDescription(root.categoryId)
                    color: root.palette.placeholderText
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            HyprlandCategoryGraphic {
                Layout.preferredWidth: 150
                Layout.preferredHeight: 78
                visible: root.width >= 760
                kind: root.categoryId
                accentColor: root.palette.highlight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: statusRow.implicitHeight + 18
            visible: root.statusText().length > 0
            radius: 12
            color: root.externalChangeWhileEditing || root.managementState === "conflict" ? "#4c232c" : "#40351f"
            border.width: 1
            border.color: root.externalChangeWhileEditing || root.managementState === "conflict" ? "#b55268" : "#9c7934"

            RowLayout {
                id: statusRow
                anchors {
                    fill: parent
                    margins: 9
                }
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: root.statusText()
                    color: root.externalChangeWhileEditing || root.managementState === "conflict" ? "#ffb8c3" : "#ffe0a6"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }

                Button {
                    visible: root.externalChangeWhileEditing
                    text: qsTr("Load Current")
                    onClicked: root.synchronizeProjection(true)
                }

                Button {
                    visible: !root.busy && !root.externalChangeWhileEditing && (!root.allOptionsAvailable || root.applyState !== "current")
                    text: qsTr("Refresh")
                    onClicked: root.refreshRequested()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                contentHeight: moduleRow.implicitHeight
                contentWidth: moduleRow.implicitWidth
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                clip: true

                RowLayout {
                    id: moduleRow
                    spacing: 8

                    Button {
                        text: qsTr("All  %1").arg(root.categoryOptions.length)
                        checkable: true
                        checked: root.selectedModule.length === 0
                        onClicked: root.selectedModule = ""
                    }

                    Repeater {
                        model: root.modules

                        delegate: Button {
                            id: moduleButton
                            required property string modelData
                            text: "%1  %2".arg(root.humanize(modelData)).arg(root.moduleCount(modelData))
                            checkable: true
                            checked: root.selectedModule === modelData
                            onClicked: root.selectedModule = modelData
                        }
                    }
                }
            }

            Button {
                visible: root.guideTarget.length > 0
                text: qsTr("Open Guided Page")
                enabled: root.guideTarget !== "bindings" || root.allOptionsAvailable
                onClicked: root.openSurfaceRequested(root.guideTarget)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                objectName: "hyprlandCatalogSearch"
                Layout.fillWidth: true
                implicitHeight: 44
                text: root.searchText
                placeholderText: qsTr("Search this category by name, path, or description")
                selectByMouse: true
                Accessible.name: qsTr("Search Hyprland options")
                onTextEdited: root.searchText = text
            }

            ComboBox {
                objectName: "hyprlandCatalogTierFilter"
                Layout.preferredWidth: 154
                implicitHeight: 44
                model: [qsTr("All levels"), qsTr("Common"), qsTr("Advanced"), qsTr("Expert"), qsTr("External")]
                currentIndex: ["all", "common", "advanced", "expert", "external"].indexOf(root.tierFilter)
                Accessible.name: qsTr("Hyprland option level")
                onActivated: index => root.tierFilter = ["all", "common", "advanced", "expert", "external"][index]
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: parent.width
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    visible: root.filteredOptions.length === 0
                    text: root.allOptionsAvailable ? qsTr("No options match this filter.") : qsTr("Waiting for the verified Hyprland catalog…")
                    color: root.palette.placeholderText
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    topPadding: 28
                    bottomPadding: 28
                }

                Repeater {
                    model: root.filteredOptions

                    delegate: HyprlandOptionRow {
                        id: optionRow
                        required property var modelData

                        Layout.fillWidth: true
                        definition: modelData
                        settingValue: root.draftValues[modelData.id]
                        controlsEnabled: root.controlsEnabled
                        onValueEdited: value => root.editValue(modelData.id, value)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: saveRow.implicitHeight + 18
            radius: 14
            color: Qt.rgba(root.palette.base.r, root.palette.base.g, root.palette.base.b, 0.92)
            border.width: 1
            border.color: Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.10)

            RowLayout {
                id: saveRow
                anchors {
                    fill: parent
                    margins: 9
                }
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: root.draftDirty ? qsTr("%1 changed option(s) in this category").arg(root.savePatchCount) : qsTr("This category matches the saved desired state")
                    color: root.draftDirty ? root.palette.text : root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Button {
                    objectName: "hyprlandCatalogDiscardButton"
                    text: qsTr("Discard")
                    visible: root.draftDirty
                    enabled: !root.busy
                    onClicked: {
                        root.draftValues = root.clone(root.synchronizedValues);
                        root.externalChangeWhileEditing = false;
                    }
                }

                Button {
                    objectName: "hyprlandCatalogSaveButton"
                    text: root.busy && root.busyOperation === "options-save" ? qsTr("Saving…") : qsTr("Save Category")
                    highlighted: true
                    enabled: root.saveEnabled
                    onClicked: root.saveRequested(root.savePatch)
                }
            }
        }
    }
}
