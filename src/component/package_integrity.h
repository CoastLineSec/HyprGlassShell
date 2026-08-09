#pragma once

#include "validation_result.h"

#include <QByteArrayView>
#include <QMap>
#include <QString>

namespace HyprShelld::Components {

struct PackageIntegrity final {
    QMap<QString, QString> files;

    friend bool operator==(const PackageIntegrity &, const PackageIntegrity &)
        = default;
};

[[nodiscard]] ValidationResult<PackageIntegrity> parsePackageIntegrity(
    QByteArrayView bytes
);

} // namespace HyprShelld::Components
