#include "shell_runtime_status.h"

#include <QDBusArgument>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHash>
#include <QStringList>

#include <utility>

namespace HyprShelld {
namespace {

const QString systemdService = QStringLiteral("org.freedesktop.systemd1");
const QString systemdManagerPath = QStringLiteral("/org/freedesktop/systemd1");
const QString systemdManagerInterface = QStringLiteral(
    "org.freedesktop.systemd1.Manager"
);
const QString invalidSnapshotError = QStringLiteral(
    "org.hyprshelld.Client.Error.InvalidSystemdSnapshot"
);

const QString targetUnit = QStringLiteral("hyprshelld.target");
const QString coordinatorUnit = QStringLiteral("hyprshelld.service");
const QString configurationUnit = QStringLiteral("hyprshelld-configd.service");
const QString componentUnit = QStringLiteral("hyprshelld-componentd.service");
const QString surfaceUnit = QStringLiteral("hyprshelld-surfaced.service");

const QStringList &runtimeUnits()
{
    static const QStringList units {
        targetUnit,
        coordinatorUnit,
        configurationUnit,
        componentUnit,
        surfaceUnit,
    };
    return units;
}

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

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    SystemdUnitRecord &record
)
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

} // namespace
} // namespace HyprShelld

Q_DECLARE_METATYPE(HyprShelld::SystemdUnitRecord)
Q_DECLARE_METATYPE(HyprShelld::SystemdUnitRecords)

namespace HyprShelld {

ShellRuntimeStatus::ShellRuntimeStatus(QObject *parent)
    : ShellRuntimeStatus(QDBusConnection::sessionBus(), parent)
{
}

ShellRuntimeStatus::ShellRuntimeStatus(
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , connection_(std::move(connection))
{
    qDBusRegisterMetaType<SystemdUnitRecord>();
    qDBusRegisterMetaType<SystemdUnitRecords>();

    pollTimer_.setInterval(2000);
    connect(
        &pollTimer_,
        &QTimer::timeout,
        this,
        &ShellRuntimeStatus::refresh
    );
}

bool ShellRuntimeStatus::active() const
{
    return active_;
}

void ShellRuntimeStatus::setActive(bool active)
{
    if (active == active_) {
        return;
    }

    active_ = active;
    ++generation_;
    setBusy(false);
    emit activeChanged();

    if (!active_) {
        pollTimer_.stop();
        setAvailable(false);
        return;
    }

    pollTimer_.start();
    refresh();
}

bool ShellRuntimeStatus::available() const
{
    return available_;
}

bool ShellRuntimeStatus::busy() const
{
    return busy_;
}

QString ShellRuntimeStatus::targetState() const
{
    return targetState_;
}

QString ShellRuntimeStatus::coordinatorState() const
{
    return coordinatorState_;
}

QString ShellRuntimeStatus::configurationState() const
{
    return configurationState_;
}

QString ShellRuntimeStatus::componentManagerState() const
{
    return componentManagerState_;
}

QString ShellRuntimeStatus::surfaceState() const
{
    return surfaceState_;
}

QString ShellRuntimeStatus::lastErrorName() const
{
    return lastErrorName_;
}

QString ShellRuntimeStatus::lastErrorMessage() const
{
    return lastErrorMessage_;
}

void ShellRuntimeStatus::refresh()
{
    if (!active_ || busy_) {
        return;
    }

    auto request = QDBusMessage::createMethodCall(
        systemdService,
        systemdManagerPath,
        systemdManagerInterface,
        QStringLiteral("ListUnitsByNames")
    );
    request << runtimeUnits();

    const auto generation = ++generation_;
    setBusy(true);

    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(request, 5000),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const QDBusPendingReply<SystemdUnitRecords> reply = *watcher;
            watcher->deleteLater();

            if (generation != generation_) {
                return;
            }

            setBusy(false);
            if (reply.isError()) {
                setAvailable(false);
                setError(reply.error().name(), reply.error().message());
                return;
            }

            QHash<QString, QString> states;
            for (const auto &record : reply.value()) {
                if (!runtimeUnits().contains(record.name)) {
                    setAvailable(false);
                    setError(
                        invalidSnapshotError,
                        QStringLiteral("systemd returned an unexpected unit")
                    );
                    return;
                }
                if (states.contains(record.name)) {
                    setAvailable(false);
                    setError(
                        invalidSnapshotError,
                        QStringLiteral("systemd returned a duplicate unit")
                    );
                    return;
                }
                const auto state = record.loadState == QStringLiteral("loaded")
                    ? record.activeState
                    : record.loadState;
                if (state.isEmpty()) {
                    setAvailable(false);
                    setError(
                        invalidSnapshotError,
                        QStringLiteral("systemd returned an empty unit state")
                    );
                    return;
                }
                states.insert(record.name, state);
            }

            if (states.size() != runtimeUnits().size()) {
                setAvailable(false);
                setError(
                    invalidSnapshotError,
                    QStringLiteral("systemd returned an incomplete unit snapshot")
                );
                return;
            }

            const auto targetState = states.value(targetUnit);
            const auto coordinatorState = states.value(coordinatorUnit);
            const auto configurationState = states.value(configurationUnit);
            const auto componentManagerState = states.value(componentUnit);
            const auto surfaceState = states.value(surfaceUnit);
            const bool statesChanged = targetState != targetState_
                || coordinatorState != coordinatorState_
                || configurationState != configurationState_
                || componentManagerState != componentManagerState_
                || surfaceState != surfaceState_;

            targetState_ = targetState;
            coordinatorState_ = coordinatorState;
            configurationState_ = configurationState;
            componentManagerState_ = componentManagerState;
            surfaceState_ = surfaceState;

            if (statesChanged) {
                emit this->statesChanged();
            }
            clearError();
            setAvailable(true);
        }
    );
}

void ShellRuntimeStatus::setAvailable(bool available)
{
    if (available == available_) {
        return;
    }

    available_ = available;
    emit availableChanged();
}

void ShellRuntimeStatus::setBusy(bool busy)
{
    if (busy == busy_) {
        return;
    }

    busy_ = busy;
    emit busyChanged();
}

void ShellRuntimeStatus::setError(const QString &name, const QString &message)
{
    if (name == lastErrorName_ && message == lastErrorMessage_) {
        return;
    }

    lastErrorName_ = name;
    lastErrorMessage_ = message;
    emit lastErrorChanged();
}

void ShellRuntimeStatus::clearError()
{
    setError({}, {});
}

} // namespace HyprShelld
