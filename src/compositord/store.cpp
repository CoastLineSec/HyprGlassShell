#include "store.h"

#include "hyprland/desired_state.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr mode_t privateDirectoryMode = 0700;
constexpr mode_t privateFileMode = 0600;
constexpr qsizetype maximumMetadataBytes = 4 * 1024 * 1024;

std::atomic<quint64> temporaryCounter{0};

[[nodiscard]] bool retryFsync(int descriptor);

StoreOperationResult failure(QString code, QString message)
{
    return {
        .success = false,
        .errorCode = std::move(code),
        .errorMessage = std::move(message),
    };
}

[[nodiscard]] bool safePath(const QString &path)
{
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path
        || path != path.normalized(QString::NormalizationForm_C)
        || path.toUtf8().size() > 4096) {
        return false;
    }
    for (const auto point : path.toUcs4()) {
        const auto category = QChar::category(static_cast<char32_t>(point));
        if (category == QChar::Other_Control
            || category == QChar::Other_Format
            || category == QChar::Separator_Line
            || category == QChar::Separator_Paragraph) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] int openDirectoryTree(
    const QString &path,
    const bool create,
    const bool requirePrivate,
    QString &error
)
{
    if (!safePath(path)) {
        error = QStringLiteral("The path is not a clean canonical absolute path: %1")
                    .arg(path);
        errno = EINVAL;
        return -1;
    }

    int current = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (current < 0) {
        error = QStringLiteral("Cannot open the filesystem root: %1")
                    .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return -1;
    }
    struct stat rootInfo {};
    if (::fstat(current, &rootInfo) != 0 || !S_ISDIR(rootInfo.st_mode)) {
        error = QStringLiteral("Cannot validate the filesystem root");
        ::close(current);
        errno = EPERM;
        return -1;
    }
    const auto rootOwner = rootInfo.st_uid;
    const auto components = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const auto &component : components) {
        const auto encoded = QFile::encodeName(component);
        int next = ::openat(
            current,
            encoded.constData(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        );
        if (next < 0 && errno == ENOENT && create) {
            if (::mkdirat(current, encoded.constData(), privateDirectoryMode)
                != 0 && errno != EEXIST) {
                error = QStringLiteral("Cannot create directory %1: %2")
                            .arg(path, QString::fromLocal8Bit(std::strerror(errno)));
                ::close(current);
                return -1;
            }
            next = ::openat(
                current,
                encoded.constData(),
                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
            );
        }
        if (next < 0) {
            error = QStringLiteral("Cannot safely open directory %1: %2")
                        .arg(path, QString::fromLocal8Bit(std::strerror(errno)));
            ::close(current);
            return -1;
        }
        struct stat opened {};
        struct stat named {};
        const auto ancestrySafe = ::fstat(next, &opened) == 0
            && ::fstatat(current, encoded.constData(), &named,
                         AT_SYMLINK_NOFOLLOW) == 0
            && S_ISDIR(opened.st_mode) && S_ISDIR(named.st_mode)
            && opened.st_dev == named.st_dev && opened.st_ino == named.st_ino
            && opened.st_mode == named.st_mode
            && opened.st_uid == named.st_uid
            && opened.st_nlink == named.st_nlink
            && (opened.st_uid == rootOwner || opened.st_uid == ::geteuid())
            && (((opened.st_mode & 0022) == 0)
                || (opened.st_uid == rootOwner
                    && (opened.st_mode & S_ISVTX) != 0));
        if (!ancestrySafe) {
            error = QStringLiteral("Directory ancestry for %1 is writable, foreign-owned, or changed while opening")
                        .arg(path);
            ::close(next);
            ::close(current);
            errno = EPERM;
            return -1;
        }
        if (create && (!retryFsync(next) || !retryFsync(current))) {
            error = QStringLiteral("Cannot durably create directory %1: %2")
                        .arg(path, QString::fromLocal8Bit(std::strerror(errno)));
            ::close(next);
            ::close(current);
            return -1;
        }
        ::close(current);
        current = next;
    }

    struct stat info {};
    if (::fstat(current, &info) != 0 || !S_ISDIR(info.st_mode)
        || info.st_uid != ::geteuid()
        || (requirePrivate
            ? ((info.st_mode & 0777) != privateDirectoryMode)
            : ((info.st_mode & 0022) != 0))) {
        error = requirePrivate
            ? QStringLiteral("Directory %1 must be owned by this user with mode 0700").arg(path)
            : QStringLiteral("Directory %1 must be user-owned and not group/world writable").arg(path);
        ::close(current);
        errno = EPERM;
        return -1;
    }
    return current;
}

