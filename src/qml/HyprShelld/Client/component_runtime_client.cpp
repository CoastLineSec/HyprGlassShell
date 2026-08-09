#include "component_runtime_client.h"

#include "component/builtin_component_defaults.h"
#include "component/component_configuration.h"
#include "component/component_contract.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QSet>

#include <algorithm>
#include <utility>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString objectPath = QStringLiteral(
    "/org/hyprshelld/Coordinator1/Components"
);
const QString interfaceName = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1"
);
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
constexpr int callTimeoutMs = 3000;

bool isKnownPlanState(const QString &state)
{
    return state == QStringLiteral("hydrating")
        || state == QStringLiteral("authoritative")
        || state == QStringLiteral("retained")
        || state == QStringLiteral("unavailable");
}

bool isKnownQuarantineReason(const QString &reason)
{
    return reason == QStringLiteral("incomplete-startup")
        || reason == QStringLiteral("timeout")
        || reason == QStringLiteral("render-failed")
        || reason == QStringLiteral("protocol-invalid");
}

bool isValidRuntimeHealthRecord(
    const QString &state,
    const QString &reason,
    const uint failureCount
)
{
    if (state == QStringLiteral("probation")) {
        return reason.isEmpty() && failureCount == 0;
    }
    return state == QStringLiteral("quarantined")
        && isKnownQuarantineReason(reason)
        && failureCount >= 1
        && failureCount <= 1000000;
}

QStringList regionIds(
    const Components::SurfaceBarLayout &layout,
    const QString &region
)
{
    if (region == QStringLiteral("start")) {
        return layout.start;
    }
    if (region == QStringLiteral("center")) {
        return layout.center;
    }
    if (region == QStringLiteral("end")) {
        return layout.end;
    }
    return {};
}

} // namespace

ComponentRuntimeClient::ComponentRuntimeClient(QObject *parent)
    : ComponentRuntimeClient(QDBusConnection::sessionBus(), parent)
{
}

ComponentRuntimeClient::ComponentRuntimeClient(
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , connection_(std::move(connection))
{
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
        &ComponentRuntimeClient::serviceOwnerChanged
    );

    connection_.connect(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(propertiesChanged(QString,QVariantMap,QStringList))
    );
    refreshProperties();
}

bool ComponentRuntimeClient::available() const
{
    return available_;
}

bool ComponentRuntimeClient::planCurrent() const
{
    return available_
        && acceptedPlan_.has_value()
        && !acceptedPlanSanitized_
        && serverState_ == QStringLiteral("authoritative")
        && acceptedRevision_ == serverRevision_
        && acceptedDigest_ == serverDigest_;
}

bool ComponentRuntimeClient::usingFallback() const
{
    return usingFallback_;
}

qulonglong ComponentRuntimeClient::planRevision() const
{
    return acceptedRevision_;
}

QString ComponentRuntimeClient::planDigest() const
{
    return acceptedDigest_;
}

QString ComponentRuntimeClient::planState() const
{
    return serverState_;
}

QString ComponentRuntimeClient::lastError() const
{
    return lastError_;
}

qulonglong ComponentRuntimeClient::runtimeHealthRevision() const
{
    return acceptedRuntimeHealthRevision_;
}

bool ComponentRuntimeClient::runtimeHealthAvailable() const
{
    return runtimeHealthAvailable_;
}

bool ComponentRuntimeClient::thirdPartySafeMode() const
{
    return thirdPartySafeMode_;
}

QVariantList ComponentRuntimeClient::runtimeStates() const
{
    return runtimeStates_;
}

QString ComponentRuntimeClient::runtimeRetryBusyComponentId() const
{
    return runtimeRetryBusyComponentId_;
}

