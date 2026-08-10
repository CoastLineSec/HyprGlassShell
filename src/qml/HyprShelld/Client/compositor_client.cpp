#include "compositor_client.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaType>
#include <QSet>
#include <QVariant>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Compositor1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Compositor1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Compositor1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
constexpr int ordinaryCallTimeoutMs = 15000;
constexpr int previewCallTimeoutMs = 45000;

[[nodiscard]] bool isDigest(const QString &value)
{
    if (value.size() != 64) return false;
    return std::ranges::all_of(value, [](const QChar character) {
        return character.isDigit()
            || (character >= QLatin1Char('a')
                && character <= QLatin1Char('f'));
    });
}

[[nodiscard]] bool isOpaqueToken(const QString &value)
{
    if (value.size() != 32) return false;
    return std::ranges::all_of(value, [](const QChar character) {
        return character.isDigit()
            || (character >= QLatin1Char('a')
                && character <= QLatin1Char('f'));
    });
}

[[nodiscard]] bool isStringRevision(
    const QJsonValue &value,
    const qulonglong expected
)
{
    bool valid = false;
    const auto parsed = value.toString().toULongLong(&valid);
    return valid && parsed == expected;
}

[[nodiscard]] std::optional<QVariantMap> parseSnapshot(
    const QByteArray &bytes,
    const qulonglong revision,
    const QString &catalogDigest,
    const QString &actionCatalogDigest
)
{
    if (bytes.isEmpty() || bytes.size() > 4 * 1024 * 1024) return std::nullopt;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const auto object = document.object();
    if (object.value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || !isStringRevision(object.value(QStringLiteral("revision")), revision)
        || object.value(QStringLiteral("catalogDigest")).toString()
            != catalogDigest
        || object.value(QStringLiteral("actionCatalogDigest")).toString()
            != actionCatalogDigest
        || !object.value(QStringLiteral("monitors")).isArray()) {
        return std::nullopt;
    }
    return object.toVariantMap();
}

struct ParsedTopology final {
    QVariantList outputs;
    QString digest;
};

[[nodiscard]] std::optional<ParsedTopology> parseTopology(
    const QByteArray &bytes
)
{
    if (bytes.isEmpty() || bytes.size() > 1024 * 1024) return std::nullopt;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const auto object = document.object();
    const auto outputs = object.value(QStringLiteral("outputs"));
    const auto digest = object.value(QStringLiteral("topologyDigest")).toString();
    if (object.value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || !isDigest(digest)
        || !outputs.isArray() || outputs.toArray().size() > 64) {
        return std::nullopt;
    }
    for (const auto &value : outputs.toArray()) {
        if (!value.isObject()) return std::nullopt;
        const auto output = value.toObject();
        if (output.value(QStringLiteral("selector")).toString().isEmpty()
            || !output.value(QStringLiteral("modes")).isArray()) {
            return std::nullopt;
        }
    }
    return ParsedTopology{outputs.toArray().toVariantList(), digest};
}

} // namespace

CompositorClient::CompositorClient(QObject *parent)
    : CompositorClient(QDBusConnection::sessionBus(), parent)
{
}

CompositorClient::CompositorClient(
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
        &CompositorClient::serviceOwnerChanged
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

bool CompositorClient::available() const { return available_; }
bool CompositorClient::writable() const { return writable_; }
bool CompositorClient::busy() const { return busy_; }
qulonglong CompositorClient::revision() const { return revision_; }
QString CompositorClient::loadState() const { return loadState_; }
QString CompositorClient::managementState() const { return managementState_; }
QString CompositorClient::entrypointDigest() const { return entrypointDigest_; }
QString CompositorClient::catalogDigest() const { return catalogDigest_; }
QString CompositorClient::actionCatalogDigest() const
{
    return actionCatalogDigest_;
}
qulonglong CompositorClient::appliedRevision() const { return appliedRevision_; }
QString CompositorClient::applyState() const { return applyState_; }
QString CompositorClient::requiredActivation() const
{
    return requiredActivation_;
}
QString CompositorClient::generationDigest() const { return generationDigest_; }
QVariantMap CompositorClient::snapshot() const { return snapshot_; }
QVariantList CompositorClient::connectedDisplays() const
{
    return connectedDisplays_;
}
qulonglong CompositorClient::displaysObservedAtMs() const
{
    return displaysObservedAtMs_;
}
QString CompositorClient::topologyDigest() const { return topologyDigest_; }
QString CompositorClient::displayConfirmationState() const
{
    return displayConfirmationState_;
}
qulonglong CompositorClient::displayConfirmationRevision() const
{
    return displayConfirmationRevision_;
}
qulonglong CompositorClient::displayConfirmationDeadlineMs() const
{
    return displayConfirmationDeadlineMs_;
}
QString CompositorClient::displayConfirmationGeneration() const
{
    return displayConfirmationGeneration_;
}
bool CompositorClient::displayConfirmationOwned() const
{
    return displayConfirmationOwned_;
}
QString CompositorClient::lastErrorName() const { return lastErrorName_; }
QString CompositorClient::lastErrorMessage() const { return lastErrorMessage_; }

void CompositorClient::refresh()
{
    if (busy_) {
        refreshQueued_ = true;
        return;
    }
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
        connection_.asyncCall(message, ordinaryCallTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();
            if (generation != refreshGeneration_) return;
            if (reply.isError() || !applyProperties(reply.value(), true)
                || !advertisedAvailable_) {
                finishHydration(false);
                return;
            }
            fetchSnapshot(generation);
        }
    );
}

