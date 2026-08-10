#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <QtTypes>

#include <optional>

namespace HyprShelld {

class CompositorOptionCatalog;

struct AppearanceSnapshotEdit final {
    QByteArray candidate;
    bool changed = false;
};

class CompositorSnapshotEditor final {
public:
    [[nodiscard]] static bool isExactV1Envelope(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest
    );

    [[nodiscard]] static std::optional<AppearanceSnapshotEdit> replaceAppearance(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const QVariantMap &values,
        QString &error
    );
};

} // namespace HyprShelld
