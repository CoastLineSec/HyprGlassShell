#include "reachable_v1_preflight.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonObject>
#include <QMap>
#include <QSet>

#include <array>
#include <limits>
#include <optional>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

constexpr qsizetype maximumReachableGenerationCountV1 = 2;

struct ParsedDocumentV1 final {
    QByteArray bytes;
    quint64 revision = 0;
    QString target;
    QString catalogDigest;
    QString actionCatalogDigest;
    quint32 patch = 0;
    DesiredMigrationDisposition migrationDisposition =
        DesiredMigrationDisposition::InvalidV1;
};

struct VerifiedGenerationEvidenceV1 final {
    const LegacyGenerationEvidenceV1 *source = nullptr;
    VerifiedDormantGenerationV1 verified;
    ParsedDocumentV1 document;
};

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool parseCanonicalRevision(
    const QString &text,
    quint64 &value
)
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
    value = text.toULongLong(&converted, 10);
    return converted;
}

[[nodiscard]] std::optional<ParsedDocumentV1> inspectDocument(
    const QByteArrayView bytes,
    const QByteArrayView migrationManifestBytes,
    const QByteArrayView sourceManifestV2Bytes,
    const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2
)
{
    const auto plan = inspectDesiredV1ToV2Migration(
        bytes,
        migrationManifestBytes,
        sourceManifestV2Bytes,
        catalogV1,
        actionCatalogV1,
        catalogV2,
        actionCatalogV2
    );
    if (plan.disposition() != DesiredMigrationDisposition::Eligible
        && plan.disposition()
            != DesiredMigrationDisposition::UnsupportedNewerPatch) {
        return std::nullopt;
    }
    const auto object = Hyprland::JsonSupport::parseStrictObject(
        bytes, Hyprland::maximumDesiredStateBytes, 64
    );
    quint64 revision = 0;
    if (!object
        || !parseCanonicalRevision(
            object.value->value(QStringLiteral("revision")).toString(),
            revision
        )) {
        return std::nullopt;
    }
    return ParsedDocumentV1{
        .bytes = QByteArray(bytes.data(), bytes.size()),
        .revision = revision,
        .target = object.value->value(
            QStringLiteral("targetHyprland")
        ).toString(),
        .catalogDigest = object.value->value(
            QStringLiteral("catalogDigest")
        ).toString(),
        .actionCatalogDigest = object.value->value(
            QStringLiteral("actionCatalogDigest")
        ).toString(),
        .patch = plan.sourcePatch(),
        .migrationDisposition = plan.disposition(),
    };
}

[[nodiscard]] ReachableV1PreflightResult failed(
    const ReachableV1PreflightReason reason
)
{
    return {
        .disposition = ReachableV1PreflightDisposition::RepairOnly,
        .reason = reason,
    };
}

[[nodiscard]] bool inconsistentRead(const LegacyReadV1 &read)
{
    switch (read.kind) {
    case LegacyReadKindV1::Missing:
        return !read.bytes.isEmpty();
    case LegacyReadKindV1::ExactRegular:
        return false;
    case LegacyReadKindV1::Unsafe:
        return true;
    }
    return true;
}

[[nodiscard]] bool present(const LegacyReadV1 &read)
{
    return read.kind == LegacyReadKindV1::ExactRegular;
}

[[nodiscard]] LegacyMirrorRelationV1 relation(
    const bool before,
    const bool after
)
{
    if (before && after) return LegacyMirrorRelationV1::Both;
    if (before) return LegacyMirrorRelationV1::Before;
    if (after) return LegacyMirrorRelationV1::After;
    return LegacyMirrorRelationV1::Unrelated;
}

[[nodiscard]] bool includesBefore(const LegacyMirrorRelationV1 value)
{
    return value == LegacyMirrorRelationV1::Before
        || value == LegacyMirrorRelationV1::Both;
}

[[nodiscard]] bool includesAfter(const LegacyMirrorRelationV1 value)
{
    return value == LegacyMirrorRelationV1::After
        || value == LegacyMirrorRelationV1::Both;
}

[[nodiscard]] QString fullGenerationEntrypoint(
    const VerifiedDormantGenerationV1 &generation
)
{
    return QDir(generation.generationRoot).filePath(generation.entrypoint);
}

[[nodiscard]] const GeneratedFile *generationEntrypoint(
    const VerifiedDormantGenerationV1 &generation
)
{
    const auto iterator = generation.files.constFind(
        QStringLiteral("hyprland.lua")
    );
    return iterator == generation.files.constEnd() ? nullptr
                                                    : &iterator.value();
}

[[nodiscard]] std::optional<QByteArray> exactOwnershipBytes(
    const std::optional<LegacyEntrypointOwnershipRecordV1> &ownership
)
{
    return ownership
        ? serializeLegacyEntrypointOwnershipRecordV1(*ownership)
        : std::optional<QByteArray>{QByteArray{}};
}

