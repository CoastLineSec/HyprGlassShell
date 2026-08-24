#include "dormant_generation_verifier.h"

#include "generation.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonObject>
#include <QSet>

namespace HyprShelld::Compositor {
namespace {

constexpr qsizetype maximumManifestBytes = 4 * 1024 * 1024;
constexpr qsizetype maximumGeneratedFileBytes = 16 * 1024 * 1024;

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

[[nodiscard]] QSet<QString> expectedFilePaths()
{
    QSet<QString> paths{QStringLiteral("hyprland.lua")};
    for (const auto &path : managedModulePaths()) {
        paths.insert(path);
    }
    return paths;
}

[[nodiscard]] QSet<QString> keysOf(const QMap<QString, QByteArray> &files)
{
    QSet<QString> paths;
    for (auto iterator = files.constBegin(); iterator != files.constEnd();
         ++iterator) {
        paths.insert(iterator.key());
    }
    return paths;
}

[[nodiscard]] QSet<QString> keysOf(const QJsonObject &files)
{
    QSet<QString> paths;
    for (auto iterator = files.constBegin(); iterator != files.constEnd();
         ++iterator) {
        paths.insert(iterator.key());
    }
    return paths;
}

} // namespace

Hyprland::ValidationResult<VerifiedDormantGenerationV2>
verifyDormantGenerationV2(
    const QByteArrayView manifestBytes,
    const QMap<QString, QByteArray> &fileBytes,
    const DormantGenerationV2Expectation &expected,
    const Hyprland::DesiredStateV2 &state,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2
)
{
    Hyprland::ValidationResult<VerifiedDormantGenerationV2> result;
    if (manifestBytes.isEmpty()
        || manifestBytes.size() > maximumManifestBytes) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("generation-v2.invalid-manifest-size"),
            QStringLiteral("The dormant v2 manifest has an invalid size.")
        );
        return result;
    }

    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        manifestBytes, maximumManifestBytes, 32
    );
    if (!parsed) {
        result.errors.append(parsed.errors);
        return result;
    }
    const auto &manifest = *parsed.value;
    auto canonicalManifest = Hyprland::JsonSupport::canonicalJson(manifest);
    canonicalManifest.append('\n');
    if (canonicalManifest != manifestBytes) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("generation-v2.noncanonical-manifest"),
            QStringLiteral(
                "The dormant v2 manifest must be recovered compact canonical JSON followed by exactly one newline."
            )
        );
    }

    // QMap size is O(1). Reject before walking, hashing, or copying a caller
    // map whose cardinality is outside the one exact renderer-v2 tree.
    if (fileBytes.size() != 17) {
        addError(
            result.errors,
            QStringLiteral("$.files"),
            QStringLiteral("generation-v2.file-set-mismatch"),
            QStringLiteral(
                "The generation must contain exactly hyprland.lua and the current 16 managed modules."
            )
        );
        return result;
    }

    const auto filesValue = manifest.value(QStringLiteral("files"));
    const auto manifestFiles = filesValue.toObject();
    const auto requiredFiles = expectedFilePaths();
    if (!filesValue.isObject() || requiredFiles.size() != 17
        || keysOf(manifestFiles) != requiredFiles
        || keysOf(fileBytes) != requiredFiles) {
        addError(
            result.errors,
            QStringLiteral("$.files"),
            QStringLiteral("generation-v2.file-set-mismatch"),
            QStringLiteral(
                "The generation must contain exactly hyprland.lua and the current 16 managed modules."
            )
        );
    }

    DormantRenderedGenerationV2 untrusted;
    untrusted.authorityId =
        manifest.value(QStringLiteral("authorityId")).toString();
    untrusted.generation =
        manifest.value(QStringLiteral("generation")).toString();
    untrusted.snapshotDigest =
        manifest.value(QStringLiteral("snapshotDigest")).toString();
    untrusted.sourceManifestDigest =
        manifest.value(QStringLiteral("sourceManifestDigest")).toString();
    untrusted.activationNonce =
        manifest.value(QStringLiteral("activationNonce")).toString();
    untrusted.createdAt =
        manifest.value(QStringLiteral("createdAt")).toString();
    untrusted.entrypoint =
        manifest.value(QStringLiteral("entrypoint")).toString();
    untrusted.manifest = manifest;
    untrusted.manifestBytes = manifestBytes.toByteArray();

    for (auto iterator = fileBytes.constBegin(); iterator != fileBytes.constEnd();
         ++iterator) {
        if (iterator->size() > maximumGeneratedFileBytes) {
            addError(
                result.errors,
                QStringLiteral("$.files.") + iterator.key(),
                QStringLiteral("generation-v2.invalid-file-size"),
                QStringLiteral(
                    "A generated file exceeds the 16 MiB byte limit."
                )
            );
            continue;
        }
        GeneratedFile file{
            .path = iterator.key(),
            .contents = *iterator,
            .sha256 = sha256(*iterator),
            .size = static_cast<quint64>(iterator->size()),
        };
        untrusted.files.insert(iterator.key(), std::move(file));
    }

    result.errors.append(validateDormantGenerationV2(
        untrusted, state, catalogV2, actionCatalogV2
    ));

    const auto revisionText =
        manifest.value(QStringLiteral("revision")).toString();
    if (untrusted.authorityId != expected.authorityId
        || expected.authorityId != state.authorityId) {
        addError(
            result.errors,
            QStringLiteral("$.authorityId"),
            QStringLiteral("generation-v2.expected-authority-mismatch"),
            QStringLiteral(
                "The manifest, Desired state, and explicit expectation must share one authority ID."
            )
        );
    }
    if (revisionText != QString::number(expected.revision)
        || expected.revision != state.semanticState.revision) {
        addError(
            result.errors,
            QStringLiteral("$.revision"),
            QStringLiteral("generation-v2.expected-revision-mismatch"),
            QStringLiteral(
                "The manifest, Desired state, and explicit expectation must share one revision."
            )
        );
    }
    if (untrusted.snapshotDigest != expected.snapshotDigest) {
        addError(
            result.errors,
            QStringLiteral("$.snapshotDigest"),
            QStringLiteral("generation-v2.expected-snapshot-mismatch"),
            QStringLiteral(
                "The manifest snapshot digest does not match the explicit expectation."
            )
        );
    }
    if (untrusted.generation != expected.generation) {
        addError(
            result.errors,
            QStringLiteral("$.generation"),
            QStringLiteral("generation-v2.expected-generation-mismatch"),
            QStringLiteral(
                "The manifest generation digest does not match the explicit expectation."
            )
        );
    }
    if (untrusted.activationNonce != expected.activationNonce) {
        addError(
            result.errors,
            QStringLiteral("$.activationNonce"),
            QStringLiteral("generation-v2.expected-nonce-mismatch"),
            QStringLiteral(
                "The manifest activation nonce does not match the explicit expectation."
            )
        );
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    const auto createdAt = QDateTime::fromString(
        untrusted.createdAt, Qt::ISODateWithMs
    );
    const auto rendererCreatedAt = createdAt.toUTC().toString(
        QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'")
    );
    if (!createdAt.isValid() || rendererCreatedAt != untrusted.createdAt) {
        addError(
            result.errors,
            QStringLiteral("$.createdAt"),
            QStringLiteral("generation-v2.creation-format-mismatch"),
            QStringLiteral(
                "The creation instant must be the exact millisecond UTC spelling produced by renderer v2."
            )
        );
        return result;
    }

    auto rerendered = renderDormantGenerationV2(
        state,
        catalogV2,
        actionCatalogV2,
        expected.generationRoot,
        expected.userCustomPath,
        expected.activationNonce,
        createdAt
    );
    if (!rerendered) {
        result.errors.append(rerendered.errors);
        return result;
    }
    result.errors.append(validateDormantGenerationV2(
        *rerendered.value, state, catalogV2, actionCatalogV2
    ));
    if (!result.errors.isEmpty()) {
        return result;
    }

    if (rerendered.value->manifestBytes != manifestBytes) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("generation-v2.rerendered-manifest-mismatch"),
            QStringLiteral(
                "The supplied manifest is not byte-identical to the authoritative renderer v2 manifest."
            )
        );
    }
    if (rerendered.value->files.size() != fileBytes.size()) {
        addError(
            result.errors,
            QStringLiteral("$.files"),
            QStringLiteral("generation-v2.rerendered-file-set-mismatch"),
            QStringLiteral(
                "The supplied file set is not the authoritative renderer v2 file set."
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
                    QStringLiteral("$.files.") + iterator.key(),
                    QStringLiteral("generation-v2.rerendered-file-mismatch"),
                    QStringLiteral(
                        "A supplied file is not byte-identical to the authoritative renderer v2 output."
                    )
                );
            }
        }
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    result.value = VerifiedDormantGenerationV2{
        .rendered = std::move(*rerendered.value),
        .generationRoot = expected.generationRoot,
        .userCustomPath = expected.userCustomPath,
    };
    return result;
}

} // namespace HyprShelld::Compositor
