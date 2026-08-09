#pragma once

#include "package_inspection_report.h"
#include "validation_result.h"

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QVector>

namespace HyprShelld::Components {

struct ComponentPackageBundleFile final {
    QString path;
    QByteArray contents;

    friend bool operator==(
        const ComponentPackageBundleFile &,
        const ComponentPackageBundleFile &
    ) = default;
};

[[nodiscard]] bool writeComponentPackageBundle(
    QIODevice &destination,
    const QVector<ComponentPackageBundleFile> &files,
    QString &error
);

[[nodiscard]] ValidationResult<QVector<ComponentPackageBundleFile>>
readComponentPackageBundle(QIODevice &source);

// Reads and verifies the complete bundle against the inspection report before
// writing anything. The destination must already exist, be empty, and not be a
// symbolic link. Materialized files are owner-readable/writable only.
[[nodiscard]] ValidationErrors materializeComponentPackageBundle(
    QIODevice &source,
    const PackageInspectionReport &expectedReport,
    const QString &destinationDirectory
);

[[nodiscard]] QString deriveComponentPackageDigest(
    const QVector<ComponentPackageBundleFile> &files
);

} // namespace HyprShelld::Components