[[nodiscard]] bool readEqualsOptionalBytes(
    const LegacyReadV1 &read,
    const std::optional<LegacyEntrypointOwnershipRecordV1> &record
)
{
    const auto expected = exactOwnershipBytes(record);
    if (!expected) return false;
    if (!record) return read.kind == LegacyReadKindV1::Missing;
    return read.kind == LegacyReadKindV1::ExactRegular
        && read.bytes == *expected;
}

[[nodiscard]] std::optional<QByteArray> derivedTargetOwnershipBytes(
    const LegacyLiveActivationBridgeRecordV1 &bridge
)
{
    LegacyEntrypointOriginalRecordV1 original;
    if (bridge.beforeOwnership) {
        original = bridge.beforeOwnership->original;
    } else if (bridge.beforeKind
               == LegacyEntrypointFileKindV1::Regular) {
        original = {
            .kind = LegacyEntrypointFileKindV1::Regular,
            .digest = bridge.beforeDigest,
            .size = bridge.beforeSize,
            .mode = bridge.beforeMode,
            .device = bridge.beforeDevice,
            .inode = bridge.beforeInode,
            .backupName = bridge.swapName,
        };
    }
    return serializeLegacyEntrypointOwnershipRecordV1({
        .generation = bridge.targetGeneration,
        .activationNonce = bridge.targetNonce,
        .entrypointDigest = bridge.targetDigest,
        .entrypointSize = bridge.targetSize,
        .entrypointDevice = bridge.targetDevice,
        .entrypointInode = bridge.targetInode,
        .original = std::move(original),
    });
}

} // namespace

LegacyPendingMirrorClassificationV1 classifyLegacyPendingMirrorPredicatesV1(
    const LegacyOrdinaryPendingPhaseV1 phase,
    const MirrorRelationsV1 &mirrors
)
{
    const auto validRelation = [](const LegacyMirrorRelationV1 value) {
        return value == LegacyMirrorRelationV1::Before
            || value == LegacyMirrorRelationV1::After
            || value == LegacyMirrorRelationV1::Both
            || value == LegacyMirrorRelationV1::Unrelated;
    };
    if (!validRelation(mirrors.desired)
        || !validRelation(mirrors.lastGood)
        || !validRelation(mirrors.activation)
        || mirrors.desired == LegacyMirrorRelationV1::Unrelated
        || mirrors.lastGood == LegacyMirrorRelationV1::Unrelated
        || mirrors.activation == LegacyMirrorRelationV1::Unrelated) {
        return LegacyPendingMirrorClassificationV1::InvalidOrUnrelated;
    }
    switch (phase) {
    case LegacyOrdinaryPendingPhaseV1::Prepared:
        return includesBefore(mirrors.desired)
                && includesBefore(mirrors.lastGood)
                && includesBefore(mirrors.activation)
            ? LegacyPendingMirrorClassificationV1::CoherentPrepared
            : LegacyPendingMirrorClassificationV1::PreparedModified;
    case LegacyOrdinaryPendingPhaseV1::Committing:
        if ((includesAfter(mirrors.activation)
             && (!includesAfter(mirrors.desired)
                 || !includesAfter(mirrors.lastGood)))
            || (includesAfter(mirrors.lastGood)
                && !includesAfter(mirrors.desired))) {
            return LegacyPendingMirrorClassificationV1::
                CommittingWriteOrder;
        }
        return LegacyPendingMirrorClassificationV1::CoherentCommitting;
    }
    return LegacyPendingMirrorClassificationV1::InvalidOrUnrelated;
}

