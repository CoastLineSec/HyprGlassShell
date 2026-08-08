#include "coordinator_client.h"

#include "coordinator1_interface.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QVariant>

#include <utility>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Coordinator1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
const QString configUnit = QStringLiteral("hyprshelld-configd.service");
const QString surfacedUnit = QStringLiteral("hyprshelld-surfaced.service");

const QStringList &allowedUnits()
{
    static const QStringList units {configUnit, surfacedUnit};
    return units;
}

} // namespace

CoordinatorClient::CoordinatorClient(QObject *parent)
    : CoordinatorClient(QDBusConnection::sessionBus(), parent)
{
}

CoordinatorClient::CoordinatorClient(
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , connection_(std::move(connection))
{
    interface_ = new OrgHyprshelldCoordinator1Interface(
        serviceName,
        objectPath,
        connection_,
        this
    );

    serviceWatcher_ = new QDBusServiceWatcher(
        serviceName,
        connection_,
        QDBusServiceWatcher::WatchForOwnerChange,
        this
    );
    connect(
        serviceWatcher_,
        &QDBusServiceWatcher::serviceOwnerChanged,
        this,
        &CoordinatorClient::serviceOwnerChanged
    );

    connection_.connect(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(propertiesChanged(QString,QVariantMap,QStringList))
    );

    refresh();
}

bool CoordinatorClient::available() const
{
    return available_;
}

bool CoordinatorClient::busy() const
{
    return !pendingRestarts_.isEmpty();
}

bool CoordinatorClient::healthy() const
{
    return healthy_;
}

QStringList CoordinatorClient::failedUnits() const
{
    return failedUnits_;
}

QString CoordinatorClient::failureSummary() const
{
    return failureSummary_;
}

QString CoordinatorClient::restartingUnit() const
{
    return pendingRestarts_.isEmpty() ? QString{} : pendingRestarts_.constLast();
}

QString CoordinatorClient::lastErrorUnit() const
{
    return lastErrorUnit_;
}

QString CoordinatorClient::lastErrorName() const
{
    return lastErrorName_;
}

QString CoordinatorClient::lastErrorMessage() const
{
    return lastErrorMessage_;
}

void CoordinatorClient::restartComponent(const QString &unitName)
{
    const auto failedWhenRequested = failedUnits_.contains(unitName);
    beginRestart(
        unitName,
        interface_->RestartComponent(unitName),
        ownerGeneration_,
        failedWhenRequested,
        failureEpochs_.value(unitName)
    );
}

void CoordinatorClient::clearError()
{
    if (lastErrorUnit_.isEmpty()
        && lastErrorName_.isEmpty()
        && lastErrorMessage_.isEmpty()) {
        return;
    }

    lastErrorUnit_.clear();
    lastErrorName_.clear();
    lastErrorMessage_.clear();
    emit lastErrorChanged();
}

void CoordinatorClient::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (changedInterface != interfaceName) {
        return;
    }

    for (const auto &property : invalidated) {
        if (property == QStringLiteral("Healthy")
            || property == QStringLiteral("FailedUnits")
            || property == QStringLiteral("FailureSummary")) {
            setAvailable(false);
            refresh();
            return;
        }
    }

    if (!available_ || !applyProperties(changed, false)) {
        setAvailable(false);
        refresh();
        return;
    }

    setAvailable(true);
}

void CoordinatorClient::serviceOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)

    ++ownerGeneration_;
    ++refreshGeneration_;
    setAvailable(false);
    clearError();

    if (!newOwner.isEmpty()) {
        refresh();
    }
}

void CoordinatorClient::refresh()
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("GetAll")
    );
    message.setArguments({interfaceName});

    const auto ownerGeneration = ownerGeneration_;
    const auto refreshGeneration = ++refreshGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, ownerGeneration, refreshGeneration] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();

            if (ownerGeneration != ownerGeneration_
                || refreshGeneration != refreshGeneration_) {
                return;
            }

            if (reply.isError()) {
                setAvailable(false);
                return;
            }

            if (!applyProperties(reply.value(), true)) {
                setAvailable(false);
                return;
            }
            setAvailable(true);
        }
    );
}

