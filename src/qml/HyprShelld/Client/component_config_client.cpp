#include "component_config_client.h"

#include "component/component_configuration.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1/Components");
const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentConfig1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
constexpr int callTimeoutMs = 3000;
const QString unavailableError = QStringLiteral(
    "org.hyprshelld.Client.ComponentConfig.Error.Unavailable"
);
const QString catalogUnavailableError = QStringLiteral(
    "org.hyprshelld.Client.ComponentConfig.Error.CatalogUnavailable"
);
const QString busyError = QStringLiteral(
    "org.hyprshelld.Client.ComponentConfig.Error.Busy"
);
const QString invalidComponentError = QStringLiteral(
    "org.hyprshelld.Client.ComponentConfig.Error.InvalidComponent"
);
const QString packageDigestMismatchError = QStringLiteral(
    "org.hyprshelld.Client.ComponentConfig.Error.PackageDigestMismatch"
);

} // namespace

ComponentConfigClient::ComponentConfigClient(QObject *parent)
    : ComponentConfigClient(QDBusConnection::sessionBus(), parent)
{
}

ComponentConfigClient::ComponentConfigClient(
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
        &ComponentConfigClient::serviceOwnerChanged
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

bool ComponentConfigClient::available() const { return available_; }
bool ComponentConfigClient::catalogAvailable() const { return catalogAvailable_; }
bool ComponentConfigClient::busy() const { return busy_; }
qulonglong ComponentConfigClient::revision() const { return revision_; }
QString ComponentConfigClient::catalogDigest() const { return catalogDigest_; }
QString ComponentConfigClient::loadState() const { return loadState_; }
QVariantMap ComponentConfigClient::snapshot() const { return snapshot_; }
QString ComponentConfigClient::pendingComponentId() const
{
    return pendingComponentId_;
}
QString ComponentConfigClient::lastErrorComponentId() const
{
    return lastErrorComponentId_;
}
QString ComponentConfigClient::lastErrorName() const { return lastErrorName_; }
QString ComponentConfigClient::lastErrorMessage() const { return lastErrorMessage_; }

void ComponentConfigClient::replaceSnapshot(const QVariantMap &snapshot)
{
    beginReplaceSnapshot(snapshot, {});
}

void ComponentConfigClient::setComponentEnabled(
    const QString &componentId,
    const QString &expectedPackageDigest,
    const bool enabled
)
{
    if (!available_ || !catalogAvailable_ || busy_) {
        const auto name = !available_ ? unavailableError
            : !catalogAvailable_ ? catalogUnavailableError : busyError;
        const auto message = !available_
            ? QStringLiteral("Component settings are unavailable")
            : !catalogAvailable_
                ? QStringLiteral("The component catalog is unavailable")
                : QStringLiteral("Another component settings change is in progress");
        setError(componentId, name, message);
        return;
    }
    if (!Components::isValidComponentId(componentId)
        || !Components::isFullSha256Digest(expectedPackageDigest)) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component ID or package digest is invalid")
        );
        return;
    }

    auto components = snapshot_.value(QStringLiteral("components")).toMap();
    const auto found = components.constFind(componentId);
    if (found == components.cend()) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component is not present in this settings snapshot")
        );
        return;
    }
    auto record = found->toMap();
    if (record.value(QStringLiteral("packageDigest")).toString()
        != expectedPackageDigest) {
        setError(
            componentId,
            packageDigestMismatchError,
            QStringLiteral("The installed package no longer matches its settings")
        );
        return;
    }
    const auto currentEnabled = record.value(QStringLiteral("enabled"));
    if (currentEnabled.metaType().id() != QMetaType::Bool) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component enablement value is invalid")
        );
        return;
    }
    clearError();
    if (currentEnabled.toBool() == enabled) {
        return;
    }

    record.insert(QStringLiteral("enabled"), enabled);
    components.insert(componentId, record);
    auto replacement = snapshot_;
    replacement.insert(QStringLiteral("components"), components);
    beginReplaceSnapshot(replacement, componentId);
}

