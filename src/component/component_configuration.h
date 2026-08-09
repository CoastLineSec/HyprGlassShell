#pragma once

#include "builtin_component_defaults.h"
#include "component_contract.h"
#include "settings_schema.h"
#include "validation_result.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtTypes>

namespace HyprShelld::Components {

inline constexpr qsizetype maximumComponentConfigurationBytes =
    2 * 1024 * 1024;

struct ConfigurationCatalogEntry final {
    QString packageDigest;
    ComponentType type = ComponentType::BarWidget;
    ComponentOrigin origin = ComponentOrigin::User;
    SettingsSchema settingsSchema;
    QSet<QString> requestedCapabilities;
};

struct ConfigurationCatalog final {
    QString digest;
    QMap<QString, ConfigurationCatalogEntry> entries;
};

struct DesiredComponent final {
    QString packageDigest;
    bool enabled = false;
    QStringList grantedCapabilities;
    QJsonObject settings;

    friend bool operator==(const DesiredComponent &, const DesiredComponent &) = default;
};

struct ComponentInstance final {
    QString componentId;
    bool enabled = false;
    QJsonObject settings;

    friend bool operator==(const ComponentInstance &, const ComponentInstance &) = default;
};

struct OutputSelector final {
    QString mode = QStringLiteral("all");
    QStringList names;

    friend bool operator==(const OutputSelector &, const OutputSelector &) = default;
};

struct BarLayout final {
    OutputSelector outputs;
    QStringList start;
    QStringList center;
    QStringList end;

    friend bool operator==(const BarLayout &, const BarLayout &) = default;
};

struct ComponentConfiguration final {
    quint64 revision = 0;
    QMap<QString, DesiredComponent> components;
    QMap<QString, ComponentInstance> instances;
    QMap<QString, BarLayout> bars;

    friend bool operator==(
        const ComponentConfiguration &,
        const ComponentConfiguration &
    ) = default;
};

// Parses the strict persisted grammar and applies catalog-owned schema
// normalization only to records whose package digest matches the live entry.
// Missing and digest-mismatched records are retained as inert recovery data.
[[nodiscard]] ValidationResult<ComponentConfiguration>
parseComponentConfiguration(
    QByteArrayView bytes,
    const ConfigurationCatalog &catalog
);

[[nodiscard]] QByteArray serializeComponentConfiguration(
    const ComponentConfiguration &configuration
);

[[nodiscard]] bool isFullSha256Digest(const QString &digest);
[[nodiscard]] bool isLowercaseUuidV4(const QString &uuid);

} // namespace HyprShelld::Components
