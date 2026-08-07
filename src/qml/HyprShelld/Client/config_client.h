#pragma once

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class OrgHyprshelldConfig1Interface;
class QDBusServiceWatcher;

namespace HyprShelld {

class ConfigClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(uint barHeight READ barHeight NOTIFY barHeightChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString recoveryState READ recoveryState NOTIFY recoveryStateChanged)
    Q_PROPERTY(uint minimumBarHeight READ minimumBarHeight CONSTANT)
    Q_PROPERTY(uint maximumBarHeight READ maximumBarHeight CONSTANT)
    Q_PROPERTY(uint defaultBarHeight READ defaultBarHeight CONSTANT)
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit ConfigClient(QObject *parent = nullptr);
    ConfigClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] uint barHeight() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString recoveryState() const;
    [[nodiscard]] uint minimumBarHeight() const;
    [[nodiscard]] uint maximumBarHeight() const;
    [[nodiscard]] uint defaultBarHeight() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void setBarHeight(uint height);
    Q_INVOKABLE void resetBarHeight();
    Q_INVOKABLE void clearError();

signals:
    void availableChanged();
    void busyChanged();
    void barHeightChanged();
    void revisionChanged();
    void recoveryStateChanged();
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
    void refresh();
    void applyProperties(const QVariantMap &properties);
    void beginMutation(const QDBusPendingCall &call);
    void setAvailable(bool available);
    void setError(const QString &name, const QString &message);

    QDBusConnection connection_;
    OrgHyprshelldConfig1Interface *interface_ = nullptr;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    quint64 ownerGeneration_ = 0;
    int pendingOperations_ = 0;
    bool available_ = false;
    uint barHeight_ = 0;
    qulonglong revision_ = 0;
    QString recoveryState_;
    QString lastErrorName_;
    QString lastErrorMessage_;
};

} // namespace HyprShelld
