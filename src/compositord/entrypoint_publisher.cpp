#include "activation_backend.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QUuid>

#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <optional>
#include <utility>

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr qsizetype maximumEntrypointBytes = 16 * 1024 * 1024;
constexpr qsizetype maximumMetadataBytes = 4 * 1024 * 1024;
constexpr mode_t managedFileMode = 0600;
constexpr mode_t immutableFileMode = 0400;
constexpr mode_t immutableDirectoryMode = 0500;
constexpr auto stableName = "hyprland.lua";
constexpr auto generationsName = "generations";
constexpr auto ownershipName = "entrypoint-ownership.json";
constexpr auto bridgeName = "live-activation.pending.json";

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1U << 1)
#endif

[[nodiscard]] int renameAt2(
    const int oldDirectory,
    const char *oldName,
    const int newDirectory,
    const char *newName,
    const unsigned int flags
)
{
#if defined(SYS_renameat2)
    return static_cast<int>(::syscall(
        SYS_renameat2, oldDirectory, oldName, newDirectory, newName, flags
    ));
#else
    Q_UNUSED(oldDirectory)
    Q_UNUSED(oldName)
    Q_UNUSED(newDirectory)
    Q_UNUSED(newName)
    Q_UNUSED(flags)
    errno = ENOSYS;
    return -1;
#endif
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool validSha256(const QStringView value)
{
    if (value.size() != 64) return false;
    for (const auto character : value) {
        if (!((character >= u'0' && character <= u'9')
              || (character >= u'a' && character <= u'f'))) return false;
    }
    return true;
}

[[nodiscard]] bool validNonce(const QStringView value)
{
    if (value.size() < 32 || value.size() > 128) return false;
    for (const auto character : value) {
        if (!((character >= u'0' && character <= u'9')
              || (character >= u'a' && character <= u'f'))) return false;
    }
    return true;
}

[[nodiscard]] bool cleanAbsolute(const QString &path)
{
    return QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path;
}

[[nodiscard]] bool retryFsync(const int descriptor)
{
    while (::fsync(descriptor) != 0) {
        if (errno != EINTR) return false;
    }
    return true;
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
        if (count == 0) return false;
        offset += count;
    }
    return true;
}

[[nodiscard]] bool sameNode(
    const struct stat &left,
    const struct stat &right
)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino
        && left.st_mode == right.st_mode && left.st_uid == right.st_uid
        && left.st_nlink == right.st_nlink;
}

[[nodiscard]] bool sameFileIdentity(
    const struct stat &left,
    const struct stat &right
)
{
    return sameNode(left, right) && left.st_size == right.st_size
        && left.st_mtim.tv_sec == right.st_mtim.tv_sec
        && left.st_mtim.tv_nsec == right.st_mtim.tv_nsec
        && left.st_ctim.tv_sec == right.st_ctim.tv_sec
        && left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

[[nodiscard]] bool trustedDirectoryMetadata(
    const struct stat &metadata,
    const uid_t rootOwner
)
{
    const auto ownerTrusted = metadata.st_uid == ::geteuid()
        || metadata.st_uid == rootOwner;
    const auto writableByOthers =
        (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0;
    const auto protectedTemporary = metadata.st_uid == rootOwner
        && (metadata.st_mode & S_ISVTX) != 0;
    return S_ISDIR(metadata.st_mode) && ownerTrusted
        && (!writableByOthers || protectedTemporary);
}

[[nodiscard]] int openTrustedDirectoryTree(const QString &path)
{
    if (!cleanAbsolute(path)) return -1;
    auto current = ::open(
        "/", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY
    );
    if (current < 0) return -1;
    struct stat rootInfo {};
    if (::fstat(current, &rootInfo) != 0 || !S_ISDIR(rootInfo.st_mode)) {
        ::close(current);
        return -1;
    }
    const auto rootOwner = rootInfo.st_uid;
    for (const auto &component : path.split(
             QLatin1Char('/'), Qt::SkipEmptyParts
         )) {
        const auto name = QFile::encodeName(component);
        if (name.isEmpty() || name == "." || name == ".."
            || name.contains('/')) {
            ::close(current);
            return -1;
        }
        struct stat named {};
        if (::fstatat(current, name.constData(), &named,
                      AT_SYMLINK_NOFOLLOW) != 0
            || !trustedDirectoryMetadata(named, rootOwner)) {
            ::close(current);
            return -1;
        }
        const auto next = ::openat(
            current, name.constData(),
            O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY
        );
        struct stat opened {};
        if (next < 0 || ::fstat(next, &opened) != 0
            || !sameNode(named, opened)) {
            if (next >= 0) ::close(next);
            ::close(current);
            return -1;
        }
        ::close(current);
        current = next;
    }
    return current;
}

[[nodiscard]] bool canonicalDirectoryMatches(
    const QString &path,
    const int retained
)
{
    if (retained < 0) return false;
    const auto canonical = openTrustedDirectoryTree(path);
    if (canonical < 0) return false;
    struct stat expected {};
    struct stat current {};
    const auto matches = ::fstat(retained, &expected) == 0
        && ::fstat(canonical, &current) == 0
        && sameNode(expected, current);
    ::close(canonical);
    return matches;
}

[[nodiscard]] bool descriptorStillNamed(
    const int parent,
    const QByteArray &name,
    const int descriptor
)
{
    struct stat opened {};
    struct stat named {};
    return descriptor >= 0 && ::fstat(descriptor, &opened) == 0
        && ::fstatat(parent, name.constData(), &named,
                     AT_SYMLINK_NOFOLLOW) == 0
        && S_ISDIR(opened.st_mode) && S_ISDIR(named.st_mode)
        && sameNode(opened, named);
}

enum class SafeFileKind { Missing, Regular, Unsafe };

struct SafeFile final {
    SafeFileKind kind = SafeFileKind::Missing;
    QByteArray bytes;
    QString digest;
    mode_t mode = 0;
    struct stat info {};
};

[[nodiscard]] SafeFile readSafeFileAt(
    const int directory,
    const QByteArray &name,
    const qsizetype maximum,
    const std::optional<mode_t> exactMode = std::nullopt
)
{
    SafeFile result;
    struct stat named {};
    if (::fstatat(directory, name.constData(), &named,
                  AT_SYMLINK_NOFOLLOW) != 0) {
        if (errno != ENOENT) result.kind = SafeFileKind::Unsafe;
        return result;
    }
    if (!S_ISREG(named.st_mode) || named.st_uid != ::geteuid()
        || named.st_nlink != 1 || named.st_size < 0
        || named.st_size > maximum
        || (named.st_mode & (S_IWGRP | S_IWOTH | S_ISUID | S_ISGID
                            | S_ISVTX)) != 0
        || (exactMode && (named.st_mode & 0777) != *exactMode)) {
        result.kind = SafeFileKind::Unsafe;
        return result;
    }
    const auto descriptor = ::openat(
        directory, name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    );
    if (descriptor < 0) {
        result.kind = SafeFileKind::Unsafe;
        return result;
    }
    struct stat opened {};
    if (::fstat(descriptor, &opened) != 0 || !sameFileIdentity(named, opened)) {
        ::close(descriptor);
        result.kind = SafeFileKind::Unsafe;
        return result;
    }
    QByteArray bytes;
    bytes.reserve(static_cast<qsizetype>(opened.st_size));
    std::array<char, 64 * 1024> buffer {};
    while (true) {
        const auto count = ::read(descriptor, buffer.data(), buffer.size());
        if (count == 0) break;
        if (count < 0) {
            if (errno == EINTR) continue;
            ::close(descriptor);
            result.kind = SafeFileKind::Unsafe;
            return result;
        }
        if (bytes.size() > maximum - count) {
            ::close(descriptor);
            result.kind = SafeFileKind::Unsafe;
            return result;
        }
        bytes.append(buffer.data(), count);
    }
    struct stat after {};
    struct stat finalNamed {};
    const auto stable = ::fstat(descriptor, &after) == 0
        && sameFileIdentity(opened, after)
        && ::fstatat(directory, name.constData(), &finalNamed,
                     AT_SYMLINK_NOFOLLOW) == 0
        && sameFileIdentity(after, finalNamed)
        && bytes.size() == after.st_size;
    ::close(descriptor);
    if (!stable) {
        result.kind = SafeFileKind::Unsafe;
        return result;
    }
    result.kind = SafeFileKind::Regular;
    result.bytes = std::move(bytes);
    result.digest = sha256(result.bytes);
    result.mode = after.st_mode & 0777;
    result.info = after;
    return result;
}

[[nodiscard]] QByteArray canonicalObject(const QJsonObject &object)
{
    auto bytes = Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] std::optional<QJsonObject> parseCanonicalObject(
    const QByteArrayView bytes,
    const qsizetype maximum = maximumMetadataBytes
)
{
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        bytes, maximum, 32
    );
    if (!parsed || canonicalObject(*parsed.value) != bytes) return std::nullopt;
    return *parsed.value;
}

[[nodiscard]] QSet<QString> objectKeys(const QJsonObject &object)
{
    QSet<QString> result;
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) result.insert(iterator.key());
    return result;
}

[[nodiscard]] bool parseUnsigned(const QString &value, quint64 &result)
{
    if (value.isEmpty() || (value.size() > 1 && value.front() == u'0')) {
        return false;
    }
    for (const auto character : value) {
        if (character < u'0' || character > u'9') return false;
    }
    bool ok = false;
    result = value.toULongLong(&ok);
    return ok;
}

[[nodiscard]] QString decimal(const quint64 value)
{
    return QString::number(value);
}

[[nodiscard]] quint64 deviceOf(const struct stat &info)
{
    return static_cast<quint64>(info.st_dev);
}

[[nodiscard]] quint64 inodeOf(const struct stat &info)
{
    return static_cast<quint64>(info.st_ino);
}

[[nodiscard]] bool exactStoredIdentity(
    const SafeFile &file,
    const QStringView digest,
    const quint64 size,
    const quint64 device,
    const quint64 inode
)
{
    return file.kind == SafeFileKind::Regular && file.digest == digest
        && static_cast<quint64>(file.bytes.size()) == size
        && deviceOf(file.info) == device && inodeOf(file.info) == inode;
}

[[nodiscard]] ManagementStatus conflictStatus(const SafeFile &stable)
{
    return {
        .state = ManagementState::Conflict,
        .entrypointKind = stable.kind == SafeFileKind::Missing
            ? EntrypointKind::Absent
            : stable.kind == SafeFileKind::Regular
                ? EntrypointKind::Regular : EntrypointKind::Unsafe,
        .entrypointDigest = stable.kind == SafeFileKind::Regular
            ? stable.digest : QString(),
    };
}

[[nodiscard]] bool privateImmutableDirectory(const int descriptor)
{
    struct stat info {};
    return ::fstat(descriptor, &info) == 0 && S_ISDIR(info.st_mode)
        && info.st_uid == ::geteuid()
        && (info.st_mode & 0777) == immutableDirectoryMode;
}