void ComponentConfigClient::setComponentSettings(
    const QString &componentId,
    const QString &expectedPackageDigest,
    const QVariantMap &settings
)
{
    if (!available_ || !catalogAvailable_ || busy_) {
        const auto name = !available_ ? unavailableError
            : !catalogAvailable_ ? catalogUnavailableError : busyError;
        setError(
            componentId,
            name,
            QStringLiteral("Component settings cannot be changed right now")
        );
        return;
    }
    if (!Components::isValidComponentId(componentId)
        || !Components::isFullSha256Digest(expectedPackageDigest)) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component ID or package digest is invalid")
        );
        return;
    }

    auto components = snapshot_.value(QStringLiteral("components")).toMap();
    auto record = components.value(componentId).toMap();
    if (!record.isEmpty()
        && record.value(QStringLiteral("packageDigest")).toString()
            != expectedPackageDigest) {
        setError(
            componentId,
            packageDigestMismatchError,
            QStringLiteral(
                "Adopt the installed package version before changing its settings"
            )
        );
        return;
    }
    if (record.isEmpty()) {
        record = {
            {QStringLiteral("packageDigest"), expectedPackageDigest},
            {QStringLiteral("enabled"), false},
            {QStringLiteral("grantedCapabilities"), QVariantList{}},
            {QStringLiteral("settings"), settings},
        };
    } else {
        record.insert(QStringLiteral("settings"), settings);
    }

    auto replacement = snapshot_;
    components.insert(componentId, record);
    replacement.insert(QStringLiteral("components"), components);
    beginReplaceSnapshot(replacement, componentId);
}

void ComponentConfigClient::adoptComponentPackage(
    const QString &componentId,
    const QString &expectedPackageDigest,
    const QVariantMap &defaultComponentSettings
)
{
    if (!available_ || !catalogAvailable_ || busy_) {
        const auto name = !available_ ? unavailableError
            : !catalogAvailable_ ? catalogUnavailableError : busyError;
        setError(
            componentId,
            name,
            QStringLiteral("The updated component cannot be adopted right now")
        );
        return;
    }
    if (!Components::isValidComponentId(componentId)
        || !Components::isFullSha256Digest(expectedPackageDigest)) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component ID or package digest is invalid")
        );
        return;
    }

    auto components = snapshot_.value(QStringLiteral("components")).toMap();
    const auto found = components.constFind(componentId);
    if (found == components.cend()) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("No previous component settings are available to adopt")
        );
        return;
    }
    auto record = found->toMap();
    const auto previousDigest = record.value(
        QStringLiteral("packageDigest")
    ).toString();
    if (!Components::isFullSha256Digest(previousDigest)) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The previous component settings record is malformed")
        );
        return;
    }
    if (previousDigest == expectedPackageDigest) {
        clearError();
        return;
    }

    // Package adoption is intentionally separate from activation. It advances
    // the digest to the catalog-reviewed package, replaces component-scoped
    // settings with that package's trusted defaults, and removes all authority.
    // Existing instances and placements remain untouched for a later explicit
    // Add/Enable action.
    record.insert(QStringLiteral("packageDigest"), expectedPackageDigest);
    record.insert(QStringLiteral("enabled"), false);
    record.insert(QStringLiteral("grantedCapabilities"), QVariantList{});
    record.insert(QStringLiteral("settings"), defaultComponentSettings);
    components.insert(componentId, record);

    auto replacement = snapshot_;
    replacement.insert(QStringLiteral("components"), components);
    beginReplaceSnapshot(replacement, componentId);
}

