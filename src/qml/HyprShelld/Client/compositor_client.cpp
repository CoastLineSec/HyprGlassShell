#include "compositor_client.h"

#include "compositor_option_catalog.h"
#include "compositor_snapshot_editor.h"
#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

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
const QString clientErrorPrefix = QStringLiteral(
    "org.hyprshelld.Client.Compositor.Error."
);

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
    return value.isString()
        && value.toString() == QString::number(expected);
}

struct ParsedSnapshot final {
    QVariantMap values;
    QJsonObject object;
};

[[nodiscard]] std::optional<ParsedSnapshot> parseSnapshot(
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
    auto canonical = Hyprland::JsonSupport::canonicalJson(object);
    canonical.append('\n');
    if (object.value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || !isStringRevision(object.value(QStringLiteral("revision")), revision)
        || object.value(QStringLiteral("catalogDigest")).toString()
            != catalogDigest
        || object.value(QStringLiteral("actionCatalogDigest")).toString()
            != actionCatalogDigest
        || canonical != bytes
        || !CompositorSnapshotEditor::isExactV1Envelope(
            object,
            revision,
            catalogDigest,
            actionCatalogDigest
        )) {
        return std::nullopt;
    }
    return ParsedSnapshot{
        .values = object.toVariantMap(),
        .object = object,
    };
}

[[nodiscard]] bool isAmbiguousReplyError(const QString &name)
{
    return name == QStringLiteral("org.freedesktop.DBus.Error.NoReply")
        || name == QStringLiteral("org.freedesktop.DBus.Error.Timeout")
        || name == QStringLiteral("org.qtproject.QtDBus.Error.NoReply")
        || name == QStringLiteral("org.qtproject.QtDBus.Error.Timeout");
}