void CompositorClient::adoptManagedConfiguration()
{
    if (!available_ || !writable_ || busy_
        || managementState_ != QStringLiteral("unmanaged")) {
        setError(
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable"),
            QStringLiteral("The compositor configuration cannot be adopted right now")
        );
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("AdoptManagedConfiguration")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(revision_),
        catalogDigest_,
        actionCatalogDigest_,
        entrypointDigest_,
    });
    beginMutation(Mutation::Adopt, message, previewCallTimeoutMs);
}

void CompositorClient::applyConfiguration()
{
    if (!available_ || !writable_ || busy_
        || managementState_ != QStringLiteral("managed")
        || displayConfirmationState_ != QStringLiteral("idle")) {
        setError(
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable"),
            QStringLiteral("The compositor configuration cannot be applied right now")
        );
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("Apply")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(revision_),
        catalogDigest_,
        actionCatalogDigest_,
    });
    beginMutation(Mutation::Apply, message, previewCallTimeoutMs);
}

void CompositorClient::previewDisplayConfiguration(
    const QVariantList &outputs,
    const uint timeoutSeconds
)
{
    if (!available_ || !writable_ || busy_
        || managementState_ != QStringLiteral("managed")
        || displayConfirmationState_ != QStringLiteral("idle")
        || applyState_ != QStringLiteral("current")
        || appliedRevision_ != revision_
        || requiredActivation_ != QStringLiteral("none")
        || timeoutSeconds < 10 || timeoutSeconds > 30
        || outputs.isEmpty() || outputs.size() > 64
        || !isDigest(topologyDigest_)) {
        setError(
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable"),
            QStringLiteral("The display configuration cannot be previewed right now")
        );
        return;
    }
    QSet<QString> connectedSelectors;
    for (const auto &value : connectedDisplays_) {
        const auto selector = value.toMap().value(
            QStringLiteral("selector")
        ).toString();
        if (selector.isEmpty() || connectedSelectors.contains(selector)) {
            setError(
                QStringLiteral("org.hyprshelld.Client.Compositor.Error.InvalidTopology"),
                QStringLiteral("The connected display inventory is invalid")
            );
            return;
        }
        connectedSelectors.insert(selector);
    }
    QSet<QString> proposedSelectors;
    bool anyEnabled = false;
    for (const auto &value : outputs) {
        if (value.metaType().id() != QMetaType::QVariantMap) {
            proposedSelectors.clear();
            break;
        }
        const auto output = value.toMap();
        const auto selector = output.value(QStringLiteral("selector")).toString();
        const auto enabled = output.value(QStringLiteral("enabled"));
        if (selector.isEmpty() || proposedSelectors.contains(selector)
            || enabled.metaType().id() != QMetaType::Bool) {
            proposedSelectors.clear();
            break;
        }
        proposedSelectors.insert(selector);
        anyEnabled = anyEnabled || enabled.toBool();
    }
    if (proposedSelectors != connectedSelectors || !anyEnabled) {
        setError(
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.InvalidProfile"),
            QStringLiteral("The display preview must configure every connected output and leave at least one enabled")
        );
        return;
    }
    const QJsonObject profile{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("topologyDigest"), topologyDigest_},
        {QStringLiteral("outputs"), QJsonArray::fromVariantList(outputs)},
    };
    auto bytes = QJsonDocument(profile).toJson(QJsonDocument::Compact);
    bytes.append('\n');

    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("PreviewDisplayConfiguration")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(revision_),
        catalogDigest_,
        actionCatalogDigest_,
        bytes,
        timeoutSeconds,
    });
    beginMutation(Mutation::Preview, message, previewCallTimeoutMs);
}

