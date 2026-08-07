#include "systemd_user_manager.h"

#include "coordinator_policy.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTimer>
#include <QVariantMap>

#include <utility>

namespace HyprShelld {
namespace {

const QString systemdService = QStringLiteral("org.freedesktop.systemd1");
const QString managerPath = QStringLiteral("/org/freedesktop/systemd1");
const QString managerInterface = QStringLiteral("org.freedesktop.systemd1.Manager");
const QString unitInterface = QStringLiteral("org.freedesktop.systemd1.Unit");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
constexpr int systemdCallTimeout = 5000;

QString dbusFailure(const QString &operation, const QDBusMessage &reply)
{
    return QStringLiteral("%1 failed: %2: %3")
        .arg(operation, reply.errorName(), reply.errorMessage());
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &argument, const SystemdUnitRecord &record)
{
    argument.beginStructure();
    argument << record.name
             << record.description
             << record.loadState
             << record.activeState
             << record.subState
             << record.following
             << record.unitPath
             << record.jobId
             << record.jobType
             << record.jobPath;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SystemdUnitRecord &record)
{
    argument.beginStructure();
    argument >> record.name
             >> record.description
             >> record.loadState
             >> record.activeState
             >> record.subState
             >> record.following
             >> record.unitPath
             >> record.jobId
             >> record.jobType
             >> record.jobPath;
    argument.endStructure();
    return argument;
}

SystemdUserManager::SystemdUserManager(
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , connection_(std::move(connection))
{
    qDBusRegisterMetaType<SystemdUnitRecord>();
    qDBusRegisterMetaType<SystemdUnitRecords>();
}

bool SystemdUserManager::start(QString &error)
{
    if (started_) {
        error = QStringLiteral("systemd monitoring has already started");
        return false;
    }

    if (!connection_.isConnected()) {
        error = connection_.lastError().message();
        return false;
    }

    const bool connected = connection_.connect(
        systemdService,
        managerPath,
        managerInterface,
        QStringLiteral("UnitNew"),
        this,
        SLOT(managerUnitNew(QString,QDBusObjectPath))
    ) && connection_.connect(
        systemdService,
        managerPath,
        managerInterface,
        QStringLiteral("UnitRemoved"),
        this,
        SLOT(managerUnitRemoved(QString,QDBusObjectPath))
    ) && connection_.connect(
        systemdService,
        managerPath,
        managerInterface,
        QStringLiteral("JobRemoved"),
        this,
        SLOT(managerJobRemoved(quint32,QDBusObjectPath,QString,QString))
    );

    if (!connected) {
        error = QStringLiteral("Cannot monitor the systemd user manager");
        return false;
    }

    started_ = true;
    subscribe();
    return true;
}

void SystemdUserManager::restartUnit(
    const QString &unitName,
    RestartCallback callback
)
{
    auto reset = QDBusMessage::createMethodCall(
        systemdService,
        managerPath,
        managerInterface,
        QStringLiteral("ResetFailedUnit")
    );
    reset << unitName;

    auto *resetWatcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(reset, systemdCallTimeout),
        this
    );
    connect(
        resetWatcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, unitName, callback = std::move(callback)](
            QDBusPendingCallWatcher *watcher
        ) mutable {
            const auto resetReply = watcher->reply();
            watcher->deleteLater();

            if (resetReply.type() == QDBusMessage::ErrorMessage) {
                callback(
                    false,
                    dbusFailure(QStringLiteral("ResetFailedUnit"), resetReply)
                );
                return;
            }
            if (resetReply.type() != QDBusMessage::ReplyMessage
                || !resetReply.arguments().isEmpty()) {
                callback(
                    false,
                    QStringLiteral("ResetFailedUnit returned an invalid reply")
                );
                return;
            }

            auto restart = QDBusMessage::createMethodCall(
                systemdService,
                managerPath,
                managerInterface,
                QStringLiteral("RestartUnit")
            );
            restart << unitName << QStringLiteral("replace");

            auto *restartWatcher = new QDBusPendingCallWatcher(
                connection_.asyncCall(restart, systemdCallTimeout),
                this
            );
            connect(
                restartWatcher,
                &QDBusPendingCallWatcher::finished,
                this,
                [callback = std::move(callback)](
                    QDBusPendingCallWatcher *restartWatcher
                ) {
                    const auto restartReply = restartWatcher->reply();
                    restartWatcher->deleteLater();

                    if (restartReply.type() == QDBusMessage::ErrorMessage) {
                        callback(
                            false,
                            dbusFailure(
                                QStringLiteral("RestartUnit"),
                                restartReply
                            )
                        );
                        return;
                    }
                    if (restartReply.type() != QDBusMessage::ReplyMessage
                        || restartReply.arguments().size() != 1
                        || !restartReply.arguments().constFirst()
                                .canConvert<QDBusObjectPath>()
                        || restartReply.arguments().constFirst()
                                .value<QDBusObjectPath>().path().isEmpty()
                        || restartReply.arguments().constFirst()
                                .value<QDBusObjectPath>().path()
                            == QStringLiteral("/")) {
                        callback(
                            false,
                            QStringLiteral("RestartUnit returned an invalid reply")
                        );
                        return;
                    }

                    callback(true, {});
                }
            );
        }
    );
}

void SystemdUserManager::managerUnitNew(
    const QString &unitName,
    const QDBusObjectPath &unitPath
)
{
    if (CoordinatorPolicy::isKnown(unitName)
        && unitPaths_.contains(unitName)
        && unitPaths_.value(unitName) != unitPath.path()) {
        scheduleRefresh();
    }
}

void SystemdUserManager::managerUnitRemoved(
    const QString &unitName,
    const QDBusObjectPath &unitPath
)
{
    if (CoordinatorPolicy::isKnown(unitName)
        && unitPaths_.contains(unitName)
        && unitPaths_.value(unitName) != unitPath.path()) {
        scheduleRefresh();
    }
}

void SystemdUserManager::managerJobRemoved(
    quint32 jobId,
    const QDBusObjectPath &jobPath,
    const QString &unitName,
    const QString &result
)
{
    Q_UNUSED(jobId)
    Q_UNUSED(jobPath)
    Q_UNUSED(result)
    if (CoordinatorPolicy::isKnown(unitName)) {
        scheduleRefresh();
    }
}

void SystemdUserManager::unitPropertiesChanged(
    const QString &interfaceName,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (interfaceName == unitInterface
        && (changed.contains(QStringLiteral("ActiveState"))
            || invalidated.contains(QStringLiteral("ActiveState")))) {
        scheduleRefresh();
    }
}

void SystemdUserManager::subscribe()
{
    auto message = QDBusMessage::createMethodCall(
        systemdService,
        managerPath,
        managerInterface,
        QStringLiteral("Subscribe")
    );
    subscribeWatcher_ = new QDBusPendingCallWatcher(
        connection_.asyncCall(message),
        this
    );

    connect(
        subscribeWatcher_,
        &QDBusPendingCallWatcher::finished,
        this,
        [this](QDBusPendingCallWatcher *watcher) {
            QDBusPendingReply<> reply = *watcher;
            subscribeWatcher_ = nullptr;
            watcher->deleteLater();

            if (reply.isError()) {
                fail(
                    QStringLiteral("Cannot subscribe to the systemd user manager: %1: %2")
                        .arg(reply.error().name(), reply.error().message())
                );
                return;
            }

            requestSnapshot();
        }
    );
}

void SystemdUserManager::requestSnapshot()
{
    refreshScheduled_ = false;
    if (stopped_ || snapshotWatcher_ != nullptr) {
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        systemdService,
        managerPath,
        managerInterface,
        QStringLiteral("ListUnitsByNames")
    );
    message << CoordinatorPolicy::allowedUnits();

    const auto serial = changeSerial_;
    snapshotWatcher_ = new QDBusPendingCallWatcher(
        connection_.asyncCall(message),
        this
    );
    connect(
        snapshotWatcher_,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, serial](QDBusPendingCallWatcher *watcher) {
            snapshotFinished(watcher, serial);
        }
    );
}

