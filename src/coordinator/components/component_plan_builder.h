#pragma once

#include "component/surface_plan.h"
#include "component/declarative_document.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Components {

struct RuntimeCatalogEntry final {
    QString componentId;
    QString componentType;
    QString packageDigest;
    QString origin;
    bool removable = true;
    QString componentApiVersion;
    QString runtimeKind;
    QString factory;
    QString runtimeEntryPoint;
    QStringList runtimeArguments;
    QByteArray declarativeRuntime;
    std::optional<DeclarativeDocument> declarativeDocument;
    QStringList capabilityIds;
    QStringList dependencyIds;

    friend bool operator==(
        const RuntimeCatalogEntry &,
        const RuntimeCatalogEntry &
    ) = default;
};

struct RuntimeCatalogSnapshot final {
    QString catalogDigest;
    QStringList listedComponentIds;
    QHash<QString, RuntimeCatalogEntry> entries;

    friend bool operator==(
        const RuntimeCatalogSnapshot &,
        const RuntimeCatalogSnapshot &
    ) = default;
};

struct RuntimeDesiredComponent final {
    bool enabled = false;
    QString packageDigest;
    QStringList grantedCapabilities;
    QJsonObject settings;

    friend bool operator==(
        const RuntimeDesiredComponent &,
        const RuntimeDesiredComponent &
    ) = default;
};

struct RuntimeDesiredVisualInstance final {
    QString componentId;
    bool enabled = false;
    QJsonObject settings;

    friend bool operator==(
        const RuntimeDesiredVisualInstance &,
        const RuntimeDesiredVisualInstance &
    ) = default;
};

struct RuntimeDesiredBarLayout final {
    QString outputMode;
    QStringList start;
    QStringList center;
    QStringList end;

    friend bool operator==(
        const RuntimeDesiredBarLayout &,
        const RuntimeDesiredBarLayout &
    ) = default;
};

struct RuntimeConfigurationSnapshot final {
    QString catalogDigest;
    quint64 revision = 0;
    QHash<QString, RuntimeDesiredComponent> components;
    QHash<QString, RuntimeDesiredVisualInstance> instances;
    QHash<QString, RuntimeDesiredBarLayout> barLayouts;

    friend bool operator==(
        const RuntimeConfigurationSnapshot &,
        const RuntimeConfigurationSnapshot &
    ) = default;
};

[[nodiscard]] ValidationResult<SurfacePlan> buildSurfacePlan(
    const RuntimeCatalogSnapshot &catalog,
    const RuntimeConfigurationSnapshot &configuration
);

} // namespace HyprShelld::Components