void CompositorClient::confirmDisplayConfiguration()
{
    if (busy_ || displayConfirmationState_
            != QStringLiteral("awaiting-confirmation")
        || !displayConfirmationOwned_ || displayConfirmationToken_.isEmpty()) {
        setError(
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable"),
            QStringLiteral("There is no display preview to keep")
        );
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("ConfirmDisplayConfiguration")
    );
    message.setArguments({displayConfirmationToken_});
    beginMutation(Mutation::Confirm, message, ordinaryCallTimeoutMs);
}

void CompositorClient::revertDisplayConfiguration()
{
    if (busy_ || displayConfirmationState_
            != QStringLiteral("awaiting-confirmation")
        || !displayConfirmationOwned_ || displayConfirmationToken_.isEmpty()) {
        setError(
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable"),
            QStringLiteral("There is no display preview to revert")
        );
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("RevertDisplayConfiguration")
    );
    message.setArguments({displayConfirmationToken_});
    beginMutation(Mutation::Revert, message, ordinaryCallTimeoutMs);
}

void CompositorClient::clearError()
{
    if (lastErrorName_.isEmpty() && lastErrorMessage_.isEmpty()) return;
    lastErrorName_.clear();
    lastErrorMessage_.clear();
    emit lastErrorChanged();
}

void CompositorClient::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (changedInterface != interfaceName) return;
    if (!applyProperties(changed)) {
        setAvailable(false);
        return;
    }
    const QStringList hydrationProperties{
        QStringLiteral("Available"),
        QStringLiteral("Revision"),
        QStringLiteral("CatalogDigest"),
        QStringLiteral("ActionCatalogDigest"),
        QStringLiteral("DisplayConfirmationState"),
        QStringLiteral("DisplayConfirmationRevision"),
        QStringLiteral("DisplayConfirmationDeadlineMs"),
        QStringLiteral("DisplayConfirmationGeneration"),
    };
    const auto hydrationChanged = std::ranges::any_of(
        hydrationProperties,
        [&changed](const QString &name) { return changed.contains(name); }
    ) || std::ranges::any_of(
        invalidated,
        [&hydrationProperties](const QString &name) {
            return hydrationProperties.contains(name);
        }
    );
    if (hydrationChanged) refresh();
}

void CompositorClient::serviceOwnerChanged(
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
    advertisedActionCatalogDigest_.clear();
    const auto confirmationChanged = displayConfirmationOwned_
        || displayConfirmationState_ != QStringLiteral("idle")
        || displayConfirmationRevision_ != 0
        || displayConfirmationDeadlineMs_ != 0
        || !displayConfirmationGeneration_.isEmpty();
    displayConfirmationToken_.clear();
    displayConfirmationOwned_ = false;
    displayConfirmationState_ = QStringLiteral("idle");
    displayConfirmationRevision_ = 0;
    displayConfirmationDeadlineMs_ = 0;
    displayConfirmationGeneration_.clear();
    if (confirmationChanged) emit displayConfirmationChanged();
    if (managementState_ == QStringLiteral("preview")) {
        managementState_ = QStringLiteral("unmanaged");
        emit managementStateChanged();
    }
    refreshQueued_ = false;
    setAvailable(false);
    setBusy(false);
    if (!newOwner.isEmpty()) refresh();
}