[[nodiscard]] bool retryFsync(const int descriptor)
{
    while (::fsync(descriptor) != 0) {
        if (errno != EINTR) return false;
    }
    return true;
}

[[nodiscard]] bool sameSnapshot(const struct stat &left, const struct stat &right)
{
    return left.st_dev == right.st_dev
        && left.st_ino == right.st_ino
        && left.st_mode == right.st_mode
        && left.st_nlink == right.st_nlink
        && left.st_uid == right.st_uid
        && left.st_size == right.st_size
        && left.st_mtim.tv_sec == right.st_mtim.tv_sec
        && left.st_mtim.tv_nsec == right.st_mtim.tv_nsec
        && left.st_ctim.tv_sec == right.st_ctim.tv_sec
        && left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

} // namespace

StorePaths StorePaths::standard()
{
    const auto stateBase = QStandardPaths::writableLocation(
        QStandardPaths::GenericStateLocation
    );
    const auto configBase = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation
    );
    const auto configRoot = QDir(configBase).filePath(QStringLiteral("hypr"));
    return {
        .stateRoot = QDir(stateBase).filePath(
            QStringLiteral("hyprshelld/compositor")
        ),
        .configRoot = configRoot,
        .managedConfigRoot = QDir(configRoot).filePath(
            QStringLiteral("hyprshelld")
        ),
    };
}

QString StorePaths::desiredPath() const
{
    return QDir(stateRoot).filePath(QStringLiteral("desired.json"));
}

QString StorePaths::lastGoodPath() const
{
    return QDir(stateRoot).filePath(QStringLiteral("last-good.json"));
}

QString StorePaths::activationPath() const
{
    return QDir(stateRoot).filePath(QStringLiteral("activation.json"));
}

QString StorePaths::pendingPath() const
{
    return QDir(stateRoot).filePath(QStringLiteral("pending.json"));
}

QString StorePaths::lockPath() const
{
    return QDir(stateRoot).filePath(QStringLiteral(".lock"));
}

QString StorePaths::userCustomPath() const
{
    return QDir(configRoot).filePath(QStringLiteral("user-custom.lua"));
}

QString StorePaths::stableEntrypointPath() const
{
    return QDir(configRoot).filePath(QStringLiteral("hyprland.lua"));
}

QString StorePaths::generationsPath() const
{
    return QDir(managedConfigRoot).filePath(QStringLiteral("generations"));
}

PersistentStore::PersistentStore(StorePaths paths)
    : paths_(std::move(paths))
{
}

PersistentStore::~PersistentStore()
{
    shutdown();
}

void PersistentStore::shutdown() noexcept
{
    if (managedDirectoryFd_ >= 0) ::close(managedDirectoryFd_);
    if (configDirectoryFd_ >= 0) ::close(configDirectoryFd_);
    if (leaseFd_ >= 0) ::close(leaseFd_);
    if (stateDirectoryFd_ >= 0) ::close(stateDirectoryFd_);
    if (stateParentDirectoryFd_ >= 0) ::close(stateParentDirectoryFd_);
    managedDirectoryFd_ = -1;
    configDirectoryFd_ = -1;
    leaseFd_ = -1;
    stateDirectoryFd_ = -1;
    stateParentDirectoryFd_ = -1;
    stateRootName_.clear();
}