[[nodiscard]] QSet<QString> directoryEntries(
    const int descriptor,
    bool &success
)
{
    success = false;
    QSet<QString> result;
    // dup()/F_DUPFD shares the open-file-description directory offset. The
    // verifier inventories each pinned directory twice, so using a duplicate
    // would make the anti-race recheck start at EOF. Opening "." creates an
    // independent description while remaining descriptor-relative.
    const auto reopened = ::openat(
        descriptor, ".",
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (reopened < 0) return result;
    auto *directory = ::fdopendir(reopened);
    if (!directory) {
        ::close(reopened);
        return result;
    }
    errno = 0;
    while (auto *entry = ::readdir(directory)) {
        const auto name = QByteArray(entry->d_name);
        if (name == "." || name == "..") continue;
        if (name.isEmpty() || name.contains('/') || name.contains('\0')) {
            ::closedir(directory);
            return {};
        }
        result.insert(QString::fromUtf8(name));
        errno = 0;
    }
    const auto readError = errno;
    ::closedir(directory);
    success = readError == 0;
    return result;
}

struct VerifiedTarget final {
    SafeFile entrypoint;
    QJsonObject manifest;
};

[[nodiscard]] std::optional<VerifiedTarget> verifyGeneration(
    const int generationsDirectory,
    const ActivationGeneration &prepared
)
{
    if (!validSha256(prepared.id) || !validNonce(prepared.nonce)) {
        return std::nullopt;
    }
    const auto nonce = prepared.nonce.toLatin1();
    const auto root = ::openat(
        generationsDirectory, nonce.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (root < 0 || !privateImmutableDirectory(root)
        || !descriptorStillNamed(generationsDirectory, nonce, root)) {
        if (root >= 0) ::close(root);
        return std::nullopt;
    }
    const auto modules = ::openat(
        root, "modules", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (modules < 0 || !privateImmutableDirectory(modules)
        || !descriptorStillNamed(root, QByteArrayLiteral("modules"), modules)) {
        if (modules >= 0) ::close(modules);
        ::close(root);
        return std::nullopt;
    }

    QSet<QString> expectedRoot {
        QStringLiteral("manifest.json"), QStringLiteral("hyprland.lua"),
        QStringLiteral("modules"),
    };
    QSet<QString> expectedModules;
    QSet<QString> expectedFiles {QStringLiteral("hyprland.lua")};
    for (const auto &path : managedModulePaths()) {
        expectedModules.insert(path.section(QLatin1Char('/'), 1));
        expectedFiles.insert(path);
    }
    bool rootListed = false;
    bool modulesListed = false;
    if (directoryEntries(root, rootListed) != expectedRoot
        || directoryEntries(modules, modulesListed) != expectedModules
        || !rootListed || !modulesListed) {
        ::close(modules);
        ::close(root);
        return std::nullopt;
    }

    const auto manifestFile = readSafeFileAt(
        root, QByteArrayLiteral("manifest.json"), maximumMetadataBytes,
        immutableFileMode
    );
    const auto manifest = manifestFile.kind == SafeFileKind::Regular
        ? parseCanonicalObject(manifestFile.bytes) : std::nullopt;
    static const QSet<QString> manifestKeys {
        QStringLiteral("formatVersion"), QStringLiteral("contractVersion"),
        QStringLiteral("generation"), QStringLiteral("snapshotDigest"),
        QStringLiteral("catalogDigest"),
        QStringLiteral("actionCatalogDigest"), QStringLiteral("revision"),
        QStringLiteral("targetHyprland"),
        QStringLiteral("compatibleHyprland"),
        QStringLiteral("rendererVersion"),
        QStringLiteral("activationNonce"), QStringLiteral("createdAt"),
        QStringLiteral("entrypoint"), QStringLiteral("files"),
    };
    quint64 revision = 0;
    auto digestInput = manifest.value_or(QJsonObject{});
    digestInput.remove(QStringLiteral("generation"));
    const auto validManifest = manifest
        && objectKeys(*manifest) == manifestKeys
        && manifestFile.bytes == prepared.manifest
        && manifest->value(QStringLiteral("formatVersion")).toInt(-1) == 1
        && manifest->value(QStringLiteral("contractVersion")).toInt(-1) == 1
        && manifest->value(QStringLiteral("rendererVersion")).toInt(-1)
            == static_cast<int>(currentRendererVersion)
        && manifest->value(QStringLiteral("generation")).toString()
            == prepared.id
        && manifest->value(QStringLiteral("activationNonce")).toString()
            == prepared.nonce
        && manifest->value(QStringLiteral("snapshotDigest")).toString()
            == prepared.snapshotDigest
        && manifest->value(QStringLiteral("entrypoint")).toString()
            == QStringLiteral("hyprland.lua")
        && parseUnsigned(
            manifest->value(QStringLiteral("revision")).toString(), revision
        )
        && revision == prepared.revision
        && sha256(Hyprland::JsonSupport::canonicalJson(digestInput))
            == prepared.id
        && manifest->value(QStringLiteral("files")).isObject();
    if (!validManifest) {
        ::close(modules);
        ::close(root);
        return std::nullopt;
    }

    const auto files = manifest->value(QStringLiteral("files")).toObject();
    if (objectKeys(files) != expectedFiles) {
        ::close(modules);
        ::close(root);
        return std::nullopt;
    }
    SafeFile entrypoint;
    QMap<QString, SafeFile> capturedPayloads;
    for (auto iterator = files.constBegin(); iterator != files.constEnd();
         ++iterator) {
        if (!iterator.value().isObject()) {
            ::close(modules);
            ::close(root);
            return std::nullopt;
        }
        const auto metadata = iterator.value().toObject();
        static const QSet<QString> fileKeys {
            QStringLiteral("sha256"), QStringLiteral("size"),
        };
        const auto parts = iterator.key().split(QLatin1Char('/'));
        const auto validPath = parts.size() == 1
            || (parts.size() == 2 && parts.front() == QStringLiteral("modules"));
        if (objectKeys(metadata) != fileKeys || !validPath) {
            ::close(modules);
            ::close(root);
            return std::nullopt;
        }
        const auto sourceDirectory = parts.size() == 1 ? root : modules;
        const auto payload = readSafeFileAt(
            sourceDirectory, QFile::encodeName(parts.back()),
            maximumEntrypointBytes, immutableFileMode
        );
        if (payload.kind != SafeFileKind::Regular
            || metadata.value(QStringLiteral("sha256")).toString()
                != payload.digest
            || !metadata.value(QStringLiteral("size")).isDouble()
            || metadata.value(QStringLiteral("size")).toInteger(-1)
                != payload.bytes.size()) {
            ::close(modules);
            ::close(root);
            return std::nullopt;
        }
        if (iterator.key() == QStringLiteral("hyprland.lua")) {
            entrypoint = payload;
        }
        capturedPayloads.insert(iterator.key(), payload);
    }
    bool finalRootListed = false;
    bool finalModulesListed = false;
    auto payloadsStillExact = true;
    for (auto iterator = capturedPayloads.constBegin();
         iterator != capturedPayloads.constEnd(); ++iterator) {
        const auto parts = iterator.key().split(QLatin1Char('/'));
        const auto sourceDirectory = parts.size() == 1 ? root : modules;
        const auto current = readSafeFileAt(
            sourceDirectory, QFile::encodeName(parts.back()),
            maximumEntrypointBytes, immutableFileMode
        );
        if (current.kind != SafeFileKind::Regular
            || current.bytes != iterator->bytes
            || !sameFileIdentity(current.info, iterator->info)) {
            payloadsStillExact = false;
            break;
        }
    }
    const auto stillExact = entrypoint.kind == SafeFileKind::Regular
        && payloadsStillExact
        && descriptorStillNamed(generationsDirectory, nonce, root)
        && descriptorStillNamed(root, QByteArrayLiteral("modules"), modules)
        && directoryEntries(root, finalRootListed) == expectedRoot
        && directoryEntries(modules, finalModulesListed) == expectedModules
        && finalRootListed && finalModulesListed;
    ::close(modules);
    ::close(root);
    if (!stillExact) return std::nullopt;
    return VerifiedTarget{.entrypoint = std::move(entrypoint),
                          .manifest = *manifest};
}

struct OriginalRecord final {
    bool absent = true;
    QString digest;
    quint64 size = 0;
    mode_t mode = 0;
    quint64 device = 0;
    quint64 inode = 0;
    QString backupName;
};

struct OwnershipRecord final {
    QString generation;
    QString nonce;
    QString entrypointDigest;
    quint64 entrypointSize = 0;
    quint64 entrypointDevice = 0;
    quint64 entrypointInode = 0;
    OriginalRecord original;
};

[[nodiscard]] QByteArray ownershipBytes(const OwnershipRecord &record)
{
    return canonicalObject({
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("generation"), record.generation},
        {QStringLiteral("activationNonce"), record.nonce},
        {QStringLiteral("entrypointDigest"), record.entrypointDigest},
        {QStringLiteral("entrypointSize"), decimal(record.entrypointSize)},
        {QStringLiteral("entrypointDevice"), decimal(record.entrypointDevice)},
        {QStringLiteral("entrypointInode"), decimal(record.entrypointInode)},
        {QStringLiteral("originalKind"), record.original.absent
             ? QStringLiteral("absent") : QStringLiteral("regular")},
        {QStringLiteral("originalDigest"), record.original.digest},
        {QStringLiteral("originalSize"), decimal(record.original.size)},
        {QStringLiteral("originalMode"),
         static_cast<qint64>(record.original.mode)},
        {QStringLiteral("originalDevice"), decimal(record.original.device)},
        {QStringLiteral("originalInode"), decimal(record.original.inode)},
        {QStringLiteral("originalBackup"), record.original.backupName},
    });
}

[[nodiscard]] bool validOriginalBackupName(const QString &name)
{
    constexpr auto prefix = ".hyprshelld-original-";
    constexpr auto suffix = ".lua";
    return name.startsWith(QLatin1StringView(prefix))
        && name.endsWith(QLatin1StringView(suffix))
        && validNonce(QStringView(name).mid(
            static_cast<qsizetype>(std::strlen(prefix)),
            name.size() - static_cast<qsizetype>(std::strlen(prefix))
                - static_cast<qsizetype>(std::strlen(suffix))
        ));
}

[[nodiscard]] std::optional<OwnershipRecord> parseOwnership(
    const QByteArrayView bytes
)
{
    const auto parsed = parseCanonicalObject(bytes);
    if (!parsed) return std::nullopt;
    static const QSet<QString> expected {
        QStringLiteral("formatVersion"), QStringLiteral("generation"),
        QStringLiteral("activationNonce"),
        QStringLiteral("entrypointDigest"),
        QStringLiteral("entrypointSize"),
        QStringLiteral("entrypointDevice"),
        QStringLiteral("entrypointInode"), QStringLiteral("originalKind"),
        QStringLiteral("originalDigest"), QStringLiteral("originalSize"),
        QStringLiteral("originalMode"), QStringLiteral("originalDevice"),
        QStringLiteral("originalInode"),
        QStringLiteral("originalBackup"),
    };
    OwnershipRecord record;
    record.generation = parsed->value(QStringLiteral("generation")).toString();
    record.nonce = parsed->value(
        QStringLiteral("activationNonce")
    ).toString();
    record.entrypointDigest = parsed->value(
        QStringLiteral("entrypointDigest")
    ).toString();
    record.original.digest = parsed->value(
        QStringLiteral("originalDigest")
    ).toString();
    record.original.backupName = parsed->value(
        QStringLiteral("originalBackup")
    ).toString();
    record.original.absent = parsed->value(
        QStringLiteral("originalKind")
    ).toString() == QStringLiteral("absent");
    const auto originalKind = parsed->value(
        QStringLiteral("originalKind")
    ).toString();
    const auto originalMode = parsed->value(
        QStringLiteral("originalMode")
    );
    record.original.mode = static_cast<mode_t>(originalMode.toInteger(-1));
    if (objectKeys(*parsed) != expected
        || parsed->value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || !validSha256(record.generation) || !validNonce(record.nonce)
        || !validSha256(record.entrypointDigest)
        || !parseUnsigned(
            parsed->value(QStringLiteral("entrypointSize")).toString(),
            record.entrypointSize
        )
        || !parseUnsigned(
            parsed->value(QStringLiteral("entrypointDevice")).toString(),
            record.entrypointDevice
        )
        || !parseUnsigned(
            parsed->value(QStringLiteral("entrypointInode")).toString(),
            record.entrypointInode
        )
        || !parseUnsigned(
            parsed->value(QStringLiteral("originalSize")).toString(),
            record.original.size
        )
        || !parseUnsigned(
            parsed->value(QStringLiteral("originalDevice")).toString(),
            record.original.device
        )
        || !parseUnsigned(
            parsed->value(QStringLiteral("originalInode")).toString(),
            record.original.inode
        )
        || !originalMode.isDouble()
        || (originalKind != QStringLiteral("absent")
            && originalKind != QStringLiteral("regular"))
        || record.entrypointSize > maximumEntrypointBytes
        || record.entrypointDevice == 0 || record.entrypointInode == 0
        || (record.original.absent
            && (!record.original.digest.isEmpty()
                || record.original.size != 0 || record.original.mode != 0
                || record.original.device != 0 || record.original.inode != 0
                || !record.original.backupName.isEmpty()))
        || (!record.original.absent
            && (!validSha256(record.original.digest)
                || record.original.size > maximumEntrypointBytes
                || record.original.mode > 0777
                || (record.original.mode & (S_IWGRP | S_IWOTH)) != 0
                || record.original.device == 0 || record.original.inode == 0
                || !validOriginalBackupName(record.original.backupName)))) {
        return std::nullopt;
    }
    return record;
}

enum class BridgePhase { Staging, Ready };

struct BridgeRecord final {
    BridgePhase phase = BridgePhase::Staging;
    QString token;
    bool adoption = false;
    QString targetGeneration;
    QString targetNonce;
    QString targetDigest;
    quint64 targetSize = 0;
    quint64 targetDevice = 0;
    quint64 targetInode = 0;
    QString swapName;
    bool beforeAbsent = true;
    QString beforeDigest;
    quint64 beforeSize = 0;
    mode_t beforeMode = 0;
    quint64 beforeDevice = 0;
    quint64 beforeInode = 0;
    QString beforeGeneration;
    QString beforeNonce;
    QByteArray beforeOwnership;
    QByteArray baselineConfigErrors;
    QString baselineProvider;
};

[[nodiscard]] QByteArray bridgeBytes(const BridgeRecord &record)
{
    return canonicalObject({
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("phase"), record.phase == BridgePhase::Staging
             ? QStringLiteral("staging") : QStringLiteral("ready")},
        {QStringLiteral("token"), record.token},
        {QStringLiteral("adoption"), record.adoption},
        {QStringLiteral("targetGeneration"), record.targetGeneration},
        {QStringLiteral("targetNonce"), record.targetNonce},
        {QStringLiteral("targetDigest"), record.targetDigest},
        {QStringLiteral("targetSize"), decimal(record.targetSize)},
        {QStringLiteral("targetDevice"), decimal(record.targetDevice)},
        {QStringLiteral("targetInode"), decimal(record.targetInode)},
        {QStringLiteral("swapName"), record.swapName},
        {QStringLiteral("beforeKind"), record.beforeAbsent
             ? QStringLiteral("absent") : QStringLiteral("regular")},
        {QStringLiteral("beforeDigest"), record.beforeDigest},
        {QStringLiteral("beforeSize"), decimal(record.beforeSize)},
        {QStringLiteral("beforeMode"), static_cast<qint64>(record.beforeMode)},
        {QStringLiteral("beforeDevice"), decimal(record.beforeDevice)},
        {QStringLiteral("beforeInode"), decimal(record.beforeInode)},
        {QStringLiteral("beforeGeneration"), record.beforeGeneration},
        {QStringLiteral("beforeNonce"), record.beforeNonce},
        {QStringLiteral("beforeOwnership"),
         QString::fromLatin1(record.beforeOwnership.toBase64())},
        {QStringLiteral("baselineConfigErrors"),
         QString::fromLatin1(record.baselineConfigErrors.toBase64())},
        {QStringLiteral("baselineProvider"), record.baselineProvider},
    });
}

[[nodiscard]] bool exactSwapName(
    const QString &name,
    const QString &token,
    const bool adoption
)
{
    return name == QStringLiteral(".hyprshelld-%1-%2.lua")
        .arg(adoption ? QStringLiteral("original")
                      : QStringLiteral("transition"), token);
}

[[nodiscard]] std::optional<BridgeRecord> parseBridge(
    const QByteArrayView bytes
)
{
    const auto parsed = parseCanonicalObject(bytes);
    if (!parsed) return std::nullopt;
    static const QSet<QString> expected {
        QStringLiteral("formatVersion"), QStringLiteral("phase"),
        QStringLiteral("token"), QStringLiteral("adoption"),
        QStringLiteral("targetGeneration"), QStringLiteral("targetNonce"),
        QStringLiteral("targetDigest"), QStringLiteral("targetSize"),
        QStringLiteral("targetDevice"), QStringLiteral("targetInode"),
        QStringLiteral("swapName"), QStringLiteral("beforeKind"),
        QStringLiteral("beforeDigest"), QStringLiteral("beforeSize"),
        QStringLiteral("beforeMode"), QStringLiteral("beforeDevice"),
        QStringLiteral("beforeInode"), QStringLiteral("beforeGeneration"),
        QStringLiteral("beforeNonce"), QStringLiteral("beforeOwnership"),
        QStringLiteral("baselineConfigErrors"),
        QStringLiteral("baselineProvider"),
    };
    BridgeRecord record;
    const auto phase = parsed->value(QStringLiteral("phase")).toString();
    record.phase = phase == QStringLiteral("ready")
        ? BridgePhase::Ready : BridgePhase::Staging;
    record.token = parsed->value(QStringLiteral("token")).toString();
    record.adoption = parsed->value(QStringLiteral("adoption")).toBool();
    record.targetGeneration = parsed->value(
        QStringLiteral("targetGeneration")
    ).toString();
    record.targetNonce = parsed->value(
        QStringLiteral("targetNonce")
    ).toString();
    record.targetDigest = parsed->value(
        QStringLiteral("targetDigest")
    ).toString();
    record.swapName = parsed->value(QStringLiteral("swapName")).toString();
    const auto beforeKind = parsed->value(
        QStringLiteral("beforeKind")
    ).toString();
    record.beforeAbsent = beforeKind == QStringLiteral("absent");
    record.beforeDigest = parsed->value(
        QStringLiteral("beforeDigest")
    ).toString();
    record.beforeGeneration = parsed->value(
        QStringLiteral("beforeGeneration")
    ).toString();
    record.beforeNonce = parsed->value(
        QStringLiteral("beforeNonce")
    ).toString();
    record.baselineProvider = parsed->value(
        QStringLiteral("baselineProvider")
    ).toString();
    const auto beforeMode = parsed->value(QStringLiteral("beforeMode"));
    record.beforeMode = static_cast<mode_t>(beforeMode.toInteger(-1));
    record.beforeOwnership = QByteArray::fromBase64(
        parsed->value(QStringLiteral("beforeOwnership")).toString().toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors
    );
    record.baselineConfigErrors = QByteArray::fromBase64(
        parsed->value(QStringLiteral("baselineConfigErrors"))
            .toString().toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors
    );
    const auto adoptionValue = parsed->value(QStringLiteral("adoption"));
    const auto encodedOwnership = parsed->value(
        QStringLiteral("beforeOwnership")
    ).toString().toLatin1();
    const auto encodedErrors = parsed->value(
        QStringLiteral("baselineConfigErrors")
    ).toString().toLatin1();
    const auto numeric = [&](const char *name, quint64 &destination) {
        return parseUnsigned(
            parsed->value(QString::fromLatin1(name)).toString(), destination
        );
    };
    if (objectKeys(*parsed) != expected
        || parsed->value(QStringLiteral("formatVersion")).toInt(-1) != 1
        || (phase != QStringLiteral("staging")
            && phase != QStringLiteral("ready"))
        || !adoptionValue.isBool() || !validNonce(record.token)
        || record.beforeOwnership.toBase64() != encodedOwnership
        || record.baselineConfigErrors.toBase64() != encodedErrors
        || !validSha256(record.targetGeneration)
        || !validNonce(record.targetNonce)
        || !validSha256(record.targetDigest)
        || !numeric("targetSize", record.targetSize)
        || !numeric("targetDevice", record.targetDevice)
        || !numeric("targetInode", record.targetInode)
        || !numeric("beforeSize", record.beforeSize)
        || !numeric("beforeDevice", record.beforeDevice)
        || !numeric("beforeInode", record.beforeInode)
        || !beforeMode.isDouble()
        || record.targetSize > maximumEntrypointBytes
        || !exactSwapName(record.swapName, record.token, record.adoption)
        || (beforeKind != QStringLiteral("absent")
            && beforeKind != QStringLiteral("regular"))
        || (record.phase == BridgePhase::Staging
            && (record.targetDevice != 0 || record.targetInode != 0))
        || (record.phase == BridgePhase::Ready
            && (record.targetDevice == 0 || record.targetInode == 0))
        || (record.beforeAbsent
            && (!record.beforeDigest.isEmpty() || record.beforeSize != 0
                || record.beforeMode != 0 || record.beforeDevice != 0
                || record.beforeInode != 0))
        || (!record.beforeAbsent
            && (!validSha256(record.beforeDigest)
                || record.beforeSize > maximumEntrypointBytes
                || record.beforeMode > 0777
                || (record.beforeMode & (S_IWGRP | S_IWOTH)) != 0
                || record.beforeDevice == 0 || record.beforeInode == 0))
        || record.baselineConfigErrors != QByteArrayLiteral("[]")
        || (record.baselineProvider != QStringLiteral("lua")
            && record.baselineProvider != QStringLiteral("hyprlang"))
        || (record.adoption
            && (record.beforeAbsent
                    ? record.baselineProvider != QStringLiteral("hyprlang")
                    : record.baselineProvider != QStringLiteral("lua")))
        || (!record.adoption
            && (record.beforeAbsent
                || record.baselineProvider != QStringLiteral("lua")))) {
        return std::nullopt;
    }
    if (record.adoption) {
        if (!record.beforeGeneration.isEmpty()
            || !record.beforeNonce.isEmpty()
            || !record.beforeOwnership.isEmpty()) return std::nullopt;
    } else {
        const auto ownership = parseOwnership(record.beforeOwnership);
        if (!ownership || record.beforeGeneration != ownership->generation
            || record.beforeNonce != ownership->nonce
            || ownership->entrypointDigest != record.beforeDigest
            || ownership->entrypointSize != record.beforeSize
            || ownership->entrypointDevice != record.beforeDevice
            || ownership->entrypointInode != record.beforeInode) {
            return std::nullopt;
        }
    }
    return record;
}

[[nodiscard]] QString readyTemporaryName(const QString &token)
{
    return QStringLiteral(".hyprshelld-ready-journal-%1").arg(token);
}

[[nodiscard]] QString ownershipTemporaryName(const QString &token)
{
    return QStringLiteral(".hyprshelld-ownership-%1").arg(token);
}

[[nodiscard]] bool faulted(
    const std::function<bool(EntrypointFaultPoint)> &hook,
    const EntrypointFaultPoint point
)
{
    return hook && hook(point);
}

[[nodiscard]] bool removeAndSync(
    const int directory,
    const QByteArray &name,
    const bool missingAllowed,
    const std::function<bool()> &rootsGuard = {}
)
{
    if (rootsGuard && !rootsGuard()) return false;
    if (::unlinkat(directory, name.constData(), 0) != 0
        && !(missingAllowed && errno == ENOENT)) return false;
    return (!rootsGuard || rootsGuard()) && retryFsync(directory)
        && (!rootsGuard || rootsGuard());
}

struct CompleteInstallResult final {
    bool success = false;
    bool newlyInstalled = false;
};

[[nodiscard]] bool syncExactFileAt(
    const int directory,
    const QByteArray &name,
    const SafeFile &captured,
    const std::function<bool()> &rootsGuard
)
{
    if (!rootsGuard()) return false;
    const auto descriptor = ::openat(
        directory, name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    );
    struct stat before {};
    struct stat after {};
    struct stat named {};
    const auto exact = descriptor >= 0
        && ::fstat(descriptor, &before) == 0
        && sameFileIdentity(captured.info, before)
        && retryFsync(descriptor)
        && ::fstat(descriptor, &after) == 0
        && ::fstatat(directory, name.constData(), &named,
                     AT_SYMLINK_NOFOLLOW) == 0
        && sameFileIdentity(before, after)
        && sameFileIdentity(after, named);
    if (descriptor >= 0) ::close(descriptor);
    return exact && rootsGuard();
}

[[nodiscard]] QByteArray randomScratchName()
{
    auto token = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    token.replace("-", "");
    return QByteArrayLiteral(".hyprshelld-scratch-") + token;
}

[[nodiscard]] CompleteInstallResult installCompleteNoReplace(
    const int directory,
    const QByteArray &name,
    const QByteArrayView bytes,
    const qsizetype maximumBytes,
    const mode_t mode,
    const std::function<bool(EntrypointFaultPoint)> &hook,
    const std::optional<EntrypointFaultPoint> beforeDirectorySync,
    const std::function<bool()> &rootsGuard
)
{
    CompleteInstallResult result;
    if (!rootsGuard()) return result;
    const auto existing = readSafeFileAt(
        directory, name, maximumBytes, mode
    );
    if (existing.kind == SafeFileKind::Regular) {
        if (existing.bytes == bytes
            && syncExactFileAt(directory, name, existing, rootsGuard)
            && retryFsync(directory) && rootsGuard()) {
            result.success = true;
        }
        return result;
    }
    if (existing.kind != SafeFileKind::Missing) return result;

    QByteArray scratch;
    int descriptor = -1;
    for (int attempt = 0; attempt < 8 && descriptor < 0; ++attempt) {
        scratch = randomScratchName();
        descriptor = ::openat(
            directory, scratch.constData(),
            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, mode
        );
        if (descriptor < 0 && errno != EEXIST) return result;
    }
    if (descriptor < 0) return result;
    const auto written = writeAll(descriptor, bytes)
        && ::fchmod(descriptor, mode) == 0 && retryFsync(descriptor);
    ::close(descriptor);
    if (!written) {
        const auto stillNamed = rootsGuard();
        const auto removed = stillNamed
            && ::unlinkat(directory, scratch.constData(), 0) == 0;
        const auto cleanupDurable = removed && retryFsync(directory)
            && rootsGuard();
        if (!cleanupDurable) errno = EIO;
        return result;
    }
    if (!rootsGuard()) return result;

    if (renameAt2(
            directory, scratch.constData(), directory, name.constData(),
            RENAME_NOREPLACE
        ) != 0) {
        const auto raced = readSafeFileAt(
            directory, name, maximumBytes, mode
        );
        const auto resumable = raced.kind == SafeFileKind::Regular
            && raced.bytes == bytes
            && syncExactFileAt(directory, name, raced, rootsGuard);
        if (rootsGuard()) {
            const auto removed = ::unlinkat(
                directory, scratch.constData(), 0
            ) == 0;
            const auto cleanupDurable = removed && retryFsync(directory)
                && rootsGuard();
            if (removed && !cleanupDurable) errno = EIO;
        }
        if (resumable && rootsGuard() && retryFsync(directory)
            && rootsGuard()) result.success = true;
        return result;
    }
    result.newlyInstalled = true;
    if ((beforeDirectorySync && faulted(hook, *beforeDirectorySync))
        || !rootsGuard() || !retryFsync(directory) || !rootsGuard()) {
        return result;
    }
    result.success = true;
    return result;
}

struct AtomicPublishResult final {
    bool success = false;
    bool namespaceChanged = false;
};

[[nodiscard]] bool prepareAtomicTemporary(
    const int directory,
    const QByteArray &name,
    const QByteArrayView bytes,
    const mode_t mode,
    const std::function<bool()> &rootsGuard
)
{
    return installCompleteNoReplace(
        directory, name, bytes, maximumMetadataBytes, mode, {}, std::nullopt,
        rootsGuard
    ).success;
}

void cleanPreparedTemporary(
    const int directory,
    const QByteArray &name,
    const std::function<bool()> &rootsGuard
)
{
    if (!rootsGuard()) return;
    const auto removed = ::unlinkat(directory, name.constData(), 0) == 0;
    // Failure remains conservative: the exact temp may survive and will be
    // resumed on the next receipt-bound attempt.
    const auto durable = removed && retryFsync(directory) && rootsGuard();
    if (removed && !durable) errno = EIO;
}

[[nodiscard]] AtomicPublishResult publishNoReplace(
    const int directory,
    const QByteArray &temporary,
    const QByteArray &destination,
    const QByteArrayView bytes,
    const mode_t mode,
    const std::function<bool(EntrypointFaultPoint)> &hook,
    const EntrypointFaultPoint beforeRename,
    const EntrypointFaultPoint afterRename,
    const std::function<bool()> &rootsGuard
)
{
    AtomicPublishResult result;
    if (!prepareAtomicTemporary(
            directory, temporary, bytes, mode, rootsGuard
        )) return result;
    const auto beforeFault = faulted(hook, beforeRename);
    const auto rootsExact = rootsGuard();
    if (beforeFault || !rootsExact) {
        if (rootsExact) cleanPreparedTemporary(
            directory, temporary, rootsGuard
        );
        return result;
    }
    if (renameAt2(directory, temporary.constData(), directory,
                  destination.constData(), RENAME_NOREPLACE) != 0) {
        if (rootsGuard()) cleanPreparedTemporary(
            directory, temporary, rootsGuard
        );
        return result;
    }
    result.namespaceChanged = true;
    if (faulted(hook, afterRename) || !rootsGuard()
        || !retryFsync(directory) || !rootsGuard()) return result;
    result.success = true;
    return result;
}

[[nodiscard]] AtomicPublishResult replaceExact(
    const int directory,
    const QByteArray &temporary,
    const QByteArray &destination,
    const QByteArrayView replacement,
    const QByteArrayView expected,
    const SafeFile &capturedExpected,
    const mode_t mode,
    const std::function<bool(EntrypointFaultPoint)> &hook,
    const EntrypointFaultPoint beforeRename,
    const EntrypointFaultPoint afterRename,
    const std::function<bool()> &rootsGuard
)
{
    AtomicPublishResult result;
    if (!prepareAtomicTemporary(
            directory, temporary, replacement, mode, rootsGuard
        )) return result;
    const auto beforeFault = faulted(hook, beforeRename);
    const auto rootsExact = rootsGuard();
    if (beforeFault || !rootsExact) {
        if (rootsExact) cleanPreparedTemporary(
            directory, temporary, rootsGuard
        );
        return result;
    }
    if (renameAt2(directory, temporary.constData(), directory,
                  destination.constData(), RENAME_EXCHANGE) != 0) {
        if (rootsGuard()) cleanPreparedTemporary(
            directory, temporary, rootsGuard
        );
        return result;
    }
    result.namespaceChanged = true;
    const auto displaced = readSafeFileAt(
        directory, temporary, maximumMetadataBytes, capturedExpected.mode
    );
    const auto exact = displaced.kind == SafeFileKind::Regular
        && displaced.bytes == expected
        // RENAME_EXCHANGE legitimately updates inode timestamps. Bind the
        // displaced object by its stable namespace identity; readSafeFileAt
        // already proved exact mode, size, bytes, and an internally stable
        // read snapshot.
        && sameNode(displaced.info, capturedExpected.info)
        && displaced.info.st_size == capturedExpected.info.st_size;
    if (!exact) {
        // Restore the object we actually displaced. Failure is intentionally
        // reported as uncertain; the durable bridge remains authoritative.
        const auto restored = rootsGuard()
            && renameAt2(
                directory, temporary.constData(), directory,
                destination.constData(), RENAME_EXCHANGE
            ) == 0;
        if (restored) static_cast<void>(retryFsync(directory));
        return result;
    }
    if (faulted(hook, afterRename) || !rootsGuard()
        || !retryFsync(directory) || !rootsGuard()) return result;
    if (!rootsGuard()
        || ::unlinkat(directory, temporary.constData(), 0) != 0
        || !retryFsync(directory) || !rootsGuard()) return result;
    result.success = true;
    return result;
}

[[nodiscard]] bool removeBridgeDurably(
    const int managedDirectory,
    const std::function<bool(EntrypointFaultPoint)> &hook,
    const std::function<bool()> &rootsGuard
)
{
    if (faulted(hook, EntrypointFaultPoint::BeforeBridgeRemoval)
        || !rootsGuard()) return false;
    if (::unlinkat(managedDirectory, bridgeName, 0) != 0) return false;
    if (faulted(
            hook,
            EntrypointFaultPoint::AfterBridgeRemovalBeforeDirectorySync
        ) || !rootsGuard() || !retryFsync(managedDirectory)) return false;
    return rootsGuard();
}

[[nodiscard]] bool exactOwnershipState(
    const SafeFile &stored,
    const QByteArrayView expected
)
{
    return expected.empty()
        ? stored.kind == SafeFileKind::Missing
        : stored.kind == SafeFileKind::Regular && stored.bytes == expected
            && parseOwnership(stored.bytes).has_value();
}

[[nodiscard]] AtomicPublishResult publishOwnershipExact(
    const int managedDirectory,
    const QString &token,
    const QByteArrayView replacement,
    const QByteArrayView expected,
    const std::function<bool(EntrypointFaultPoint)> &hook,
    const std::function<bool()> &rootsGuard
)
{
    const auto current = readSafeFileAt(
        managedDirectory, QByteArrayLiteral(ownershipName),
        maximumMetadataBytes, managedFileMode
    );
    if (current.kind == SafeFileKind::Regular
        && current.bytes == replacement
        && parseOwnership(current.bytes)) {
        return {.success = true};
    }
    if (!exactOwnershipState(current, expected)) return {};
    const auto temporary = QFile::encodeName(ownershipTemporaryName(token));
    if (expected.empty()) {
        return publishNoReplace(
            managedDirectory, temporary, QByteArrayLiteral(ownershipName),
            replacement, managedFileMode, hook,
            EntrypointFaultPoint::BeforeOwnershipRename,
            EntrypointFaultPoint::AfterOwnershipRenameBeforeDirectorySync,
            rootsGuard
        );
    }
    return replaceExact(
        managedDirectory, temporary, QByteArrayLiteral(ownershipName),
        replacement, expected, current, managedFileMode, hook,
        EntrypointFaultPoint::BeforeOwnershipRename,
        EntrypointFaultPoint::AfterOwnershipRenameBeforeDirectorySync,
        rootsGuard
    );
}

[[nodiscard]] bool removeOwnershipTemporary(
    const int managedDirectory,
    const QString &token,
    const QByteArrayView expected,
    const std::function<bool()> &rootsGuard
)
{
    const auto name = QFile::encodeName(ownershipTemporaryName(token));
    const auto file = readSafeFileAt(
        managedDirectory, name, maximumMetadataBytes, managedFileMode
    );
    if (file.kind == SafeFileKind::Unsafe) return false;
    if (file.kind == SafeFileKind::Missing) return true;
    if (expected.empty() || file.bytes != expected) return false;
    return removeAndSync(managedDirectory, name, false, rootsGuard);
}

[[nodiscard]] bool removeReadyTemporary(
    const int managedDirectory,
    const QString &token,
    const QByteArrayView expectedStaging,
    const QByteArrayView expectedReady,
    const std::function<bool()> &rootsGuard
)
{
    const auto name = QFile::encodeName(readyTemporaryName(token));
    const auto file = readSafeFileAt(
        managedDirectory, name, maximumMetadataBytes, managedFileMode
    );
    if (file.kind == SafeFileKind::Unsafe) return false;
    if (file.kind == SafeFileKind::Missing) return true;
    // Before the Ready-journal exchange the temp contains the exact Ready
    // replacement; after exchange it contains the exact displaced Staging
    // journal. Both are receipt-bound crash states. No other bytes may be
    // removed from the predictable temp namespace.
    if (file.bytes != expectedStaging
        && (expectedReady.empty() || file.bytes != expectedReady)) {
        return false;
    }
    return removeAndSync(managedDirectory, name, false, rootsGuard);
}

[[nodiscard]] bool exactBefore(
    const BridgeRecord &bridge,
    const SafeFile &stable
)
{
    return bridge.beforeAbsent
        ? stable.kind == SafeFileKind::Missing
        : exactStoredIdentity(
            stable, bridge.beforeDigest, bridge.beforeSize,
            bridge.beforeDevice, bridge.beforeInode
        ) && stable.mode == bridge.beforeMode;
}

[[nodiscard]] bool exactTarget(
    const BridgeRecord &bridge,
    const SafeFile &stable
)
{
    return bridge.phase == BridgePhase::Ready
        && exactStoredIdentity(
            stable, bridge.targetDigest, bridge.targetSize,
            bridge.targetDevice, bridge.targetInode
        ) && stable.mode == managedFileMode;
}

[[nodiscard]] bool exactStagedTarget(
    const BridgeRecord &bridge,
    const SafeFile &file
)
{
    if (file.kind != SafeFileKind::Regular
        || file.digest != bridge.targetDigest
        || static_cast<quint64>(file.bytes.size()) != bridge.targetSize
        || file.mode != managedFileMode) {
        return false;
    }
    return bridge.phase == BridgePhase::Staging
        || (deviceOf(file.info) == bridge.targetDevice
            && inodeOf(file.info) == bridge.targetInode);
}

[[nodiscard]] ManagementStatus statusForBridgeSide(
    const BridgeRecord &bridge,
    const SafeFile &stable,
    const bool target
)
{
    if (target) {
        return {
            .state = ManagementState::Managed,
            .entrypointKind = EntrypointKind::Regular,
            .entrypointDigest = stable.digest,
            .managedGeneration = bridge.targetGeneration,
            .managedNonce = bridge.targetNonce,
        };
    }
    return {
        .state = bridge.beforeGeneration.isEmpty()
            ? ManagementState::Unmanaged : ManagementState::Managed,
        .entrypointKind = bridge.beforeAbsent
            ? EntrypointKind::Absent : EntrypointKind::Regular,
        .entrypointDigest = stable.digest,
        .managedGeneration = bridge.beforeGeneration,
        .managedNonce = bridge.beforeNonce,
    };
}

} // namespace

