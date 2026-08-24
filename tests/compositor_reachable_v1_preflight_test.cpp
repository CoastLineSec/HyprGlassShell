#include "compositord/reachable_v1_preflight.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <array>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";
constexpr auto nonceB = "fedcba9876543210fedcba9876543210";
constexpr auto token = "11111111111111111111111111111111";

struct Artifact final {
    LegacyGenerationEvidenceV1 evidence;
    DesiredState state;
};

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

[[nodiscard]] QJsonObject readObject(const QString &path)
{
    const auto document = QJsonDocument::fromJson(readBytes(path));
    return document.isObject() ? document.object() : QJsonObject{};
}

[[nodiscard]] QByteArray canonicalObject(const QJsonObject &object)
{
    auto bytes = JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QJsonObject strictObject(const QByteArrayView bytes)
{
    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumDesiredStateBytes, 64
    );
    return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

void rebindGeneration(QJsonObject &manifest)
{
    manifest.remove(QStringLiteral("generation"));
    manifest.insert(
        QStringLiteral("generation"),
        sha256(JsonSupport::canonicalJson(manifest))
    );
}

[[nodiscard]] QDateTime fixedTime()
{
    return QDateTime::fromString(
        QStringLiteral("2026-08-09T12:34:56.789Z"),
        Qt::ISODateWithMs
    );
}

[[nodiscard]] LegacyReadV1 exact(QByteArray bytes)
{
    return {
        .kind = LegacyReadKindV1::ExactRegular,
        .bytes = std::move(bytes),
    };
}

} // namespace

