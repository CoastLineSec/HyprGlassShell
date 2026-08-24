#pragma once

#include "activation_backend.h"
#include "configuration_authority.h"
#include "shared_border_reconciler.h"
#include "shared_spacing_reconciler.h"
#include "shared_visual_source.h"

#include <QByteArray>
#include <QDeadlineTimer>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <functional>
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
    Q_PROPERTY(QString DisplayConfirmationState READ displayConfirmationState)
    Q_PROPERTY(qulonglong DisplayConfirmationRevision READ displayConfirmationRevision)
    Q_PROPERTY(qulonglong DisplayConfirmationDeadlineMs READ displayConfirmationDeadlineMs)
    Q_PROPERTY(QString DisplayConfirmationGeneration READ displayConfirmationGeneration)
    Q_PROPERTY(QString SharedBorderSyncState READ sharedBorderSyncState)
    Q_PROPERTY(qulonglong SharedBorderSourceRevision READ sharedBorderSourceRevision)
    Q_PROPERTY(QString SharedBorderSyncError READ sharedBorderSyncError)
    Q_PROPERTY(QString SharedSpacingSyncState READ sharedSpacingSyncState)
    Q_PROPERTY(qulonglong SharedSpacingSourceRevision READ sharedSpacingSourceRevision)
    Q_PROPERTY(QString SharedSpacingSyncError READ sharedSpacingSyncError)

public:
    using DisplayDeadlineRemaining = std::function<qint64(
        const QDeadlineTimer &
    )>;
    using DisplayOwnerPresent = std::function<bool(const QString &, int)>;

    CompositorService(
        std::unique_ptr<ActivationBackend> activationBackend,
        QDBusConnection connection,
        QObject *parent = nullptr,
        DisplayDeadlineRemaining displayDeadlineRemaining = {},
        DisplayOwnerPresent displayOwnerPresent = {},
        std::unique_ptr<SharedVisualSource> sharedVisualSource = {},
        QByteArray inputDeviceInventoryEpoch = {}
    );

    // Must be called only after the process owns org.hyprshelld.Compositor1.
    // Configuration-authority-dependent methods remain fail-closed until it
    // succeeds. Authenticated input-device discovery is independent of that
    // authority.
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
    [[nodiscard]] QString displayConfirmationState() const;
    [[nodiscard]] qulonglong displayConfirmationRevision() const;
    [[nodiscard]] qulonglong displayConfirmationDeadlineMs() const;
    [[nodiscard]] QString displayConfirmationGeneration() const;
    [[nodiscard]] QString sharedBorderSyncState() const;
    [[nodiscard]] qulonglong sharedBorderSourceRevision() const;
    [[nodiscard]] QString sharedBorderSyncError() const;
    [[nodiscard]] QString sharedSpacingSyncState() const;
    [[nodiscard]] qulonglong sharedSpacingSourceRevision() const;
    [[nodiscard]] QString sharedSpacingSyncError() const;

