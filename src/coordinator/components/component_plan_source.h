#pragma once

#include "component/component_configuration.h"
#include "component/validation_result.h"
#include "component_plan_builder.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

namespace HyprShelld::Components {

// Normalized fields returned by ComponentManager1.GetComponent. Keeping the
// D-Bus decoding separate from this record makes the authority boundary
// independently testable.
struct RuntimeCatalogComponentRecord final {
    QString componentId;
    quint32 manifestVersion = 0;
    QString componentType;
    QString version;
    QString name;
    QString description;
    QStringList authorNames;
    QStringList authorEmails;
    QStringList authorHomepages;
    QString license;
    QString homepage;
    QString source;
    QString issues;
    QString componentApiVersion;
    QString runtimeKind;
    QString runtimeFactory;
    QString runtimeEntryPoint;
    QStringList runtimeArguments;
    QByteArray declarativeRuntime;
    QByteArray settingsSchema;
    QStringList capabilityIds;
    QStringList capabilityReasons;
    QStringList dependencyIds;
    QStringList dependencyVersionRequirements;
    QString packageDigest;
    QString origin;
    bool removable = true;
};

struct HydratedRuntimeCatalog final {
    ConfigurationCatalog configurationCatalog;
    RuntimeCatalogSnapshot runtimeCatalog;
};

struct ValidatedRuntimeCatalogRecord final {
    ConfigurationCatalogEntry configurationEntry;
    RuntimeCatalogEntry runtimeEntry;
};

// These two validators are also used by the asynchronous hydrator so malformed
// catalog generations fail before they can amplify into additional D-Bus calls
// or retained record data.
[[nodiscard]] ValidationResult<QStringList> validateRuntimeCatalogListing(
    const QStringList &listedComponentIds,
    const QString &catalogDigest
);

[[nodiscard]] ValidationResult<ValidatedRuntimeCatalogRecord>
validateRuntimeCatalogRecord(const RuntimeCatalogComponentRecord &record);

[[nodiscard]] ValidationResult<HydratedRuntimeCatalog>
hydrateRuntimeCatalog(
    const QStringList &listedComponentIds,
    const QString &catalogDigest,
    const QVector<RuntimeCatalogComponentRecord> &records
);

// The bytes are always parsed by ComponentConfiguration's authoritative
// strict parser before any runtime DTO is produced.
[[nodiscard]] ValidationResult<RuntimeConfigurationSnapshot>
hydrateRuntimeConfiguration(
    QByteArrayView snapshotBytes,
    quint64 snapshotRevision,
    const QString &snapshotCatalogDigest,
    const ConfigurationCatalog &catalog
);

} // namespace HyprShelld::Components