[[nodiscard]] std::optional<QByteArray> candidateAtRevision(
    const QByteArray &candidate,
    const qulonglong revision
)
{
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(candidate, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    auto object = document.object();
    object.insert(QStringLiteral("revision"), QString::number(revision));
    auto bytes = Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    if (bytes.size() > Hyprland::maximumDesiredStateBytes) {
        return std::nullopt;
    }
    return bytes;
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

CompositorClient::~CompositorClient() = default;

bool CompositorClient::available() const { return available_; }
bool CompositorClient::writable() const { return writable_; }
bool CompositorClient::busy() const { return busy_; }
QString CompositorClient::busyOperation() const { return busyOperation_; }
qulonglong CompositorClient::revision() const { return revision_; }
QString CompositorClient::revisionToken() const
{
    return QString::number(revision_);
}
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
bool CompositorClient::catalogAvailable() const { return catalogAvailable_; }
bool CompositorClient::displayDiscoveryAvailable() const
{
    return displayDiscoveryAvailable_;
}
bool CompositorClient::appearanceAvailable() const
{
    return available_ && catalogAvailable_ && appearanceProjectionValid_
        && writable_ && !busy_
        && revision_ != std::numeric_limits<qulonglong>::max()
        && managementState_ == QStringLiteral("managed")
        && displayConfirmationState_ == QStringLiteral("idle")
        && applyState_ == QStringLiteral("current")
        && appliedRevision_ == revision_
        && requiredActivation_ == QStringLiteral("none");
}
bool CompositorClient::retryApplyAvailable() const
{
    return available_ && writable_ && !busy_
        && managementState_ == QStringLiteral("managed")
        && displayConfirmationState_ == QStringLiteral("idle")
        && (applyState_ == QStringLiteral("retained")
            || applyState_ == QStringLiteral("failed"))
        && appliedRevision_ != revision_
        && requiredActivation_ == QStringLiteral("reload");
}
bool CompositorClient::recoveryAvailable() const
{
    return available_ && writable_ && !busy_
        && revision_ != std::numeric_limits<qulonglong>::max()
        && managementState_ == QStringLiteral("managed")
        && displayConfirmationState_ == QStringLiteral("idle")
        && isDigest(generationDigest_)
        && appliedRevision_ != revision_
        && (applyState_ == QStringLiteral("retained")
            || applyState_ == QStringLiteral("failed"));
}
QVariantList CompositorClient::appearanceOptions() const
{
    return catalogAvailable_ && optionCatalog_
        ? optionCatalog_->appearanceOptions()
        : QVariantList{};
}
QVariantMap CompositorClient::appearanceValues() const
{
    return catalogAvailable_ && appearanceProjectionValid_
        ? appearanceValues_
        : QVariantMap{};
}
QString CompositorClient::appearanceErrorName() const
{
    return appearanceErrorName_;
}
QString CompositorClient::appearanceErrorMessage() const
{
    return appearanceErrorMessage_;
}
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
    setCatalogAvailable(false);
    setDisplayDiscoveryAvailable(false);
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

void CompositorClient::saveAppearance(const QVariantMap &values)
{
    if (!appearanceAvailable() || optionCatalog_ == nullptr) {
        setError(
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("The appearance configuration cannot be saved right now")
        );
        return;
    }
    QString error;
    const auto edit = CompositorSnapshotEditor::replaceAppearance(
        snapshotObject_,
        revision_,
        catalogDigest_,
        actionCatalogDigest_,
        *optionCatalog_,
        values,
        error
    );
    if (!edit) {
        setError(
            clientErrorPrefix + QStringLiteral("InvalidAppearance"),
            error
        );
        return;
    }
    if (!edit->changed) {
        setError(
            clientErrorPrefix + QStringLiteral("NoChanges"),
            QStringLiteral("The appearance configuration has not changed")
        );
        return;
    }
    clearError();
    setBusy(true);
    setBusyOperation(QStringLiteral("appearance-save"));
    sendAppearanceReplace(
        {
            .candidate = edit->candidate,
            .expectedRevision = revision_,
            .catalogDigest = catalogDigest_,
            .actionCatalogDigest = actionCatalogDigest_,
        },
        false
    );
}

void CompositorClient::retryApply()
{
    if (!retryApplyAvailable()) {
        setError(
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("There is no saved compositor configuration to apply")
        );
        return;
    }
    clearError();
    setBusy(true);
    setBusyOperation(QStringLiteral("appearance-apply"));
    sendApplyRequest(
        {
            .revision = revision_,
            .catalogDigest = catalogDigest_,
            .actionCatalogDigest = actionCatalogDigest_,
        },
        false
    );
}

void CompositorClient::recoverConfiguration()
{
    if (!recoveryAvailable()) {
        setError(
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("There is no last working compositor configuration to restore")
        );
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("Recover")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(revision_),
        catalogDigest_,
        actionCatalogDigest_,
    });
    beginMutation(Mutation::Recover, message, previewCallTimeoutMs);
}

void CompositorClient::previewDisplayConfiguration(
    const QVariantList &outputs,
    const uint timeoutSeconds
)
{
    if (!available_ || !writable_ || busy_
        || managementState_ != QStringLiteral("managed")
        || !displayDiscoveryAvailable_
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
    optionCatalog_.reset();
    catalogOwnerGeneration_ = 0;
    appearanceProjectionValid_ = false;
    appearanceValues_.clear();
    setAppearanceError({}, {});
    setCatalogAvailable(false);
    setDisplayDiscoveryAvailable(false);
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
        emit appearanceChanged();
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
            const auto changed = snapshot_ != parsed->values
                || revision_ != snapshotRevision;
            snapshot_ = parsed->values;
            snapshotObject_ = parsed->object;
            revision_ = snapshotRevision;
            catalogDigest_ = snapshotCatalog;
            actionCatalogDigest_ = snapshotActionCatalog;
            if (changed) emit snapshotChanged();
            updateAppearanceProjection();
            setAvailable(true);
            fetchOptionCatalog(generation);
            fetchConnectedDisplays(generation);
        }
    );
}

