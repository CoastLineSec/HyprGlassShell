#pragma once

#include "validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QtTypes>

namespace HyprShelld::Hyprland::JsonSupport {

[[nodiscard]] ValidationResult<QJsonObject> parseStrictObject(
    QByteArrayView bytes,
    qsizetype maximumBytes,
    int maximumDepth
);

[[nodiscard]] QByteArray canonicalJson(const QJsonValue &value);

} // namespace HyprShelld::Hyprland::JsonSupport
