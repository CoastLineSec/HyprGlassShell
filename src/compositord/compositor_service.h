#pragma once

#include "activation_backend.h"
#include "configuration_authority.h"

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusContext>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <memory>

namespace HyprShelld::Compositor {

class CompositorService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.Compositor1")
    Q_PROPERTY(bool Available READ available)
    Q_PROPERTY(bool Writable READ writable)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString LoadState READ loadState)
    Q_PROPERTY(QString ManagementState READ managementState)
    Q_PROPERTY(QString EntrypointDigest READ entrypointDigest)
    Q_PROPERTY(QString CatalogDigest READ catalogDigest)
    Q_PROPERTY(QString ActionCatalogDigest READ actionCatalogDigest)
    Q_PROPERTY(qulonglong AppliedRevision READ appliedRevision)
    Q_PROPERTY(QString ApplyState READ applyState)
    Q_PROPERTY(QString RequiredActivation READ requiredActivation)
    Q_PROPERTY(QString GenerationDigest READ generationDigest)

public:
    CompositorService(
        std::unique_ptr<ActivationBackend> activationBackend,
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    // Must be called only after the process owns org.hyprshelld.Compositor1.
    // Until it succeeds every public method remains fail-closed.
    [[nodiscard]] bool initializeAuthority(
        std::unique_ptr<ConfigurationAuthority> authority,
        QString &error
    );

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool writable() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString loadState() const;
    [[nodiscard]] QString managementState() const;
    [[nodiscard]] QString entrypointDigest() const;
    [[nodiscard]] QString catalogDigest() const;
    [[nodiscard]] QString actionCatalogDigest() const;
    [[nodiscard]] qulonglong appliedRevision() const;
    [[nodiscard]] QString applyState() const;
    [[nodiscard]] QString requiredActivation() const;
    [[nodiscard]] QString generationDigest() const;

public slots:
    QByteArray GetSnapshot(
        qulonglong &revision,
        QString &catalogDigest,
        QString &actionCatalogDigest
    ) const;
    qulonglong ReplaceSnapshot(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const QByteArray &candidateSnapshot
    );
    qulonglong Apply(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        QString &generationDigest
    );
    qulonglong Recover(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        qulonglong &appliedRevision,
        QString &generationDigest
    );
    qulonglong AdoptManagedConfiguration(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const QString &expectedEntrypointDigest,
        QString &generationDigest,
        QString &entrypointDigest
    );

signals:
    // Mirrors the exact changed map sent on
    // org.freedesktop.DBus.Properties.PropertiesChanged. This also gives
    // in-process observers a deterministic view of independently detected
    // entrypoint changes.
    void propertiesPublished(const QVariantMap &changed);

private:
    struct Completion final {
        bool success = false;
        QString errorCode;
        QString errorMessage;
        AuthoritySnapshot snapshot;
        ManagementStatus management;
    };

    [[nodiscard]] bool checkMutationCatalogAuthority(
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest
    ) const;
    [[nodiscard]] bool checkMutationAuthority(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest
    ) const;
    [[nodiscard]] Completion completePrepared(
        AuthorityResult prepared,
        bool adoption,
        const QString &expectedEntrypointDigest = {},
        std::optional<ActivationRequirement> expectedRequirement = std::nullopt
    );
    void acceptState(
        const AuthoritySnapshot &snapshot,
        const ManagementStatus &management
    );
    void configureManagementMonitoring();
    void rearmManagementWatch();
    void refreshManagementStatus();
    void publishProperties(const QVariantMap &changed);
    void reportError(const QString &code, const QString &message) const;
    [[nodiscard]] static QString boundedErrorCode(
        const QString &code,
        const QString &fallback
    );

    std::unique_ptr<ConfigurationAuthority> authority_;
    std::unique_ptr<ActivationBackend> activationBackend_;
    QDBusConnection connection_;
    AuthoritySnapshot snapshot_;
    ManagementStatus management_;
    QFileSystemWatcher managementWatcher_;
    QTimer managementPollTimer_;
    QString managementWatchPath_;
};

} // namespace HyprShelld::Compositor