void CompositorClient::fetchOptionCatalog(const quint64 generation)
{
    if (optionCatalog_ != nullptr
        && catalogOwnerGeneration_ == ownerGeneration_
        && optionCatalog_->digest() == catalogDigest_) {
        setCatalogAvailable(true);
        updateAppearanceProjection();
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetOptionCatalog")
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
                || arguments.at(1).metaType().id() != QMetaType::QString) {
                optionCatalog_.reset();
                setCatalogAvailable(false);
                setAppearanceError(
                    reply.type() == QDBusMessage::ErrorMessage
                        ? reply.errorName()
                        : clientErrorPrefix + QStringLiteral("InvalidCatalogReply"),
                    reply.type() == QDBusMessage::ErrorMessage
                        ? reply.errorMessage()
                        : QStringLiteral("The compositor service returned an invalid option catalog reply")
                );
                return;
            }
            QString error;
            auto parsed = CompositorOptionCatalog::fromBytes(
                arguments.at(0).toByteArray(),
                arguments.at(1).toString(),
                catalogDigest_,
                error
            );
            if (!parsed) {
                optionCatalog_.reset();
                setCatalogAvailable(false);
                setAppearanceError(
                    clientErrorPrefix + QStringLiteral("InvalidCatalog"),
                    error
                );
                return;
            }
            optionCatalog_ = std::make_unique<CompositorOptionCatalog>(
                std::move(*parsed)
            );
            catalogOwnerGeneration_ = ownerGeneration_;
            setCatalogAvailable(true);
            setAppearanceError({}, {});
            updateAppearanceProjection();
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
                setDisplayDiscoveryAvailable(false);
                return;
            }
            const auto topology = parseTopology(arguments.at(0).toByteArray());
            if (!topology) {
                setDisplayDiscoveryAvailable(false);
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
            setDisplayDiscoveryAvailable(true);
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
                    setDisplayDiscoveryAvailable(false);
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
                setDisplayDiscoveryAvailable(false);
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
        emit appearanceChanged();
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
        emit appearanceChanged();
    }
    if (advertisedCatalogDigest_ != catalogDigest_
        || advertisedActionCatalogDigest_ != actionCatalogDigest_) {
        const auto optionAuthorityChanged =
            advertisedCatalogDigest_ != catalogDigest_;
        catalogDigest_ = advertisedCatalogDigest_;
        actionCatalogDigest_ = advertisedActionCatalogDigest_;
        if (optionAuthorityChanged) setCatalogAvailable(false);
        emit catalogDigestChanged();
        emit appearanceChanged();
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
        emit appearanceChanged();
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
        emit appearanceChanged();
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
    setBusyOperation(
        mutation == Mutation::Adopt ? QStringLiteral("adopt")
        : mutation == Mutation::Apply ? QStringLiteral("apply")
        : mutation == Mutation::Preview ? QStringLiteral("display-preview")
        : mutation == Mutation::Confirm ? QStringLiteral("display-confirm")
        : mutation == Mutation::Revert ? QStringLiteral("display-revert")
        : QStringLiteral("recover")
    );
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
                    : mutation == Mutation::Recover
                        ? arguments.size() == 3 && unsignedAt(0)
                            && unsignedAt(1) && stringAt(2)
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
                    || arguments.at(0).toULongLong() == requestedRevision)
                && (mutation != Mutation::Recover
                    || (requestedRevision
                            != std::numeric_limits<qulonglong>::max()
                        && arguments.at(0).toULongLong()
                            == requestedRevision + 1
                        && arguments.at(1).toULongLong()
                            == requestedRevision + 1
                        && isDigest(arguments.at(2).toString())));
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

