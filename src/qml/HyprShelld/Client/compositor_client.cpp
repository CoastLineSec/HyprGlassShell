#include "compositor_client.h"

#include "compositor_option_catalog.h"
#include "compositor_snapshot_editor.h"
#include "hyprland/default_keybindings.h"
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
constexpr qsizetype maximumAuthorityErrorCodeUnits = 1024;
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

[[nodiscard]] QJsonObject protectedWorkspaceRule()
{
    return {
        {
            QStringLiteral("id"),
            QLatin1String(Hyprland::sharedSpacingWorkspaceRuleId),
        },
        {
            QStringLiteral("selector"),
            QLatin1String(Hyprland::sharedSpacingWorkspaceRuleSelector),
        },
        {QStringLiteral("enabled"), true},
        {QStringLiteral("monitor"), QString()},
        {QStringLiteral("persistent"), false},
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QString()},
        {
            QStringLiteral("overrides"),
            QJsonObject{{
                QStringLiteral("gaps_out"), QJsonArray{0, 0, 0, 0},
            }},
        },
    };
}

[[nodiscard]] bool isProtectedWorkspaceIdentity(const QJsonObject &record)
{
    return record.value(QStringLiteral("id")).toString()
            == QLatin1String(Hyprland::sharedSpacingWorkspaceRuleId)
        || record.value(QStringLiteral("selector")).toString()
            == QLatin1String(Hyprland::sharedSpacingWorkspaceRuleSelector);
}

[[nodiscard]] bool isPinchGestureDirection(const QString &direction)
{
    return direction == QStringLiteral("pinch")
        || direction == QStringLiteral("pinchIn")
        || direction == QStringLiteral("pinchOut");
}

[[nodiscard]] QString gestureCompatibilityReason(const QJsonObject &gesture)
{
    const auto direction = gesture.value(QStringLiteral("direction")).toString();
    const auto pinch = isPinchGestureDirection(direction);
    const auto action = gesture.value(QStringLiteral("action")).toObject();
    const auto actionType = action.value(QStringLiteral("type")).toString();
    if (actionType == QStringLiteral("unset")) {
        return QStringLiteral(
            "This exact Unset record is preserved for compatibility and cannot be edited"
        );
    }
    if (pinch && gesture.value(QStringLiteral("scale")).toDouble() != 1.0) {
        return QStringLiteral(
            "Hyprland ignores scale for pinch gestures; this exact record is preserved for compatibility"
        );
    }
    if (pinch && actionType == QStringLiteral("scrollMove")) {
        return QStringLiteral(
            "Scroll Move does not act on pinch gestures; this exact record is preserved for compatibility"
        );
    }
    if (!pinch && actionType == QStringLiteral("cursorZoom")
        && action.value(QStringLiteral("mode")).toString()
            == QStringLiteral("live")) {
        return QStringLiteral(
            "Live cursor zoom only acts on pinch gestures; this exact record is preserved for compatibility"
        );
    }
    return {};
}

[[nodiscard]] QVariantList gestureCompatibilityProjection(
    const QJsonArray &gestures
)
{
    QVariantList result;
    result.reserve(gestures.size());
    for (const auto &value : gestures) {
        const auto gesture = value.toObject();
        const auto reason = gestureCompatibilityReason(gesture);
        result.append(QVariantMap{
            {QStringLiteral("id"), gesture.value(QStringLiteral("id")).toString()},
            {QStringLiteral("editable"), reason.isEmpty()},
            {QStringLiteral("reason"), reason},
        });
    }
    return result;
}

[[nodiscard]] QVariantList authoredGestureActions(
    const CompositorActionCatalog *catalog
)
{
    if (catalog == nullptr) return {};
    static const QStringList expected{
        QStringLiteral("close"), QStringLiteral("cursorZoom"),
        QStringLiteral("float"), QStringLiteral("fullscreen"),
        QStringLiteral("move"), QStringLiteral("resize"),
        QStringLiteral("scrollMove"), QStringLiteral("special"),
        QStringLiteral("unset"), QStringLiteral("workspace"),
    };
    const auto &actions = catalog->catalog().gestureActions;
    if (actions.size() != expected.size()) return {};
    for (qsizetype index = 0; index < actions.size(); ++index) {
        if (actions.at(index).id != expected.at(index)
            || actions.at(index).label.isEmpty()
            || actions.at(index).description.isEmpty()) {
            return {};
        }
    }
    QVariantList result;
    result.reserve(actions.size() - 1);
    for (const auto &action : actions) {
        if (action.id == QStringLiteral("unset")) continue;
        result.append(QVariantMap{
            {QStringLiteral("id"), action.id},
            {QStringLiteral("label"), action.label},
            {QStringLiteral("description"), action.description},
        });
    }
    return result;
}

[[nodiscard]] QString bindingActionType(const Hyprland::ActionKind kind)
{
    switch (kind) {
    case Hyprland::ActionKind::Dispatcher:
        return QStringLiteral("dispatcher");
    case Hyprland::ActionKind::DefaultApp:
        return QStringLiteral("defaultApp");
    case Hyprland::ActionKind::HyprShelld:
        return QStringLiteral("hyprshelld");
    case Hyprland::ActionKind::Gesture:
        return {};
    }
    return {};
}

[[nodiscard]] QVariantList authoredBindingActions(
    const CompositorActionCatalog *catalog
)
{
    if (catalog == nullptr
        || catalog->catalog().dispatcherActions.size() != 47
        || catalog->catalog().semanticActions.size() != 29) {
        return {};
    }

    QVariantList result;
    result.reserve(76);
    const auto append = [&result](const Hyprland::ActionDefinition &action) {
        const auto actionType = bindingActionType(action.kind);
        if (actionType.isEmpty() || action.id.isEmpty()
            || action.label.isEmpty() || action.description.isEmpty()) {
            return false;
        }
        result.append(QVariantMap{
            {QStringLiteral("id"), action.id},
            {QStringLiteral("label"), action.label},
            {QStringLiteral("description"), action.description},
            {QStringLiteral("actionType"), actionType},
            {QStringLiteral("kind"), actionType},
            {QStringLiteral("luaPath"), action.luaPath},
            {QStringLiteral("uiTier"), Hyprland::toString(action.uiTier)},
            {QStringLiteral("risk"), Hyprland::toString(action.risk)},
            {QStringLiteral("schemaReference"), action.schemaReference},
            {QStringLiteral("documentation"), action.documentation},
        });
        return true;
    };
    for (const auto &action : catalog->catalog().dispatcherActions) {
        if (!append(action)) return {};
    }
    for (const auto &action : catalog->catalog().semanticActions) {
        if (!append(action)) return {};
    }
    return result;
}

[[nodiscard]] QVariantMap authoredBindingOptions(
    const Hyprland::BindingOptions &options
)
{
    QVariantMap result{
        {QStringLiteral("repeating"), options.repeating},
        {QStringLiteral("locked"), options.locked},
        {QStringLiteral("release"), options.release},
        {QStringLiteral("nonConsuming"), options.nonConsuming},
        {QStringLiteral("autoConsuming"), options.autoConsuming},
        {QStringLiteral("transparent"), options.transparent},
        {QStringLiteral("ignoreMods"), options.ignoreMods},
        {QStringLiteral("dontInhibit"), options.dontInhibit},
        {QStringLiteral("longPress"), options.longPress},
        {QStringLiteral("submapUniversal"), options.submapUniversal},
        {QStringLiteral("click"), options.click},
        {QStringLiteral("drag"), options.drag},
        {QStringLiteral("allowInputCapture"), options.allowInputCapture},
    };
    if (options.device) {
        result.insert(
            QStringLiteral("device"),
            QVariantMap{
                {QStringLiteral("inclusive"), options.device->inclusive},
                {QStringLiteral("list"), options.device->list},
            }
        );
    }
    return result;
}