StoreOperationResult PersistentStore::initialize()
{
    if (initialized()) {
        return failure(
            QStringLiteral("store.already-initialized"),
            QStringLiteral("The compositor store is already initialized")
        );
    }
    if (QDir(paths_.configRoot).filePath(QStringLiteral("hyprshelld"))
        != paths_.managedConfigRoot) {
        return failure(
            QStringLiteral("store.invalid-layout"),
            QStringLiteral("The managed compositor root must be configRoot/hyprshelld")
        );
    }
    const auto failAndClose = [this](StoreOperationResult result) {
        if (managedDirectoryFd_ >= 0) ::close(managedDirectoryFd_);
        if (configDirectoryFd_ >= 0) ::close(configDirectoryFd_);
        if (leaseFd_ >= 0) ::close(leaseFd_);
        if (stateDirectoryFd_ >= 0) ::close(stateDirectoryFd_);
        if (stateParentDirectoryFd_ >= 0) ::close(stateParentDirectoryFd_);
        managedDirectoryFd_ = -1;
        configDirectoryFd_ = -1;
        leaseFd_ = -1;
        stateDirectoryFd_ = -1;
        stateParentDirectoryFd_ = -1;
        stateRootName_.clear();
        return result;
    };
    QString error;
    if (!safePath(paths_.stateRoot)) {
        return failAndClose(failure(
            QStringLiteral("store.unsafe-state-root"),
            QStringLiteral("The state root is not a canonical absolute path")
        ));
    }
    const QFileInfo stateRootInfo(paths_.stateRoot);
    const auto stateParentPath = stateRootInfo.absolutePath();
    stateRootName_ = QFile::encodeName(stateRootInfo.fileName());
    if (stateRootName_.isEmpty()
        || QDir(stateParentPath).filePath(stateRootInfo.fileName())
            != paths_.stateRoot) {
        return failAndClose(failure(
            QStringLiteral("store.unsafe-state-root"),
            QStringLiteral("The state root has no safe parent/basename identity")
        ));
    }
    stateParentDirectoryFd_ = openDirectoryTree(
        stateParentPath, true, false, error
    );
    if (stateParentDirectoryFd_ < 0) {
        return failAndClose(failure(QStringLiteral("store.unsafe-state-root"), error));
    }
    if (::flock(stateParentDirectoryFd_, LOCK_EX | LOCK_NB) != 0) {
        return failAndClose(failure(
            errno == EWOULDBLOCK
                ? QStringLiteral("store.busy")
                : QStringLiteral("store.lease-unavailable"),
            errno == EWOULDBLOCK
                ? QStringLiteral("Another compositord instance owns the state parent")
                : QStringLiteral("Cannot acquire the compositor state-parent lease: %1")
                      .arg(QString::fromLocal8Bit(std::strerror(errno)))
        ));
    }
    if (::mkdirat(
            stateParentDirectoryFd_, stateRootName_.constData(),
            privateDirectoryMode
        ) != 0 && errno != EEXIST) {
        return failAndClose(failure(
            QStringLiteral("store.unsafe-state-root"),
            QStringLiteral("Cannot create the private compositor state root")
        ));
    }
    stateDirectoryFd_ = ::openat(
        stateParentDirectoryFd_, stateRootName_.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    struct stat stateOpened {};
    struct stat stateNamed {};
    const auto stateRootSafe = stateDirectoryFd_ >= 0
        && ::fstat(stateDirectoryFd_, &stateOpened) == 0
        && ::fstatat(
               stateParentDirectoryFd_, stateRootName_.constData(),
               &stateNamed, AT_SYMLINK_NOFOLLOW
           ) == 0
        && S_ISDIR(stateOpened.st_mode) && S_ISDIR(stateNamed.st_mode)
        && stateOpened.st_dev == stateNamed.st_dev
        && stateOpened.st_ino == stateNamed.st_ino
        && stateOpened.st_mode == stateNamed.st_mode
        && stateOpened.st_uid == stateNamed.st_uid
        && stateOpened.st_nlink == stateNamed.st_nlink
        && stateOpened.st_uid == ::geteuid()
        && (stateOpened.st_mode & 0777) == privateDirectoryMode;
    if (!stateRootSafe
        || !retryFsync(stateDirectoryFd_)
        || !retryFsync(stateParentDirectoryFd_)) {
        return failAndClose(failure(
            QStringLiteral("store.unsafe-state-root"),
            QStringLiteral("The compositor state root is unsafe or not durable")
        ));
    }

    leaseFd_ = ::openat(
        stateDirectoryFd_, ".lock",
        O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
        privateFileMode
    );
    if (leaseFd_ < 0) {
        return failAndClose(failure(
            QStringLiteral("store.lease-unavailable"),
            QStringLiteral("Cannot safely open the compositor lease: %1")
                .arg(QString::fromLocal8Bit(std::strerror(errno)))
        ));
    }
    struct stat leaseInfo {};
    if (::fstat(leaseFd_, &leaseInfo) != 0
        || !S_ISREG(leaseInfo.st_mode) || leaseInfo.st_nlink != 1
        || leaseInfo.st_uid != ::geteuid()
        || (leaseInfo.st_mode & 0777) != privateFileMode) {
        return failAndClose(failure(
            QStringLiteral("store.unsafe-lease"),
            QStringLiteral("The compositor lease is not a private regular file")
        ));
    }
    if (::flock(leaseFd_, LOCK_EX | LOCK_NB) != 0) {
        return failAndClose(failure(
            errno == EWOULDBLOCK
                ? QStringLiteral("store.busy")
                : QStringLiteral("store.lease-unavailable"),
            errno == EWOULDBLOCK
                ? QStringLiteral("Another compositord instance owns the store")
                : QStringLiteral("Cannot acquire the compositor lease: %1")
                      .arg(QString::fromLocal8Bit(std::strerror(errno)))
        ));
    }
    // The named file proves ownership and mode, while the retained directory
    // inode prevents unlinking/replacing `.lock` from manufacturing a second
    // lease winner inside the same state root.
    if (::flock(stateDirectoryFd_, LOCK_EX | LOCK_NB) != 0) {
        return failAndClose(failure(
            errno == EWOULDBLOCK
                ? QStringLiteral("store.busy")
                : QStringLiteral("store.lease-unavailable"),
            errno == EWOULDBLOCK
                ? QStringLiteral("Another compositord instance owns the store directory")
                : QStringLiteral("Cannot acquire the compositor directory lease: %1")
                      .arg(QString::fromLocal8Bit(std::strerror(errno)))
        ));
    }
    struct stat leasePathInfo {};
    if (::fstatat(stateDirectoryFd_, ".lock", &leasePathInfo,
                  AT_SYMLINK_NOFOLLOW) != 0
        || leasePathInfo.st_dev != leaseInfo.st_dev
        || leasePathInfo.st_ino != leaseInfo.st_ino
        || leasePathInfo.st_mode != leaseInfo.st_mode
        || leasePathInfo.st_uid != leaseInfo.st_uid
        || leasePathInfo.st_nlink != leaseInfo.st_nlink) {
        return failAndClose(failure(
            QStringLiteral("store.lease-path-changed"),
            QStringLiteral("The locked compositor lease is no longer named by .lock")
        ));
    }
    if (!retryFsync(leaseFd_) || !retryFsync(stateDirectoryFd_)
        || !leaseStillNamed()) {
        return failAndClose(failure(
            QStringLiteral("store.lease-path-changed"),
            QStringLiteral("The compositor lease or canonical state root is not durably anchored")
        ));
    }

    configDirectoryFd_ = openDirectoryTree(
        paths_.configRoot, true, false, error
    );
    if (configDirectoryFd_ < 0) {
        return failAndClose(failure(QStringLiteral("store.unsafe-config-root"), error));
    }
    managedDirectoryFd_ = openDirectoryTree(
        paths_.managedConfigRoot, true, true, error
    );
    if (managedDirectoryFd_ < 0) {
        return failAndClose(failure(QStringLiteral("store.unsafe-managed-root"), error));
    }
    if (!managedDirectoryStillNamed()) {
        return failAndClose(failure(
            QStringLiteral("store.unsafe-managed-root"),
            QStringLiteral("The managed compositor root changed while opening")
        ));
    }
    return {.success = true};
}

bool PersistentStore::managedDirectoryStillNamed() const
{
    if (configDirectoryFd_ < 0 || managedDirectoryFd_ < 0) return false;
    struct stat opened {};
    struct stat named {};
    return ::fstat(managedDirectoryFd_, &opened) == 0
        && ::fstatat(configDirectoryFd_, "hyprshelld", &named,
                     AT_SYMLINK_NOFOLLOW) == 0
        && S_ISDIR(opened.st_mode) && S_ISDIR(named.st_mode)
        && opened.st_dev == named.st_dev && opened.st_ino == named.st_ino
        && opened.st_mode == named.st_mode && opened.st_uid == named.st_uid
        && opened.st_nlink == named.st_nlink
        && opened.st_uid == ::geteuid()
        && (opened.st_mode & 0777) == privateDirectoryMode;
}

const char *PersistentStore::fileName(const StoreFile file) const
{
    switch (file) {
    case StoreFile::Desired: return "desired.json";
    case StoreFile::LastGood: return "last-good.json";
    case StoreFile::Activation: return "activation.json";
    case StoreFile::Pending: return "pending.json";
    }
    return "";
}

bool PersistentStore::leaseStillNamed() const
{
    if (leaseFd_ < 0 || stateParentDirectoryFd_ < 0
        || stateDirectoryFd_ < 0 || stateRootName_.isEmpty()) return false;
    struct stat stateOpened {};
    struct stat stateNamed {};
    if (::fstat(stateDirectoryFd_, &stateOpened) != 0
        || ::fstatat(
               stateParentDirectoryFd_, stateRootName_.constData(),
               &stateNamed, AT_SYMLINK_NOFOLLOW
           ) != 0
        || !S_ISDIR(stateOpened.st_mode) || !S_ISDIR(stateNamed.st_mode)
        || stateOpened.st_dev != stateNamed.st_dev
        || stateOpened.st_ino != stateNamed.st_ino
        || stateOpened.st_mode != stateNamed.st_mode
        || stateOpened.st_uid != stateNamed.st_uid
        || stateOpened.st_nlink != stateNamed.st_nlink
        || stateOpened.st_uid != ::geteuid()
        || (stateOpened.st_mode & 0777) != privateDirectoryMode) {
        return false;
    }
    QString canonicalError;
    const auto canonical = openDirectoryTree(
        paths_.stateRoot, false, true, canonicalError
    );
    if (canonical < 0) return false;
    struct stat canonicalState {};
    const auto canonicalMatches = ::fstat(canonical, &canonicalState) == 0
        && canonicalState.st_dev == stateOpened.st_dev
        && canonicalState.st_ino == stateOpened.st_ino
        && canonicalState.st_mode == stateOpened.st_mode
        && canonicalState.st_uid == stateOpened.st_uid
        && canonicalState.st_nlink == stateOpened.st_nlink;
    ::close(canonical);
    if (!canonicalMatches) return false;
    struct stat opened {};
    struct stat named {};
    return ::fstat(leaseFd_, &opened) == 0
        && ::fstatat(stateDirectoryFd_, ".lock", &named,
                     AT_SYMLINK_NOFOLLOW) == 0
        && S_ISREG(opened.st_mode) && S_ISREG(named.st_mode)
        && opened.st_dev == named.st_dev && opened.st_ino == named.st_ino
        && opened.st_mode == named.st_mode && opened.st_uid == named.st_uid
        && opened.st_nlink == named.st_nlink && opened.st_nlink == 1
        && opened.st_uid == ::geteuid()
        && (opened.st_mode & 0777) == privateFileMode;
}

StoreReadResult PersistentStore::read(const StoreFile file) const
{
    if (!initialized() || !leaseStillNamed()) {
        return {
            .status = StoreReadStatus::Unreadable,
            .errorCode = QStringLiteral("store.uninitialized"),
            .errorMessage = QStringLiteral("The compositor store lease is unavailable or no longer named"),
        };
    }
    const auto descriptor = ::openat(
        stateDirectoryFd_, fileName(file),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    );
    if (descriptor < 0) {
        if (errno == ENOENT) return {};
        return {
            .status = errno == ELOOP ? StoreReadStatus::Unsafe
                                     : StoreReadStatus::Unreadable,
            .errorCode = errno == ELOOP
                ? QStringLiteral("store.unsafe-file")
                : QStringLiteral("store.read-failed"),
            .errorMessage = QStringLiteral("Cannot safely read %1: %2")
                .arg(QString::fromLatin1(fileName(file)),
                     QString::fromLocal8Bit(std::strerror(errno))),
        };
    }
    struct stat before {};
    if (::fstat(descriptor, &before) != 0 || !S_ISREG(before.st_mode)
        || before.st_nlink != 1 || before.st_uid != ::geteuid()
        || (before.st_mode & 0777) != privateFileMode) {
        ::close(descriptor);
        return {
            .status = StoreReadStatus::Unsafe,
            .errorCode = QStringLiteral("store.unsafe-file"),
            .errorMessage = QStringLiteral("A persistent compositor file is not private and regular"),
        };
    }
    const qsizetype maximum = file == StoreFile::Desired
        || file == StoreFile::LastGood
        ? Hyprland::maximumDesiredStateBytes : maximumMetadataBytes;
    if (before.st_size < 0 || before.st_size > maximum) {
        ::close(descriptor);
        return {
            .status = StoreReadStatus::Oversized,
            .errorCode = QStringLiteral("store.file-oversized"),
            .errorMessage = QStringLiteral("A persistent compositor file exceeds its size limit"),
        };
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
            return {
                .status = StoreReadStatus::Unreadable,
                .errorCode = QStringLiteral("store.read-failed"),
                .errorMessage = QStringLiteral("Cannot read a persistent compositor file"),
            };
        }
        if (bytes.size() > maximum - count) {
            ::close(descriptor);
            return {
                .status = StoreReadStatus::Oversized,
                .errorCode = QStringLiteral("store.file-oversized"),
                .errorMessage = QStringLiteral("A persistent compositor file exceeds its size limit"),
            };
        }
        bytes.append(buffer.data(), count);
    }
    struct stat after {};
    struct stat named {};
    const auto stable = ::fstat(descriptor, &after) == 0
        && sameSnapshot(before, after)
        && ::fstatat(stateDirectoryFd_, fileName(file), &named,
                     AT_SYMLINK_NOFOLLOW) == 0
        && named.st_dev == after.st_dev && named.st_ino == after.st_ino
        && named.st_mode == after.st_mode && named.st_uid == after.st_uid
        && named.st_nlink == after.st_nlink
        && bytes.size() == after.st_size;
    ::close(descriptor);
    if (!stable) {
        return {
            .status = StoreReadStatus::Unsafe,
            .errorCode = QStringLiteral("store.file-changed"),
            .errorMessage = QStringLiteral("A persistent compositor file changed while being read"),
        };
    }
    return {.status = StoreReadStatus::Present, .bytes = std::move(bytes)};
}