struct AtomicEntrypointPublisher::Impl final {
    QString expectedStateRoot;
    QString expectedConfigRoot;
    QString expectedManagedRoot;
    QString expectedStable;
    QString ownershipRecord;
    std::function<bool(EntrypointFaultPoint)> faultHook;
    ActivationFilesystemContext filesystem;
    bool initialized = false;

    [[nodiscard]] bool rootsStillNamed() const
    {
        // The authority-provided descriptors prevent operations from being
        // redirected into attacker-selected trees. Canonical re-probes also
        // detect a rename/recreate before and after every transition phase.
        // As with the authority store itself, a same-UID actor can still race
        // a path rename in the instant between a name check and a dirfd
        // syscall; that actor is inside the local-user trust boundary. Any
        // detected post-phase mismatch fails closed and retains the bridge.
        if (!initialized || !filesystem.complete()) return false;
        if (!canonicalDirectoryMatches(
                expectedStateRoot, filesystem.stateDirectoryFd
            )
            || !canonicalDirectoryMatches(
                expectedConfigRoot, filesystem.configDirectoryFd
            )
            || !canonicalDirectoryMatches(
                expectedManagedRoot, filesystem.managedDirectoryFd
            )
            || !canonicalDirectoryMatches(
                QDir(expectedManagedRoot).filePath(
                    QString::fromLatin1(generationsName)
                ), filesystem.generationsDirectoryFd
            )
            || !descriptorStillNamed(
                filesystem.configDirectoryFd,
                QFile::encodeName(QFileInfo(expectedManagedRoot).fileName()),
                filesystem.managedDirectoryFd
            )
            || !descriptorStillNamed(
                filesystem.managedDirectoryFd,
                QByteArrayLiteral(generationsName),
                filesystem.generationsDirectoryFd
            )) return false;
        struct stat config {};
        struct stat managed {};
        return ::fstat(filesystem.configDirectoryFd, &config) == 0
            && ::fstat(filesystem.managedDirectoryFd, &managed) == 0
            && config.st_dev == managed.st_dev;
    }

