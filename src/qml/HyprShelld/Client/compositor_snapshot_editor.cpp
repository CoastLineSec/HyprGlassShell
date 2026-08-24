#include "compositor_snapshot_editor.h"

#include "compositor_option_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QMap>
#include <QMetaType>
#include <QSet>

#include <cmath>
#include <limits>
#include <ranges>
#include <utility>

namespace HyprShelld {
namespace {

[[nodiscard]] bool snapshotMatchesAuthority(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest
)
{
    static const QSet<QString> exactFields{
        QStringLiteral("formatVersion"),
        QStringLiteral("revision"),
        QStringLiteral("targetHyprland"),
        QStringLiteral("catalogDigest"),
        QStringLiteral("actionCatalogDigest"),
        QStringLiteral("overrides"),
        QStringLiteral("monitors"),
        QStringLiteral("devices"),
        QStringLiteral("curves"),
        QStringLiteral("animations"),
        QStringLiteral("gestures"),
        QStringLiteral("workspaceRules"),
        QStringLiteral("windowRules"),
        QStringLiteral("layerRules"),
        QStringLiteral("submaps"),
        QStringLiteral("bindings"),
        QStringLiteral("permissions"),
        QStringLiteral("environment"),
    };
    const QStringList arrayFields{
        QStringLiteral("monitors"),
        QStringLiteral("devices"),
        QStringLiteral("curves"),
        QStringLiteral("animations"),
        QStringLiteral("gestures"),
        QStringLiteral("workspaceRules"),
        QStringLiteral("windowRules"),
        QStringLiteral("layerRules"),
        QStringLiteral("submaps"),
        QStringLiteral("bindings"),
        QStringLiteral("permissions"),
        QStringLiteral("environment"),
    };
    const auto revisionText = snapshot.value(
        QStringLiteral("revision")
    ).toString();
    const auto snapshotKeys = snapshot.keys();
    const QSet<QString> actualFields(
        snapshotKeys.cbegin(), snapshotKeys.cend()
    );
    return actualFields == exactFields
        && snapshot.value(QStringLiteral("formatVersion")).toInt(-1) == 1
        && snapshot.value(QStringLiteral("revision")).isString()
        && revisionText == QString::number(expectedRevision)
        && snapshot.value(QStringLiteral("targetHyprland")).isString()
        && !snapshot.value(QStringLiteral("targetHyprland")).toString().isEmpty()
        && snapshot.value(QStringLiteral("catalogDigest")).toString()
            == expectedCatalogDigest
        && snapshot.value(QStringLiteral("actionCatalogDigest")).toString()
            == expectedActionCatalogDigest
        && snapshot.value(QStringLiteral("overrides")).isObject()
        && std::ranges::all_of(arrayFields, [&snapshot](const QString &field) {
            return snapshot.value(field).isArray();
        });
}

enum class OptionGroup {
    Appearance,
    Input,
    Windows,
    Workspaces,
    Advanced,
};

[[nodiscard]] QString groupName(const OptionGroup group)
{
    switch (group) {
    case OptionGroup::Appearance: return QStringLiteral("appearance");
    case OptionGroup::Input: return QStringLiteral("input");
    case OptionGroup::Windows: return QStringLiteral("windows");
    case OptionGroup::Workspaces: return QStringLiteral("workspaces");
    case OptionGroup::Advanced: return QStringLiteral("advanced");
    }
    return {};
}

[[nodiscard]] bool groupContractAvailable(
    const CompositorOptionCatalog &catalog,
    const OptionGroup group
)
{
    switch (group) {
    case OptionGroup::Appearance:
        return catalog.appearanceContractAvailable();
    case OptionGroup::Input:
        return catalog.inputContractAvailable();
    case OptionGroup::Windows:
        return catalog.windowsContractAvailable();
    case OptionGroup::Workspaces:
        return catalog.workspacesContractAvailable();
    case OptionGroup::Advanced:
        return catalog.advancedContractAvailable();
    }
    return false;
}

[[nodiscard]] QStringList groupOptionIds(
    const CompositorOptionCatalog &catalog,
    const OptionGroup group
)
{
    switch (group) {
    case OptionGroup::Appearance: return catalog.appearanceOptionIds();
    case OptionGroup::Input: return catalog.inputOptionIds();
    case OptionGroup::Windows: return catalog.windowsOptionIds();
    case OptionGroup::Workspaces: return catalog.workspacesOptionIds();
    case OptionGroup::Advanced: return catalog.advancedOptionIds();
    }
    return {};
}

[[nodiscard]] const Hyprland::OptionDefinition *groupOption(
    const CompositorOptionCatalog &catalog,
    const OptionGroup group,
    const QString &id
)
{
    switch (group) {
    case OptionGroup::Appearance: return catalog.appearanceOption(id);
    case OptionGroup::Input: return catalog.inputOption(id);
    case OptionGroup::Windows: return catalog.windowsOption(id);
    case OptionGroup::Workspaces: return catalog.workspacesOption(id);
    case OptionGroup::Advanced: return catalog.advancedOption(id);
    }
    return nullptr;
}

[[nodiscard]] QString invalidValueError(const OptionGroup group)
{
    switch (group) {
    case OptionGroup::Appearance:
        return QStringLiteral("An appearance value is invalid");
    case OptionGroup::Input:
        return QStringLiteral("An input value is invalid");
    case OptionGroup::Windows:
        return QStringLiteral("A windows value is invalid");
    case OptionGroup::Workspaces:
        return QStringLiteral("A workspaces value is invalid");
    case OptionGroup::Advanced:
        return QStringLiteral("An advanced value is invalid");
    }
    return {};
}

[[nodiscard]] bool hasStrictOptionValue(
    const Hyprland::OptionDefinition &option,
    const QVariant &variant,
    QJsonValue &value
)
{
    if (!variant.isValid() || variant.isNull()) return false;
    value = QJsonValue::fromVariant(variant);
    if (value.isUndefined() || value.isNull()) return false;
    const auto safeIntegral = [](const QJsonValue &candidate) {
        constexpr auto maximumSafeInteger = 9007199254740991.0;
        return candidate.isDouble()
            && std::isfinite(candidate.toDouble())
            && std::trunc(candidate.toDouble()) == candidate.toDouble()
            && std::abs(candidate.toDouble()) <= maximumSafeInteger;
    };
    switch (option.type) {
    case Hyprland::OptionType::Boolean:
        if (!value.isBool()) return false;
        break;
    case Hyprland::OptionType::Integer:
    case Hyprland::OptionType::FontWeight:
        if (!safeIntegral(value)) return false;
        break;
    case Hyprland::OptionType::Number:
        if (!value.isDouble() || !std::isfinite(value.toDouble())) {
            return false;
        }
        break;
    case Hyprland::OptionType::String:
    case Hyprland::OptionType::Color:
        if (variant.metaType().id() != QMetaType::QString
            || !value.isString()) {
            return false;
        }
        break;
    case Hyprland::OptionType::Enumeration:
        if (option.defaultValue.isString()) {
            if (variant.metaType().id() != QMetaType::QString
                || !value.isString()) {
                return false;
            }
        } else if (option.defaultValue.isDouble()) {
            if (!safeIntegral(value)) return false;
        } else {
            return false;
        }
        break;
    case Hyprland::OptionType::Vector2: {
        if (!value.isArray()) return false;
        const auto parts = value.toArray();
        if (parts.size() != 2
            || std::ranges::any_of(parts, [](const QJsonValue &part) {
                return !part.isDouble() || !std::isfinite(part.toDouble());
            })) {
            return false;
        }
        break;
    }
    case Hyprland::OptionType::CssGap: {
        if (!value.isArray()) return false;
        const auto parts = value.toArray();
        if (parts.size() != 4
            || std::ranges::any_of(parts, [&safeIntegral](const QJsonValue &part) {
                return !safeIntegral(part);
            })) {
            return false;
        }
        break;
    }
    case Hyprland::OptionType::Gradient:
        if (!value.isObject()) return false;
        break;
    }
    return Hyprland::validateOptionValue(option, value).isEmpty();
}

[[nodiscard]] std::optional<QJsonValue> strictJsonValue(
    const QVariant &variant,
    const int depth = 0
)
{
    if (!variant.isValid() || variant.isNull() || depth > 64) {
        return std::nullopt;
    }
    switch (variant.metaType().id()) {
    case QMetaType::Bool:
        return QJsonValue(variant.toBool());
    case QMetaType::QString:
        return QJsonValue(variant.toString());
    case QMetaType::Double:
    case QMetaType::Float: {
        const auto number = variant.toDouble();
        return std::isfinite(number)
            ? std::optional<QJsonValue>(QJsonValue(number))
            : std::nullopt;
    }
    case QMetaType::Int:
    case QMetaType::LongLong: {
        constexpr auto maximumSafeInteger = 9007199254740991LL;
        const auto number = variant.toLongLong();
        if (number < -maximumSafeInteger || number > maximumSafeInteger) {
            return std::nullopt;
        }
        return QJsonValue(static_cast<double>(number));
    }
    case QMetaType::UInt:
    case QMetaType::ULongLong: {
        constexpr auto maximumSafeInteger = 9007199254740991ULL;
        const auto number = variant.toULongLong();
        if (number > maximumSafeInteger) return std::nullopt;
        return QJsonValue(static_cast<double>(number));
    }
    case QMetaType::QVariantList: {
        QJsonArray result;
        const auto list = variant.toList();
        for (const auto &item : list) {
            const auto value = strictJsonValue(item, depth + 1);
            if (!value) return std::nullopt;
            result.append(*value);
        }
        return result;
    }
    case QMetaType::QVariantMap: {
        QJsonObject result;
        const auto map = variant.toMap();
        for (auto iterator = map.cbegin(); iterator != map.cend(); ++iterator) {
            const auto value = strictJsonValue(iterator.value(), depth + 1);
            if (!value) return std::nullopt;
            result.insert(iterator.key(), *value);
        }
        return result;
    }
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<QJsonArray> strictJsonArray(
    const QVariantList &values
)
{
    QJsonArray result;
    for (const auto &item : values) {
        const auto value = strictJsonValue(item, 1);
        if (!value) return std::nullopt;
        result.append(*value);
    }
    return result;
}

[[nodiscard]] bool isPinchGestureDirection(const QString &direction)
{
    return direction == QStringLiteral("pinch")
        || direction == QStringLiteral("pinchIn")
        || direction == QStringLiteral("pinchOut");
}

[[nodiscard]] QString authoredGestureError(const QJsonObject &gesture)
{
    const auto direction = gesture.value(QStringLiteral("direction")).toString();
    const auto pinch = isPinchGestureDirection(direction);
    const auto action = gesture.value(QStringLiteral("action")).toObject();
    const auto actionType = action.value(QStringLiteral("type")).toString();
    if (actionType == QStringLiteral("unset")) {
        return QStringLiteral(
            "Unset gestures are preserved for compatibility but cannot be newly created or edited"
        );
    }
    if (pinch && gesture.value(QStringLiteral("scale")).toDouble() != 1.0) {
        return QStringLiteral(
            "New and edited pinch gestures must use a scale of 1"
        );
    }
    if (pinch && actionType == QStringLiteral("scrollMove")) {
        return QStringLiteral(
            "Scroll Move cannot be assigned to a pinch gesture"
        );
    }
    if (!pinch && actionType == QStringLiteral("cursorZoom")
        && action.value(QStringLiteral("mode")).toString()
            == QStringLiteral("live")) {
        return QStringLiteral(
            "Live cursor zoom can only be assigned to a pinch gesture"
        );
    }
    return {};
}

[[nodiscard]] std::optional<QMap<QString, QString>> logicalCurveTypes(
    const QJsonArray &curves
)
{
    QMap<QString, QString> result;
    for (const auto &value : curves) {
        if (!value.isObject()) return std::nullopt;
        const auto record = value.toObject();
        const auto nameValue = record.value(QStringLiteral("name"));
        const auto typeValue = record.value(QStringLiteral("type"));
        if (!nameValue.isString() || nameValue.toString().isEmpty()
            || !typeValue.isString()
            || (typeValue.toString() != QStringLiteral("bezier")
                && typeValue.toString() != QStringLiteral("spring"))
            || result.contains(nameValue.toString())) {
            return std::nullopt;
        }
        result.insert(nameValue.toString(), typeValue.toString());
    }
    return result;
}

[[nodiscard]] bool replaceGroupValues(
    const OptionGroup group,
    const CompositorOptionCatalog &catalog,
    const QVariantMap &values,
    QJsonObject &candidateObject,
    bool &changed,
    QString &error
)
{
    const auto contractAvailable = groupContractAvailable(catalog, group);
    const auto ids = groupOptionIds(catalog, group);
    const auto name = groupName(group);
    if (!contractAvailable || values.size() != ids.size()
        || QSet<QString>(values.keyBegin(), values.keyEnd())
            != QSet<QString>(ids.cbegin(), ids.cend())) {
        error = QStringLiteral("Exactly the supported %1 values are required")
                    .arg(name);
        return false;
    }

    auto overrides = candidateObject.value(QStringLiteral("overrides")).toObject();
    const auto originalOverrides = overrides;
    for (const auto &id : ids) {
        const auto *option = groupOption(catalog, group, id);
        QJsonValue value;
        if (option == nullptr
            || !hasStrictOptionValue(*option, values.value(id), value)) {
            error = invalidValueError(group);
            return false;
        }
        if (value == option->defaultValue) {
            overrides.remove(id);
        } else {
            overrides.insert(id, value);
        }
    }

    candidateObject.insert(QStringLiteral("overrides"), overrides);
    changed = overrides != originalOverrides;
    return true;
}

[[nodiscard]] bool replaceAllOptionValues(
    const CompositorOptionCatalog &catalog,
    const QVariantMap &values,
    QJsonObject &candidateObject,
    bool &changed,
    QString &error
)
{
    if (!catalog.allOptionsContractAvailable()) {
        error = QStringLiteral("The compositor option catalog is unsupported");
        return false;
    }

    const auto originalOverrides = candidateObject.value(
        QStringLiteral("overrides")
    ).toObject();
    QMap<QString, QJsonValue> strictValues;
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator) {
        const auto *option = catalog.allOption(iterator.key());
        if (option == nullptr || !option->writable) {
            error = QStringLiteral(
                "Only known writable compositor option values are accepted"
            );
            return false;
        }

        QJsonValue value;
        if (!hasStrictOptionValue(*option, iterator.value(), value)) {
            error = QStringLiteral("A compositor option value is invalid");
            return false;
        }
        strictValues.insert(option->id, value);
    }

    auto provisionalOverrides = originalOverrides;
    for (auto iterator = strictValues.cbegin();
         iterator != strictValues.cend(); ++iterator) {
        provisionalOverrides.insert(iterator.key(), iterator.value());
    }

    auto overrides = originalOverrides;
    for (auto iterator = strictValues.cbegin();
         iterator != strictValues.cend(); ++iterator) {
        const auto *option = catalog.allOption(iterator.key());
        if (option == nullptr) {
            error = QStringLiteral(
                "Only known writable compositor option values are accepted"
            );
            return false;
        }

        auto defaultValue = option->defaultValue;
        if (option->inheritedDefaultFrom) {
            auto resolutionOverrides = provisionalOverrides;
            resolutionOverrides.remove(option->id);
            auto resolutionSnapshot = candidateObject;
            resolutionSnapshot.insert(
                QStringLiteral("overrides"), resolutionOverrides
            );
            QString resolutionError;
            const auto resolved = catalog.allValues(
                resolutionSnapshot, resolutionError
            );
            if (!resolved || !resolved->contains(option->id)) {
                error = resolutionError.isEmpty()
                    ? QStringLiteral(
                        "The compositor option default could not be resolved"
                    )
                    : resolutionError;
                return false;
            }
            defaultValue = QJsonValue::fromVariant(
                resolved->value(option->id)
            );
        }

        if (iterator.value() == defaultValue) {
            overrides.remove(option->id);
        } else {
            overrides.insert(option->id, iterator.value());
        }
    }

    candidateObject.insert(QStringLiteral("overrides"), overrides);
    changed = overrides != originalOverrides;
    return true;
}

[[nodiscard]] std::optional<CompositorSnapshotEdit> validatedCandidate(
    QJsonObject candidateObject,
    const bool changed,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    QString &error
)
{
    auto candidate = Hyprland::JsonSupport::canonicalJson(candidateObject);
    candidate.append('\n');
    if (candidate.size() > Hyprland::maximumDesiredStateBytes) {
        error = QStringLiteral("The compositor snapshot exceeds its size limit");
        return std::nullopt;
    }
    const auto parsed = Hyprland::parseDesiredState(
        candidate, catalog.catalog(), actionCatalog.catalog()
    );
    if (!parsed) {
        error = parsed.errors.isEmpty()
            ? QStringLiteral("The complete compositor snapshot is invalid")
            : QStringLiteral("%1: %2")
                .arg(
                    parsed.errors.constFirst().path,
                    parsed.errors.constFirst().message
                );
        return std::nullopt;
    }
    if (changed) {
        const auto safetyErrors = Hyprland::validateManagedActivationSafety(
            *parsed.value, catalog.catalog()
        );
        if (!safetyErrors.isEmpty()) {
            error = QStringLiteral("%1: %2")
                        .arg(
                            safetyErrors.constFirst().path,
                            safetyErrors.constFirst().message
                        );
            return std::nullopt;
        }
    }
    return CompositorSnapshotEdit{
        .candidate = std::move(candidate),
        .changed = changed,
    };
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

[[nodiscard]] std::optional<CompositorSnapshotEdit> replaceGroup(
    const OptionGroup group,
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantMap &values,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || actionCatalog.digest() != expectedActionCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }

    auto candidateObject = snapshot;
    bool changed = false;
    if (!replaceGroupValues(
            group, catalog, values, candidateObject, changed, error
        )) {
        return std::nullopt;
    }
    return validatedCandidate(
        std::move(candidateObject), changed, catalog, actionCatalog, error
    );
}

[[nodiscard]] std::optional<CompositorSnapshotEdit> replaceCollections(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QString &firstField,
    const QVariantList &firstValues,
    const QString &secondField,
    const QVariantList &secondValues,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || actionCatalog.digest() != expectedActionCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }

    const auto first = strictJsonArray(firstValues);
    const auto second = secondField.isEmpty()
        ? std::optional<QJsonArray>(QJsonArray{})
        : strictJsonArray(secondValues);
    if (!first || !second) {
        error = QStringLiteral("A compositor collection record is invalid");
        return std::nullopt;
    }

    auto candidateObject = snapshot;
    auto changed = candidateObject.value(firstField).toArray() != *first;
    candidateObject.insert(firstField, *first);
    if (!secondField.isEmpty()) {
        changed = changed
            || candidateObject.value(secondField).toArray() != *second;
        candidateObject.insert(secondField, *second);
    }
    return validatedCandidate(
        std::move(candidateObject), changed, catalog, actionCatalog, error
    );
}

} // namespace

bool CompositorSnapshotEditor::isExactV1Envelope(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest
)
{
    return snapshotMatchesAuthority(
        snapshot,
        expectedRevision,
        expectedCatalogDigest,
        expectedActionCatalogDigest
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceAllOptions(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantMap &values,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || actionCatalog.digest() != expectedActionCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }

    auto candidateObject = snapshot;
    bool changed = false;
    if (!replaceAllOptionValues(
            catalog, values, candidateObject, changed, error
        )) {
        return std::nullopt;
    }
    return validatedCandidate(
        std::move(candidateObject), changed, catalog, actionCatalog, error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceBindings(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantList &bindings,
    const QVariantList &submaps,
    QString &error
)
{
    return replaceCollections(
        snapshot, expectedRevision, expectedCatalogDigest,
        expectedActionCatalogDigest, catalog, actionCatalog,
        QStringLiteral("bindings"), bindings,
        QStringLiteral("submaps"), submaps, error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceEnvironment(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantList &environment,
    QString &error
)
{
    return replaceCollections(
        snapshot, expectedRevision, expectedCatalogDigest,
        expectedActionCatalogDigest, catalog, actionCatalog,
        QStringLiteral("environment"), environment, {}, {}, error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replacePermissions(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantList &permissions,
    QString &error
)
{
    return replaceCollections(
        snapshot, expectedRevision, expectedCatalogDigest,
        expectedActionCatalogDigest, catalog, actionCatalog,
        QStringLiteral("permissions"), permissions, {}, {}, error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceInputDevices(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantList &devices,
    QString &error
)
{
    return replaceCollections(
        snapshot, expectedRevision, expectedCatalogDigest,
        expectedActionCatalogDigest, catalog, actionCatalog,
        QStringLiteral("devices"), devices, {}, {}, error
    );
}

std::optional<AppearanceSnapshotEdit>
CompositorSnapshotEditor::replaceAppearance(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantMap &values,
    const QVariantList &curves,
    const QVariantList &animations,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || actionCatalog.digest() != expectedActionCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }
    if (curves.size() > Hyprland::maximumCurves
        || animations.size() > Hyprland::maximumAnimations) {
        error = QStringLiteral(
            "The curves or animations collection exceeds its item limit"
        );
        return std::nullopt;
    }

    const auto nextCurves = strictJsonArray(curves);
    const auto nextAnimations = strictJsonArray(animations);
    if (!nextCurves || !nextAnimations) {
        error = QStringLiteral(
            "Curves and animations must contain only maps, lists, strings, booleans, and finite numbers"
        );
        return std::nullopt;
    }

    const auto originalCurves = snapshot.value(
        QStringLiteral("curves")
    ).toArray();
    const auto originalAnimations = snapshot.value(
        QStringLiteral("animations")
    ).toArray();
    const auto originalCurveTypes = logicalCurveTypes(originalCurves);
    const auto nextCurveTypes = logicalCurveTypes(*nextCurves);
    if (!originalCurveTypes || !nextCurveTypes) {
        error = QStringLiteral("The curve structure is invalid");
        return std::nullopt;
    }
    if (*nextCurveTypes != *originalCurveTypes) {
        error = QStringLiteral(
            "Curve additions, removals, renames, and type changes require a verified compositor restart workflow and cannot be saved from Settings"
        );
        return std::nullopt;
    }
    auto candidateObject = snapshot;
    bool scalarChanged = false;
    if (!replaceGroupValues(
            OptionGroup::Appearance,
            catalog,
            values,
            candidateObject,
            scalarChanged,
            error
        )) {
        return std::nullopt;
    }
    candidateObject.insert(QStringLiteral("curves"), *nextCurves);
    candidateObject.insert(QStringLiteral("animations"), *nextAnimations);

    return validatedCandidate(
        std::move(candidateObject),
        scalarChanged || *nextCurves != originalCurves
            || *nextAnimations != originalAnimations,
        catalog,
        actionCatalog,
        error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceInput(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantMap &values,
    const QVariantList &gestures,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || actionCatalog.digest() != expectedActionCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }
    if (gestures.size() > Hyprland::maximumGestures) {
        error = QStringLiteral("The gestures collection exceeds its item limit");
        return std::nullopt;
    }
    const auto nextGestures = strictJsonArray(gestures);
    if (!nextGestures) {
        error = QStringLiteral(
            "Gestures must contain only maps, lists, strings, booleans, and finite numbers"
        );
        return std::nullopt;
    }

    const auto originalGestures = snapshot.value(
        QStringLiteral("gestures")
    ).toArray();
    QMap<QString, QJsonObject> originalById;
    for (const auto &value : originalGestures) {
        if (!value.isObject()) {
            error = QStringLiteral("The saved gestures collection is invalid");
            return std::nullopt;
        }
        const auto object = value.toObject();
        originalById.insert(
            object.value(QStringLiteral("id")).toString(), object
        );
    }
    for (const auto &value : *nextGestures) {
        if (!value.isObject()) {
            error = QStringLiteral("Every gesture must be an object");
            return std::nullopt;
        }
        const auto object = value.toObject();
        const auto id = object.value(QStringLiteral("id")).toString();
        const auto original = originalById.constFind(id);
        if (original != originalById.constEnd()) {
            if (*original == object) continue;
            if (!authoredGestureError(*original).isEmpty()) {
                error = QStringLiteral(
                    "Compatibility gestures can be reordered or removed but cannot be edited"
                );
                return std::nullopt;
            }
        }
        if (const auto authoredError = authoredGestureError(object);
            !authoredError.isEmpty()) {
            error = authoredError;
            return std::nullopt;
        }
    }

    auto candidateObject = snapshot;
    bool scalarChanged = false;
    if (!replaceGroupValues(
            OptionGroup::Input,
            catalog,
            values,
            candidateObject,
            scalarChanged,
            error
        )) {
        return std::nullopt;
    }
    candidateObject.insert(QStringLiteral("gestures"), *nextGestures);
    return validatedCandidate(
        std::move(candidateObject),
        scalarChanged || *nextGestures != originalGestures,
        catalog,
        actionCatalog,
        error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceWindows(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantMap &values,
    QString &error
)
{
    return replaceGroup(
        OptionGroup::Windows,
        snapshot,
        expectedRevision,
        expectedCatalogDigest,
        expectedActionCatalogDigest,
        catalog,
        actionCatalog,
        values,
        error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceWorkspaces(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantMap &values,
    const QVariantList &workspaceRules,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || actionCatalog.digest() != expectedActionCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }
    if (workspaceRules.size() > Hyprland::maximumUserWorkspaceRules) {
        error = QStringLiteral(
            "The workspace rules collection exceeds its user item limit"
        );
        return std::nullopt;
    }

    const auto nextUserRules = strictJsonArray(workspaceRules);
    if (!nextUserRules) {
        error = QStringLiteral(
            "Workspace rules must contain only maps, lists, strings, booleans, and finite numbers"
        );
        return std::nullopt;
    }
    for (const auto &value : *nextUserRules) {
        if (!value.isObject() || isProtectedWorkspaceIdentity(value.toObject())) {
            error = QStringLiteral(
                "User workspace rules cannot use the protected HyprShelld identity"
            );
            return std::nullopt;
        }
    }

    const auto originalRules = snapshot.value(
        QStringLiteral("workspaceRules")
    ).toArray();
    if (originalRules.isEmpty()
        || !originalRules.last().isObject()
        || originalRules.last().toObject() != protectedWorkspaceRule()) {
        error = QStringLiteral(
            "The protected HyprShelld workspace rule is unavailable or not final"
        );
        return std::nullopt;
    }
    for (qsizetype index = 0; index < originalRules.size() - 1; ++index) {
        if (!originalRules.at(index).isObject()
            || isProtectedWorkspaceIdentity(originalRules.at(index).toObject())) {
            error = QStringLiteral(
                "The protected HyprShelld workspace rule is invalid"
            );
            return std::nullopt;
        }
    }

    auto candidateObject = snapshot;
    bool scalarChanged = false;
    if (!replaceGroupValues(
            OptionGroup::Workspaces,
            catalog,
            values,
            candidateObject,
            scalarChanged,
            error
        )) {
        return std::nullopt;
    }
    auto nextRules = *nextUserRules;
    nextRules.append(originalRules.last());
    candidateObject.insert(QStringLiteral("workspaceRules"), nextRules);

    auto originalUserRules = originalRules;
    originalUserRules.removeLast();
    return validatedCandidate(
        std::move(candidateObject),
        scalarChanged || *nextUserRules != originalUserRules,
        catalog,
        actionCatalog,
        error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceAdvanced(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantMap &values,
    QString &error
)
{
    return replaceGroup(
        OptionGroup::Advanced,
        snapshot,
        expectedRevision,
        expectedCatalogDigest,
        expectedActionCatalogDigest,
        catalog,
        actionCatalog,
        values,
        error
    );
}

std::optional<CompositorSnapshotEdit>
CompositorSnapshotEditor::replaceRules(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const CompositorActionCatalog &actionCatalog,
    const QVariantList &windowRules,
    const QVariantList &layerRules,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || actionCatalog.digest() != expectedActionCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }
    if (windowRules.size() > Hyprland::maximumWindowRules
        || layerRules.size() > Hyprland::maximumLayerRules) {
        error = QStringLiteral("The rules collection exceeds its item limit");
        return std::nullopt;
    }

    const auto nextWindowRules = strictJsonArray(windowRules);
    const auto nextLayerRules = strictJsonArray(layerRules);
    if (!nextWindowRules || !nextLayerRules) {
        error = QStringLiteral(
            "Rules must contain only maps, lists, strings, booleans, and finite numbers"
        );
        return std::nullopt;
    }
    const auto originalWindowRules = snapshot.value(
        QStringLiteral("windowRules")
    ).toArray();
    const auto originalLayerRules = snapshot.value(
        QStringLiteral("layerRules")
    ).toArray();
    auto candidateObject = snapshot;
    candidateObject.insert(QStringLiteral("windowRules"), *nextWindowRules);
    candidateObject.insert(QStringLiteral("layerRules"), *nextLayerRules);
    const auto changed = *nextWindowRules != originalWindowRules
        || *nextLayerRules != originalLayerRules;
    return validatedCandidate(
        std::move(candidateObject),
        changed,
        catalog,
        actionCatalog,
        error
    );
}

} // namespace HyprShelld
