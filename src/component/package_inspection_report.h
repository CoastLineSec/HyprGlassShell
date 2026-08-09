#pragma once

#include "component_contract.h"
#include "validation_result.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Components {

inline constexpr qint64 maximumComponentArchiveBytes = 32 * 1024 * 1024;
inline constexpr qint64 maximumComponentExpandedBytes = 128 * 1024 * 1024;
inline constexpr qint64 maximumComponentFileBytes = 32 * 1024 * 1024;
inline constexpr qsizetype maximumComponentArchiveEntries = 512;

struct InspectedPackageFile final {
    QString path;
    quint64 size = 0;
    QString sha256;

    friend bool operator==(
        const InspectedPackageFile &,
        const InspectedPackageFile &
    ) = default;
};

struct PackageInspectionReport final {
    QString inspectionToken;
    QString archiveSha256;
    QString packageDigest;
    quint64 archiveSize = 0;
    quint64 expandedSize = 0;
    ComponentManifest manifest;
    QJsonObject normalizedManifest;
    std::optional<QJsonObject> normalizedSettingsSchema;
    QVector<InspectedPackageFile> files;

    friend bool operator==(
        const PackageInspectionReport &,
        const PackageInspectionReport &
    ) = default;
};

[[nodiscard]] QByteArray serializePackageInspectionReport(
    const PackageInspectionReport &report
);

[[nodiscard]] ValidationResult<PackageInspectionReport>
parsePackageInspectionReport(QByteArrayView bytes);

[[nodiscard]] bool isCanonicalPackageFilePath(const QString &path);
[[nodiscard]] bool isAllowedComponentPackageFilePath(const QString &path);

} // namespace HyprShelld::Components