void SystemdUserManager::scheduleRefresh()
{
    if (stopped_) {
        return;
    }

    ++changeSerial_;
    if (snapshotWatcher_ != nullptr || refreshScheduled_) {
        return;
    }

    refreshScheduled_ = true;
    QTimer::singleShot(0, this, &SystemdUserManager::requestSnapshot);
}

void SystemdUserManager::snapshotFinished(
    QDBusPendingCallWatcher *watcher,
    quint64 serial
)
{
    QDBusPendingReply<SystemdUnitRecords> reply = *watcher;
    snapshotWatcher_ = nullptr;
    watcher->deleteLater();

    if (stopped_) {
        return;
    }

    if (reply.isError()) {
        fail(
            QStringLiteral("Cannot read systemd unit state: %1: %2")
                .arg(reply.error().name(), reply.error().message())
        );
        return;
    }

    if (serial != changeSerial_) {
        requestSnapshot();
        return;
    }

    QHash<QString, QString> states;
    QHash<QString, QString> paths;
    for (const auto &record : reply.value()) {
        if (!CoordinatorPolicy::isKnown(record.name) || states.contains(record.name)) {
            fail(QStringLiteral("systemd returned an invalid unit snapshot"));
            return;
        }
        if (record.unitPath.path().isEmpty()) {
            fail(QStringLiteral("systemd returned an empty unit object path"));
            return;
        }
        states.insert(record.name, record.activeState);
        paths.insert(record.name, record.unitPath.path());
    }

    if (states.size() != CoordinatorPolicy::allowedUnits().size()) {
        fail(QStringLiteral("systemd returned an incomplete unit snapshot"));
        return;
    }

    QString error;
    const bool pathsChanged = paths != unitPaths_;
    if (pathsChanged && !updateUnitConnections(paths, error)) {
        fail(error);
        return;
    }

    if (pathsChanged) {
        requestSnapshot();
        return;
    }

    emit snapshotReady(states);
}

bool SystemdUserManager::updateUnitConnections(
    const QHash<QString, QString> &paths,
    QString &error
)
{
    for (auto it = unitPaths_.cbegin(); it != unitPaths_.cend(); ++it) {
        if (paths.value(it.key()) == it.value()) {
            continue;
        }
        connection_.disconnect(
            systemdService,
            it.value(),
            propertiesInterface,
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(unitPropertiesChanged(QString,QVariantMap,QStringList))
        );
    }

    for (auto it = paths.cbegin(); it != paths.cend(); ++it) {
        if (unitPaths_.value(it.key()) == it.value()) {
            continue;
        }
        if (!connection_.connect(
                systemdService,
                it.value(),
                propertiesInterface,
                QStringLiteral("PropertiesChanged"),
                this,
                SLOT(unitPropertiesChanged(QString,QVariantMap,QStringList))
            )) {
            error = QStringLiteral("Cannot monitor systemd unit %1").arg(it.key());
            return false;
        }
    }

    unitPaths_ = paths;
    return true;
}

void SystemdUserManager::fail(const QString &error)
{
    if (stopped_) {
        return;
    }

    stopped_ = true;
    emit monitoringFailed(error);
}

} // namespace HyprShelld