StoreOperationResult PersistentStore::write(
    const StoreFile file,
    const QByteArrayView bytes
)
{
    if (!initialized() || !leaseStillNamed()) {
        return failure(QStringLiteral("store.uninitialized"),
                       QStringLiteral("The compositor store lease is unavailable or no longer named"));
    }
    const qsizetype maximum = file == StoreFile::Desired
        || file == StoreFile::LastGood
        ? Hyprland::maximumDesiredStateBytes : maximumMetadataBytes;
    if (bytes.isEmpty() || bytes.size() > maximum) {
        return failure(QStringLiteral("store.invalid-write"),
                       QStringLiteral("The persistent payload is empty or oversized"));
    }
    struct stat existing {};
    if (::fstatat(stateDirectoryFd_, fileName(file), &existing,
                  AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(existing.st_mode) || existing.st_nlink != 1
            || existing.st_uid != ::geteuid()
            || (existing.st_mode & 0777) != privateFileMode) {
            return failure(QStringLiteral("store.unsafe-target"),
                           QStringLiteral("Refusing to replace an unsafe persistent target"));
        }
    } else if (errno != ENOENT) {
        return failure(QStringLiteral("store.target-probe-failed"),
                       QStringLiteral("Cannot inspect the persistent target"));
    }

    QByteArray temporary;
    int descriptor = -1;
    for (int attempt = 0; attempt < 32 && descriptor < 0; ++attempt) {
        temporary = QByteArrayLiteral(".tmp-")
            + QByteArray::number(static_cast<qulonglong>(::getpid())) + '-'
            + QByteArray::number(temporaryCounter.fetch_add(1));
        descriptor = ::openat(
            stateDirectoryFd_, temporary.constData(),
            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
            privateFileMode
        );
        if (descriptor < 0 && errno != EEXIST) break;
    }
    if (descriptor < 0) {
        return failure(QStringLiteral("store.temp-create-failed"),
                       QStringLiteral("Cannot create a private temporary persistent file"));
    }
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::write(
            descriptor, bytes.data() + offset,
            static_cast<size_t>(bytes.size() - offset)
        );
        if (count < 0) {
            if (errno == EINTR) continue;
            break;
        }
        offset += count;
    }
    const auto savedWriteError = errno;
    const auto written = offset == bytes.size()
        && ::fchmod(descriptor, privateFileMode) == 0
        && retryFsync(descriptor);
    ::close(descriptor);
    if (!written) {
        if (::unlinkat(stateDirectoryFd_, temporary.constData(), 0) == 0) {
            (void)retryFsync(stateDirectoryFd_);
        }
        return failure(QStringLiteral("store.write-failed"),
                       QStringLiteral("Cannot durably write persistent data: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(savedWriteError))));
    }
    if (paths_.faultHook
        && paths_.faultHook(StoreFaultPoint::BeforePublishRename, file)) {
        if (::unlinkat(stateDirectoryFd_, temporary.constData(), 0) == 0) {
            (void)retryFsync(stateDirectoryFd_);
        }
        return failure(
            QStringLiteral("store.injected-pre-publish-failure"),
            QStringLiteral("Injected failure before persistent publication")
        );
    }
    if (::renameat(stateDirectoryFd_, temporary.constData(),
                   stateDirectoryFd_, fileName(file)) != 0) {
        ::unlinkat(stateDirectoryFd_, temporary.constData(), 0);
        return failure(QStringLiteral("store.rename-failed"),
                       QStringLiteral("Cannot publish persistent data atomically"));
    }
    const auto injectedPostRename = paths_.faultHook
        && paths_.faultHook(
            StoreFaultPoint::AfterPublishRenameBeforeDirectorySync, file
        );
    if (injectedPostRename || !retryFsync(stateDirectoryFd_)) {
        return {
            .success = false,
            .committedButNotDurable = true,
            .errorCode = QStringLiteral("store.directory-sync-failed"),
            .errorMessage = QStringLiteral("Persistent data was published, but its directory sync failed"),
        };
    }
    return {.success = true};
}

StoreOperationResult PersistentStore::remove(const StoreFile file)
{
    if (!initialized() || !leaseStillNamed()) {
        return failure(QStringLiteral("store.uninitialized"),
                       QStringLiteral("The compositor store lease is unavailable or no longer named"));
    }
    struct stat existing {};
    if (::fstatat(stateDirectoryFd_, fileName(file), &existing,
                  AT_SYMLINK_NOFOLLOW) != 0) {
        if (errno == ENOENT) return {.success = true};
        return failure(QStringLiteral("store.target-probe-failed"),
                       QStringLiteral("Cannot inspect the persistent target"));
    }
    if (!S_ISREG(existing.st_mode) || existing.st_nlink != 1
        || existing.st_uid != ::geteuid()
        || (existing.st_mode & 0777) != privateFileMode) {
        return failure(QStringLiteral("store.unsafe-target"),
                       QStringLiteral("Refusing to remove an unsafe persistent target"));
    }
    if (::unlinkat(stateDirectoryFd_, fileName(file), 0) != 0) {
        return failure(QStringLiteral("store.remove-failed"),
                       QStringLiteral("Cannot remove persistent transaction state"));
    }
    const auto injectedPostRemove = paths_.faultHook
        && paths_.faultHook(
            StoreFaultPoint::AfterRemoveBeforeDirectorySync, file
        );
    if (injectedPostRemove || !retryFsync(stateDirectoryFd_)) {
        return {
            .success = false,
            .committedButNotDurable = true,
            .errorCode = QStringLiteral("store.directory-sync-failed"),
            .errorMessage = QStringLiteral("Persistent data was removed, but its directory sync failed"),
        };
    }
    return {.success = true};
}

} // namespace HyprShelld::Compositor
