pragma Singleton

import QtQuick
import Quickshell
import qs.Common

// Shared "fluid glass" look. The QML hyprglass bar (BarCanvas) and the settings
// glass fields both read these values, so tuning the look in one place is reflected
// everywhere. These get wired to user-facing settings later.
Singleton {
    id: root

    // Continuous glass level (0..1) — drives frost + tint together.
    property real level: 0.5
    // No-tint mode (frost only).
    property bool noTint: false
    // Tint colour. System tint follows light/dark; Custom/Matugen set this later.
    property color tintHue: SessionData.isLightMode ? "#ffffff" : "#16161c"
}
