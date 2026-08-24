#include "shared_spacing_reconciler.h"

#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace HyprShelld::Compositor {
namespace {

const QString gapsInId = QStringLiteral("hyprland.general.gaps_in");
const QString gapsOutId = QStringLiteral("hyprland.general.gaps_out");
constexpr qint64 maximumSafeInteger = 9007199254740991LL;

std::optional<qint64> safeInteger(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < -static_cast<double>(maximumSafeInteger)
        || number > static_cast<double>(maximumSafeInteger)) {
        return std::nullopt;
    }
    return static_cast<qint64>(number);
}

std::optional<SharedGap> gap(const QJsonValue &value)
{
    if (!value.isArray()) {
        return std::nullopt;
    }
    const auto array = value.toArray();
    if (array.size() != 4) {
        return std::nullopt;
    }
    SharedGap result{};
    for (qsizetype index = 0; index < array.size(); ++index) {
        const auto item = safeInteger(array.at(index));
        if (!item) {
            return std::nullopt;
        }
        result.at(static_cast<std::size_t>(index)) = *item;
    }
    return result;
}

QJsonArray gapArray(const SharedGap &value)
{
    return {
        static_cast<double>(value.at(0)),
        static_cast<double>(value.at(1)),
        static_cast<double>(value.at(2)),
        static_cast<double>(value.at(3)),
    };
}

std::optional<QJsonObject> snapshotObject(
    const QByteArray &snapshot,
    QString &error
)
{
    if (snapshot.isEmpty()
        || snapshot.size() > Hyprland::maximumDesiredStateBytes) {
        error = QStringLiteral("The compositor snapshot has an invalid size");
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(snapshot, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()
        || !document.object().value(QStringLiteral("overrides")).isObject()
        || !document.object().value(
            QStringLiteral("workspaceRules")
        ).isArray()) {
        error = QStringLiteral("The compositor snapshot is invalid");
        return std::nullopt;
    }
    return document.object();
}

QJsonObject protectedRule()
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
            QJsonObject{
                {QStringLiteral("gaps_out"), QJsonArray{0, 0, 0, 0}},
            },
        },
    };
}

bool isProtectedIdentity(const QJsonObject &record)
{
    return record.value(QStringLiteral("id")).toString()
            == QLatin1String(Hyprland::sharedSpacingWorkspaceRuleId)
        || record.value(QStringLiteral("selector")).toString()
            == QLatin1String(
                Hyprland::sharedSpacingWorkspaceRuleSelector
            );
}

std::optional<SharedSpacingValues> valuesFromObject(
    const QJsonObject &snapshot,
    const SharedGap &gapsInDefault,
    const SharedGap &gapsOutDefault,
    QString &error
)
{
    const auto overrides = snapshot.value(
        QStringLiteral("overrides")
    ).toObject();
    SharedSpacingValues result{
        .gapsIn = gapsInDefault,
        .gapsOut = gapsOutDefault,
    };
    if (overrides.contains(gapsInId)) {
        const auto value = gap(overrides.value(gapsInId));
        if (!value) {
            error = QStringLiteral(
                "The shared compositor inner spacing is invalid"
            );
            return std::nullopt;
        }
        result.gapsIn = *value;
    }
    if (overrides.contains(gapsOutId)) {
        const auto value = gap(overrides.value(gapsOutId));
        if (!value) {
            error = QStringLiteral(
                "The shared compositor outer spacing is invalid"
            );
            return std::nullopt;
        }
        result.gapsOut = *value;
    }
    return result;
}

} // namespace

bool SharedSpacingReconciler::configure(
    const QByteArray &catalogBytes,
    const QString &expectedDigest,
    QString &error
)
{
    error.clear();
    const auto parsed = Hyprland::parseCatalog(catalogBytes);
    if (!parsed || parsed.value->digest != expectedDigest) {
        error = QStringLiteral("The compositor option catalog is unavailable");
        return false;
    }
    const Hyprland::OptionDefinition *gapsIn = nullptr;
    const Hyprland::OptionDefinition *gapsOut = nullptr;
    for (const auto &option : parsed.value->options) {
        if (option.id == gapsInId) {
            gapsIn = &option;
        } else if (option.id == gapsOutId) {
            gapsOut = &option;
        }
    }
    const auto valid = [](const Hyprland::OptionDefinition *option) {
        return option && option->writable
            && option->type == Hyprland::OptionType::CssGap
            && option->applyMode == Hyprland::ApplyMode::Reload
            && gap(option->defaultValue).has_value();
    };
    if (!valid(gapsIn) || !valid(gapsOut)) {
        error = QStringLiteral(
            "The shared compositor spacing contract is invalid"
        );
        return false;
    }
    catalogDigest_ = expectedDigest;
    gapsInDefault_ = *gap(gapsIn->defaultValue);
    gapsOutDefault_ = *gap(gapsOut->defaultValue);
    return true;
}

bool SharedSpacingReconciler::configuredFor(const QString &digest) const
{
    return !catalogDigest_.isEmpty() && catalogDigest_ == digest;
}

