#include "component_inspection_sessions.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace HyprShelld {
namespace {

constexpr auto spoolFileName = "package.hyprshelld-component";
constexpr auto reportFileName = "inspection-report.json";
constexpr auto materializedFileName = "materialized.bundle";
constexpr auto lockFileName = ".component-inspections.lock";
constexpr qint64 maximumReportBytes = 1024 * 1024;
constexpr qsizetype maximumActiveSessions = 8;
constexpr qsizetype maximumSessionsPerSender = 2;
constexpr qsizetype maximumExpiredTombstones = 64;
constexpr qint64 maximumMaterializedBytes =
    Components::maximumComponentExpandedBytes
    + Components::maximumComponentArchiveEntries
        * (static_cast<qint64>(sizeof(quint16)) + 255
           + static_cast<qint64>(sizeof(quint64)))
    + static_cast<qint64>(sizeof(quint32));

const QRegularExpression &tokenPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-f]{32}$")
    );
    return pattern;
}

const QRegularExpression &digestPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return pattern;
}

const QRegularExpression &senderPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^:[A-Za-z0-9_-]+(?:\\.[A-Za-z0-9_-]+)+$")
    );
    return pattern;
}

QString managerError(const QString &suffix)
{
    return QStringLiteral("org.hyprshelld.ComponentManager1.Error.%1")
        .arg(suffix);
}

QString systemError(const char *operation)
{
    return QStringLiteral("%1: %2")
        .arg(
            QString::fromLatin1(operation),
            QString::fromLocal8Bit(std::strerror(errno))
        );
}

class UniqueFd final {
public:
    UniqueFd() = default;
    explicit UniqueFd(const int descriptor)
        : descriptor_(descriptor)
    {
    }

    ~UniqueFd()
    {
        if (descriptor_ >= 0) {
            ::close(descriptor_);
        }
    }

    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;

    UniqueFd(UniqueFd &&other) noexcept
        : descriptor_(std::exchange(other.descriptor_, -1))
    {
    }