QVariantList ComponentRuntimeClient::barInstances(
    const QString &layoutId,
    const QString &outputName,
    const QString &region
) const
{
    Q_UNUSED(outputName)

    if (usingFallback_) {
        return fallbackBarInstances(layoutId, region);
    }
    if (!acceptedPlan_) {
        return {};
    }

    const auto layout = acceptedPlan_->barLayouts.constFind(layoutId);
    if (layout == acceptedPlan_->barLayouts.cend()
        || layout->outputMode != QStringLiteral("all")) {
        return {};
    }

    QVariantList instances;
    const auto ids = regionIds(*layout, region);
    instances.reserve(ids.size());
    for (const auto &instanceId : ids) {
        const auto instance = acceptedPlan_->instances.constFind(instanceId);
        if (instance == acceptedPlan_->instances.cend()) {
            return {};
        }
        if (instance->runtimeKind == QStringLiteral("declarative-v1")
            && (layoutId != QStringLiteral("main")
                || !planCurrent()
                || authorizedSurfacePlanRevision_ != acceptedRevision_)) {
            continue;
        }
        instances.append(instanceMap(instanceId, *instance, false));
    }
    return instances;
}

void ComponentRuntimeClient::reportActivationStable(
    const QString &instanceId,
    const QString &componentId,
    const QString &packageDigest,
    const QString &surfacePlanDigest
)
{
    if (!planCurrent() || !acceptedPlan_
        || authorizedSurfacePlanRevision_ != acceptedRevision_
        || surfacePlanDigest != acceptedDigest_) {
        return;
    }
    const auto instance = acceptedPlan_->instances.constFind(instanceId);
    if (instance == acceptedPlan_->instances.cend()
        || instance->runtimeKind != QStringLiteral("declarative-v1")
        || instance->componentId != componentId
        || instance->packageDigest != packageDigest) {
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("ActivationStable")
    );
    message.setArguments({
        instanceId,
        componentId,
        packageDigest,
        QVariant::fromValue<qulonglong>(acceptedRevision_),
    });
    connection_.asyncCall(message, callTimeoutMs);
}

bool ComponentRuntimeClient::authorizeCurrentPlan()
{
    if (!planCurrent() || authorizationBusy_) {
        return false;
    }
    const auto hasDeclarative = std::ranges::any_of(
        acceptedPlan_->instances,
        [](const auto &instance) {
            return instance.runtimeKind == QStringLiteral("declarative-v1");
        }
    );
    if (!hasDeclarative) {
        authorizedSurfacePlanRevision_ = acceptedRevision_;
        return true;
    }
    if (authorizedSurfacePlanRevision_ == acceptedRevision_) {
        return true;
    }
    const auto revision = acceptedRevision_;
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("AuthorizeSurfacePlan")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(revision),
    });
    authorizationBusy_ = true;
    const auto authorizationGeneration = ++authorizationGeneration_;
    const auto ownerGeneration = ownerGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [
            this,
            watcher,
            ownerGeneration,
            authorizationGeneration,
            revision
        ] {
            const QDBusPendingReply<bool> reply = *watcher;
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_
                || authorizationGeneration != authorizationGeneration_
                || revision != acceptedRevision_
                || !planCurrent()) {
                return;
            }
            authorizationBusy_ = false;
            if (reply.isError() || !reply.value()) {
                setLastError(
                    reply.isError()
                        ? reply.error().message()
                        : QStringLiteral("Component plan authorization was rejected.")
                );
                refreshProperties();
                return;
            }
            authorizedSurfacePlanRevision_ = revision;
            setLastError({});
            emit planChanged();
        }
    );
    return true;
}

bool ComponentRuntimeClient::cancelCurrentPlanAuthorization()
{
    if (!available_ || !acceptedPlan_ || !planCurrent()
        || (!authorizationBusy_
            && authorizedSurfacePlanRevision_ != acceptedRevision_)) {
        return false;
    }
    const auto revision = acceptedRevision_;
    authorizedSurfacePlanRevision_ = 0;
    authorizationBusy_ = false;
    ++authorizationGeneration_;
    emit planChanged();
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("CancelSurfacePlanAuthorization")
    );
    message.setArguments({QVariant::fromValue<qulonglong>(revision)});
    connection_.asyncCall(message, callTimeoutMs);
    return true;
}

