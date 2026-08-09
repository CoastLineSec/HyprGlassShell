#pragma once

#include "validation_result.h"

#include <QByteArrayView>
#include <QJsonObject>
#include <QtTypes>

namespace HyprShelld::Components {

struct StrictJsonLimits final {
    qsizetype maximumBytes = 128 * 1024;
    int maximumDepth = 32;
};

// Qt's JSON model is used after parsing, but this reader owns the grammar so
// duplicate object keys cannot be silently collapsed by QJsonDocument.
[[nodiscard]] ValidationResult<QJsonObject> parseStrictJsonObject(
    QByteArrayView bytes,
    StrictJsonLimits limits = {}
);

} // namespace HyprShelld::Components
