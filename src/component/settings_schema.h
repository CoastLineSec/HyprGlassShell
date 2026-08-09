#pragma once

#include "validation_result.h"

#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Components {

enum class SettingScope {
    Component,
    Instance,
};

enum class SettingType {
    Boolean,
    Integer,
    Number,
    String,
    Enumeration,
    Color,
    Keybinding,
    File,
    Directory,
};

struct EnumOption final {
    QString value;
    QString label;

    friend bool operator==(const EnumOption &, const EnumOption &) = default;
};

struct VisibilityCondition final {
    QString key;
    QJsonValue equals;

    friend bool operator==(
        const VisibilityCondition &,
        const VisibilityCondition &
    ) = default;
};

struct SettingDefinition final {
    QString key;
    SettingScope scope = SettingScope::Instance;
    SettingType type = SettingType::Boolean;
    QString label;
    QString description;
    QString group;
    qint32 order = 0;
    QJsonValue defaultValue;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> step;
    std::optional<qsizetype> minimumLength;
    std::optional<qsizetype> maximumLength;
    QVector<EnumOption> options;
    std::optional<VisibilityCondition> visibleWhen;

    friend bool operator==(
        const SettingDefinition &,
        const SettingDefinition &
    ) = default;
};

struct SettingsSchema final {
    quint32 schemaVersion = 1;
    QVector<SettingDefinition> settings;

    [[nodiscard]] const SettingDefinition *find(const QString &key) const;

    friend bool operator==(const SettingsSchema &, const SettingsSchema &) = default;
};

[[nodiscard]] QString toString(SettingScope scope);
[[nodiscard]] QString toString(SettingType type);

[[nodiscard]] std::optional<SettingScope> settingScopeFromString(
    const QString &value
);
[[nodiscard]] std::optional<SettingType> settingTypeFromString(
    const QString &value
);

[[nodiscard]] ValidationResult<SettingsSchema> parseSettingsSchema(
    QByteArrayView bytes
);

[[nodiscard]] ValidationResult<QJsonValue> normalizeSettingValue(
    const SettingDefinition &definition,
    const QJsonValue &value,
    const QString &path = QStringLiteral("$")
);

// Unknown keys and keys from the other scope are rejected. When defaults are
// requested, every definition in the selected scope appears in the result.
[[nodiscard]] ValidationResult<QJsonObject> normalizeSettings(
    const SettingsSchema &schema,
    SettingScope scope,
    const QJsonObject &values,
    bool applyDefaults = true
);

} // namespace HyprShelld::Components