public slots:
    QByteArray GetSnapshot(
        qulonglong &revision,
        QString &catalogDigest,
        QString &actionCatalogDigest
    ) const;
    QByteArray GetOptionCatalog(QString &catalogDigest) const;
    QByteArray GetActionCatalog(
        QString &actionCatalogDigest,
        QByteArray &configSchema,
        QString &configSchemaDigest
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
    QByteArray GetConnectedDisplays(qulonglong &observedAtMs);
    QByteArray GetConnectedInputDevices(qulonglong &observedAtMs);
    qulonglong PreviewDisplayConfiguration(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const QByteArray &profile,
        uint timeoutSeconds,
        QString &confirmationToken,
        qulonglong &deadlineMs,
        QString &generationDigest
    );
    QString GetPendingDisplayConfirmation(
        qulonglong &previewRevision,
        qulonglong &deadlineMs,
        QString &generationDigest
    );
    qulonglong ConfirmDisplayConfiguration(
        const QString &confirmationToken,
        QString &generationDigest
    );
    qulonglong RevertDisplayConfiguration(
        const QString &confirmationToken
    );
    void RetrySharedBorderSync();
    void RetrySharedSpacingSync();

signals:
    // Mirrors the exact changed map sent on
    // org.freedesktop.DBus.Properties.PropertiesChanged. This also gives
    // in-process observers a deterministic view of independently detected
    // entrypoint changes.
    void propertiesPublished(const QVariantMap &changed);

private slots:
    // Named slot keeps timeout ordering testable through QMetaObject without
    // exposing the confirmation timer or adding a production-only clock API.
    void handleDisplayConfirmationTimeout();
    void handleDisplayOwnerLoss(const QString &owner);
    void reconcileSharedVisual();

private:
    struct Completion final {
        bool success = false;
        QString errorCode;
        QString errorMessage;
        AuthoritySnapshot snapshot;
        ManagementStatus management;
    };

    struct DisplayConfirmation final {
        QString token;
        QString owner;
        QString generation;
        QString runtimeIdentity;
        QString topologyDigest;
        quint64 baseRevision = 0;
        quint64 previewRevision = 0;
        quint64 deadlineMs = 0;
        QDeadlineTimer deadline;
        ActivationReceipt receipt;
        Hyprland::DisplayProfile profile;
        QByteArray cachedTopologyDocument;
        quint64 cachedTopologyObservedAtMs = 0;
        bool liveRolledBack = false;
        bool terminationStarted = false;
        std::optional<ManagementStatus> rolledBackStatus;
    };

    enum class DisplayTerminalAction {
        Confirmed,
        Reverted,
        Expired,
    };

    struct DisplayTerminal final {
        QString token;
        QString owner;
        DisplayTerminalAction action = DisplayTerminalAction::Reverted;
        quint64 revision = 0;
        QString generation;
    };

    [[nodiscard]] bool checkMutationCatalogAuthority(
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        bool requireWritable
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
    [[nodiscard]] bool hasDisplayConfirmation() const;
    [[nodiscard]] QString callerIdentity() const;
    [[nodiscard]] bool displayOwnerStillPresent(
        const QString &owner,
        int maximumWaitMilliseconds
    ) const;
    [[nodiscard]] bool checkNoDisplayConfirmation();
    [[nodiscard]] bool validatePreparedGeneration(
        const AuthorityResult &prepared,
        QString &error
    ) const;
    [[nodiscard]] bool displayTopologyStillExact(
        const DisplayConfirmation &confirmation,
        QString &error,
        int maximumWaitMilliseconds = -1,
        Hyprland::ConnectedDisplayTopology *observedTopology = nullptr
    );
    [[nodiscard]] bool revertDisplayConfirmation(
        DisplayTerminalAction action,
        QString &error
    );
    void clearDisplayOwnerWatch();
    void appendDisplayProperties(QVariantMap &changed) const;
    void publishDisplayProperties();
    void scheduleSharedVisualReconcile();
    void sharedVisualSourceChanged();
    void setSharedBorderStatus(
        const QString &state,
        qulonglong sourceRevision,
        const QString &error = {}
    );
    void setSharedSpacingStatus(
        const QString &state,
        qulonglong sourceRevision,
        const QString &error = {}
    );
    void clearFailedSharedBorderAttempt();
    void clearFailedSharedSpacingAttempt();
    void failSharedBorder(const QString &error);
    void failSharedSpacing(const QString &error);
    void failSharedGroups(
        bool borderChanged,
        bool spacingChanged,
        const QString &error
    );
    [[nodiscard]] QString sharedBorderAttemptKey() const;
    [[nodiscard]] QString sharedSpacingAttemptKey() const;
    [[nodiscard]] bool ensureSharedBorderCatalog(QString &error);
    [[nodiscard]] bool ensureSharedSpacingCatalog(QString &error);
    [[nodiscard]] bool sharedVisualReplacementAllowed(
        const QByteArray &candidate,
        QString &error
    );
    [[nodiscard]] bool sharedVisualActivationAllowed(
        const QByteArray &candidate,
        QString &error
    );
    void acceptUnreconciled(
        const AuthoritySnapshot &snapshot,
        ManagementStatus management
    );
    void acceptState(
        const AuthoritySnapshot &snapshot,
        const ManagementStatus &management,
        bool includeDisplayProperties = false
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
    std::unique_ptr<SharedVisualSource> sharedVisualSource_;
    QDBusConnection connection_;
    AuthoritySnapshot snapshot_;
    ManagementStatus management_;
    QFileSystemWatcher managementWatcher_;
    QTimer managementPollTimer_;
    QString managementWatchPath_;
    QTimer displayConfirmationTimer_;
    std::unique_ptr<QDBusServiceWatcher> displayOwnerWatcher_;
    std::optional<DisplayConfirmation> displayConfirmation_;
    std::optional<DisplayTerminal> displayTerminal_;
    QString displayConfirmationState_ = QStringLiteral("idle");
    QByteArray inputDeviceInventoryEpoch_;
    DisplayDeadlineRemaining displayDeadlineRemaining_;
    DisplayOwnerPresent displayOwnerPresent_;
    SharedBorderReconciler sharedBorderReconciler_;
    SharedSpacingReconciler sharedSpacingReconciler_;
    std::optional<SharedVisualProjection> lastSharedVisualProjection_;
    std::optional<quint64> pendingSharedVisualApplyRevision_;
    bool pendingSharedBorderApply_ = false;
    bool pendingSharedSpacingApply_ = false;
    QString sharedBorderSyncState_ = QStringLiteral("unavailable");
    quint64 sharedBorderSourceRevision_ = 0;
    QString sharedBorderSyncError_ = QStringLiteral(
        "Shared visual settings are unavailable"
    );
    QString sharedSpacingSyncState_ = QStringLiteral("unavailable");
    quint64 sharedSpacingSourceRevision_ = 0;
    QString sharedSpacingSyncError_ = QStringLiteral(
        "Shared visual settings are unavailable"
    );
    QString failedSharedBorderAttempt_;
    QString failedSharedBorderError_;
    QString failedSharedSpacingAttempt_;
    QString failedSharedSpacingError_;
    bool failedGroupsShareOperation_ = false;
    quint64 sharedVisualAuthorityGeneration_ = 0;
    bool sharedVisualReconcileScheduled_ = false;
    bool sharedVisualReconcileRunning_ = false;
    bool forceSharedBorderRetry_ = false;
    bool forceSharedSpacingRetry_ = false;
};

} // namespace HyprShelld::Compositor