ReachableV1PreflightResult inspectReachableV1Preflight(
    const ReachableV1PreflightInput &input,
    const QByteArrayView migrationManifestBytes,
    const QByteArrayView sourceManifestV2Bytes,
    const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2
)
{
    ReachableV1PreflightResult result;
    const std::array reads{
        &input.desired,
        &input.lastGood,
        &input.applied,
        &input.pending,
        &input.ownership,
        &input.bridge,
    };
    for (const auto *read : reads) {
        if (inconsistentRead(*read)) {
            return failed(ReachableV1PreflightReason::UnsafeRead);
        }
    }

    if (!present(input.desired)) {
        const auto transactionResidue = present(input.lastGood)
            || present(input.applied) || present(input.pending);
        const auto otherResidue = present(input.ownership)
            || present(input.bridge)
            || !input.referencedGenerations.isEmpty();
        if (!transactionResidue && !otherResidue) {
            return {
                .disposition = ReachableV1PreflightDisposition::Absent,
                .reason = ReachableV1PreflightReason::None,
            };
        }
        return failed(
            transactionResidue
                ? ReachableV1PreflightReason::MissingDesired
                : ReachableV1PreflightReason::AbsentResidue
        );
    }

    QVector<ParsedDocumentV1> documents;
    documents.reserve(7);
    const auto desiredDocument = inspectDocument(
        input.desired.bytes,
        migrationManifestBytes,
        sourceManifestV2Bytes,
        catalogV1,
        actionCatalogV1,
        catalogV2,
        actionCatalogV2
    );
    if (!desiredDocument) {
        return failed(ReachableV1PreflightReason::InvalidDesired);
    }
    documents.append(*desiredDocument);
    result.sourcePatch = desiredDocument->patch;

    std::optional<ParsedDocumentV1> lastGoodDocument;
    if (present(input.lastGood)) {
        lastGoodDocument = inspectDocument(
            input.lastGood.bytes,
            migrationManifestBytes,
            sourceManifestV2Bytes,
            catalogV1,
            actionCatalogV1,
            catalogV2,
            actionCatalogV2
        );
        if (!lastGoodDocument) {
            return failed(ReachableV1PreflightReason::InvalidLastGood);
        }
        documents.append(*lastGoodDocument);
    }

    std::optional<LegacyAppliedRecordV1> applied;
    if (present(input.applied)) {
        applied = parseLegacyAppliedRecordV1(input.applied.bytes);
        if (!applied) {
            return failed(ReachableV1PreflightReason::InvalidApplied);
        }
    }

    std::optional<LegacyOrdinaryPendingRecordV1> pending;
    std::optional<ParsedDocumentV1> pendingCandidateDocument;
    if (present(input.pending)) {
        pending = parseLegacyOrdinaryPendingRecordV1(
            input.pending.bytes, catalogV1, actionCatalogV1
        );
        if (!pending) {
            return failed(ReachableV1PreflightReason::InvalidPending);
        }
        pendingCandidateDocument = inspectDocument(
            pending->candidateSnapshotBytes,
            migrationManifestBytes,
            sourceManifestV2Bytes,
            catalogV1,
            actionCatalogV1,
            catalogV2,
            actionCatalogV2
        );
        if (!pendingCandidateDocument) {
            return failed(ReachableV1PreflightReason::InvalidPending);
        }
        documents.append(*pendingCandidateDocument);
    }

    std::optional<LegacyEntrypointOwnershipRecordV1> ownership;
    if (present(input.ownership)) {
        ownership = parseLegacyEntrypointOwnershipRecordV1(
            input.ownership.bytes
        );
        if (!ownership) {
            return failed(ReachableV1PreflightReason::InvalidOwnership);
        }
    }

    std::optional<LegacyLiveActivationBridgeRecordV1> bridge;
    if (present(input.bridge)) {
        bridge = parseLegacyLiveActivationBridgeRecordV1(input.bridge.bytes);
        if (!bridge) {
            return failed(ReachableV1PreflightReason::InvalidBridge);
        }
    }

    if (input.referencedGenerations.size()
        > maximumReachableGenerationCountV1) {
        return failed(ReachableV1PreflightReason::ExtraGeneration);
    }

    QSet<QString> seenNonces;
    QSet<QString> seenGenerations;
    for (const auto &evidence : input.referencedGenerations) {
        if (seenNonces.contains(evidence.expected.activationNonce)
            || seenGenerations.contains(evidence.expected.generation)) {
            return failed(ReachableV1PreflightReason::DuplicateGeneration);
        }
        seenNonces.insert(evidence.expected.activationNonce);
        seenGenerations.insert(evidence.expected.generation);
    }

    QVector<const LegacyAppliedRecordV1 *> appliedReferences;
    if (applied) appliedReferences.append(&*applied);
    if (pending) {
        appliedReferences.append(&pending->afterActivation);
        if (pending->beforeActivation) {
            appliedReferences.append(&*pending->beforeActivation);
        }
    }
    QVector<QPair<QString, QString>> identityReferences;
    for (const auto *record : appliedReferences) {
        identityReferences.append({
            record->generation, record->activationNonce
        });
    }
    if (ownership) {
        identityReferences.append({
            ownership->generation, ownership->activationNonce
        });
    }
    if (bridge) {
        identityReferences.append({
            bridge->targetGeneration, bridge->targetNonce
        });
        if (!bridge->beforeGeneration.isEmpty()) {
            identityReferences.append({
                bridge->beforeGeneration, bridge->beforeNonce
            });
        }
    }
    for (qsizetype left = 0; left < identityReferences.size(); ++left) {
        for (qsizetype right = left + 1;
             right < identityReferences.size(); ++right) {
            const auto &a = identityReferences.at(left);
            const auto &b = identityReferences.at(right);
            if ((a.first == b.first && a.second != b.second)
                || (a.first != b.first && a.second == b.second)) {
                return failed(
                    ReachableV1PreflightReason::DuplicateGeneration
                );
            }
        }
    }
    for (const auto &identity : identityReferences) {
        for (const auto &evidence : input.referencedGenerations) {
            const auto &expected = evidence.expected;
            if ((identity.first == expected.generation
                 && identity.second != expected.activationNonce)
                || (identity.first != expected.generation
                    && identity.second == expected.activationNonce)) {
                return failed(
                    ReachableV1PreflightReason::DuplicateGeneration
                );
            }
        }
    }

    QSet<qsizetype> rawReferencedGenerationIndexes;
    auto findRawIdentity = [&](const QString &generation,
                               const QString &nonce)
        -> std::optional<qsizetype> {
        for (qsizetype index = 0;
             index < input.referencedGenerations.size(); ++index) {
            const auto &candidate = input.referencedGenerations.at(index);
            if (candidate.expected.generation == generation
                && candidate.expected.activationNonce == nonce) {
                rawReferencedGenerationIndexes.insert(index);
                return index;
            }
        }
        return std::nullopt;
    };
    for (const auto &identity : identityReferences) {
        if (!findRawIdentity(identity.first, identity.second)) {
            return failed(ReachableV1PreflightReason::MissingGeneration);
        }
    }
    for (const auto *record : appliedReferences) {
        bool matched = false;
        for (qsizetype index = 0;
             index < input.referencedGenerations.size(); ++index) {
            const auto &candidate = input.referencedGenerations.at(index);
            if (candidate.expected.generation == record->generation
                && candidate.expected.activationNonce
                    == record->activationNonce
                && candidate.expected.revision == record->revision
                && candidate.expected.snapshotDigest
                    == record->snapshotDigest
                && QDir(candidate.expected.generationRoot).filePath(
                       QStringLiteral("hyprland.lua")
                   ) == record->entrypoint) {
                rawReferencedGenerationIndexes.insert(index);
                matched = true;
                break;
            }
        }
        if (!matched) {
            return failed(ReachableV1PreflightReason::MissingGeneration);
        }
    }
    if (rawReferencedGenerationIndexes.size()
            != input.referencedGenerations.size()) {
        return failed(ReachableV1PreflightReason::ExtraGeneration);
    }

    QVector<VerifiedGenerationEvidenceV1> generations;
    generations.reserve(input.referencedGenerations.size());
    QMap<QString, qsizetype> generationByNonce;
    for (const auto &evidence : input.referencedGenerations) {
        auto verified = verifyDormantGenerationV1ForMigration(
            evidence.desiredBytes,
            evidence.manifestBytes,
            evidence.files,
            evidence.expected,
            catalogV1,
            actionCatalogV1
        );
        if (!verified) {
            return failed(ReachableV1PreflightReason::InvalidGeneration);
        }
        const auto document = inspectDocument(
            evidence.desiredBytes,
            migrationManifestBytes,
            sourceManifestV2Bytes,
            catalogV1,
            actionCatalogV1,
            catalogV2,
            actionCatalogV2
        );
        if (!document) {
            return failed(ReachableV1PreflightReason::InvalidGeneration);
        }
        const auto index = generations.size();
        generationByNonce.insert(evidence.expected.activationNonce, index);
        generations.append({
            .source = &evidence,
            .verified = std::move(*verified.value),
            .document = *document,
        });
        documents.append(*document);
    }

    auto findGeneration = [&](const QString &generation,
                              const QString &nonce)
        -> const VerifiedGenerationEvidenceV1 * {
        const auto iterator = generationByNonce.constFind(nonce);
        if (iterator == generationByNonce.constEnd()) return nullptr;
        const auto index = iterator.value();
        const auto &candidate = generations.at(index);
        if (candidate.verified.generation != generation) return nullptr;
        return &candidate;
    };

    auto bindApplied = [&](const LegacyAppliedRecordV1 &record)
        -> const VerifiedGenerationEvidenceV1 * {
        const auto *generation = findGeneration(
            record.generation, record.activationNonce
        );
        if (!generation) return nullptr;
        if (generation->source->expected.revision != record.revision
            || generation->verified.snapshotDigest != record.snapshotDigest
            || fullGenerationEntrypoint(generation->verified)
                != record.entrypoint) {
            return nullptr;
        }
        return generation;
    };

    QMap<const LegacyAppliedRecordV1 *, const VerifiedGenerationEvidenceV1 *>
        appliedGenerations;
    if (applied) {
        const auto *generation = bindApplied(*applied);
        if (!generation) {
            return failed(ReachableV1PreflightReason::InvalidGeneration);
        }
        appliedGenerations.insert(&*applied, generation);
    }
    if (pending) {
        const auto *afterGeneration = bindApplied(pending->afterActivation);
        if (!afterGeneration) {
            return failed(ReachableV1PreflightReason::InvalidGeneration);
        }
        appliedGenerations.insert(&pending->afterActivation, afterGeneration);
        if (pending->beforeActivation) {
            const auto *beforeGeneration = bindApplied(
                *pending->beforeActivation
            );
            if (!beforeGeneration) {
                return failed(ReachableV1PreflightReason::InvalidGeneration);
            }
            appliedGenerations.insert(
                &*pending->beforeActivation, beforeGeneration
            );
        }
    }

    const VerifiedGenerationEvidenceV1 *ownershipGeneration = nullptr;
    if (ownership) {
        ownershipGeneration = findGeneration(
            ownership->generation, ownership->activationNonce
        );
        if (!ownershipGeneration) {
            return failed(ReachableV1PreflightReason::InvalidGeneration);
        }
    }

    const VerifiedGenerationEvidenceV1 *bridgeTargetGeneration = nullptr;
    const VerifiedGenerationEvidenceV1 *bridgePriorGeneration = nullptr;
    if (bridge) {
        bridgeTargetGeneration = findGeneration(
            bridge->targetGeneration, bridge->targetNonce
        );
        if (!bridgeTargetGeneration) {
            return failed(ReachableV1PreflightReason::InvalidGeneration);
        }
        if (!bridge->beforeGeneration.isEmpty()) {
            bridgePriorGeneration = findGeneration(
                bridge->beforeGeneration, bridge->beforeNonce
            );
            if (!bridgePriorGeneration) {
                return failed(ReachableV1PreflightReason::InvalidGeneration);
            }
        }
    }

    if (pending) {
        if (pending->beforeActivation
            && (pending->beforeActivation->revision
                    > pending->expectedRevision
                || pending->beforeActivation->activationNonce
                    == pending->afterActivation.activationNonce)) {
            return failed(ReachableV1PreflightReason::PendingKindRelation);
        }
        switch (pending->kind) {
        case LegacyOrdinaryPendingKindV1::Apply:
            if (pending->candidateSnapshotBytes != input.desired.bytes
                || pending->candidateSnapshot.revision
                    != pending->expectedRevision) {
                return failed(
                    ReachableV1PreflightReason::PendingKindRelation
                );
            }
            break;
        case LegacyOrdinaryPendingKindV1::Recovery: {
            if (!pending->beforeActivation
                || pending->expectedRevision
                    == std::numeric_limits<quint64>::max()
                || pending->candidateSnapshot.revision
                    != pending->expectedRevision + 1) {
                return failed(
                    ReachableV1PreflightReason::PendingKindRelation
                );
            }
            auto projected = pending->candidateSnapshot;
            projected.revision = pending->beforeActivation->revision;
            const auto projectedBytes = Hyprland::serializeDesiredState(
                projected
            );
            const auto *beforeGeneration = appliedGenerations.value(
                &*pending->beforeActivation, nullptr
            );
            if (sha256(projectedBytes)
                    != pending->beforeActivation->snapshotDigest
                || !beforeGeneration
                || projectedBytes
                    != beforeGeneration->source->desiredBytes) {
                return failed(
                    ReachableV1PreflightReason::PendingKindRelation
                );
            }
            break;
        }
        case LegacyOrdinaryPendingKindV1::DisplayPreview:
            if (!pending->beforeActivation
                || pending->beforeActivation->revision
                    != pending->expectedRevision
                || pending->beforeActivation->snapshotDigest
                    != pending->beforeDesiredDigest) {
                return failed(
                    ReachableV1PreflightReason::PendingKindRelation
                );
            }
            break;
        }
    }

    std::optional<QByteArray> resolvedLastGoodBytes;
    std::optional<LegacyAppliedRecordV1> resolvedApplied;
    std::optional<QByteArray> resolvedAppliedBytes;
    QByteArray resolvedDesiredBytes = input.desired.bytes;
    ParsedDocumentV1 resolvedDesiredDocument = *desiredDocument;
    std::optional<ParsedDocumentV1> resolvedLastGoodDocument =
        lastGoodDocument;
    auto pendingResolution = PendingResolutionV1::None;

    if (pending) {
        const auto desiredDigest = sha256(input.desired.bytes);
        const auto desiredBefore = desiredDocument->revision
                == pending->expectedRevision
            && desiredDigest == pending->beforeDesiredDigest;
        const auto desiredAfter = desiredDocument->revision
                == pending->candidateSnapshot.revision
            && desiredDigest == pending->snapshotDigest;

        const auto lastGoodBefore = !lastGoodDocument
            ? !pending->beforeActivation.has_value()
            : pending->beforeActivation
                && lastGoodDocument->revision
                    == pending->beforeActivation->revision
                && sha256(lastGoodDocument->bytes)
                    == pending->beforeActivation->snapshotDigest;
        const auto lastGoodAfter = lastGoodDocument
            && lastGoodDocument->revision
                == pending->candidateSnapshot.revision
            && sha256(lastGoodDocument->bytes) == pending->snapshotDigest;

        const auto activationBefore = !applied
            ? !pending->beforeActivation.has_value()
            : pending->beforeActivation
                && *applied == *pending->beforeActivation;
        const auto activationAfter = applied
            && *applied == pending->afterActivation;

        result.mirrors = {
            .desired = relation(desiredBefore, desiredAfter),
            .lastGood = relation(lastGoodBefore, lastGoodAfter),
            .activation = relation(activationBefore, activationAfter),
        };
        const auto mirrorClassification =
            classifyLegacyPendingMirrorPredicatesV1(
                pending->phase, result.mirrors
            );
        if (mirrorClassification
            == LegacyPendingMirrorClassificationV1::InvalidOrUnrelated) {
            result.reason = ReachableV1PreflightReason::MirrorUnrelated;
            return result;
        }
        if (mirrorClassification
            == LegacyPendingMirrorClassificationV1::PreparedModified) {
            result.reason = ReachableV1PreflightReason::PreparedModified;
            return result;
        }
        if (mirrorClassification
            == LegacyPendingMirrorClassificationV1::
                CommittingWriteOrder) {
            result.reason =
                ReachableV1PreflightReason::CommittingWriteOrder;
            return result;
        }

        if (mirrorClassification
            == LegacyPendingMirrorClassificationV1::CoherentPrepared) {
            pendingResolution = PendingResolutionV1::RemovePrepared;
            if (lastGoodDocument) {
                resolvedLastGoodBytes = lastGoodDocument->bytes;
            }
            resolvedApplied = applied;
            if (applied) resolvedAppliedBytes = input.applied.bytes;
        } else {
            pendingResolution =
                PendingResolutionV1::RollForwardCommitting;
            resolvedDesiredBytes = pending->candidateSnapshotBytes;
            resolvedDesiredDocument = *pendingCandidateDocument;
            resolvedLastGoodBytes = pending->candidateSnapshotBytes;
            resolvedLastGoodDocument = pendingCandidateDocument;
            resolvedApplied = pending->afterActivation;
            resolvedAppliedBytes = serializeLegacyAppliedRecordV1(
                pending->afterActivation
            );
            if (!resolvedAppliedBytes) {
                result.reason = ReachableV1PreflightReason::InvalidPending;
                return result;
            }
        }
    } else {
        if (lastGoodDocument) {
            resolvedLastGoodBytes = lastGoodDocument->bytes;
        }
        resolvedApplied = applied;
        if (applied) resolvedAppliedBytes = input.applied.bytes;
    }

    if (resolvedLastGoodBytes.has_value()
        != resolvedApplied.has_value()) {
        result.reason = ReachableV1PreflightReason::LastGoodAppliedMismatch;
        return result;
    }
    if (resolvedApplied) {
        if (!resolvedLastGoodDocument
            || resolvedLastGoodDocument->revision
                != resolvedApplied->revision
            || sha256(resolvedLastGoodDocument->bytes)
                != resolvedApplied->snapshotDigest) {
            result.reason =
                ReachableV1PreflightReason::LastGoodAppliedMismatch;
            return result;
        }
        if (resolvedDesiredDocument.revision < resolvedApplied->revision) {
            result.reason =
                ReachableV1PreflightReason::DesiredRevisionRegression;
            return result;
        }
        if (resolvedDesiredDocument.revision == resolvedApplied->revision
            && resolvedDesiredBytes != *resolvedLastGoodBytes) {
            result.reason =
                ReachableV1PreflightReason::SameRevisionMismatch;
            return result;
        }
    }

    QMap<quint64, QByteArray> exactBytesByRevision;
    for (const auto &document : documents) {
        const auto existing = exactBytesByRevision.constFind(
            document.revision
        );
        if (existing != exactBytesByRevision.constEnd()
            && existing.value() != document.bytes) {
            result.reason =
                ReachableV1PreflightReason::SameRevisionMismatch;
            return result;
        }
        exactBytesByRevision.insert(document.revision, document.bytes);
    }

    const auto &baseline = documents.front();
    bool unsupportedTarget = false;
    for (const auto &document : documents) {
        if (document.target != baseline.target
            || document.patch != baseline.patch) {
            result.reason = ReachableV1PreflightReason::MixedTarget;
            return result;
        }
        if (document.catalogDigest != baseline.catalogDigest) {
            result.reason =
                ReachableV1PreflightReason::MixedCatalogLineage;
            return result;
        }
        if (document.actionCatalogDigest
            != baseline.actionCatalogDigest) {
            result.reason =
                ReachableV1PreflightReason::MixedActionLineage;
            return result;
        }
        unsupportedTarget = unsupportedTarget
            || document.migrationDisposition
                == DesiredMigrationDisposition::UnsupportedNewerPatch;
    }
    result.sourcePatch = baseline.patch;

    const auto ownershipEntrypointExact = [&] {
        if (!ownership || !ownershipGeneration) return !ownership;
        const auto *entrypoint = generationEntrypoint(
            ownershipGeneration->verified
        );
        return entrypoint
            && entrypoint->sha256 == ownership->entrypointDigest
            && entrypoint->size == ownership->entrypointSize;
    };
    const auto bridgeTargetEntrypointExact = [&] {
        if (!bridge || !bridgeTargetGeneration) return !bridge;
        const auto *entrypoint = generationEntrypoint(
            bridgeTargetGeneration->verified
        );
        return entrypoint && entrypoint->sha256 == bridge->targetDigest
            && entrypoint->size == bridge->targetSize;
    };
    const auto bridgePriorEntrypointExact = [&] {
        if (!bridge || bridge->beforeGeneration.isEmpty()) return true;
        if (!bridge->beforeOwnership || !bridgePriorGeneration) return false;
        const auto *entrypoint = generationEntrypoint(
            bridgePriorGeneration->verified
        );
        return entrypoint
            && entrypoint->sha256
                == bridge->beforeOwnership->entrypointDigest
            && entrypoint->size
                == bridge->beforeOwnership->entrypointSize;
    };

    if (pending && bridge) {
        if (bridge->targetGeneration
                != pending->afterActivation.generation
            || bridge->targetNonce
                != pending->afterActivation.activationNonce
            || bridge->adoption
                != !pending->beforeActivation.has_value()
            || (pending->phase
                    == LegacyOrdinaryPendingPhaseV1::Committing
                && bridge->phase
                    != LegacyLiveActivationBridgePhaseV1::Ready)) {
            result.reason =
                ReachableV1PreflightReason::PendingBridgeMismatch;
            return result;
        }
        if (pending->beforeActivation) {
            if (bridge->beforeGeneration
                    != pending->beforeActivation->generation
                || bridge->beforeNonce
                    != pending->beforeActivation->activationNonce
                || !bridge->beforeOwnership) {
                result.reason =
                    ReachableV1PreflightReason::PendingBridgeMismatch;
                return result;
            }
        } else if (!bridge->beforeGeneration.isEmpty()
                   || !bridge->beforeNonce.isEmpty()
                   || bridge->beforeOwnership) {
            result.reason =
                ReachableV1PreflightReason::PendingBridgeMismatch;
            return result;
        }
        if (!readEqualsOptionalBytes(
                input.ownership, bridge->beforeOwnership
            )) {
            result.reason = ReachableV1PreflightReason::OwnershipMismatch;
            return result;
        }
        if (!bridgeTargetEntrypointExact()
            || !bridgePriorEntrypointExact()) {
            result.reason = ReachableV1PreflightReason::BridgeMismatch;
            return result;
        }
        if (pending->phase
            == LegacyOrdinaryPendingPhaseV1::Prepared) {
            result.entrypoint.side = EntrypointSideV1::BridgePrior;
            result.entrypoint.kind = bridge->beforeKind;
            result.entrypoint.generation = bridge->beforeGeneration;
            result.entrypoint.activationNonce = bridge->beforeNonce;
            result.entrypoint.digest = bridge->beforeDigest;
            result.entrypoint.size = bridge->beforeSize;
            if (bridgePriorGeneration) {
                result.entrypoint.generationEntrypoint =
                    fullGenerationEntrypoint(
                        bridgePriorGeneration->verified
                    );
            }
        } else {
            result.entrypoint.side = EntrypointSideV1::BridgeTarget;
            result.entrypoint.kind =
                LegacyEntrypointFileKindV1::Regular;
            result.entrypoint.generation = bridge->targetGeneration;
            result.entrypoint.activationNonce = bridge->targetNonce;
            result.entrypoint.digest = bridge->targetDigest;
            result.entrypoint.size = bridge->targetSize;
            result.entrypoint.generationEntrypoint =
                fullGenerationEntrypoint(
                    bridgeTargetGeneration->verified
                );
        }
    } else if (pending
               && pending->phase
                   == LegacyOrdinaryPendingPhaseV1::Committing) {
        result.reason =
            ReachableV1PreflightReason::PendingBridgeMismatch;
        return result;
    } else if (!bridge) {
        if (resolvedApplied.has_value() != ownership.has_value()) {
            result.reason = ReachableV1PreflightReason::OwnershipMismatch;
            return result;
        }
        if (resolvedApplied) {
            if (!ownership
                || ownership->generation != resolvedApplied->generation
                || ownership->activationNonce
                    != resolvedApplied->activationNonce
                || !ownershipEntrypointExact()) {
                result.reason =
                    ReachableV1PreflightReason::OwnershipMismatch;
                return result;
            }
            result.entrypoint.side = EntrypointSideV1::Managed;
            result.entrypoint.kind =
                LegacyEntrypointFileKindV1::Regular;
            result.entrypoint.generation = ownership->generation;
            result.entrypoint.activationNonce =
                ownership->activationNonce;
            result.entrypoint.digest = ownership->entrypointDigest;
            result.entrypoint.size = ownership->entrypointSize;
            result.entrypoint.generationEntrypoint =
                fullGenerationEntrypoint(ownershipGeneration->verified);
        } else {
            result.entrypoint.side = EntrypointSideV1::Unmanaged;
        }
    } else {
        const auto priorOwnershipExact = readEqualsOptionalBytes(
            input.ownership, bridge->beforeOwnership
        );
        const auto targetOwnershipBytes = derivedTargetOwnershipBytes(*bridge);
        const auto targetOwnershipExact = targetOwnershipBytes
            && input.ownership.kind == LegacyReadKindV1::ExactRegular
            && input.ownership.bytes == *targetOwnershipBytes;
        if (!priorOwnershipExact && !targetOwnershipExact) {
            result.reason = ReachableV1PreflightReason::OwnershipMismatch;
            return result;
        }
        const auto targetSelected = resolvedApplied
            && resolvedApplied->generation == bridge->targetGeneration
            && resolvedApplied->activationNonce == bridge->targetNonce;
        const auto priorSelected = bridge->beforeGeneration.isEmpty()
            ? !resolvedApplied.has_value()
            : resolvedApplied
                && resolvedApplied->generation == bridge->beforeGeneration
                && resolvedApplied->activationNonce == bridge->beforeNonce;
        if (priorSelected && !targetSelected && !priorOwnershipExact) {
            result.reason = ReachableV1PreflightReason::OwnershipMismatch;
            return result;
        }
        if (targetSelected && priorSelected) {
            result.reason = ReachableV1PreflightReason::BridgeAmbiguous;
            return result;
        }
        if (!targetSelected && !priorSelected) {
            result.reason = ReachableV1PreflightReason::BridgeMismatch;
            return result;
        }
        if (!bridgeTargetEntrypointExact()
            || !bridgePriorEntrypointExact()) {
            result.reason = ReachableV1PreflightReason::BridgeMismatch;
            return result;
        }
        if (priorSelected) {
            if (!priorOwnershipExact) {
                result.reason =
                    ReachableV1PreflightReason::OwnershipMismatch;
                return result;
            }
            result.entrypoint.side = EntrypointSideV1::BridgePrior;
            result.entrypoint.kind = bridge->beforeKind;
            result.entrypoint.generation = bridge->beforeGeneration;
            result.entrypoint.activationNonce = bridge->beforeNonce;
            result.entrypoint.digest = bridge->beforeDigest;
            result.entrypoint.size = bridge->beforeSize;
            if (bridgePriorGeneration) {
                result.entrypoint.generationEntrypoint =
                    fullGenerationEntrypoint(
                        bridgePriorGeneration->verified
                    );
            }
        } else {
            if (bridge->phase
                != LegacyLiveActivationBridgePhaseV1::Ready) {
                result.reason = ReachableV1PreflightReason::BridgeMismatch;
                return result;
            }
            result.entrypoint.side = EntrypointSideV1::BridgeTarget;
            result.entrypoint.kind =
                LegacyEntrypointFileKindV1::Regular;
            result.entrypoint.generation = bridge->targetGeneration;
            result.entrypoint.activationNonce = bridge->targetNonce;
            result.entrypoint.digest = bridge->targetDigest;
            result.entrypoint.size = bridge->targetSize;
            result.entrypoint.generationEntrypoint =
                fullGenerationEntrypoint(
                    bridgeTargetGeneration->verified
                );
        }
    }

    if (unsupportedTarget) {
        result.disposition =
            ReachableV1PreflightDisposition::UnsupportedTarget;
        result.reason = ReachableV1PreflightReason::UnsupportedTarget;
        result.pendingResolution = PendingResolutionV1::None;
        result.entrypoint = {};
        result.resolved.reset();
        return result;
    }
    result.pendingResolution = pendingResolution;
    result.resolved = ResolvedReachableV1{
        .desiredBytes = std::move(resolvedDesiredBytes),
        .lastGoodBytes = std::move(resolvedLastGoodBytes),
        .applied = std::move(resolvedApplied),
        .appliedBytes = std::move(resolvedAppliedBytes),
    };
    result.disposition = ReachableV1PreflightDisposition::
        ByteCoherentNeedsEntrypointQualification;
    result.reason =
        ReachableV1PreflightReason::EntrypointQualificationRequired;
    return result;
}

} // namespace HyprShelld::Compositor