void ComponentConfigClient::addComponentToBar(
    const QString &componentId,
    const QString &expectedPackageDigest,
    const QVariantMap &defaultComponentSettings
)
{
    if (!available_ || !catalogAvailable_ || busy_) {
        const auto name = !available_ ? unavailableError
            : !catalogAvailable_ ? catalogUnavailableError : busyError;
        setError(
            componentId,
            name,
            QStringLiteral("The component cannot be added to the bar right now")
        );
        return;
    }
    if (!Components::isValidComponentId(componentId)
        || !Components::isFullSha256Digest(expectedPackageDigest)) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component ID or package digest is invalid")
        );
        return;
    }

    auto components = snapshot_.value(QStringLiteral("components")).toMap();
    auto record = components.value(componentId).toMap();
    if (!record.isEmpty()) {
        if (record.value(QStringLiteral("packageDigest")).toString()
            != expectedPackageDigest) {
            setError(
                componentId,
                packageDigestMismatchError,
                QStringLiteral("The installed package no longer matches its settings")
            );
            return;
        }
        const auto enabled = record.value(QStringLiteral("enabled"));
        const auto grants = record.value(QStringLiteral("grantedCapabilities"));
        const auto settings = record.value(QStringLiteral("settings"));
        if (enabled.metaType().id() != QMetaType::Bool
            || grants.metaType().id() != QMetaType::QVariantList
            || !grants.toList().isEmpty()
            || settings.metaType().id() != QMetaType::QVariantMap) {
            setError(
                componentId,
                invalidComponentError,
                QStringLiteral("The component settings record is not safe to activate")
            );
            return;
        }
    } else {
        record = {
            {QStringLiteral("packageDigest"), expectedPackageDigest},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("grantedCapabilities"), QVariantList{}},
            {QStringLiteral("settings"), defaultComponentSettings},
        };
    }
    record.insert(QStringLiteral("enabled"), true);
    record.insert(QStringLiteral("grantedCapabilities"), QVariantList{});
    components.insert(componentId, record);

    auto instances = snapshot_.value(QStringLiteral("instances")).toMap();
    QStringList componentInstanceIds;
    for (auto iterator = instances.constBegin(); iterator != instances.constEnd();
         ++iterator) {
        const auto candidate = iterator.value().toMap();
        if (candidate.value(QStringLiteral("componentId")).toString()
            == componentId) {
            componentInstanceIds.append(iterator.key());
        }
    }
    if (componentInstanceIds.size() > 1) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral(
                "Multiple existing instances make the bar placement ambiguous"
            )
        );
        return;
    }
    QString instanceId = componentInstanceIds.value(0);

    if (instanceId.isEmpty()) {
        for (int attempt = 0; attempt < 8 && instanceId.isEmpty(); ++attempt) {
            const auto candidate = QUuid::createUuid().toString(
                QUuid::WithoutBraces
            );
            if (Components::isLowercaseUuidV4(candidate)
                && !instances.contains(candidate)) {
                instanceId = candidate;
            }
        }
        if (instanceId.isEmpty()) {
            setError(
                componentId,
                invalidComponentError,
                QStringLiteral("A unique component placement could not be created")
            );
            return;
        }
        instances.insert(instanceId, QVariantMap{
            {QStringLiteral("componentId"), componentId},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("settings"), QVariantMap{}},
        });
    } else {
        auto instance = instances.value(instanceId).toMap();
        const auto enabled = instance.value(QStringLiteral("enabled"));
        const auto settings = instance.value(QStringLiteral("settings"));
        if (!Components::isLowercaseUuidV4(instanceId)
            || enabled.metaType().id() != QMetaType::Bool
            || settings.metaType().id() != QMetaType::QVariantMap) {
            setError(
                componentId,
                invalidComponentError,
                QStringLiteral("The existing component placement is malformed")
            );
            return;
        }
        instance.insert(QStringLiteral("enabled"), true);
        instance.insert(QStringLiteral("settings"), QVariantMap{});
        instances.insert(instanceId, instance);
    }

    auto layouts = snapshot_.value(QStringLiteral("layouts")).toMap();
    auto bars = layouts.value(QStringLiteral("bars")).toMap();
    const auto mainRegions = bars.value(QStringLiteral("main")).toMap()
                                 .value(QStringLiteral("regions")).toMap();
    bool placedInMain = false;
    for (const auto &regionName : {
             QStringLiteral("start"),
             QStringLiteral("center"),
             QStringLiteral("end"),
         }) {
        const auto region = mainRegions.value(regionName).toList();
        placedInMain = placedInMain || std::ranges::any_of(
            region,
            [&instanceId](const QVariant &value) {
                return value.metaType().id() == QMetaType::QString
                    && value.toString() == instanceId;
            }
        );
    }
    if (!placedInMain) {
        for (auto layout = bars.begin(); layout != bars.end(); ++layout) {
            auto layoutMap = layout.value().toMap();
            auto regions = layoutMap.value(QStringLiteral("regions")).toMap();
            for (const auto &regionName : {
                     QStringLiteral("start"),
                     QStringLiteral("center"),
                     QStringLiteral("end"),
                 }) {
                auto region = regions.value(regionName).toList();
                region.removeIf([&instanceId](const QVariant &value) {
                    return value.metaType().id() == QMetaType::QString
                        && value.toString() == instanceId;
                });
                regions.insert(regionName, region);
            }
            layoutMap.insert(QStringLiteral("regions"), regions);
            layout.value() = layoutMap;
        }

        auto main = bars.value(QStringLiteral("main")).toMap();
        if (main.isEmpty()) {
            main = {
                {QStringLiteral("outputs"), QVariantMap{
                    {QStringLiteral("mode"), QStringLiteral("all")},
                }},
                {QStringLiteral("regions"), QVariantMap{
                    {QStringLiteral("start"), QVariantList{}},
                    {QStringLiteral("center"), QVariantList{}},
                    {QStringLiteral("end"), QVariantList{}},
                }},
            };
        }
        auto regions = main.value(QStringLiteral("regions")).toMap();
        auto end = regions.value(QStringLiteral("end")).toList();
        end.append(instanceId);
        regions.insert(QStringLiteral("end"), end);
        main.insert(QStringLiteral("regions"), regions);
        bars.insert(QStringLiteral("main"), main);
        layouts.insert(QStringLiteral("bars"), bars);
    }

    auto replacement = snapshot_;
    replacement.insert(QStringLiteral("components"), components);
    replacement.insert(QStringLiteral("instances"), instances);
    replacement.insert(QStringLiteral("layouts"), layouts);
    if (replacement == snapshot_) {
        clearError();
        return;
    }
    beginReplaceSnapshot(replacement, componentId);
}

