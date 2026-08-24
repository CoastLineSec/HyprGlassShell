#include "generation.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstring>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr mode_t mutableDirectoryMode = 0700;
constexpr mode_t immutableDirectoryMode = 0500;
constexpr mode_t mutableFileMode = 0600;
constexpr mode_t immutableFileMode = 0400;
constexpr qsizetype maximumManifestBytes = 4 * 1024 * 1024;
constexpr qsizetype maximumGeneratedFileBytes = 16 * 1024 * 1024;

GenerationResult failure(QString code, QString message)
{
    return {
        .success = false,
        .errorCode = std::move(code),
        .errorMessage = std::move(message),
    };
}

[[nodiscard]] bool validNonce(const QString &nonce)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-f]{32,128}$")
    );
    return expression.match(nonce).hasMatch();
}

[[nodiscard]] bool validSha256(const QString &value)
{
    if (value.size() != 64) return false;
    for (const auto character : value) {
        if (!((character >= u'0' && character <= u'9')
              || (character >= u'a' && character <= u'f'))) return false;
    }
    return true;
}

[[nodiscard]] bool validCreatedAt(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}Z$")
    );
    return expression.match(value).hasMatch()
        && QDateTime::fromString(
               value, QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'")
           ).isValid();
}

[[nodiscard]] bool retryFsync(const int descriptor)
{
    while (::fsync(descriptor) != 0) {
        if (errno != EINTR) return false;
    }
    return true;
}

[[nodiscard]] bool privateDirectory(
    const int descriptor,
    const mode_t mode
)
{
    struct stat info {};
    return ::fstat(descriptor, &info) == 0
        && S_ISDIR(info.st_mode) && info.st_uid == ::geteuid()
        && (info.st_mode & 0777) == mode;
}

[[nodiscard]] bool descriptorStillNamed(
    const int parent,
    const QByteArray &name,
    const int descriptor
)
{
    struct stat opened {};
    struct stat named {};
    return ::fstat(descriptor, &opened) == 0
        && ::fstatat(parent, name.constData(), &named,
                     AT_SYMLINK_NOFOLLOW) == 0
        && S_ISDIR(opened.st_mode) && S_ISDIR(named.st_mode)
        && opened.st_dev == named.st_dev && opened.st_ino == named.st_ino
        && opened.st_mode == named.st_mode && opened.st_uid == named.st_uid
        && opened.st_nlink == named.st_nlink;
}

