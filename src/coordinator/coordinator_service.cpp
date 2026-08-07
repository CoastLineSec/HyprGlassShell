#include "coordinator_service.h"

#include "systemd_user_manager.h"

#include <QDBusMessage>
#include <QDebug>
#include <QVariantMap>

#include <utility>

namespace HyprShelld {
namespace {

const QString coordinatorInterface = QStringLiteral("org.hyprshelld.Coordinator1");
const QString coordinatorPath = QStringLiteral("/org/hyprshelld/Coordinator1");
const QString unknownComponentError = QStringLiteral(
    "org.hyprshelld.Coordinator1.Error.UnknownComponent"
);
const QString componentNotFailedError = QStringLiteral(
    "org.hyprshelld.Coordinator1.Error.ComponentNotFailed"
);
const QString restartFailedError = QStringLiteral(
    "org.hyprshelld.Coordinator1.Error.RestartFailed"
);

} // namespace

CoordinatorService::CoordinatorService(
    SystemdUserManager *manager,
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , manager_(manager)
    , connection_(std::move(connection))
{
    Q_ASSERT(manager_ != nullptr);

    connect(
        manager_,
        &SystemdUserManager::snapshotReady,
        this,
        &CoordinatorService::applySnapshot
    );
    connect(
        manager_,
        &SystemdUserManager::monitoringFailed,
        this,
        &CoordinatorService::fatalError
    );
}

bool CoordinatorService::healthy() const
{
    return policy_.healthy();
}

QStringList CoordinatorService::failedUnits() const
{
    return policy_.failedUnits();
}

QString CoordinatorService::failureSummary() const
{
    return policy_.failureSummary();
}

bool CoordinatorService::start(QString &error)
{
    return manager_->start(error);
}

void CoordinatorService::RestartComponent(const QString &unitName)
{
    if (!CoordinatorPolicy::isKnown(unitName)) {
        reportError(
            unknownComponentError,
            QStringLiteral("The requested component is not managed by HyprShelld")
        );
        return;
    }

    if (!policy_.isFailed(unitName)) {
        reportError(
            componentNotFailedError,
            QStringLiteral("The requested component is not failed")
        );
        return;
    }

    if (!calledFromDBus()) {
        manager_->restartUnit(
            unitName,
            [](bool accepted, const QString &error) {
                if (!accepted) {
                    qWarning().noquote() << error;
                }
            }
        );
        return;
    }

    const auto request = message();
    setDelayedReply(true);
    manager_->restartUnit(
        unitName,
        [connection = connection_, request](
            bool accepted,
            const QString &error
        ) {
            if (accepted) {
                connection.send(request.createReply());
                return;
            }

            qWarning().noquote() << error;
            connection.send(
                request.createErrorReply(
                    restartFailedError,
                    QStringLiteral("systemd did not accept the component restart")
                )
            );
        }
    );
}

void CoordinatorService::applySnapshot(
    const QHash<QString, QString> &activeStates
)
{
    const bool changed = policy_.applySnapshot(activeStates);
    if (!initialized_) {
        initialized_ = true;
        emit initialized();
        return;
    }

    if (changed) {
        publishChange();
    }
}

void CoordinatorService::reportError(
    const QString &name,
    const QString &message
) const
{
    if (calledFromDBus()) {
        sendErrorReply(name, message);
    }
}

void CoordinatorService::publishChange() const
{
    QVariantMap changed;
    changed.insert(QStringLiteral("Healthy"), healthy());
    changed.insert(QStringLiteral("FailedUnits"), failedUnits());
    changed.insert(QStringLiteral("FailureSummary"), failureSummary());

    auto signal = QDBusMessage::createSignal(
        coordinatorPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );
    signal.setArguments({coordinatorInterface, changed, QStringList()});

    if (!connection_.send(signal)) {
        qWarning() << "Failed to publish coordinator health change";
    }
}

} // namespace HyprShelld
