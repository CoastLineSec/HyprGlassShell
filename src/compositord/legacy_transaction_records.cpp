#include "legacy_transaction_records.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QJsonObject>
#include <QSet>

#include <limits>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

constexpr char preSharedSpacingActionCatalogDigest[] =
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2";
constexpr char sharedSpacingActionCatalogDigest[] =
    "1438f04a169b4ecfc945078403d6286154bc89a0e32cb3a1a5073d209e0c358b";
constexpr char sharedSpacingConfigSchemaDigest[] =
    "75e299cc9f5d3a3289450df089cad4aa22efd26ba2f11fbbf27d46f78b898202";
constexpr char preDeviceQuarantineCatalogDigest[] =
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0";
constexpr char deviceQuarantineCatalogDigest[] =
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388";
constexpr char bindingsQuarantineCatalogDigest[] =
    "402c8a8c570dd3760d4d7bea8c358c7f12021a7c51457e62a4771d69a581254b";

struct CompatibleDesiredStateV1 final {
    Hyprland::DesiredState state;
    bool preSharedSpacingAuthority = false;
    bool preDeviceQuarantineAuthority = false;
    bool preBindingsQuarantineAuthority = false;
};

[[nodiscard]] QSet<QString> keysOf(const QJsonObject &object)
{
    QSet<QString> result;
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        result.insert(iterator.key());
    }
    return result;
}

[[nodiscard]] bool parseRevision(const QString &text, quint64 &revision)
{
    if (text.isEmpty() || (text.size() > 1 && text.front() == u'0')) {
        return false;
    }
    for (const auto character : text) {
        if (character < u'0' || character > u'9') {
            return false;
        }
    }
    bool converted = false;
    revision = text.toULongLong(&converted, 10);
    return converted;
}