    UniqueFd &operator=(UniqueFd &&other) noexcept
    {
        if (this != &other) {
            if (descriptor_ >= 0) {
                ::close(descriptor_);
            }
            descriptor_ = std::exchange(other.descriptor_, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const
    {
        return descriptor_;
    }

    [[nodiscard]] explicit operator bool() const
    {
        return descriptor_ >= 0;
    }

private:
    int descriptor_ = -1;
};

struct FileIdentity final {
    quint64 device = 0;
    quint64 inode = 0;
    quint64 size = 0;
    qint64 modifiedSeconds = 0;
    qint64 modifiedNanoseconds = 0;
    qint64 changedSeconds = 0;
    qint64 changedNanoseconds = 0;

    friend bool operator==(const FileIdentity &, const FileIdentity &) =
        default;
};

FileIdentity identityFor(const struct stat &status)
{
    return {
        .device = static_cast<quint64>(status.st_dev),
        .inode = static_cast<quint64>(status.st_ino),
        .size = static_cast<quint64>(status.st_size),
        .modifiedSeconds = status.st_mtim.tv_sec,
        .modifiedNanoseconds = status.st_mtim.tv_nsec,
        .changedSeconds = status.st_ctim.tv_sec,
        .changedNanoseconds = status.st_ctim.tv_nsec,
    };
}

bool isPrivateRegularFile(
    const struct stat &status,
    const qint64 minimumSize,
    const qint64 maximumSize
)
{
    return S_ISREG(status.st_mode) && status.st_uid == geteuid()
        && status.st_nlink == 1 && (status.st_mode & 0077) == 0
        && status.st_size >= minimumSize && status.st_size <= maximumSize;
}

bool statPrivateFile(
    const QString &path,
    const qint64 minimumSize,
    const qint64 maximumSize,
    FileIdentity &identity,
    QString &error
)
{
    const auto encoded = QFile::encodeName(path);
    const UniqueFd descriptor(::open(
        encoded.constData(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!descriptor) {
        error = systemError("Cannot open a staged inspection file");
        return false;
    }
    struct stat status {};
    if (::fstat(descriptor.get(), &status) != 0
        || !isPrivateRegularFile(status, minimumSize, maximumSize)) {
        error = QStringLiteral(
            "A staged inspection file has unsafe type, ownership, links, permissions, or size"
        );
        return false;
    }
    identity = identityFor(status);
    return true;
}

bool readPrivateFile(
    const QString &path,
    const qint64 minimumSize,
    const qint64 maximumSize,
    QByteArray &bytes,
    FileIdentity &identity,
    QString &error
)
{
    const auto encoded = QFile::encodeName(path);
    const UniqueFd descriptor(::open(
        encoded.constData(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!descriptor) {
        error = systemError("Cannot open a staged inspection file");
        return false;
    }

    struct stat before {};
    if (::fstat(descriptor.get(), &before) != 0
        || !isPrivateRegularFile(before, minimumSize, maximumSize)) {
        error = QStringLiteral(
            "A staged inspection file has unsafe type, ownership, links, permissions, or size"
        );
        return false;
    }

    bytes.resize(static_cast<qsizetype>(before.st_size));
    qint64 offset = 0;
    while (offset < before.st_size) {
        const auto count = ::pread(
            descriptor.get(),
            bytes.data() + offset,
            static_cast<size_t>(before.st_size - offset),
            static_cast<off_t>(offset)
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            error = QStringLiteral("A staged inspection file was truncated while reading");
            return false;
        }
        offset += count;
    }

    char trailing = 0;
    while (true) {
        const auto count = ::pread(
            descriptor.get(),
            &trailing,
            1,
            static_cast<off_t>(before.st_size)
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count != 0) {
            error = QStringLiteral("A staged inspection file changed while reading");
            return false;
        }
        break;
    }

    struct stat after {};
    if (::fstat(descriptor.get(), &after) != 0
        || identityFor(after) != identityFor(before)) {
        error = QStringLiteral("A staged inspection file changed while reading");
        return false;
    }
    identity = identityFor(after);
    return true;
}

bool hashPrivateFile(
    const QString &path,
    const qint64 expectedSize,
    QString &digest,
    FileIdentity &identity,
    QString &error
)
{
    const auto encoded = QFile::encodeName(path);
    const UniqueFd descriptor(::open(
        encoded.constData(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!descriptor) {
        error = systemError("Cannot open the staged package");
        return false;
    }
    struct stat before {};
    if (::fstat(descriptor.get(), &before) != 0
        || !isPrivateRegularFile(
            before,
            expectedSize,
            expectedSize
        )) {
        error = QStringLiteral("The staged package file is no longer trustworthy");
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 64 * 1024> buffer{};
    qint64 offset = 0;
    while (offset < expectedSize) {
        const auto wanted = static_cast<size_t>(std::min<qint64>(
            buffer.size(),
            expectedSize - offset
        ));
        const auto count = ::pread(
            descriptor.get(),
            buffer.data(),
            wanted,
            static_cast<off_t>(offset)
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            error = QStringLiteral("The staged package changed while hashing");
            return false;
        }
        hash.addData(QByteArrayView(buffer.data(), count));
        offset += count;
    }

    struct stat after {};
    if (::fstat(descriptor.get(), &after) != 0
        || identityFor(after) != identityFor(before)) {
        error = QStringLiteral("The staged package changed while hashing");
        return false;
    }
    identity = identityFor(after);
    digest = QString::fromLatin1(hash.result().toHex());
    return true;
}

bool writeAll(const int descriptor, const char *bytes, const qint64 size)
{
    qint64 offset = 0;
    while (offset < size) {
        const auto count = ::write(
            descriptor,
            bytes + offset,
            static_cast<size_t>(size - offset)
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        offset += count;
    }
    return true;
}

bool sourceMetadataUnchanged(
    const struct stat &before,
    const struct stat &after
)
{
    return before.st_dev == after.st_dev && before.st_ino == after.st_ino
        && before.st_size == after.st_size
        && before.st_mtim.tv_sec == after.st_mtim.tv_sec
        && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec
        && before.st_ctim.tv_sec == after.st_ctim.tv_sec
        && before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
}

bool fsyncRetry(const int descriptor)
{
    while (::fsync(descriptor) != 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    return true;
}

bool ensurePrivateRoot(const QString &path, UniqueFd &root, QString &error)
{
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()
        || path.contains(QChar::Null)
        || QDir::cleanPath(path) != path) {
        error = QStringLiteral("The inspection spool root is invalid");
        return false;
    }
    if (!QDir().mkpath(path)) {
        error = QStringLiteral("Cannot create the inspection spool root");
        return false;
    }

    const auto encoded = QFile::encodeName(path);
    UniqueFd candidate(::open(
        encoded.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!candidate) {
        error = systemError("Cannot open the inspection spool root");
        return false;
    }
    struct stat status {};
    if (::fstat(candidate.get(), &status) != 0
        || !S_ISDIR(status.st_mode) || status.st_uid != geteuid()) {
        error = QStringLiteral(
            "The inspection spool root has unsafe type or ownership"
        );
        return false;
    }
    if (::fchmod(candidate.get(), 0700) != 0) {
        error = systemError("Cannot secure the inspection spool root");
        return false;
    }
    root = std::move(candidate);
    return true;
}

bool acquireSpoolRootLock(
    const QString &path,
    UniqueFd &root,
    UniqueFd &lock,
    QString &error
)
{
    UniqueFd candidateRoot;
    if (!ensurePrivateRoot(path, candidateRoot, error)) {
        return false;
    }

    UniqueFd candidateLock(::openat(
        candidateRoot.get(),
        lockFileName,
        O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
        0600
    ));
    if (!candidateLock) {
        error = systemError("Cannot open the inspection spool lock");
        return false;
    }

    struct stat status {};
    if (::fstat(candidateLock.get(), &status) != 0
        || !S_ISREG(status.st_mode) || status.st_uid != geteuid()
        || status.st_nlink != 1) {
        error = QStringLiteral(
            "The inspection spool lock has unsafe type, ownership, or links"
        );
        return false;
    }
    if (::fchmod(candidateLock.get(), 0600) != 0) {
        error = systemError("Cannot protect the inspection spool lock");
        return false;
    }
    if (::flock(candidateLock.get(), LOCK_EX | LOCK_NB) != 0) {
        error = errno == EWOULDBLOCK || errno == EAGAIN
            ? QStringLiteral(
                "Another component manager owns the inspection spool"
            )
            : systemError("Cannot lock the inspection spool");
        return false;
    }

    root = std::move(candidateRoot);
    lock = std::move(candidateLock);
    return true;
}

void cleanupFixedDirectoryAt(
    const int rootDescriptor,
    const QString &token
)
{
    if (rootDescriptor < 0 || !tokenPattern().match(token).hasMatch()) {
        return;
    }
    const auto encodedToken = token.toUtf8();
    const UniqueFd directory(::openat(
        rootDescriptor,
        encodedToken.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!directory) {
        return;
    }
    ::fchmod(directory.get(), 0700);
    for (const auto *name : {
             spoolFileName,
             reportFileName,
             materializedFileName,
         }) {
        ::unlinkat(directory.get(), name, 0);
    }
    ::unlinkat(rootDescriptor, encodedToken.constData(), AT_REMOVEDIR);
}

void cleanupStaleSessionDirectories(const int lockedRootDescriptor)
{
    if (lockedRootDescriptor < 0) {
        return;
    }
    const auto scanDescriptor = ::fcntl(
        lockedRootDescriptor,
        F_DUPFD_CLOEXEC,
        0
    );
    if (scanDescriptor < 0) {
        return;
    }
    auto *directory = ::fdopendir(scanDescriptor);
    if (directory == nullptr) {
        ::close(scanDescriptor);
        return;
    }
    while (const auto *entry = ::readdir(directory)) {
        const auto token = QString::fromLatin1(entry->d_name);
        if (tokenPattern().match(token).hasMatch()) {
            cleanupFixedDirectoryAt(lockedRootDescriptor, token);
        }
    }
    ::closedir(directory);
}

bool freezeSessionFiles(
    const QString &directoryPath,
    const QString &spoolPath,
    const QString &reportPath,
    const QString &materializedPath,
    QString &error
)
{
    Q_UNUSED(spoolPath)
    Q_UNUSED(reportPath)
    Q_UNUSED(materializedPath)
    const auto encodedDirectory = QFile::encodeName(directoryPath);
    const UniqueFd directory(::open(
        encodedDirectory.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!directory) {
        error = systemError("Cannot open an inspection session to freeze it");
        return false;
    }
    for (const auto *name : {
             spoolFileName,
             reportFileName,
             materializedFileName,
         }) {
        const UniqueFd descriptor(::openat(
            directory.get(),
            name,
            O_RDONLY | O_CLOEXEC | O_NOFOLLOW
        ));
        struct stat status {};
        if (!descriptor || ::fstat(descriptor.get(), &status) != 0
            || !isPrivateRegularFile(status, 0, maximumMaterializedBytes)
            || ::fchmod(descriptor.get(), 0400) != 0) {
            error = systemError("Cannot freeze an inspection output");
            return false;
        }
    }
    if (::fchmod(directory.get(), 0500) != 0) {
        error = systemError("Cannot freeze an inspection session");
        return false;
    }
    return true;
}

ComponentInspectionOperationResult operationError(
    const QString &name,
    const QString &message
)
{
    return {
        .success = false,
        .errorName = name,
        .errorMessage = message,
    };
}

} // namespace

struct ComponentInspectionSessions::Private final {
    struct Session final {
        QString owner;
        QString token;
        QString archiveDigest;
        quint64 archiveSize = 0;
        qint64 expiresAtMs = 0;
        ComponentInspectionState state = ComponentInspectionState::Pending;
        QString directoryPath;
        QString spoolPath;
        QString reportPath;
        QString materializedPath;
        QByteArray reportBytes;
        std::optional<Components::PackageInspectionReport> report;
        QString errorName;
        QString errorMessage;
        FileIdentity spoolIdentity;
        FileIdentity reportIdentity;
        FileIdentity materializedIdentity;
    };

    struct Tombstone final {
        QString owner;
        qint64 purgeAtMs = 0;
    };

    QString spoolRoot;
    UniqueFd rootDescriptor;
    UniqueFd lockDescriptor;
    QString unavailableReason;
    std::unique_ptr<ComponentInspectorLauncher> launcher;
    qint64 timeToLiveMs = 0;
    Clock clock;
    QTimer *expiryTimer = nullptr;
    QHash<QString, Session> sessions;
    QHash<QString, Tombstone> expired;

    [[nodiscard]] bool ownsSpool() const
    {
        return rootDescriptor.get() >= 0 && lockDescriptor.get() >= 0;
    }

    [[nodiscard]] qint64 now() const
    {
        return clock ? clock() : QDateTime::currentMSecsSinceEpoch();
    }

    [[nodiscard]] qsizetype countForSender(const QString &sender) const
    {
        return std::ranges::count_if(
            sessions,
            [&sender](const Session &session) {
                return session.owner == sender;
            }
        );
    }

    void addExpired(const Session &session, const qint64 currentTime)
    {
        expired.insert(session.token, {
            .owner = session.owner,
            .purgeAtMs = currentTime + timeToLiveMs,
        });
        while (expired.size() > maximumExpiredTombstones) {
            expired.erase(expired.begin());
        }
    }

    void pruneExpired(const qint64 currentTime)
    {
        const auto tokens = expired.keys();
        for (const auto &token : tokens) {
            if (expired.value(token).purgeAtMs <= currentTime) {
                expired.remove(token);
            }
        }
    }

    [[nodiscard]] bool loadSuccessfulOutputs(
        Session &session,
        QString &error
    )
    {
        if (!freezeSessionFiles(
                session.directoryPath,
                session.spoolPath,
                session.reportPath,
                session.materializedPath,
                error
            )) {
            return false;
        }

        QString spoolDigest;
        if (!hashPrivateFile(
                session.spoolPath,
                static_cast<qint64>(session.archiveSize),
                spoolDigest,
                session.spoolIdentity,
                error
            )
            || spoolDigest != session.archiveDigest
            || !readPrivateFile(
                session.reportPath,
                1,
                maximumReportBytes,
                session.reportBytes,
                session.reportIdentity,
                error
            )
            || !statPrivateFile(
                session.materializedPath,
                1,
                maximumMaterializedBytes,
                session.materializedIdentity,
                error
            )) {
            if (error.isEmpty()) {
                error = QStringLiteral(
                    "The inspector outputs do not match the staged package"
                );
            }
            return false;
        }

        auto parsed = Components::parsePackageInspectionReport(
            QByteArrayView(session.reportBytes)
        );
        if (!parsed || parsed.value->inspectionToken != session.token
            || parsed.value->archiveSha256 != session.archiveDigest
            || parsed.value->archiveSize != session.archiveSize) {
            error = QStringLiteral(
                "The inspector returned a malformed or mismatched report"
            );
            return false;
        }
        session.report = std::move(*parsed.value);
        return true;
    }

    [[nodiscard]] bool revalidateComplete(
        Session &session,
        QString &error
    )
    {
        if (session.state != ComponentInspectionState::Complete
            || !session.report.has_value()) {
            error = QStringLiteral("The inspection is not complete");
            return false;
        }

        QString spoolDigest;
        FileIdentity spool;
        FileIdentity report;
        FileIdentity materialized;
        QByteArray reportBytes;
        if (!hashPrivateFile(
                session.spoolPath,
                static_cast<qint64>(session.archiveSize),
                spoolDigest,
                spool,
                error
            )
            || !readPrivateFile(
                session.reportPath,
                1,
                maximumReportBytes,
                reportBytes,
                report,
                error
            )
            || !statPrivateFile(
                session.materializedPath,
                1,
                maximumMaterializedBytes,
                materialized,
                error
            )
            || spool != session.spoolIdentity
            || report != session.reportIdentity
            || materialized != session.materializedIdentity
            || spoolDigest != session.archiveDigest
            || reportBytes != session.reportBytes) {
            if (error.isEmpty()) {
                error = QStringLiteral(
                    "The accepted inspection artifacts changed after validation"
                );
            }
            return false;
        }
        return true;
    }
};

ComponentInspectionArtifact::~ComponentInspectionArtifact()
{
    cleanup();
}

ComponentInspectionArtifact::ComponentInspectionArtifact(
    ComponentInspectionArtifact &&other
) noexcept
    : token(std::move(other.token))
    , archiveDigest(std::move(other.archiveDigest))
    , archiveSize(other.archiveSize)
    , reportBytes(std::move(other.reportBytes))
    , report(std::move(other.report))
    , spoolPath(std::move(other.spoolPath))
    , materializedPath(std::move(other.materializedPath))
    , sessionDirectory_(std::exchange(other.sessionDirectory_, {}))
    , rootDescriptor_(std::exchange(other.rootDescriptor_, -1))
    , lockDescriptor_(std::exchange(other.lockDescriptor_, -1))
{
}

ComponentInspectionArtifact &ComponentInspectionArtifact::operator=(
    ComponentInspectionArtifact &&other
) noexcept
{
    if (this != &other) {
        cleanup();
        token = std::move(other.token);
        archiveDigest = std::move(other.archiveDigest);
        archiveSize = other.archiveSize;
        reportBytes = std::move(other.reportBytes);
        report = std::move(other.report);
        spoolPath = std::move(other.spoolPath);
        materializedPath = std::move(other.materializedPath);
        sessionDirectory_ = std::exchange(other.sessionDirectory_, {});
        rootDescriptor_ = std::exchange(other.rootDescriptor_, -1);
        lockDescriptor_ = std::exchange(other.lockDescriptor_, -1);
    }
    return *this;
}

void ComponentInspectionArtifact::cleanup()
{
    if (rootDescriptor_ >= 0 && lockDescriptor_ >= 0) {
        cleanupFixedDirectoryAt(rootDescriptor_, token);
    }
    if (rootDescriptor_ >= 0) {
        ::close(rootDescriptor_);
        rootDescriptor_ = -1;
    }
    if (lockDescriptor_ >= 0) {
        ::close(lockDescriptor_);
        lockDescriptor_ = -1;
    }
    sessionDirectory_.clear();
}

ComponentInspectionSessions::ComponentInspectionSessions(
    QString spoolRoot,
    std::unique_ptr<ComponentInspectorLauncher> launcher,
    const qint64 timeToLiveMs,
    Clock clock,
    QObject *parent
)
    : QObject(parent)
    , d_(std::make_unique<Private>(Private{
          .spoolRoot = QDir::cleanPath(std::move(spoolRoot)),
          .rootDescriptor = {},
          .lockDescriptor = {},
          .unavailableReason = {},
          .launcher = std::move(launcher),
          .timeToLiveMs = timeToLiveMs,
          .clock = std::move(clock),
          .expiryTimer = nullptr,
          .sessions = {},
          .expired = {},
      }))
{
    if (!acquireSpoolRootLock(
            d_->spoolRoot,
            d_->rootDescriptor,
            d_->lockDescriptor,
            d_->unavailableReason
        )) {
        d_->rootDescriptor = UniqueFd();
        d_->lockDescriptor = UniqueFd();
        return;
    }
    cleanupStaleSessionDirectories(d_->rootDescriptor.get());
    d_->expiryTimer = new QTimer(this);
    d_->expiryTimer->setInterval(static_cast<int>(std::clamp<qint64>(
        d_->timeToLiveMs / 4,
        1000,
        30000
    )));
    connect(
        d_->expiryTimer,
        &QTimer::timeout,
        this,
        &ComponentInspectionSessions::expireNow
    );
    if (d_->timeToLiveMs > 0) {
        d_->expiryTimer->start();
    }
}

ComponentInspectionSessions::~ComponentInspectionSessions()
{
    if (!d_->ownsSpool()) {
        return;
    }
    const auto tokens = d_->sessions.keys();
    for (const auto &token : tokens) {
        d_->launcher->cancel(token);
        cleanupFixedDirectoryAt(d_->rootDescriptor.get(), token);
    }
}

ComponentInspectionBeginResult ComponentInspectionSessions::begin(
    const QString &sender,
    const int packageFileDescriptor
)
{
    ComponentInspectionBeginResult result;
    if (!d_->ownsSpool() || !d_->launcher || d_->timeToLiveMs <= 0
        || sender.size() > 255
        || !senderPattern().match(sender).hasMatch()) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                d_->unavailableReason.isEmpty()
                    ? QStringLiteral("Package inspection is unavailable")
                    : d_->unavailableReason
            );
        return result;
    }
    expireNow();
    if (d_->sessions.size() >= maximumActiveSessions
        || d_->countForSender(sender) >= maximumSessionsPerSender) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                QStringLiteral("Too many package inspections are active")
            );
        return result;
    }

    const auto sourceFlags = ::fcntl(packageFileDescriptor, F_GETFL);
    struct stat sourceBefore {};
    if (sourceFlags < 0 || (sourceFlags & O_ACCMODE) != O_RDONLY
        || ::fstat(packageFileDescriptor, &sourceBefore) != 0
        || !S_ISREG(sourceBefore.st_mode) || sourceBefore.st_size <= 0
        || sourceBefore.st_size > Components::maximumComponentArchiveBytes) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InvalidPackageDescriptor")),
                QStringLiteral(
                    "The package descriptor must be a read-only, nonempty regular file no larger than 32 MiB"
                )
            );
        return result;
    }

    QString fileError;
    const auto rootDescriptor = d_->rootDescriptor.get();

    QString token;
    QString directoryPath;
    UniqueFd directoryDescriptor;
    for (int attempt = 0; attempt < 8; ++attempt) {
        token = QUuid::createUuid()
                    .toString(QUuid::WithoutBraces)
                    .remove(QLatin1Char('-'));
        if (!tokenPattern().match(token).hasMatch()
            || d_->sessions.contains(token)) {
            continue;
        }
        const auto encodedToken = token.toUtf8();
        if (::mkdirat(rootDescriptor, encodedToken.constData(), 0700)
            != 0) {
            if (errno == EEXIST) {
                continue;
            }
            fileError = systemError("Cannot create an inspection session");
            break;
        }
        directoryDescriptor = UniqueFd(::openat(
            rootDescriptor,
            encodedToken.constData(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        ));
        if (!directoryDescriptor) {
            fileError = systemError("Cannot open an inspection session");
            directoryPath = QDir(d_->spoolRoot).filePath(token);
            cleanupFixedDirectoryAt(rootDescriptor, token);
            token.clear();
            break;
        }
        directoryPath = QDir(d_->spoolRoot).filePath(token);
        break;
    }
    if (!directoryDescriptor) {
        if (fileError.isEmpty()) {
            fileError = QStringLiteral(
                "Cannot allocate a unique inspection session"
            );
        }
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                fileError
            );
        return result;
    }

    const UniqueFd spoolDescriptor(::openat(
        directoryDescriptor.get(),
        spoolFileName,
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600
    ));
    const UniqueFd reportDescriptor(::openat(
        directoryDescriptor.get(),
        reportFileName,
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600
    ));
    const UniqueFd materializedDescriptor(::openat(
        directoryDescriptor.get(),
        materializedFileName,
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600
    ));
    if (!spoolDescriptor || !reportDescriptor || !materializedDescriptor
        || ::fchmod(spoolDescriptor.get(), 0600) != 0
        || ::fchmod(reportDescriptor.get(), 0600) != 0
        || ::fchmod(materializedDescriptor.get(), 0600) != 0) {
        fileError = systemError("Cannot create private inspection files");
        cleanupFixedDirectoryAt(rootDescriptor, token);
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                fileError
            );
        return result;
    }

    QCryptographicHash archiveHash(QCryptographicHash::Sha256);
    std::array<char, 64 * 1024> buffer{};
    qint64 offset = 0;
    bool copied = true;
    while (offset < sourceBefore.st_size) {
        const auto wanted = static_cast<size_t>(std::min<qint64>(
            buffer.size(),
            sourceBefore.st_size - offset
        ));
        const auto count = ::pread(
            packageFileDescriptor,
            buffer.data(),
            wanted,
            static_cast<off_t>(offset)
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0
            || !writeAll(spoolDescriptor.get(), buffer.data(), count)) {
            copied = false;
            break;
        }
        archiveHash.addData(QByteArrayView(buffer.data(), count));
        offset += count;
    }

    char trailing = 0;
    ssize_t trailingCount = -1;
    if (copied) {
        do {
            trailingCount = ::pread(
                packageFileDescriptor,
                &trailing,
                1,
                static_cast<off_t>(sourceBefore.st_size)
            );
        } while (trailingCount < 0 && errno == EINTR);
    }
    struct stat sourceAfter {};
    if (!copied || trailingCount != 0
        || ::fstat(packageFileDescriptor, &sourceAfter) != 0
        || !sourceMetadataUnchanged(sourceBefore, sourceAfter)
        || !fsyncRetry(spoolDescriptor.get())
        || !fsyncRetry(directoryDescriptor.get())) {
        cleanupFixedDirectoryAt(rootDescriptor, token);
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InvalidPackageDescriptor")),
                QStringLiteral(
                    "The package descriptor changed or could not be copied into private staging"
                )
            );
        return result;
    }

    Private::Session session{
        .owner = sender,
        .token = token,
        .archiveDigest = QString::fromLatin1(archiveHash.result().toHex()),
        .archiveSize = static_cast<quint64>(sourceBefore.st_size),
        .expiresAtMs = d_->now() + d_->timeToLiveMs,
        .directoryPath = directoryPath,
        .spoolPath = QDir(directoryPath).filePath(
            QString::fromLatin1(spoolFileName)
        ),
        .reportPath = QDir(directoryPath).filePath(
            QString::fromLatin1(reportFileName)
        ),
        .materializedPath = QDir(directoryPath).filePath(
            QString::fromLatin1(materializedFileName)
        ),
        .reportBytes = {},
        .report = std::nullopt,
        .errorName = {},
        .errorMessage = {},
        .spoolIdentity = {},
        .reportIdentity = {},
        .materializedIdentity = {},
    };
    const auto request = ComponentInspectorLaunchRequest{
        .token = session.token,
        .archiveDigest = session.archiveDigest,
        .spoolPath = session.spoolPath,
        .reportPath = session.reportPath,
        .materializedPath = session.materializedPath,
    };
    d_->sessions.insert(token, session);

    QPointer<ComponentInspectionSessions> guard(this);
    QString launchError;
    if (!d_->launcher->start(
            request,
            [guard, token](ComponentInspectorLaunchResult completion) {
                if (guard) {
                    guard->launcherFinished(token, std::move(completion));
                }
            },
            launchError
        )) {
        d_->sessions.remove(token);
        cleanupFixedDirectoryAt(rootDescriptor, token);
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                launchError.isEmpty()
                    ? QStringLiteral("Cannot launch the package inspector")
                    : launchError
            );
        return result;
    }

    result.success = true;
    result.token = token;
    result.archiveDigest = session.archiveDigest;
    result.archiveSize = session.archiveSize;
    result.expiresAtMs = session.expiresAtMs;
    return result;
}

void ComponentInspectionSessions::launcherFinished(
    const QString &token,
    ComponentInspectorLaunchResult result
)
{
    if (!d_->ownsSpool()) {
        return;
    }
    auto found = d_->sessions.find(token);
    if (found == d_->sessions.end()
        || found->state != ComponentInspectionState::Pending) {
        return;
    }
    if (found->expiresAtMs <= d_->now()) {
        expireNow();
        return;
    }

    auto fail = [this, &found](QString name, QString message) {
        QByteArray review;
        FileIdentity ignored;
        QString ignoredError;
        readPrivateFile(
            found->reportPath,
            0,
            maximumReportBytes,
            review,
            ignored,
            ignoredError
        );
        const auto errorName = name.isEmpty()
            ? managerError(QStringLiteral("InspectionUnavailable"))
            : std::move(name);
        const auto errorMessage = message.isEmpty()
            ? QStringLiteral("The package inspector failed")
            : std::move(message);
        const auto owner = found->owner;
        const auto failedToken = found->token;
        cleanupFixedDirectoryAt(
            d_->rootDescriptor.get(),
            found->token
        );
        d_->sessions.erase(found);
        emit inspectionFinished(
            owner,
            failedToken,
            review,
            {},
            {},
            errorName,
            errorMessage
        );
    };

    if (!result.success) {
        fail(std::move(result.errorName), std::move(result.errorMessage));
        return;
    }

    QString outputError;
    if (!d_->loadSuccessfulOutputs(*found, outputError)) {
        fail(
            managerError(QStringLiteral("InspectionUnavailable")),
            outputError
        );
        return;
    }
    found->state = ComponentInspectionState::Complete;
    const auto owner = found->owner;
    const auto report = found->reportBytes;
    const auto spoolPath = found->spoolPath;
    const auto materializedPath = found->materializedPath;
    emit inspectionFinished(
        owner,
        token,
        report,
        spoolPath,
        materializedPath,
        {},
        {}
    );
}

ComponentInspectionLookupResult ComponentInspectionSessions::lookup(
    const QString &sender,
    const QString &token
)
{
    ComponentInspectionLookupResult result;
    result.token = token;
    if (!d_->ownsSpool()) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                d_->unavailableReason.isEmpty()
                    ? QStringLiteral("Package inspection is unavailable")
                    : d_->unavailableReason
            );
        return result;
    }
    expireNow();

    auto found = d_->sessions.find(token);
    if (found == d_->sessions.end()) {
        const auto expired = d_->expired.constFind(token);
        const auto ownerMismatch = expired != d_->expired.cend()
            && expired->owner != sender;
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(ownerMismatch
                        ? QStringLiteral("InspectionOwnerMismatch")
                        : expired != d_->expired.cend()
                            ? QStringLiteral("InspectionExpired")
                            : QStringLiteral("UnknownInspection")),
                ownerMismatch
                    ? QStringLiteral("The inspection belongs to another caller")
                    : expired != d_->expired.cend()
                        ? QStringLiteral("The inspection has expired")
                        : QStringLiteral("The inspection token is unknown")
            );
        return result;
    }
    if (found->owner != sender) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionOwnerMismatch")),
                QStringLiteral("The inspection belongs to another caller")
            );
        return result;
    }

    bool publishFailure = false;
    QString failureOwner;
    QString failureName;
    QString failureMessage;
    if (found->state == ComponentInspectionState::Complete) {
        QString validationError;
        if (!d_->revalidateComplete(*found, validationError)) {
            failureOwner = found->owner;
            cleanupFixedDirectoryAt(
                d_->rootDescriptor.get(),
                found->token
            );
            found->state = ComponentInspectionState::Failed;
            found->directoryPath.clear();
            found->spoolPath.clear();
            found->reportPath.clear();
            found->materializedPath.clear();
            found->errorName = managerError(
                QStringLiteral("InspectionUnavailable")
            );
            found->errorMessage = validationError;
            failureName = found->errorName;
            failureMessage = found->errorMessage;
            publishFailure = true;
        }
    }

    result.success = true;
    result.state = found->state;
    result.archiveDigest = found->archiveDigest;
    result.archiveSize = found->archiveSize;
    result.expiresAtMs = found->expiresAtMs;
    result.reportBytes = found->reportBytes;
    result.errorName = found->errorName;
    result.errorMessage = found->errorMessage;
    if (found->state == ComponentInspectionState::Complete) {
        result.spoolPath = found->spoolPath;
        result.materializedPath = found->materializedPath;
    }
    if (publishFailure) {
        emit inspectionFinished(
            failureOwner,
            token,
            {},
            {},
            {},
            failureName,
            failureMessage
        );
    }
    return result;
}