    [[nodiscard]] SafeFile stable() const
    {
        if (!rootsStillNamed()) return {.kind = SafeFileKind::Unsafe};
        return readSafeFileAt(
            filesystem.configDirectoryFd, QByteArrayLiteral(stableName),
            maximumEntrypointBytes
        );
    }

    [[nodiscard]] SafeFile ownership() const
    {
        if (!rootsStillNamed()) return {.kind = SafeFileKind::Unsafe};
        return readSafeFileAt(
            filesystem.managedDirectoryFd, QByteArrayLiteral(ownershipName),
            maximumMetadataBytes, managedFileMode
        );
    }

    [[nodiscard]] SafeFile bridge() const
    {
        if (!rootsStillNamed()) return {.kind = SafeFileKind::Unsafe};
        return readSafeFileAt(
            filesystem.managedDirectoryFd, QByteArrayLiteral(bridgeName),
            maximumMetadataBytes, managedFileMode
        );
    }
};

AtomicEntrypointPublisher::AtomicEntrypointPublisher(
    QString stateRoot,
    QString configRoot,
    QString managedConfigRoot,
    QString stableEntrypoint,
    QString ownershipRecord,
    std::function<bool(EntrypointFaultPoint)> faultHook
)
    : impl_(std::make_unique<Impl>())
{
    impl_->expectedStateRoot = std::move(stateRoot);
    impl_->expectedConfigRoot = std::move(configRoot);
    impl_->expectedManagedRoot = std::move(managedConfigRoot);
    impl_->expectedStable = std::move(stableEntrypoint);
    impl_->ownershipRecord = std::move(ownershipRecord);
    impl_->faultHook = std::move(faultHook);
}