[[nodiscard]] bool validSha256(const QStringView value)
{
    if (value.size() != 64) {
        return false;
    }
    for (const auto character : value) {
        if (!((character >= u'0' && character <= u'9')
              || (character >= u'a' && character <= u'f'))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] QString hashBytes(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool acceptsPreSharedSpacingAuthority(
    const Hyprland::ActionCatalog &actions
)
{
    return Hyprland::actionCatalogDigest(actions)
            == QLatin1String(sharedSpacingActionCatalogDigest)
        && actions.configSchemaDigest
            == QLatin1String(sharedSpacingConfigSchemaDigest);
}

[[nodiscard]] bool acceptsBindingsQuarantineAuthority(
    const Hyprland::Catalog &catalog
)
{
    return Hyprland::catalogDigest(catalog)
        == QLatin1String(bindingsQuarantineCatalogDigest);
}

[[nodiscard]] std::optional<CompatibleDesiredStateV1>
parseCompatibleDesiredStateV1(
    const QByteArrayView bytes,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
)
{
    const auto current = Hyprland::parseDesiredState(bytes, catalog, actions);
    if (current && Hyprland::serializeDesiredState(*current.value) == bytes) {
        return CompatibleDesiredStateV1{.state = *current.value};
    }

    const auto object = Hyprland::JsonSupport::parseStrictObject(
        bytes, Hyprland::maximumDesiredStateBytes, 64
    );
    if (!object) {
        return std::nullopt;
    }

    const auto storedCatalogDigest = object.value->value(
        QStringLiteral("catalogDigest")
    ).toString();
    const auto storedActionCatalogDigest = object.value->value(
        QStringLiteral("actionCatalogDigest")
    ).toString();
    const auto preDeviceQuarantineAuthority =
        acceptsBindingsQuarantineAuthority(catalog)
        && storedCatalogDigest
            == QLatin1String(preDeviceQuarantineCatalogDigest);
    const auto preBindingsQuarantineAuthority =
        acceptsBindingsQuarantineAuthority(catalog)
        && storedCatalogDigest
            == QLatin1String(deviceQuarantineCatalogDigest);
    const auto currentCatalogAuthority =
        storedCatalogDigest == Hyprland::catalogDigest(catalog);
    const auto preSharedSpacingAuthority =
        acceptsPreSharedSpacingAuthority(actions)
        && storedActionCatalogDigest
            == QLatin1String(preSharedSpacingActionCatalogDigest);
    const auto currentActionAuthority =
        storedActionCatalogDigest == Hyprland::actionCatalogDigest(actions);
    if ((!preDeviceQuarantineAuthority
         && !preBindingsQuarantineAuthority
         && !currentCatalogAuthority)
        || (!preSharedSpacingAuthority && !currentActionAuthority)
        || (!preDeviceQuarantineAuthority
            && !preBindingsQuarantineAuthority
            && !preSharedSpacingAuthority)) {
        return std::nullopt;
    }

    auto migratedObject = *object.value;
    migratedObject.insert(
        QStringLiteral("catalogDigest"), Hyprland::catalogDigest(catalog)
    );
    migratedObject.insert(
        QStringLiteral("actionCatalogDigest"),
        Hyprland::actionCatalogDigest(actions)
    );
    auto migratedBytes = Hyprland::JsonSupport::canonicalJson(migratedObject);
    migratedBytes.append('\n');
    const auto migrated = Hyprland::parseDesiredState(
        QByteArrayView(migratedBytes), catalog, actions
    );
    if (!migrated || migrated.value->readOnly) {
        return std::nullopt;
    }
    if (preSharedSpacingAuthority) {
        for (const auto &rule : migrated.value->workspaceRules) {
            if (rule.id
                    == QLatin1String(Hyprland::sharedSpacingWorkspaceRuleId)
                || rule.selector
                    == QLatin1String(
                        Hyprland::sharedSpacingWorkspaceRuleSelector
                    )) {
                return std::nullopt;
            }
        }
    }

    auto legacyState = *migrated.value;
    legacyState.catalogDigest = storedCatalogDigest;
    legacyState.actionCatalogDigest = storedActionCatalogDigest;
    if (Hyprland::serializeDesiredState(legacyState) != bytes) {
        return std::nullopt;
    }
    return CompatibleDesiredStateV1{
        .state = std::move(legacyState),
        .preSharedSpacingAuthority = preSharedSpacingAuthority,
        .preDeviceQuarantineAuthority = preDeviceQuarantineAuthority,
        .preBindingsQuarantineAuthority = preBindingsQuarantineAuthority,
    };
}

[[nodiscard]] std::optional<ActivationRequirement> requirementFromName(
    const QStringView name
)
{
    if (name == QStringLiteral("reload")) {
        return ActivationRequirement::Reload;
    }
    if (name == QStringLiteral("restart")) {
        return ActivationRequirement::Restart;
    }
    if (name == QStringLiteral("session")) {
        return ActivationRequirement::Session;
    }
    return std::nullopt;
}

[[nodiscard]] bool validAppliedRecord(const LegacyAppliedRecordV1 &record)
{
    const auto requirementName = activationRequirementName(
        record.requiredActivation
    );
    return record.snapshotDigest.size() == 64
        && record.generation.size() == 64
        && record.activationNonce.size() >= 32
        && record.activationNonce.size() <= 128
        && !record.entrypoint.isEmpty()
        && requirementFromName(requirementName).has_value();
}

[[nodiscard]] QJsonObject appliedObject(
    const LegacyAppliedRecordV1 &record
)
{
    return {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("revision"), QString::number(record.revision)},
        {QStringLiteral("snapshotDigest"), record.snapshotDigest},
        {QStringLiteral("generation"), record.generation},
        {QStringLiteral("activationNonce"), record.activationNonce},
        {QStringLiteral("entrypoint"), record.entrypoint},
        {
            QStringLiteral("requiredActivation"),
            activationRequirementName(record.requiredActivation),
        },
    };
}

[[nodiscard]] QByteArray appliedBytesUnchecked(
    const LegacyAppliedRecordV1 &record
)
{
    auto bytes = Hyprland::JsonSupport::canonicalJson(appliedObject(record));
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] std::optional<LegacyAppliedRecordV1> parseAppliedObject(
    const QJsonObject &object
)
{
    static const QSet<QString> expected{
        QStringLiteral("formatVersion"),
        QStringLiteral("revision"),
        QStringLiteral("snapshotDigest"),
        QStringLiteral("generation"),
        QStringLiteral("activationNonce"),
        QStringLiteral("entrypoint"),
        QStringLiteral("requiredActivation"),
    };
    quint64 revision = 0;
    const auto requirement = requirementFromName(
        object.value(QStringLiteral("requiredActivation")).toString()
    );
    const auto digest = object.value(
        QStringLiteral("snapshotDigest")
    ).toString();
    const auto generation = object.value(
        QStringLiteral("generation")
    ).toString();
    const auto nonce = object.value(
        QStringLiteral("activationNonce")
    ).toString();
    const auto entrypoint = object.value(
        QStringLiteral("entrypoint")
    ).toString();
    if (keysOf(object) != expected
        || object.value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || !parseRevision(
            object.value(QStringLiteral("revision")).toString(), revision
        )
        || digest.size() != 64 || generation.size() != 64
        || nonce.size() < 32 || nonce.size() > 128 || entrypoint.isEmpty()
        || !requirement) {
        return std::nullopt;
    }
    return LegacyAppliedRecordV1{
        .revision = revision,
        .snapshotDigest = digest,
        .generation = generation,
        .activationNonce = nonce,
        .entrypoint = entrypoint,
        .requiredActivation = *requirement,
    };
}

[[nodiscard]] QString kindName(const LegacyOrdinaryPendingKindV1 kind)
{
    switch (kind) {
    case LegacyOrdinaryPendingKindV1::Apply:
        return QStringLiteral("apply");
    case LegacyOrdinaryPendingKindV1::Recovery:
        return QStringLiteral("recovery");
    case LegacyOrdinaryPendingKindV1::DisplayPreview:
        return QStringLiteral("display-preview");
    }
    return {};
}

[[nodiscard]] std::optional<LegacyOrdinaryPendingKindV1> kindFromName(
    const QStringView name
)
{
    if (name == QStringLiteral("apply")) {
        return LegacyOrdinaryPendingKindV1::Apply;
    }
    if (name == QStringLiteral("recovery")) {
        return LegacyOrdinaryPendingKindV1::Recovery;
    }
    if (name == QStringLiteral("display-preview")) {
        return LegacyOrdinaryPendingKindV1::DisplayPreview;
    }
    return std::nullopt;
}

[[nodiscard]] QString phaseName(const LegacyOrdinaryPendingPhaseV1 phase)
{
    switch (phase) {
    case LegacyOrdinaryPendingPhaseV1::Prepared:
        return QStringLiteral("prepared");
    case LegacyOrdinaryPendingPhaseV1::Committing:
        return QStringLiteral("committing");
    }
    return {};
}

[[nodiscard]] std::optional<LegacyOrdinaryPendingPhaseV1> phaseFromName(
    const QStringView name
)
{
    if (name == QStringLiteral("prepared")) {
        return LegacyOrdinaryPendingPhaseV1::Prepared;
    }
    if (name == QStringLiteral("committing")) {
        return LegacyOrdinaryPendingPhaseV1::Committing;
    }
    return std::nullopt;
}

[[nodiscard]] QJsonObject snapshotObject(const QByteArrayView bytes)
{
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        bytes, Hyprland::maximumDesiredStateBytes, 64
    );
    return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] bool validPendingRecord(
    const LegacyOrdinaryPendingRecordV1 &record,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
)
{
    if (kindName(record.kind).isEmpty() || phaseName(record.phase).isEmpty()
        || !validSha256(record.beforeDesiredDigest)
        || !validAppliedRecord(record.afterActivation)
        || (record.beforeActivation
            && !validAppliedRecord(*record.beforeActivation))) {
        return false;
    }

    const auto candidate = parseCompatibleDesiredStateV1(
        QByteArrayView(record.candidateSnapshotBytes), catalog, actions
    );
    if (!candidate || candidate->state != record.candidateSnapshot
        || Hyprland::serializeDesiredState(record.candidateSnapshot)
            != record.candidateSnapshotBytes) {
        return false;
    }

    const auto computedSnapshotDigest = hashBytes(
        QByteArrayView(record.candidateSnapshotBytes)
    );
    if (record.afterActivation.revision
            != record.candidateSnapshot.revision
        || record.afterActivation.snapshotDigest != computedSnapshotDigest
        || record.snapshotDigest != record.afterActivation.snapshotDigest) {
        return false;
    }

    if (record.kind == LegacyOrdinaryPendingKindV1::Apply) {
        return record.candidateSnapshot.revision == record.expectedRevision
            && record.beforeDesiredDigest
                == record.afterActivation.snapshotDigest;
    }
    return record.expectedRevision != std::numeric_limits<quint64>::max()
        && record.candidateSnapshot.revision == record.expectedRevision + 1;
}

[[nodiscard]] QByteArray pendingBytesUnchecked(
    const LegacyOrdinaryPendingRecordV1 &record
)
{
    QJsonObject object{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("kind"), kindName(record.kind)},
        {QStringLiteral("phase"), phaseName(record.phase)},
        {
            QStringLiteral("expectedRevision"),
            QString::number(record.expectedRevision),
        },
        {
            QStringLiteral("beforeDesiredDigest"),
            record.beforeDesiredDigest,
        },
        {
            QStringLiteral("candidateSnapshot"),
            snapshotObject(record.candidateSnapshotBytes),
        },
        {QStringLiteral("snapshotDigest"), record.snapshotDigest},
        {
            QStringLiteral("afterActivation"),
            appliedObject(record.afterActivation),
        },
        {
            QStringLiteral("beforeActivation"),
            record.beforeActivation
                ? QJsonValue(appliedObject(*record.beforeActivation))
                : QJsonValue::Null,
        },
    };
    auto bytes = Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

} // namespace

