#include "activation_backend.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

namespace HyprShelld::Compositor {
namespace {

constexpr qsizetype maximumEntrypointBytes = 16 * 1024 * 1024;

bool sameFileSnapshot(const struct stat &left, const struct stat &right)
{
    return left.st_dev == right.st_dev
        && left.st_ino == right.st_ino
        && left.st_mode == right.st_mode
        && left.st_nlink == right.st_nlink
        && left.st_size == right.st_size
        && left.st_mtim.tv_sec == right.st_mtim.tv_sec
        && left.st_mtim.tv_nsec == right.st_mtim.tv_nsec
        && left.st_ctim.tv_sec == right.st_ctim.tv_sec
        && left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

bool trustedDirectoryMetadata(
    const struct stat &metadata,
    const uid_t rootOwner
)
{
    const auto ownerIsTrusted = metadata.st_uid == ::geteuid()
        || metadata.st_uid == rootOwner;
    const auto writableByOthers =
        (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0;
    const auto protectedTemporaryRoot = metadata.st_uid == rootOwner
        && (metadata.st_mode & S_ISVTX) != 0;
    return S_ISDIR(metadata.st_mode)
        && ownerIsTrusted
        && (!writableByOthers || protectedTemporaryRoot);
}

int openTrustedDirectoryTree(const QString &path)
{
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path) {
        return -1;
    }
    auto current = ::open(
        "/",
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY
    );
    if (current < 0) {
        return -1;
    }
    struct stat rootMetadata{};
    if (::fstat(current, &rootMetadata) != 0
        || !S_ISDIR(rootMetadata.st_mode)) {
        ::close(current);
        return -1;
    }
    const auto rootOwner = rootMetadata.st_uid;

    const auto components = path.split(
        QLatin1Char('/'),
        Qt::SkipEmptyParts
    );
    for (const auto &component : components) {
        const auto name = QFile::encodeName(component);
        if (name.isEmpty() || name == "." || name == ".."
            || name.contains('/')) {
            ::close(current);
            return -1;
        }
        struct stat pathMetadata{};
        if (::fstatat(
                current,
                name.constData(),
                &pathMetadata,
                AT_SYMLINK_NOFOLLOW
            ) != 0
            || !trustedDirectoryMetadata(pathMetadata, rootOwner)) {
            ::close(current);
            return -1;
        }
        const auto next = ::openat(
            current,
            name.constData(),
            O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY
        );
        if (next < 0) {
            ::close(current);
            return -1;
        }
        struct stat openedMetadata{};
        if (::fstat(next, &openedMetadata) != 0
            || !sameFileSnapshot(pathMetadata, openedMetadata)) {
            ::close(next);
            ::close(current);
            return -1;
        }
        ::close(current);
        current = next;
    }
    return current;
}

ManagementStatus unsafeEntrypoint()
{
    return {
        .state = ManagementState::Conflict,
        .entrypointKind = EntrypointKind::Unsafe,
    };
}

ManagementStatus inspectEntrypoint(
    const QString &configRoot,
    const QString &path
)
{
    const auto expectedPath = QDir(configRoot).filePath(
        QStringLiteral("hyprland.lua")
    );
    if (!QDir::isAbsolutePath(configRoot)
        || !QDir::isAbsolutePath(path)
        || QDir::cleanPath(configRoot) != configRoot
        || QDir::cleanPath(path) != path
        || path != expectedPath) {
        return unsafeEntrypoint();
    }

    const auto rootDescriptor = openTrustedDirectoryTree(configRoot);
    if (rootDescriptor < 0) {
        return unsafeEntrypoint();
    }

    constexpr auto fileName = "hyprland.lua";
    struct stat pathStat{};
    if (::fstatat(
            rootDescriptor,
            fileName,
            &pathStat,
            AT_SYMLINK_NOFOLLOW
        ) != 0) {
        const auto missing = errno == ENOENT;
        ::close(rootDescriptor);
        return missing ? ManagementStatus{} : unsafeEntrypoint();
    }
    if (!S_ISREG(pathStat.st_mode)
        || pathStat.st_uid != ::geteuid()
        || (pathStat.st_mode & (S_IWGRP | S_IWOTH)) != 0
        || pathStat.st_nlink != 1
        || pathStat.st_size < 0
        || pathStat.st_size > maximumEntrypointBytes) {
        ::close(rootDescriptor);
        return unsafeEntrypoint();
    }

    // Use openat so a renamed or symlinked ancestor cannot redirect the final
    // lookup after the trusted directory descriptor is established.
    const auto descriptor = ::openat(
        rootDescriptor,
        fileName,
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    );
    if (descriptor < 0) {
        ::close(rootDescriptor);
        return unsafeEntrypoint();
    }

    struct stat openedStat {};
    if (::fstat(descriptor, &openedStat) != 0
        || !sameFileSnapshot(pathStat, openedStat)) {
        ::close(descriptor);
        ::close(rootDescriptor);
        return unsafeEntrypoint();
    }

    QByteArray bytes;
    bytes.reserve(static_cast<qsizetype>(openedStat.st_size));
    char buffer[16 * 1024];
    while (true) {
        const auto count = ::read(descriptor, buffer, sizeof(buffer));
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            ::close(descriptor);
            ::close(rootDescriptor);
            return unsafeEntrypoint();
        }
        if (bytes.size() > maximumEntrypointBytes - count) {
            ::close(descriptor);
            ::close(rootDescriptor);
            return unsafeEntrypoint();
        }
        bytes.append(buffer, count);
    }

    struct stat finalStat {};
    struct stat finalPathStat {};
    const auto stable = ::fstat(descriptor, &finalStat) == 0
        && sameFileSnapshot(openedStat, finalStat)
        && ::fstatat(
               rootDescriptor,
               fileName,
               &finalPathStat,
               AT_SYMLINK_NOFOLLOW
           ) == 0
        && sameFileSnapshot(finalStat, finalPathStat)
        && bytes.size() == finalStat.st_size;
    ::close(descriptor);
    ::close(rootDescriptor);
    if (!stable) {
        return unsafeEntrypoint();
    }

    return {
        .state = ManagementState::Unmanaged,
        .entrypointKind = EntrypointKind::Regular,
        .entrypointDigest = QString::fromLatin1(
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
        ),
    };
}

ActivationResult unavailableResult(const ManagementStatus &status)
{
    return {
        .success = false,
        .errorCode = QStringLiteral("ActivationRequired"),
        .errorMessage = QStringLiteral(
            "Live Hyprland activation is unavailable in this service build"
        ),
        .status = status,
    };
}

} // namespace

DeferredActivationBackend::DeferredActivationBackend(
    QString configRoot,
    QString stableEntrypoint
)
    : configRoot_(std::move(configRoot))
    , stableEntrypoint_(std::move(stableEntrypoint))
{
}

ManagementStatus DeferredActivationBackend::status() const
{
    return inspectEntrypoint(configRoot_, stableEntrypoint_);
}

bool DeferredActivationBackend::canSatisfy(ActivationRequirement) const
{
    return false;
}

ActivationResult DeferredActivationBackend::adopt(
    const ActivationGeneration &,
    QStringView
)
{
    return unavailableResult(status());
}

ActivationResult DeferredActivationBackend::activate(
    const ActivationGeneration &
)
{
    return unavailableResult(status());
}

ActivationResult DeferredActivationBackend::rollback(
    const ActivationReceipt &
)
{
    return unavailableResult(status());
}

QString managementStateName(const ManagementState state)
{
    switch (state) {
    case ManagementState::Unmanaged:
        return QStringLiteral("unmanaged");
    case ManagementState::Managed:
        return QStringLiteral("managed");
    case ManagementState::Conflict:
        return QStringLiteral("conflict");
    }
    Q_UNREACHABLE_RETURN(QString());
}

} // namespace HyprShelld::Compositor