[[nodiscard]] QVariantList authoredDefaultBindings(
    const CompositorActionCatalog *catalog
)
{
    if (catalog == nullptr) return {};
    const auto &defaults = Hyprland::shippedDefaultKeybindings();
    QVariantList result;
    result.reserve(defaults.size());
    for (const auto &binding : defaults) {
        const auto *action = Hyprland::findAction(
            catalog->catalog(), Hyprland::ActionKind::Dispatcher,
            binding.action
        );
        if (action == nullptr) return {};
        result.append(QVariantMap{
            {QStringLiteral("id"), binding.id},
            {QStringLiteral("modifiers"), binding.modifiers},
            {QStringLiteral("key"), binding.key},
            {QStringLiteral("actionType"), QStringLiteral("dispatcher")},
            {QStringLiteral("action"), binding.action},
            {QStringLiteral("arguments"), binding.arguments.toVariantMap()},
            {QStringLiteral("description"), binding.description},
            {QStringLiteral("enabled"), binding.enabled},
            {QStringLiteral("submap"), binding.submap},
            {QStringLiteral("options"), authoredBindingOptions(binding.options)},
        });
    }
    return result;
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
bool CompositorClient::allOptionsAvailable() const
{
    return optionGroupAvailable(OptionGroup::AllOptions);
}
QVariantList CompositorClient::allOptions() const
{
    return catalogAvailable_ && optionCatalog_
        ? optionCatalog_->allOptions()
        : QVariantList{};
}
QVariantMap CompositorClient::allValues() const
{
    const auto *state = optionGroupState(OptionGroup::AllOptions);
    return catalogAvailable_ && state != nullptr && state->projectionValid
        ? state->values : QVariantMap{};
}
QString CompositorClient::allOptionsErrorName() const
{
    return scopedErrorName(OptionGroup::AllOptions);
}
QString CompositorClient::allOptionsErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::AllOptions);
}
bool CompositorClient::actionCatalogAvailable() const
{
    return actionCatalogAvailable_
        && authoredBindingActions(actionCatalog_.get()).size() == 76;
}
bool CompositorClient::bindingsAvailable() const
{
    return optionGroupAvailable(OptionGroup::Bindings);
}
bool CompositorClient::bindingsProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Bindings);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->projectionValid && actionCatalogAvailable()
        && authoredDefaultBindings(actionCatalog_.get()).size()
            == Hyprland::shippedDefaultKeybindingCount;
}
QVariantList CompositorClient::defaultBindings() const
{
    return actionCatalogAvailable()
        ? authoredDefaultBindings(actionCatalog_.get()) : QVariantList{};
}
QVariantList CompositorClient::bindings() const
{
    const auto *state = optionGroupState(OptionGroup::Bindings);
    return bindingsProjectionAvailable() && state != nullptr
        ? state->bindings : QVariantList{};
}
QVariantList CompositorClient::submaps() const
{
    const auto *state = optionGroupState(OptionGroup::Bindings);
    return bindingsProjectionAvailable() && state != nullptr
        ? state->submaps : QVariantList{};
}
QVariantList CompositorClient::bindingActions() const
{
    return actionCatalogAvailable()
        ? authoredBindingActions(actionCatalog_.get()) : QVariantList{};
}
QString CompositorClient::bindingsErrorName() const
{
    return scopedErrorName(OptionGroup::Bindings);
}
QString CompositorClient::bindingsErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Bindings);
}
bool CompositorClient::environmentAvailable() const
{
    return optionGroupAvailable(OptionGroup::Environment);
}
bool CompositorClient::environmentProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Environment);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->projectionValid;
}
QVariantList CompositorClient::environmentVariables() const
{
    const auto *state = optionGroupState(OptionGroup::Environment);
    return environmentProjectionAvailable() && state != nullptr
        ? state->environment : QVariantList{};
}
QString CompositorClient::environmentErrorName() const
{
    return scopedErrorName(OptionGroup::Environment);
}
QString CompositorClient::environmentErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Environment);
}
bool CompositorClient::permissionsAvailable() const
{
    return optionGroupAvailable(OptionGroup::Permissions);
}
bool CompositorClient::permissionsProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Permissions);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->projectionValid;
}
QVariantList CompositorClient::permissions() const
{
    const auto *state = optionGroupState(OptionGroup::Permissions);
    return permissionsProjectionAvailable() && state != nullptr
        ? state->permissions : QVariantList{};
}
QString CompositorClient::permissionErrorName() const
{
    return scopedErrorName(OptionGroup::Permissions);
}
QString CompositorClient::permissionErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Permissions);
}
bool CompositorClient::displayDiscoveryAvailable() const
{
    return displayDiscoveryAvailable_;
}
bool CompositorClient::appearanceAvailable() const
{
    return optionGroupAvailable(OptionGroup::Appearance);
}
bool CompositorClient::appearanceProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Appearance);
    return available_ && catalogAvailable_ && state != nullptr
        && state->projectionValid;
}
bool CompositorClient::appearanceAnimationProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Appearance);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->appearanceAnimationProjectionValid;
}
bool CompositorClient::inputAvailable() const
{
    return optionGroupAvailable(OptionGroup::Input);
}
bool CompositorClient::inputProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Input);
    return available_ && catalogAvailable_ && state != nullptr
        && state->projectionValid;
}
bool CompositorClient::inputGesturesProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Input);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->inputGesturesProjectionValid
        && authoredGestureActions(actionCatalog_.get()).size() == 9;
}
bool CompositorClient::windowsAvailable() const
{
    return optionGroupAvailable(OptionGroup::Windows);
}
bool CompositorClient::windowsProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Windows);
    return available_ && catalogAvailable_ && state != nullptr
        && state->projectionValid;
}
bool CompositorClient::workspacesAvailable() const
{
    return optionGroupAvailable(OptionGroup::Workspaces);
}
bool CompositorClient::workspacesProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Workspaces);
    return available_ && catalogAvailable_ && state != nullptr
        && state->projectionValid;
}
bool CompositorClient::advancedAvailable() const
{
    return optionGroupAvailable(OptionGroup::Advanced);
}
bool CompositorClient::advancedProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Advanced);
    return available_ && catalogAvailable_ && state != nullptr
        && state->projectionValid;
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
    const auto *state = optionGroupState(OptionGroup::Appearance);
    return catalogAvailable_ && state != nullptr && state->projectionValid
        ? state->values : QVariantMap{};
}
QVariantList CompositorClient::appearanceCurves() const
{
    const auto *state = optionGroupState(OptionGroup::Appearance);
    return appearanceAnimationProjectionAvailable() && state != nullptr
        ? state->appearanceCurves : QVariantList{};
}
QVariantList CompositorClient::appearanceAnimations() const
{
    const auto *state = optionGroupState(OptionGroup::Appearance);
    return appearanceAnimationProjectionAvailable() && state != nullptr
        ? state->appearanceAnimations : QVariantList{};
}
QString CompositorClient::appearanceErrorName() const
{
    return scopedErrorName(OptionGroup::Appearance);
}
QString CompositorClient::appearanceErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Appearance);
}
QVariantList CompositorClient::inputOptions() const
{
    return catalogAvailable_ && optionCatalog_
        ? optionCatalog_->inputOptions()
        : QVariantList{};
}
QVariantMap CompositorClient::inputValues() const
{
    const auto *state = optionGroupState(OptionGroup::Input);
    return catalogAvailable_ && state != nullptr && state->projectionValid
        ? state->values : QVariantMap{};
}
QVariantList CompositorClient::inputGestures() const
{
    const auto *state = optionGroupState(OptionGroup::Input);
    return inputGesturesProjectionAvailable() && state != nullptr
        ? state->inputGestures : QVariantList{};
}
QVariantList CompositorClient::inputGestureCompatibility() const
{
    const auto *state = optionGroupState(OptionGroup::Input);
    return inputGesturesProjectionAvailable() && state != nullptr
        ? state->inputGestureCompatibility : QVariantList{};
}
QVariantList CompositorClient::inputGestureActions() const
{
    return inputGesturesProjectionAvailable()
        ? authoredGestureActions(actionCatalog_.get()) : QVariantList{};
}
QString CompositorClient::inputErrorName() const
{
    return scopedErrorName(OptionGroup::Input);
}
QString CompositorClient::inputErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Input);
}
bool CompositorClient::inputDeviceDiscoveryAvailable() const
{
    return inputDeviceDiscoveryAvailable_;
}
bool CompositorClient::inputDeviceDiscoveryBusy() const
{
    return inputDeviceDiscoveryBusy_;
}
QVariantList CompositorClient::connectedInputDevices() const
{
    return inputDeviceProjection_.connectedDevices;
}
qulonglong CompositorClient::inputDevicesObservedAtMs() const
{
    return inputDevicesObservedAtMs_;
}
QString CompositorClient::inputDeviceInventoryDigest() const
{
    return connectedInputDeviceInventory_
        ? connectedInputDeviceInventory_->inventoryDigest : QString{};
}
QVariantMap CompositorClient::inputDeviceUnaddressableCounts() const
{
    if (!connectedInputDeviceInventory_) return {};
    const auto &counts = connectedInputDeviceInventory_->unaddressable;
    return {
        {QStringLiteral("switches"), counts.switches},
        {QStringLiteral("tabletPads"), counts.tabletPads},
        {QStringLiteral("tabletTools"), counts.tabletTools},
    };
}
QString CompositorClient::inputDeviceDiscoveryErrorName() const
{
    return inputDeviceDiscoveryErrorName_;
}
QString CompositorClient::inputDeviceDiscoveryErrorMessage() const
{
    return inputDeviceDiscoveryErrorMessage_;
}
bool CompositorClient::inputDeviceProjectionAvailable() const
{
    return savedInputDevices_.has_value();
}
QVariantList CompositorClient::savedInputDevices() const
{
    return inputDeviceProjection_.savedDevices;
}
QVariantList CompositorClient::otherSavedInputDevices() const
{
    return inputDeviceProjection_.otherSavedDevices;
}
QString CompositorClient::inputDeviceProjectionRevisionToken() const
{
    return savedInputDevices_
        ? QString::number(inputDeviceProjectionRevision_) : QString{};
}
QString CompositorClient::inputDeviceProjectionInventoryDigest() const
{
    return savedInputDevices_ && connectedInputDeviceInventory_
        ? connectedInputDeviceInventory_->inventoryDigest : QString{};
}
QString CompositorClient::inputDeviceProjectionErrorName() const
{
    return inputDeviceProjectionErrorName_;
}
QString CompositorClient::inputDeviceProjectionErrorMessage() const
{
    return inputDeviceProjectionErrorMessage_;
}
bool CompositorClient::inputDevicesAvailable() const
{
    return optionGroupAvailable(OptionGroup::InputDevices);
}
bool CompositorClient::inputDevicesProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::InputDevices);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->projectionValid;
}
QVariantList CompositorClient::inputDevices() const
{
    const auto *state = optionGroupState(OptionGroup::InputDevices);
    return inputDevicesProjectionAvailable() && state != nullptr
        ? state->inputDevices : QVariantList{};
}
QString CompositorClient::inputDevicesErrorName() const
{
    return scopedErrorName(OptionGroup::InputDevices);
}
QString CompositorClient::inputDevicesErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::InputDevices);
}
QVariantList CompositorClient::windowsOptions() const
{
    return catalogAvailable_ && optionCatalog_
        ? optionCatalog_->windowsOptions()
        : QVariantList{};
}
QVariantMap CompositorClient::windowsValues() const
{
    const auto *state = optionGroupState(OptionGroup::Windows);
    return catalogAvailable_ && state != nullptr && state->projectionValid
        ? state->values : QVariantMap{};
}
QString CompositorClient::windowsErrorName() const
{
    return scopedErrorName(OptionGroup::Windows);
}
QString CompositorClient::windowsErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Windows);
}
QVariantList CompositorClient::workspacesOptions() const
{
    return catalogAvailable_ && optionCatalog_
        ? optionCatalog_->workspacesOptions()
        : QVariantList{};
}
QVariantMap CompositorClient::workspacesValues() const
{
    const auto *state = optionGroupState(OptionGroup::Workspaces);
    return catalogAvailable_ && state != nullptr && state->projectionValid
        ? state->values : QVariantMap{};
}
bool CompositorClient::workspaceRulesProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Workspaces);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->workspaceRulesProjectionValid;
}
QVariantList CompositorClient::workspaceRules() const
{
    const auto *state = optionGroupState(OptionGroup::Workspaces);
    return workspaceRulesProjectionAvailable() && state != nullptr
        ? state->workspaceRules : QVariantList{};
}
QString CompositorClient::workspacesErrorName() const
{
    return scopedErrorName(OptionGroup::Workspaces);
}
QString CompositorClient::workspacesErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Workspaces);
}
QVariantList CompositorClient::advancedOptions() const
{
    return catalogAvailable_ && optionCatalog_
        ? optionCatalog_->advancedOptions()
        : QVariantList{};
}
QVariantMap CompositorClient::advancedValues() const
{
    const auto *state = optionGroupState(OptionGroup::Advanced);
    return catalogAvailable_ && state != nullptr && state->projectionValid
        ? state->values : QVariantMap{};
}
QString CompositorClient::advancedErrorName() const
{
    return scopedErrorName(OptionGroup::Advanced);
}
QString CompositorClient::advancedErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Advanced);
}
bool CompositorClient::rulesAvailable() const
{
    return optionGroupAvailable(OptionGroup::Rules);
}
bool CompositorClient::rulesProjectionAvailable() const
{
    const auto *state = optionGroupState(OptionGroup::Rules);
    return available_ && catalogAvailable_ && actionCatalogAvailable_
        && completeSnapshotValid_ && state != nullptr
        && state->projectionValid;
}
QVariantList CompositorClient::windowRules() const
{
    const auto *state = optionGroupState(OptionGroup::Rules);
    return rulesProjectionAvailable() && state != nullptr
        ? state->windowRules : QVariantList{};
}
QVariantList CompositorClient::layerRules() const
{
    const auto *state = optionGroupState(OptionGroup::Rules);
    return rulesProjectionAvailable() && state != nullptr
        ? state->layerRules : QVariantList{};
}
QString CompositorClient::rulesErrorName() const
{
    return scopedErrorName(OptionGroup::Rules);
}
QString CompositorClient::rulesErrorMessage() const
{
    return scopedErrorMessage(OptionGroup::Rules);
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
QString CompositorClient::sharedBorderSyncState() const
{
    return sharedBorderSyncState_;
}
qulonglong CompositorClient::sharedBorderSourceRevision() const
{
    return sharedBorderSourceRevision_;
}
QString CompositorClient::sharedBorderSourceRevisionToken() const
{
    return QString::number(sharedBorderSourceRevision_);
}
QString CompositorClient::sharedBorderSyncError() const
{
    return sharedBorderSyncError_;
}

QString CompositorClient::sharedSpacingSyncState() const
{
    return sharedSpacingSyncState_;
}

qulonglong CompositorClient::sharedSpacingSourceRevision() const
{
    return sharedSpacingSourceRevision_;
}

QString CompositorClient::sharedSpacingSourceRevisionToken() const
{
    return QString::number(sharedSpacingSourceRevision_);
}

QString CompositorClient::sharedSpacingSyncError() const
{
    return sharedSpacingSyncError_;
}
QString CompositorClient::lastErrorOperation() const
{
    return lastErrorOperation_;
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
    setActionCatalogAvailable(false);
    setDisplayDiscoveryAvailable(false);
    clearInputDeviceSavedProjection({}, {});
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
            if (reply.isError() || !applyProperties(reply.value(), true)) {
                const auto name = reply.isError()
                    ? reply.error().name()
                    : clientErrorPrefix
                        + QStringLiteral("InvalidPropertiesReply");
                const auto message = reply.isError()
                    ? reply.error().message()
                    : QStringLiteral(
                        "The compositor service returned invalid initial properties"
                    );
                ++inputDeviceRefreshGeneration_;
                clearInputDeviceAuthorities(name, message);
                finishHydration(false);
                return;
            }
            refreshConnectedInputDevices();
            if (!advertisedAvailable_) {
                clearInputDeviceSavedProjection(
                    QStringLiteral(
                        "org.hyprshelld.Compositor1.Error.Unavailable"
                    ),
                    QStringLiteral(
                        "Saved compositor input settings are unavailable"
                    )
                );
                finishHydration(false);
                return;
            }
            fetchSnapshot(generation);
        }
    );
}

