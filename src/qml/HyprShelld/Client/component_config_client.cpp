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
