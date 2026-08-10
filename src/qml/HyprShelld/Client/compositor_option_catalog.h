#pragma once

#include "hyprland/catalog.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace HyprShelld {

class CompositorOptionCatalog final {
public:
    [[nodiscard]] static std::optional<CompositorOptionCatalog> fromBytes(
        QByteArrayView bytes,
        const QString &replyDigest,
        const QString &advertisedDigest,
        QString &error
    );

    [[nodiscard]] const QString &digest() const;
    [[nodiscard]] const QVariantList &appearanceOptions() const;
    [[nodiscard]] const Hyprland::OptionDefinition *appearanceOption(
        const QString &id
    ) const;
    [[nodiscard]] QStringList appearanceOptionIds() const;
    [[nodiscard]] std::optional<QVariantMap> appearanceValues(
        const QJsonObject &snapshot,
        QString &error
    ) const;

private:
    Hyprland::Catalog catalog_;
    QVariantList appearanceOptions_;
};

} // namespace HyprShelld
