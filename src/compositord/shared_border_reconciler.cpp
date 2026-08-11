#include "shared_border_reconciler.h"

#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace HyprShelld::Compositor {
namespace {

const QString borderSizeId = QStringLiteral("hyprland.general.border_size");
const QString roundingId = QStringLiteral("hyprland.decoration.rounding");

std::optional<quint32> unsignedInteger(
    const QJsonValue &value,
    const quint32 maximum
)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0
        || number > static_cast<double>(maximum)
        || std::floor(number) != number) {
        return std::nullopt;
    }
    return static_cast<quint32>(number);
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
        || !document.object().value(QStringLiteral("overrides")).isObject()) {
        error = QStringLiteral("The compositor snapshot is invalid");
        return std::nullopt;
    }
    return document.object();
}

std::optional<SharedBorderValues> valuesFromObject(
    const QJsonObject &snapshot,
    const quint32 borderDefault,
    const quint32 roundingDefault,
    QString &error
)
{
    const auto overrides = snapshot.value(
        QStringLiteral("overrides")
    ).toObject();
    const auto border = overrides.contains(borderSizeId)
        ? unsignedInteger(overrides.value(borderSizeId), 20U)
        : std::optional<quint32>(borderDefault);
    const auto rounding = overrides.contains(roundingId)
        ? unsignedInteger(overrides.value(roundingId), 20U)
        : std::optional<quint32>(roundingDefault);
    if (!border || !rounding) {
        error = QStringLiteral("The shared compositor border values are invalid");
        return std::nullopt;
    }
    return SharedBorderValues{
        .borderSize = *border,
        .rounding = *rounding,
    };
}

} // namespace

bool SharedBorderReconciler::configure(
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
    const Hyprland::OptionDefinition *border = nullptr;
    const Hyprland::OptionDefinition *rounding = nullptr;
    for (const auto &option : parsed.value->options) {
        if (option.id == borderSizeId) {
            border = &option;
        } else if (option.id == roundingId) {
            rounding = &option;
        }
    }
    const auto borderDefault = border
        ? unsignedInteger(border->defaultValue, 20U) : std::nullopt;
    const auto roundingDefault = rounding
        ? unsignedInteger(rounding->defaultValue, 20U) : std::nullopt;
    if (!border || !rounding || !borderDefault || !roundingDefault
        || !border->writable || !rounding->writable
        || border->type != Hyprland::OptionType::Integer
        || rounding->type != Hyprland::OptionType::Integer
        || border->applyMode != Hyprland::ApplyMode::Reload
        || rounding->applyMode != Hyprland::ApplyMode::Reload) {
        error = QStringLiteral("The shared compositor border contract is invalid");
        return false;
    }
    catalogDigest_ = expectedDigest;
    borderDefault_ = *borderDefault;
    roundingDefault_ = *roundingDefault;
    return true;
}

bool SharedBorderReconciler::configuredFor(const QString &digest) const
{
    return !catalogDigest_.isEmpty() && catalogDigest_ == digest;
}

SharedBorderValues SharedBorderReconciler::valuesFor(
    const SharedBorderProjection &projection
) const
{
    return {
        .borderSize = projection.borderEnabled
            ? projection.borderWidth : 0U,
        .rounding = projection.borderRadius,
    };
}

std::optional<SharedBorderValues> SharedBorderReconciler::resolvedValues(
    const QByteArray &snapshot,
    QString &error
) const
{
    error.clear();
    if (catalogDigest_.isEmpty()) {
        error = QStringLiteral("The shared compositor border contract is unavailable");
        return std::nullopt;
    }
    const auto object = snapshotObject(snapshot, error);
    return object
        ? valuesFromObject(*object, borderDefault_, roundingDefault_, error)
        : std::nullopt;
}

std::optional<SharedBorderEdit> SharedBorderReconciler::edit(
    const QByteArray &snapshot,
    const quint64 expectedRevision,
    const QString &expectedCatalogDigest,
    const SharedBorderProjection &projection,
    QString &error
) const
{
    error.clear();
    if (!configuredFor(expectedCatalogDigest)) {
        error = QStringLiteral("The shared compositor border contract is stale");
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
    const auto original = overrides;
    const auto values = valuesFor(projection);
    const auto setValue = [&overrides](
        const QString &id,
        const quint32 value,
        const quint32 defaultValue
    ) {
        if (value == defaultValue) {
            overrides.remove(id);
        } else {
            overrides.insert(id, static_cast<qint64>(value));
        }
    };
    setValue(borderSizeId, values.borderSize, borderDefault_);
    setValue(roundingId, values.rounding, roundingDefault_);
    object->insert(QStringLiteral("overrides"), overrides);

    auto candidate = Hyprland::JsonSupport::canonicalJson(*object);
    candidate.append('\n');
    if (candidate.size() > Hyprland::maximumDesiredStateBytes) {
        error = QStringLiteral("The compositor snapshot exceeds its size limit");
        return std::nullopt;
    }
    return SharedBorderEdit{
        .candidate = std::move(candidate),
        .changed = overrides != original,
    };
}

bool SharedBorderReconciler::replacementPreserves(
    const QByteArray &candidate,
    const SharedBorderValues &values,
    QString &error
) const
{
    const auto candidateValues = resolvedValues(candidate, error);
    return candidateValues && *candidateValues == values;
}

} // namespace HyprShelld::Compositor