AtomicEntrypointPublisher::~AtomicEntrypointPublisher() = default;

BackendResult AtomicEntrypointPublisher::initialize(
    ActivationFilesystemContext context
)
{
    BackendResult result;
    const auto pathsValid = context.complete()
        && cleanAbsolute(impl_->expectedStateRoot)
        && cleanAbsolute(impl_->expectedConfigRoot)
        && cleanAbsolute(impl_->expectedManagedRoot)
        && cleanAbsolute(impl_->expectedStable)
        && cleanAbsolute(impl_->ownershipRecord)
        && context.stateRoot == impl_->expectedStateRoot
        && context.configRoot == impl_->expectedConfigRoot
        && context.managedConfigRoot == impl_->expectedManagedRoot
        && context.stableEntrypoint == impl_->expectedStable
        && impl_->expectedManagedRoot == QDir(impl_->expectedConfigRoot)
            .filePath(QStringLiteral("hyprshelld"))
        && impl_->expectedStable == QDir(impl_->expectedConfigRoot)
            .filePath(QString::fromLatin1(stableName))
        && impl_->ownershipRecord == QDir(impl_->expectedManagedRoot)
            .filePath(QString::fromLatin1(ownershipName));
    if (!pathsValid || impl_->initialized) {
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The authority filesystem context does not match the live publisher"
        );
        result.status = conflictStatus({.kind = SafeFileKind::Unsafe});
        return result;
    }
    impl_->filesystem = std::move(context);
    impl_->initialized = true;
    if (!impl_->rootsStillNamed()) {
        impl_->filesystem.reset();
        impl_->initialized = false;
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The authority filesystem roots are no longer canonically named"
        );
        result.status = conflictStatus({.kind = SafeFileKind::Unsafe});
        return result;
    }
    result.success = true;
    result.status = status();
    return result;
}

QString AtomicEntrypointPublisher::managementWatchPath() const
{
    return impl_->expectedStable;
}