class CompositorReachableV1PreflightTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalogV1_;
    ActionCatalog actionsV1_;
    Catalog catalogV2_;
    ActionCatalog actionsV2_;
    QJsonObject defaults_;
    QByteArray migrationManifest_;
    QByteArray sourceManifestV2_;

    [[nodiscard]] QByteArray desiredBytes(
        const quint64 revision,
        const QString &target = QStringLiteral("0.56.1"),
        const QString &catalogDigest =
            QString::fromLatin1(reviewedCatalogDigest),
        const QString &actionDigest =
            QString::fromLatin1(reviewedActionCatalogDigest),
        const bool animationsDisabled = false,
        const bool protectedRule = true
    ) const
    {
        auto root = defaults_;
        root.insert(
            QStringLiteral("revision"), QString::number(revision)
        );
        root.insert(QStringLiteral("targetHyprland"), target);
        root.insert(QStringLiteral("catalogDigest"), catalogDigest);
        root.insert(QStringLiteral("actionCatalogDigest"), actionDigest);
        if (animationsDisabled) {
            root.insert(
                QStringLiteral("overrides"),
                QJsonObject{{
                    QStringLiteral("hyprland.animations.enabled"), false
                }}
            );
        }
        if (!protectedRule) {
            root.insert(QStringLiteral("workspaceRules"), QJsonArray{});
        }
        return canonicalObject(root);
    }

    [[nodiscard]] Artifact artifact(
        const QByteArray &desired,
        const QString &nonce
    ) const
    {
        Artifact result;
        auto normalized = strictObject(desired);
        const auto sourceCatalog = normalized.value(
            QStringLiteral("catalogDigest")
        ).toString();
        const auto sourceActions = normalized.value(
            QStringLiteral("actionCatalogDigest")
        ).toString();
        normalized.insert(
            QStringLiteral("catalogDigest"),
            QLatin1String(reviewedCatalogDigest)
        );
        normalized.insert(
            QStringLiteral("actionCatalogDigest"),
            QLatin1String(reviewedActionCatalogDigest)
        );
        const auto parsed = parseDesiredState(
            canonicalObject(normalized), catalogV1_, actionsV1_
        );
        if (!parsed) return result;
        result.state = *parsed.value;
        result.state.catalogDigest = sourceCatalog;
        result.state.actionCatalogDigest = sourceActions;

        auto renderState = *parsed.value;
        auto &expected = result.evidence.expected;
        expected.revision = renderState.revision;
        expected.snapshotDigest = sha256(desired);
        expected.activationNonce = nonce;
        expected.generationRoot = QStringLiteral(
            "/tmp/hyprshelld-reachable-v1/"
        ) + nonce;
        expected.userCustomPath = QStringLiteral(
            "/tmp/hyprshelld-reachable-v1/user-custom.lua"
        );
        const auto rendered = renderGeneration(
            renderState,
            catalogV1_,
            actionsV1_,
            expected.generationRoot,
            expected.userCustomPath,
            nonce,
            fixedTime()
        );
        if (!rendered) return result;
        for (auto iterator = rendered.value->files.constBegin();
             iterator != rendered.value->files.constEnd(); ++iterator) {
            result.evidence.files.insert(
                iterator.key(), iterator->contents
            );
        }
        auto manifest = rendered.value->manifest;
        manifest.insert(
            QStringLiteral("snapshotDigest"), expected.snapshotDigest
        );
        manifest.insert(QStringLiteral("catalogDigest"), sourceCatalog);
        manifest.insert(
            QStringLiteral("actionCatalogDigest"), sourceActions
        );
        rebindGeneration(manifest);
        expected.generation = manifest.value(
            QStringLiteral("generation")
        ).toString();
        result.evidence.desiredBytes = desired;
        result.evidence.manifestBytes = canonicalObject(manifest);
        return result;
    }

    [[nodiscard]] LegacyAppliedRecordV1 applied(
        const Artifact &artifact
    ) const
    {
        return {
            .revision = artifact.evidence.expected.revision,
            .snapshotDigest = artifact.evidence.expected.snapshotDigest,
            .generation = artifact.evidence.expected.generation,
            .activationNonce =
                artifact.evidence.expected.activationNonce,
            .entrypoint = QDir(
                artifact.evidence.expected.generationRoot
            ).filePath(QStringLiteral("hyprland.lua")),
            .requiredActivation = ActivationRequirement::Reload,
        };
    }

    [[nodiscard]] LegacyEntrypointOwnershipRecordV1 ownership(
        const Artifact &artifact,
        const quint64 device = 101,
        const quint64 inode = 102
    ) const
    {
        const auto bytes = artifact.evidence.files.value(
            QStringLiteral("hyprland.lua")
        );
        return {
            .generation = artifact.evidence.expected.generation,
            .activationNonce =
                artifact.evidence.expected.activationNonce,
            .entrypointDigest = sha256(bytes),
            .entrypointSize = static_cast<quint64>(bytes.size()),
            .entrypointDevice = device,
            .entrypointInode = inode,
        };
    }

    [[nodiscard]] LegacyLiveActivationBridgeRecordV1 bridge(
        const Artifact &before,
        const Artifact &after,
        const LegacyLiveActivationBridgePhaseV1 phase =
            LegacyLiveActivationBridgePhaseV1::Ready
    ) const
    {
        const auto prior = ownership(before);
        const auto targetBytes = after.evidence.files.value(
            QStringLiteral("hyprland.lua")
        );
        return {
            .phase = phase,
            .token = QString::fromLatin1(token),
            .adoption = false,
            .targetGeneration = after.evidence.expected.generation,
            .targetNonce = after.evidence.expected.activationNonce,
            .targetDigest = sha256(targetBytes),
            .targetSize = static_cast<quint64>(targetBytes.size()),
            .targetDevice = phase
                    == LegacyLiveActivationBridgePhaseV1::Ready
                ? 301ULL : 0ULL,
            .targetInode = phase
                    == LegacyLiveActivationBridgePhaseV1::Ready
                ? 302ULL : 0ULL,
            .swapName = QStringLiteral(".hyprshelld-transition-")
                + QString::fromLatin1(token) + QStringLiteral(".lua"),
            .beforeKind = LegacyEntrypointFileKindV1::Regular,
            .beforeDigest = prior.entrypointDigest,
            .beforeSize = prior.entrypointSize,
            .beforeMode = 0600,
            .beforeDevice = prior.entrypointDevice,
            .beforeInode = prior.entrypointInode,
            .beforeGeneration = prior.generation,
            .beforeNonce = prior.activationNonce,
            .beforeOwnership = prior,
            .baselineConfigErrors = QByteArrayLiteral("[]"),
            .baselineProvider = QStringLiteral("lua"),
        };
    }

    [[nodiscard]] LegacyLiveActivationBridgeRecordV1 adoptionBridge(
        const Artifact &after,
        const LegacyLiveActivationBridgePhaseV1 phase =
            LegacyLiveActivationBridgePhaseV1::Ready
    ) const
    {
        const auto targetBytes = after.evidence.files.value(
            QStringLiteral("hyprland.lua")
        );
        return {
            .phase = phase,
            .token = QString::fromLatin1(token),
            .adoption = true,
            .targetGeneration = after.evidence.expected.generation,
            .targetNonce = after.evidence.expected.activationNonce,
            .targetDigest = sha256(targetBytes),
            .targetSize = static_cast<quint64>(targetBytes.size()),
            .targetDevice = phase
                    == LegacyLiveActivationBridgePhaseV1::Ready
                ? 301ULL : 0ULL,
            .targetInode = phase
                    == LegacyLiveActivationBridgePhaseV1::Ready
                ? 302ULL : 0ULL,
            .swapName = QStringLiteral(".hyprshelld-original-")
                + QString::fromLatin1(token) + QStringLiteral(".lua"),
            .beforeKind = LegacyEntrypointFileKindV1::Absent,
            .baselineConfigErrors = QByteArrayLiteral("[]"),
            .baselineProvider = QStringLiteral("hyprlang"),
        };
    }

    [[nodiscard]] LegacyEntrypointOwnershipRecordV1 derivedTargetOwnership(
        const LegacyLiveActivationBridgeRecordV1 &record
    ) const
    {
        LegacyEntrypointOriginalRecordV1 original;
        if (record.beforeOwnership) {
            original = record.beforeOwnership->original;
        } else if (record.beforeKind
                   == LegacyEntrypointFileKindV1::Regular) {
            original = {
                .kind = LegacyEntrypointFileKindV1::Regular,
                .digest = record.beforeDigest,
                .size = record.beforeSize,
                .mode = record.beforeMode,
                .device = record.beforeDevice,
                .inode = record.beforeInode,
                .backupName = record.swapName,
            };
        }
        return {
            .generation = record.targetGeneration,
            .activationNonce = record.targetNonce,
            .entrypointDigest = record.targetDigest,
            .entrypointSize = record.targetSize,
            .entrypointDevice = record.targetDevice,
            .entrypointInode = record.targetInode,
            .original = original,
        };
    }

    [[nodiscard]] LegacyOrdinaryPendingRecordV1 applyPending(
        const Artifact &before,
        const Artifact &after,
        const LegacyOrdinaryPendingPhaseV1 phase
    ) const
    {
        const auto beforeApplied = applied(before);
        const auto afterApplied = applied(after);
        return {
            .kind = LegacyOrdinaryPendingKindV1::Apply,
            .phase = phase,
            .expectedRevision = before.state.revision,
            .beforeDesiredDigest = beforeApplied.snapshotDigest,
            .candidateSnapshot = after.state,
            .candidateSnapshotBytes = after.evidence.desiredBytes,
            .snapshotDigest = afterApplied.snapshotDigest,
            .afterActivation = afterApplied,
            .beforeActivation = beforeApplied,
        };
    }

    [[nodiscard]] LegacyOrdinaryPendingRecordV1 recoveryPending(
        const QByteArray &currentDesired,
        const Artifact &before,
        const Artifact &after,
        const LegacyOrdinaryPendingPhaseV1 phase
    ) const
    {
        const auto beforeApplied = applied(before);
        const auto afterApplied = applied(after);
        return {
            .kind = LegacyOrdinaryPendingKindV1::Recovery,
            .phase = phase,
            .expectedRevision = 20,
            .beforeDesiredDigest = sha256(currentDesired),
            .candidateSnapshot = after.state,
            .candidateSnapshotBytes = after.evidence.desiredBytes,
            .snapshotDigest = afterApplied.snapshotDigest,
            .afterActivation = afterApplied,
            .beforeActivation = beforeApplied,
        };
    }

    [[nodiscard]] LegacyOrdinaryPendingRecordV1 adoptionApplyPending(
        const Artifact &after,
        const LegacyOrdinaryPendingPhaseV1 phase
    ) const
    {
        const auto afterApplied = applied(after);
        return {
            .kind = LegacyOrdinaryPendingKindV1::Apply,
            .phase = phase,
            .expectedRevision = after.state.revision,
            .beforeDesiredDigest = afterApplied.snapshotDigest,
            .candidateSnapshot = after.state,
            .candidateSnapshotBytes = after.evidence.desiredBytes,
            .snapshotDigest = afterApplied.snapshotDigest,
            .afterActivation = afterApplied,
            .beforeActivation = std::nullopt,
        };
    }

    [[nodiscard]] ReachableV1PreflightInput managedInput(
        const Artifact &artifact
    ) const
    {
        const auto appliedRecord = applied(artifact);
        const auto ownershipRecord = ownership(artifact);
        return {
            .desired = exact(artifact.evidence.desiredBytes),
            .lastGood = exact(artifact.evidence.desiredBytes),
            .applied = exact(*serializeLegacyAppliedRecordV1(appliedRecord)),
            .ownership = exact(
                *serializeLegacyEntrypointOwnershipRecordV1(ownershipRecord)
            ),
            .referencedGenerations = {artifact.evidence},
        };
    }

    [[nodiscard]] ReachableV1PreflightResult inspect(
        const ReachableV1PreflightInput &input
    ) const
    {
        return inspectReachableV1Preflight(
            input,
            migrationManifest_,
            sourceManifestV2_,
            catalogV1_,
            actionsV1_,
            catalogV2_,
            actionsV2_
        );
    }

