pragma ComponentBehavior: Bound

import QtQuick
import Quickshell

Item {
    id: root

    required property var instances
    required property string outputName
    required property var workspaceSource
    property bool interactive: true
    property bool keyboardNavigationEnabled: false
    property bool animationsEnabled: true

    objectName: "barComponentHost"
    implicitWidth: componentLayout.implicitWidth
    implicitHeight: componentLayout.implicitHeight
    clip: true

    ScriptModel {
        id: instanceModel

        values: Array.from(root.instances || [])
        objectProp: "instanceId"
    }

    Item {
        id: componentLayout

        property int layoutRevision: 0
        property real spacing: 8
        width: root.width
        height: root.height
        implicitHeight: root.height
        implicitWidth: {
            layoutRevision;
            let width = 0;
            let visibleCount = 0;
            for (let index = 0; index < componentRepeater.count; ++index) {
                const component = componentRepeater.itemAt(index);
                if (!component || component.width <= 0)
                    continue;
                if (visibleCount > 0)
                    width += spacing;
                width += component.width;
                ++visibleCount;
            }
            return width;
        }

        function positionFor(targetIndex) {
            let position = 0;
            let visibleCount = 0;
            for (let index = 0; index < targetIndex; ++index) {
                const component = componentRepeater.itemAt(index);
                if (!component || component.width <= 0)
                    continue;
                if (visibleCount > 0)
                    position += spacing;
                position += component.width;
                ++visibleCount;
            }
            if (visibleCount > 0) {
                const target = componentRepeater.itemAt(targetIndex);
                if (target && target.width > 0)
                    position += spacing;
            }
            return position;
        }

        Repeater {
            id: componentRepeater

            model: instanceModel

            onCountChanged: ++componentLayout.layoutRevision

            delegate: BuiltinComponentFactory {
                required property var modelData
                required property int index

                activation: modelData
                outputName: root.outputName
                workspaceSource: root.workspaceSource
                interactive: root.interactive
                keyboardNavigationEnabled:
                    root.keyboardNavigationEnabled
                animationsEnabled: root.animationsEnabled
                x: {
                    componentLayout.layoutRevision;
                    return componentLayout.positionFor(index);
                }
                width: implicitWidth
                height: componentLayout.height

                onWidthChanged: ++componentLayout.layoutRevision
                onIndexChanged: ++componentLayout.layoutRevision
            }
        }
    }
}