void CompositorClient::refreshConnectedInputDevices()
{
    const auto generation = ++inputDeviceRefreshGeneration_;
    const auto changed = !inputDeviceDiscoveryBusy_
        || !inputDeviceDiscoveryErrorName_.isEmpty()
        || !inputDeviceDiscoveryErrorMessage_.isEmpty();
    inputDeviceDiscoveryBusy_ = true;
    inputDeviceDiscoveryErrorName_.clear();
    inputDeviceDiscoveryErrorMessage_.clear();
    if (changed) emit inputDeviceDiscoveryChanged();
    fetchConnectedInputDevices(generation);
}

void CompositorClient::adoptManagedConfiguration()
{
    if (!available_ || !writable_ || busy_
        || managementState_ != QStringLiteral("unmanaged")) {
        setError(
            QStringLiteral("adopt"),
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
            QStringLiteral("compositor-apply"),
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

void CompositorClient::saveAppearance(
    const QVariantMap &values,
    const QVariantList &curves,
    const QVariantList &animations
)
{
    const auto group = OptionGroup::Appearance;
    if (!optionGroupAvailable(group) || optionCatalog_ == nullptr
        || actionCatalog_ == nullptr) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral(
                "The appearance configuration cannot be saved right now"
            )
        );
        return;
    }

    QString error;
    const auto edit = CompositorSnapshotEditor::replaceAppearance(
        snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
        *optionCatalog_, *actionCatalog_, values, curves, animations, error
    );
    if (!edit) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("InvalidAppearance"),
            error
        );
        return;
    }
    if (!edit->changed) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("NoChanges"),
            QStringLiteral("The appearance configuration has not changed")
        );
        return;
    }

    clearError();
    clearOperationError(group);
    setBusy(true);
    setBusyOperation(optionGroupSaveOperation(group));
    sendSnapshotReplace(
        {
            .group = group,
            .candidate = edit->candidate,
            .expectedRevision = revision_,
            .catalogDigest = catalogDigest_,
            .actionCatalogDigest = actionCatalogDigest_,
        },
        false
    );
}

void CompositorClient::saveOptions(const QVariantMap &values)
{
    saveOptionGroup(OptionGroup::AllOptions, values);
}

void CompositorClient::saveBindings(
    const QVariantList &bindings,
    const QVariantList &submaps
)
{
    saveCollectionGroup(OptionGroup::Bindings, bindings, submaps);
}

void CompositorClient::saveEnvironment(const QVariantList &environment)
{
    saveCollectionGroup(OptionGroup::Environment, environment);
}

void CompositorClient::savePermissions(const QVariantList &permissions)
{
    saveCollectionGroup(OptionGroup::Permissions, permissions);
}

void CompositorClient::saveInputDevices(const QVariantList &devices)
{
    saveCollectionGroup(OptionGroup::InputDevices, devices);
}

void CompositorClient::saveInput(
    const QVariantMap &values,
    const QVariantList &gestures
)
{
    const auto group = OptionGroup::Input;
    if (!optionGroupAvailable(group) || optionCatalog_ == nullptr
        || actionCatalog_ == nullptr) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral(
                "The input configuration cannot be saved right now"
            )
        );
        return;
    }

    QString error;
    const auto edit = CompositorSnapshotEditor::replaceInput(
        snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
        *optionCatalog_, *actionCatalog_, values, gestures, error
    );
    if (!edit) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("InvalidInput"),
            error
        );
        return;
    }
    if (!edit->changed) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("NoChanges"),
            QStringLiteral("The input configuration has not changed")
        );
        return;
    }

    clearError();
    clearOperationError(group);
    setBusy(true);
    setBusyOperation(optionGroupSaveOperation(group));
    sendSnapshotReplace(
        {
            .group = group,
            .candidate = edit->candidate,
            .expectedRevision = revision_,
            .catalogDigest = catalogDigest_,
            .actionCatalogDigest = actionCatalogDigest_,
        },
        false
    );
}

void CompositorClient::saveWindows(const QVariantMap &values)
{
    saveOptionGroup(OptionGroup::Windows, values);
}

void CompositorClient::saveAdvanced(const QVariantMap &values)
{
    saveOptionGroup(OptionGroup::Advanced, values);
}

void CompositorClient::saveWorkspaces(
    const QVariantMap &values,
    const QVariantList &workspaceRules
)
{
    const auto group = OptionGroup::Workspaces;
    if (!optionGroupAvailable(group) || optionCatalog_ == nullptr
        || actionCatalog_ == nullptr) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral(
                "The workspaces configuration cannot be saved right now"
            )
        );
        return;
    }

    QString error;
    const auto edit = CompositorSnapshotEditor::replaceWorkspaces(
        snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
        *optionCatalog_, *actionCatalog_, values, workspaceRules, error
    );
    if (!edit) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("InvalidWorkspaces"),
            error
        );
        return;
    }
    if (!edit->changed) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("NoChanges"),
            QStringLiteral("The workspaces configuration has not changed")
        );
        return;
    }

    clearError();
    clearOperationError(group);
    setBusy(true);
    setBusyOperation(optionGroupSaveOperation(group));
    sendSnapshotReplace(
        {
            .group = group,
            .candidate = edit->candidate,
            .expectedRevision = revision_,
            .catalogDigest = catalogDigest_,
            .actionCatalogDigest = actionCatalogDigest_,
        },
        false
    );
}

void CompositorClient::saveRules(
    const QVariantList &windowRules,
    const QVariantList &layerRules
)
{
    const auto group = OptionGroup::Rules;
    if (!optionGroupAvailable(group) || optionCatalog_ == nullptr
        || actionCatalog_ == nullptr) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("The rules configuration cannot be saved right now")
        );
        return;
    }

    QString error;
    const auto edit = CompositorSnapshotEditor::replaceRules(
        snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
        *optionCatalog_, *actionCatalog_, windowRules, layerRules, error
    );
    if (!edit) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("InvalidRules"),
            error
        );
        return;
    }
    if (!edit->changed) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("NoChanges"),
            QStringLiteral("The rules configuration has not changed")
        );
        return;
    }

    clearError();
    clearOperationError(group);
    setBusy(true);
    setBusyOperation(optionGroupSaveOperation(group));
    sendSnapshotReplace(
        {
            .group = group,
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
            QStringLiteral("compositor-apply"),
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("There is no saved compositor configuration to apply")
        );
        return;
    }
    clearError();
    setBusy(true);
    sendApplyRequest(
        {
            .group = OptionGroup::None,
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
            QStringLiteral("recover"),
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
            QStringLiteral("display-preview"),
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
                QStringLiteral("display-preview"),
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
            QStringLiteral("display-preview"),
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
            QStringLiteral("display-confirm"),
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
            QStringLiteral("display-revert"),
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

void CompositorClient::retrySharedBorderSync()
{
    if (!available_ || busy_) {
        setError(
            QStringLiteral("shared-border-sync"),
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable"),
            QStringLiteral("Shared border synchronization cannot be retried right now")
        );
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("RetrySharedBorderSync")
    );
    beginMutation(Mutation::SharedBorderSync, message, ordinaryCallTimeoutMs);
}

void CompositorClient::retrySharedSpacingSync()
{
    if (!available_ || busy_) {
        setError(
            QStringLiteral("shared-spacing-sync"),
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable"),
            QStringLiteral("Shared spacing synchronization cannot be retried right now")
        );
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("RetrySharedSpacingSync")
    );
    beginMutation(Mutation::SharedSpacingSync, message, ordinaryCallTimeoutMs);
}

void CompositorClient::clearError()
{
    if (lastErrorOperation_.isEmpty() && lastErrorName_.isEmpty()
        && lastErrorMessage_.isEmpty()) {
        return;
    }
    lastErrorOperation_.clear();
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
        QStringLiteral("SharedBorderSyncState"),
        QStringLiteral("SharedBorderSourceRevision"),
        QStringLiteral("SharedBorderSyncError"),
        QStringLiteral("SharedSpacingSyncState"),
        QStringLiteral("SharedSpacingSourceRevision"),
        QStringLiteral("SharedSpacingSyncError"),
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
    ++inputDeviceRefreshGeneration_;
    advertisedAvailable_ = false;
    advertisedCatalogDigest_.clear();
    advertisedActionCatalogDigest_.clear();
    optionCatalog_.reset();
    actionCatalog_.reset();
    catalogOwnerGeneration_ = 0;
    actionCatalogOwnerGeneration_ = 0;
    completeSnapshotValid_ = false;
    for (const auto group : {
             OptionGroup::AllOptions,
             OptionGroup::Appearance,
             OptionGroup::Input,
             OptionGroup::Windows,
             OptionGroup::Workspaces,
             OptionGroup::Advanced,
             OptionGroup::Rules,
             OptionGroup::Bindings,
             OptionGroup::Environment,
             OptionGroup::Permissions,
             OptionGroup::InputDevices,
         }) {
        auto *state = optionGroupState(group);
        state->projectionValid = false;
        state->appearanceAnimationProjectionValid = false;
        state->inputGesturesProjectionValid = false;
        state->workspaceRulesProjectionValid = false;
        state->values.clear();
        state->appearanceCurves.clear();
        state->appearanceAnimations.clear();
        state->inputGestures.clear();
        state->inputGestureCompatibility.clear();
        state->workspaceRules.clear();
        state->windowRules.clear();
        state->layerRules.clear();
        state->bindings.clear();
        state->submaps.clear();
        state->environment.clear();
        state->permissions.clear();
        state->inputDevices.clear();
        state->complexProjectionErrorName.clear();
        state->complexProjectionErrorMessage.clear();
        clearOperationError(group);
        setAuthorityError(group, {}, {});
        setProjectionError(group, {}, {});
    }
    setCatalogAvailable(false);
    setActionCatalogAvailable(false);
    setDisplayDiscoveryAvailable(false);
    clearInputDeviceAuthorities({}, {});
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
    const auto sharedBorderChanged = sharedBorderSyncState_
            != QStringLiteral("unavailable")
        || sharedBorderSourceRevision_ != 0
        || sharedBorderSyncError_
            != QStringLiteral("Shared visual settings are unavailable");
    sharedBorderSyncState_ = QStringLiteral("unavailable");
    sharedBorderSourceRevision_ = 0;
    sharedBorderSyncError_ = QStringLiteral(
        "Shared visual settings are unavailable"
    );
    if (sharedBorderChanged) emit sharedBorderSyncChanged();
    const auto sharedSpacingChanged = sharedSpacingSyncState_
            != QStringLiteral("unavailable")
        || sharedSpacingSourceRevision_ != 0
        || sharedSpacingSyncError_
            != QStringLiteral("Shared visual settings are unavailable");
    sharedSpacingSyncState_ = QStringLiteral("unavailable");
    sharedSpacingSourceRevision_ = 0;
    sharedSpacingSyncError_ = QStringLiteral(
        "Shared visual settings are unavailable"
    );
    if (sharedSpacingChanged) emit sharedSpacingSyncChanged();
    if (managementState_ == QStringLiteral("preview")) {
        managementState_ = QStringLiteral("unmanaged");
        emit managementStateChanged();
        emitAllOptionGroupsChanged();
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
                clearInputDeviceSavedProjection(
                    reply.type() == QDBusMessage::ErrorMessage
                        ? reply.errorName()
                        : clientErrorPrefix
                            + QStringLiteral("InvalidSnapshotReply"),
                    reply.type() == QDBusMessage::ErrorMessage
                        ? reply.errorMessage()
                        : QStringLiteral(
                            "The compositor service returned an invalid saved-settings snapshot reply"
                        )
                );
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
                clearInputDeviceSavedProjection(
                    clientErrorPrefix
                        + QStringLiteral("InvalidSnapshotAuthority"),
                    QStringLiteral(
                        "The saved compositor input settings did not match the advertised exact authority"
                    )
                );
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
            completeSnapshotValid_ = false;
            if (changed) emit snapshotChanged();
            const auto devices = Hyprland::parseDesiredInputDevices(
                snapshotObject_
            );
            if (devices) {
                savedInputDevices_ = *devices.value;
                inputDeviceProjectionRevision_ = snapshotRevision;
                inputDeviceProjectionErrorName_.clear();
                inputDeviceProjectionErrorMessage_.clear();
                const auto projectionChanges = updateInputDeviceProjection();
                if (projectionChanges.discovery) {
                    emit inputDeviceDiscoveryChanged();
                }
                emit inputDeviceProjectionChanged();
            } else {
                const auto detail = devices.errors.isEmpty()
                    ? QStringLiteral(
                        "The saved device settings could not be verified"
                    )
                    : QStringLiteral("%1: %2")
                        .arg(
                            devices.errors.constFirst().path,
                            devices.errors.constFirst().message
                        );
                clearInputDeviceSavedProjection(
                    clientErrorPrefix
                        + QStringLiteral("InvalidInputDeviceProjection"),
                    detail
                );
            }
            updateAllOptionGroupProjections();
            setAvailable(true);
            fetchOptionCatalog(generation);
            fetchActionCatalog(generation);
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
        updateAllOptionGroupProjections();
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
                const auto name = reply.type() == QDBusMessage::ErrorMessage
                    ? reply.errorName()
                    : clientErrorPrefix
                        + QStringLiteral("InvalidCatalogReply");
                const auto message = reply.type()
                        == QDBusMessage::ErrorMessage
                    ? reply.errorMessage()
                    : QStringLiteral("The compositor service returned an invalid option catalog reply");
                for (const auto group : {
                         OptionGroup::AllOptions,
                         OptionGroup::Appearance,
                         OptionGroup::Input,
                         OptionGroup::Windows,
                         OptionGroup::Workspaces,
                         OptionGroup::Advanced,
                         OptionGroup::Rules,
                         OptionGroup::Bindings,
                         OptionGroup::Environment,
                         OptionGroup::Permissions,
                         OptionGroup::InputDevices,
                     }) {
                    setProjectionError(group, name, message);
                }
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
                const auto name = clientErrorPrefix
                    + QStringLiteral("InvalidCatalog");
                for (const auto group : {
                         OptionGroup::AllOptions,
                         OptionGroup::Appearance,
                         OptionGroup::Input,
                         OptionGroup::Windows,
                         OptionGroup::Workspaces,
                         OptionGroup::Advanced,
                         OptionGroup::Rules,
                         OptionGroup::Bindings,
                         OptionGroup::Environment,
                         OptionGroup::Permissions,
                         OptionGroup::InputDevices,
                     }) {
                    setProjectionError(group, name, error);
                }
                return;
            }
            optionCatalog_ = std::make_unique<CompositorOptionCatalog>(
                std::move(*parsed)
            );
            catalogOwnerGeneration_ = ownerGeneration_;
            setCatalogAvailable(true);
            updateAllOptionGroupProjections();
        }
    );
}