ComponentInspectionTakeResult ComponentInspectionSessions::takeForInstall(
    const QString &sender,
    const QString &token,
    const QString &expectedArchiveDigest
)
{
    ComponentInspectionTakeResult result;
    if (!d_->ownsSpool()) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                d_->unavailableReason.isEmpty()
                    ? QStringLiteral("Package inspection is unavailable")
                    : d_->unavailableReason
            );
        return result;
    }
    expireNow();
    auto found = d_->sessions.find(token);
    if (found == d_->sessions.end()) {
        const auto expired = d_->expired.constFind(token);
        const auto ownerMismatch = expired != d_->expired.cend()
            && expired->owner != sender;
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(ownerMismatch
                        ? QStringLiteral("InspectionOwnerMismatch")
                        : expired != d_->expired.cend()
                            ? QStringLiteral("InspectionExpired")
                            : QStringLiteral("UnknownInspection")),
                ownerMismatch
                    ? QStringLiteral("The inspection belongs to another caller")
                    : expired != d_->expired.cend()
                        ? QStringLiteral("The inspection has expired")
                        : QStringLiteral("The inspection token is unknown")
            );
        return result;
    }
    if (found->owner != sender) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionOwnerMismatch")),
                QStringLiteral("The inspection belongs to another caller")
            );
        return result;
    }
    if (!digestPattern().match(expectedArchiveDigest).hasMatch()
        || expectedArchiveDigest != found->archiveDigest) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("ArchiveDigestMismatch")),
                QStringLiteral(
                    "The expected archive digest does not own this inspection"
                )
            );
        return result;
    }
    if (found->state != ComponentInspectionState::Complete
        || !found->report.has_value()) {
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                found->errorName.isEmpty()
                    ? managerError(QStringLiteral("InspectionUnavailable"))
                    : found->errorName,
                found->errorMessage.isEmpty()
                    ? QStringLiteral("The inspection has not completed successfully")
                    : found->errorMessage
            );
        return result;
    }

    QString validationError;
    if (!d_->revalidateComplete(*found, validationError)) {
        cleanupFixedDirectoryAt(
            d_->rootDescriptor.get(),
            found->token
        );
        d_->sessions.erase(found);
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                validationError
            );
        return result;
    }

    const auto artifactRootDescriptor = ::fcntl(
        d_->rootDescriptor.get(),
        F_DUPFD_CLOEXEC,
        0
    );
    const auto artifactLockDescriptor = ::fcntl(
        d_->lockDescriptor.get(),
        F_DUPFD_CLOEXEC,
        0
    );
    if (artifactRootDescriptor < 0 || artifactLockDescriptor < 0) {
        const auto leaseError = systemError(
            "Cannot retain the inspection spool lease"
        );
        if (artifactRootDescriptor >= 0) {
            ::close(artifactRootDescriptor);
        }
        if (artifactLockDescriptor >= 0) {
            ::close(artifactLockDescriptor);
        }
        static_cast<ComponentInspectionOperationResult &>(result) =
            operationError(
                managerError(QStringLiteral("InspectionUnavailable")),
                leaseError
            );
        return result;
    }

    ComponentInspectionArtifact artifact;
    artifact.token = found->token;
    artifact.archiveDigest = found->archiveDigest;
    artifact.archiveSize = found->archiveSize;
    artifact.reportBytes = std::move(found->reportBytes);
    artifact.report = std::move(*found->report);
    artifact.spoolPath = found->spoolPath;
    artifact.materializedPath = found->materializedPath;
    artifact.sessionDirectory_ = found->directoryPath;
    artifact.rootDescriptor_ = artifactRootDescriptor;
    artifact.lockDescriptor_ = artifactLockDescriptor;
    d_->sessions.erase(found);

    result.success = true;
    result.artifact.emplace(std::move(artifact));
    return result;
}

