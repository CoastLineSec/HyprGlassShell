#pragma once

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class OrgHyprshelldCoordinator1Interface;
class QDBusServiceWatcher;

namespace HyprShelld {

class CoordinatorClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool healthy READ healthy NOTIFY healthyChanged)
    Q_PROPERTY(QStringList failedUnits READ failedUnits NOTIFY failedUnitsChanged)
    Q_PROPERTY(QString failureSummary READ failureSummary NOTIFY failureSummaryChanged)
    Q_PROPERTY(QString restartingUnit READ restartingUnit NOTIFY restartingUnitChanged)
    Q_PROPERTY(QString lastErrorUnit READ lastErrorUnit NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit CoordinatorClient(QObject *parent = nullptr);
    CoordinatorClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool healthy() const;
    [[nodiscard]] QStringList failedUnits() const;
    [[nodiscard]] QString failureSummary() const;
    [[nodiscard]] QString restartingUnit() const;
    [[nodiscard]] QString lastErrorUnit() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void restartComponent(const QString &unitName);
    Q_INVOKABLE void clearError();

signals:
    void availableChanged();
    void busyChanged();
    void healthyChanged();
    void failedUnitsChanged();
    void failureSummaryChanged();
    void healthChanged();
    void restartingUnitChanged();
    void lastErrorChanged();
    void operationFailed(const QString &name, const QString &message);
    void persistentFailureAdded(
        const QString &summary,
        const QStringList &unitNames
    );

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
    void refresh();
    [[nodiscard]] bool applyProperties(
        const QVariantMap &properties,
        bool requireComplete
    );
    void beginRestart(
        const QString &unitName,
        const QDBusPendingCall &call,
        quint64 generation,
        bool failedWhenRequested,
        quint64 failureEpoch
    );
    void finishRestart(const QString &unitName);
    void setAvailable(bool available);
    void setError(
        const QString &unitName,
        const QString &name,
        const QString &message
    );

    QDBusConnection connection_;
    OrgHyprshelldCoordinator1Interface *interface_ = nullptr;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    quint64 ownerGeneration_ = 0;
    quint64 refreshGeneration_ = 0;
    quint64 failureEpoch_ = 0;
    QHash<QString, quint64> failureEpochs_;
    QStringList pendingRestarts_;
    bool available_ = false;
    bool healthy_ = true;
    QStringList failedUnits_;
    QString failureSummary_;
    QString lastErrorUnit_;
    QString lastErrorName_;
    QString lastErrorMessage_;
};

} // namespace HyprShelld
