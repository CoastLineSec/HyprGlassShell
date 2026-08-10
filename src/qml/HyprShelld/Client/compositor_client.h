#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;
class QDBusMessage;

namespace HyprShelld {

class CompositorClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool writable READ writable NOTIFY writableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY snapshotChanged)
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
    Q_PROPERTY(QVariantList connectedDisplays READ connectedDisplays NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(qulonglong displaysObservedAtMs READ displaysObservedAtMs NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(QString topologyDigest READ topologyDigest NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(QString displayConfirmationState READ displayConfirmationState NOTIFY displayConfirmationChanged)
    Q_PROPERTY(qulonglong displayConfirmationRevision READ displayConfirmationRevision NOTIFY displayConfirmationChanged)
    Q_PROPERTY(qulonglong displayConfirmationDeadlineMs READ displayConfirmationDeadlineMs NOTIFY displayConfirmationChanged)
    Q_PROPERTY(QString displayConfirmationGeneration READ displayConfirmationGeneration NOTIFY displayConfirmationChanged)
    Q_PROPERTY(bool displayConfirmationOwned READ displayConfirmationOwned NOTIFY displayConfirmationChanged)
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit CompositorClient(QObject *parent = nullptr);
    CompositorClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool writable() const;
    [[nodiscard]] bool busy() const;
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
    [[nodiscard]] QVariantMap snapshot() const;
    [[nodiscard]] QVariantList connectedDisplays() const;
    [[nodiscard]] qulonglong displaysObservedAtMs() const;
    [[nodiscard]] QString topologyDigest() const;
    [[nodiscard]] QString displayConfirmationState() const;
    [[nodiscard]] qulonglong displayConfirmationRevision() const;
    [[nodiscard]] qulonglong displayConfirmationDeadlineMs() const;
    [[nodiscard]] QString displayConfirmationGeneration() const;
    [[nodiscard]] bool displayConfirmationOwned() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void adoptManagedConfiguration();
    Q_INVOKABLE void applyConfiguration();
    Q_INVOKABLE void previewDisplayConfiguration(
        const QVariantList &outputs,
        uint timeoutSeconds = 15
    );
    Q_INVOKABLE void confirmDisplayConfiguration();
    Q_INVOKABLE void revertDisplayConfiguration();
    Q_INVOKABLE void clearError();

signals:
    void availableChanged();
    void writableChanged();
    void busyChanged();
    void snapshotChanged();
    void loadStateChanged();
    void managementStateChanged();
    void catalogDigestChanged();
    void applyStateChanged();
    void connectedDisplaysChanged();
    void displayConfirmationChanged();
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
    };

    void fetchSnapshot(quint64 generation);
    void fetchConnectedDisplays(quint64 generation);
    void fetchPendingDisplayConfirmation(quint64 generation);
    [[nodiscard]] bool applyProperties(
        const QVariantMap &properties,
        bool requireAll = false
    );
    void beginMutation(Mutation mutation, const QDBusMessage &message, int timeoutMs);
    void finishMutation();
    void finishHydration(bool accepted);
    void setAvailable(bool available);
    void setBusy(bool busy);
    void setError(const QString &name, const QString &message);

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    QVariantMap snapshot_;
    QVariantList connectedDisplays_;
    quint64 ownerGeneration_ = 0;
    quint64 refreshGeneration_ = 0;
    qulonglong advertisedRevision_ = 0;
    qulonglong revision_ = 0;
    qulonglong appliedRevision_ = 0;
    qulonglong displaysObservedAtMs_ = 0;
    qulonglong displayConfirmationRevision_ = 0;
    qulonglong displayConfirmationDeadlineMs_ = 0;
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
    QString lastErrorName_;
    QString lastErrorMessage_;
    bool advertisedAvailable_ = false;
    bool writable_ = false;
    bool available_ = false;
    bool busy_ = false;
    bool refreshQueued_ = false;
    bool displayConfirmationOwned_ = false;
};

} // namespace HyprShelld