void CompositorClient::fetchSnapshot(const quint64 generation)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetSnapshot")
    );
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, ordinaryCallTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != refreshGeneration_) return;
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || arguments.size() != 4
                || arguments.at(0).metaType().id() != QMetaType::QByteArray
                || arguments.at(1).metaType().id() != QMetaType::ULongLong
                || arguments.at(2).metaType().id() != QMetaType::QString
                || arguments.at(3).metaType().id() != QMetaType::QString) {
                finishHydration(false);
                return;
            }
            const auto snapshotRevision = arguments.at(1).toULongLong();
            const auto snapshotCatalog = arguments.at(2).toString();
            const auto snapshotActionCatalog = arguments.at(3).toString();
            const auto parsed = parseSnapshot(
                arguments.at(0).toByteArray(),
                snapshotRevision,
                snapshotCatalog,
                snapshotActionCatalog
            );
            if (!parsed || snapshotRevision != advertisedRevision_
                || snapshotCatalog != advertisedCatalogDigest_
                || snapshotActionCatalog != advertisedActionCatalogDigest_) {
                finishHydration(false);
                return;
            }
            const auto changed = snapshot_ != *parsed
                || revision_ != snapshotRevision;
            snapshot_ = *parsed;
            revision_ = snapshotRevision;
            catalogDigest_ = snapshotCatalog;
            actionCatalogDigest_ = snapshotActionCatalog;
            if (changed) emit snapshotChanged();
            fetchConnectedDisplays(generation);
        }
    );
}

void CompositorClient::fetchConnectedDisplays(const quint64 generation)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetConnectedDisplays")
    );
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, ordinaryCallTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != refreshGeneration_) return;
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || arguments.size() != 2
                || arguments.at(0).metaType().id() != QMetaType::QByteArray
                || arguments.at(1).metaType().id() != QMetaType::ULongLong) {
                finishHydration(false);
                return;
            }
            const auto topology = parseTopology(arguments.at(0).toByteArray());
            if (!topology) {
                finishHydration(false);
                return;
            }
            const auto observedAtMs = arguments.at(1).toULongLong();
            if (connectedDisplays_ != topology->outputs
                || topologyDigest_ != topology->digest
                || displaysObservedAtMs_ != observedAtMs) {
                connectedDisplays_ = topology->outputs;
                topologyDigest_ = topology->digest;
                displaysObservedAtMs_ = observedAtMs;
                emit connectedDisplaysChanged();
            }
            if (displayConfirmationState_
                    == QStringLiteral("awaiting-confirmation")
                && displayConfirmationToken_.isEmpty()) {
                fetchPendingDisplayConfirmation(generation);
            } else {
                finishHydration(true);
            }
        }
    );
}

void CompositorClient::fetchPendingDisplayConfirmation(
    const quint64 generation
)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetPendingDisplayConfirmation")
    );
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, ordinaryCallTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != refreshGeneration_) return;
            if (reply.type() == QDBusMessage::ErrorMessage) {
                if (reply.errorName() != QStringLiteral(
                        "org.hyprshelld.Compositor1.Error.NoDisplayConfirmation"
                    )) {
                    setError(reply.errorName(), reply.errorMessage());
                    finishHydration(false);
                    return;
                }
                const auto changed = displayConfirmationOwned_
                    || !displayConfirmationToken_.isEmpty();
                displayConfirmationOwned_ = false;
                displayConfirmationToken_.clear();
                if (changed) emit displayConfirmationChanged();
                finishHydration(true);
                return;
            }
            const auto arguments = reply.arguments();
            if (arguments.size() != 4
                || arguments.at(0).metaType().id() != QMetaType::QString
                || arguments.at(1).metaType().id() != QMetaType::ULongLong
                || arguments.at(2).metaType().id() != QMetaType::ULongLong
                || arguments.at(3).metaType().id() != QMetaType::QString
                || !isOpaqueToken(arguments.at(0).toString())
                || arguments.at(1).toULongLong()
                    != displayConfirmationRevision_
                || arguments.at(2).toULongLong()
                    != displayConfirmationDeadlineMs_
                || arguments.at(3).toString()
                    != displayConfirmationGeneration_) {
                finishHydration(false);
                return;
            }
            displayConfirmationToken_ = arguments.at(0).toString();
            if (!displayConfirmationOwned_) {
                displayConfirmationOwned_ = true;
                emit displayConfirmationChanged();
            }
            finishHydration(true);
        }
    );
}