void ComponentRuntimeClient::reportActivationFailed(
    const QString &instanceId,
    const QString &componentId,
    const QString &packageDigest,
    const QString &surfacePlanDigest,
    const QString &reason
)
{
    if (!planCurrent() || !acceptedPlan_
        || authorizedSurfacePlanRevision_ != acceptedRevision_
        || surfacePlanDigest != acceptedDigest_) {
        return;
    }
    const auto instance = acceptedPlan_->instances.constFind(instanceId);
    if (instance == acceptedPlan_->instances.cend()
        || instance->runtimeKind != QStringLiteral("declarative-v1")
        || instance->componentId != componentId
        || instance->packageDigest != packageDigest) {
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("ActivationFailed")
    );
    message.setArguments({
        instanceId,
        componentId,
        packageDigest,
        QVariant::fromValue<qulonglong>(acceptedRevision_),
        reason,
    });
    connection_.asyncCall(message, callTimeoutMs);
}

QVariantMap ComponentRuntimeClient::runtimeStatus(
    const QString &componentId,
    const QString &packageDigest
) const
{
    for (const auto &value : runtimeStates_) {
        const auto record = value.toMap();
        if (record.value(QStringLiteral("componentId")).toString()
                == componentId
            && record.value(QStringLiteral("packageDigest")).toString()
                == packageDigest) {
            return record;
        }
    }
    return {};
}

bool ComponentRuntimeClient::retryComponent(
    const QString &componentId,
    const QString &packageDigest
)
{
    if (!available_ || !runtimeHealthAvailable_ || thirdPartySafeMode_
        || !runtimeRetryBusyComponentId_.isEmpty()
        || !Components::isValidComponentId(componentId)
        || !Components::isFullSha256Digest(packageDigest)) {
        return false;
    }
    const auto status = runtimeStatus(componentId, packageDigest);
    if (status.value(QStringLiteral("state")).toString()
        != QStringLiteral("quarantined")) {
        return false;
    }

    runtimeRetryBusyComponentId_ = componentId;
    const auto retryGeneration = ++retryGeneration_;
    emit runtimeRetryBusyChanged();
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("RetryComponent")
    );
    message.setArguments({
        componentId,
        packageDigest,
        QVariant::fromValue<qulonglong>(acceptedRuntimeHealthRevision_),
    });
    const auto ownerGeneration = ownerGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, ownerGeneration, retryGeneration] {
            const QDBusPendingReply<qulonglong> reply = *watcher;
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_
                || retryGeneration != retryGeneration_) {
                return;
            }
            if (reply.isError()) {
                setLastError(reply.error().message());
            }
            if (!runtimeRetryBusyComponentId_.isEmpty()) {
                runtimeRetryBusyComponentId_.clear();
                emit runtimeRetryBusyChanged();
            }
            refreshProperties();
        }
    );
    return true;
}

void ComponentRuntimeClient::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (changedInterface != interfaceName) {
        return;
    }

    for (const auto &property : invalidated) {
        if (property == QStringLiteral("SurfacePlanRevision")
            || property == QStringLiteral("SurfacePlanDigest")
            || property == QStringLiteral("SurfacePlanState")
            || property == QStringLiteral("RuntimeHealthRevision")
            || property == QStringLiteral("ThirdPartySafeMode")) {
            invalidateRuntime(
                QStringLiteral("The component runtime invalidated required properties.")
            );
            refreshProperties();
            return;
        }
    }

    if (!applyProperties(changed, false)) {
        invalidateRuntime(
            QStringLiteral("The component runtime published invalid properties.")
        );
        refreshProperties();
        return;
    }
    setAvailable(true);
}

void ComponentRuntimeClient::serviceOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)

    const auto previousCurrent = planCurrent();
    const auto previousState = serverState_;
    ++ownerGeneration_;
    ++refreshGeneration_;
    ++healthRefreshGeneration_;
    authorizedSurfacePlanRevision_ = 0;
    authorizationBusy_ = false;
    ++authorizationGeneration_;
    retainBuiltinsOnly();
    serverRevision_ = acceptedRevision_;
    serverDigest_ = acceptedDigest_;
    serverState_ = acceptedPlan_
        ? QStringLiteral("retained")
        : QStringLiteral("unavailable");
    serverRuntimeHealthRevision_ = 0;
    acceptedRuntimeHealthRevision_ = 0;
    runtimeHealthAvailable_ = false;
    thirdPartySafeMode_ = true;
    runtimeStates_.clear();
    emit runtimeHealthChanged();
    if (!runtimeRetryBusyComponentId_.isEmpty()) {
        ++retryGeneration_;
        runtimeRetryBusyComponentId_.clear();
        emit runtimeRetryBusyChanged();
    }
    setAvailable(false);
    publishStateIfChanged(previousCurrent, previousState);

    if (!newOwner.isEmpty()) {
        refreshProperties();
    }
}