void ComponentConfigClient::preparePackageChange(
    const QString &componentId,
    const QString &expectedPackageDigest
)
{
    if (!available_ || !catalogAvailable_ || busy_) {
        setError(
            componentId,
            !available_ ? unavailableError
                : !catalogAvailable_ ? catalogUnavailableError : busyError,
            QStringLiteral("The component cannot be prepared for a package change")
        );
        return;
    }
    if (!Components::isValidComponentId(componentId)
        || !Components::isFullSha256Digest(expectedPackageDigest)) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component ID or package digest is invalid")
        );
        return;
    }

    auto components = snapshot_.value(QStringLiteral("components")).toMap();
    const auto found = components.constFind(componentId);
    if (found == components.cend()) {
        clearError();
        return;
    }
    auto record = found->toMap();
    if (record.value(QStringLiteral("packageDigest")).toString()
        != expectedPackageDigest) {
        setError(
            componentId,
            packageDigestMismatchError,
            QStringLiteral("The installed package no longer matches its settings")
        );
        return;
    }
    const auto enabled = record.value(QStringLiteral("enabled"));
    const auto grants = record.value(QStringLiteral("grantedCapabilities"));
    if (enabled.metaType().id() != QMetaType::Bool
        || grants.metaType().id() != QMetaType::QVariantList) {
        setError(
            componentId,
            invalidComponentError,
            QStringLiteral("The component settings record is malformed")
        );
        return;
    }
    if (!enabled.toBool() && grants.toList().isEmpty()) {
        clearError();
        return;
    }
    record.insert(QStringLiteral("enabled"), false);
    record.insert(QStringLiteral("grantedCapabilities"), QVariantList{});
    components.insert(componentId, record);
    auto replacement = snapshot_;
    replacement.insert(QStringLiteral("components"), components);
    beginReplaceSnapshot(replacement, componentId);
}

void ComponentConfigClient::beginReplaceSnapshot(
    const QVariantMap &snapshot,
    const QString &componentId
)
{
    if (!available_ || !catalogAvailable_ || busy_) {
        return;
    }
    clearError();
    auto object = QJsonObject::fromVariantMap(snapshot);
    object.insert(QStringLiteral("formatVersion"), 1);
    object.insert(QStringLiteral("revision"), QString::number(revision_));
    auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    bytes.append('\n');

    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("ReplaceSnapshot")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(revision_),
        catalogDigest_,
        bytes,
    });
    setPendingComponentId(componentId);
    setBusy(true);
    const auto ownerGeneration = ownerGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, ownerGeneration, componentId] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_) {
                return;
            }
            if (reply.type() == QDBusMessage::ErrorMessage) {
                const auto refreshAfterError = refreshQueued_
                    || reply.errorName() == QStringLiteral(
                        "org.hyprshelld.ComponentConfig1.Error.StaleRevision"
                    )
                    || reply.errorName() == QStringLiteral(
                        "org.hyprshelld.ComponentConfig1.Error.StaleCatalogDigest"
                    );
                finishMutation();
                setError(
                    componentId,
                    reply.errorName(),
                    reply.errorMessage()
                );
                if (refreshAfterError) {
                    refresh();
                }
                return;
            }
            refresh();
        }
    );
}