void CompositorClient::sendAppearanceReplace(
    const AppearanceSaveRequest &request,
    const bool retry
)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("ReplaceSnapshot")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(request.expectedRevision),
        request.catalogDigest,
        request.actionCatalogDigest,
        request.candidate,
    });
    const auto ownerGeneration = ownerGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, ordinaryCallTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, ownerGeneration, request, retry] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_) return;
            if (reply.type() == QDBusMessage::ErrorMessage) {
                if (isAmbiguousReplyError(reply.errorName())) {
                    if (!retry) {
                        sendAppearanceReplace(request, true);
                    } else {
                        // Re-read the exact authority tuple. If the first or
                        // retry call committed, verification proceeds to Apply;
                        // otherwise it fails without another increment attempt.
                        verifyAppearanceReplacement(request);
                    }
                    return;
                }
                finishMutation();
                setError(reply.errorName(), reply.errorMessage());
                refresh();
                return;
            }
            const auto arguments = reply.arguments();
            const auto expectedRevision = request.expectedRevision
                == std::numeric_limits<qulonglong>::max()
                ? request.expectedRevision
                : request.expectedRevision + 1;
            if (arguments.size() != 1
                || arguments.at(0).metaType().id() != QMetaType::ULongLong
                || arguments.at(0).toULongLong() != expectedRevision) {
                finishMutation();
                setError(
                    clientErrorPrefix + QStringLiteral("InvalidReply"),
                    QStringLiteral("The compositor service returned an invalid replacement reply")
                );
                refresh();
                return;
            }
            verifyAppearanceReplacement(request);
        }
    );
}