std::optional<QByteArray> serializeLegacyAppliedRecordV1(
    const LegacyAppliedRecordV1 &record
)
{
    if (!validAppliedRecord(record)) {
        return std::nullopt;
    }
    auto bytes = appliedBytesUnchecked(record);
    if (bytes.size() > maximumLegacyAppliedRecordV1Bytes) {
        return std::nullopt;
    }
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        bytes,
        maximumLegacyAppliedRecordV1Bytes,
        maximumLegacyAppliedRecordV1Depth
    );
    if (!parsed || parseAppliedObject(*parsed.value) != record) {
        return std::nullopt;
    }
    return bytes;
}

std::optional<LegacyAppliedRecordV1> parseLegacyAppliedRecordV1(
    const QByteArrayView bytes
)
{
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        bytes,
        maximumLegacyAppliedRecordV1Bytes,
        maximumLegacyAppliedRecordV1Depth
    );
    if (!parsed) {
        return std::nullopt;
    }
    const auto record = parseAppliedObject(*parsed.value);
    if (!record || QByteArrayView(appliedBytesUnchecked(*record)) != bytes) {
        return std::nullopt;
    }
    return record;
}

std::optional<QByteArray> serializeLegacyOrdinaryPendingRecordV1(
    const LegacyOrdinaryPendingRecordV1 &record,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
)
{
    if (!validPendingRecord(record, catalog, actions)) {
        return std::nullopt;
    }
    auto bytes = pendingBytesUnchecked(record);
    if (bytes.size() > maximumLegacyOrdinaryPendingRecordV1Bytes) {
        return std::nullopt;
    }
    const auto parsed = parseLegacyOrdinaryPendingRecordV1(
        QByteArrayView(bytes), catalog, actions
    );
    if (!parsed || *parsed != record) {
        return std::nullopt;
    }
    return bytes;
}