void ComponentRuntimeClient::refreshProperties()
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
        connection_.asyncCall(message, callTimeoutMs),
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
                invalidateRuntime(reply.error().message());
                return;
            }
            if (!applyProperties(reply.value(), true)) {
                invalidateRuntime(
                    QStringLiteral("The component runtime published invalid properties.")
                );
                return;
            }
            setAvailable(true);
        }
    );
}

bool ComponentRuntimeClient::applyProperties(
    const QVariantMap &properties,
    bool requireComplete
)
{
    auto revision = serverRevision_;
    auto digest = serverDigest_;
    auto state = serverState_;
    auto healthRevision = serverRuntimeHealthRevision_;
    auto safeMode = thirdPartySafeMode_;

    const auto revisionProperty = properties.constFind(
        QStringLiteral("SurfacePlanRevision")
    );
    const auto digestProperty = properties.constFind(
        QStringLiteral("SurfacePlanDigest")
    );
    const auto stateProperty = properties.constFind(
        QStringLiteral("SurfacePlanState")
    );
    const auto healthRevisionProperty = properties.constFind(
        QStringLiteral("RuntimeHealthRevision")
    );
    const auto safeModeProperty = properties.constFind(
        QStringLiteral("ThirdPartySafeMode")
    );
    if (requireComplete
        && (revisionProperty == properties.cend()
            || digestProperty == properties.cend()
            || stateProperty == properties.cend()
            || healthRevisionProperty == properties.cend()
            || safeModeProperty == properties.cend())) {
        return false;
    }

    if (revisionProperty != properties.cend()) {
        if (revisionProperty->metaType()
            != QMetaType::fromType<qulonglong>()) {
            return false;
        }
        revision = revisionProperty->toULongLong();
    }
    if (digestProperty != properties.cend()) {
        if (digestProperty->metaType() != QMetaType::fromType<QString>()) {
            return false;
        }
        digest = digestProperty->toString();
    }
    if (stateProperty != properties.cend()) {
        if (stateProperty->metaType() != QMetaType::fromType<QString>()) {
            return false;
        }
        state = stateProperty->toString();
    }
    if (healthRevisionProperty != properties.cend()) {
        if (healthRevisionProperty->metaType()
            != QMetaType::fromType<qulonglong>()) {
            return false;
        }
        healthRevision = healthRevisionProperty->toULongLong();
    }
    if (safeModeProperty != properties.cend()) {
        if (safeModeProperty->metaType()
            != QMetaType::fromType<bool>()) {
            return false;
        }
        safeMode = safeModeProperty->toBool();
    }

    if (!isKnownPlanState(state)
        || (revision == 0) != digest.isEmpty()
        || (revision == 0
            && state != QStringLiteral("hydrating")
            && state != QStringLiteral("unavailable"))
        || (revision != 0
            && state != QStringLiteral("authoritative")
            && state != QStringLiteral("retained"))) {
        return false;
    }
    if (revision != 0
        && (!Components::isFullSha256Digest(digest)
            || Components::surfacePlanRevision(digest) != revision)) {
        return false;
    }

    const auto previousCurrent = planCurrent();
    const auto previousState = serverState_;
    serverRevision_ = revision;
    serverDigest_ = digest;
    serverState_ = state;
    if (state != QStringLiteral("authoritative")
        || revision != acceptedRevision_
        || digest != acceptedDigest_) {
        authorizedSurfacePlanRevision_ = 0;
        authorizationBusy_ = false;
        ++authorizationGeneration_;
    }
    const auto healthChanged = healthRevision
            != serverRuntimeHealthRevision_
        || safeMode != thirdPartySafeMode_;
    serverRuntimeHealthRevision_ = healthRevision;
    thirdPartySafeMode_ = safeMode;
    publishStateIfChanged(previousCurrent, previousState);
    if (healthChanged) {
        emit runtimeHealthChanged();
    }
    if (!runtimeHealthAvailable_
        || acceptedRuntimeHealthRevision_ != healthRevision) {
        runtimeHealthAvailable_ = false;
        runtimeStates_.clear();
        emit runtimeHealthChanged();
        fetchRuntimeStates(healthRevision);
    }

    if (revision == 0) {
        return true;
    }
    if (acceptedPlan_ && !acceptedPlanSanitized_
        && acceptedRevision_ == revision
        && acceptedDigest_ == digest) {
        setLastError({});
        return true;
    }

    fetchPlan(revision, digest);
    return true;
}