void CompositorClient::fetchActionCatalog(const quint64 generation)
{
    if (actionCatalog_ != nullptr
        && actionCatalogOwnerGeneration_ == ownerGeneration_
        && actionCatalog_->digest() == actionCatalogDigest_) {
        clearAuthorityErrors();
        setActionCatalogAvailable(true);
        updateAllOptionGroupProjections();
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetActionCatalog")
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
                || arguments.at(1).metaType().id() != QMetaType::QString
                || arguments.at(2).metaType().id() != QMetaType::QByteArray
                || arguments.at(3).metaType().id() != QMetaType::QString) {
                actionCatalog_.reset();
                setActionCatalogAvailable(false);
                const auto name = reply.type() == QDBusMessage::ErrorMessage
                    ? reply.errorName()
                    : clientErrorPrefix
                        + QStringLiteral("InvalidActionCatalogReply");
                const auto detail = reply.type()
                        == QDBusMessage::ErrorMessage
                    ? reply.errorMessage()
                    : QStringLiteral("The compositor service returned an invalid action authority reply");
                for (const auto group : {
                         OptionGroup::AllOptions,
                         OptionGroup::Appearance,
                         OptionGroup::Input,
                         OptionGroup::Windows,
                         OptionGroup::Workspaces,
                         OptionGroup::Advanced,
                         OptionGroup::Rules,
                         OptionGroup::Bindings,
                         OptionGroup::Environment,
                         OptionGroup::Permissions,
                         OptionGroup::InputDevices,
                     }) {
                    setAuthorityError(group, name, detail);
                }
                updateOptionGroupProjection(OptionGroup::Rules);
                return;
            }

            QString error;
            auto parsed = CompositorActionCatalog::fromBytes(
                arguments.at(0).toByteArray(),
                arguments.at(1).toString(),
                actionCatalogDigest_,
                arguments.at(2).toByteArray(),
                arguments.at(3).toString(),
                error
            );
            if (!parsed) {
                actionCatalog_.reset();
                setActionCatalogAvailable(false);
                const auto name = clientErrorPrefix
                    + QStringLiteral("InvalidActionCatalog");
                for (const auto group : {
                         OptionGroup::AllOptions,
                         OptionGroup::Appearance,
                         OptionGroup::Input,
                         OptionGroup::Windows,
                         OptionGroup::Workspaces,
                         OptionGroup::Advanced,
                         OptionGroup::Rules,
                         OptionGroup::Bindings,
                         OptionGroup::Environment,
                         OptionGroup::Permissions,
                         OptionGroup::InputDevices,
                     }) {
                    setAuthorityError(group, name, error);
                }
                updateOptionGroupProjection(OptionGroup::Rules);
                return;
            }

            actionCatalog_ = std::make_unique<CompositorActionCatalog>(
                std::move(*parsed)
            );
            actionCatalogOwnerGeneration_ = ownerGeneration_;
            clearAuthorityErrors();
            setActionCatalogAvailable(true);
            updateAllOptionGroupProjections();
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

void CompositorClient::fetchConnectedInputDevices(const quint64 generation)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetConnectedInputDevices")
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
            if (generation != inputDeviceRefreshGeneration_) return;

            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || arguments.size() != 2
                || arguments.at(0).metaType().id() != QMetaType::QByteArray
                || arguments.at(1).metaType().id() != QMetaType::ULongLong) {
                clearInputDeviceDiscovery(
                    reply.type() == QDBusMessage::ErrorMessage
                        ? reply.errorName()
                        : clientErrorPrefix
                            + QStringLiteral("InvalidInputDeviceReply"),
                    reply.type() == QDBusMessage::ErrorMessage
                        ? reply.errorMessage()
                        : QStringLiteral(
                            "The compositor service returned an invalid input-device inventory reply"
                        )
                );
                return;
            }

            const auto parsed =
                Hyprland::parseConnectedInputDeviceInventoryDocument(
                    QByteArrayView(arguments.at(0).toByteArray())
                );
            const auto observedAtMs = arguments.at(1).toULongLong();
            if (!parsed || observedAtMs == 0) {
                QString detail;
                if (observedAtMs == 0) {
                    detail = QStringLiteral(
                        "A successful input-device inventory requires a positive observation time"
                    );
                } else if (!parsed.errors.isEmpty()) {
                    detail = QStringLiteral("%1: %2")
                        .arg(
                            parsed.errors.constFirst().path,
                            parsed.errors.constFirst().message
                        );
                } else {
                    detail = QStringLiteral(
                        "The input-device inventory could not be verified"
                    );
                }
                clearInputDeviceDiscovery(
                    clientErrorPrefix
                        + QStringLiteral("InvalidInputDeviceInventory"),
                    detail
                );
                return;
            }

            connectedInputDeviceInventory_ = *parsed.value;
            inputDevicesObservedAtMs_ = observedAtMs;
            inputDeviceDiscoveryAvailable_ = true;
            inputDeviceDiscoveryBusy_ = false;
            inputDeviceDiscoveryErrorName_.clear();
            inputDeviceDiscoveryErrorMessage_.clear();
            Q_UNUSED(updateInputDeviceProjection())
            emit inputDeviceDiscoveryChanged();
            if (savedInputDevices_) emit inputDeviceProjectionChanged();
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
                    setError(
                        QStringLiteral("display-refresh"),
                        reply.errorName(),
                        reply.errorMessage()
                    );
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
        QStringLiteral("SharedBorderSyncState"),
        QStringLiteral("SharedBorderSourceRevision"),
        QStringLiteral("SharedBorderSyncError"),
        QStringLiteral("SharedSpacingSyncState"),
        QStringLiteral("SharedSpacingSourceRevision"),
        QStringLiteral("SharedSpacingSyncError"),
    };
    if (requireAll) {
        for (const auto &name : required) {
            if (!properties.contains(name)) return false;
        }
    }
    const auto completeGroupOrAbsent = [&properties](
        const QStringList &names
    ) {
        const auto supplied = std::ranges::count_if(
            names,
            [&properties](const QString &name) {
                return properties.contains(name);
            }
        );
        return supplied == 0 || supplied == names.size();
    };
    if (!completeGroupOrAbsent({
            QStringLiteral("SharedBorderSyncState"),
            QStringLiteral("SharedBorderSourceRevision"),
            QStringLiteral("SharedBorderSyncError"),
        })
        || !completeGroupOrAbsent({
            QStringLiteral("SharedSpacingSyncState"),
            QStringLiteral("SharedSpacingSourceRevision"),
            QStringLiteral("SharedSpacingSyncError"),
        })) {
        return false;
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
    auto nextSharedBorderState = sharedBorderSyncState_;
    auto nextSharedBorderRevision = sharedBorderSourceRevision_;
    auto nextSharedBorderError = sharedBorderSyncError_;
    auto nextSharedSpacingState = sharedSpacingSyncState_;
    auto nextSharedSpacingRevision = sharedSpacingSourceRevision_;
    auto nextSharedSpacingError = sharedSpacingSyncError_;
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
        )
        || !string(
            QStringLiteral("SharedBorderSyncState"),
            nextSharedBorderState
        )
        || !unsignedInteger(
            QStringLiteral("SharedBorderSourceRevision"),
            nextSharedBorderRevision
        )
        || !string(
            QStringLiteral("SharedBorderSyncError"),
            nextSharedBorderError
        )
        || !string(
            QStringLiteral("SharedSpacingSyncState"),
            nextSharedSpacingState
        )
        || !unsignedInteger(
            QStringLiteral("SharedSpacingSourceRevision"),
            nextSharedSpacingRevision
        )
        || !string(
            QStringLiteral("SharedSpacingSyncError"),
            nextSharedSpacingError
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
        || !QStringList{
            QStringLiteral("unavailable"), QStringLiteral("override"),
            QStringLiteral("pending"), QStringLiteral("saved"),
            QStringLiteral("current"), QStringLiteral("failed"),
        }.contains(nextSharedBorderState)
        || ((nextSharedBorderState == QStringLiteral("unavailable")
                || nextSharedBorderState == QStringLiteral("failed"))
            != !nextSharedBorderError.isEmpty())
        || !QStringList{
            QStringLiteral("unavailable"), QStringLiteral("override"),
            QStringLiteral("pending"), QStringLiteral("saved"),
            QStringLiteral("current"), QStringLiteral("failed"),
        }.contains(nextSharedSpacingState)
        || ((nextSharedSpacingState == QStringLiteral("unavailable")
                || nextSharedSpacingState == QStringLiteral("failed"))
            != !nextSharedSpacingError.isEmpty())
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
        emitAllOptionGroupsChanged();
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
        emitAllOptionGroupsChanged();
    }
    if (advertisedCatalogDigest_ != catalogDigest_
        || advertisedActionCatalogDigest_ != actionCatalogDigest_) {
        const auto optionAuthorityChanged =
            advertisedCatalogDigest_ != catalogDigest_;
        const auto actionAuthorityChanged =
            advertisedActionCatalogDigest_ != actionCatalogDigest_;
        catalogDigest_ = advertisedCatalogDigest_;
        actionCatalogDigest_ = advertisedActionCatalogDigest_;
        if (optionAuthorityChanged || actionAuthorityChanged) {
            for (const auto group : {
                     OptionGroup::AllOptions,
                     OptionGroup::Appearance,
                     OptionGroup::Input,
                     OptionGroup::Windows,
                     OptionGroup::Workspaces,
                     OptionGroup::Advanced,
                     OptionGroup::Rules,
                     OptionGroup::Bindings,
                     OptionGroup::Environment,
                     OptionGroup::Permissions,
                     OptionGroup::InputDevices,
                 }) {
                clearOperationError(group);
            }
            completeSnapshotValid_ = false;
        }
        if (optionAuthorityChanged) {
            optionCatalog_.reset();
            catalogOwnerGeneration_ = 0;
            setCatalogAvailable(false);
        }
        if (actionAuthorityChanged) {
            actionCatalog_.reset();
            actionCatalogOwnerGeneration_ = 0;
            setActionCatalogAvailable(false);
        }
        emit catalogDigestChanged();
        emitAllOptionGroupsChanged();
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
        emitAllOptionGroupsChanged();
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
        emitAllOptionGroupsChanged();
    }
    if (nextSharedBorderState != sharedBorderSyncState_
        || nextSharedBorderRevision != sharedBorderSourceRevision_
        || nextSharedBorderError != sharedBorderSyncError_) {
        sharedBorderSyncState_ = nextSharedBorderState;
        sharedBorderSourceRevision_ = nextSharedBorderRevision;
        sharedBorderSyncError_ = nextSharedBorderError;
        emit sharedBorderSyncChanged();
    }
    if (nextSharedSpacingState != sharedSpacingSyncState_
        || nextSharedSpacingRevision != sharedSpacingSourceRevision_
        || nextSharedSpacingError != sharedSpacingSyncError_) {
        sharedSpacingSyncState_ = nextSharedSpacingState;
        sharedSpacingSourceRevision_ = nextSharedSpacingRevision;
        sharedSpacingSyncError_ = nextSharedSpacingError;
        emit sharedSpacingSyncChanged();
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
    const auto operation =
        mutation == Mutation::Adopt ? QStringLiteral("adopt")
        : mutation == Mutation::Apply
            ? QStringLiteral("compositor-apply")
        : mutation == Mutation::Preview ? QStringLiteral("display-preview")
        : mutation == Mutation::Confirm ? QStringLiteral("display-confirm")
        : mutation == Mutation::Revert ? QStringLiteral("display-revert")
        : mutation == Mutation::Recover ? QStringLiteral("recover")
        : mutation == Mutation::SharedBorderSync
            ? QStringLiteral("shared-border-sync")
            : QStringLiteral("shared-spacing-sync");
    setBusyOperation(operation);
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
        [this, watcher, ownerGeneration, mutation, operation, requestedRevision,
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
                setError(operation, reply.errorName(), reply.errorMessage());
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
                    : mutation == Mutation::SharedBorderSync
                            || mutation == Mutation::SharedSpacingSync
                        ? arguments.isEmpty()
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
                    operation,
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

void CompositorClient::saveOptionGroup(
    const OptionGroup group,
    const QVariantMap &values
)
{
    const auto name = optionGroupName(group);
    if (group == OptionGroup::Appearance || group == OptionGroup::Input
        || group == OptionGroup::Workspaces || group == OptionGroup::Rules
        || !optionGroupAvailable(group)
        || optionCatalog_ == nullptr || actionCatalog_ == nullptr) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("The %1 configuration cannot be saved right now")
                .arg(name)
        );
        return;
    }

    QString error;
    std::optional<CompositorSnapshotEdit> edit;
    switch (group) {
    case OptionGroup::AllOptions:
        edit = CompositorSnapshotEditor::replaceAllOptions(
            snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
            *optionCatalog_, *actionCatalog_, values, error
        );
        break;
    case OptionGroup::Input:
        break;
    case OptionGroup::Windows:
        edit = CompositorSnapshotEditor::replaceWindows(
            snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
            *optionCatalog_, *actionCatalog_, values, error
        );
        break;
    case OptionGroup::Advanced:
        edit = CompositorSnapshotEditor::replaceAdvanced(
            snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
            *optionCatalog_, *actionCatalog_, values, error
        );
        break;
    case OptionGroup::None:
    case OptionGroup::Appearance:
    case OptionGroup::Workspaces:
    case OptionGroup::Rules:
    case OptionGroup::Bindings:
    case OptionGroup::Environment:
    case OptionGroup::Permissions:
    case OptionGroup::InputDevices:
        break;
    }
    if (!edit) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Invalid")
                + optionGroupErrorSuffix(group),
            error
        );
        return;
    }
    if (!edit->changed) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("NoChanges"),
            QStringLiteral("The %1 configuration has not changed")
                .arg(name)
        );
        return;
    }

    clearError();
    clearOperationError(group);
    setBusy(true);
    setBusyOperation(optionGroupSaveOperation(group));
    sendSnapshotReplace(
        {
            .group = group,
            .candidate = edit->candidate,
            .expectedRevision = revision_,
            .catalogDigest = catalogDigest_,
            .actionCatalogDigest = actionCatalogDigest_,
        },
        false
    );
}