std::optional<LegacyOrdinaryPendingRecordV1>
parseLegacyOrdinaryPendingRecordV1(
    const QByteArrayView bytes,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
)
{
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        bytes,
        maximumLegacyOrdinaryPendingRecordV1Bytes,
        maximumLegacyOrdinaryPendingRecordV1Depth
    );
    if (!parsed) {
        return std::nullopt;
    }
    const auto &object = *parsed.value;
    static const QSet<QString> expected{
        QStringLiteral("formatVersion"),
        QStringLiteral("kind"),
        QStringLiteral("phase"),
        QStringLiteral("expectedRevision"),
        QStringLiteral("beforeDesiredDigest"),
        QStringLiteral("candidateSnapshot"),
        QStringLiteral("snapshotDigest"),
        QStringLiteral("afterActivation"),
        QStringLiteral("beforeActivation"),
    };
    quint64 expectedRevision = 0;
    const auto beforeDesiredDigest = object.value(
        QStringLiteral("beforeDesiredDigest")
    ).toString();
    if (keysOf(object) != expected
        || object.value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || !parseRevision(
            object.value(QStringLiteral("expectedRevision")).toString(),
            expectedRevision
        )
        || !validSha256(beforeDesiredDigest)
        || !object.value(QStringLiteral("candidateSnapshot")).isObject()
        || !object.value(QStringLiteral("afterActivation")).isObject()) {
        return std::nullopt;
    }

    const auto kind = kindFromName(
        object.value(QStringLiteral("kind")).toString()
    );
    const auto phase = phaseFromName(
        object.value(QStringLiteral("phase")).toString()
    );
    if (!kind || !phase) {
        return std::nullopt;
    }

    auto candidateBytes = Hyprland::JsonSupport::canonicalJson(
        object.value(QStringLiteral("candidateSnapshot"))
    );
    candidateBytes.append('\n');
    const auto candidate = parseCompatibleDesiredStateV1(
        QByteArrayView(candidateBytes), catalog, actions
    );
    const auto after = parseAppliedObject(
        object.value(QStringLiteral("afterActivation")).toObject()
    );
    if (!candidate || !after
        || after->revision != candidate->state.revision
        || after->snapshotDigest != hashBytes(candidateBytes)
        || object.value(QStringLiteral("snapshotDigest")).toString()
            != after->snapshotDigest) {
        return std::nullopt;
    }

    std::optional<LegacyAppliedRecordV1> before;
    if (!object.value(QStringLiteral("beforeActivation")).isNull()) {
        if (!object.value(QStringLiteral("beforeActivation")).isObject()) {
            return std::nullopt;
        }
        before = parseAppliedObject(
            object.value(QStringLiteral("beforeActivation")).toObject()
        );
        if (!before) {
            return std::nullopt;
        }
    }

    if ((*kind == LegacyOrdinaryPendingKindV1::Apply
         && candidate->state.revision != expectedRevision)
        || ((*kind == LegacyOrdinaryPendingKindV1::Recovery
             || *kind == LegacyOrdinaryPendingKindV1::DisplayPreview)
            && (expectedRevision == std::numeric_limits<quint64>::max()
                || candidate->state.revision != expectedRevision + 1))) {
        return std::nullopt;
    }
    if (*kind == LegacyOrdinaryPendingKindV1::Apply
        && beforeDesiredDigest != after->snapshotDigest) {
        return std::nullopt;
    }

    LegacyOrdinaryPendingRecordV1 result{
        .kind = *kind,
        .phase = *phase,
        .expectedRevision = expectedRevision,
        .beforeDesiredDigest = beforeDesiredDigest,
        .candidateSnapshot = std::move(candidate->state),
        .candidateSnapshotBytes = std::move(candidateBytes),
        .snapshotDigest = after->snapshotDigest,
        .afterActivation = *after,
        .beforeActivation = std::move(before),
    };
    if (QByteArrayView(pendingBytesUnchecked(result)) != bytes) {
        return std::nullopt;
    }
    return result;
}

} // namespace HyprShelld::Compositor
