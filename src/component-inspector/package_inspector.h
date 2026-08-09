#pragma once

#include "component/package_inspection_report.h"
#include "component/validation_result.h"

#include <QIODevice>
#include <QString>

#include <optional>

namespace HyprShelld::Components {

struct PackageInspectionResult final {
    std::optional<PackageInspectionReport> report;
    ValidationErrors errors;

    [[nodiscard]] bool ok() const
    {
        return report.has_value() && errors.isEmpty();
    }

    explicit operator bool() const
    {
        return ok();
    }
};

// The archive descriptor is duplicated and its bytes are retained before ZIP
// parsing, preventing path reopening and caller-side replacement races. The
// materialized bundle remains untouched until the complete package validates.
[[nodiscard]] PackageInspectionResult inspectComponentPackage(
    int archiveFileDescriptor,
    const QString &inspectionToken,
    const QString &expectedArchiveSha256,
    QIODevice *materializedBundle = nullptr
);

} // namespace HyprShelld::Components