SharedSpacingValues SharedSpacingReconciler::valuesFor(
    const SharedVisualProjection &projection
) const
{
    const auto inner = static_cast<qint64>(projection.innerSpacing);
    const auto outer = static_cast<qint64>(projection.outerSpacing);
    return {
        .gapsIn = {inner, inner, inner, inner},
        .gapsOut = {0, outer, outer, outer},
    };
}

std::optional<SharedSpacingValues> SharedSpacingReconciler::resolvedValues(
    const QByteArray &snapshot,
    QString &error
) const
{
    error.clear();
    if (catalogDigest_.isEmpty()) {
        error = QStringLiteral(
            "The shared compositor spacing contract is unavailable"
        );
        return std::nullopt;
    }
    const auto object = snapshotObject(snapshot, error);
    return object
        ? valuesFromObject(*object, gapsInDefault_, gapsOutDefault_, error)
        : std::nullopt;
}

std::optional<SharedSpacingEdit> SharedSpacingReconciler::edit(
    const QByteArray &snapshot,
    const quint64 expectedRevision,
    const QString &expectedCatalogDigest,
    const SharedVisualProjection &projection,
    QString &error
) const
{
    error.clear();
    if (projection.syncWindowSpacing
        && !configuredFor(expectedCatalogDigest)) {
        error = QStringLiteral(
            "The shared compositor spacing contract is stale"
        );
        return std::nullopt;
    }
    auto object = snapshotObject(snapshot, error);
    if (!object
        || object->value(QStringLiteral("revision")).toString()
            != QString::number(expectedRevision)
        || object->value(QStringLiteral("catalogDigest")).toString()
            != expectedCatalogDigest) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }

    auto overrides = object->value(QStringLiteral("overrides")).toObject();
    const auto originalOverrides = overrides;
    if (projection.syncWindowSpacing) {
        const auto values = valuesFor(projection);
        const auto setGap = [&overrides](
            const QString &id,
            const SharedGap &value,
            const SharedGap &defaultValue
        ) {
            if (value == defaultValue) {
                overrides.remove(id);
            } else {
                overrides.insert(id, gapArray(value));
            }
        };
        setGap(gapsInId, values.gapsIn, gapsInDefault_);
        setGap(gapsOutId, values.gapsOut, gapsOutDefault_);
        object->insert(QStringLiteral("overrides"), overrides);
    }

    const auto originalRules = object->value(
        QStringLiteral("workspaceRules")
    ).toArray();
    QJsonArray rules;
    bool protectedRuleSeen = false;
    for (const auto &value : originalRules) {
        if (!value.isObject()) {
            error = QStringLiteral("The compositor workspace rules are invalid");
            return std::nullopt;
        }
        const auto record = value.toObject();
        if (isProtectedIdentity(record)) {
            if (record != protectedRule()) {
                error = QStringLiteral(
                    "The protected maximized-workspace rule is invalid"
                );
                return std::nullopt;
            }
            if (protectedRuleSeen) {
                error = QStringLiteral(
                    "The protected maximized-workspace rule is duplicated"
                );
                return std::nullopt;
            }
            protectedRuleSeen = true;
            continue;
        }
        rules.append(record);
    }
    if (rules.size() > Hyprland::maximumUserWorkspaceRules) {
        error = QStringLiteral("Too many user workspace rules are configured");
        return std::nullopt;
    }
    rules.append(protectedRule());
    object->insert(QStringLiteral("workspaceRules"), rules);

    auto candidate = Hyprland::JsonSupport::canonicalJson(*object);
    candidate.append('\n');
    if (candidate.size() > Hyprland::maximumDesiredStateBytes) {
        error = QStringLiteral("The compositor snapshot exceeds its size limit");
        return std::nullopt;
    }
    return SharedSpacingEdit{
        .candidate = std::move(candidate),
        .changed = overrides != originalOverrides || rules != originalRules,
        .spacingChanged = overrides != originalOverrides,
        .protectedRuleChanged = rules != originalRules,
    };
}

bool SharedSpacingReconciler::replacementPreservesSpacing(
    const QByteArray &candidate,
    const SharedSpacingValues &values,
    QString &error
) const
{
    const auto candidateValues = resolvedValues(candidate, error);
    return candidateValues && *candidateValues == values;
}

bool SharedSpacingReconciler::hasExactFinalProtectedRule(
    const QByteArray &candidate,
    QString &error
) const
{
    error.clear();
    const auto object = snapshotObject(candidate, error);
    if (!object) {
        return false;
    }
    const auto rules = object->value(QStringLiteral("workspaceRules")).toArray();
    qsizetype protectedCount = 0;
    qsizetype protectedIndex = -1;
    for (qsizetype index = 0; index < rules.size(); ++index) {
        if (!rules.at(index).isObject()) {
            error = QStringLiteral("The compositor workspace rules are invalid");
            return false;
        }
        const auto record = rules.at(index).toObject();
        if (!isProtectedIdentity(record)) {
            continue;
        }
        if (record != protectedRule()) {
            error = QStringLiteral(
                "The protected maximized-workspace rule is invalid"
            );
            return false;
        }
        ++protectedCount;
        protectedIndex = index;
    }
    if (protectedCount != 1 || protectedIndex != rules.size() - 1) {
        error = QStringLiteral(
            "The protected maximized-workspace rule must be unique and final"
        );
        return false;
    }
    return true;
}

} // namespace HyprShelld::Compositor