namespace {

[[nodiscard]] std::optional<VerifiedTarget> verifyOwnedGeneration(
    const int generationsDirectory,
    const OwnershipRecord &ownership,
    const QString &managedRoot
)
{
    const auto nonce = ownership.nonce.toLatin1();
    const auto generationDirectory = ::openat(
        generationsDirectory, nonce.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (generationDirectory < 0) return std::nullopt;
    const auto manifestFile = readSafeFileAt(
        generationDirectory, QByteArrayLiteral("manifest.json"),
        maximumMetadataBytes, immutableFileMode
    );
    ::close(generationDirectory);
    const auto manifest = manifestFile.kind == SafeFileKind::Regular
        ? parseCanonicalObject(manifestFile.bytes) : std::nullopt;
    quint64 revision = 0;
    if (!manifest
        || !parseUnsigned(
            manifest->value(QStringLiteral("revision")).toString(), revision
        )) return std::nullopt;
    ActivationGeneration prepared {
        .id = ownership.generation,
        .nonce = ownership.nonce,
        .snapshotDigest = manifest->value(
            QStringLiteral("snapshotDigest")
        ).toString(),
        .revision = revision,
        .directory = QDir(managedRoot).filePath(
            QStringLiteral("generations/%1").arg(ownership.nonce)
        ),
        .manifest = manifestFile.bytes,
    };
    prepared.entrypoint = QDir(prepared.directory).filePath(
        QString::fromLatin1(stableName)
    );
    return verifyGeneration(generationsDirectory, prepared);
}

[[nodiscard]] bool validateOwnershipRecord(
    const OwnershipRecord &ownership,
    const SafeFile &stable,
    const int managedDirectory,
    const int generationsDirectory,
    const QString &managedRoot
)
{
    if (!exactStoredIdentity(
            stable, ownership.entrypointDigest,
            ownership.entrypointSize, ownership.entrypointDevice,
            ownership.entrypointInode
        ) || stable.mode != managedFileMode) return false;
    if (!ownership.original.absent) {
        const auto original = readSafeFileAt(
            managedDirectory,
            QFile::encodeName(ownership.original.backupName),
            maximumEntrypointBytes
        );
        if (!exactStoredIdentity(
                original, ownership.original.digest,
                ownership.original.size, ownership.original.device,
                ownership.original.inode
            ) || original.mode != ownership.original.mode) return false;
    }
    const auto generation = verifyOwnedGeneration(
        generationsDirectory, ownership, managedRoot
    );
    return generation && generation->entrypoint.bytes == stable.bytes;
}

} // namespace

ManagementStatus AtomicEntrypointPublisher::status() const
{
    if (!impl_->rootsStillNamed()) {
        return conflictStatus({.kind = SafeFileKind::Unsafe});
    }
    const auto stable = impl_->stable();
    if (stable.kind == SafeFileKind::Unsafe) return conflictStatus(stable);
    const auto bridge = impl_->bridge();
    if (bridge.kind != SafeFileKind::Missing) return conflictStatus(stable);
    const auto stored = impl_->ownership();
    if (stored.kind == SafeFileKind::Missing) {
        // Loader comments are deliberately not an ownership signal. Only the
        // exact private ownership record can make the namespace managed.
        return {
            .state = ManagementState::Unmanaged,
            .entrypointKind = stable.kind == SafeFileKind::Missing
                ? EntrypointKind::Absent : EntrypointKind::Regular,
            .entrypointDigest = stable.digest,
        };
    }
    const auto ownership = stored.kind == SafeFileKind::Regular
        ? parseOwnership(stored.bytes) : std::nullopt;
    if (!ownership || !validateOwnershipRecord(
            *ownership, stable, impl_->filesystem.managedDirectoryFd,
            impl_->filesystem.generationsDirectoryFd,
            impl_->expectedManagedRoot
        ) || !impl_->rootsStillNamed()) return conflictStatus(stable);
    return {
        .state = ManagementState::Managed,
        .entrypointKind = EntrypointKind::Regular,
        .entrypointDigest = stable.digest,
        .managedGeneration = ownership->generation,
        .managedNonce = ownership->nonce,
    };
}

EntrypointPublishResult AtomicEntrypointPublisher::publish(
    const ActivationGeneration &prepared,
    const bool adoption,
    const QStringView expectedEntrypointDigest,
    const QByteArrayView baselineConfigErrors,
    const QStringView baselineProvider
)
{
    EntrypointPublishResult result;
    if (!impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The authority filesystem roots are no longer authoritative"
        );
        result.status = conflictStatus({.kind = SafeFileKind::Unsafe});
        return result;
    }
    const auto bridgeBefore = impl_->bridge();
    if (bridgeBefore.kind != SafeFileKind::Missing) {
        result.errorCode = QStringLiteral("ApplyFailed");
        result.errorMessage = QStringLiteral(
            "A live activation bridge is already pending"
        );
        result.status = conflictStatus(impl_->stable());
        return result;
    }
    const auto stable = impl_->stable();
    const auto storedOwnership = impl_->ownership();
    const auto currentStatus = status();
    const auto expectedMatches = expectedEntrypointDigest.isEmpty()
        ? stable.kind == SafeFileKind::Missing
        : stable.kind == SafeFileKind::Regular
            && stable.digest == expectedEntrypointDigest;
    const auto baselineValid = baselineConfigErrors == QByteArrayLiteral("[]")
        && (baselineProvider == QStringLiteral("lua")
            || baselineProvider == QStringLiteral("hyprlang"));
    const auto recoverableAdoption = stable.kind == SafeFileKind::Missing
        ? baselineProvider == QStringLiteral("hyprlang")
        : stable.kind == SafeFileKind::Regular
            && baselineProvider == QStringLiteral("lua");
    const auto modeValid = adoption
        ? currentStatus.state == ManagementState::Unmanaged
            && storedOwnership.kind == SafeFileKind::Missing
            && expectedMatches && recoverableAdoption
        : currentStatus.state == ManagementState::Managed
            && storedOwnership.kind == SafeFileKind::Regular
            && stable.kind == SafeFileKind::Regular
            && stable.digest == currentStatus.entrypointDigest;
    if (!baselineValid || !modeValid) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The entrypoint or its recoverable live baseline changed"
        );
        result.status = conflictStatus(stable);
        return result;
    }

    if (adoption && stable.kind == SafeFileKind::Regular) {
        const auto syncFault = faulted(
            impl_->faultHook,
            EntrypointFaultPoint::BeforeOriginalEntrypointSync
        );
        const auto synced = !syncFault && impl_->rootsStillNamed()
            && syncExactFileAt(
                impl_->filesystem.configDirectoryFd,
                QByteArrayLiteral("hyprland.lua"), stable,
                [this] { return impl_->rootsStillNamed(); }
            );
        const auto durableOriginal = impl_->stable();
        const auto rootsExact = impl_->rootsStillNamed();
        if (!synced
            || !exactStoredIdentity(
                durableOriginal, stable.digest,
                static_cast<quint64>(stable.bytes.size()),
                deviceOf(stable.info), inodeOf(stable.info)
            ) || durableOriginal.mode != stable.mode
            || durableOriginal.bytes != stable.bytes
            || !rootsExact) {
            result.errorCode = QStringLiteral("PersistenceFailed");
            result.errorMessage = QStringLiteral(
                "The original user entrypoint could not be synchronized and rebound exactly"
            );
            result.status = rootsExact ? currentStatus
                                       : conflictStatus(durableOriginal);
            return result;
        }
    }

    const auto expectedDirectory = QDir(impl_->expectedManagedRoot).filePath(
        QStringLiteral("generations/%1").arg(prepared.nonce)
    );
    const auto expectedEntrypoint = QDir(expectedDirectory).filePath(
        QString::fromLatin1(stableName)
    );
    const auto verified = prepared.directory == expectedDirectory
        && prepared.entrypoint == expectedEntrypoint
        ? verifyGeneration(
            impl_->filesystem.generationsDirectoryFd, prepared
        ) : std::nullopt;
    if (!verified || !impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral(
            "The immutable prepared generation failed descriptor-relative verification"
        );
        result.status = currentStatus;
        return result;
    }

    auto token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    token.remove(QLatin1Char('-'));
    const auto swapName = QStringLiteral(".hyprshelld-%1-%2.lua")
        .arg(adoption ? QStringLiteral("original")
                      : QStringLiteral("transition"), token);
    const auto priorOwnership = adoption ? QByteArray()
                                         : storedOwnership.bytes;
    BridgeRecord bridge {
        .phase = BridgePhase::Staging,
        .token = token,
        .adoption = adoption,
        .targetGeneration = prepared.id,
        .targetNonce = prepared.nonce,
        .targetDigest = verified->entrypoint.digest,
        .targetSize = static_cast<quint64>(
            verified->entrypoint.bytes.size()
        ),
        .swapName = swapName,
        .beforeAbsent = stable.kind == SafeFileKind::Missing,
        .beforeDigest = stable.digest,
        .beforeSize = static_cast<quint64>(stable.bytes.size()),
        .beforeMode = stable.kind == SafeFileKind::Regular ? stable.mode : 0,
        .beforeDevice = stable.kind == SafeFileKind::Regular
            ? deviceOf(stable.info) : 0,
        .beforeInode = stable.kind == SafeFileKind::Regular
            ? inodeOf(stable.info) : 0,
        .beforeGeneration = adoption ? QString()
                                     : currentStatus.managedGeneration,
        .beforeNonce = adoption ? QString() : currentStatus.managedNonce,
        .beforeOwnership = priorOwnership,
        .baselineConfigErrors = baselineConfigErrors.toByteArray(),
        .baselineProvider = baselineProvider.toString(),
    };
    const auto initialBytes = bridgeBytes(bridge);
    const auto bridgeTemporary = QByteArrayLiteral(".hyprshelld-bridge-")
        + token.toLatin1();
    const auto initialPublished = publishNoReplace(
        impl_->filesystem.managedDirectoryFd, bridgeTemporary,
        QByteArrayLiteral(bridgeName), initialBytes, managedFileMode,
        impl_->faultHook, EntrypointFaultPoint::BeforeJournalRename,
        EntrypointFaultPoint::AfterJournalRenameBeforeDirectorySync,
        [this] { return impl_->rootsStillNamed(); }
    );
    const auto storedInitial = impl_->bridge();
    if (!initialPublished.success) {
        result.namespaceMayHaveChanged = initialPublished.namespaceChanged;
        if (storedInitial.kind == SafeFileKind::Regular
            && storedInitial.bytes == initialBytes
            && parseBridge(storedInitial.bytes)) {
            result.receipt.rollbackToken = token.toLatin1();
        }
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = initialPublished.namespaceChanged
            ? QStringLiteral(
                "The staging activation bridge may be visible but is not durable"
            )
            : QStringLiteral("The activation bridge could not be created");
        result.status = conflictStatus(stable);
        return result;
    }

    result.receipt.rollbackToken = token.toLatin1();
    const auto encodedSwap = QFile::encodeName(swapName);
    const auto stagedInstall = installCompleteNoReplace(
        impl_->filesystem.managedDirectoryFd, encodedSwap,
        verified->entrypoint.bytes, maximumEntrypointBytes, managedFileMode,
        impl_->faultHook,
        EntrypointFaultPoint::AfterTargetFileSyncBeforeDirectorySync,
        [this] { return impl_->rootsStillNamed(); }
    );
    if (!stagedInstall.success) {
        result.namespaceMayHaveChanged = result.namespaceMayHaveChanged
            || stagedInstall.newlyInstalled;
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The candidate loader could not be staged durably"
        );
        result.status = conflictStatus(stable);
        return result;
    }
    const auto staged = readSafeFileAt(
        impl_->filesystem.managedDirectoryFd, encodedSwap,
        maximumEntrypointBytes, managedFileMode
    );
    if (staged.kind != SafeFileKind::Regular
        || staged.bytes != verified->entrypoint.bytes) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral(
            "The durably staged candidate loader changed"
        );
        result.status = conflictStatus(stable);
        return result;
    }
    bridge.phase = BridgePhase::Ready;
    bridge.targetDevice = deviceOf(staged.info);
    bridge.targetInode = inodeOf(staged.info);
    const auto readyBytes = bridgeBytes(bridge);
    const auto readyPublished = replaceExact(
        impl_->filesystem.managedDirectoryFd,
        QFile::encodeName(readyTemporaryName(token)),
        QByteArrayLiteral(bridgeName), readyBytes, initialBytes, storedInitial,
        managedFileMode, impl_->faultHook,
        EntrypointFaultPoint::BeforeReadyJournalRename,
        EntrypointFaultPoint::AfterReadyJournalRenameBeforeDirectorySync,
        [this] { return impl_->rootsStillNamed(); }
    );
    if (!readyPublished.success) {
        result.namespaceMayHaveChanged = readyPublished.namespaceChanged;
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = readyPublished.namespaceChanged
            ? QStringLiteral(
                "The ready activation bridge may be visible but is not durable"
            )
            : QStringLiteral("The ready activation bridge could not be published");
        result.status = conflictStatus(stable);
        return result;
    }

    const auto stableReprobe = impl_->stable();
    const auto ownershipReprobe = impl_->ownership();
    const auto beforeExchangeFault = faulted(
        impl_->faultHook, EntrypointFaultPoint::BeforeEntrypointExchange
    );
    if (!exactBefore(bridge, stableReprobe)
        || !exactOwnershipState(ownershipReprobe, bridge.beforeOwnership)
        || beforeExchangeFault || !impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The entrypoint or ownership tuple changed before publication"
        );
        result.status = conflictStatus(stableReprobe);
        return result;
    }

    const auto switched = bridge.beforeAbsent
        ? renameAt2(
            impl_->filesystem.managedDirectoryFd, encodedSwap.constData(),
            impl_->filesystem.configDirectoryFd, stableName,
            RENAME_NOREPLACE
        ) == 0
        : renameAt2(
            impl_->filesystem.managedDirectoryFd, encodedSwap.constData(),
            impl_->filesystem.configDirectoryFd, stableName,
            RENAME_EXCHANGE
        ) == 0;
    result.namespaceMayHaveChanged = switched;
    const auto stableAfter = impl_->stable();
    const auto swappedPrior = readSafeFileAt(
        impl_->filesystem.managedDirectoryFd, encodedSwap,
        maximumEntrypointBytes
    );
    const auto swapExact = bridge.beforeAbsent
        ? swappedPrior.kind == SafeFileKind::Missing
        : exactStoredIdentity(
            swappedPrior, bridge.beforeDigest, bridge.beforeSize,
            bridge.beforeDevice, bridge.beforeInode
        ) && swappedPrior.mode == bridge.beforeMode;
    const auto publishedExact = switched && exactTarget(bridge, stableAfter)
        && swapExact;
    if (!publishedExact) {
        // EXCHANGE can reveal a concurrent replacement without clobbering it.
        // Put that displaced object back if the candidate is still exact.
        if (switched && !bridge.beforeAbsent
            && exactTarget(bridge, stableAfter)) {
            const auto restored = renameAt2(
                impl_->filesystem.managedDirectoryFd, encodedSwap.constData(),
                impl_->filesystem.configDirectoryFd, stableName,
                RENAME_EXCHANGE
            ) == 0;
            const auto restoreDurable = restored
                && retryFsync(impl_->filesystem.managedDirectoryFd)
                && retryFsync(impl_->filesystem.configDirectoryFd)
                && impl_->rootsStillNamed();
            if (!restoreDurable) {
                result.errorMessage = QStringLiteral(
                    "The unexpected entrypoint displacement could not be restored durably"
                );
            }
        }
        result.errorCode = switched ? QStringLiteral("EntrypointChanged")
                                    : QStringLiteral("PersistenceFailed");
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = switched
                ? QStringLiteral("The entrypoint CAS displaced an unexpected object")
                : QStringLiteral("The managed entrypoint CAS could not be performed");
        }
        result.status = conflictStatus(stableAfter);
        return result;
    }
    if (faulted(
            impl_->faultHook,
            EntrypointFaultPoint::AfterEntrypointExchangeBeforeDirectorySync
        )
        || !retryFsync(impl_->filesystem.configDirectoryFd)
        || !retryFsync(impl_->filesystem.managedDirectoryFd)
        || !impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The managed entrypoint is visible but its namespace is not durably proven"
        );
        result.status = conflictStatus(stableAfter);
        return result;
    }
    result.success = true;
    result.status = statusForBridgeSide(bridge, stableAfter, true);
    return result;
}