void ComponentRuntimeClient::fetchPlan(
    quint64 revision,
    const QString &digest
)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetSurfacePlan")
    );
    message.setArguments({QVariant::fromValue<qulonglong>(revision)});

    const auto ownerGeneration = ownerGeneration_;
    const auto refreshGeneration = ++refreshGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [
            this,
            watcher,
            ownerGeneration,
            refreshGeneration,
            revision,
            digest
        ] {
            const QDBusPendingReply<QByteArray, QString> reply = *watcher;
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_
                || refreshGeneration != refreshGeneration_
                || revision != serverRevision_
                || digest != serverDigest_) {
                return;
            }
            if (reply.isError()) {
                invalidatePlan(reply.error().message());
                refreshProperties();
                return;
            }

            const auto bytes = reply.argumentAt<0>();
            const auto returnedDigest = reply.argumentAt<1>();
            if (bytes.size() > Components::maximumSurfacePlanBytes
                || returnedDigest != digest
                || Components::surfacePlanDigest(QByteArrayView(bytes)) != digest
                || Components::surfacePlanRevision(digest) != revision) {
                invalidatePlan(
                    QStringLiteral("The component runtime plan digest is invalid.")
                );
                return;
            }

            auto parsed = Components::parseSurfacePlan(QByteArrayView(bytes));
            if (!parsed) {
                invalidatePlan(
                    QStringLiteral("The component runtime plan is invalid.")
                );
                return;
            }
            acceptPlan(std::move(*parsed.value), revision, digest);
        }
    );
}

void ComponentRuntimeClient::fetchRuntimeStates(
    const quint64 runtimeHealthRevision
)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("ListComponentRuntimeStates")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(runtimeHealthRevision)
    });
    const auto ownerGeneration = ownerGeneration_;
    const auto healthGeneration = ++healthRefreshGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [
            this,
            watcher,
            ownerGeneration,
            healthGeneration,
            runtimeHealthRevision
        ] {
            const QDBusPendingReply<
                QStringList,
                QStringList,
                QStringList,
                QStringList,
                QList<uint>
            > reply = *watcher;
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_
                || healthGeneration != healthRefreshGeneration_
                || runtimeHealthRevision
                    != serverRuntimeHealthRevision_) {
                return;
            }
            if (reply.isError()) {
                invalidateRuntimeHealth(reply.error().message());
                return;
            }

            const auto componentIds = reply.argumentAt<0>();
            const auto packageDigests = reply.argumentAt<1>();
            const auto states = reply.argumentAt<2>();
            const auto reasons = reply.argumentAt<3>();
            const auto failureCounts = reply.argumentAt<4>();
            if (componentIds.size() > 512
                || packageDigests.size() != componentIds.size()
                || states.size() != componentIds.size()
                || reasons.size() != componentIds.size()
                || failureCounts.size() != componentIds.size()) {
                invalidateRuntimeHealth(
                    QStringLiteral(
                        "The component runtime health list is malformed."
                    )
                );
                return;
            }

            QVariantList records;
            QSet<QString> seen;
            QString previousComponentId;
            QString previousPackageDigest;
            records.reserve(componentIds.size());
            for (qsizetype index = 0; index < componentIds.size(); ++index) {
                const auto &componentId = componentIds.at(index);
                const auto &packageDigest = packageDigests.at(index);
                const auto state = states.at(index);
                const auto &reason = reasons.at(index);
                const auto failureCount = failureCounts.at(index);
                const auto key = componentId + QLatin1Char('/')
                    + packageDigest;
                const auto outOfOrder = !previousComponentId.isEmpty()
                    && (componentId < previousComponentId
                        || (componentId == previousComponentId
                            && packageDigest <= previousPackageDigest));
                if (!Components::isValidComponentId(componentId)
                    || !Components::isFullSha256Digest(packageDigest)
                    || !isValidRuntimeHealthRecord(
                        state,
                        reason,
                        failureCount
                    )
                    || outOfOrder
                    || seen.contains(key)) {
                    invalidateRuntimeHealth(
                        QStringLiteral("The component runtime health record is malformed.")
                    );
                    return;
                }
                seen.insert(key);
                previousComponentId = componentId;
                previousPackageDigest = packageDigest;
                records.append(QVariantMap{
                    {QStringLiteral("componentId"), componentId},
                    {
                        QStringLiteral("packageDigest"),
                        packageDigest
                    },
                    {QStringLiteral("state"), state},
                    {QStringLiteral("reason"), reason},
                    {
                        QStringLiteral("failureCount"),
                        failureCount
                    },
                });
            }
            runtimeStates_ = std::move(records);
            acceptedRuntimeHealthRevision_ = runtimeHealthRevision;
            runtimeHealthAvailable_ = true;
            setLastError({});
            emit runtimeHealthChanged();
        }
    );
}