void ComponentConfigClient::clearError()
{
    if (lastErrorComponentId_.isEmpty()
        && lastErrorName_.isEmpty() && lastErrorMessage_.isEmpty()) {
        return;
    }
    lastErrorComponentId_.clear();
    lastErrorName_.clear();
    lastErrorMessage_.clear();
    emit lastErrorChanged();
}

void ComponentConfigClient::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (changedInterface != interfaceName) {
        return;
    }
    if (!applyProperties(changed)) {
        setAvailable(false);
        return;
    }
    const auto relevantInvalidated = std::ranges::any_of(
        invalidated,
        [](const QString &name) {
            return QStringList{
                QStringLiteral("Available"),
                QStringLiteral("CatalogAvailable"),
                QStringLiteral("Revision"),
                QStringLiteral("CatalogDigest"),
                QStringLiteral("LoadState"),
            }.contains(name);
        }
    );
    if (relevantInvalidated) {
        setAvailable(false);
        if (busy_) {
            refreshQueued_ = true;
            return;
        }
        refresh();
        return;
    }
    if (changed.contains(QStringLiteral("Available"))
        || changed.contains(QStringLiteral("Revision"))
        || changed.contains(QStringLiteral("CatalogDigest"))) {
        const auto snapshotMatchesAdvertisement = advertisedAvailable_
            && !snapshot_.isEmpty()
            && revision_ == advertisedRevision_
            && catalogDigest_ == advertisedCatalogDigest_;
        if (snapshotMatchesAdvertisement) {
            setAvailable(true);
            return;
        }
        setAvailable(false);
        if (busy_) {
            refreshQueued_ = true;
            return;
        }
        if (!advertisedAvailable_) {
            return;
        }
        ++refreshGeneration_;
        fetchSnapshot(refreshGeneration_);
    }
}

void ComponentConfigClient::serviceOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)
    ++ownerGeneration_;
    ++refreshGeneration_;
    advertisedAvailable_ = false;
    advertisedCatalogDigest_.clear();
    refreshQueued_ = false;
    setAvailable(false);
    setBusy(false);
    setPendingComponentId({});
    if (catalogAvailable_) {
        catalogAvailable_ = false;
        emit catalogAvailableChanged();
    }
    if (!newOwner.isEmpty()) {
        refresh();
    }
}

void ComponentConfigClient::refresh()
{
    refreshQueued_ = false;
    setAvailable(false);
    const auto generation = ++refreshGeneration_;
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("GetAll")
    );
    message.setArguments({interfaceName});
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();
            if (generation != refreshGeneration_) {
                return;
            }
            if (reply.isError()) {
                finishHydration(false);
                return;
            }
            if (!applyProperties(reply.value(), true)) {
                finishHydration(false);
                return;
            }
            if (advertisedAvailable_) {
                fetchSnapshot(generation);
            } else {
                finishHydration(false);
            }
        }
    );
}

void ComponentConfigClient::fetchSnapshot(const quint64 generation)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetSnapshot")
    );
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != refreshGeneration_) {
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || arguments.size() != 3
                || arguments.at(0).metaType().id() != QMetaType::QByteArray
                || arguments.at(1).metaType().id() != QMetaType::ULongLong
                || arguments.at(2).metaType().id() != QMetaType::QString) {
                finishHydration(false);
                return;
            }
            const auto bytes = arguments.at(0).toByteArray();
            const auto snapshotRevision = arguments.at(1).toULongLong();
            const auto snapshotDigest = arguments.at(2).toString();
            Components::ConfigurationCatalog emptyCatalog;
            const auto parsed = Components::parseComponentConfiguration(
                QByteArrayView(bytes),
                emptyCatalog
            );
            if (!parsed || parsed.value->revision != snapshotRevision
                || !Components::isFullSha256Digest(snapshotDigest)
                || snapshotRevision != advertisedRevision_
                || snapshotDigest != advertisedCatalogDigest_) {
                finishHydration(false);
                return;
            }
            const auto nextSnapshot = QJsonDocument::fromJson(bytes)
                                          .object().toVariantMap();
            const auto changed = snapshot_ != nextSnapshot
                || revision_ != snapshotRevision;
            snapshot_ = nextSnapshot;
            revision_ = snapshotRevision;
            if (catalogDigest_ != snapshotDigest) {
                catalogDigest_ = snapshotDigest;
                emit catalogDigestChanged();
            }
            if (changed) {
                emit snapshotChanged();
            }
            finishHydration(true);
        }
    );
}