EntrypointReconciliationResult
AtomicEntrypointPublisher::pendingReconciliation() const
{
    EntrypointReconciliationResult result;
    if (!impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The authority filesystem roots changed during reconciliation"
        );
        return result;
    }
    const auto stored = impl_->bridge();
    if (stored.kind == SafeFileKind::Missing) {
        result.success = true;
        return result;
    }
    const auto bridge = stored.kind == SafeFileKind::Regular
        ? parseBridge(stored.bytes) : std::nullopt;
    if (!bridge || !impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The live activation bridge is unsafe or invalid"
        );
        return result;
    }
    result.success = true;
    result.value = EntrypointReconciliation {
        .pending = true,
        .targetGeneration = bridge->targetGeneration,
        .priorGeneration = bridge->beforeGeneration,
        .priorNonce = bridge->beforeNonce,
        .baselineConfigErrors = bridge->baselineConfigErrors,
        .baselineProvider = bridge->baselineProvider,
        .receipt = {.rollbackToken = bridge->token.toLatin1()},
    };
    return result;
}

EntrypointRollbackResult AtomicEntrypointPublisher::rollback(
    const ActivationReceipt &receipt
)
{
    EntrypointRollbackResult result;
    if (!impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral("The rollback roots changed");
        result.status = conflictStatus({.kind = SafeFileKind::Unsafe});
        return result;
    }
    const auto stored = impl_->bridge();
    const auto bridge = stored.kind == SafeFileKind::Regular
        ? parseBridge(stored.bytes) : std::nullopt;
    if (!bridge || bridge->token.toLatin1() != receipt.rollbackToken) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The rollback receipt is no longer authoritative"
        );
        result.status = conflictStatus(impl_->stable());
        return result;
    }
    const auto ownership = impl_->ownership();
    if (!exactOwnershipState(ownership, bridge->beforeOwnership)) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The ownership record changed before rollback"
        );
        result.status = conflictStatus(impl_->stable());
        return result;
    }
    auto stable = impl_->stable();
    auto swapped = readSafeFileAt(
        impl_->filesystem.managedDirectoryFd,
        QFile::encodeName(bridge->swapName), maximumEntrypointBytes
    );
    const auto alreadyBefore = exactBefore(*bridge, stable);
    const auto currentlyTarget = exactTarget(*bridge, stable);
    if (!alreadyBefore && !currentlyTarget) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The live entrypoint is neither exact side of the rollback bridge"
        );
        result.status = conflictStatus(stable);
        return result;
    }

    bool changed = false;
    bool switched = true;
    if (currentlyTarget) {
        const auto beforeSwitchFault = faulted(
            impl_->faultHook, EntrypointFaultPoint::BeforeEntrypointExchange
        );
        if (beforeSwitchFault || !impl_->rootsStillNamed()) {
            switched = false;
        } else if (bridge->beforeAbsent) {
            switched = ::unlinkat(
                impl_->filesystem.configDirectoryFd, stableName, 0
            ) == 0;
            changed = switched;
        } else if (exactStoredIdentity(
                       swapped, bridge->beforeDigest, bridge->beforeSize,
                       bridge->beforeDevice, bridge->beforeInode
                   ) && swapped.mode == bridge->beforeMode) {
            switched = renameAt2(
                impl_->filesystem.managedDirectoryFd,
                QFile::encodeName(bridge->swapName).constData(),
                impl_->filesystem.configDirectoryFd, stableName,
                RENAME_EXCHANGE
            ) == 0;
            changed = switched;
        } else {
            switched = false;
        }
        if (switched) {
            const auto afterSwitchFault = faulted(
                impl_->faultHook,
                EntrypointFaultPoint::AfterEntrypointExchangeBeforeDirectorySync
            );
            switched = !afterSwitchFault && impl_->rootsStillNamed()
                && retryFsync(impl_->filesystem.configDirectoryFd)
                && retryFsync(impl_->filesystem.managedDirectoryFd)
                && impl_->rootsStillNamed();
        }
    }
    stable = impl_->stable();
    // Rollback success binds the live stable entrypoint, not cleanup of the
    // private swap name. If that name was concurrently replaced while the
    // stable path was already on the prior side, live rollback can still be
    // proved; side-bound finalization will preserve the foreign node, retain
    // the bridge, and fail closed instead of unlinking it.
    const auto exact = switched && exactBefore(*bridge, stable)
        && exactOwnershipState(impl_->ownership(), bridge->beforeOwnership)
        && impl_->rootsStillNamed();
    result.namespaceMayHaveChanged = changed;
    result.proofNonce = bridge->beforeNonce;
    result.baselineConfigErrors = bridge->baselineConfigErrors;
    result.baselineProvider = bridge->baselineProvider;
    result.status = exact
        ? statusForBridgeSide(*bridge, stable, false)
        : conflictStatus(stable);
    if (!exact) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = QStringLiteral(
            "The prior entrypoint could not be restored exactly"
        );
        return result;
    }
    result.success = true;
    return result;
}