bool CompositorClient::applyProperties(
    const QVariantMap &properties,
    const bool requireAll
)
{
    const QStringList required{
        QStringLiteral("Available"),
        QStringLiteral("Writable"),
        QStringLiteral("Revision"),
        QStringLiteral("LoadState"),
        QStringLiteral("ManagementState"),
        QStringLiteral("EntrypointDigest"),
        QStringLiteral("CatalogDigest"),
        QStringLiteral("ActionCatalogDigest"),
        QStringLiteral("AppliedRevision"),
        QStringLiteral("ApplyState"),
        QStringLiteral("RequiredActivation"),
        QStringLiteral("GenerationDigest"),
        QStringLiteral("DisplayConfirmationState"),
        QStringLiteral("DisplayConfirmationRevision"),
        QStringLiteral("DisplayConfirmationDeadlineMs"),
        QStringLiteral("DisplayConfirmationGeneration"),
    };
    if (requireAll) {
        for (const auto &name : required) {
            if (!properties.contains(name)) return false;
        }
    }
    const auto boolean = [&properties](const QString &name, bool &target) {
        const auto found = properties.constFind(name);
        if (found == properties.cend()) return true;
        if (found->metaType().id() != QMetaType::Bool) return false;
        target = found->toBool();
        return true;
    };
    const auto unsignedInteger = [&properties](
        const QString &name,
        qulonglong &target
    ) {
        const auto found = properties.constFind(name);
        if (found == properties.cend()) return true;
        if (found->metaType().id() != QMetaType::ULongLong) return false;
        target = found->toULongLong();
        return true;
    };
    const auto string = [&properties](const QString &name, QString &target) {
        const auto found = properties.constFind(name);
        if (found == properties.cend()) return true;
        if (found->metaType().id() != QMetaType::QString) return false;
        target = found->toString();
        return true;
    };

    auto nextWritable = writable_;
    auto nextLoadState = loadState_;
    auto nextManagementState = managementState_;
    auto nextEntrypointDigest = entrypointDigest_;
    auto nextAppliedRevision = appliedRevision_;
    auto nextApplyState = applyState_;
    auto nextRequiredActivation = requiredActivation_;
    auto nextGenerationDigest = generationDigest_;
    auto nextConfirmationState = displayConfirmationState_;
    auto nextConfirmationRevision = displayConfirmationRevision_;
    auto nextConfirmationDeadline = displayConfirmationDeadlineMs_;
    auto nextConfirmationGeneration = displayConfirmationGeneration_;
    if (!boolean(QStringLiteral("Available"), advertisedAvailable_)
        || !boolean(QStringLiteral("Writable"), nextWritable)
        || !unsignedInteger(QStringLiteral("Revision"), advertisedRevision_)
        || !string(QStringLiteral("LoadState"), nextLoadState)
        || !string(QStringLiteral("ManagementState"), nextManagementState)
        || !string(QStringLiteral("EntrypointDigest"), nextEntrypointDigest)
        || !string(QStringLiteral("CatalogDigest"), advertisedCatalogDigest_)
        || !string(
            QStringLiteral("ActionCatalogDigest"),
            advertisedActionCatalogDigest_
        )
        || !unsignedInteger(QStringLiteral("AppliedRevision"), nextAppliedRevision)
        || !string(QStringLiteral("ApplyState"), nextApplyState)
        || !string(QStringLiteral("RequiredActivation"), nextRequiredActivation)
        || !string(QStringLiteral("GenerationDigest"), nextGenerationDigest)
        || !string(
            QStringLiteral("DisplayConfirmationState"),
            nextConfirmationState
        )
        || !unsignedInteger(
            QStringLiteral("DisplayConfirmationRevision"),
            nextConfirmationRevision
        )
        || !unsignedInteger(
            QStringLiteral("DisplayConfirmationDeadlineMs"),
            nextConfirmationDeadline
        )
        || !string(
            QStringLiteral("DisplayConfirmationGeneration"),
            nextConfirmationGeneration
        )) {
        return false;
    }
    if (!QStringList{
            QStringLiteral("normal"), QStringLiteral("recovered"),
            QStringLiteral("defaulted"), QStringLiteral("unsupported"),
            QStringLiteral("unavailable"),
        }.contains(nextLoadState)
        || !QStringList{
            QStringLiteral("unmanaged"), QStringLiteral("managed"),
            QStringLiteral("preview"), QStringLiteral("conflict"),
        }.contains(nextManagementState)
        || !QStringList{
            QStringLiteral("unavailable"), QStringLiteral("inactive"),
            QStringLiteral("current"), QStringLiteral("retained"),
            QStringLiteral("failed"),
        }.contains(nextApplyState)
        || !QStringList{
            QStringLiteral("none"), QStringLiteral("reload"),
            QStringLiteral("restart"), QStringLiteral("session"),
        }.contains(nextRequiredActivation)
        || !QStringList{
            QStringLiteral("idle"),
            QStringLiteral("awaiting-confirmation"),
            QStringLiteral("reverting"), QStringLiteral("committing"),
            QStringLiteral("failed"),
        }.contains(nextConfirmationState)
        || (!advertisedCatalogDigest_.isEmpty()
            && !isDigest(advertisedCatalogDigest_))
        || (!advertisedActionCatalogDigest_.isEmpty()
            && !isDigest(advertisedActionCatalogDigest_))
        || (!nextGenerationDigest.isEmpty() && !isDigest(nextGenerationDigest))
        || (nextConfirmationState == QStringLiteral("awaiting-confirmation")
            && (nextConfirmationRevision == 0
                || nextConfirmationDeadline == 0
                || !isDigest(nextConfirmationGeneration)))
        || (nextConfirmationState != QStringLiteral("awaiting-confirmation")
            && (nextConfirmationRevision != 0
                || nextConfirmationDeadline != 0
                || !nextConfirmationGeneration.isEmpty()))) {
        return false;
    }

    if (nextWritable != writable_) {
        writable_ = nextWritable;
        emit writableChanged();
    }
    if (nextLoadState != loadState_) {
        loadState_ = nextLoadState;
        emit loadStateChanged();
    }
    if (nextManagementState != managementState_
        || nextEntrypointDigest != entrypointDigest_) {
        managementState_ = nextManagementState;
        entrypointDigest_ = nextEntrypointDigest;
        emit managementStateChanged();
    }
    if (advertisedCatalogDigest_ != catalogDigest_
        || advertisedActionCatalogDigest_ != actionCatalogDigest_) {
        catalogDigest_ = advertisedCatalogDigest_;
        actionCatalogDigest_ = advertisedActionCatalogDigest_;
        emit catalogDigestChanged();
    }
    if (nextAppliedRevision != appliedRevision_
        || nextApplyState != applyState_
        || nextRequiredActivation != requiredActivation_
        || nextGenerationDigest != generationDigest_) {
        appliedRevision_ = nextAppliedRevision;
        applyState_ = nextApplyState;
        requiredActivation_ = nextRequiredActivation;
        generationDigest_ = nextGenerationDigest;
        emit applyStateChanged();
    }
    if (nextConfirmationState != displayConfirmationState_
        || nextConfirmationRevision != displayConfirmationRevision_
        || nextConfirmationDeadline != displayConfirmationDeadlineMs_
        || nextConfirmationGeneration != displayConfirmationGeneration_) {
        const auto keepOwnership = nextConfirmationState
                == QStringLiteral("awaiting-confirmation")
            && displayConfirmationOwned_
            && !displayConfirmationToken_.isEmpty()
            && nextConfirmationRevision == displayConfirmationRevision_
            && nextConfirmationDeadline == displayConfirmationDeadlineMs_
            && nextConfirmationGeneration == displayConfirmationGeneration_;
        displayConfirmationState_ = nextConfirmationState;
        displayConfirmationRevision_ = nextConfirmationRevision;
        displayConfirmationDeadlineMs_ = nextConfirmationDeadline;
        displayConfirmationGeneration_ = nextConfirmationGeneration;
        if (!keepOwnership) {
            displayConfirmationToken_.clear();
            displayConfirmationOwned_ = false;
        }
        emit displayConfirmationChanged();
    }
    return !advertisedAvailable_
        || (isDigest(advertisedCatalogDigest_)
            && isDigest(advertisedActionCatalogDigest_));
}