void ComponentRuntimeClient::acceptPlan(
    Components::SurfacePlan plan,
    quint64 revision,
    const QString &digest
)
{
    if (revision != serverRevision_ || digest != serverDigest_) {
        return;
    }

    acceptedPlan_ = std::move(plan);
    acceptedPlanSanitized_ = false;
    authorizedSurfacePlanRevision_ = 0;
    authorizationBusy_ = false;
    ++authorizationGeneration_;
    acceptedRevision_ = revision;
    acceptedDigest_ = digest;
    usingFallback_ = false;
    setLastError({});
    emit planChanged();
    emit planStateChanged();
}

void ComponentRuntimeClient::setAvailable(bool available)
{
    if (available_ == available) {
        return;
    }
    const auto previousCurrent = planCurrent();
    available_ = available;
    emit availableChanged();
    if (previousCurrent != planCurrent()) {
        emit planStateChanged();
    }
}

void ComponentRuntimeClient::setLastError(const QString &error)
{
    if (lastError_ == error) {
        return;
    }
    lastError_ = error;
    emit lastErrorChanged();
}

void ComponentRuntimeClient::invalidatePlan(const QString &error)
{
    ++refreshGeneration_;
    authorizedSurfacePlanRevision_ = 0;
    authorizationBusy_ = false;
    ++authorizationGeneration_;
    retainBuiltinsOnly();
    setLastError(error);
    emit planChanged();
}

void ComponentRuntimeClient::invalidateRuntimeHealth(const QString &error)
{
    ++healthRefreshGeneration_;
    acceptedRuntimeHealthRevision_ = 0;
    runtimeHealthAvailable_ = false;
    runtimeStates_.clear();
    if (!runtimeRetryBusyComponentId_.isEmpty()) {
        ++retryGeneration_;
        runtimeRetryBusyComponentId_.clear();
        emit runtimeRetryBusyChanged();
    }
    setLastError(error);
    emit runtimeHealthChanged();
}

