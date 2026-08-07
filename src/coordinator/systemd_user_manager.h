#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QDBusPendingCallWatcher;

namespace HyprShelld {

struct SystemdUnitRecord {
    QString name;
    QString description;
    QString loadState;
    QString activeState;
    QString subState;
    QString following;
    QDBusObjectPath unitPath;
    quint32 jobId = 0;
    QString jobType;
    QDBusObjectPath jobPath;
};

using SystemdUnitRecords = QList<SystemdUnitRecord>;

QDBusArgument &operator<<(QDBusArgument &argument, const SystemdUnitRecord &record);
const QDBusArgument &operator>>(const QDBusArgument &argument, SystemdUnitRecord &record);

class SystemdUserManager final : public QObject {
    Q_OBJECT

public:
    using RestartCallback = std::function<void(bool accepted, const QString &error)>;

    explicit SystemdUserManager(
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    [[nodiscard]] bool start(QString &error);
    void restartUnit(const QString &unitName, RestartCallback callback);

signals:
    void snapshotReady(const QHash<QString, QString> &activeStates);
    void monitoringFailed(const QString &error);

private slots:
    void managerUnitNew(const QString &unitName, const QDBusObjectPath &unitPath);
    void managerUnitRemoved(const QString &unitName, const QDBusObjectPath &unitPath);
    void managerJobRemoved(
        quint32 jobId,
        const QDBusObjectPath &jobPath,
        const QString &unitName,
        const QString &result
    );
    void unitPropertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );

private:
    void subscribe();
    void requestSnapshot();
    void scheduleRefresh();
    void snapshotFinished(QDBusPendingCallWatcher *watcher, quint64 serial);
    [[nodiscard]] bool updateUnitConnections(
        const QHash<QString, QString> &paths,
        QString &error
    );
    void fail(const QString &error);

    QDBusConnection connection_;
    QHash<QString, QString> unitPaths_;
    QDBusPendingCallWatcher *subscribeWatcher_ = nullptr;
    QDBusPendingCallWatcher *snapshotWatcher_ = nullptr;
    quint64 changeSerial_ = 0;
    bool refreshScheduled_ = false;
    bool started_ = false;
    bool stopped_ = false;
};

} // namespace HyprShelld

Q_DECLARE_METATYPE(HyprShelld::SystemdUnitRecord)
Q_DECLARE_METATYPE(HyprShelld::SystemdUnitRecords)
