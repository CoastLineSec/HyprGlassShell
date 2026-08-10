#include "compositor_snapshot_editor.h"

#include "compositor_option_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

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

std::optional<AppearanceSnapshotEdit>
CompositorSnapshotEditor::replaceAppearance(
    const QJsonObject &snapshot,
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const CompositorOptionCatalog &catalog,
    const QVariantMap &values,
    QString &error
)
{
    error.clear();
    if (expectedRevision == std::numeric_limits<qulonglong>::max()
        || catalog.digest() != expectedCatalogDigest
        || !snapshotMatchesAuthority(
            snapshot,
            expectedRevision,
            expectedCatalogDigest,
            expectedActionCatalogDigest
        )) {
        error = QStringLiteral("The compositor snapshot authority is stale");
        return std::nullopt;
    }

    const auto ids = catalog.appearanceOptionIds();
    if (values.size() != ids.size()
        || QSet<QString>(values.keyBegin(), values.keyEnd())
            != QSet<QString>(ids.cbegin(), ids.cend())) {
        error = QStringLiteral("Exactly the supported appearance values are required");
        return std::nullopt;
    }

    auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
    const auto originalOverrides = overrides;
    for (const auto &id : ids) {
        const auto *option = catalog.appearanceOption(id);
        const auto value = QJsonValue::fromVariant(values.value(id));
        if (option == nullptr || value.isUndefined() || value.isNull()
            || !Hyprland::validateOptionValue(*option, value).isEmpty()) {
            error = QStringLiteral("An appearance value is invalid");
            return std::nullopt;
        }
        if (value == option->defaultValue) {
            overrides.remove(id);
        } else {
            overrides.insert(id, value);
        }
    }

    auto candidateObject = snapshot;
    candidateObject.insert(QStringLiteral("overrides"), overrides);
    auto candidate = Hyprland::JsonSupport::canonicalJson(candidateObject);
    candidate.append('\n');
    if (candidate.size() > Hyprland::maximumDesiredStateBytes) {
        error = QStringLiteral("The compositor snapshot exceeds its size limit");
        return std::nullopt;
    }
    return AppearanceSnapshotEdit{
        .candidate = std::move(candidate),
        .changed = overrides != originalOverrides,
    };
}

} // namespace HyprShelld