ComponentInspectionOperationResult ComponentInspectionSessions::cancel(
    const QString &sender,
    const QString &token
)
{
    if (!d_->ownsSpool()) {
        return operationError(
            managerError(QStringLiteral("InspectionUnavailable")),
            d_->unavailableReason.isEmpty()
                ? QStringLiteral("Package inspection is unavailable")
                : d_->unavailableReason
        );
    }
    expireNow();
    auto found = d_->sessions.find(token);
    if (found == d_->sessions.end()) {
        const auto expired = d_->expired.constFind(token);
        if (expired != d_->expired.cend()) {
            return operationError(
                managerError(expired->owner == sender
                        ? QStringLiteral("InspectionExpired")
                        : QStringLiteral("InspectionOwnerMismatch")),
                expired->owner == sender
                    ? QStringLiteral("The inspection has expired")
                    : QStringLiteral("The inspection belongs to another caller")
            );
        }
        return operationError(
            managerError(QStringLiteral("UnknownInspection")),
            QStringLiteral("The inspection token is unknown")
        );
    }
    if (found->owner != sender) {
        return operationError(
            managerError(QStringLiteral("InspectionOwnerMismatch")),
            QStringLiteral("The inspection belongs to another caller")
        );
    }

    d_->launcher->cancel(token);
    cleanupFixedDirectoryAt(d_->rootDescriptor.get(), found->token);
    d_->sessions.erase(found);
    return {
        .success = true,
        .errorName = {},
        .errorMessage = {},
    };
}

