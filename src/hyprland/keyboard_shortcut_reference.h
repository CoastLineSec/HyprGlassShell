#pragma once

#include "validation.h"

#include <QByteArrayView>
#include <QString>
#include <QVector>

#include <optional>

namespace HyprShelld::Hyprland {

inline constexpr auto legacyShortcutSourceDigest =
    "47bbde429980d2fa9817c88915cac595ec887573802ed162980613f576b9979d";
inline constexpr auto legacyShortcutArtifactDigest =
    "ee9f5cbe19e4deea91d4640725c14df4153bec6c1e68b11d8770e39c866fc7ba";
inline constexpr qsizetype legacyShortcutArtifactBytes = 30109;
inline constexpr qsizetype legacyShortcutReferenceRows = 117;

struct KeyboardShortcutReferenceOptions final {
    std::optional<bool> locked;
    std::optional<bool> repeating;
    std::optional<bool> mouse;
    std::optional<QString> description;

    friend bool operator==(
        const KeyboardShortcutReferenceOptions &,
        const KeyboardShortcutReferenceOptions &
    ) = default;
};

struct KeyboardShortcutReferenceRow final {
    int ordinal = 0;
    int sourceLine = 0;
    QString section;
    QString chord;
    QString action;
    QString sourceText;
    KeyboardShortcutReferenceOptions options;

    friend bool operator==(
        const KeyboardShortcutReferenceRow &,
        const KeyboardShortcutReferenceRow &
    ) = default;
};

struct KeyboardShortcutReference final {
    QString sourceDigest;
    QString artifactDigest;
    QVector<KeyboardShortcutReferenceRow> rows;

    friend bool operator==(
        const KeyboardShortcutReference &,
        const KeyboardShortcutReference &
    ) = default;
};

[[nodiscard]] ValidationResult<KeyboardShortcutReference>
parseKeyboardShortcutReference(QByteArrayView bytes);

[[nodiscard]] ValidationResult<KeyboardShortcutReference>
embeddedKeyboardShortcutReference();

} // namespace HyprShelld::Hyprland
