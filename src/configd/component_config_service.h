#pragma once

#include "component_store.h"

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace HyprShelld {

class ComponentConfigService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentConfig1")
    Q_PROPERTY(bool Available READ available)
    Q_PROPERTY(bool CatalogAvailable READ catalogAvailable)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString CatalogDigest READ catalogDigest)
    Q_PROPERTY(QString LoadState READ loadState)

public:
    ComponentConfigService(
        ComponentStore store,
        QDBusConnection connection,
        std::optional<LegacyWorkspaceSettings> legacyWorkspaceSettings =
            std::nullopt,
        QObject *parent = nullptr
    );

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool catalogAvailable() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString catalogDigest() const;
    [[nodiscard]] QString loadState() const;

    void applyCatalog(Components::ConfigurationCatalog catalog);
    void setCatalogUnavailable();

signals:
    void authoritativeSnapshotEstablished();

public slots:
    QByteArray GetSnapshot(
        qulonglong &revision,
        QString &catalogDigest
    ) const;
    qulonglong ReplaceSnapshot(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QByteArray &candidateSnapshot
    );

private:
    void reportError(const QString &name, const QString &message) const;
    void publishProperties(const QVariantMap &changed) const;
    [[nodiscard]] bool preservesDormantState(
        const Components::ComponentConfiguration &candidate,
        QString &error
    ) const;

    ComponentStore store_;
    Components::ComponentConfiguration state_;
    Components::ConfigurationCatalog catalog_;
    QDBusConnection connection_;
    QString catalogDigest_;
    QString loadState_ = QStringLiteral("unavailable");
    bool available_ = false;
    bool writable_ = false;
    bool catalogAvailable_ = false;
    bool loadedOnce_ = false;
    std::optional<LegacyWorkspaceSettings> legacyWorkspaceSettings_;
};

} // namespace HyprShelld