void ComponentInspectionSessions::cancelAllForSender(const QString &sender)
{
    if (!d_->ownsSpool()) {
        return;
    }
    const auto tokens = d_->sessions.keys();
    for (const auto &token : tokens) {
        const auto found = d_->sessions.find(token);
        if (found == d_->sessions.end() || found->owner != sender) {
            continue;
        }
        d_->launcher->cancel(token);
        cleanupFixedDirectoryAt(d_->rootDescriptor.get(), found->token);
        d_->sessions.erase(found);
    }
}

void ComponentInspectionSessions::expireNow()
{
    if (!d_->ownsSpool()) {
        return;
    }
    const auto currentTime = d_->now();
    d_->pruneExpired(currentTime);
    const auto tokens = d_->sessions.keys();
    for (const auto &token : tokens) {
        const auto found = d_->sessions.find(token);
        if (found == d_->sessions.end()
            || found->expiresAtMs > currentTime) {
            continue;
        }
        const auto owner = found->owner;
        const auto expiredToken = found->token;
        d_->launcher->cancel(token);
        d_->addExpired(*found, currentTime);
        cleanupFixedDirectoryAt(d_->rootDescriptor.get(), found->token);
        d_->sessions.erase(found);
        emit inspectionFinished(
            owner,
            expiredToken,
            {},
            {},
            {},
            managerError(QStringLiteral("InspectionExpired")),
            QStringLiteral("The package inspection expired before it was installed")
        );
    }
}

} // namespace HyprShelld
