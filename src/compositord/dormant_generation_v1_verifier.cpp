#include "dormant_generation_v1_verifier.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

constexpr qsizetype maximumManifestBytes = 4 * 1024 * 1024;
constexpr qsizetype maximumGeneratedFileBytes = 16 * 1024 * 1024;

constexpr const char *predecessorCatalogDigests[] = {
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0",
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388",
    Hyprland::reviewedCatalogDigest,
};

constexpr const char *predecessorActionCatalogDigests[] = {
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2",
    Hyprland::reviewedActionCatalogDigest,
};

struct CurrentAuthorities final {
    Hyprland::Catalog catalog;
    Hyprland::ActionCatalog actions;
};

void addError(
    Hyprland::ValidationErrors &errors,
    QString path,
    QString code,
    QString message
)
{
    errors.append({
        .path = std::move(path),
        .code = std::move(code),
        .message = std::move(message),
    });
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool validSha256(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return expression.match(value).hasMatch();
}

[[nodiscard]] bool validNonce(const QString &value)
{
    // The legacy v1 contract intentionally permits the all-zero spelling.
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-f]{32,128}$")
    );
    return expression.match(value).hasMatch();
}

[[nodiscard]] bool exactCreatedAt(
    const QString &value,
    QDateTime &createdAt
)
{
    static const QRegularExpression expression(
        QStringLiteral(
            "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}Z$"
        )
    );
    if (!expression.match(value).hasMatch()) {
        return false;
    }
    createdAt = QDateTime::fromString(value, Qt::ISODateWithMs);
    return createdAt.isValid()
        && createdAt.toUTC().toString(
               QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'")
           ) == value;
}

[[nodiscard]] bool exactInteger(
    const QJsonValue &value,
    const qint64 expected
)
{
    return value.isDouble()
        && std::isfinite(value.toDouble())
        && std::floor(value.toDouble()) == value.toDouble()
        && value.toDouble() == static_cast<double>(expected);
}

[[nodiscard]] bool hasExactKeys(
    const QJsonObject &object,
    const QSet<QString> &expected
)
{
    QSet<QString> actual;
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        actual.insert(iterator.key());
    }
    return actual == expected;
}

[[nodiscard]] QSet<QString> expectedFilePaths()
{
    QSet<QString> paths{QStringLiteral("hyprland.lua")};
    for (const auto &path : managedModulePaths()) {
        paths.insert(path);
    }
    return paths;
}

[[nodiscard]] QSet<QString> keysOf(
    const QMap<QString, QByteArray> &files
)
{
    QSet<QString> result;
    for (auto iterator = files.constBegin(); iterator != files.constEnd();
         ++iterator) {
        result.insert(iterator.key());
    }
    return result;
}

[[nodiscard]] QSet<QString> keysOf(const QJsonObject &files)
{
    QSet<QString> result;
    for (auto iterator = files.constBegin(); iterator != files.constEnd();
         ++iterator) {
        result.insert(iterator.key());
    }
    return result;
}

