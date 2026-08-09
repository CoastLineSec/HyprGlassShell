#pragma once

#include "settings_schema.h"
#include "validation_result.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Components {

inline constexpr qsizetype maximumDeclarativeDocumentBytes = 16 * 1024;
inline constexpr qsizetype maximumDeclarativeResolvedTextLength = 128;
inline constexpr qsizetype maximumDeclarativeTooltipLength = 256;
inline constexpr quint32 minimumDeclarativeMaximumWidth = 48;
inline constexpr quint32 maximumDeclarativeMaximumWidth = 512;

enum class DeclarativeTextSourceKind {
    Literal,
    ComponentSetting,
};

// Applies the renderer's final plain-text boundary to a literal or a value
// resolved from trusted component settings.
[[nodiscard]] bool isValidDeclarativeResolvedText(const QString &value);

struct DeclarativeTextSource final {
    DeclarativeTextSourceKind kind = DeclarativeTextSourceKind::Literal;
    QString value;

    friend bool operator==(
        const DeclarativeTextSource &,
        const DeclarativeTextSource &
    ) = default;
};

// Version one deliberately exposes a single trusted primitive. It describes
// data only: the shell owns all rendering, interaction, and resource access.
struct DeclarativeDocument final {
    quint32 documentVersion = 1;
    DeclarativeTextSource text;
    std::optional<QString> tooltip;
    std::optional<quint32> maximumWidth;

    friend bool operator==(
        const DeclarativeDocument &,
        const DeclarativeDocument &
    ) = default;
};

// A settings schema is optional only for literal documents. A setting-backed
// text source is accepted solely when it resolves to a component-scoped string
// or enumeration in the supplied schema.
[[nodiscard]] ValidationResult<DeclarativeDocument>
parseDeclarativeDocument(
    QByteArrayView bytes,
    const SettingsSchema *settingsSchema = nullptr
);

// Serializes a previously validated typed document into the only bytes that
// cross the manager/runtime boundary. The result is compact, deterministic
// JSON with no trailing newline.
[[nodiscard]] QByteArray serializeDeclarativeDocument(
    const DeclarativeDocument &document
);

} // namespace HyprShelld::Components
