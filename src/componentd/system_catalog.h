#pragma once

#include "component/component_contract.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QtTypes>
#include <QVector>

#include <optional>

namespace HyprShelld::Components {

struct CatalogEntry final {
    ComponentManifest manifest;
    QByteArray settingsSchema;
    QString packageDigest;

    friend bool operator==(const CatalogEntry &, const CatalogEntry &) = default;
};

struct CatalogLoadResult;

class SystemCatalog final {
public:
    [[nodiscard]] static CatalogLoadResult load(const QString &rootPath);
    [[nodiscard]] static CatalogLoadResult withUserEntries(
        SystemCatalog systemCatalog,
        QVector<CatalogEntry> userEntries
    );

    [[nodiscard]] const QString &catalogDigest() const;
    [[nodiscard]] QStringList componentIds() const;
    [[nodiscard]] const CatalogEntry *find(const QString &componentId) const;

private:
    QHash<QString, CatalogEntry> entries_;
    QString catalogDigest_;
};

struct CatalogLoadResult final {
    std::optional<SystemCatalog> catalog;
    QString error;

    [[nodiscard]] bool ok() const
    {
        return catalog.has_value() && error.isEmpty();
    }
};

} // namespace HyprShelld::Components
