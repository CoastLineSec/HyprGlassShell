#pragma once

#include "system_catalog.h"

#include <QByteArray>
#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QStringList>

namespace HyprShelld {

class ComponentManagerService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentManager1")
    Q_PROPERTY(QString CatalogDigest READ catalogDigest)

public:
    explicit ComponentManagerService(
        Components::SystemCatalog catalog,
        QObject *parent = nullptr
    );

    [[nodiscard]] QString catalogDigest() const;

public slots:
    QStringList ListComponents(QString &catalogDigest) const;

    uint GetComponent(
        const QString &componentId,
        const QString &expectedCatalogDigest,
        QString &componentType,
        QString &version,
        QString &name,
        QString &description,
        QStringList &authorNames,
        QStringList &authorEmails,
        QStringList &authorHomepages,
        QString &license,
        QString &homepage,
        QString &source,
        QString &issues,
        QString &componentApiVersion,
        QString &runtimeKind,
        QString &runtimeFactory,
        QString &runtimeEntryPoint,
        QStringList &runtimeArguments,
        QByteArray &settingsSchema,
        QStringList &capabilityIds,
        QStringList &capabilityReasons,
        QStringList &dependencyIds,
        QStringList &dependencyVersionRequirements,
        QString &packageDigest,
        QString &origin,
        bool &removable
    ) const;

private:
    void reportError(const QString &name, const QString &message) const;

    Components::SystemCatalog catalog_;
};

} // namespace HyprShelld
