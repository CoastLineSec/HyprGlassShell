pragma Singleton

import QtQuick
import QtQuick.Controls
import HyprShelld.Client

QtObject {
    id: root

    readonly property string preference: ConfigClient.appearanceMode
    // Non-persistent fixture/preview hook. Production surfaces leave this
    // empty and always resolve from the Config1 preference.
    property string previewMode: ""
    readonly property string systemMode:
        Application.styleHints.colorScheme === Qt.Light ? "light" : "dark"
    readonly property string effectiveMode:
        previewMode === "light" || previewMode === "dark"
            ? previewMode
            : preference === "automatic" ? systemMode
                : preference === "light" ? "light" : "dark"
    readonly property bool isLight: effectiveMode === "light"

    readonly property int transitionDuration: 180

    // Legacy HGS' elevation ladder. A surface chooses one rung instead of
    // inventing a page-local gray.
    readonly property color well: colorFor(effectiveMode, "well")
    readonly property color canvas: colorFor(effectiveMode, "canvas")
    readonly property color card: colorFor(effectiveMode, "card")
    readonly property color floating: colorFor(effectiveMode, "floating")
    readonly property color track: colorFor(effectiveMode, "track")

    readonly property color onSurface: colorFor(effectiveMode, "onSurface")
    readonly property color onSurfaceMuted:
        colorFor(effectiveMode, "onSurfaceMuted")
    readonly property color onSurfaceDisabled:
        colorFor(effectiveMode, "onSurfaceDisabled")
    readonly property color outline: colorFor(effectiveMode, "outline")
    readonly property color outlineStrong:
        colorFor(effectiveMode, "outlineStrong")
    readonly property color shadow: colorFor(effectiveMode, "shadow")

    readonly property color primary: colorFor(effectiveMode, "primary")
    readonly property color onPrimary: colorFor(effectiveMode, "onPrimary")
    readonly property color primaryContainer:
        colorFor(effectiveMode, "primaryContainer")
    readonly property color onPrimaryContainer:
        colorFor(effectiveMode, "onPrimaryContainer")

    readonly property color success: colorFor(effectiveMode, "success")
    readonly property color successContainer:
        colorFor(effectiveMode, "successContainer")
    readonly property color onSuccessContainer:
        colorFor(effectiveMode, "onSuccessContainer")
    readonly property color successOutline:
        colorFor(effectiveMode, "successOutline")

    readonly property color warning: colorFor(effectiveMode, "warning")
    readonly property color warningContainer:
        colorFor(effectiveMode, "warningContainer")
    readonly property color onWarningContainer:
        colorFor(effectiveMode, "onWarningContainer")
    readonly property color warningOutline:
        colorFor(effectiveMode, "warningOutline")

    readonly property color error: colorFor(effectiveMode, "error")
    readonly property color errorContainer:
        colorFor(effectiveMode, "errorContainer")
    readonly property color onErrorContainer:
        colorFor(effectiveMode, "onErrorContainer")
    readonly property color errorOutline:
        colorFor(effectiveMode, "errorOutline")

    readonly property color info: colorFor(effectiveMode, "info")
    readonly property color infoContainer:
        colorFor(effectiveMode, "infoContainer")
    readonly property color onInfoContainer:
        colorFor(effectiveMode, "onInfoContainer")
    readonly property color infoOutline:
        colorFor(effectiveMode, "infoOutline")

    readonly property color surfaceHover: overlay(onSurface, 0.07, card)
    readonly property color surfacePressed: overlay(onSurface, 0.14, card)
    readonly property color neutralButton: overlay(onSurface, 0.10, card)
    readonly property color neutralButtonHover: overlay(onSurface, 0.15, card)

    function normalizedMode(mode) {
        if (mode === "automatic")
            return root.systemMode;
        return mode === "light" ? "light" : "dark";
    }

    function overlay(foreground, opacity, background) {
        return Qt.rgba(
            foreground.r * opacity + background.r * (1 - opacity),
            foreground.g * opacity + background.g * (1 - opacity),
            foreground.b * opacity + background.b * (1 - opacity),
            1
        );
    }

    function colorFor(mode, role) {
        const light = root.normalizedMode(mode) === "light";
        const colors = light ? {
            well: "#d0d0d0",
            canvas: "#d0d0d0",
            card: "#e0e0e0",
            floating: "#f0f0f0",
            track: "#b0b0b0",
            onSurface: "#1a1a1a",
            onSurfaceMuted: "#5a5a5a",
            onSurfaceDisabled: "#777777",
            outline: "#767676",
            outlineStrong: "#535353",
            shadow: "#50000000",
            primary: "#394fb3",
            onPrimary: "#f7f8ff",
            primaryContainer: "#cbd3ff",
            onPrimaryContainer: "#182765",
            success: "#23643b",
            successContainer: "#c4e8cf",
            onSuccessContainer: "#163e27",
            successOutline: "#397a50",
            warning: "#7a4600",
            warningContainer: "#f0d3a8",
            onWarningContainer: "#482800",
            warningOutline: "#8a5311",
            error: "#9b2439",
            errorContainer: "#efc7ce",
            onErrorContainer: "#5b1422",
            errorOutline: "#a93a4e",
            info: "#175f84",
            infoContainer: "#c5e4f2",
            onInfoContainer: "#123d54",
            infoOutline: "#347594"
        } : {
            well: "#141414",
            canvas: "#141414",
            card: "#242424",
            floating: "#343434",
            track: "#444444",
            onSurface: "#ececec",
            onSurfaceMuted: "#a8a8a8",
            onSurfaceDisabled: "#777777",
            outline: "#5d5d5d",
            outlineStrong: "#8a8a8a",
            shadow: "#a0000000",
            primary: "#91a4ff",
            onPrimary: "#11162d",
            primaryContainer: "#29396f",
            onPrimaryContainer: "#dbe1ff",
            success: "#68d391",
            successContainer: "#193224",
            onSuccessContainer: "#a9e6bc",
            successOutline: "#568f69",
            warning: "#f6ad55",
            warningContainer: "#33251a",
            onWarningContainer: "#ffd5a1",
            warningOutline: "#8b6b43",
            error: "#fb7185",
            errorContainer: "#382125",
            onErrorContainer: "#ffb8c3",
            errorOutline: "#9b5360",
            info: "#8fd4ff",
            infoContainer: "#192d39",
            onInfoContainer: "#b9e7ff",
            infoOutline: "#4b7890"
        };
        return colors[role] !== undefined ? colors[role] : colors.onSurface;
    }
}
