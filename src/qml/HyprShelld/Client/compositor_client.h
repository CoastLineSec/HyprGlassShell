#pragma once

#include <QDBusConnection>
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;
class QDBusMessage;

namespace HyprShelld {

class CompositorOptionCatalog;

class CompositorClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool writable READ writable NOTIFY writableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyOperation READ busyOperation NOTIFY busyOperationChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY snapshotChanged)
    Q_PROPERTY(QString revisionToken READ revisionToken NOTIFY snapshotChanged)
    Q_PROPERTY(QString loadState READ loadState NOTIFY loadStateChanged)
    Q_PROPERTY(QString managementState READ managementState NOTIFY managementStateChanged)
    Q_PROPERTY(QString entrypointDigest READ entrypointDigest NOTIFY managementStateChanged)
    Q_PROPERTY(QString catalogDigest READ catalogDigest NOTIFY catalogDigestChanged)
    Q_PROPERTY(QString actionCatalogDigest READ actionCatalogDigest NOTIFY catalogDigestChanged)
    Q_PROPERTY(qulonglong appliedRevision READ appliedRevision NOTIFY applyStateChanged)
    Q_PROPERTY(QString applyState READ applyState NOTIFY applyStateChanged)
    Q_PROPERTY(QString requiredActivation READ requiredActivation NOTIFY applyStateChanged)
    Q_PROPERTY(QString generationDigest READ generationDigest NOTIFY applyStateChanged)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool catalogAvailable READ catalogAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool displayDiscoveryAvailable READ displayDiscoveryAvailable NOTIFY displayDiscoveryAvailableChanged)
    Q_PROPERTY(bool appearanceAvailable READ appearanceAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool retryApplyAvailable READ retryApplyAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList appearanceOptions READ appearanceOptions NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantMap appearanceValues READ appearanceValues NOTIFY appearanceChanged)
    Q_PROPERTY(QString appearanceErrorName READ appearanceErrorName NOTIFY appearanceChanged)
    Q_PROPERTY(QString appearanceErrorMessage READ appearanceErrorMessage NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList connectedDisplays READ connectedDisplays NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(qulonglong displaysObservedAtMs READ displaysObservedAtMs NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(QString topologyDigest READ topologyDigest NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(QString displayConfirmationState READ displayConfirmationState NOTIFY displayConfirmationChanged)
    Q_PROPERTY(qulonglong displayConfirmationRevision READ displayConfirmationRevision NOTIFY displayConfirmationChanged)
    Q_PROPERTY(qulonglong displayConfirmationDeadlineMs READ displayConfirmationDeadlineMs NOTIFY displayConfirmationChanged)
    Q_PROPERTY(QString displayConfirmationGeneration READ displayConfirmationGeneration NOTIFY displayConfirmationChanged)
    Q_PROPERTY(bool displayConfirmationOwned READ displayConfirmationOwned NOTIFY displayConfirmationChanged)
    Q_PROPERTY(QString sharedBorderSyncState READ sharedBorderSyncState NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(qulonglong sharedBorderSourceRevision READ sharedBorderSourceRevision NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(QString sharedBorderSourceRevisionToken READ sharedBorderSourceRevisionToken NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(QString sharedBorderSyncError READ sharedBorderSyncError NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit CompositorClient(QObject *parent = nullptr);
    CompositorClient(QDBusConnection connection, QObject *parent);
    ~CompositorClient() override;

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool writable() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString busyOperation() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString revisionToken() const;
    [[nodiscard]] QString loadState() const;
    [[nodiscard]] QString managementState() const;
    [[nodiscard]] QString entrypointDigest() const;
    [[nodiscard]] QString catalogDigest() const;
    [[nodiscard]] QString actionCatalogDigest() const;
    [[nodiscard]] qulonglong appliedRevision() const;
    [[nodiscard]] QString applyState() const;
    [[nodiscard]] QString requiredActivation() const;
    [[nodiscard]] QString generationDigest() const;
    [[nodiscard]] QVariantMap snapshot() const;
    [[nodiscard]] bool catalogAvailable() const;
    [[nodiscard]] bool displayDiscoveryAvailable() const;
    [[nodiscard]] bool appearanceAvailable() const;
    [[nodiscard]] bool retryApplyAvailable() const;
    [[nodiscard]] bool recoveryAvailable() const;
    [[nodiscard]] QVariantList appearanceOptions() const;
    [[nodiscard]] QVariantMap appearanceValues() const;
    [[nodiscard]] QString appearanceErrorName() const;
    [[nodiscard]] QString appearanceErrorMessage() const;
    [[nodiscard]] QVariantList connectedDisplays() const;
    [[nodiscard]] qulonglong displaysObservedAtMs() const;
    [[nodiscard]] QString topologyDigest() const;
    [[nodiscard]] QString displayConfirmationState() const;
    [[nodiscard]] qulonglong displayConfirmationRevision() const;
    [[nodiscard]] qulonglong displayConfirmationDeadlineMs() const;
    [[nodiscard]] QString displayConfirmationGeneration() const;
    [[nodiscard]] bool displayConfirmationOwned() const;
    [[nodiscard]] QString sharedBorderSyncState() const;
    [[nodiscard]] qulonglong sharedBorderSourceRevision() const;
    [[nodiscard]] QString sharedBorderSourceRevisionToken() const;
    [[nodiscard]] QString sharedBorderSyncError() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void adoptManagedConfiguration();
    Q_INVOKABLE void applyConfiguration();
    Q_INVOKABLE void saveAppearance(const QVariantMap &values);
    Q_INVOKABLE void retryApply();
    Q_INVOKABLE void recoverConfiguration();
    Q_INVOKABLE void previewDisplayConfiguration(
        const QVariantList &outputs,
        uint timeoutSeconds = 15
    );
    Q_INVOKABLE void confirmDisplayConfiguration();
    Q_INVOKABLE void revertDisplayConfiguration();
    Q_INVOKABLE void retrySharedBorderSync();
    Q_INVOKABLE void clearError();

signals:
    void availableChanged();
    void writableChanged();
    void busyChanged();
    void busyOperationChanged();
    void snapshotChanged();
    void appearanceChanged();
    void displayDiscoveryAvailableChanged();
    void loadStateChanged();
    void managementStateChanged();
    void catalogDigestChanged();
    void applyStateChanged();
    void connectedDisplaysChanged();
    void displayConfirmationChanged();
    void sharedBorderSyncChanged();
    void lastErrorChanged();
    void operationFailed(const QString &name, const QString &message);

private slots:
    void propertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );
    void serviceOwnerChanged(
        const QString &name,
        const QString &oldOwner,
        const QString &newOwner
    );

private:
    enum class Mutation {
        Adopt,
        Apply,
        Preview,
        Confirm,
        Revert,
        Recover,
        SharedBorderSync,
    };

    struct AppearanceSaveRequest final {
        QByteArray candidate;
        qulonglong expectedRevision = 0;
        QString catalogDigest;
        QString actionCatalogDigest;
    };

    struct ApplyRequest final {
        qulonglong revision = 0;
        QString catalogDigest;
        QString actionCatalogDigest;
    };

    void fetchSnapshot(quint64 generation);
    void fetchOptionCatalog(quint64 generation);
    void fetchConnectedDisplays(quint64 generation);
    void fetchPendingDisplayConfirmation(quint64 generation);
    [[nodiscard]] bool applyProperties(
        const QVariantMap &properties,
        bool requireAll = false
    );
    void beginMutation(Mutation mutation, const QDBusMessage &message, int timeoutMs);
    void sendAppearanceReplace(
        const AppearanceSaveRequest &request,
        bool retry
    );
    void verifyAppearanceReplacement(const AppearanceSaveRequest &request);
    void sendApplyRequest(const ApplyRequest &request, bool retry);
    void reconcileApplyOutcome(const ApplyRequest &request);
    void finishMutation();
    void finishHydration(bool accepted);
    void updateAppearanceProjection();
    void setAvailable(bool available);
    void setCatalogAvailable(bool available);
    void setDisplayDiscoveryAvailable(bool available);
    void setBusy(bool busy);
    void setBusyOperation(const QString &operation);
    void setAppearanceError(const QString &name, const QString &message);
    void setError(const QString &name, const QString &message);

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    QVariantMap snapshot_;
    QJsonObject snapshotObject_;
    std::unique_ptr<CompositorOptionCatalog> optionCatalog_;
    QVariantMap appearanceValues_;
    QVariantList connectedDisplays_;
    quint64 ownerGeneration_ = 0;
    quint64 refreshGeneration_ = 0;
    qulonglong advertisedRevision_ = 0;
    qulonglong revision_ = 0;
    qulonglong appliedRevision_ = 0;
    qulonglong displaysObservedAtMs_ = 0;
    qulonglong displayConfirmationRevision_ = 0;
    qulonglong displayConfirmationDeadlineMs_ = 0;
    qulonglong sharedBorderSourceRevision_ = 0;
    QString loadState_ = QStringLiteral("unavailable");
    QString managementState_ = QStringLiteral("unmanaged");
    QString entrypointDigest_;
    QString advertisedCatalogDigest_;
    QString advertisedActionCatalogDigest_;
    QString catalogDigest_;
    QString actionCatalogDigest_;
    QString topologyDigest_;
    QString applyState_ = QStringLiteral("unavailable");
    QString requiredActivation_ = QStringLiteral("none");
    QString generationDigest_;
    QString displayConfirmationState_ = QStringLiteral("idle");
    QString displayConfirmationToken_;
    QString displayConfirmationGeneration_;
    QString sharedBorderSyncState_ = QStringLiteral("unavailable");
    QString sharedBorderSyncError_ = QStringLiteral(
        "Shared visual settings are unavailable"
    );
    QString lastErrorName_;
    QString lastErrorMessage_;
    QString busyOperation_;
    QString appearanceErrorName_;
    QString appearanceErrorMessage_;
    bool advertisedAvailable_ = false;
    bool writable_ = false;
    bool available_ = false;
    bool catalogAvailable_ = false;
    bool displayDiscoveryAvailable_ = false;
    bool appearanceProjectionValid_ = false;
    bool busy_ = false;
    bool refreshQueued_ = false;
    bool displayConfirmationOwned_ = false;
    quint64 catalogOwnerGeneration_ = 0;
};

} // namespace HyprShelld
