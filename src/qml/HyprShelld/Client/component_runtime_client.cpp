#include "component_runtime_client.h"

#include "component/builtin_component_defaults.h"
#include "component/component_configuration.h"
#include "component/component_contract.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>

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
        instances.append(instanceMap(instanceId, *instance, false));
    }
    return instances;
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
            || property == QStringLiteral("SurfacePlanState")) {
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
    serverRevision_ = acceptedRevision_;
    serverDigest_ = acceptedDigest_;
    serverState_ = acceptedPlan_
        ? QStringLiteral("retained")
        : QStringLiteral("unavailable");
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

    const auto revisionProperty = properties.constFind(
        QStringLiteral("SurfacePlanRevision")
    );
    const auto digestProperty = properties.constFind(
        QStringLiteral("SurfacePlanDigest")
    );
    const auto stateProperty = properties.constFind(
        QStringLiteral("SurfacePlanState")
    );
    if (requireComplete
        && (revisionProperty == properties.cend()
            || digestProperty == properties.cend()
            || stateProperty == properties.cend())) {
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
    publishStateIfChanged(previousCurrent, previousState);

    if (revision == 0) {
        return true;
    }
    if (acceptedPlan_
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
                setLastError(reply.error().message());
                refreshProperties();
                return;
            }

            const auto bytes = reply.argumentAt<0>();
            const auto returnedDigest = reply.argumentAt<1>();
            if (bytes.size() > Components::maximumSurfacePlanBytes
                || returnedDigest != digest
                || Components::surfacePlanDigest(QByteArrayView(bytes)) != digest
                || Components::surfacePlanRevision(digest) != revision) {
                invalidateRuntime(
                    QStringLiteral("The component runtime plan digest is invalid.")
                );
                return;
            }

            auto parsed = Components::parseSurfacePlan(QByteArrayView(bytes));
            if (!parsed) {
                invalidateRuntime(
                    QStringLiteral("The component runtime plan is invalid.")
                );
                return;
            }
            acceptPlan(std::move(*parsed.value), revision, digest);
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

void ComponentRuntimeClient::invalidateRuntime(const QString &error)
{
    const auto previousCurrent = planCurrent();
    const auto previousState = serverState_;
    ++refreshGeneration_;
    serverRevision_ = acceptedRevision_;
    serverDigest_ = acceptedDigest_;
    serverState_ = acceptedPlan_
        ? QStringLiteral("retained")
        : QStringLiteral("unavailable");
    setAvailable(false);
    setLastError(error);
    publishStateIfChanged(previousCurrent, previousState);
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
        .runtimeKind = QStringLiteral("builtin-v1"),
        .factory = QString::fromLatin1(Components::workspaceSwitcherFactory),
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
        {QStringLiteral("runtimeKind"), instance.runtimeKind},
        {QStringLiteral("factory"), instance.factory},
        {QStringLiteral("settings"), instance.settings.toVariantMap()},
        {QStringLiteral("compiledFallback"), compiledFallback},
    };
}

} // namespace HyprShelld