bool CoordinatorClient::applyProperties(
    const QVariantMap &properties,
    bool requireComplete
)
{
    auto healthy = healthy_;
    auto failedUnits = failedUnits_;
    auto failureSummary = failureSummary_;
    auto failedUnitsSupplied = false;

    const auto healthyProperty = properties.constFind(QStringLiteral("Healthy"));
    const auto failedUnitsProperty = properties.constFind(
        QStringLiteral("FailedUnits")
    );
    const auto failureSummaryProperty = properties.constFind(
        QStringLiteral("FailureSummary")
    );

    if (requireComplete
        && (healthyProperty == properties.cend()
            || failedUnitsProperty == properties.cend()
            || failureSummaryProperty == properties.cend())) {
        return false;
    }

    if (healthyProperty != properties.cend()) {
        if (healthyProperty->metaType() != QMetaType::fromType<bool>()) {
            return false;
        }
        healthy = healthyProperty->toBool();
    }

    if (failedUnitsProperty != properties.cend()) {
        if (failedUnitsProperty->metaType()
            != QMetaType::fromType<QStringList>()) {
            return false;
        }

        failedUnits.clear();
        for (const auto &unitName : failedUnitsProperty->toStringList()) {
            if (!allowedUnits().contains(unitName)
                || failedUnits.contains(unitName)) {
                return false;
            }
            failedUnits.append(unitName);
        }
        failedUnits.sort();
        failedUnitsSupplied = true;
    }

    if (failureSummaryProperty != properties.cend()) {
        if (failureSummaryProperty->metaType()
            != QMetaType::fromType<QString>()) {
            return false;
        }
        failureSummary = failureSummaryProperty->toString();
    }

    if (healthy != failedUnits.isEmpty()
        || healthy != failureSummary.isEmpty()) {
        return false;
    }

    QStringList addedFailures;
    if (failedUnitsSupplied) {
        for (const auto &unitName : failedUnits) {
            if (!failedUnits_.contains(unitName)) {
                addedFailures.append(unitName);
            }
        }
    }

    const auto healthyChanged = healthy != healthy_;
    const auto failedUnitsChanged = failedUnits != failedUnits_;
    const auto failureSummaryChanged = failureSummary != failureSummary_;

    for (const auto &unitName : allowedUnits()) {
        if (failedUnits.contains(unitName) != failedUnits_.contains(unitName)) {
            failureEpochs_.insert(unitName, ++failureEpoch_);
        }
    }

    healthy_ = healthy;
    failedUnits_ = failedUnits;
    failureSummary_ = failureSummary;

    if (failedUnitsSupplied
        && !lastErrorUnit_.isEmpty()
        && !failedUnits_.contains(lastErrorUnit_)) {
        clearError();
    }

    if (healthyChanged) {
        emit this->healthyChanged();
    }
    if (failedUnitsChanged) {
        emit this->failedUnitsChanged();
    }
    if (failureSummaryChanged) {
        emit this->failureSummaryChanged();
    }
    if (healthyChanged || failedUnitsChanged || failureSummaryChanged) {
        emit healthChanged();
    }
    if (!addedFailures.isEmpty()) {
        emit persistentFailureAdded(failureSummary_, addedFailures);
    }

    return true;
}

void CoordinatorClient::beginRestart(
    const QString &unitName,
    const QDBusPendingCall &call,
    quint64 generation,
    bool failedWhenRequested,
    quint64 failureEpoch
)
{
    clearError();

    const auto wasBusy = busy();
    const auto previousRestartingUnit = restartingUnit();
    pendingRestarts_.append(unitName);

    if (!wasBusy) {
        emit busyChanged();
    }
    if (previousRestartingUnit != restartingUnit()) {
        emit restartingUnitChanged();
    }

    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [
            this,
            watcher,
            generation,
            unitName,
            failedWhenRequested,
            failureEpoch
        ] {
            const QDBusPendingReply<> reply = *watcher;
            watcher->deleteLater();

            const auto failureEpisodeIsCurrent = !failedWhenRequested
                || (failedUnits_.contains(unitName)
                    && failureEpochs_.value(unitName) == failureEpoch);
            if (generation == ownerGeneration_
                && reply.isError()
                && failureEpisodeIsCurrent) {
                setError(
                    unitName,
                    reply.error().name(),
                    reply.error().message()
                );
                if (reply.error().type() == QDBusError::ServiceUnknown
                    || reply.error().name()
                        == QStringLiteral(
                            "org.freedesktop.DBus.Error.NameHasNoOwner"
                        )) {
                    setAvailable(false);
                }
            }

            finishRestart(unitName);
        }
    );
}

void CoordinatorClient::finishRestart(const QString &unitName)
{
    const auto wasBusy = busy();
    const auto previousRestartingUnit = restartingUnit();
    const auto operationIndex = pendingRestarts_.indexOf(unitName);
    if (operationIndex >= 0) {
        pendingRestarts_.removeAt(operationIndex);
    }

    if (previousRestartingUnit != restartingUnit()) {
        emit restartingUnitChanged();
    }
    if (wasBusy && !busy()) {
        emit busyChanged();
    }
}

void CoordinatorClient::setAvailable(bool available)
{
    if (available == available_) {
        return;
    }

    available_ = available;
    emit availableChanged();
}

void CoordinatorClient::setError(
    const QString &unitName,
    const QString &name,
    const QString &message
)
{
    if (unitName != lastErrorUnit_
        || name != lastErrorName_
        || message != lastErrorMessage_) {
        lastErrorUnit_ = unitName;
        lastErrorName_ = name;
        lastErrorMessage_ = message;
        emit lastErrorChanged();
    }

    emit operationFailed(name, message);
}

} // namespace HyprShelld