void CompositorClient::verifyAppearanceReplacement(
    const AppearanceSaveRequest &request
)
{
    const auto ownerGeneration = ownerGeneration_;
    auto propertiesMessage = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("GetAll")
    );
    propertiesMessage.setArguments({interfaceName});
    auto *propertiesWatcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(propertiesMessage, ordinaryCallTimeoutMs),
        this
    );
    connect(
        propertiesWatcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, propertiesWatcher, ownerGeneration, request] {
            const QDBusPendingReply<QVariantMap> reply = *propertiesWatcher;
            propertiesWatcher->deleteLater();
            if (ownerGeneration != ownerGeneration_) return;
            const auto nextRevision = request.expectedRevision
                == std::numeric_limits<qulonglong>::max()
                ? request.expectedRevision
                : request.expectedRevision + 1;
            if (reply.isError() || !applyProperties(reply.value(), true)
                || !advertisedAvailable_
                || advertisedRevision_ != nextRevision
                || advertisedCatalogDigest_ != request.catalogDigest
                || advertisedActionCatalogDigest_
                    != request.actionCatalogDigest
                || !writable_
                || managementState_ != QStringLiteral("managed")
                || displayConfirmationState_ != QStringLiteral("idle")) {
                finishMutation();
                setError(
                    clientErrorPrefix + QStringLiteral("ReplacementUnconfirmed"),
                    QStringLiteral("The saved compositor configuration could not be confirmed")
                );
                refresh();
                return;
            }

            auto snapshotMessage = QDBusMessage::createMethodCall(
                serviceName,
                objectPath,
                interfaceName,
                QStringLiteral("GetSnapshot")
            );
            auto *snapshotWatcher = new QDBusPendingCallWatcher(
                connection_.asyncCall(snapshotMessage, ordinaryCallTimeoutMs),
                this
            );
            connect(
                snapshotWatcher,
                &QDBusPendingCallWatcher::finished,
                this,
                [this, snapshotWatcher, ownerGeneration, request,
                 nextRevision] {
                    const auto snapshotReply = snapshotWatcher->reply();
                    snapshotWatcher->deleteLater();
                    if (ownerGeneration != ownerGeneration_) return;
                    const auto arguments = snapshotReply.arguments();
                    const auto expected = candidateAtRevision(
                        request.candidate, nextRevision
                    );
                    if (snapshotReply.type() == QDBusMessage::ErrorMessage
                        || arguments.size() != 4
                        || arguments.at(0).metaType().id()
                            != QMetaType::QByteArray
                        || arguments.at(1).metaType().id()
                            != QMetaType::ULongLong
                        || arguments.at(2).metaType().id()
                            != QMetaType::QString
                        || arguments.at(3).metaType().id()
                            != QMetaType::QString
                        || arguments.at(1).toULongLong() != nextRevision
                        || arguments.at(2).toString()
                            != request.catalogDigest
                        || arguments.at(3).toString()
                            != request.actionCatalogDigest
                        || !expected
                        || arguments.at(0).toByteArray() != *expected) {
                        finishMutation();
                        setError(
                            clientErrorPrefix + QStringLiteral("ReplacementUnconfirmed"),
                            QStringLiteral("The saved compositor snapshot did not match the requested appearance configuration")
                        );
                        refresh();
                        return;
                    }
                    const auto parsed = parseSnapshot(
                        arguments.at(0).toByteArray(),
                        nextRevision,
                        request.catalogDigest,
                        request.actionCatalogDigest
                    );
                    if (!parsed) {
                        finishMutation();
                        setError(
                            clientErrorPrefix + QStringLiteral("InvalidSnapshot"),
                            QStringLiteral("The saved compositor snapshot is invalid")
                        );
                        refresh();
                        return;
                    }
                    const auto changed = snapshot_ != parsed->values
                        || revision_ != nextRevision;
                    snapshot_ = parsed->values;
                    snapshotObject_ = parsed->object;
                    revision_ = nextRevision;
                    catalogDigest_ = request.catalogDigest;
                    actionCatalogDigest_ = request.actionCatalogDigest;
                    if (changed) emit snapshotChanged();
                    updateAppearanceProjection();

                    if (applyState_ == QStringLiteral("current")
                        && appliedRevision_ == nextRevision
                        && requiredActivation_ == QStringLiteral("none")) {
                        finishMutation();
                        refresh();
                        return;
                    }
                    if (requiredActivation_ != QStringLiteral("reload")
                        || (applyState_ != QStringLiteral("retained")
                            && applyState_ != QStringLiteral("failed"))) {
                        finishMutation();
                        setError(
                            clientErrorPrefix + QStringLiteral("ActivationRequired"),
                            QStringLiteral("The appearance configuration was saved but cannot be reloaded automatically")
                        );
                        refresh();
                        return;
                    }
                    setBusyOperation(QStringLiteral("appearance-apply"));
                    sendApplyRequest(
                        {
                            .revision = nextRevision,
                            .catalogDigest = request.catalogDigest,
                            .actionCatalogDigest =
                                request.actionCatalogDigest,
                        },
                        false
                    );
                }
            );
        }
    );
}

void CompositorClient::sendApplyRequest(
    const ApplyRequest &request,
    const bool retry
)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("Apply")
    );
    message.setArguments({
        QVariant::fromValue<qulonglong>(request.revision),
        request.catalogDigest,
        request.actionCatalogDigest,
    });
    const auto ownerGeneration = ownerGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, previewCallTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, ownerGeneration, request, retry] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_) return;
            if (reply.type() == QDBusMessage::ErrorMessage) {
                if (isAmbiguousReplyError(reply.errorName())) {
                    const auto alreadyCurrent =
                        advertisedRevision_ == request.revision
                        && advertisedCatalogDigest_ == request.catalogDigest
                        && advertisedActionCatalogDigest_
                            == request.actionCatalogDigest
                        && appliedRevision_ == request.revision
                        && applyState_ == QStringLiteral("current")
                        && requiredActivation_ == QStringLiteral("none")
                        && isDigest(generationDigest_);
                    if (alreadyCurrent) {
                        finishMutation();
                        refresh();
                    } else if (!retry) {
                        sendApplyRequest(request, true);
                    } else {
                        reconcileApplyOutcome(request);
                    }
                    return;
                }
                finishMutation();
                setError(reply.errorName(), reply.errorMessage());
                refresh();
                return;
            }
            const auto arguments = reply.arguments();
            if (arguments.size() != 2
                || arguments.at(0).metaType().id() != QMetaType::ULongLong
                || arguments.at(1).metaType().id() != QMetaType::QString
                || arguments.at(0).toULongLong() != request.revision
                || !isDigest(arguments.at(1).toString())) {
                finishMutation();
                setError(
                    clientErrorPrefix + QStringLiteral("InvalidReply"),
                    QStringLiteral("The compositor service returned an invalid apply reply")
                );
                refresh();
                return;
            }
            finishMutation();
            refresh();
        }
    );
}