void ComponentRuntimeClient::invalidateRuntime(const QString &error)
{
    const auto previousCurrent = planCurrent();
    const auto previousState = serverState_;
    ++refreshGeneration_;
    ++healthRefreshGeneration_;
    authorizedSurfacePlanRevision_ = 0;
    authorizationBusy_ = false;
    ++authorizationGeneration_;
    retainBuiltinsOnly();
    serverRevision_ = acceptedRevision_;
    serverDigest_ = acceptedDigest_;
    serverState_ = acceptedPlan_
        ? QStringLiteral("retained")
        : QStringLiteral("unavailable");
    serverRuntimeHealthRevision_ = 0;
    acceptedRuntimeHealthRevision_ = 0;
    runtimeHealthAvailable_ = false;
    thirdPartySafeMode_ = true;
    runtimeStates_.clear();
    emit runtimeHealthChanged();
    if (!runtimeRetryBusyComponentId_.isEmpty()) {
        ++retryGeneration_;
        runtimeRetryBusyComponentId_.clear();
        emit runtimeRetryBusyChanged();
    }
    setAvailable(false);
    setLastError(error);
    publishStateIfChanged(previousCurrent, previousState);
}

void ComponentRuntimeClient::retainBuiltinsOnly()
{
    if (!acceptedPlan_) {
        return;
    }

    QSet<QString> removed;
    for (auto iterator = acceptedPlan_->instances.cbegin();
         iterator != acceptedPlan_->instances.cend(); ++iterator) {
        if (iterator->runtimeKind != QStringLiteral("builtin-v1")) {
            removed.insert(iterator.key());
        }
    }
    if (removed.isEmpty()) {
        return;
    }
    for (const auto &instanceId : removed) {
        acceptedPlan_->instances.remove(instanceId);
    }
    const auto filter = [&removed](QStringList &instances) {
        instances.removeIf([&removed](const QString &instanceId) {
            return removed.contains(instanceId);
        });
    };
    for (auto iterator = acceptedPlan_->barLayouts.begin();
         iterator != acceptedPlan_->barLayouts.end(); ++iterator) {
        filter(iterator->start);
        filter(iterator->center);
        filter(iterator->end);
    }
    acceptedPlanSanitized_ = true;
    emit planChanged();
}

void ComponentRuntimeClient::publishStateIfChanged(
    bool previousCurrent,
    const QString &previousState
)
{
    if (previousCurrent != planCurrent() || previousState != serverState_) {
        emit planStateChanged();
    }
}

QVariantList ComponentRuntimeClient::fallbackBarInstances(
    const QString &layoutId,
    const QString &region
) const
{
    if (layoutId != QLatin1StringView(Components::defaultBarLayoutId)
        || region != QStringLiteral("start")) {
        return {};
    }

    Components::SurfaceInstance instance {
        .componentId = QString::fromLatin1(Components::workspaceSwitcherId),
        .componentType = QStringLiteral("bar-widget"),
        .packageDigest = {},
        .runtimeKind = QStringLiteral("builtin-v1"),
        .factory = QString::fromLatin1(Components::workspaceSwitcherFactory),
        .declarativeText = {},
        .declarativeTooltip = {},
        .declarativeMaximumWidth = 0,
        .settings = Components::workspaceSwitcherDefaultSettings(),
    };
    return {
        instanceMap(
            QString::fromLatin1(Components::workspaceSwitcherDefaultInstanceId),
            instance,
            true
        ),
    };
}

QVariantMap ComponentRuntimeClient::instanceMap(
    const QString &instanceId,
    const Components::SurfaceInstance &instance,
    bool compiledFallback
) const
{
    return {
        {QStringLiteral("instanceId"), instanceId},
        {QStringLiteral("componentId"), instance.componentId},
        {QStringLiteral("componentType"), instance.componentType},
        {QStringLiteral("packageDigest"), instance.packageDigest},
        {
            QStringLiteral("surfacePlanDigest"),
            instance.runtimeKind == QStringLiteral("declarative-v1")
                ? acceptedDigest_ : QString()
        },
        {QStringLiteral("runtimeKind"), instance.runtimeKind},
        {QStringLiteral("factory"), instance.factory},
        {QStringLiteral("declarativeText"), instance.declarativeText},
        {
            QStringLiteral("declarativeTooltip"),
            instance.declarativeTooltip
        },
        {
            QStringLiteral("declarativeMaximumWidth"),
            instance.declarativeMaximumWidth
        },
        {QStringLiteral("settings"), instance.settings.toVariantMap()},
        {QStringLiteral("compiledFallback"), compiledFallback},
    };
}

} // namespace HyprShelld