[[nodiscard]] QString hashBytes(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool writeAll(const int descriptor, const QByteArrayView bytes)
{
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::write(
            descriptor,
            bytes.data() + offset,
            static_cast<size_t>(bytes.size() - offset)
        );
        if (count < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        offset += count;
    }
    return true;
}

[[nodiscard]] bool createImmutableFile(
    const int directory,
    const QByteArray &name,
    const QByteArrayView contents
)
{
    const auto descriptor = ::openat(
        directory, name.constData(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        mutableFileMode
    );
    if (descriptor < 0) return false;
    const auto ok = writeAll(descriptor, contents)
        && ::fchmod(descriptor, immutableFileMode) == 0
        && retryFsync(descriptor);
    ::close(descriptor);
    return ok;
}

[[nodiscard]] bool removeTreeContents(const int directory)
{
    if (::fchmod(directory, mutableDirectoryMode) != 0) return false;
    const auto duplicate = ::dup(directory);
    if (duplicate < 0) return false;
    auto *stream = ::fdopendir(duplicate);
    if (!stream) {
        ::close(duplicate);
        return false;
    }
    bool success = true;
    while (const auto *entry = ::readdir(stream)) {
        const QByteArray name(entry->d_name);
        if (name == "." || name == "..") continue;
        struct stat info {};
        if (::fstatat(directory, name.constData(), &info,
                      AT_SYMLINK_NOFOLLOW) != 0
            || info.st_uid != ::geteuid()) {
            success = false;
            break;
        }
        if (S_ISREG(info.st_mode)) {
            if (info.st_nlink != 1
                || ::unlinkat(directory, name.constData(), 0) != 0) {
                success = false;
                break;
            }
        } else if (S_ISDIR(info.st_mode)) {
            const auto child = ::openat(
                directory, name.constData(),
                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
            );
            if (child < 0 || !removeTreeContents(child)) {
                if (child >= 0) ::close(child);
                success = false;
                break;
            }
            ::close(child);
            if (::unlinkat(directory, name.constData(), AT_REMOVEDIR) != 0) {
                success = false;
                break;
            }
        } else {
            success = false;
            break;
        }
    }
    ::closedir(stream);
    return success;
}

[[nodiscard]] bool cleanupStagingDirectories(const int generations)
{
    const auto duplicate = ::dup(generations);
    if (duplicate < 0) return false;
    auto *stream = ::fdopendir(duplicate);
    if (!stream) {
        ::close(duplicate);
        return false;
    }
    bool success = true;
    while (const auto *entry = ::readdir(stream)) {
        const QByteArray name(entry->d_name);
        if (!name.startsWith(".pending-")) continue;
        const auto suffix = QString::fromLatin1(name.sliced(
            static_cast<qsizetype>(sizeof(".pending-") - 1)
        ));
        if (!validNonce(suffix)) continue;
        const auto child = ::openat(
            generations, name.constData(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        );
        if (child < 0 || !privateDirectory(child, mutableDirectoryMode)
            || !descriptorStillNamed(generations, name, child)
            || !removeTreeContents(child)
            || !descriptorStillNamed(generations, name, child)) {
            if (child >= 0) ::close(child);
            success = false;
            break;
        }
        ::close(child);
        if (::unlinkat(generations, name.constData(), AT_REMOVEDIR) != 0) {
            success = false;
            break;
        }
    }
    ::closedir(stream);
    return success && retryFsync(generations);
}

[[nodiscard]] bool cleanupStaging(
    const int generations,
    const QByteArray &name
)
{
    const auto child = ::openat(
        generations, name.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (child < 0) return errno == ENOENT;
    struct stat info {};
    const auto safe = ::fstat(child, &info) == 0
        && S_ISDIR(info.st_mode) && info.st_uid == ::geteuid()
        && ((info.st_mode & 0777) == mutableDirectoryMode
            || (info.st_mode & 0777) == immutableDirectoryMode)
        && descriptorStillNamed(generations, name, child);
    const auto removed = safe && removeTreeContents(child)
        && descriptorStillNamed(generations, name, child);
    ::close(child);
    if (!removed) return false;
    return ::unlinkat(generations, name.constData(), AT_REMOVEDIR) == 0
        && retryFsync(generations);
}

struct ReadFileResult final {
    bool success = false;
    QByteArray bytes;
    struct stat info {};
};

[[nodiscard]] ReadFileResult readImmutableFile(
    const int directory,
    const QByteArray &name,
    const qsizetype maximum
)
{
    ReadFileResult result;
    const auto descriptor = ::openat(
        directory, name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    );
    if (descriptor < 0) return result;
    struct stat before {};
    if (::fstat(descriptor, &before) != 0 || !S_ISREG(before.st_mode)
        || before.st_uid != ::geteuid() || before.st_nlink != 1
        || (before.st_mode & 0777) != immutableFileMode
        || before.st_size < 0 || before.st_size > maximum) {
        ::close(descriptor);
        return result;
    }
    QByteArray bytes;
    bytes.reserve(static_cast<qsizetype>(before.st_size));
    std::array<char, 64 * 1024> buffer{};
    while (true) {
        const auto count = ::read(descriptor, buffer.data(), buffer.size());
        if (count == 0) break;
        if (count < 0) {
            if (errno == EINTR) continue;
            ::close(descriptor);
            return result;
        }
        if (bytes.size() > maximum - count) {
            ::close(descriptor);
            return result;
        }
        bytes.append(buffer.data(), count);
    }
    struct stat after {};
    struct stat named {};
    const auto stable = ::fstat(descriptor, &after) == 0
        && before.st_dev == after.st_dev && before.st_ino == after.st_ino
        && before.st_mode == after.st_mode && before.st_nlink == after.st_nlink
        && before.st_uid == after.st_uid && before.st_size == after.st_size
        && before.st_mtim.tv_sec == after.st_mtim.tv_sec
        && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec
        && ::fstatat(directory, name.constData(), &named,
                     AT_SYMLINK_NOFOLLOW) == 0
        && named.st_dev == after.st_dev && named.st_ino == after.st_ino
        && named.st_mode == after.st_mode && named.st_uid == after.st_uid
        && named.st_nlink == after.st_nlink
        && bytes.size() == after.st_size;
    ::close(descriptor);
    if (!stable) return result;
    result.success = true;
    result.bytes = std::move(bytes);
    result.info = after;
    return result;
}

[[nodiscard]] QSet<QString> directoryEntries(const int directory, bool &ok)
{
    QSet<QString> result;
    const auto duplicate = ::dup(directory);
    if (duplicate < 0) {
        ok = false;
        return result;
    }
    auto *stream = ::fdopendir(duplicate);
    if (!stream) {
        ::close(duplicate);
        ok = false;
        return result;
    }
    ::rewinddir(stream);
    ok = true;
    while (const auto *entry = ::readdir(stream)) {
        const QByteArray name(entry->d_name);
        if (name == "." || name == "..") continue;
        result.insert(QFile::decodeName(name));
    }
    ::closedir(stream);
    return result;
}

[[nodiscard]] bool parseRevision(const QString &text, quint64 &revision)
{
    if (text.isEmpty() || (text.size() > 1 && text.front() == u'0')) return false;
    for (const auto character : text) {
        if (!character.isDigit() || character.unicode() > '9') return false;
    }
    bool converted = false;
    revision = text.toULongLong(&converted, 10);
    return converted;
}

void addDormantV2Error(
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

[[nodiscard]] bool exactInteger(
    const QJsonValue &value,
    const qint64 expected
)
{
    return value.isDouble()
        && std::floor(value.toDouble()) == value.toDouble()
        && value.toDouble() == static_cast<double>(expected);
}

[[nodiscard]] bool validDormantV2CreatedAt(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral(
            "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(?:\\.[0-9]{1,9})?Z$"
        )
    );
    if (value.size() > 32 || !expression.match(value).hasMatch()) {
        return false;
    }
    return QDateTime::fromString(
        value.first(19), QStringLiteral("yyyy-MM-dd'T'HH:mm:ss")
    ).isValid();
}

[[nodiscard]] bool validDormantV2RelativePath(const QString &path)
{
    static const QRegularExpression expression(
        QStringLiteral(
            "^[A-Za-z0-9][A-Za-z0-9._-]*(?:/[A-Za-z0-9][A-Za-z0-9._-]*)*$"
        )
    );
    return !path.isEmpty() && path.size() <= 255
        && expression.match(path).hasMatch();
}

[[nodiscard]] QSet<QString> expectedFileSet()
{
    QSet<QString> result{QStringLiteral("hyprland.lua")};
    for (const auto &path : managedModulePaths()) result.insert(path);
    return result;
}

} // namespace

Hyprland::ValidationErrors validateDormantGenerationV2(
    const DormantRenderedGenerationV2 &rendered,
    const Hyprland::DesiredStateV2 &state,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2
)
{
    Hyprland::ValidationErrors errors;
    const auto fail = [&errors](
                          QString path,
                          QString code,
                          QString message
                      ) {
        addDormantV2Error(
            errors, std::move(path), std::move(code), std::move(message)
        );
    };

    if (rendered.manifestBytes.isEmpty()
        || rendered.manifestBytes.size() > maximumManifestBytes) {
        fail(
            QStringLiteral("$"),
            QStringLiteral("generation-v2.invalid-manifest-size"),
            QStringLiteral("The dormant v2 manifest has an invalid size.")
        );
        return errors;
    }
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        QByteArrayView(rendered.manifestBytes), maximumManifestBytes, 32
    );
    if (!parsed) {
        errors.append(parsed.errors);
        return errors;
    }
    const auto manifest = *parsed.value;
    auto canonicalManifest = Hyprland::JsonSupport::canonicalJson(manifest);
    canonicalManifest.append('\n');
    if (canonicalManifest != rendered.manifestBytes) {
        fail(
            QStringLiteral("$"),
            QStringLiteral("generation-v2.noncanonical-manifest"),
            QStringLiteral(
                "The dormant v2 manifest must be compact canonical JSON followed by one newline."
            )
        );
    }
    if (manifest != rendered.manifest) {
        fail(
            QStringLiteral("$"),
            QStringLiteral("generation-v2.manifest-object-mismatch"),
            QStringLiteral(
                "The parsed dormant v2 manifest does not match the rendered manifest object."
            )
        );
    }

    const QSet<QString> requiredKeys{
        QStringLiteral("formatVersion"),
        QStringLiteral("contractVersion"),
        QStringLiteral("authorityId"),
        QStringLiteral("generation"),
        QStringLiteral("snapshotDigest"),
        QStringLiteral("sourceManifestDigest"),
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
    if (!hasExactKeys(manifest, requiredKeys)) {
        fail(
            QStringLiteral("$"),
            QStringLiteral("generation-v2.invalid-root-fields"),
            QStringLiteral(
                "The dormant v2 manifest must have exactly its 16 contract fields."
            )
        );
    }
    if (!exactInteger(
            manifest.value(QStringLiteral("formatVersion")),
            dormantGenerationV2FormatVersion
        )
        || !exactInteger(
            manifest.value(QStringLiteral("contractVersion")),
            dormantGenerationV2ContractVersion
        )
        || !exactInteger(
            manifest.value(QStringLiteral("rendererVersion")),
            dormantRendererV2Version
        )) {
        fail(
            QStringLiteral("$.formatVersion"),
            QStringLiteral("generation-v2.invalid-version"),
            QStringLiteral(
                "Generation format, contract, and renderer versions must all be exactly 2."
            )
        );
    }

    const auto authorityId =
        manifest.value(QStringLiteral("authorityId")).toString();
    const auto generation =
        manifest.value(QStringLiteral("generation")).toString();
    const auto snapshotDigest =
        manifest.value(QStringLiteral("snapshotDigest")).toString();
    const auto sourceManifestDigest =
        manifest.value(QStringLiteral("sourceManifestDigest")).toString();
    const auto catalogDigest =
        manifest.value(QStringLiteral("catalogDigest")).toString();
    const auto actionCatalogDigest =
        manifest.value(QStringLiteral("actionCatalogDigest")).toString();
    const auto activationNonce =
        manifest.value(QStringLiteral("activationNonce")).toString();
    const auto createdAt =
        manifest.value(QStringLiteral("createdAt")).toString();
    const auto entrypoint =
        manifest.value(QStringLiteral("entrypoint")).toString();

    if (!Hyprland::isCanonicalAuthorityId(authorityId)) {
        fail(
            QStringLiteral("$.authorityId"),
            QStringLiteral("generation-v2.invalid-authority-id"),
            QStringLiteral(
                "Authority ID must be a nonzero 32-character lowercase hexadecimal value."
            )
        );
    }
    if (!Hyprland::isCanonicalAuthorityId(activationNonce)) {
        fail(
            QStringLiteral("$.activationNonce"),
            QStringLiteral("generation-v2.invalid-nonce"),
            QStringLiteral(
                "Activation nonce must be a nonzero 32-character lowercase hexadecimal value."
            )
        );
    }
    for (const auto &[path, digest] : std::array{
             std::pair{QStringLiteral("$.generation"), generation},
             std::pair{QStringLiteral("$.snapshotDigest"), snapshotDigest},
             std::pair{
                 QStringLiteral("$.sourceManifestDigest"),
                 sourceManifestDigest,
             },
             std::pair{QStringLiteral("$.catalogDigest"), catalogDigest},
             std::pair{
                 QStringLiteral("$.actionCatalogDigest"),
                 actionCatalogDigest,
             },
         }) {
        if (!validSha256(digest)) {
            fail(
                path,
                QStringLiteral("generation-v2.invalid-sha256"),
                QStringLiteral("A lowercase SHA-256 digest is required.")
            );
        }
    }
    if (!validDormantV2CreatedAt(createdAt)) {
        fail(
            QStringLiteral("$.createdAt"),
            QStringLiteral("generation-v2.invalid-created-at"),
            QStringLiteral("A UTC RFC 3339 creation instant is required.")
        );
    }
    if (!validDormantV2RelativePath(entrypoint)) {
        fail(
            QStringLiteral("$.entrypoint"),
            QStringLiteral("generation-v2.invalid-entrypoint"),
            QStringLiteral("A safe relative entrypoint path is required.")
        );
    }

    quint64 revision = 0;
    const auto revisionText =
        manifest.value(QStringLiteral("revision")).toString();
    if (!parseRevision(revisionText, revision)) {
        fail(
            QStringLiteral("$.revision"),
            QStringLiteral("generation-v2.invalid-revision"),
            QStringLiteral("A canonical unsigned 64-bit revision is required.")
        );
    }
    if (manifest.value(QStringLiteral("targetHyprland")).toString()
        != QStringLiteral("0.56.2")) {
        fail(
            QStringLiteral("$.targetHyprland"),
            QStringLiteral("generation-v2.invalid-target"),
            QStringLiteral("The dormant v2 target must be exactly 0.56.2.")
        );
    }

    const auto compatibleValue =
        manifest.value(QStringLiteral("compatibleHyprland"));
    const auto compatible = compatibleValue.toObject();
    const QSet<QString> compatibleKeys{
        QStringLiteral("major"),
        QStringLiteral("minor"),
        QStringLiteral("reviewedVersion"),
        QStringLiteral("minimumPatch"),
        QStringLiteral("maximumPatch"),
    };
    if (!compatibleValue.isObject()
        || !hasExactKeys(compatible, compatibleKeys)
        || !exactInteger(compatible.value(QStringLiteral("major")), 0)
        || !exactInteger(compatible.value(QStringLiteral("minor")), 56)
        || compatible.value(QStringLiteral("reviewedVersion")).toString()
            != QStringLiteral("0.56.2")
        || !exactInteger(
            compatible.value(QStringLiteral("minimumPatch")), 2
        )
        || !exactInteger(
            compatible.value(QStringLiteral("maximumPatch")), 2
        )) {
        fail(
            QStringLiteral("$.compatibleHyprland"),
            QStringLiteral("generation-v2.invalid-compatibility"),
            QStringLiteral(
                "The dormant v2 compatibility range must be exact patch range [2,2]."
            )
        );
    }

    const auto expectedSource =
        QLatin1String(Hyprland::dormantReviewedSourceManifestDigest);
    const auto expectedCatalog =
        QLatin1String(Hyprland::dormantReviewedCatalogV2Digest);
    const auto expectedActions =
        QLatin1String(Hyprland::dormantReviewedActionCatalogV2Digest);
    if (sourceManifestDigest != expectedSource
        || catalogV2.sourceManifestDigest != expectedSource
        || actionCatalogV2.sourceManifestDigest != expectedSource
        || catalogV2.sourceManifestDigest
            != actionCatalogV2.sourceManifestDigest) {
        fail(
            QStringLiteral("$.sourceManifestDigest"),
            QStringLiteral("generation-v2.source-authority-mismatch"),
            QStringLiteral(
                "Manifest, scalar catalog, and action catalog must bind the same exact source manifest."
            )
        );
    }
    if (catalogDigest != expectedCatalog
        || catalogV2.digest != expectedCatalog
        || state.semanticState.catalogDigest != expectedCatalog) {
        fail(
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("generation-v2.catalog-authority-mismatch"),
            QStringLiteral(
                "Manifest and desired state must bind the exact dormant v2 scalar catalog."
            )
        );
    }
    if (actionCatalogDigest != expectedActions
        || actionCatalogV2.digest != expectedActions
        || state.semanticState.actionCatalogDigest != expectedActions) {
        fail(
            QStringLiteral("$.actionCatalogDigest"),
            QStringLiteral("generation-v2.action-authority-mismatch"),
            QStringLiteral(
                "Manifest and desired state must bind the exact dormant v2 action catalog."
            )
        );
    }
    if (actionCatalogV2.source.repository
            != QStringLiteral("https://github.com/hyprwm/Hyprland")
        || actionCatalogV2.source.tag != QStringLiteral("v0.56.2")
        || actionCatalogV2.source.commit
            != QStringLiteral(
                "efb50993780079460b0cbed1363e2166a2de1d9f"
            )
        || actionCatalogV2.source.path
            != QStringLiteral(
                "src/config/lua/bindings/LuaBindingsDispatchers.cpp"
            )
        || actionCatalogV2.source.sha256
            != QStringLiteral(
                "a109eeb982856e0fe2ac9d88c29115a09984511787e19a20e7b4804e14a9d4de"
            )) {
        fail(
            QStringLiteral("$.actionCatalogDigest"),
            QStringLiteral("generation-v2.dispatcher-source-mismatch"),
            QStringLiteral(
                "The dormant v2 action authority has the wrong dispatcher source record."
            )
        );
    }

    const auto canonicalStateResult =
        Hyprland::serializeDormantDesiredStateV2(state);
    if (!canonicalStateResult) {
        errors.append(canonicalStateResult.errors);
    } else {
        const auto reparsedState = Hyprland::parseDormantDesiredStateV2(
            QByteArrayView(*canonicalStateResult.value),
            catalogV2,
            actionCatalogV2
        );
        if (!reparsedState) {
            errors.append(reparsedState.errors);
        } else if (*reparsedState.value != state) {
            fail(
                QStringLiteral("$"),
                QStringLiteral("generation-v2.unvalidated-state"),
                QStringLiteral(
                    "The supplied desired state is not the exact dormant v2 parser product."
                )
            );
        }
        auto canonicalState = *canonicalStateResult.value;
        if (!canonicalState.endsWith('\n')) {
            fail(
                QStringLiteral("$.snapshotDigest"),
                QStringLiteral("generation-v2.invalid-canonical-state"),
                QStringLiteral(
                    "The dormant v2 desired serializer must terminate with one newline."
                )
            );
        } else {
            canonicalState.chop(1);
            if (snapshotDigest != hashBytes(canonicalState)) {
                fail(
                    QStringLiteral("$.snapshotDigest"),
                    QStringLiteral("generation-v2.snapshot-mismatch"),
                    QStringLiteral(
                        "Snapshot digest does not bind canonical dormant v2 Desired JSON."
                    )
                );
            }
        }
    }
    if (authorityId != state.authorityId
        || rendered.authorityId != authorityId) {
        fail(
            QStringLiteral("$.authorityId"),
            QStringLiteral("generation-v2.authority-mismatch"),
            QStringLiteral(
                "Generation manifest, rendered output, and desired state must share one authority ID."
            )
        );
    }
    if (revision != state.semanticState.revision
        || revisionText != QString::number(state.semanticState.revision)) {
        fail(
            QStringLiteral("$.revision"),
            QStringLiteral("generation-v2.revision-mismatch"),
            QStringLiteral("Generation revision does not match Desired.")
        );
    }
    if (manifest.value(QStringLiteral("targetHyprland")).toString()
        != state.semanticState.targetHyprland) {
        fail(
            QStringLiteral("$.targetHyprland"),
            QStringLiteral("generation-v2.target-mismatch"),
            QStringLiteral("Generation target does not match Desired.")
        );
    }

    const auto filesValue = manifest.value(QStringLiteral("files"));
    const auto fileMap = filesValue.toObject();
    if (!filesValue.isObject() || fileMap.isEmpty() || fileMap.size() > 64
        || rendered.files.size() != fileMap.size()) {
        fail(
            QStringLiteral("$.files"),
            QStringLiteral("generation-v2.invalid-files"),
            QStringLiteral(
                "The manifest must bind exactly one through 64 rendered files."
            )
        );
    }
    for (auto iterator = fileMap.constBegin(); iterator != fileMap.constEnd();
         ++iterator) {
        const auto path = QStringLiteral("$.files.") + iterator.key();
        if (!validDormantV2RelativePath(iterator.key())
            || !iterator.value().isObject()) {
            fail(
                path,
                QStringLiteral("generation-v2.invalid-file-record"),
                QStringLiteral(
                    "Each generated file requires a safe path and metadata object."
                )
            );
            continue;
        }
        const auto metadata = iterator.value().toObject();
        const QSet<QString> metadataKeys{
            QStringLiteral("sha256"), QStringLiteral("size"),
        };
        const auto metadataDigest =
            metadata.value(QStringLiteral("sha256")).toString();
        const auto metadataSizeValue = metadata.value(QStringLiteral("size"));
        const auto metadataSize = metadataSizeValue.toInteger(-1);
        if (!hasExactKeys(metadata, metadataKeys)
            || !validSha256(metadataDigest)
            || !metadataSizeValue.isDouble()
            || std::floor(metadataSizeValue.toDouble())
                != metadataSizeValue.toDouble()
            || metadataSize < 0
            || metadataSize > maximumGeneratedFileBytes) {
            fail(
                path,
                QStringLiteral("generation-v2.invalid-file-metadata"),
                QStringLiteral(
                    "Each file requires exact bounded size and lowercase SHA-256 metadata."
                )
            );
            continue;
        }
        const auto renderedFile = rendered.files.constFind(iterator.key());
        if (renderedFile == rendered.files.constEnd()
            || renderedFile->path != iterator.key()
            || renderedFile->contents.size() > maximumGeneratedFileBytes
            || renderedFile->size
                != static_cast<quint64>(renderedFile->contents.size())
            || renderedFile->sha256 != hashBytes(renderedFile->contents)
            || metadataSize
                != static_cast<qint64>(renderedFile->contents.size())
            || metadataDigest != renderedFile->sha256) {
            fail(
                path,
                QStringLiteral("generation-v2.file-digest-mismatch"),
                QStringLiteral(
                    "Rendered file bytes, size, SHA-256, and manifest metadata do not agree."
                )
            );
        }
    }
    if (rendered.entrypoint != entrypoint
        || !fileMap.contains(entrypoint)
        || !rendered.files.contains(entrypoint)) {
        fail(
            QStringLiteral("$.entrypoint"),
            QStringLiteral("generation-v2.entrypoint-mismatch"),
            QStringLiteral(
                "The declared entrypoint must be an exact rendered file key."
            )
        );
    }

    if (rendered.generation != generation
        || rendered.snapshotDigest != snapshotDigest
        || rendered.sourceManifestDigest != sourceManifestDigest
        || rendered.activationNonce != activationNonce
        || rendered.createdAt != createdAt) {
        fail(
            QStringLiteral("$"),
            QStringLiteral("generation-v2.rendered-field-mismatch"),
            QStringLiteral(
                "Rendered v2 scalar fields do not match their manifest values."
            )
        );
    }
    auto generationInput = manifest;
    generationInput.remove(QStringLiteral("generation"));
    if (generation
        != hashBytes(Hyprland::JsonSupport::canonicalJson(generationInput))) {
        fail(
            QStringLiteral("$.generation"),
            QStringLiteral("generation-v2.generation-mismatch"),
            QStringLiteral(
                "Generation digest does not bind the canonical manifest with generation omitted."
            )
        );
    }
    return errors;
}

GenerationStore::GenerationStore(PersistentStore &store)
    : store_(store)
{
}

GenerationStore::~GenerationStore()
{
    shutdown();
}

void GenerationStore::shutdown() noexcept
{
    if (generationsDirectoryFd_ >= 0) ::close(generationsDirectoryFd_);
    generationsDirectoryFd_ = -1;
}

bool GenerationStore::directoryStillNamed() const
{
    return generationsDirectoryFd_ >= 0 && store_.rootsStillNamed()
        && privateDirectory(generationsDirectoryFd_, mutableDirectoryMode)
        && descriptorStillNamed(
            store_.managedDirectoryFd(),
            QByteArrayLiteral("generations"),
            generationsDirectoryFd_
        );
}

GenerationResult GenerationStore::initialize()
{
    if (!store_.initialized()) {
        return failure(QStringLiteral("generation.store-uninitialized"),
                       QStringLiteral("The persistent store lease is required first"));
    }
    if (generationsDirectoryFd_ >= 0) {
        return failure(QStringLiteral("generation.already-initialized"),
                       QStringLiteral("The generation store is already initialized"));
    }
    if (!store_.managedDirectoryStillNamed()) {
        return failure(QStringLiteral("generation.unsafe-managed-root"),
                       QStringLiteral("The managed compositor root is no longer named by the config root"));
    }
    if (::mkdirat(store_.managedDirectoryFd(), "generations",
                  mutableDirectoryMode) != 0 && errno != EEXIST) {
        return failure(QStringLiteral("generation.create-root-failed"),
                       QStringLiteral("Cannot create the immutable generations root"));
    }
    generationsDirectoryFd_ = ::openat(
        store_.managedDirectoryFd(), "generations",
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (generationsDirectoryFd_ < 0
        || !store_.managedDirectoryStillNamed()
        || !privateDirectory(generationsDirectoryFd_, mutableDirectoryMode)
        || !descriptorStillNamed(
            store_.managedDirectoryFd(),
            QByteArrayLiteral("generations"),
            generationsDirectoryFd_
        )) {
        return failure(QStringLiteral("generation.unsafe-root"),
                       QStringLiteral("The generations root is not a private directory"));
    }
    if (!retryFsync(generationsDirectoryFd_)
        || !retryFsync(store_.managedDirectoryFd())) {
        return failure(QStringLiteral("generation.create-root-sync-failed"),
                       QStringLiteral("Cannot durably create the immutable generations root"));
    }
    if (!cleanupStagingDirectories(generationsDirectoryFd_)) {
        return failure(QStringLiteral("generation.recovery-failed"),
                       QStringLiteral("Cannot safely remove an interrupted generation staging tree"));
    }

    const QByteArray customName("user-custom.lua");
    const auto custom = ::openat(
        store_.configDirectoryFd(), customName.constData(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        mutableFileMode
    );
    if (custom >= 0) {
        struct stat created {};
        const auto captured = ::fstat(custom, &created) == 0;
        const QByteArrayView initial(initialUserCustomContents);
        const auto ok = captured && writeAll(custom, initial)
            && ::fchmod(custom, mutableFileMode) == 0
            && retryFsync(custom);
        ::close(custom);
        if (!ok || !retryFsync(store_.configDirectoryFd())) {
            struct stat named {};
            if (captured
                && ::fstatat(store_.configDirectoryFd(), customName.constData(),
                             &named, AT_SYMLINK_NOFOLLOW) == 0
                && named.st_dev == created.st_dev
                && named.st_ino == created.st_ino) {
                const auto unlinked = ::unlinkat(
                    store_.configDirectoryFd(), customName.constData(), 0
                ) == 0;
                if (unlinked && !retryFsync(store_.configDirectoryFd())) {
                    return failure(
                        QStringLiteral("generation.custom-cleanup-sync-failed"),
                        QStringLiteral("A failed user-custom.lua initialization was removed, but its directory could not be synced")
                    );
                }
            }
            return failure(QStringLiteral("generation.custom-create-failed"),
                           QStringLiteral("Cannot durably initialize user-custom.lua"));
        }
    } else if (errno != EEXIST) {
        return failure(QStringLiteral("generation.custom-create-failed"),
                       QStringLiteral("Cannot create user-custom.lua without following an existing path"));
    }
    // EEXIST intentionally preserves any user-owned path entry without
    // reading, following, chmodding, rewriting, or deleting it.
    return {.success = true};
}

QString GenerationStore::directoryForNonce(const QString &nonce) const
{
    return QDir(store_.paths().generationsPath()).filePath(nonce);
}

GenerationResult GenerationStore::publish(const RenderedGeneration &rendered)
{
    if (generationsDirectoryFd_ < 0
        || !store_.managedDirectoryStillNamed()
        || !descriptorStillNamed(
            store_.managedDirectoryFd(),
            QByteArrayLiteral("generations"),
            generationsDirectoryFd_
        )) {
        return failure(QStringLiteral("generation.uninitialized"),
                       QStringLiteral("The generation store is not initialized"));
    }
    QSet<QString> renderedPaths;
    for (auto iterator = rendered.files.constBegin();
         iterator != rendered.files.constEnd(); ++iterator) {
        renderedPaths.insert(iterator.key());
    }
    if (!validNonce(rendered.activationNonce)
        || renderedPaths != expectedFileSet()
        || QDir(directoryForNonce(rendered.activationNonce)).dirName()
            != rendered.activationNonce) {
        return failure(QStringLiteral("generation.invalid-render"),
                       QStringLiteral("The rendered generation violates the fixed file/nonce contract"));
    }
    const auto nonce = rendered.activationNonce.toLatin1();
    struct stat existing {};
    if (::fstatat(generationsDirectoryFd_, nonce.constData(), &existing,
                  AT_SYMLINK_NOFOLLOW) == 0) {
        const auto verified = verify(rendered.activationNonce);
        if (verified.success && verified.generation
            && verified.generation->id == rendered.generation
            && verified.generation->manifest == rendered.manifestBytes) {
            return verified;
        }
        return failure(QStringLiteral("generation.nonce-conflict"),
                       QStringLiteral("The activation nonce already names a different or invalid immutable tree"));
    }
    if (errno != ENOENT) {
        return failure(QStringLiteral("generation.probe-failed"),
                       QStringLiteral("Cannot inspect the target generation"));
    }

    const auto staging = QByteArrayLiteral(".pending-") + nonce;
    auto stagingCreated = ::mkdirat(
        generationsDirectoryFd_, staging.constData(), mutableDirectoryMode
    ) == 0;
    if (!stagingCreated && errno == EEXIST
        && cleanupStaging(generationsDirectoryFd_, staging)) {
        stagingCreated = ::mkdirat(
            generationsDirectoryFd_, staging.constData(), mutableDirectoryMode
        ) == 0;
    }
    if (!stagingCreated) {
        return failure(QStringLiteral("generation.staging-create-failed"),
                       QStringLiteral("Cannot create the generation staging directory"));
    }
    const auto stagingFd = ::openat(
        generationsDirectoryFd_, staging.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (stagingFd < 0
        || !privateDirectory(stagingFd, mutableDirectoryMode)
        || !descriptorStillNamed(
            generationsDirectoryFd_, staging, stagingFd
        )
        || ::mkdirat(stagingFd, "modules", mutableDirectoryMode) != 0) {
        if (stagingFd >= 0) ::close(stagingFd);
        const auto cleaned = cleanupStaging(
            generationsDirectoryFd_, staging
        );
        return failure(QStringLiteral("generation.staging-create-failed"),
                       cleaned
                           ? QStringLiteral("Cannot create the generation module staging directory")
                           : QStringLiteral("Cannot create or safely clean the generation module staging directory"));
    }
    const auto modulesFd = ::openat(
        stagingFd, "modules",
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    bool written = modulesFd >= 0
        && privateDirectory(modulesFd, mutableDirectoryMode)
        && descriptorStillNamed(
            stagingFd, QByteArrayLiteral("modules"), modulesFd
        );
    for (auto iterator = rendered.files.constBegin();
         written && iterator != rendered.files.constEnd(); ++iterator) {
        const auto parts = iterator.key().split(QLatin1Char('/'));
        const auto destination = parts.size() == 1 ? stagingFd : modulesFd;
        const auto name = QFile::encodeName(parts.back());
        written = createImmutableFile(
            destination, name, QByteArrayView(iterator->contents)
        );
    }
    if (written) {
        written = createImmutableFile(
            stagingFd, QByteArrayLiteral("manifest.json"),
            QByteArrayView(rendered.manifestBytes)
        );
    }
    if (written) written = ::fchmod(modulesFd, immutableDirectoryMode) == 0
        && retryFsync(modulesFd)
        && ::fchmod(stagingFd, immutableDirectoryMode) == 0
        && retryFsync(stagingFd);
    if (modulesFd >= 0) ::close(modulesFd);
    ::close(stagingFd);
    if (!written) {
        const auto cleaned = cleanupStaging(
            generationsDirectoryFd_, staging
        );
        return failure(QStringLiteral("generation.write-failed"),
                       cleaned
                           ? QStringLiteral("Cannot durably write the immutable generation")
                           : QStringLiteral("Cannot durably write or safely clean the immutable generation"));
    }
    if (::renameat(generationsDirectoryFd_, staging.constData(),
                   generationsDirectoryFd_, nonce.constData()) != 0
        || !retryFsync(generationsDirectoryFd_)) {
        const auto cleaned = cleanupStaging(
            generationsDirectoryFd_, staging
        );
        return failure(QStringLiteral("generation.publish-failed"),
                       cleaned
                           ? QStringLiteral("Cannot atomically publish the immutable generation")
                           : QStringLiteral("Cannot atomically publish or safely clean the immutable generation"));
    }
    const auto verified = verify(rendered.activationNonce);
    if (!verified.success || !verified.generation
        || verified.generation->id != rendered.generation
        || verified.generation->manifest != rendered.manifestBytes) {
        return failure(QStringLiteral("generation.post-publish-verification-failed"),
                       QStringLiteral("The published generation failed exact verification"));
    }
    return verified;
}

GenerationResult GenerationStore::verify(const QString &nonce) const
{
    if (generationsDirectoryFd_ < 0 || !validNonce(nonce)
        || !store_.managedDirectoryStillNamed()
        || !descriptorStillNamed(
            store_.managedDirectoryFd(),
            QByteArrayLiteral("generations"),
            generationsDirectoryFd_
        )) {
        return failure(QStringLiteral("generation.invalid-reference"),
                       QStringLiteral("The generation reference is unavailable or invalid"));
    }
    const auto encodedNonce = nonce.toLatin1();
    const auto root = ::openat(
        generationsDirectoryFd_, encodedNonce.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (root < 0 || !privateDirectory(root, immutableDirectoryMode)
        || !descriptorStillNamed(
            generationsDirectoryFd_, encodedNonce, root
        )) {
        if (root >= 0) ::close(root);
        return failure(QStringLiteral("generation.unsafe-tree"),
                       QStringLiteral("The generation root is missing, mutable, or unsafe"));
    }
    const auto modules = ::openat(
        root, "modules", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (modules < 0 || !privateDirectory(modules, immutableDirectoryMode)
        || !descriptorStillNamed(
            root, QByteArrayLiteral("modules"), modules
        )) {
        if (modules >= 0) ::close(modules);
        ::close(root);
        return failure(QStringLiteral("generation.unsafe-tree"),
                       QStringLiteral("The generation module directory is missing, mutable, or unsafe"));
    }
    bool rootListed = false;
    bool modulesListed = false;
    const auto rootEntries = directoryEntries(root, rootListed);
    const auto moduleEntries = directoryEntries(modules, modulesListed);
    QSet<QString> expectedRoot{
        QStringLiteral("manifest.json"), QStringLiteral("hyprland.lua"),
        QStringLiteral("modules"),
    };
    QSet<QString> expectedModules;
    for (const auto &path : managedModulePaths()) {
        expectedModules.insert(path.section(QLatin1Char('/'), 1));
    }
    if (!rootListed || !modulesListed || rootEntries != expectedRoot
        || moduleEntries != expectedModules) {
        ::close(modules);
        ::close(root);
        return failure(QStringLiteral("generation.unexpected-tree-entry"),
                       QStringLiteral("The immutable tree does not have the exact managed file set"));
    }

    const auto manifestFile = readImmutableFile(
        root, QByteArrayLiteral("manifest.json"), maximumManifestBytes
    );
    if (!manifestFile.success) {
        ::close(modules);
        ::close(root);
        return failure(QStringLiteral("generation.invalid-manifest"),
                       QStringLiteral("The immutable generation manifest cannot be read safely"));
    }
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        QByteArrayView(manifestFile.bytes), maximumManifestBytes, 32
    );
    if (!parsed) {
        ::close(modules);
        ::close(root);
        return failure(QStringLiteral("generation.invalid-manifest"),
                       QStringLiteral("The immutable generation manifest is not strict JSON"));
    }
    const auto manifest = *parsed.value;
    auto canonicalManifest = Hyprland::JsonSupport::canonicalJson(manifest);
    canonicalManifest.append('\n');
    const QSet<QString> requiredKeys{
        QStringLiteral("formatVersion"), QStringLiteral("contractVersion"),
        QStringLiteral("generation"), QStringLiteral("snapshotDigest"),
        QStringLiteral("catalogDigest"), QStringLiteral("actionCatalogDigest"),
        QStringLiteral("revision"), QStringLiteral("targetHyprland"),
        QStringLiteral("compatibleHyprland"), QStringLiteral("rendererVersion"),
        QStringLiteral("activationNonce"), QStringLiteral("createdAt"),
        QStringLiteral("entrypoint"), QStringLiteral("files"),
    };
    QSet<QString> actualKeys;
    for (auto iterator = manifest.constBegin(); iterator != manifest.constEnd(); ++iterator) {
        actualKeys.insert(iterator.key());
    }
    const auto generation = manifest.value(QStringLiteral("generation")).toString();
    const auto snapshotDigest = manifest.value(
        QStringLiteral("snapshotDigest")
    ).toString();
    const auto catalogDigest = manifest.value(
        QStringLiteral("catalogDigest")
    ).toString();
    const auto actionCatalogDigest = manifest.value(
        QStringLiteral("actionCatalogDigest")
    ).toString();
    auto digestInput = manifest;
    digestInput.remove(QStringLiteral("generation"));
    quint64 revision = 0;
    const auto files = manifest.value(QStringLiteral("files"));
    if (actualKeys != requiredKeys
        || manifest.value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || manifest.value(QStringLiteral("contractVersion")).toInt(-1) != 1
        || manifest.value(QStringLiteral("rendererVersion")).toInt(-1)
            != static_cast<int>(currentRendererVersion)
        || manifest.value(QStringLiteral("activationNonce")).toString() != nonce
        || manifest.value(QStringLiteral("entrypoint")).toString()
            != QStringLiteral("hyprland.lua")
        || canonicalManifest != manifestFile.bytes
        || !files.isObject()
        || !validSha256(generation)
        || !validSha256(snapshotDigest)
        || !validSha256(catalogDigest)
        || !validSha256(actionCatalogDigest)
        || manifest.value(QStringLiteral("targetHyprland")).toString().isEmpty()
        || !validCreatedAt(
            manifest.value(QStringLiteral("createdAt")).toString()
        )
        || generation != hashBytes(
            Hyprland::JsonSupport::canonicalJson(digestInput)
        )
        || !parseRevision(
            manifest.value(QStringLiteral("revision")).toString(), revision
        )) {
        ::close(modules);
        ::close(root);
        return failure(QStringLiteral("generation.invalid-manifest"),
                       QStringLiteral("The immutable generation manifest violates its cross-field contract"));
    }

    const auto fileMap = files.toObject();
    QSet<QString> manifestPaths;
    bool payloadValid = true;
    for (auto iterator = fileMap.constBegin(); iterator != fileMap.constEnd(); ++iterator) {
        manifestPaths.insert(iterator.key());
        if (!iterator.value().isObject()) {
            payloadValid = false;
            break;
        }
        const auto metadata = iterator.value().toObject();
        if (metadata.size() != 2 || !metadata.contains(QStringLiteral("sha256"))
            || !metadata.contains(QStringLiteral("size"))) {
            payloadValid = false;
            break;
        }
        const auto parts = iterator.key().split(QLatin1Char('/'));
        if (parts.size() < 1 || parts.size() > 2
            || (parts.size() == 2 && parts.front() != QStringLiteral("modules"))) {
            payloadValid = false;
            break;
        }
        const auto sourceDirectory = parts.size() == 1 ? root : modules;
        const auto payload = readImmutableFile(
            sourceDirectory, QFile::encodeName(parts.back()),
            maximumGeneratedFileBytes
        );
        const auto expectedSize = metadata.value(QStringLiteral("size"));
        if (!payload.success || !expectedSize.isDouble()
            || expectedSize.toInteger(-1) != payload.bytes.size()
            || metadata.value(QStringLiteral("sha256")).toString()
                != hashBytes(payload.bytes)) {
            payloadValid = false;
            break;
        }
    }
    bool rootRelisted = false;
    bool modulesRelisted = false;
    const auto finalRootEntries = directoryEntries(root, rootRelisted);
    const auto finalModuleEntries = directoryEntries(modules, modulesRelisted);
    const auto treeStillNamed = descriptorStillNamed(
        generationsDirectoryFd_, encodedNonce, root
    ) && descriptorStillNamed(
        root, QByteArrayLiteral("modules"), modules
    );
    ::close(modules);
    ::close(root);
    if (!payloadValid || !treeStillNamed
        || !rootRelisted || !modulesRelisted
        || finalRootEntries != expectedRoot
        || finalModuleEntries != expectedModules
        || manifestPaths != expectedFileSet()) {
        return failure(QStringLiteral("generation.payload-mismatch"),
                       QStringLiteral("The immutable payload differs from its manifest"));
    }

    VerifiedGeneration verified{
        .id = generation,
        .nonce = nonce,
        .directory = directoryForNonce(nonce),
        .entrypoint = QDir(directoryForNonce(nonce)).filePath(
            QStringLiteral("hyprland.lua")
        ),
        .manifest = manifestFile.bytes,
        .snapshotDigest = manifest.value(QStringLiteral("snapshotDigest")).toString(),
        .revision = revision,
    };
    return {.success = true, .generation = std::move(verified)};
}

} // namespace HyprShelld::Compositor