void CompositorClient::saveCollectionGroup(
    const OptionGroup group,
    const QVariantList &first,
    const QVariantList &second
)
{
    const auto name = optionGroupName(group);
    if ((group != OptionGroup::Bindings
         && group != OptionGroup::Environment
         && group != OptionGroup::Permissions
         && group != OptionGroup::InputDevices)
        || !optionGroupAvailable(group)
        || optionCatalog_ == nullptr || actionCatalog_ == nullptr) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("The %1 configuration cannot be saved right now")
                .arg(name)
        );
        return;
    }

    QString error;
    std::optional<CompositorSnapshotEdit> edit;
    switch (group) {
    case OptionGroup::Bindings:
        edit = CompositorSnapshotEditor::replaceBindings(
            snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
            *optionCatalog_, *actionCatalog_, first, second, error
        );
        break;
    case OptionGroup::Environment:
        edit = CompositorSnapshotEditor::replaceEnvironment(
            snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
            *optionCatalog_, *actionCatalog_, first, error
        );
        break;
    case OptionGroup::Permissions:
        edit = CompositorSnapshotEditor::replacePermissions(
            snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
            *optionCatalog_, *actionCatalog_, first, error
        );
        break;
    case OptionGroup::InputDevices:
        edit = CompositorSnapshotEditor::replaceInputDevices(
            snapshotObject_, revision_, catalogDigest_, actionCatalogDigest_,
            *optionCatalog_, *actionCatalog_, first, error
        );
        break;
    case OptionGroup::None:
    case OptionGroup::AllOptions:
    case OptionGroup::Appearance:
    case OptionGroup::Input:
    case OptionGroup::Windows:
    case OptionGroup::Workspaces:
    case OptionGroup::Advanced:
    case OptionGroup::Rules:
        break;
    }
    if (!edit) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("Invalid")
                + optionGroupErrorSuffix(group),
            error
        );
        return;
    }
    if (!edit->changed) {
        setOperationError(
            group,
            clientErrorPrefix + QStringLiteral("NoChanges"),
            QStringLiteral("The %1 configuration has not changed").arg(name)
        );
        return;
    }

    clearError();
    clearOperationError(group);
    setBusy(true);
    setBusyOperation(optionGroupSaveOperation(group));
    sendSnapshotReplace(
        {
            .group = group,
            .candidate = edit->candidate,
            .expectedRevision = revision_,
            .catalogDigest = catalogDigest_,
            .actionCatalogDigest = actionCatalogDigest_,
        },
        false
    );
}

void CompositorClient::sendSnapshotReplace(
    const SnapshotSaveRequest &request,
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
                        sendSnapshotReplace(request, true);
                    } else {
                        // Re-read the exact authority tuple. If the first or
                        // retry call committed, verification proceeds to Apply;
                        // otherwise it fails without another increment attempt.
                        verifySnapshotReplacement(request);
                    }
                    return;
                }
                finishMutation();
                setOperationError(
                    request.group, reply.errorName(), reply.errorMessage()
                );
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
                setOperationError(
                    request.group,
                    clientErrorPrefix + QStringLiteral("InvalidReply"),
                    QStringLiteral("The compositor service returned an invalid replacement reply")
                );
                refresh();
                return;
            }
            verifySnapshotReplacement(request);
        }
    );
}