void CompositorClient::reconcileApplyOutcome(const ApplyRequest &request)
{
    const auto ownerGeneration = ownerGeneration_;
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
        [this, watcher, ownerGeneration, request] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();
            if (ownerGeneration != ownerGeneration_) return;
            const auto confirmed = !reply.isError()
                && applyProperties(reply.value(), true)
                && advertisedAvailable_
                && advertisedRevision_ == request.revision
                && advertisedCatalogDigest_ == request.catalogDigest
                && advertisedActionCatalogDigest_
                    == request.actionCatalogDigest
                && appliedRevision_ == request.revision
                && applyState_ == QStringLiteral("current")
                && requiredActivation_ == QStringLiteral("none")
                && isDigest(generationDigest_);
            finishMutation();
            if (!confirmed) {
                setError(
                    clientErrorPrefix + QStringLiteral("ApplyUnconfirmed"),
                    QStringLiteral("The compositor reload outcome could not be confirmed")
                );
            }
            refresh();
        }
    );
}

void CompositorClient::finishMutation()
{
    setBusyOperation({});
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
    emit appearanceChanged();
}

void CompositorClient::setCatalogAvailable(const bool available)
{
    if (available == catalogAvailable_) return;
    catalogAvailable_ = available;
    emit appearanceChanged();
}

void CompositorClient::setDisplayDiscoveryAvailable(const bool available)
{
    if (available == displayDiscoveryAvailable_) return;
    displayDiscoveryAvailable_ = available;
    emit displayDiscoveryAvailableChanged();
}

void CompositorClient::setBusy(const bool busy)
{
    if (!busy) setBusyOperation({});
    if (busy == busy_) return;
    busy_ = busy;
    emit busyChanged();
    emit appearanceChanged();
}

void CompositorClient::setBusyOperation(const QString &operation)
{
    if (operation == busyOperation_) return;
    busyOperation_ = operation;
    emit busyOperationChanged();
}

void CompositorClient::setAppearanceError(
    const QString &name,
    const QString &message
)
{
    if (name == appearanceErrorName_ && message == appearanceErrorMessage_) {
        return;
    }
    appearanceErrorName_ = name;
    appearanceErrorMessage_ = message;
    emit appearanceChanged();
}

void CompositorClient::updateAppearanceProjection()
{
    QVariantMap nextValues;
    auto nextValid = false;
    QString error;
    if (catalogAvailable_ && optionCatalog_ != nullptr
        && optionCatalog_->digest() == catalogDigest_) {
        const auto values = optionCatalog_->appearanceValues(
            snapshotObject_, error
        );
        if (values) {
            nextValues = *values;
            nextValid = true;
            setAppearanceError({}, {});
        } else {
            setAppearanceError(
                clientErrorPrefix + QStringLiteral("InvalidAppearanceSnapshot"),
                error
            );
        }
    }
    if (nextValid == appearanceProjectionValid_
        && nextValues == appearanceValues_) {
        return;
    }
    appearanceProjectionValid_ = nextValid;
    appearanceValues_ = std::move(nextValues);
    emit appearanceChanged();
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
