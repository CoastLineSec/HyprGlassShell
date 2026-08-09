#pragma once

#include "system_catalog.h"
#include "user_package_store.h"

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace HyprShelld { class ComponentInspectionSessions; }

namespace HyprShelld {

class ComponentManagerService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentManager1")
    Q_PROPERTY(QString CatalogDigest READ catalogDigest NOTIFY catalogDigestChanged)

public:
    ~ComponentManagerService() override;

    explicit ComponentManagerService(
        Components::SystemCatalog catalog,
        QObject *parent = nullptr
    );
    ComponentManagerService(
        Components::SystemCatalog systemCatalog,
        std::unique_ptr<Components::UserPackageStore> userPackageStore,
        std::unique_ptr<ComponentInspectionSessions> inspectionSessions,
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    void initializePackageManagement(
        std::unique_ptr<Components::UserPackageStore> userPackageStore,
        std::unique_ptr<ComponentInspectionSessions> inspectionSessions
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

    QString BeginPackageInspection(
        const QDBusUnixFileDescriptor &packageFile
    );
    void CancelPackageInspection(const QString &inspectionToken);
    QString InstallInspectedPackage(
        const QString &inspectionToken,
        const QString &expectedArchiveDigest,
        const QString &expectedCatalogDigest,
        QString &packageDigest,
        QString &catalogDigest
    );
    QString RemovePackage(
        const QString &componentId,
        const QString &expectedPackageDigest,
        const QString &expectedCatalogDigest
    );

signals:
    void catalogDigestChanged();

private slots:
    void inspectionFinished(
        const QString &sender,
        const QString &token,
        const QByteArray &reportBytes,
        const QString &spoolPath,
        const QString &materializedPath,
        const QString &errorName,
        const QString &errorMessage
    );
    void nameOwnerChanged(
        const QString &name,
        const QString &oldOwner,
        const QString &newOwner
    );

private:
    void reportError(const QString &name, const QString &message) const;
    void publishCatalogDigestChanged() const;
    [[nodiscard]] bool reloadUserCatalog(
        QString &error,
        bool publishDigestChange = false
    );
    [[nodiscard]] bool acceptUserCatalog(
        QVector<Components::CatalogEntry> entries,
        QString &error,
        bool publishDigestChange
    );
    [[nodiscard]] QString caller() const;
    [[nodiscard]] static QString managerErrorName(const QString &suffix);

    Components::SystemCatalog systemCatalog_;
    Components::SystemCatalog catalog_;
    std::unique_ptr<Components::UserPackageStore> userPackageStore_;
    std::unique_ptr<ComponentInspectionSessions> inspectionSessions_;
    QDBusConnection connection_;
};

} // namespace HyprShelld