void CompositorClient::verifySnapshotReplacement(
    const SnapshotSaveRequest &request
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
                setOperationError(
                    request.group,
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
                        setOperationError(
                            request.group,
                            clientErrorPrefix + QStringLiteral("ReplacementUnconfirmed"),
                            QStringLiteral("The saved compositor snapshot did not match the requested %1 configuration")
                                .arg(optionGroupName(request.group))
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
                        setOperationError(
                            request.group,
                            clientErrorPrefix + QStringLiteral("InvalidSnapshot"),
                            QStringLiteral("The saved compositor snapshot is invalid")
                        );
                        refresh();
                        return;
                    }
                    const auto changed = snapshot_ != parsed->values
                        || revision_ != nextRevision;
                    completeSnapshotValid_ = false;
                    snapshot_ = parsed->values;
                    snapshotObject_ = parsed->object;
                    revision_ = nextRevision;
                    catalogDigest_ = request.catalogDigest;
                    actionCatalogDigest_ = request.actionCatalogDigest;
                    if (changed) emit snapshotChanged();
                    updateAllOptionGroupProjections();

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
                            optionGroupApplyOperation(request.group),
                            clientErrorPrefix + QStringLiteral("ActivationRequired"),
                            QStringLiteral("The %1 configuration was saved but cannot be reloaded automatically")
                                .arg(optionGroupName(request.group))
                        );
                        refresh();
                        return;
                    }
                    sendApplyRequest(
                        {
                            .group = request.group,
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
    const auto operation = optionGroupApplyOperation(request.group);
    if (!retry) {
        setBusyOperation(operation);
    }
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
        [this, watcher, ownerGeneration, operation, request, retry] {
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
                setError(operation, reply.errorName(), reply.errorMessage());
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
                    operation,
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
    const auto operation = optionGroupApplyOperation(request.group);
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
        [this, watcher, ownerGeneration, operation, request] {
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
                    operation,
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
    emitAllOptionGroupsChanged();
}

void CompositorClient::setCatalogAvailable(const bool available)
{
    if (!available) completeSnapshotValid_ = false;
    if (available == catalogAvailable_) return;
    catalogAvailable_ = available;
    emitAllOptionGroupsChanged();
}

void CompositorClient::setActionCatalogAvailable(const bool available)
{
    if (!available) completeSnapshotValid_ = false;
    if (available == actionCatalogAvailable_) return;
    actionCatalogAvailable_ = available;
    emitAllOptionGroupsChanged();
}

void CompositorClient::setDisplayDiscoveryAvailable(const bool available)
{
    if (available == displayDiscoveryAvailable_) return;
    displayDiscoveryAvailable_ = available;
    emit displayDiscoveryAvailableChanged();
}

CompositorClient::InputDeviceProjectionChanges
CompositorClient::updateInputDeviceProjection()
{
    auto next = projectInputDevices(
        savedInputDevices_, connectedInputDeviceInventory_
    );
    const InputDeviceProjectionChanges changes{
        .discovery = next.connectedDevices
            != inputDeviceProjection_.connectedDevices,
        .projection = next.savedDevices
                != inputDeviceProjection_.savedDevices
            || next.otherSavedDevices
                != inputDeviceProjection_.otherSavedDevices,
    };
    inputDeviceProjection_ = std::move(next);
    return changes;
}

void CompositorClient::clearInputDeviceSavedProjection(
    const QString &errorName,
    const QString &errorMessage
)
{
    const auto changed = savedInputDevices_.has_value()
        || inputDeviceProjectionRevision_ != 0
        || inputDeviceProjectionErrorName_ != errorName
        || inputDeviceProjectionErrorMessage_ != errorMessage;
    savedInputDevices_.reset();
    inputDeviceProjectionRevision_ = 0;
    inputDeviceProjectionErrorName_ = errorName;
    inputDeviceProjectionErrorMessage_ = errorMessage;
    const auto projectionChanges = updateInputDeviceProjection();
    if (projectionChanges.discovery) emit inputDeviceDiscoveryChanged();
    if (changed || projectionChanges.projection) {
        emit inputDeviceProjectionChanged();
    }
}

void CompositorClient::clearInputDeviceDiscovery(
    const QString &errorName,
    const QString &errorMessage
)
{
    const auto projectionHadReceipt = savedInputDevices_.has_value()
        && connectedInputDeviceInventory_.has_value();
    const auto changed = connectedInputDeviceInventory_.has_value()
        || inputDevicesObservedAtMs_ != 0
        || inputDeviceDiscoveryAvailable_
        || inputDeviceDiscoveryBusy_
        || inputDeviceDiscoveryErrorName_ != errorName
        || inputDeviceDiscoveryErrorMessage_ != errorMessage;
    connectedInputDeviceInventory_.reset();
    inputDevicesObservedAtMs_ = 0;
    inputDeviceDiscoveryAvailable_ = false;
    inputDeviceDiscoveryBusy_ = false;
    inputDeviceDiscoveryErrorName_ = errorName;
    inputDeviceDiscoveryErrorMessage_ = errorMessage;
    const auto projectionChanges = updateInputDeviceProjection();
    if (changed || projectionChanges.discovery) {
        emit inputDeviceDiscoveryChanged();
    }
    if (projectionHadReceipt || projectionChanges.projection) {
        emit inputDeviceProjectionChanged();
    }
}

void CompositorClient::clearInputDeviceAuthorities(
    const QString &errorName,
    const QString &errorMessage
)
{
    const auto discoveryChanged = connectedInputDeviceInventory_.has_value()
        || inputDevicesObservedAtMs_ != 0
        || inputDeviceDiscoveryAvailable_
        || inputDeviceDiscoveryBusy_
        || inputDeviceDiscoveryErrorName_ != errorName
        || inputDeviceDiscoveryErrorMessage_ != errorMessage;
    const auto projectionChanged = savedInputDevices_.has_value()
        || inputDeviceProjectionRevision_ != 0
        || inputDeviceProjectionErrorName_ != errorName
        || inputDeviceProjectionErrorMessage_ != errorMessage;

    connectedInputDeviceInventory_.reset();
    inputDevicesObservedAtMs_ = 0;
    inputDeviceDiscoveryAvailable_ = false;
    inputDeviceDiscoveryBusy_ = false;
    inputDeviceDiscoveryErrorName_ = errorName;
    inputDeviceDiscoveryErrorMessage_ = errorMessage;
    savedInputDevices_.reset();
    inputDeviceProjectionRevision_ = 0;
    inputDeviceProjectionErrorName_ = errorName;
    inputDeviceProjectionErrorMessage_ = errorMessage;

    const auto projectionChanges = updateInputDeviceProjection();
    if (discoveryChanged || projectionChanges.discovery) {
        emit inputDeviceDiscoveryChanged();
    }
    if (projectionChanged || projectionChanges.projection) {
        emit inputDeviceProjectionChanged();
    }
}

void CompositorClient::setBusy(const bool busy)
{
    if (!busy) setBusyOperation({});
    if (busy == busy_) return;
    busy_ = busy;
    emit busyChanged();
    emitAllOptionGroupsChanged();
}

void CompositorClient::setBusyOperation(const QString &operation)
{
    if (operation == busyOperation_) return;
    busyOperation_ = operation;
    emit busyOperationChanged();
}

QString CompositorClient::scopedErrorName(const OptionGroup group) const
{
    const auto *state = optionGroupState(group);
    if (state == nullptr) return {};
    if (!state->projectionErrorName.isEmpty()
        || !state->projectionErrorMessage.isEmpty()) {
        return state->projectionErrorName;
    }
    if ((group == OptionGroup::Input || group == OptionGroup::Workspaces)
        && (!state->complexProjectionErrorName.isEmpty()
            || !state->complexProjectionErrorMessage.isEmpty())) {
        return state->complexProjectionErrorName;
    }
    if (!state->authorityErrorName.isEmpty()
        || !state->authorityErrorMessage.isEmpty()) {
        return state->authorityErrorName;
    }
    return state->operationErrorName;
}

QString CompositorClient::scopedErrorMessage(const OptionGroup group) const
{
    const auto *state = optionGroupState(group);
    if (state == nullptr) return {};
    if (!state->projectionErrorName.isEmpty()
        || !state->projectionErrorMessage.isEmpty()) {
        return state->projectionErrorMessage;
    }
    if ((group == OptionGroup::Input || group == OptionGroup::Workspaces)
        && (!state->complexProjectionErrorName.isEmpty()
            || !state->complexProjectionErrorMessage.isEmpty())) {
        return state->complexProjectionErrorMessage;
    }
    if (!state->authorityErrorName.isEmpty()
        || !state->authorityErrorMessage.isEmpty()) {
        return state->authorityErrorMessage;
    }
    return state->operationErrorMessage;
}

bool CompositorClient::optionGroupAvailable(const OptionGroup group) const
{
    const auto *state = optionGroupState(group);
    return state != nullptr && available_ && catalogAvailable_
        && actionCatalogAvailable_ && completeSnapshotValid_
        && state->projectionValid
        && (group != OptionGroup::Appearance
            || state->appearanceAnimationProjectionValid)
        && (group != OptionGroup::Input
            || state->inputGesturesProjectionValid)
        && (group != OptionGroup::Workspaces
            || state->workspaceRulesProjectionValid)
        && (group != OptionGroup::Bindings || actionCatalogAvailable())
        && writable_ && !busy_
        && revision_ != std::numeric_limits<qulonglong>::max()
        && managementState_ == QStringLiteral("managed")
        && displayConfirmationState_ == QStringLiteral("idle")
        && applyState_ == QStringLiteral("current")
        && appliedRevision_ == revision_
        && requiredActivation_ == QStringLiteral("none");
}

CompositorClient::OptionGroupState *CompositorClient::optionGroupState(
    const OptionGroup group
)
{
    switch (group) {
    case OptionGroup::AllOptions: return &optionGroups_.at(0);
    case OptionGroup::Appearance: return &optionGroups_.at(1);
    case OptionGroup::Input: return &optionGroups_.at(2);
    case OptionGroup::Windows: return &optionGroups_.at(3);
    case OptionGroup::Workspaces: return &optionGroups_.at(4);
    case OptionGroup::Advanced: return &optionGroups_.at(5);
    case OptionGroup::Rules: return &optionGroups_.at(6);
    case OptionGroup::Bindings: return &optionGroups_.at(7);
    case OptionGroup::Environment: return &optionGroups_.at(8);
    case OptionGroup::Permissions: return &optionGroups_.at(9);
    case OptionGroup::InputDevices: return &optionGroups_.at(10);
    case OptionGroup::None: return nullptr;
    }
    return nullptr;
}

const CompositorClient::OptionGroupState *CompositorClient::optionGroupState(
    const OptionGroup group
) const
{
    switch (group) {
    case OptionGroup::AllOptions: return &optionGroups_.at(0);
    case OptionGroup::Appearance: return &optionGroups_.at(1);
    case OptionGroup::Input: return &optionGroups_.at(2);
    case OptionGroup::Windows: return &optionGroups_.at(3);
    case OptionGroup::Workspaces: return &optionGroups_.at(4);
    case OptionGroup::Advanced: return &optionGroups_.at(5);
    case OptionGroup::Rules: return &optionGroups_.at(6);
    case OptionGroup::Bindings: return &optionGroups_.at(7);
    case OptionGroup::Environment: return &optionGroups_.at(8);
    case OptionGroup::Permissions: return &optionGroups_.at(9);
    case OptionGroup::InputDevices: return &optionGroups_.at(10);
    case OptionGroup::None: return nullptr;
    }
    return nullptr;
}

QString CompositorClient::optionGroupName(const OptionGroup group)
{
    switch (group) {
    case OptionGroup::AllOptions: return QStringLiteral("options");
    case OptionGroup::Appearance: return QStringLiteral("appearance");
    case OptionGroup::Input: return QStringLiteral("input");
    case OptionGroup::Windows: return QStringLiteral("windows");
    case OptionGroup::Workspaces: return QStringLiteral("workspaces");
    case OptionGroup::Advanced: return QStringLiteral("advanced");
    case OptionGroup::Rules: return QStringLiteral("rules");
    case OptionGroup::Bindings: return QStringLiteral("bindings");
    case OptionGroup::Environment: return QStringLiteral("environment");
    case OptionGroup::Permissions: return QStringLiteral("permissions");
    case OptionGroup::InputDevices: return QStringLiteral("input-devices");
    case OptionGroup::None: return QStringLiteral("compositor");
    }
    return {};
}

QString CompositorClient::optionGroupErrorSuffix(const OptionGroup group)
{
    switch (group) {
    case OptionGroup::AllOptions: return QStringLiteral("Options");
    case OptionGroup::Appearance: return QStringLiteral("Appearance");
    case OptionGroup::Input: return QStringLiteral("Input");
    case OptionGroup::Windows: return QStringLiteral("Windows");
    case OptionGroup::Workspaces: return QStringLiteral("Workspaces");
    case OptionGroup::Advanced: return QStringLiteral("Advanced");
    case OptionGroup::Rules: return QStringLiteral("Rules");
    case OptionGroup::Bindings: return QStringLiteral("Bindings");
    case OptionGroup::Environment: return QStringLiteral("Environment");
    case OptionGroup::Permissions: return QStringLiteral("Permissions");
    case OptionGroup::InputDevices: return QStringLiteral("InputDevices");
    case OptionGroup::None: return {};
    }
    return {};
}

QString CompositorClient::optionGroupSaveOperation(const OptionGroup group)
{
    return group == OptionGroup::None
        ? QStringLiteral("compositor-save")
        : optionGroupName(group) + QStringLiteral("-save");
}

QString CompositorClient::optionGroupApplyOperation(const OptionGroup group)
{
    return group == OptionGroup::None
        ? QStringLiteral("compositor-apply")
        : optionGroupName(group) + QStringLiteral("-apply");
}

void CompositorClient::emitOptionGroupChanged(const OptionGroup group)
{
    switch (group) {
    case OptionGroup::AllOptions:
        emit allOptionsChanged();
        break;
    case OptionGroup::Appearance:
        emit appearanceChanged();
        break;
    case OptionGroup::Input:
        emit inputChanged();
        break;
    case OptionGroup::Windows:
        emit windowsChanged();
        break;
    case OptionGroup::Workspaces:
        emit workspacesChanged();
        break;
    case OptionGroup::Advanced:
        emit advancedChanged();
        break;
    case OptionGroup::Rules:
        emit rulesChanged();
        break;
    case OptionGroup::Bindings:
        emit bindingsChanged();
        break;
    case OptionGroup::Environment:
        emit environmentChanged();
        break;
    case OptionGroup::Permissions:
        emit permissionsChanged();
        break;
    case OptionGroup::InputDevices:
        emit inputDevicesChanged();
        break;
    case OptionGroup::None:
        break;
    }
}

void CompositorClient::emitAllOptionGroupsChanged()
{
    emit allOptionsChanged();
    emit appearanceChanged();
    emit inputChanged();
    emit windowsChanged();
    emit workspacesChanged();
    emit advancedChanged();
    emit rulesChanged();
    emit bindingsChanged();
    emit environmentChanged();
    emit permissionsChanged();
    emit inputDevicesChanged();
}

void CompositorClient::setProjectionError(
    const OptionGroup group,
    const QString &name,
    const QString &message
)
{
    const auto previousName = scopedErrorName(group);
    const auto previousMessage = scopedErrorMessage(group);
    auto *state = optionGroupState(group);
    if (state == nullptr) return;
    state->projectionErrorName = name;
    state->projectionErrorMessage = message;
    if (previousName != scopedErrorName(group)
        || previousMessage != scopedErrorMessage(group)) {
        emitOptionGroupChanged(group);
    }
}

void CompositorClient::setOperationError(
    const OptionGroup group,
    const QString &name,
    const QString &message
)
{
    const auto previousName = scopedErrorName(group);
    const auto previousMessage = scopedErrorMessage(group);
    auto *state = optionGroupState(group);
    if (state == nullptr) return;
    state->operationErrorName = name;
    state->operationErrorMessage = message;
    if (previousName != scopedErrorName(group)
        || previousMessage != scopedErrorMessage(group)) {
        emitOptionGroupChanged(group);
    }
    emit operationFailed(name, message);
}

void CompositorClient::setAuthorityError(
    const OptionGroup group,
    const QString &name,
    const QString &message
)
{
    const auto previousName = scopedErrorName(group);
    const auto previousMessage = scopedErrorMessage(group);
    auto *state = optionGroupState(group);
    if (state == nullptr) return;
    state->authorityErrorName = name;
    state->authorityErrorMessage = message.left(
        maximumAuthorityErrorCodeUnits
    );
    if (previousName != scopedErrorName(group)
        || previousMessage != scopedErrorMessage(group)) {
        emitOptionGroupChanged(group);
    }
}

void CompositorClient::clearAuthorityErrors()
{
    for (const auto group : {
             OptionGroup::AllOptions,
             OptionGroup::Appearance,
             OptionGroup::Input,
             OptionGroup::Windows,
             OptionGroup::Workspaces,
             OptionGroup::Advanced,
             OptionGroup::Rules,
             OptionGroup::Bindings,
             OptionGroup::Environment,
             OptionGroup::Permissions,
             OptionGroup::InputDevices,
         }) {
        setAuthorityError(group, {}, {});
    }
}

void CompositorClient::clearOperationError(const OptionGroup group)
{
    const auto previousName = scopedErrorName(group);
    const auto previousMessage = scopedErrorMessage(group);
    auto *state = optionGroupState(group);
    if (state == nullptr) return;
    state->operationErrorName.clear();
    state->operationErrorMessage.clear();
    if (previousName != scopedErrorName(group)
        || previousMessage != scopedErrorMessage(group)) {
        emitOptionGroupChanged(group);
    }
}

void CompositorClient::updateOptionGroupProjection(const OptionGroup group)
{
    auto *state = optionGroupState(group);
    if (state == nullptr) return;
    if (group == OptionGroup::Rules) {
        const auto previousCompleteSnapshotValid = completeSnapshotValid_;
        auto *appearanceState = optionGroupState(OptionGroup::Appearance);
        auto *inputState = optionGroupState(OptionGroup::Input);
        auto *workspacesState = optionGroupState(OptionGroup::Workspaces);
        auto *bindingsState = optionGroupState(OptionGroup::Bindings);
        auto *environmentState = optionGroupState(OptionGroup::Environment);
        auto *permissionsState = optionGroupState(OptionGroup::Permissions);
        auto *inputDevicesState = optionGroupState(OptionGroup::InputDevices);
        QVariantList nextAppearanceCurves;
        QVariantList nextAppearanceAnimations;
        QVariantList nextInputGestures;
        QVariantList nextInputGestureCompatibility;
        QVariantList nextWindowRules;
        QVariantList nextLayerRules;
        QVariantList nextWorkspaceRules;
        QVariantList nextBindings;
        QVariantList nextSubmaps;
        QVariantList nextEnvironment;
        QVariantList nextPermissions;
        QVariantList nextInputDevices;
        auto nextAppearanceAnimationValid = false;
        auto nextInputGesturesValid = false;
        auto nextValid = false;
        auto nextWorkspaceRulesValid = false;
        auto nextBindingsValid = false;
        auto nextEnvironmentValid = false;
        auto nextPermissionsValid = false;
        auto nextInputDevicesValid = false;
        auto nextCompleteSnapshotValid = false;
        auto projectionChecked = false;
        QString nextWorkspaceRulesErrorName;
        QString nextWorkspaceRulesErrorMessage;
        QString nextInputGesturesErrorName;
        QString nextInputGesturesErrorMessage;
        QString nextBindingsErrorName;
        QString nextBindingsErrorMessage;
        QString nextEnvironmentErrorName;
        QString nextEnvironmentErrorMessage;
        QString nextPermissionsErrorName;
        QString nextPermissionsErrorMessage;
        QString nextInputDevicesErrorName;
        QString nextInputDevicesErrorMessage;
        if (catalogAvailable_ && actionCatalogAvailable_
            && optionCatalog_ != nullptr && actionCatalog_ != nullptr
            && optionCatalog_->digest() == catalogDigest_
            && actionCatalog_->digest() == actionCatalogDigest_) {
            projectionChecked = true;
            auto bytes = Hyprland::JsonSupport::canonicalJson(snapshotObject_);
            bytes.append('\n');
            const auto parsed = Hyprland::parseDesiredState(
                bytes, optionCatalog_->catalog(), actionCatalog_->catalog()
            );
            if (parsed) {
                nextWindowRules = snapshotObject_.value(
                    QStringLiteral("windowRules")
                ).toArray().toVariantList();
                nextLayerRules = snapshotObject_.value(
                    QStringLiteral("layerRules")
                ).toArray().toVariantList();
                const auto authoredWorkspaceRules = snapshotObject_.value(
                    QStringLiteral("workspaceRules")
                ).toArray();
                auto protectedRuleIsExact = !authoredWorkspaceRules.isEmpty()
                    && authoredWorkspaceRules.last().isObject()
                    && authoredWorkspaceRules.last().toObject()
                        == protectedWorkspaceRule();
                for (qsizetype index = 0;
                     protectedRuleIsExact
                         && index < authoredWorkspaceRules.size() - 1;
                     ++index) {
                    const auto value = authoredWorkspaceRules.at(index);
                    if (!value.isObject()
                        || isProtectedWorkspaceIdentity(value.toObject())) {
                        protectedRuleIsExact = false;
                        break;
                    }
                    nextWorkspaceRules.append(value.toVariant());
                }
                if (protectedRuleIsExact) {
                    nextAppearanceCurves = snapshotObject_.value(
                        QStringLiteral("curves")
                    ).toArray().toVariantList();
                    nextAppearanceAnimations = snapshotObject_.value(
                        QStringLiteral("animations")
                    ).toArray().toVariantList();
                    const auto gestureArray = snapshotObject_.value(
                        QStringLiteral("gestures")
                    ).toArray();
                    nextInputGestures = gestureArray.toVariantList();
                    nextInputGestureCompatibility =
                        gestureCompatibilityProjection(gestureArray);
                    nextBindings = snapshotObject_.value(
                        QStringLiteral("bindings")
                    ).toArray().toVariantList();
                    nextSubmaps = snapshotObject_.value(
                        QStringLiteral("submaps")
                    ).toArray().toVariantList();
                    nextEnvironment = snapshotObject_.value(
                        QStringLiteral("environment")
                    ).toArray().toVariantList();
                    nextPermissions = snapshotObject_.value(
                        QStringLiteral("permissions")
                    ).toArray().toVariantList();
                    nextInputDevices = snapshotObject_.value(
                        QStringLiteral("devices")
                    ).toArray().toVariantList();
                    nextAppearanceAnimationValid = true;
                    nextInputGesturesValid =
                        authoredGestureActions(actionCatalog_.get()).size() == 9;
                    nextWorkspaceRulesValid = true;
                    nextBindingsValid = actionCatalogAvailable();
                    nextEnvironmentValid = true;
                    nextPermissionsValid = true;
                    nextInputDevicesValid = true;
                    nextCompleteSnapshotValid = true;
                    clearAuthorityErrors();
                } else {
                    completeSnapshotValid_ = false;
                    nextWorkspaceRules.clear();
                    nextWorkspaceRulesErrorName = clientErrorPrefix
                        + QStringLiteral("InvalidWorkspaceRulesSnapshot");
                    nextWorkspaceRulesErrorMessage = QStringLiteral(
                        "The protected HyprShelld workspace rule must be unique and final"
                    );
                    const auto name = clientErrorPrefix
                        + QStringLiteral("InvalidSnapshotAuthority");
                    for (const auto affected : {
                             OptionGroup::AllOptions,
                             OptionGroup::Appearance,
                             OptionGroup::Input,
                             OptionGroup::Windows,
                             OptionGroup::Workspaces,
                             OptionGroup::Advanced,
                             OptionGroup::Rules,
                             OptionGroup::Bindings,
                             OptionGroup::Environment,
                             OptionGroup::Permissions,
                             OptionGroup::InputDevices,
                         }) {
                        setAuthorityError(
                            affected, name, nextWorkspaceRulesErrorMessage
                        );
                    }
                }
                nextValid = true;
                setProjectionError(group, {}, {});
            } else {
                completeSnapshotValid_ = false;
                const auto detail = parsed.errors.isEmpty()
                    ? QStringLiteral(
                        "The complete compositor snapshot could not be verified"
                    )
                    : QStringLiteral("%1: %2")
                        .arg(
                            parsed.errors.constFirst().path,
                            parsed.errors.constFirst().message
                        );
                const auto gestureError = std::ranges::find_if(
                    parsed.errors,
                    [](const auto &candidate) {
                        return candidate.path.startsWith(
                            QStringLiteral("$.gestures")
                        );
                    }
                );
                if (gestureError != parsed.errors.cend()) {
                    nextInputGesturesErrorName = clientErrorPrefix
                        + QStringLiteral("InvalidInputGesturesSnapshot");
                    nextInputGesturesErrorMessage = QStringLiteral("%1: %2")
                        .arg(gestureError->path, gestureError->message);
                }
                const auto bindingsError = std::ranges::find_if(
                    parsed.errors,
                    [](const auto &candidate) {
                        return candidate.path.startsWith(
                                   QStringLiteral("$.bindings")
                               )
                            || candidate.path.startsWith(
                                QStringLiteral("$.submaps")
                            );
                    }
                );
                if (bindingsError != parsed.errors.cend()) {
                    nextBindingsErrorName = clientErrorPrefix
                        + QStringLiteral("InvalidBindingsSnapshot");
                    nextBindingsErrorMessage = QStringLiteral("%1: %2")
                        .arg(bindingsError->path, bindingsError->message);
                }
                const auto environmentError = std::ranges::find_if(
                    parsed.errors,
                    [](const auto &candidate) {
                        return candidate.path.startsWith(
                            QStringLiteral("$.environment")
                        );
                    }
                );
                if (environmentError != parsed.errors.cend()) {
                    nextEnvironmentErrorName = clientErrorPrefix
                        + QStringLiteral("InvalidEnvironmentSnapshot");
                    nextEnvironmentErrorMessage = QStringLiteral("%1: %2")
                        .arg(environmentError->path, environmentError->message);
                }
                const auto permissionsError = std::ranges::find_if(
                    parsed.errors,
                    [](const auto &candidate) {
                        return candidate.path.startsWith(
                            QStringLiteral("$.permissions")
                        );
                    }
                );
                if (permissionsError != parsed.errors.cend()) {
                    nextPermissionsErrorName = clientErrorPrefix
                        + QStringLiteral("InvalidPermissionsSnapshot");
                    nextPermissionsErrorMessage = QStringLiteral("%1: %2")
                        .arg(permissionsError->path, permissionsError->message);
                }
                const auto inputDevicesError = std::ranges::find_if(
                    parsed.errors,
                    [](const auto &candidate) {
                        return candidate.path.startsWith(
                            QStringLiteral("$.devices")
                        );
                    }
                );
                if (inputDevicesError != parsed.errors.cend()) {
                    nextInputDevicesErrorName = clientErrorPrefix
                        + QStringLiteral("InvalidInputDevicesSnapshot");
                    nextInputDevicesErrorMessage = QStringLiteral("%1: %2")
                        .arg(
                            inputDevicesError->path,
                            inputDevicesError->message
                        );
                }
                const auto name = clientErrorPrefix
                    + QStringLiteral("InvalidSnapshotAuthority");
                for (const auto affected : {
                         OptionGroup::AllOptions,
                         OptionGroup::Appearance,
                         OptionGroup::Input,
                         OptionGroup::Windows,
                         OptionGroup::Workspaces,
                         OptionGroup::Advanced,
                         OptionGroup::Rules,
                         OptionGroup::Bindings,
                         OptionGroup::Environment,
                         OptionGroup::Permissions,
                         OptionGroup::InputDevices,
                     }) {
                    setAuthorityError(affected, name, detail);
                }
                setProjectionError(group, {}, {});
            }
        }

        const auto previousWorkspacesErrorName = scopedErrorName(
            OptionGroup::Workspaces
        );
        const auto previousWorkspacesErrorMessage = scopedErrorMessage(
            OptionGroup::Workspaces
        );
        const auto workspaceProjectionChanged = workspacesState != nullptr
            && (nextWorkspaceRulesValid
                    != workspacesState->workspaceRulesProjectionValid
                || nextWorkspaceRules != workspacesState->workspaceRules);
        if (workspacesState != nullptr) {
            workspacesState->workspaceRulesProjectionValid =
                nextWorkspaceRulesValid;
            workspacesState->workspaceRules = std::move(nextWorkspaceRules);
            workspacesState->complexProjectionErrorName =
                nextWorkspaceRulesErrorName;
            workspacesState->complexProjectionErrorMessage =
                nextWorkspaceRulesErrorMessage;
        }
        const auto workspacesErrorChanged = previousWorkspacesErrorName
                != scopedErrorName(OptionGroup::Workspaces)
            || previousWorkspacesErrorMessage
                != scopedErrorMessage(OptionGroup::Workspaces);
        if (workspaceProjectionChanged || workspacesErrorChanged) {
            emit workspacesChanged();
        }

        const auto previousInputErrorName = scopedErrorName(
            OptionGroup::Input
        );
        const auto previousInputErrorMessage = scopedErrorMessage(
            OptionGroup::Input
        );
        const auto inputGesturesProjectionChanged = inputState != nullptr
            && (nextInputGesturesValid
                    != inputState->inputGesturesProjectionValid
                || nextInputGestures != inputState->inputGestures
                || nextInputGestureCompatibility
                    != inputState->inputGestureCompatibility);
        if (inputState != nullptr) {
            inputState->inputGesturesProjectionValid = nextInputGesturesValid;
            inputState->inputGestures = std::move(nextInputGestures);
            inputState->inputGestureCompatibility =
                std::move(nextInputGestureCompatibility);
            inputState->complexProjectionErrorName =
                nextInputGesturesErrorName;
            inputState->complexProjectionErrorMessage =
                nextInputGesturesErrorMessage;
        }
        const auto inputErrorChanged = previousInputErrorName
                != scopedErrorName(OptionGroup::Input)
            || previousInputErrorMessage
                != scopedErrorMessage(OptionGroup::Input);
        if (inputGesturesProjectionChanged || inputErrorChanged) {
            emit inputChanged();
        }

        const auto updateCollectionState = [this, projectionChecked](
            const OptionGroup collectionGroup,
            OptionGroupState *collectionState,
            const bool valid,
            const QVariantList &first,
            const QVariantList &second,
            const QString &errorName,
            const QString &errorMessage
        ) {
            if (collectionState == nullptr) return;
            const auto previousErrorName = scopedErrorName(collectionGroup);
            const auto previousErrorMessage = scopedErrorMessage(
                collectionGroup
            );
            auto changed = collectionState->projectionValid != valid;
            collectionState->projectionValid = valid;
            switch (collectionGroup) {
            case OptionGroup::Bindings:
                changed = changed || collectionState->bindings != first
                    || collectionState->submaps != second;
                collectionState->bindings = first;
                collectionState->submaps = second;
                break;
            case OptionGroup::Environment:
                changed = changed || collectionState->environment != first;
                collectionState->environment = first;
                break;
            case OptionGroup::Permissions:
                changed = changed || collectionState->permissions != first;
                collectionState->permissions = first;
                break;
            case OptionGroup::InputDevices:
                changed = changed || collectionState->inputDevices != first;
                collectionState->inputDevices = first;
                break;
            case OptionGroup::None:
            case OptionGroup::AllOptions:
            case OptionGroup::Appearance:
            case OptionGroup::Input:
            case OptionGroup::Windows:
            case OptionGroup::Workspaces:
            case OptionGroup::Advanced:
            case OptionGroup::Rules:
                return;
            }
            if (projectionChecked) {
                collectionState->projectionErrorName = errorName;
                collectionState->projectionErrorMessage = errorMessage;
            }
            const auto errorChanged = previousErrorName
                    != scopedErrorName(collectionGroup)
                || previousErrorMessage != scopedErrorMessage(collectionGroup);
            if (changed || errorChanged) {
                emitOptionGroupChanged(collectionGroup);
            }
        };
        updateCollectionState(
            OptionGroup::Bindings,
            bindingsState,
            nextBindingsValid,
            nextBindings,
            nextSubmaps,
            nextBindingsErrorName,
            nextBindingsErrorMessage
        );
        updateCollectionState(
            OptionGroup::Environment,
            environmentState,
            nextEnvironmentValid,
            nextEnvironment,
            {},
            nextEnvironmentErrorName,
            nextEnvironmentErrorMessage
        );
        updateCollectionState(
            OptionGroup::Permissions,
            permissionsState,
            nextPermissionsValid,
            nextPermissions,
            {},
            nextPermissionsErrorName,
            nextPermissionsErrorMessage
        );
        updateCollectionState(
            OptionGroup::InputDevices,
            inputDevicesState,
            nextInputDevicesValid,
            nextInputDevices,
            {},
            nextInputDevicesErrorName,
            nextInputDevicesErrorMessage
        );

        const auto authorityChanged =
            nextCompleteSnapshotValid != previousCompleteSnapshotValid;
        completeSnapshotValid_ = nextCompleteSnapshotValid;
        const auto appearanceProjectionChanged = appearanceState != nullptr
            && (nextAppearanceAnimationValid
                    != appearanceState->appearanceAnimationProjectionValid
                || nextAppearanceCurves != appearanceState->appearanceCurves
                || nextAppearanceAnimations
                    != appearanceState->appearanceAnimations);
        if (appearanceState != nullptr) {
            appearanceState->appearanceAnimationProjectionValid =
                nextAppearanceAnimationValid;
            appearanceState->appearanceCurves =
                std::move(nextAppearanceCurves);
            appearanceState->appearanceAnimations =
                std::move(nextAppearanceAnimations);
        }
        if (appearanceProjectionChanged) emit appearanceChanged();
        const auto projectionChanged = nextValid != state->projectionValid
            || nextWindowRules != state->windowRules
            || nextLayerRules != state->layerRules;
        state->projectionValid = nextValid;
        state->windowRules = std::move(nextWindowRules);
        state->layerRules = std::move(nextLayerRules);
        if (projectionChanged) emit rulesChanged();
        if (authorityChanged) emitAllOptionGroupsChanged();
        return;
    }
    QVariantMap nextValues;
    auto nextValid = false;
    QString error;
    if (catalogAvailable_ && optionCatalog_ != nullptr
        && optionCatalog_->digest() == catalogDigest_) {
        auto contractAvailable = false;
        QString contractError;
        std::optional<QVariantMap> values;
        switch (group) {
        case OptionGroup::AllOptions:
            contractAvailable = optionCatalog_->allOptionsContractAvailable();
            contractError = optionCatalog_->allOptionsContractError();
            if (contractAvailable) {
                values = optionCatalog_->allValues(snapshotObject_, error);
            }
            break;
        case OptionGroup::Appearance:
            contractAvailable = optionCatalog_->appearanceContractAvailable();
            contractError = optionCatalog_->appearanceContractError();
            if (contractAvailable) {
                values = optionCatalog_->appearanceValues(snapshotObject_, error);
            }
            break;
        case OptionGroup::Input:
            contractAvailable = optionCatalog_->inputContractAvailable();
            contractError = optionCatalog_->inputContractError();
            if (contractAvailable) {
                values = optionCatalog_->inputValues(snapshotObject_, error);
            }
            break;
        case OptionGroup::Windows:
            contractAvailable = optionCatalog_->windowsContractAvailable();
            contractError = optionCatalog_->windowsContractError();
            if (contractAvailable) {
                values = optionCatalog_->windowsValues(snapshotObject_, error);
            }
            break;
        case OptionGroup::Workspaces:
            contractAvailable = optionCatalog_->workspacesContractAvailable();
            contractError = optionCatalog_->workspacesContractError();
            if (contractAvailable) {
                values = optionCatalog_->workspacesValues(
                    snapshotObject_, error
                );
            }
            break;
        case OptionGroup::Advanced:
            contractAvailable = optionCatalog_->advancedContractAvailable();
            contractError = optionCatalog_->advancedContractError();
            if (contractAvailable) {
                values = optionCatalog_->advancedValues(snapshotObject_, error);
            }
            break;
        case OptionGroup::Rules:
        case OptionGroup::Bindings:
        case OptionGroup::Environment:
        case OptionGroup::Permissions:
        case OptionGroup::InputDevices:
            break;
        case OptionGroup::None:
            break;
        }
        const auto suffix = optionGroupErrorSuffix(group);
        if (!contractAvailable) {
            setProjectionError(
                group,
                clientErrorPrefix + QStringLiteral("Invalid") + suffix
                    + QStringLiteral("Catalog"),
                contractError
            );
        } else if (values) {
            nextValues = *values;
            nextValid = true;
            setProjectionError(group, {}, {});
        } else {
            setProjectionError(
                group,
                clientErrorPrefix + QStringLiteral("Invalid") + suffix
                    + QStringLiteral("Snapshot"),
                error
            );
        }
    }
    if (nextValid == state->projectionValid && nextValues == state->values) {
        return;
    }
    state->projectionValid = nextValid;
    state->values = std::move(nextValues);
    emitOptionGroupChanged(group);
}

void CompositorClient::updateAllOptionGroupProjections()
{
    updateOptionGroupProjection(OptionGroup::AllOptions);
    updateOptionGroupProjection(OptionGroup::Appearance);
    updateOptionGroupProjection(OptionGroup::Input);
    updateOptionGroupProjection(OptionGroup::Windows);
    updateOptionGroupProjection(OptionGroup::Workspaces);
    updateOptionGroupProjection(OptionGroup::Advanced);
    updateOptionGroupProjection(OptionGroup::Rules);
}

void CompositorClient::setError(
    const QString &operation,
    const QString &name,
    const QString &message
)
{
    if (operation != lastErrorOperation_ || name != lastErrorName_
        || message != lastErrorMessage_) {
        lastErrorOperation_ = operation;
        lastErrorName_ = name;
        lastErrorMessage_ = message;
        emit lastErrorChanged();
    }
    emit operationFailed(name, message);
}

} // namespace HyprShelld