template<std::size_t Size>
[[nodiscard]] bool containsDigest(
    const char *const (&accepted)[Size],
    const QString &candidate
)
{
    for (const auto &digest : accepted) {
        if (candidate == QLatin1String(digest)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<CurrentAuthorities> reparseAuthorities(
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
)
{
    const auto parsedCatalog = Hyprland::parseCatalog(
        Hyprland::canonicalCatalogJson(catalog)
    );
    if (!parsedCatalog
        || parsedCatalog.value->digest
            != QLatin1String(Hyprland::reviewedCatalogDigest)) {
        return std::nullopt;
    }

    const auto parsedActions = Hyprland::parseActionCatalog(
        Hyprland::canonicalActionCatalogJson(actions),
        actions.configSchemaDocument
    );
    if (!parsedActions
        || parsedActions.value->digest
            != QLatin1String(Hyprland::reviewedActionCatalogDigest)) {
        return std::nullopt;
    }
    return CurrentAuthorities{
        .catalog = *parsedCatalog.value,
        .actions = *parsedActions.value,
    };
}

[[nodiscard]] bool containsProtectedWorkspaceIdentity(
    const Hyprland::DesiredState &state
)
{
    for (const auto &rule : state.workspaceRules) {
        if (rule.id == QLatin1String(Hyprland::sharedSpacingWorkspaceRuleId)
            || rule.selector
                == QLatin1String(
                    Hyprland::sharedSpacingWorkspaceRuleSelector
                )) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] QJsonObject exactCompatibility(
    const Hyprland::Catalog &catalog
)
{
    QJsonObject result{
        {QStringLiteral("major"),
         static_cast<qint64>(catalog.hyprland.major)},
        {QStringLiteral("minor"),
         static_cast<qint64>(catalog.hyprland.minor)},
        {QStringLiteral("reviewedVersion"),
         Hyprland::toString(catalog.hyprland.reviewedVersion)},
        {QStringLiteral("minimumPatch"),
         static_cast<qint64>(catalog.hyprland.minimumPatch)},
    };
    if (catalog.hyprland.maximumPatch) {
        result.insert(
            QStringLiteral("maximumPatch"),
            static_cast<qint64>(*catalog.hyprland.maximumPatch)
        );
    } else {
        result.insert(QStringLiteral("maximumPatch"), QJsonValue::Null);
    }
    return result;
}

} // namespace

Hyprland::ValidationResult<VerifiedDormantGenerationV1>
verifyDormantGenerationV1ForMigration(
    const QByteArrayView exactDesiredBytes,
    const QByteArrayView manifestBytes,
    const QMap<QString, QByteArray> &fileBytes,
    const DormantGenerationV1Expectation &expected,
    const Hyprland::Catalog &currentV1Catalog,
    const Hyprland::ActionCatalog &currentV1Actions
)
{
    Hyprland::ValidationResult<VerifiedDormantGenerationV1> result;

    if (exactDesiredBytes.isEmpty()
        || exactDesiredBytes.size() > Hyprland::maximumDesiredStateBytes) {
        addError(
            result.errors,
            QStringLiteral("$.desired"),
            QStringLiteral("generation-v1.invalid-desired-size"),
            QStringLiteral("The recovered v1 Desired bytes have an invalid size.")
        );
    }
    if (manifestBytes.isEmpty()
        || manifestBytes.size() > maximumManifestBytes) {
        addError(
            result.errors,
            QStringLiteral("$.manifest"),
            QStringLiteral("generation-v1.invalid-manifest-size"),
            QStringLiteral("The recovered v1 generation manifest has an invalid size.")
        );
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    // QMap::size() is O(1). Reject before walking, hashing, or copying a map
    // outside the only qualified v1 renderer tree.
    if (fileBytes.size() != 17) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.files"),
            QStringLiteral("generation-v1.file-set-mismatch"),
            QStringLiteral(
                "The generation must contain exactly hyprland.lua and the current 16 managed modules."
            )
        );
        return result;
    }
    const auto requiredFiles = expectedFilePaths();
    for (auto iterator = fileBytes.constBegin();
         iterator != fileBytes.constEnd(); ++iterator) {
        if (iterator.key().size() > 255) {
            addError(
                result.errors,
                QStringLiteral("$.manifest.files"),
                QStringLiteral("generation-v1.file-set-mismatch"),
                QStringLiteral(
                    "The generation must contain exactly the bounded current renderer paths."
                )
            );
            return result;
        }
    }
    if (requiredFiles.size() != 17 || keysOf(fileBytes) != requiredFiles) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.files"),
            QStringLiteral("generation-v1.file-set-mismatch"),
            QStringLiteral(
                "The generation must contain exactly hyprland.lua and the current 16 managed modules."
            )
        );
        return result;
    }
    for (auto iterator = fileBytes.constBegin(); iterator != fileBytes.constEnd();
         ++iterator) {
        if (iterator->size() > maximumGeneratedFileBytes) {
            addError(
                result.errors,
                QStringLiteral("$.manifest.files.") + iterator.key(),
                QStringLiteral("generation-v1.invalid-file-size"),
                QStringLiteral("A generated file exceeds the 16 MiB byte limit.")
            );
        }
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    const auto authorities = reparseAuthorities(
        currentV1Catalog, currentV1Actions
    );
    if (!authorities) {
        addError(
            result.errors,
            QStringLiteral("$.authorities"),
            QStringLiteral("generation-v1.unqualified-current-authority"),
            QStringLiteral(
                "The verifier requires the exact current active-v1 scalar and action authorities."
            )
        );
        return result;
    }

    const auto sourceParsed = Hyprland::JsonSupport::parseStrictObject(
        exactDesiredBytes, Hyprland::maximumDesiredStateBytes, 64
    );
    if (!sourceParsed) {
        result.errors.append(sourceParsed.errors);
        return result;
    }
    auto canonicalSource = Hyprland::JsonSupport::canonicalJson(
        *sourceParsed.value
    );
    canonicalSource.append('\n');
    if (canonicalSource != exactDesiredBytes) {
        addError(
            result.errors,
            QStringLiteral("$.desired"),
            QStringLiteral("generation-v1.noncanonical-desired"),
            QStringLiteral(
                "Recovered v1 Desired must be compact canonical JSON followed by exactly one newline."
            )
        );
        return result;
    }

    auto normalizedRoot = *sourceParsed.value;
    const auto catalogValue = normalizedRoot.value(
        QStringLiteral("catalogDigest")
    );
    const auto actionValue = normalizedRoot.value(
        QStringLiteral("actionCatalogDigest")
    );
    if (!catalogValue.isString() || !actionValue.isString()
        || !validSha256(catalogValue.toString())
        || !validSha256(actionValue.toString())
        || !containsDigest(
            predecessorCatalogDigests, catalogValue.toString()
        )
        || !containsDigest(
            predecessorActionCatalogDigests, actionValue.toString()
        )) {
        addError(
            result.errors,
            QStringLiteral("$.desired.catalogDigest"),
            QStringLiteral("generation-v1.unknown-lineage"),
            QStringLiteral(
                "The recovered v1 Desired state is outside the finite qualified catalog/action lineage."
            )
        );
        return result;
    }
    const auto sourceCatalogDigest = catalogValue.toString();
    const auto sourceActionDigest = actionValue.toString();
    normalizedRoot.insert(
        QStringLiteral("catalogDigest"),
        QLatin1String(Hyprland::reviewedCatalogDigest)
    );
    normalizedRoot.insert(
        QStringLiteral("actionCatalogDigest"),
        QLatin1String(Hyprland::reviewedActionCatalogDigest)
    );
    auto normalizedBytes = Hyprland::JsonSupport::canonicalJson(
        normalizedRoot
    );
    normalizedBytes.append('\n');

    const auto normalizedState = Hyprland::parseDesiredState(
        normalizedBytes, authorities->catalog, authorities->actions
    );
    if (!normalizedState) {
        result.errors.append(normalizedState.errors);
        return result;
    }
    auto restoredState = *normalizedState.value;
    restoredState.catalogDigest = sourceCatalogDigest;
    restoredState.actionCatalogDigest = sourceActionDigest;
    if (Hyprland::serializeDesiredState(restoredState)
        != exactDesiredBytes) {
        addError(
            result.errors,
            QStringLiteral("$.desired"),
            QStringLiteral("generation-v1.desired-reproduction-mismatch"),
            QStringLiteral(
                "The normalized v1 parser product does not reproduce the exact recovered bytes after lineage restoration."
            )
        );
        return result;
    }
    if (sourceActionDigest
            == QLatin1String(predecessorActionCatalogDigests[0])
        && containsProtectedWorkspaceIdentity(*normalizedState.value)) {
        addError(
            result.errors,
            QStringLiteral("$.desired.workspaceRules"),
            QStringLiteral("generation-v1.pre-shared-protected-rule"),
            QStringLiteral(
                "The pre-shared-spacing action lineage cannot contain the later reserved workspace identity or selector."
            )
        );
        return result;
    }

    const auto parsedManifest = Hyprland::JsonSupport::parseStrictObject(
        manifestBytes, maximumManifestBytes, 32
    );
    if (!parsedManifest) {
        result.errors.append(parsedManifest.errors);
        return result;
    }
    const auto &manifest = *parsedManifest.value;
    auto canonicalManifest = Hyprland::JsonSupport::canonicalJson(manifest);
    canonicalManifest.append('\n');
    if (canonicalManifest != manifestBytes) {
        addError(
            result.errors,
            QStringLiteral("$.manifest"),
            QStringLiteral("generation-v1.noncanonical-manifest"),
            QStringLiteral(
                "The v1 manifest must be compact canonical JSON followed by exactly one newline."
            )
        );
        return result;
    }

    const QSet<QString> requiredRootKeys{
        QStringLiteral("formatVersion"),
        QStringLiteral("contractVersion"),
        QStringLiteral("generation"),
        QStringLiteral("snapshotDigest"),
        QStringLiteral("catalogDigest"),
        QStringLiteral("actionCatalogDigest"),
        QStringLiteral("revision"),
        QStringLiteral("targetHyprland"),
        QStringLiteral("compatibleHyprland"),
        QStringLiteral("rendererVersion"),
        QStringLiteral("activationNonce"),
        QStringLiteral("createdAt"),
        QStringLiteral("entrypoint"),
        QStringLiteral("files"),
    };
    if (!hasExactKeys(manifest, requiredRootKeys)) {
        addError(
            result.errors,
            QStringLiteral("$.manifest"),
            QStringLiteral("generation-v1.invalid-root-fields"),
            QStringLiteral("The v1 manifest must have exactly its 14 contract fields.")
        );
    }
    if (!exactInteger(manifest.value(QStringLiteral("formatVersion")), 1)
        || !exactInteger(
            manifest.value(QStringLiteral("contractVersion")), 1
        )
        || !exactInteger(
            manifest.value(QStringLiteral("rendererVersion")),
            currentRendererVersion
        )) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.formatVersion"),
            QStringLiteral("generation-v1.invalid-version"),
            QStringLiteral(
                "Generation format, contract, and renderer versions must match active v1 exactly."
            )
        );
    }

    const auto generation =
        manifest.value(QStringLiteral("generation")).toString();
    const auto snapshotDigest =
        manifest.value(QStringLiteral("snapshotDigest")).toString();
    const auto catalogDigest =
        manifest.value(QStringLiteral("catalogDigest")).toString();
    const auto actionCatalogDigest =
        manifest.value(QStringLiteral("actionCatalogDigest")).toString();
    const auto revision =
        manifest.value(QStringLiteral("revision")).toString();
    const auto target =
        manifest.value(QStringLiteral("targetHyprland")).toString();
    const auto activationNonce =
        manifest.value(QStringLiteral("activationNonce")).toString();
    const auto createdAtText =
        manifest.value(QStringLiteral("createdAt")).toString();
    const auto entrypoint =
        manifest.value(QStringLiteral("entrypoint")).toString();
    for (const auto &[path, digest] : std::array{
             std::pair{QStringLiteral("$.manifest.generation"), generation},
             std::pair{
                 QStringLiteral("$.manifest.snapshotDigest"), snapshotDigest
             },
             std::pair{
                 QStringLiteral("$.manifest.catalogDigest"), catalogDigest
             },
             std::pair{
                 QStringLiteral("$.manifest.actionCatalogDigest"),
                 actionCatalogDigest,
             },
         }) {
        if (!validSha256(digest)) {
            addError(
                result.errors,
                path,
                QStringLiteral("generation-v1.invalid-sha256"),
                QStringLiteral("A lowercase SHA-256 digest is required.")
            );
        }
    }
    if (!validNonce(activationNonce)) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.activationNonce"),
            QStringLiteral("generation-v1.invalid-nonce"),
            QStringLiteral(
                "The v1 activation nonce must be 32 to 128 lowercase hexadecimal characters."
            )
        );
    }
    QDateTime createdAt;
    if (!exactCreatedAt(createdAtText, createdAt)) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.createdAt"),
            QStringLiteral("generation-v1.invalid-created-at"),
            QStringLiteral(
                "The v1 creation instant must be the renderer's exact millisecond UTC spelling."
            )
        );
    }
    if (entrypoint != QStringLiteral("hyprland.lua")) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.entrypoint"),
            QStringLiteral("generation-v1.invalid-entrypoint"),
            QStringLiteral("The v1 entrypoint must be exactly hyprland.lua.")
        );
    }

    const auto desiredSnapshotDigest = sha256(exactDesiredBytes);
    if (snapshotDigest != desiredSnapshotDigest
        || snapshotDigest != expected.snapshotDigest) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.snapshotDigest"),
            QStringLiteral("generation-v1.snapshot-mismatch"),
            QStringLiteral(
                "The snapshot digest must bind the complete exact Desired bytes including their final newline."
            )
        );
    }
    if (catalogDigest != sourceCatalogDigest
        || actionCatalogDigest != sourceActionDigest) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.catalogDigest"),
            QStringLiteral("generation-v1.lineage-mismatch"),
            QStringLiteral(
                "The generation manifest and exact Desired bytes must bind the same v1 lineage."
            )
        );
    }
    if (revision != QString::number(normalizedState.value->revision)
        || revision != QString::number(expected.revision)) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.revision"),
            QStringLiteral("generation-v1.revision-mismatch"),
            QStringLiteral(
                "The manifest, Desired state, and explicit expectation must share one canonical revision."
            )
        );
    }
    if (target != normalizedState.value->targetHyprland) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.targetHyprland"),
            QStringLiteral("generation-v1.target-mismatch"),
            QStringLiteral("The manifest target does not match exact Desired bytes.")
        );
    }
    const auto compatibilityValue =
        manifest.value(QStringLiteral("compatibleHyprland"));
    if (!compatibilityValue.isObject()
        || compatibilityValue.toObject()
            != exactCompatibility(authorities->catalog)) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.compatibleHyprland"),
            QStringLiteral("generation-v1.compatibility-mismatch"),
            QStringLiteral(
                "The manifest compatibility object must match the exact current active-v1 catalog."
            )
        );
    }
    if (activationNonce != expected.activationNonce) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.activationNonce"),
            QStringLiteral("generation-v1.expected-nonce-mismatch"),
            QStringLiteral(
                "The manifest activation nonce does not match the explicit expectation."
            )
        );
    }
    if (generation != expected.generation) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.generation"),
            QStringLiteral("generation-v1.expected-generation-mismatch"),
            QStringLiteral(
                "The manifest generation digest does not match the explicit expectation."
            )
        );
    }

    const auto filesValue = manifest.value(QStringLiteral("files"));
    const auto manifestFiles = filesValue.toObject();
    if (!filesValue.isObject() || requiredFiles.size() != 17
        || keysOf(manifestFiles) != requiredFiles
        || keysOf(fileBytes) != requiredFiles) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.files"),
            QStringLiteral("generation-v1.file-set-mismatch"),
            QStringLiteral(
                "The manifest and supplied bytes must contain exactly the current 17-file renderer tree."
            )
        );
    } else {
        const QSet<QString> metadataKeys{
            QStringLiteral("sha256"), QStringLiteral("size"),
        };
        for (auto iterator = manifestFiles.constBegin();
             iterator != manifestFiles.constEnd(); ++iterator) {
            const auto path = QStringLiteral("$.manifest.files.")
                + iterator.key();
            if (!iterator.value().isObject()) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("generation-v1.invalid-file-metadata"),
                    QStringLiteral("Each file requires one metadata object.")
                );
                continue;
            }
            const auto metadata = iterator.value().toObject();
            const auto metadataDigest =
                metadata.value(QStringLiteral("sha256")).toString();
            const auto metadataSize = metadata.value(QStringLiteral("size"));
            const auto bytes = fileBytes.constFind(iterator.key());
            const auto exactSize = bytes == fileBytes.constEnd()
                ? qint64(-1) : static_cast<qint64>(bytes->size());
            if (!hasExactKeys(metadata, metadataKeys)
                || !validSha256(metadataDigest)
                || exactSize < 0
                || !exactInteger(metadataSize, exactSize)
                || metadataDigest != sha256(*bytes)) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("generation-v1.file-digest-mismatch"),
                    QStringLiteral(
                        "Supplied file bytes, bounded size, SHA-256, and manifest metadata must agree exactly."
                    )
                );
            }
        }
    }

    auto generationInput = manifest;
    generationInput.remove(QStringLiteral("generation"));
    if (generation
        != sha256(Hyprland::JsonSupport::canonicalJson(generationInput))) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.generation"),
            QStringLiteral("generation-v1.generation-mismatch"),
            QStringLiteral(
                "The generation digest must bind the canonical manifest with generation omitted and no trailing newline."
            )
        );
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    auto rerendered = renderGeneration(
        *normalizedState.value,
        authorities->catalog,
        authorities->actions,
        expected.generationRoot,
        expected.userCustomPath,
        expected.activationNonce,
        createdAt
    );
    if (!rerendered) {
        result.errors.append(rerendered.errors);
        return result;
    }

    // The current renderer is the byte authority. Only the three historical
    // lineage fields are projected back into its fresh v1 manifest before the
    // generation digest is recomputed.
    auto projectedManifest = rerendered.value->manifest;
    projectedManifest.insert(
        QStringLiteral("snapshotDigest"), desiredSnapshotDigest
    );
    projectedManifest.insert(
        QStringLiteral("catalogDigest"), sourceCatalogDigest
    );
    projectedManifest.insert(
        QStringLiteral("actionCatalogDigest"), sourceActionDigest
    );
    projectedManifest.remove(QStringLiteral("generation"));
    const auto projectedGeneration = sha256(
        Hyprland::JsonSupport::canonicalJson(projectedManifest)
    );
    projectedManifest.insert(
        QStringLiteral("generation"), projectedGeneration
    );
    auto projectedManifestBytes = Hyprland::JsonSupport::canonicalJson(
        projectedManifest
    );
    projectedManifestBytes.append('\n');

    if (projectedManifestBytes != manifestBytes
        || projectedGeneration != expected.generation) {
        addError(
            result.errors,
            QStringLiteral("$.manifest"),
            QStringLiteral("generation-v1.rerendered-manifest-mismatch"),
            QStringLiteral(
                "The supplied manifest is not byte-identical to the authoritative current-renderer lineage projection."
            )
        );
    }
    if (rerendered.value->files.size() != fileBytes.size()) {
        addError(
            result.errors,
            QStringLiteral("$.manifest.files"),
            QStringLiteral("generation-v1.rerendered-file-set-mismatch"),
            QStringLiteral(
                "The supplied file set is not the authoritative current-renderer file set."
            )
        );
    } else {
        for (auto iterator = rerendered.value->files.constBegin();
             iterator != rerendered.value->files.constEnd(); ++iterator) {
            const auto supplied = fileBytes.constFind(iterator.key());
            if (supplied == fileBytes.constEnd()
                || *supplied != iterator->contents) {
                addError(
                    result.errors,
                    QStringLiteral("$.manifest.files.") + iterator.key(),
                    QStringLiteral("generation-v1.rerendered-file-mismatch"),
                    QStringLiteral(
                        "A supplied file is not byte-identical to the authoritative current renderer output."
                    )
                );
            }
        }
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    result.value = VerifiedDormantGenerationV1{
        .generation = projectedGeneration,
        .snapshotDigest = desiredSnapshotDigest,
        .activationNonce = expected.activationNonce,
        .createdAt = rerendered.value->createdAt,
        .entrypoint = rerendered.value->entrypoint,
        .files = std::move(rerendered.value->files),
        .manifest = std::move(projectedManifest),
        .manifestBytes = std::move(projectedManifestBytes),
        .generationRoot = expected.generationRoot,
        .userCustomPath = expected.userCustomPath,
    };
    return result;
}

} // namespace HyprShelld::Compositor