void CompositorClient::beginMutation(
    const Mutation mutation,
    const QDBusMessage &message,
    const int timeoutMs
)
{
    clearError();
    setBusy(true);
    const auto ownerGeneration = ownerGeneration_;
    // PropertiesChanged for the durable transition can be delivered before
    // the method reply on the same bus connection. Validate that reply against
    // the tuple which authorized the request, not fields the signal may have
    // already advanced or cleared.
    const auto requestedRevision = revision_;
    const auto requestedConfirmationRevision = displayConfirmationRevision_;
    const auto requestedConfirmationGeneration =
        displayConfirmationGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, timeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, ownerGeneration, mutation, requestedRevision,
         requestedConfirmationRevision, requestedConfirmationGeneration] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_) return;
            if (reply.type() == QDBusMessage::ErrorMessage) {
                if (mutation == Mutation::Preview) {
                    displayConfirmationToken_.clear();
                    displayConfirmationOwned_ = false;
                }
                finishMutation();
                setError(reply.errorName(), reply.errorMessage());
                refresh();
                return;
            }
            const auto arguments = reply.arguments();
            const auto unsignedAt = [&arguments](const int index) {
                return index < arguments.size()
                    && arguments.at(index).metaType().id()
                        == QMetaType::ULongLong;
            };
            const auto stringAt = [&arguments](const int index) {
                return index < arguments.size()
                    && arguments.at(index).metaType().id()
                        == QMetaType::QString;
            };
            const auto structuralReplyValid = mutation == Mutation::Adopt
                ? arguments.size() == 3 && unsignedAt(0)
                    && stringAt(1) && stringAt(2)
                : mutation == Mutation::Apply
                    ? arguments.size() == 2 && unsignedAt(0) && stringAt(1)
                : mutation == Mutation::Preview
                    ? arguments.size() == 4 && unsignedAt(0)
                        && stringAt(1) && unsignedAt(2) && stringAt(3)
                    : mutation == Mutation::Confirm
                        ? arguments.size() == 2 && unsignedAt(0) && stringAt(1)
                        : arguments.size() == 1 && unsignedAt(0);
            const auto replyValid = structuralReplyValid
                && (mutation != Mutation::Adopt
                    || (isDigest(arguments.at(1).toString())
                        && isDigest(arguments.at(2).toString())))
                && (mutation != Mutation::Apply
                    || isDigest(arguments.at(1).toString()))
                && (mutation != Mutation::Preview
                    || (requestedRevision
                            != std::numeric_limits<qulonglong>::max()
                        && arguments.at(0).toULongLong()
                            == requestedRevision + 1
                        && isOpaqueToken(arguments.at(1).toString())
                        && arguments.at(2).toULongLong() != 0
                        && isDigest(arguments.at(3).toString())))
                && (mutation != Mutation::Confirm
                    || (arguments.at(0).toULongLong()
                            == requestedConfirmationRevision
                        && arguments.at(1).toString()
                            == requestedConfirmationGeneration))
                && (mutation != Mutation::Revert
                    || arguments.at(0).toULongLong() == requestedRevision);
            if (!replyValid) {
                if (mutation == Mutation::Preview) {
                    displayConfirmationToken_.clear();
                    displayConfirmationOwned_ = false;
                }
                finishMutation();
                setError(
                    QStringLiteral("org.hyprshelld.Client.Compositor.Error.InvalidReply"),
                    QStringLiteral("The compositor service returned an invalid reply")
                );
                refresh();
                return;
            }
            if (mutation == Mutation::Preview) {
                displayConfirmationRevision_ = arguments.at(0).toULongLong();
                displayConfirmationToken_ = arguments.at(1).toString();
                displayConfirmationDeadlineMs_ = arguments.at(2).toULongLong();
                displayConfirmationGeneration_ = arguments.at(3).toString();
                displayConfirmationState_ = QStringLiteral("awaiting-confirmation");
                displayConfirmationOwned_ = !displayConfirmationToken_.isEmpty()
                    && isDigest(displayConfirmationGeneration_);
                emit displayConfirmationChanged();
            } else if (mutation == Mutation::Confirm
                       || mutation == Mutation::Revert) {
                displayConfirmationToken_.clear();
                displayConfirmationOwned_ = false;
                displayConfirmationState_ = mutation == Mutation::Confirm
                    ? QStringLiteral("committing")
                    : QStringLiteral("reverting");
                displayConfirmationRevision_ = 0;
                displayConfirmationDeadlineMs_ = 0;
                displayConfirmationGeneration_.clear();
                emit displayConfirmationChanged();
            }
            finishMutation();
            refresh();
        }
    );
}

void CompositorClient::finishMutation()
{
    setBusy(false);
}

void CompositorClient::finishHydration(const bool accepted)
{
    setAvailable(accepted);
}

void CompositorClient::setAvailable(const bool available)
{
    if (available == available_) return;
    available_ = available;
    emit availableChanged();
    if (available_) clearError();
}

void CompositorClient::setBusy(const bool busy)
{
    if (busy == busy_) return;
    busy_ = busy;
    emit busyChanged();
}

void CompositorClient::setError(
    const QString &name,
    const QString &message
)
{
    if (name != lastErrorName_ || message != lastErrorMessage_) {
        lastErrorName_ = name;
        lastErrorMessage_ = message;
        emit lastErrorChanged();
    }
    emit operationFailed(name, message);
}

} // namespace HyprShelld
