import QtQuick
import QtQuick.Effects
import qs.Common
import qs.Services
import qs.Widgets

Item {
    id: aboutTab

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property string compositorName: "hyprland"
    property string compositorLogo: "/assets/hyprland.svg"
    property string compositorUrl: "https://hypr.land"
    property string compositorTooltip: I18n.tr("Hyprland Website")

    property string hgsDiscordUrl: "https://discord.gg/ppWTpKmPgT"
    property string hgsDiscordTooltip: I18n.tr("HyprGlassShell Discord")
    property string compositorDiscordUrl: "https://discord.com/invite/hQ9XvMUjjr"
    property string compositorDiscordTooltip: I18n.tr("Hyprland Discord Server")

    property bool showCompositorDiscord: true
    property bool showReddit: false
    property bool showIrc: false

    HGSFlickable {
        anchors.fill: parent
        clip: true
        contentHeight: mainColumn.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: mainColumn
            topPadding: 4

            width: Math.min(550, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingXL

            // ASCII Art Header
            StyledRect {
                width: parent.width
                height: asciiSection.implicitHeight + Theme.spacingL * 2
                radius: Theme.cornerRadius
                color: Theme.surfaceContainerHigh
                border.color: Qt.rgba(Theme.outline.r, Theme.outline.g, Theme.outline.b, 0.2)
                border.width: 0

                Column {
                    id: asciiSection

                    anchors.fill: parent
                    anchors.margins: Theme.spacingL
                    spacing: Theme.spacingM

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: parent.width < 350 ? Theme.spacingM : Theme.spacingL

                        property bool compactLogo: parent.width < 400
                        property bool hideLogo: parent.width < 280

                        Image {
                            id: logoImage

                            visible: !parent.hideLogo
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.compactLogo ? 80 : 120
                            height: width * (569.94629 / 506.50931)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            asynchronous: true
                            source: "file://" + Theme.shellDir + "/assets/hgslogonormal.svg"
                            layer.enabled: true
                            layer.smooth: true
                            layer.mipmap: true
                            layer.effect: MultiEffect {
                                saturation: 0
                                colorization: 1
                                colorizationColor: Theme.primary
                            }
                        }

                        StyledText {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "HGS LINUX"
                            font.pixelSize: parent.compactLogo ? 32 : 48
                            font.weight: Font.Bold
                            font.family: interFont.name
                            color: Theme.surfaceText
                            antialiasing: true

                            FontLoader {
                                id: interFont
                                source: Qt.resolvedUrl("../../assets/fonts/inter/InterVariable.ttf")
                            }
                        }
                    }

                    StyledText {
                        text: {
                            if (!ShellVersionService.shellVersion && !HGSService.cliVersion)
                                return "hgs";

                            let version = ShellVersionService.shellVersion || "";
                            let cliVersion = HGSService.cliVersion || "";

                            // Debian/Ubuntu/OpenSUSE git format: 1.0.3+git2264.c5c5ce84
                            let match = version.match(/^([\d.]+)\+git(\d+)\./);
                            if (match) {
                                return `hgs (git) v${match[1]}-${match[2]}`;
                            }

                            // Fedora COPR git format: 0.0.git.2267.d430cae9
                            match = version.match(/^[\d.]+\.git\.(\d+)\./);
                            if (match) {
                                function extractBaseVersion(value) {
                                    if (!value)
                                        return "";
                                    let baseMatch = value.match(/(\d+\.\d+\.\d+)/);
                                    if (baseMatch)
                                        return baseMatch[1];
                                    baseMatch = value.match(/(\d+\.\d+)/);
                                    if (baseMatch)
                                        return baseMatch[1];
                                    return "";
                                }

                                let baseVersion = extractBaseVersion(cliVersion);
                                if (!baseVersion)
                                    baseVersion = extractBaseVersion(ShellVersionService.semverVersion);
                                if (baseVersion) {
                                    return `hgs (git) v${baseVersion}-${match[1]}`;
                                }
                                return `hgs (git) v${match[1]}`;
                            }

                            // Stable release format: 1.0.3
                            match = version.match(/^([\d.]+)$/);
                            if (match) {
                                return `hgs v${match[1]}`;
                            }

                            if (!version && cliVersion) {
                                match = cliVersion.match(/^([\d.]+)\+git(\d+)\./);
                                if (match) {
                                    return `hgs (git) v${match[1]}-${match[2]}`;
                                }
                                match = cliVersion.match(/^([\d.]+)$/);
                                if (match) {
                                    return `hgs v${match[1]}`;
                                }
                                return `hgs ${cliVersion}`;
                            }

                            return `hgs ${version}`;
                        }
                        font.pixelSize: Theme.fontSizeXLarge
                        font.weight: Font.Bold
                        color: Theme.surfaceText
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }

                    StyledText {
                        visible: ShellVersionService.shellCodename.length > 0
                        text: `"${ShellVersionService.shellCodename}"`
                        font.pixelSize: Theme.fontSizeMedium
                        font.italic: true
                        color: Theme.surfaceVariantText
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }

                    Row {
                        id: resourceButtonsRow
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: Theme.spacingS

                        property bool compactMode: parent.width < 450

                        HGSButton {
                            id: docsButton
                            text: resourceButtonsRow.compactMode ? "" : I18n.tr("Docs")
                            iconName: "menu_book"
                            iconSize: 18
                            backgroundColor: Qt.rgba(Theme.surfaceText.r, Theme.surfaceText.g, Theme.surfaceText.b, 0.08)
                            textColor: Theme.surfaceText
                            onClicked: Qt.openUrlExternally("https://coastlinesec.com/docs")
                            onHoveredChanged: {
                                if (hovered)
                                    resourceTooltip.show(resourceButtonsRow.compactMode ? I18n.tr("Docs") + " - coastlinesec.com/docs" : "coastlinesec.com/docs", docsButton, 0, 0, "bottom");
                                else
                                    resourceTooltip.hide();
                            }
                        }

                        HGSButton {
                            id: githubButton
                            text: resourceButtonsRow.compactMode ? "" : I18n.tr("GitHub")
                            iconName: "code"
                            iconSize: 18
                            backgroundColor: Qt.rgba(Theme.surfaceText.r, Theme.surfaceText.g, Theme.surfaceText.b, 0.08)
                            textColor: Theme.surfaceText
                            onClicked: Qt.openUrlExternally("https://github.com/CoastLineSec/HyprGlassShell")
                            onHoveredChanged: {
                                if (hovered)
                                    resourceTooltip.show(resourceButtonsRow.compactMode ? "GitHub - CoastLineSec/HyprGlassShell" : "github.com/CoastLineSec/HyprGlassShell", githubButton, 0, 0, "bottom");
                                else
                                    resourceTooltip.hide();
                            }
                        }

                        HGSButton {
                            id: kofiButton
                            text: resourceButtonsRow.compactMode ? "" : I18n.tr("Ko-fi")
                            iconName: "favorite"
                            iconSize: 18
                            backgroundColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12)
                            textColor: Theme.primary
                            onClicked: Qt.openUrlExternally("https://ko-fi.com/coastlinesec")
                            onHoveredChanged: {
                                if (hovered)
                                    resourceTooltip.show(resourceButtonsRow.compactMode ? I18n.tr("Ko-fi") + " - ko-fi.com/coastlinesec" : "ko-fi.com/coastlinesec", kofiButton, 0, 0, "bottom");
                                else
                                    resourceTooltip.hide();
                            }
                        }
                    }

                    HGSTooltipV2 {
                        id: resourceTooltip
                    }

                    Item {
                        id: communityIcons
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 24
                        width: {
                            let baseWidth = compositorButton.width + hgsDiscordButton.width + Theme.spacingM;
                            if (showIrc) {
                                baseWidth += ircButton.width + Theme.spacingM;
                            }
                            if (showCompositorDiscord) {
                                baseWidth += compositorDiscordButton.width + Theme.spacingM;
                            }
                            if (showReddit) {
                                baseWidth += redditButton.width + Theme.spacingM;
                            }
                            return baseWidth;
                        }

                        Item {
                            id: compositorButton
                            width: 24
                            height: 24
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.verticalCenterOffset: -2
                            x: 0

                            property bool hovered: false
                            property string tooltipText: compositorTooltip

                            Image {
                                anchors.fill: parent
                                source: Qt.resolvedUrl(".").toString().replace("file://", "").replace("/Modules/Settings/", "") + compositorLogo
                                sourceSize: Qt.size(24, 24)
                                smooth: true
                                fillMode: Image.PreserveAspectFit
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onEntered: parent.hovered = true
                                onExited: parent.hovered = false
                                onClicked: Qt.openUrlExternally(compositorUrl)
                            }
                        }

                        Item {
                            id: ircButton
                            width: 24
                            height: 24
                            x: compositorButton.x + compositorButton.width + Theme.spacingM
                            anchors.verticalCenter: parent.verticalCenter
                            visible: showIrc

                            property bool hovered: false
                            property string tooltipText: ircTooltip

                            HGSIcon {
                                anchors.centerIn: parent
                                name: "forum"
                                size: 20
                                color: Theme.surfaceText
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onEntered: parent.hovered = true
                                onExited: parent.hovered = false
                                onClicked: Qt.openUrlExternally(ircUrl)
                            }
                        }

                        Item {
                            id: hgsDiscordButton
                            width: 20
                            height: 20
                            x: {
                                if (showIrc)
                                    return ircButton.x + ircButton.width + Theme.spacingM;
                                return compositorButton.x + compositorButton.width + Theme.spacingM;
                            }
                            anchors.verticalCenter: parent.verticalCenter

                            property bool hovered: false
                            property string tooltipText: hgsDiscordTooltip

                            Image {
                                anchors.fill: parent
                                source: Qt.resolvedUrl(".").toString().replace("file://", "").replace("/Modules/Settings/", "") + "/assets/discord.svg"
                                sourceSize: Qt.size(20, 20)
                                smooth: true
                                fillMode: Image.PreserveAspectFit
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onEntered: parent.hovered = true
                                onExited: parent.hovered = false
                                onClicked: Qt.openUrlExternally(hgsDiscordUrl)
                            }
                        }

                        Item {
                            id: compositorDiscordButton
                            width: 20
                            height: 20
                            x: hgsDiscordButton.x + hgsDiscordButton.width + Theme.spacingM
                            anchors.verticalCenter: parent.verticalCenter
                            visible: showCompositorDiscord

                            property bool hovered: false
                            property string tooltipText: compositorDiscordTooltip

                            Image {
                                anchors.fill: parent
                                source: Qt.resolvedUrl(".").toString().replace("file://", "").replace("/Modules/Settings/", "") + "/assets/discord.svg"
                                sourceSize: Qt.size(20, 20)
                                smooth: true
                                fillMode: Image.PreserveAspectFit
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onEntered: parent.hovered = true
                                onExited: parent.hovered = false
                                onClicked: Qt.openUrlExternally(compositorDiscordUrl)
                            }
                        }

                        Item {
                            id: redditButton
                            width: 20
                            height: 20
                            x: showCompositorDiscord ? compositorDiscordButton.x + compositorDiscordButton.width + Theme.spacingM : hgsDiscordButton.x + hgsDiscordButton.width + Theme.spacingM
                            anchors.verticalCenter: parent.verticalCenter
                            visible: showReddit

                            property bool hovered: false
                            property string tooltipText: redditTooltip

                            Image {
                                anchors.fill: parent
                                source: Qt.resolvedUrl(".").toString().replace("file://", "").replace("/Modules/Settings/", "") + "/assets/reddit.svg"
                                sourceSize: Qt.size(20, 20)
                                smooth: true
                                fillMode: Image.PreserveAspectFit
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onEntered: parent.hovered = true
                                onExited: parent.hovered = false
                                onClicked: Qt.openUrlExternally(redditUrl)
                            }
                        }
                    }
                }
            }

            // Project Information
            StyledRect {
                width: parent.width
                height: projectSection.implicitHeight + Theme.spacingL * 2
                radius: Theme.cornerRadius
                color: Theme.surfaceContainerHigh
                border.color: Qt.rgba(Theme.outline.r, Theme.outline.g, Theme.outline.b, 0.2)
                border.width: 0

                Column {
                    id: projectSection

                    anchors.fill: parent
                    anchors.margins: Theme.spacingL
                    spacing: Theme.spacingM

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        HGSIcon {
                            name: "info"
                            size: Theme.iconSize
                            color: Theme.primary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        StyledText {
                            text: I18n.tr("About")
                            font.pixelSize: Theme.fontSizeLarge
                            font.weight: Font.Medium
                            color: Theme.surfaceText
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    StyledText {
                        text: I18n.tr('hgs is a highly customizable, modern desktop shell with a <a href="https://m3.material.io/" style="text-decoration:none; color:%1;">material 3 inspired</a> design.<br /><br/>It is built with <a href="https://quickshell.org" style="text-decoration:none; color:%1;">Quickshell</a>, a QT6 framework for building desktop shells, and <a href="https://go.dev" style="text-decoration:none; color:%1;">Go</a>, a statically typed, compiled programming language.').arg(Theme.primary)
                        textFormat: Text.RichText
                        font.pixelSize: Theme.fontSizeMedium
                        linkColor: Theme.primary
                        onLinkActivated: url => Qt.openUrlExternally(url)
                        color: Theme.surfaceVariantText
                        width: parent.width
                        wrapMode: Text.WordWrap

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            acceptedButtons: Qt.NoButton
                            propagateComposedEvents: true
                        }
                    }
                }
            }

            StyledRect {
                visible: HGSService.isConnected
                width: parent.width
                height: backendSection.implicitHeight + Theme.spacingL * 2
                radius: Theme.cornerRadius
                color: Theme.surfaceContainerHigh
                border.color: Qt.rgba(Theme.outline.r, Theme.outline.g, Theme.outline.b, 0.2)
                border.width: 0

                Column {
                    id: backendSection

                    anchors.fill: parent
                    anchors.margins: Theme.spacingL
                    spacing: Theme.spacingM

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        HGSIcon {
                            name: "dns"
                            size: Theme.iconSize
                            color: Theme.primary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        StyledText {
                            text: I18n.tr("Backend")
                            font.pixelSize: Theme.fontSizeLarge
                            font.weight: Font.Medium
                            color: Theme.surfaceText
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Row {
                        anchors.left: parent.left
                        spacing: Theme.spacingL

                        Column {
                            spacing: 2

                            StyledText {
                                text: I18n.tr("Version")
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.surfaceVariantText
                                horizontalAlignment: Text.AlignLeft
                            }

                            StyledText {
                                text: HGSService.cliVersion || "—"
                                font.pixelSize: Theme.fontSizeMedium
                                font.weight: Font.Medium
                                color: Theme.surfaceText
                                horizontalAlignment: Text.AlignLeft
                            }
                        }

                        Rectangle {
                            width: 1
                            height: 32
                            color: Theme.outlineVariant
                        }

                        Column {
                            spacing: 2

                            StyledText {
                                text: I18n.tr("API")
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.surfaceVariantText
                                horizontalAlignment: Text.AlignLeft
                            }

                            StyledText {
                                text: `v${HGSService.apiVersion}`
                                font.pixelSize: Theme.fontSizeMedium
                                font.weight: Font.Medium
                                color: Theme.surfaceText
                                horizontalAlignment: Text.AlignLeft
                            }
                        }

                        Rectangle {
                            width: 1
                            height: 32
                            color: Theme.outlineVariant
                        }

                        Column {
                            spacing: 2

                            StyledText {
                                text: I18n.tr("Status")
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.surfaceVariantText
                                horizontalAlignment: Text.AlignLeft
                            }

                            Row {
                                spacing: 4

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: Theme.success
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                StyledText {
                                    text: I18n.tr("Connected")
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.weight: Font.Medium
                                    color: Theme.surfaceText
                                    horizontalAlignment: Text.AlignLeft
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingS
                        visible: HGSService.capabilities.length > 0

                        StyledText {
                            text: I18n.tr("Capabilities")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        Flow {
                            width: parent.width
                            spacing: 6

                            Repeater {
                                model: HGSService.capabilities

                                Rectangle {
                                    width: capText.implicitWidth + 16
                                    height: 26
                                    radius: 13
                                    color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12)

                                    StyledText {
                                        id: capText
                                        anchors.centerIn: parent
                                        text: modelData
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.primary
                                    }
                                }
                            }
                        }
                    }
                }
            }

            StyledRect {
                width: parent.width
                height: toolsSection.implicitHeight + Theme.spacingL * 2
                radius: Theme.cornerRadius
                color: Theme.surfaceContainerHigh
                border.color: Qt.rgba(Theme.outline.r, Theme.outline.g, Theme.outline.b, 0.2)
                border.width: 0

                Column {
                    id: toolsSection

                    anchors.fill: parent
                    anchors.margins: Theme.spacingL
                    spacing: Theme.spacingM

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        HGSIcon {
                            name: "build"
                            size: Theme.iconSize
                            color: Theme.primary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        StyledText {
                            text: I18n.tr("Tools")
                            font.pixelSize: Theme.fontSizeLarge
                            font.weight: Font.Medium
                            color: Theme.surfaceText
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Row {
                        anchors.left: parent.left
                        spacing: Theme.spacingS

                        HGSButton {
                            text: I18n.tr("Show Welcome")
                            iconName: "waving_hand"
                            backgroundColor: Qt.rgba(Theme.surfaceText.r, Theme.surfaceText.g, Theme.surfaceText.b, 0.08)
                            textColor: Theme.surfaceText
                            onClicked: FirstLaunchService.showWelcome()
                        }

                        HGSButton {
                            text: I18n.tr("System Check")
                            iconName: "vital_signs"
                            backgroundColor: Qt.rgba(Theme.surfaceText.r, Theme.surfaceText.g, Theme.surfaceText.b, 0.08)
                            textColor: Theme.surfaceText
                            onClicked: FirstLaunchService.showDoctor()
                        }
                    }
                }
            }

            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: I18n.tr('<a href="https://github.com/CoastLineSec/HyprGlassShell/blob/master/LICENSE" style="text-decoration:none; color:%1;">MIT License</a>').arg(Theme.surfaceVariantText)
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.surfaceVariantText
                textFormat: Text.RichText
                wrapMode: Text.NoWrap
                onLinkActivated: url => Qt.openUrlExternally(url)

                MouseArea {
                    anchors.fill: parent
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    acceptedButtons: Qt.NoButton
                    propagateComposedEvents: true
                }
            }
        }
    }

    // Community tooltip - positioned absolutely above everything
    Rectangle {
        id: communityTooltip
        parent: aboutTab
        z: 1000

        property var hoveredButton: {
            if (compositorButton.hovered)
                return compositorButton;
            if (matrixButton.visible && matrixButton.hovered)
                return matrixButton;
            if (ircButton.visible && ircButton.hovered)
                return ircButton;
            if (hgsDiscordButton.hovered)
                return hgsDiscordButton;
            if (compositorDiscordButton.visible && compositorDiscordButton.hovered)
                return compositorDiscordButton;
            if (redditButton.visible && redditButton.hovered)
                return redditButton;
            return null;
        }

        property string tooltipText: hoveredButton ? hoveredButton.tooltipText : ""

        visible: hoveredButton !== null && tooltipText !== ""
        width: tooltipLabel.implicitWidth + 24
        height: tooltipLabel.implicitHeight + 12

        color: Theme.surfaceContainer
        radius: Theme.cornerRadius
        border.width: 0
        border.color: Theme.outlineMedium

        x: hoveredButton ? hoveredButton.mapToItem(aboutTab, hoveredButton.width / 2, 0).x - width / 2 : 0
        y: hoveredButton ? communityIcons.mapToItem(aboutTab, 0, 0).y - height - 8 : 0

        ElevationShadow {
            anchors.fill: parent
            z: -1
            level: Theme.elevationLevel1
            fallbackOffset: 1
            targetRadius: communityTooltip.radius
            targetColor: communityTooltip.color
            borderColor: communityTooltip.border.color
            borderWidth: communityTooltip.border.width
            shadowOpacity: Theme.elevationLevel1 && Theme.elevationLevel1.alpha !== undefined ? Theme.elevationLevel1.alpha : 0.2
            shadowEnabled: Theme.elevationEnabled
        }

        StyledText {
            id: tooltipLabel
            anchors.centerIn: parent
            text: communityTooltip.tooltipText
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.surfaceText
        }
    }
}