bool ComponentConfigClient::applyProperties(
    const QVariantMap &properties,
    const bool requireAll
)
{
    const QStringList required{
        QStringLiteral("Available"),
        QStringLiteral("CatalogAvailable"),
        QStringLiteral("Revision"),
        QStringLiteral("CatalogDigest"),
        QStringLiteral("LoadState"),
    };
    if (requireAll) {
        for (const auto &name : required) {
            if (!properties.contains(name)) {
                return false;
            }
        }
    }
    if (const auto value = properties.constFind(QStringLiteral("Available"));
        value != properties.cend()) {
        if (value->metaType().id() != QMetaType::Bool) {
            return false;
        }
        advertisedAvailable_ = value->toBool();
    }
    if (const auto value = properties.constFind(QStringLiteral("CatalogAvailable"));
        value != properties.cend()) {
        if (value->metaType().id() != QMetaType::Bool) {
            return false;
        }
        const auto next = value->toBool();
        if (next != catalogAvailable_) {
            catalogAvailable_ = next;
            emit catalogAvailableChanged();
        }
    }
    if (const auto value = properties.constFind(QStringLiteral("CatalogDigest"));
        value != properties.cend()) {
        if (value->metaType().id() != QMetaType::QString) {
            return false;
        }
        const auto next = value->toString();
        if (!next.isEmpty() && !Components::isFullSha256Digest(next)) {
            return false;
        }
        advertisedCatalogDigest_ = next;
    }
    if (const auto value = properties.constFind(QStringLiteral("Revision"));
        value != properties.cend()) {
        if (value->metaType().id() != QMetaType::ULongLong) {
            return false;
        }
        advertisedRevision_ = value->toULongLong();
    }
    if (const auto value = properties.constFind(QStringLiteral("LoadState"));
        value != properties.cend()) {
        if (value->metaType().id() != QMetaType::QString) {
            return false;
        }
        const auto next = value->toString();
        if (!QStringList{
                QStringLiteral("normal"),
                QStringLiteral("recovered"),
                QStringLiteral("defaulted"),
                QStringLiteral("unsupported"),
                QStringLiteral("unavailable"),
            }.contains(next)) {
            return false;
        }
        if (next != loadState_) {
            loadState_ = next;
            emit loadStateChanged();
        }
    }
    return !advertisedAvailable_
        || Components::isFullSha256Digest(advertisedCatalogDigest_);
}

void ComponentConfigClient::setAvailable(const bool available)
{
    if (available == available_) return;
    available_ = available;
    emit availableChanged();
    if (available_) clearError();
}

void ComponentConfigClient::setBusy(const bool busy)
{
    if (busy == busy_) return;
    busy_ = busy;
    emit busyChanged();
}

void ComponentConfigClient::setPendingComponentId(
    const QString &componentId
)
{
    if (componentId == pendingComponentId_) return;
    pendingComponentId_ = componentId;
    emit pendingComponentIdChanged();
}

void ComponentConfigClient::finishMutation()
{
    if (!busy_ && pendingComponentId_.isEmpty()) return;
    setBusy(false);
    setPendingComponentId({});
}

void ComponentConfigClient::finishHydration(const bool accepted)
{
    const auto refreshAgain = refreshQueued_;
    finishMutation();
    if (refreshAgain) {
        refresh();
        return;
    }
    setAvailable(accepted);
}

void ComponentConfigClient::setError(
    const QString &componentId,
    const QString &name,
    const QString &message
)
{
    if (componentId != lastErrorComponentId_
        || name != lastErrorName_ || message != lastErrorMessage_) {
        lastErrorComponentId_ = componentId;
        lastErrorName_ = name;
        lastErrorMessage_ = message;
        emit lastErrorChanged();
    }
    emit operationFailed(name, message);
}

} // namespace HyprShelld