BackendResult AtomicEntrypointPublisher::verifyTransition(
    const ActivationReceipt &receipt,
    const bool target
) const
{
    BackendResult result;
    if (!impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral("The transition roots changed");
        result.status = conflictStatus({.kind = SafeFileKind::Unsafe});
        return result;
    }
    const auto stored = impl_->bridge();
    const auto bridge = stored.kind == SafeFileKind::Regular
        ? parseBridge(stored.bytes) : std::nullopt;
    const auto stable = impl_->stable();
    if (!bridge || bridge->token.toLatin1() != receipt.rollbackToken) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral("The transition receipt changed");
        result.status = conflictStatus(stable);
        return result;
    }
    const auto exact = target ? exactTarget(*bridge, stable)
                              : exactBefore(*bridge, stable);
    const auto ownershipExact = exactOwnershipState(
        impl_->ownership(), bridge->beforeOwnership
    );
    bool generationExact = true;
    if (target) {
        const OwnershipRecord candidate {
            .generation = bridge->targetGeneration,
            .nonce = bridge->targetNonce,
            .entrypointDigest = bridge->targetDigest,
            .entrypointSize = bridge->targetSize,
            .entrypointDevice = bridge->targetDevice,
            .entrypointInode = bridge->targetInode,
        };
        const auto generation = verifyOwnedGeneration(
            impl_->filesystem.generationsDirectoryFd, candidate,
            impl_->expectedManagedRoot
        );
        generationExact = generation
            && generation->entrypoint.bytes == stable.bytes;
    } else if (!bridge->beforeGeneration.isEmpty()) {
        const auto prior = parseOwnership(bridge->beforeOwnership);
        generationExact = prior && validateOwnershipRecord(
            *prior, stable, impl_->filesystem.managedDirectoryFd,
            impl_->filesystem.generationsDirectoryFd,
            impl_->expectedManagedRoot
        );
    }
    if (!exact || !ownershipExact || !generationExact
        || !impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The entrypoint or ownership tuple changed after live proof"
        );
        result.status = conflictStatus(stable);
        return result;
    }
    result.success = true;
    result.status = statusForBridgeSide(*bridge, stable, target);
    return result;
}

BackendResult AtomicEntrypointPublisher::finalize(
    const ActivationReceipt &receipt,
    const bool target
)
{
    BackendResult result;
    if (!impl_->rootsStillNamed()) {
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral("The finalization roots changed");
        result.status = conflictStatus({.kind = SafeFileKind::Unsafe});
        return result;
    }
    const auto stored = impl_->bridge();
    const auto bridge = stored.kind == SafeFileKind::Regular
        ? parseBridge(stored.bytes) : std::nullopt;
    if (!bridge || bridge->token.toLatin1() != receipt.rollbackToken) {
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The finalization receipt is no longer authoritative"
        );
        result.status = conflictStatus(impl_->stable());
        return result;
    }
    const auto stable = impl_->stable();
    const auto targetSide = exactTarget(*bridge, stable);
    const auto beforeSide = exactBefore(*bridge, stable);
    if ((target && !targetSide) || (!target && !beforeSide)) {
        result.errorCode = QStringLiteral("EntrypointChanged");
        result.errorMessage = QStringLiteral(
            "The entrypoint is not the receipt-bound finalization side"
        );
        result.status = conflictStatus(stable);
        return result;
    }

    const auto swapName = QFile::encodeName(bridge->swapName);
    auto swapped = readSafeFileAt(
        impl_->filesystem.managedDirectoryFd, swapName,
        maximumEntrypointBytes
    );
    auto stagingBridge = *bridge;
    stagingBridge.phase = BridgePhase::Staging;
    stagingBridge.targetDevice = 0;
    stagingBridge.targetInode = 0;
    QByteArray expectedReadyTemporary;
    if (bridge->phase == BridgePhase::Ready) {
        expectedReadyTemporary = bridgeBytes(*bridge);
    } else if (exactStagedTarget(*bridge, swapped)) {
        auto readyBridge = *bridge;
        readyBridge.phase = BridgePhase::Ready;
        readyBridge.targetDevice = deviceOf(swapped.info);
        readyBridge.targetInode = inodeOf(swapped.info);
        expectedReadyTemporary = bridgeBytes(readyBridge);
    }
    if (target) {
        OriginalRecord original;
        std::optional<OwnershipRecord> prior;
        if (!bridge->beforeGeneration.isEmpty()) {
            prior = parseOwnership(bridge->beforeOwnership);
            if (!prior) {
                result.errorCode = QStringLiteral("PersistenceFailed");
                result.errorMessage = QStringLiteral(
                    "The prior ownership payload cannot be carried forward"
                );
                result.status = conflictStatus(stable);
                return result;
            }
            original = prior->original;
        } else if (!bridge->beforeAbsent) {
            if (!exactStoredIdentity(
                    swapped, bridge->beforeDigest, bridge->beforeSize,
                    bridge->beforeDevice, bridge->beforeInode
                ) || swapped.mode != bridge->beforeMode
                || !validOriginalBackupName(bridge->swapName)) {
                result.errorCode = QStringLiteral("EntrypointChanged");
                result.errorMessage = QStringLiteral(
                    "The original user loader was not preserved exactly"
                );
                result.status = conflictStatus(stable);
                return result;
            }
            original = {
                .absent = false,
                .digest = bridge->beforeDigest,
                .size = bridge->beforeSize,
                .mode = bridge->beforeMode,
                .device = bridge->beforeDevice,
                .inode = bridge->beforeInode,
                .backupName = bridge->swapName,
            };
        }
        const OwnershipRecord targetOwnership {
            .generation = bridge->targetGeneration,
            .nonce = bridge->targetNonce,
            .entrypointDigest = bridge->targetDigest,
            .entrypointSize = bridge->targetSize,
            .entrypointDevice = bridge->targetDevice,
            .entrypointInode = bridge->targetInode,
            .original = original,
        };
        // Finalization is the last durable recovery boundary. Re-verify the
        // immutable generation, live loader inode, and retained original as
        // one complete ownership tuple before publishing it or removing the
        // bridge. A digest-only match is not sufficient here: the generation
        // tree or an adoption backup could have been replaced after the live
        // reload proof.
        if (!validateOwnershipRecord(
                targetOwnership, stable,
                impl_->filesystem.managedDirectoryFd,
                impl_->filesystem.generationsDirectoryFd,
                impl_->expectedManagedRoot
            ) || !impl_->rootsStillNamed()) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = QStringLiteral(
                "The target ownership tuple changed before finalization"
            );
            result.status = conflictStatus(stable);
            return result;
        }
        const auto targetOwnershipBytes = ownershipBytes(targetOwnership);
        const auto ownershipBeforePromotion = impl_->ownership();
        const auto ownershipAlreadyTarget =
            ownershipBeforePromotion.kind == SafeFileKind::Regular
            && ownershipBeforePromotion.bytes == targetOwnershipBytes
            && parseOwnership(ownershipBeforePromotion.bytes).has_value();
        if (!bridge->beforeGeneration.isEmpty()
            && !(exactStoredIdentity(
                    swapped, bridge->beforeDigest, bridge->beforeSize,
                    bridge->beforeDevice, bridge->beforeInode
                 ) && swapped.mode == bridge->beforeMode)
            && !(swapped.kind == SafeFileKind::Missing
                 && ownershipAlreadyTarget)) {
            result.errorCode = QStringLiteral("EntrypointChanged");
            result.errorMessage = QStringLiteral(
                "The displaced prior managed loader changed"
            );
            result.status = conflictStatus(stable);
            return result;
        }
        const auto promoted = publishOwnershipExact(
            impl_->filesystem.managedDirectoryFd, bridge->token,
            targetOwnershipBytes, bridge->beforeOwnership,
            impl_->faultHook,
            [this] { return impl_->rootsStillNamed(); }
        );
        if (!promoted.success || !impl_->rootsStillNamed()) {
            result.errorCode = QStringLiteral("PersistenceFailed");
            result.errorMessage = promoted.namespaceChanged
                ? QStringLiteral(
                    "Target ownership may be visible but is not durably proven"
                )
                : QStringLiteral("Target ownership could not be promoted exactly");
            result.status = conflictStatus(stable);
            return result;
        }
        if (!removeOwnershipTemporary(
                impl_->filesystem.managedDirectoryFd, bridge->token,
                bridge->beforeOwnership,
                [this] { return impl_->rootsStillNamed(); }
            )) {
            result.errorCode = QStringLiteral("PersistenceFailed");
            result.errorMessage = QStringLiteral(
                "The displaced ownership staging record could not be removed"
            );
            result.status = conflictStatus(stable);
            return result;
        }
        // A managed-to-managed transition does not need to retain the prior
        // loader after the authority commits. Initial adoption deliberately
        // keeps the exact original inode referenced by ownership.
        if (!bridge->beforeGeneration.isEmpty()
            && swapped.kind != SafeFileKind::Missing
            && (!impl_->rootsStillNamed()
                || !removeAndSync(
                    impl_->filesystem.managedDirectoryFd, swapName, false,
                    [this] { return impl_->rootsStillNamed(); }
                ) || !impl_->rootsStillNamed())) {
            result.errorCode = QStringLiteral("PersistenceFailed");
            result.errorMessage = QStringLiteral(
                "The displaced managed transition inode could not be removed"
            );
            result.status = conflictStatus(stable);
            return result;
        }
    } else {
        const auto priorOwnership = impl_->ownership();
        auto priorOwnershipValid = exactOwnershipState(
            priorOwnership, bridge->beforeOwnership
        );
        if (priorOwnershipValid && !bridge->beforeGeneration.isEmpty()) {
            const auto parsedPrior = parseOwnership(bridge->beforeOwnership);
            priorOwnershipValid = parsedPrior
                && validateOwnershipRecord(
                    *parsedPrior, stable,
                    impl_->filesystem.managedDirectoryFd,
                    impl_->filesystem.generationsDirectoryFd,
                    impl_->expectedManagedRoot
                );
        }
        if (!priorOwnershipValid || !impl_->rootsStillNamed()) {
            result.errorCode = QStringLiteral("EntrypointChanged");
            result.errorMessage = QStringLiteral(
                "Rollback ownership or its immutable generation no longer matches the journal"
            );
            result.status = conflictStatus(stable);
            return result;
        }
        if (swapped.kind != SafeFileKind::Missing) {
            const auto removableCandidate = exactStagedTarget(
                *bridge, swapped
            );
            if (!removableCandidate
                || !removeAndSync(
                    impl_->filesystem.managedDirectoryFd, swapName, false,
                    [this] { return impl_->rootsStillNamed(); }
                )) {
                result.errorCode = QStringLiteral("PersistenceFailed");
                result.errorMessage = QStringLiteral(
                    "The rolled-back candidate inode could not be removed exactly"
                );
                result.status = conflictStatus(stable);
                return result;
            }
        }
    }
    if (!removeReadyTemporary(
            impl_->filesystem.managedDirectoryFd, bridge->token,
            bridgeBytes(stagingBridge),
            expectedReadyTemporary,
            [this] { return impl_->rootsStillNamed(); }
        ) || !removeBridgeDurably(
            impl_->filesystem.managedDirectoryFd, impl_->faultHook,
            [this] { return impl_->rootsStillNamed(); }
        )) {
        result.errorCode = QStringLiteral("PersistenceFailed");
        result.errorMessage = QStringLiteral(
            "The live activation bridge could not be finalized durably"
        );
        result.status = conflictStatus(stable);
        return result;
    }
    result.status = status();
    if (target
        ? result.status.state != ManagementState::Managed
            || result.status.managedGeneration != bridge->targetGeneration
        : result.status.state != (bridge->beforeGeneration.isEmpty()
                ? ManagementState::Unmanaged : ManagementState::Managed)
            || (!bridge->beforeGeneration.isEmpty()
                && result.status.managedGeneration
                    != bridge->beforeGeneration)) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral(
            "The finalized ownership state failed its exact re-probe"
        );
        result.status = conflictStatus(impl_->stable());
        return result;
    }
    result.success = true;
    return result;
}

} // namespace HyprShelld::Compositor
