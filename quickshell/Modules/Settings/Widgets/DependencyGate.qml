pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Widgets
import "AccessibilityDeps.js" as Deps

// Wraps an accessibility control that needs an external program. When the
// program isn't installed, the control is shown but disabled, with a short,
// distro-aware message naming the package to install. The control is never
// hidden — only made unadjustable.
Item {
    id: gate

    property string toolKey: ""
    property bool available: false
    property bool probed: false
    property string family: "default"

    default property alias content: holder.children

    width: parent ? parent.width : 0
    height: column.implicitHeight

    function probe() {
        var bin = Deps.binary(gate.toolKey);
        if (!bin) {
            gate.available = false;
            gate.probed = true;
            return;
        }
        Proc.runCommand("dep-os-" + gate.toolKey, ["sh", "-c", ". /etc/os-release 2>/dev/null; echo \"$ID $ID_LIKE\""], (idOut) => {
            gate.family = Deps.family(idOut || "");
            Proc.runCommand("dep-have-" + gate.toolKey, ["sh", "-c", "command -v " + bin + " >/dev/null 2>&1 && echo yes || echo no"], (out) => {
                gate.available = (out || "").trim() === "yes";
                gate.probed = true;
            });
        });
    }

    Component.onCompleted: probe()

    Column {
        id: column
        width: parent.width
        spacing: Theme.spacingS

        Item {
            id: holder
            width: parent.width
            implicitHeight: childrenRect.height
            height: childrenRect.height
            enabled: gate.available
            opacity: gate.available ? 1.0 : 0.5
        }

        StyledText {
            width: parent.width
            visible: gate.probed && !gate.available
            text: Deps.message(gate.toolKey, gate.family)
            color: Theme.warning
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }
}