private slots:
    void initTestCase()
    {
        const auto v1Catalog = parseCatalog(readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE
        )));
        const auto v1Actions = parseActionCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_ACTION_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE))
        );
        const auto v2Catalog = parseDormantCatalogV2(readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE
        )));
        const auto v2Actions = parseDormantActionCatalogV2(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_SCHEMA_FILE))
        );
        QVERIFY(v1Catalog);
        QVERIFY(v1Actions);
        QVERIFY(v2Catalog);
        QVERIFY(v2Actions);
        catalogV1_ = *v1Catalog.value;
        actionsV1_ = *v1Actions.value;
        catalogV2_ = *v2Catalog.value;
        actionsV2_ = *v2Actions.value;
        defaults_ = readObject(QStringLiteral(
            HYPRSHELLD_HYPRLAND_DEFAULTS_FILE
        ));
        migrationManifest_ = readBytes(QStringLiteral(
            HYPRSHELLD_MIGRATION_MANIFEST_FILE
        ));
        sourceManifestV2_ = readBytes(QStringLiteral(
            HYPRSHELLD_SOURCE_MANIFEST_V2_FILE
        ));
        QVERIFY(!defaults_.isEmpty());
        QCOMPARE(migrationManifest_.size(), qsizetype(37215));
        QCOMPARE(sourceManifestV2_.size(), qsizetype(73262));
    }

    void absentUnsafeAndUnknownReadsAreClosed()
    {
        const auto absent = inspect({});
        QCOMPARE(absent.disposition,
                 ReachableV1PreflightDisposition::Absent);
        QCOMPARE(absent.reason, ReachableV1PreflightReason::None);
        QVERIFY(!absent.resolved);

        ReachableV1PreflightInput unsafe;
        unsafe.desired.kind = static_cast<LegacyReadKindV1>(99);
        const auto rejected = inspect(unsafe);
        QCOMPARE(rejected.disposition,
                 ReachableV1PreflightDisposition::RepairOnly);
        QCOMPARE(rejected.reason,
                 ReachableV1PreflightReason::UnsafeRead);
        QCOMPARE(rejected.pendingResolution, PendingResolutionV1::None);
        QVERIFY(!rejected.resolved);
    }

    void mirrorPredicateClassifierCoversAllOverlappingTriples()
    {
        const std::array relations{
            LegacyMirrorRelationV1::Before,
            LegacyMirrorRelationV1::After,
            LegacyMirrorRelationV1::Both,
            LegacyMirrorRelationV1::Unrelated,
        };
        const auto before = [](const LegacyMirrorRelationV1 value) {
            return value == LegacyMirrorRelationV1::Before
                || value == LegacyMirrorRelationV1::Both;
        };
        const auto after = [](const LegacyMirrorRelationV1 value) {
            return value == LegacyMirrorRelationV1::After
                || value == LegacyMirrorRelationV1::Both;
        };
        for (const auto desired : relations) {
            for (const auto lastGood : relations) {
                for (const auto activation : relations) {
                    const MirrorRelationsV1 mirrors{
                        .desired = desired,
                        .lastGood = lastGood,
                        .activation = activation,
                    };
                    const auto unrelated =
                        desired == LegacyMirrorRelationV1::Unrelated
                        || lastGood == LegacyMirrorRelationV1::Unrelated
                        || activation == LegacyMirrorRelationV1::Unrelated;
                    const auto expectedPrepared = unrelated
                        ? LegacyPendingMirrorClassificationV1::
                            InvalidOrUnrelated
                        : before(desired) && before(lastGood)
                                && before(activation)
                            ? LegacyPendingMirrorClassificationV1::
                                CoherentPrepared
                            : LegacyPendingMirrorClassificationV1::
                                PreparedModified;
                    QCOMPARE(
                        classifyLegacyPendingMirrorPredicatesV1(
                            LegacyOrdinaryPendingPhaseV1::Prepared,
                            mirrors
                        ),
                        expectedPrepared
                    );

                    const auto writeOrderInvalid =
                        after(activation)
                            && (!after(desired) || !after(lastGood))
                        || after(lastGood) && !after(desired);
                    const auto expectedCommitting = unrelated
                        ? LegacyPendingMirrorClassificationV1::
                            InvalidOrUnrelated
                        : writeOrderInvalid
                            ? LegacyPendingMirrorClassificationV1::
                                CommittingWriteOrder
                            : LegacyPendingMirrorClassificationV1::
                                CoherentCommitting;
                    QCOMPARE(
                        classifyLegacyPendingMirrorPredicatesV1(
                            LegacyOrdinaryPendingPhaseV1::Committing,
                            mirrors
                        ),
                        expectedCommitting
                    );
                }
            }
        }

        const MirrorRelationsV1 unavailable{
            .desired = LegacyMirrorRelationV1::NotApplicable,
            .lastGood = LegacyMirrorRelationV1::Before,
            .activation = LegacyMirrorRelationV1::Before,
        };
        QCOMPARE(
            classifyLegacyPendingMirrorPredicatesV1(
                LegacyOrdinaryPendingPhaseV1::Prepared, unavailable
            ),
            LegacyPendingMirrorClassificationV1::InvalidOrUnrelated
        );
        auto invalidRelation = unavailable;
        invalidRelation.desired = static_cast<LegacyMirrorRelationV1>(99);
        QCOMPARE(
            classifyLegacyPendingMirrorPredicatesV1(
                LegacyOrdinaryPendingPhaseV1::Prepared, invalidRelation
            ),
            LegacyPendingMirrorClassificationV1::InvalidOrUnrelated
        );
        const MirrorRelationsV1 coherent{
            .desired = LegacyMirrorRelationV1::Before,
            .lastGood = LegacyMirrorRelationV1::Before,
            .activation = LegacyMirrorRelationV1::Before,
        };
        QCOMPARE(
            classifyLegacyPendingMirrorPredicatesV1(
                static_cast<LegacyOrdinaryPendingPhaseV1>(99), coherent
            ),
            LegacyPendingMirrorClassificationV1::InvalidOrUnrelated
        );
    }

    void coherentManagedTupleStillNeedsEntrypointQualification()
    {
        const auto generation = artifact(desiredBytes(17),
                                         QString::fromLatin1(nonceA));
        QVERIFY(!generation.evidence.manifestBytes.isEmpty());
        const auto checked = inspect(managedInput(generation));
        QCOMPARE(
            checked.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(
            checked.reason,
            ReachableV1PreflightReason::EntrypointQualificationRequired
        );
        QCOMPARE(checked.entrypoint.side, EntrypointSideV1::Managed);
        QCOMPARE(checked.pendingResolution, PendingResolutionV1::None);
        QVERIFY(checked.resolved);
        QCOMPARE(checked.resolved->desiredBytes,
                 generation.evidence.desiredBytes);
    }

    void unmanagedAndNoBridgeOwnershipPresenceAreExact()
    {
        const auto generation = artifact(desiredBytes(17),
                                         QString::fromLatin1(nonceA));
        ReachableV1PreflightInput unmanaged{
            .desired = exact(generation.evidence.desiredBytes),
        };
        const auto checked = inspect(unmanaged);
        QCOMPARE(
            checked.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(checked.entrypoint.side, EntrypointSideV1::Unmanaged);
        QVERIFY(!checked.entrypoint.kind);

        auto missingOwner = managedInput(generation);
        missingOwner.ownership = {};
        QCOMPARE(inspect(missingOwner).reason,
                 ReachableV1PreflightReason::OwnershipMismatch);

        auto ownerWithoutApplied = unmanaged;
        ownerWithoutApplied.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(
                ownership(generation)
            )
        );
        ownerWithoutApplied.referencedGenerations = {generation.evidence};
        QCOMPARE(inspect(ownerWithoutApplied).reason,
                 ReachableV1PreflightReason::OwnershipMismatch);

        ReachableV1PreflightInput residue;
        residue.ownership = ownerWithoutApplied.ownership;
        QCOMPARE(inspect(residue).reason,
                 ReachableV1PreflightReason::AbsentResidue);

        ReachableV1PreflightInput emptyRegular;
        emptyRegular.desired = exact({});
        QCOMPARE(inspect(emptyRegular).reason,
                 ReachableV1PreflightReason::InvalidDesired);

        ReachableV1PreflightInput missingDesired;
        missingDesired.lastGood = exact(generation.evidence.desiredBytes);
        QCOMPARE(inspect(missingDesired).reason,
                 ReachableV1PreflightReason::MissingDesired);
    }

    void activationRequirementRemainsSourceSyntaxOnly()
    {
        const auto generation = artifact(desiredBytes(17),
                                         QString::fromLatin1(nonceA));
        auto input = managedInput(generation);
        auto record = applied(generation);
        record.requiredActivation = ActivationRequirement::Session;
        input.applied = exact(*serializeLegacyAppliedRecordV1(record));
        const auto checked = inspect(input);
        QCOMPARE(
            checked.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(checked.resolved->applied->requiredActivation,
                 ActivationRequirement::Session);

        const auto after = artifact(
            generation.evidence.desiredBytes,
            QString::fromLatin1(nonceB)
        );
        auto pending = applyPending(
            generation,
            after,
            LegacyOrdinaryPendingPhaseV1::Prepared
        );
        pending.afterActivation.requiredActivation =
            ActivationRequirement::Session;
        input = managedInput(generation);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            pending, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            generation.evidence, after.evidence
        };
        QCOMPARE(
            inspect(input).pendingResolution,
            PendingResolutionV1::RemovePrepared
        );
    }

    void preparedApplyAcceptsStructuralBothMirrors()
    {
        const auto bytes = desiredBytes(17);
        const auto before = artifact(bytes, QString::fromLatin1(nonceA));
        const auto after = artifact(bytes, QString::fromLatin1(nonceB));
        const auto journal = applyPending(
            before, after, LegacyOrdinaryPendingPhaseV1::Prepared
        );
        auto input = managedInput(before);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            journal, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };

        const auto checked = inspect(input);
        QCOMPARE(
            checked.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(checked.pendingResolution,
                 PendingResolutionV1::RemovePrepared);
        QCOMPARE(checked.mirrors.desired,
                 LegacyMirrorRelationV1::Both);
        QCOMPARE(checked.mirrors.lastGood,
                 LegacyMirrorRelationV1::Both);
        QCOMPARE(checked.mirrors.activation,
                 LegacyMirrorRelationV1::Before);
    }

    void committingApplyUsesPredicateMembershipNotExclusiveLabels()
    {
        const auto bytes = desiredBytes(17);
        const auto before = artifact(bytes, QString::fromLatin1(nonceA));
        const auto after = artifact(bytes, QString::fromLatin1(nonceB));
        const auto journal = applyPending(
            before, after, LegacyOrdinaryPendingPhaseV1::Committing
        );
        auto input = managedInput(before);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            journal, catalogV1_, actionsV1_
        ));
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(before, after)
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };

        const auto checked = inspect(input);
        QCOMPARE(
            checked.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(checked.pendingResolution,
                 PendingResolutionV1::RollForwardCommitting);
        QCOMPARE(checked.mirrors.desired,
                 LegacyMirrorRelationV1::Both);
        QCOMPARE(checked.mirrors.lastGood,
                 LegacyMirrorRelationV1::Both);
        QCOMPARE(checked.entrypoint.side,
                 EntrypointSideV1::BridgeTarget);
    }

    void committingWriteOrderRejectsActivationAheadOfMirrors()
    {
        const auto current = desiredBytes(20);
        const auto before = artifact(desiredBytes(17),
                                     QString::fromLatin1(nonceA));
        const auto after = artifact(desiredBytes(21),
                                    QString::fromLatin1(nonceB));
        const auto journal = recoveryPending(
            current,
            before,
            after,
            LegacyOrdinaryPendingPhaseV1::Committing
        );
        auto input = managedInput(before);
        input.desired = exact(current);
        input.applied = exact(*serializeLegacyAppliedRecordV1(
            applied(after)
        ));
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            journal, catalogV1_, actionsV1_
        ));
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(before, after)
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };

        const auto checked = inspect(input);
        QCOMPARE(checked.reason,
                 ReachableV1PreflightReason::CommittingWriteOrder);
        QCOMPARE(checked.pendingResolution, PendingResolutionV1::None);
        QVERIFY(!checked.resolved);
    }

    void committingRecoveryAcceptsOnlyDurableWriteOrderPrefixes()
    {
        const auto before = artifact(desiredBytes(17),
                                     QString::fromLatin1(nonceA));
        const auto current = desiredBytes(20);
        const auto after = artifact(desiredBytes(21),
                                    QString::fromLatin1(nonceB));
        const auto journal = recoveryPending(
            current,
            before,
            after,
            LegacyOrdinaryPendingPhaseV1::Committing
        );
        const auto pendingBytes = *serializeLegacyOrdinaryPendingRecordV1(
            journal, catalogV1_, actionsV1_
        );
        const auto bridgeBytes =
            *serializeLegacyLiveActivationBridgeRecordV1(
                bridge(before, after)
            );
        const auto beforeApplied =
            *serializeLegacyAppliedRecordV1(applied(before));
        const auto afterApplied =
            *serializeLegacyAppliedRecordV1(applied(after));
        const auto priorOwner =
            *serializeLegacyEntrypointOwnershipRecordV1(ownership(before));

        const auto makeInput = [&](const bool desiredAfter,
                                   const bool lastGoodAfter,
                                   const bool activationAfter) {
            return ReachableV1PreflightInput{
                .desired = exact(
                    desiredAfter ? after.evidence.desiredBytes : current
                ),
                .lastGood = exact(
                    lastGoodAfter ? after.evidence.desiredBytes
                                  : before.evidence.desiredBytes
                ),
                .applied = exact(
                    activationAfter ? afterApplied : beforeApplied
                ),
                .pending = exact(pendingBytes),
                .ownership = exact(priorOwner),
                .bridge = exact(bridgeBytes),
                .referencedGenerations = {
                    before.evidence, after.evidence
                },
            };
        };
        const std::array valid{
            std::array{false, false, false},
            std::array{true, false, false},
            std::array{true, true, false},
            std::array{true, true, true},
        };
        for (const auto &prefix : valid) {
            const auto checked = inspect(makeInput(
                prefix[0], prefix[1], prefix[2]
            ));
            QCOMPARE(
                checked.disposition,
                ReachableV1PreflightDisposition::
                    ByteCoherentNeedsEntrypointQualification
            );
            QCOMPARE(
                checked.pendingResolution,
                PendingResolutionV1::RollForwardCommitting
            );
        }
        const std::array invalid{
            std::array{false, true, false},
            std::array{false, false, true},
            std::array{true, false, true},
        };
        for (const auto &tuple : invalid) {
            const auto checked = inspect(makeInput(
                tuple[0], tuple[1], tuple[2]
            ));
            QCOMPARE(
                checked.reason,
                ReachableV1PreflightReason::CommittingWriteOrder
            );
            QCOMPARE(checked.pendingResolution, PendingResolutionV1::None);
        }
    }

    void applyRecoveryAndDisplayRelationsAreExact()
    {
        const auto current17 = artifact(desiredBytes(17),
                                        QString::fromLatin1(nonceA));
        const auto changed17 = artifact(
            desiredBytes(
                17,
                QStringLiteral("0.56.1"),
                QString::fromLatin1(reviewedCatalogDigest),
                QString::fromLatin1(reviewedActionCatalogDigest),
                true
            ),
            QString::fromLatin1(nonceB)
        );
        auto invalidApply = LegacyOrdinaryPendingRecordV1{
            .kind = LegacyOrdinaryPendingKindV1::Apply,
            .phase = LegacyOrdinaryPendingPhaseV1::Prepared,
            .expectedRevision = 17,
            .beforeDesiredDigest =
                changed17.evidence.expected.snapshotDigest,
            .candidateSnapshot = changed17.state,
            .candidateSnapshotBytes = changed17.evidence.desiredBytes,
            .snapshotDigest = changed17.evidence.expected.snapshotDigest,
            .afterActivation = applied(changed17),
            .beforeActivation = applied(current17),
        };
        auto input = managedInput(current17);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            invalidApply, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            current17.evidence, changed17.evidence
        };
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::PendingKindRelation);

        const auto current20 = desiredBytes(20);
        const auto badRecoveryAfter = artifact(
            desiredBytes(
                21,
                QStringLiteral("0.56.1"),
                QString::fromLatin1(reviewedCatalogDigest),
                QString::fromLatin1(reviewedActionCatalogDigest),
                true
            ),
            QString::fromLatin1(nonceB)
        );
        const auto badRecovery = recoveryPending(
            current20,
            current17,
            badRecoveryAfter,
            LegacyOrdinaryPendingPhaseV1::Prepared
        );
        input = managedInput(current17);
        input.desired = exact(current20);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            badRecovery, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            current17.evidence, badRecoveryAfter.evidence
        };
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::PendingKindRelation);

        const auto displayBefore = artifact(
            desiredBytes(20), QString::fromLatin1(nonceA)
        );
        const auto displayAfter = artifact(
            desiredBytes(
                21,
                QStringLiteral("0.56.1"),
                QString::fromLatin1(reviewedCatalogDigest),
                QString::fromLatin1(reviewedActionCatalogDigest),
                true
            ),
            QString::fromLatin1(nonceB)
        );
        auto display = LegacyOrdinaryPendingRecordV1{
            .kind = LegacyOrdinaryPendingKindV1::DisplayPreview,
            .phase = LegacyOrdinaryPendingPhaseV1::Prepared,
            .expectedRevision = 20,
            .beforeDesiredDigest =
                displayBefore.evidence.expected.snapshotDigest,
            .candidateSnapshot = displayAfter.state,
            .candidateSnapshotBytes = displayAfter.evidence.desiredBytes,
            .snapshotDigest = displayAfter.evidence.expected.snapshotDigest,
            .afterActivation = applied(displayAfter),
            .beforeActivation = applied(displayBefore),
        };
        input = managedInput(displayBefore);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            display, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            displayBefore.evidence, displayAfter.evidence
        };
        QCOMPARE(
            inspect(input).pendingResolution,
            PendingResolutionV1::RemovePrepared
        );

        display.beforeActivation = applied(current17);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            display, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            current17.evidence, displayAfter.evidence
        };
        input.lastGood = exact(current17.evidence.desiredBytes);
        input.applied = exact(*serializeLegacyAppliedRecordV1(
            applied(current17)
        ));
        input.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(
                ownership(current17)
            )
        );
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::PendingKindRelation);
    }

    void byteDerivedMirrorsRouteThroughTheSharedClassifier()
    {
        const auto before = artifact(desiredBytes(17),
                                     QString::fromLatin1(nonceA));
        const auto current = desiredBytes(20);
        const auto after = artifact(desiredBytes(21),
                                    QString::fromLatin1(nonceB));
        const auto recovery = recoveryPending(
            current,
            before,
            after,
            LegacyOrdinaryPendingPhaseV1::Prepared
        );
        auto input = managedInput(before);
        input.desired = exact(after.evidence.desiredBytes);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            recovery, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };
        const auto modified = inspect(input);
        QCOMPARE(modified.mirrors.desired,
                 LegacyMirrorRelationV1::After);
        QCOMPARE(modified.reason,
                 ReachableV1PreflightReason::PreparedModified);

        const auto sameBytes = desiredBytes(17);
        const auto applyBefore = artifact(
            sameBytes, QString::fromLatin1(nonceA)
        );
        const auto applyAfter = artifact(
            sameBytes, QString::fromLatin1(nonceB)
        );
        const auto pending = applyPending(
            applyBefore,
            applyAfter,
            LegacyOrdinaryPendingPhaseV1::Prepared
        );
        input = managedInput(applyBefore);
        auto sourceSyntaxDifference = applied(applyBefore);
        sourceSyntaxDifference.requiredActivation =
            ActivationRequirement::Session;
        input.applied = exact(*serializeLegacyAppliedRecordV1(
            sourceSyntaxDifference
        ));
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            pending, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            applyBefore.evidence, applyAfter.evidence
        };
        const auto unrelated = inspect(input);
        QCOMPARE(unrelated.mirrors.activation,
                 LegacyMirrorRelationV1::Unrelated);
        QCOMPARE(unrelated.reason,
                 ReachableV1PreflightReason::MirrorUnrelated);
    }

    void noPendingBridgeSelectsPriorOrTargetAndRequiresReadyTarget()
    {
        const auto bytes = desiredBytes(17);
        const auto before = artifact(bytes, QString::fromLatin1(nonceA));
        const auto after = artifact(bytes, QString::fromLatin1(nonceB));
        auto input = managedInput(before);
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(before, after)
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };
        const auto prior = inspect(input);
        QCOMPARE(prior.entrypoint.side, EntrypointSideV1::BridgePrior);

        input.applied = exact(*serializeLegacyAppliedRecordV1(applied(after)));
        const auto target = inspect(input);
        QCOMPARE(
            target.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(target.entrypoint.side, EntrypointSideV1::BridgeTarget);

        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(
                before,
                after,
                LegacyLiveActivationBridgePhaseV1::Staging
            )
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::BridgeMismatch);
    }

    void generationCardinalityAndAliasesFailBeforeVerification()
    {
        const auto first = artifact(desiredBytes(17),
                                    QString::fromLatin1(nonceA));
        const auto second = artifact(desiredBytes(18),
                                     QString::fromLatin1(nonceB));
        auto input = managedInput(first);

        input.referencedGenerations.clear();
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MissingGeneration);

        input = managedInput(first);
        auto invalidExtra = second.evidence;
        invalidExtra.manifestBytes.clear();
        input.referencedGenerations.append(invalidExtra);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::ExtraGeneration);

        input = managedInput(first);
        auto alias = second.evidence;
        alias.expected.generation = first.evidence.expected.generation;
        input.referencedGenerations.append(alias);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::DuplicateGeneration);

        input = managedInput(first);
        alias = second.evidence;
        alias.expected.activationNonce =
            first.evidence.expected.activationNonce;
        input.referencedGenerations.append(alias);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::DuplicateGeneration);

        input = managedInput(first);
        input.referencedGenerations.append(first.evidence);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::DuplicateGeneration);

        input = managedInput(first);
        input.referencedGenerations[0].expected.activationNonce =
            second.evidence.expected.activationNonce;
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::DuplicateGeneration);

        input = managedInput(first);
        input.referencedGenerations[0].expected.generation =
            second.evidence.expected.generation;
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::DuplicateGeneration);

        input = managedInput(first);
        input.referencedGenerations.append(second.evidence);
        input.referencedGenerations.append(second.evidence);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::ExtraGeneration);
    }

    void invalidGenerationAndAppliedBindingsAreRejectedExactly()
    {
        const auto generation = artifact(desiredBytes(17),
                                         QString::fromLatin1(nonceA));
        auto input = managedInput(generation);
        input.referencedGenerations[0].manifestBytes.clear();
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::InvalidGeneration);

        input = managedInput(generation);
        auto record = applied(generation);
        record.entrypoint.append(QStringLiteral(".alias"));
        input.applied = exact(*serializeLegacyAppliedRecordV1(record));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MissingGeneration);

        input = managedInput(generation);
        record = applied(generation);
        record.snapshotDigest = QString(64, QLatin1Char('0'));
        input.applied = exact(*serializeLegacyAppliedRecordV1(record));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MissingGeneration);
    }

    void ownershipLoaderDigestAndSizeBindVerifiedBytes()
    {
        const auto generation = artifact(desiredBytes(17),
                                         QString::fromLatin1(nonceA));
        auto input = managedInput(generation);
        auto record = ownership(generation);
        record.entrypointDigest = QString(64, QLatin1Char('0'));
        input.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(record)
        );
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::OwnershipMismatch);

        input = managedInput(generation);
        record = ownership(generation);
        ++record.entrypointSize;
        input.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(record)
        );
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::OwnershipMismatch);
    }

    void recordAliasesAndExactBridgePairAreDistinguished()
    {
        const auto bytes = desiredBytes(17);
        const auto before = artifact(bytes, QString::fromLatin1(nonceA));
        const auto after = artifact(bytes, QString::fromLatin1(nonceB));

        auto input = managedInput(before);
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(before, before)
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::BridgeAmbiguous);

        auto aliasedBridge = bridge(before, after);
        aliasedBridge.targetGeneration =
            before.evidence.expected.generation;
        input = managedInput(before);
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            aliasedBridge
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::DuplicateGeneration);
    }

    void pendingBridgePhasesAndOwnershipCrossProductsAreExact()
    {
        const auto bytes = desiredBytes(17);
        const auto before = artifact(bytes, QString::fromLatin1(nonceA));
        const auto after = artifact(bytes, QString::fromLatin1(nonceB));
        auto prepared = applyPending(
            before, after, LegacyOrdinaryPendingPhaseV1::Prepared
        );
        auto committing = prepared;
        committing.phase = LegacyOrdinaryPendingPhaseV1::Committing;

        auto input = managedInput(before);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            prepared, catalogV1_, actionsV1_
        ));
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(
                before,
                after,
                LegacyLiveActivationBridgePhaseV1::Staging
            )
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };
        const auto preparedChecked = inspect(input);
        QCOMPARE(
            preparedChecked.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(preparedChecked.entrypoint.side,
                 EntrypointSideV1::BridgePrior);

        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            committing, catalogV1_, actionsV1_
        ));
        input.bridge = {};
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::PendingBridgeMismatch);

        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(
                before,
                after,
                LegacyLiveActivationBridgePhaseV1::Staging
            )
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::PendingBridgeMismatch);

        auto wrongTarget = bridge(before, after);
        wrongTarget.targetGeneration =
            before.evidence.expected.generation;
        wrongTarget.targetNonce = before.evidence.expected.activationNonce;
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            prepared, catalogV1_, actionsV1_
        ));
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            wrongTarget
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::PendingBridgeMismatch);

        auto noPendingTarget = managedInput(before);
        const auto ready = bridge(before, after);
        noPendingTarget.applied = exact(*serializeLegacyAppliedRecordV1(
            applied(after)
        ));
        noPendingTarget.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(ready)
        );
        noPendingTarget.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(
                derivedTargetOwnership(ready)
            )
        );
        noPendingTarget.referencedGenerations = {
            before.evidence, after.evidence
        };
        QCOMPARE(
            inspect(noPendingTarget).entrypoint.side,
            EntrypointSideV1::BridgeTarget
        );

        auto nearTarget = derivedTargetOwnership(ready);
        ++nearTarget.entrypointDevice;
        noPendingTarget.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(nearTarget)
        );
        QCOMPARE(inspect(noPendingTarget).reason,
                 ReachableV1PreflightReason::OwnershipMismatch);

        auto priorSelected = managedInput(before);
        priorSelected.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(ready)
        );
        priorSelected.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(
                derivedTargetOwnership(ready)
            )
        );
        priorSelected.referencedGenerations = {
            before.evidence, after.evidence
        };
        QCOMPARE(inspect(priorSelected).reason,
                 ReachableV1PreflightReason::OwnershipMismatch);

        auto badTargetLoader = ready;
        badTargetLoader.targetDigest = QString(64, QLatin1Char('0'));
        priorSelected = managedInput(before);
        priorSelected.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(badTargetLoader)
        );
        priorSelected.referencedGenerations = {
            before.evidence, after.evidence
        };
        QCOMPARE(inspect(priorSelected).reason,
                 ReachableV1PreflightReason::BridgeMismatch);

        auto badTargetSize = ready;
        ++badTargetSize.targetSize;
        priorSelected.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(badTargetSize)
        );
        QCOMPARE(inspect(priorSelected).reason,
                 ReachableV1PreflightReason::BridgeMismatch);

        auto badPriorLoader = ready;
        auto badPriorOwnership = *badPriorLoader.beforeOwnership;
        badPriorOwnership.entrypointDigest =
            QString(64, QLatin1Char('0'));
        badPriorLoader.beforeOwnership = badPriorOwnership;
        badPriorLoader.beforeDigest = badPriorOwnership.entrypointDigest;
        priorSelected = managedInput(before);
        priorSelected.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(badPriorLoader)
        );
        priorSelected.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(badPriorOwnership)
        );
        priorSelected.referencedGenerations = {
            before.evidence, after.evidence
        };
        QCOMPARE(inspect(priorSelected).reason,
                 ReachableV1PreflightReason::BridgeMismatch);

        badPriorLoader = ready;
        badPriorOwnership = *badPriorLoader.beforeOwnership;
        ++badPriorOwnership.entrypointSize;
        badPriorLoader.beforeOwnership = badPriorOwnership;
        badPriorLoader.beforeSize = badPriorOwnership.entrypointSize;
        priorSelected.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(badPriorLoader)
        );
        priorSelected.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(badPriorOwnership)
        );
        QCOMPARE(inspect(priorSelected).reason,
                 ReachableV1PreflightReason::BridgeMismatch);

        auto sameNonceAlias = ready;
        sameNonceAlias.targetNonce =
            before.evidence.expected.activationNonce;
        priorSelected = managedInput(before);
        priorSelected.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(sameNonceAlias)
        );
        priorSelected.referencedGenerations = {
            before.evidence, after.evidence
        };
        QCOMPARE(inspect(priorSelected).reason,
                 ReachableV1PreflightReason::DuplicateGeneration);

        priorSelected = managedInput(before);
        priorSelected.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(badTargetLoader)
        );
        priorSelected.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(
                derivedTargetOwnership(badTargetLoader)
            )
        );
        priorSelected.referencedGenerations = {
            before.evidence, after.evidence
        };
        QCOMPARE(inspect(priorSelected).reason,
                 ReachableV1PreflightReason::OwnershipMismatch);
    }

    void preparedFailureAndFuturePendingNeverLeakAPlan()
    {
        const auto bytes = desiredBytes(17);
        const auto before = artifact(bytes, QString::fromLatin1(nonceA));
        const auto after = artifact(bytes, QString::fromLatin1(nonceB));
        const auto pending = applyPending(
            before, after, LegacyOrdinaryPendingPhaseV1::Prepared
        );
        auto input = managedInput(before);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            pending, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            before.evidence, after.evidence
        };
        auto wrongOwnership = ownership(before);
        wrongOwnership.entrypointDigest = QString(64, QLatin1Char('0'));
        input.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(wrongOwnership)
        );
        const auto rejected = inspect(input);
        QCOMPARE(rejected.reason,
                 ReachableV1PreflightReason::OwnershipMismatch);
        QCOMPARE(rejected.pendingResolution, PendingResolutionV1::None);
        QVERIFY(!rejected.resolved);
        QCOMPARE(rejected.entrypoint.side, EntrypointSideV1::Unmanaged);
        QVERIFY(!rejected.entrypoint.kind);

        const auto futureBytes = desiredBytes(
            17, QStringLiteral("0.56.3")
        );
        const auto futureBefore = artifact(
            futureBytes, QString::fromLatin1(nonceA)
        );
        const auto futureAfter = artifact(
            futureBytes, QString::fromLatin1(nonceB)
        );
        const auto futurePending = applyPending(
            futureBefore,
            futureAfter,
            LegacyOrdinaryPendingPhaseV1::Prepared
        );
        input = managedInput(futureBefore);
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            futurePending, catalogV1_, actionsV1_
        ));
        input.referencedGenerations = {
            futureBefore.evidence, futureAfter.evidence
        };
        const auto unsupported = inspect(input);
        QCOMPARE(
            unsupported.disposition,
            ReachableV1PreflightDisposition::UnsupportedTarget
        );
        QCOMPARE(unsupported.pendingResolution, PendingResolutionV1::None);
        QVERIFY(!unsupported.resolved);
        QCOMPARE(unsupported.entrypoint.side,
                 EntrypointSideV1::Unmanaged);
        QVERIFY(!unsupported.entrypoint.kind);

        auto futureCommitting = futurePending;
        futureCommitting.phase =
            LegacyOrdinaryPendingPhaseV1::Committing;
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            futureCommitting, catalogV1_, actionsV1_
        ));
        input.bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
            bridge(futureBefore, futureAfter)
        ));
        const auto unsupportedCommitting = inspect(input);
        QCOMPARE(
            unsupportedCommitting.disposition,
            ReachableV1PreflightDisposition::UnsupportedTarget
        );
        QCOMPARE(unsupportedCommitting.pendingResolution,
                 PendingResolutionV1::None);
        QVERIFY(!unsupportedCommitting.resolved);
        QVERIFY(!unsupportedCommitting.entrypoint.kind);
    }

    void adoptionBridgeKeepsMissingPriorOwnershipExact()
    {
        const auto after = artifact(desiredBytes(17),
                                    QString::fromLatin1(nonceB));
        auto pending = adoptionApplyPending(
            after, LegacyOrdinaryPendingPhaseV1::Prepared
        );
        ReachableV1PreflightInput input{
            .desired = exact(after.evidence.desiredBytes),
            .pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
                pending, catalogV1_, actionsV1_
            )),
            .bridge = exact(*serializeLegacyLiveActivationBridgeRecordV1(
                adoptionBridge(
                    after,
                    LegacyLiveActivationBridgePhaseV1::Staging
                )
            )),
            .referencedGenerations = {after.evidence},
        };
        const auto prepared = inspect(input);
        QCOMPARE(
            prepared.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(prepared.entrypoint.side,
                 EntrypointSideV1::BridgePrior);
        QCOMPARE(prepared.entrypoint.kind,
                 std::optional{LegacyEntrypointFileKindV1::Absent});

        pending.phase = LegacyOrdinaryPendingPhaseV1::Committing;
        input.pending = exact(*serializeLegacyOrdinaryPendingRecordV1(
            pending, catalogV1_, actionsV1_
        ));
        const auto ready = adoptionBridge(after);
        input.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(ready)
        );
        const auto committing = inspect(input);
        QCOMPARE(committing.pendingResolution,
                 PendingResolutionV1::RollForwardCommitting);
        QCOMPARE(committing.entrypoint.side,
                 EntrypointSideV1::BridgeTarget);

        input.pending = {};
        input.lastGood = exact(after.evidence.desiredBytes);
        input.applied = exact(*serializeLegacyAppliedRecordV1(
            applied(after)
        ));
        const auto targetWithPriorReceipt = inspect(input);
        QCOMPARE(targetWithPriorReceipt.entrypoint.side,
                 EntrypointSideV1::BridgeTarget);

        input.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(
                derivedTargetOwnership(ready)
            )
        );
        QCOMPARE(inspect(input).entrypoint.side,
                 EntrypointSideV1::BridgeTarget);

        auto regular = adoptionBridge(after);
        const auto originalBytes = QByteArrayLiteral("user-loader");
        regular.beforeKind = LegacyEntrypointFileKindV1::Regular;
        regular.beforeDigest = sha256(originalBytes);
        regular.beforeSize = originalBytes.size();
        regular.beforeMode = 0644;
        regular.beforeDevice = 701;
        regular.beforeInode = 702;
        regular.baselineProvider = QStringLiteral("lua");
        input.bridge = exact(
            *serializeLegacyLiveActivationBridgeRecordV1(regular)
        );
        input.ownership = exact(
            *serializeLegacyEntrypointOwnershipRecordV1(
                derivedTargetOwnership(regular)
            )
        );
        const auto regularChecked = inspect(input);
        QCOMPARE(regularChecked.entrypoint.side,
                 EntrypointSideV1::BridgeTarget);
        const auto decoded = parseLegacyEntrypointOwnershipRecordV1(
            input.ownership.bytes
        );
        QVERIFY(decoded);
        QCOMPARE(decoded->original.backupName, regular.swapName);
        QCOMPARE(decoded->original.digest, regular.beforeDigest);
    }

    void snapshotLfPairAndRevisionRelationsFailClosed()
    {
        const auto applied17 = artifact(desiredBytes(17),
                                        QString::fromLatin1(nonceA));
        auto input = managedInput(applied17);
        auto withoutLf = applied17.evidence.desiredBytes;
        withoutLf.chop(1);
        input.desired = exact(withoutLf);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::InvalidDesired);

        input = managedInput(applied17);
        auto doubleLf = applied17.evidence.desiredBytes;
        doubleLf.append('\n');
        input.desired = exact(doubleLf);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::InvalidDesired);

        input = managedInput(applied17);
        input.lastGood = exact(withoutLf);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::InvalidLastGood);

        const auto after = artifact(
            applied17.evidence.desiredBytes,
            QString::fromLatin1(nonceB)
        );
        const auto pending = applyPending(
            applied17, after, LegacyOrdinaryPendingPhaseV1::Prepared
        );
        auto pendingBytes = *serializeLegacyOrdinaryPendingRecordV1(
            pending, catalogV1_, actionsV1_
        );
        pendingBytes.chop(1);
        input = managedInput(applied17);
        input.pending = exact(pendingBytes);
        input.referencedGenerations = {
            applied17.evidence, after.evidence
        };
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::InvalidPending);

        input = managedInput(applied17);
        input.referencedGenerations[0].desiredBytes.chop(1);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::InvalidGeneration);

        input = managedInput(applied17);
        input.referencedGenerations[0].manifestBytes.chop(1);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::InvalidGeneration);

        input = managedInput(applied17);
        auto wrongDomainApplied = applied(applied17);
        wrongDomainApplied.snapshotDigest = sha256(withoutLf);
        input.applied = exact(*serializeLegacyAppliedRecordV1(
            wrongDomainApplied
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MissingGeneration);

        input = managedInput(applied17);
        input.desired = exact(desiredBytes(16));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::DesiredRevisionRegression);

        input = managedInput(applied17);
        input.desired = exact(desiredBytes(
            17,
            QStringLiteral("0.56.1"),
            QString::fromLatin1(reviewedCatalogDigest),
            QString::fromLatin1(reviewedActionCatalogDigest),
            true
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::SameRevisionMismatch);

        const auto later = desiredBytes(18);
        input = managedInput(applied17);
        input.desired = exact(later);
        input.lastGood = exact(later);
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::LastGoodAppliedMismatch);
    }

    void desiredMayAdvanceButLastGoodAndAppliedRemainAPair()
    {
        const auto generation = artifact(desiredBytes(17),
                                         QString::fromLatin1(nonceA));
        auto input = managedInput(generation);
        input.desired = exact(desiredBytes(18));
        const auto advanced = inspect(input);
        QCOMPARE(
            advanced.disposition,
            ReachableV1PreflightDisposition::
                ByteCoherentNeedsEntrypointQualification
        );
        QCOMPARE(advanced.resolved->desiredBytes, desiredBytes(18));

        input = managedInput(generation);
        input.lastGood = {};
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::LastGoodAppliedMismatch);

        input = managedInput(generation);
        input.applied = {};
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::LastGoodAppliedMismatch);

        input = managedInput(generation);
        auto wrongRevision = applied(generation);
        ++wrongRevision.revision;
        input.applied = exact(*serializeLegacyAppliedRecordV1(
            wrongRevision
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MissingGeneration);
    }

    void mixedLineageAndUnsupportedTargetAreClosed()
    {
        constexpr auto oldestCatalog =
            "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0";
        constexpr auto oldCatalog =
            "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388";
        constexpr auto oldAction =
            "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2";
        const std::array catalogs{
            QString::fromLatin1(oldestCatalog),
            QString::fromLatin1(oldCatalog),
            QString::fromLatin1(reviewedCatalogDigest),
        };
        const std::array actions{
            QString::fromLatin1(oldAction),
            QString::fromLatin1(reviewedActionCatalogDigest),
        };
        for (const auto &catalog : catalogs) {
            for (const auto &action : actions) {
                const auto generation = artifact(
                    desiredBytes(
                        17,
                        QStringLiteral("0.56.1"),
                        catalog,
                        action,
                        false,
                        false
                    ),
                    QString::fromLatin1(nonceA)
                );
                const auto checked = inspect(managedInput(generation));
                QCOMPARE(
                    checked.disposition,
                    ReachableV1PreflightDisposition::
                        ByteCoherentNeedsEntrypointQualification
                );
            }
        }

        const auto last = artifact(desiredBytes(17),
                                   QString::fromLatin1(nonceA));
        auto input = managedInput(last);
        input.desired = exact(desiredBytes(
            18,
            QStringLiteral("0.56.1"),
            QString::fromLatin1(oldCatalog)
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MixedCatalogLineage);

        input = managedInput(last);
        input.desired = exact(desiredBytes(
            18,
            QStringLiteral("0.56.1"),
            QString::fromLatin1(reviewedCatalogDigest),
            QString::fromLatin1(oldAction),
            false,
            false
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MixedActionLineage);

        input = managedInput(last);
        input.desired = exact(desiredBytes(
            18, QStringLiteral("0.56.0")
        ));
        QCOMPARE(inspect(input).reason,
                 ReachableV1PreflightReason::MixedTarget);

        const auto future = artifact(
            desiredBytes(17, QStringLiteral("0.56.3")),
            QString::fromLatin1(nonceA)
        );
        QVERIFY(!future.evidence.manifestBytes.isEmpty());
        const auto unsupported = inspect(managedInput(future));
        QCOMPARE(
            unsupported.disposition,
            ReachableV1PreflightDisposition::UnsupportedTarget
        );
        QCOMPARE(unsupported.reason,
                 ReachableV1PreflightReason::UnsupportedTarget);
        QCOMPARE(unsupported.sourcePatch, quint32(3));
        QCOMPARE(unsupported.pendingResolution, PendingResolutionV1::None);
        QVERIFY(!unsupported.resolved);
        QCOMPARE(unsupported.entrypoint.side,
                 EntrypointSideV1::Unmanaged);
    }
};

QTEST_MAIN(CompositorReachableV1PreflightTest)
#include "compositor_reachable_v1_preflight_test.moc"
