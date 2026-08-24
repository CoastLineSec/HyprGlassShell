#include "activation_backend.h"

#include "hyprland/json_support.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>
#include <QScopeGuard>
#include <QStringDecoder>
#include <QUuid>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <climits>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr qsizetype maximumLockBytes = 4 * 1024;
constexpr qsizetype maximumProcBytes = 256 * 1024;
constexpr qsizetype maximumIpcReplyBytes = 512 * 1024;
constexpr qsizetype maximumEventBufferBytes = 256 * 1024;
constexpr qsizetype maximumEventFrameBytes = 64 * 1024;
constexpr int maximumJsonDepth = 16;
constexpr auto controlSocketName = ".socket.sock";
constexpr auto eventSocketName = ".socket2.sock";
constexpr auto lockFileName = "hyprland.lock";

class UniqueFd final {
public:
    UniqueFd() = default;
    explicit UniqueFd(const int descriptor) : descriptor_(descriptor) {}

    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;

    UniqueFd(UniqueFd &&other) noexcept
        : descriptor_(std::exchange(other.descriptor_, -1))
    {
    }

    UniqueFd &operator=(UniqueFd &&other) noexcept
    {
        if (this == &other) return *this;
        reset(std::exchange(other.descriptor_, -1));
        return *this;
    }

    ~UniqueFd() { reset(); }

    [[nodiscard]] int get() const { return descriptor_; }
    [[nodiscard]] bool valid() const { return descriptor_ >= 0; }

    void reset(const int descriptor = -1)
    {
        if (descriptor_ >= 0) {
            while (::close(descriptor_) != 0 && errno == EINTR) {}
        }
        descriptor_ = descriptor;
    }

private:
    int descriptor_ = -1;
};

struct PeerCredentials final {
    pid_t pid = -1;
    uid_t uid = std::numeric_limits<uid_t>::max();
    gid_t gid = std::numeric_limits<gid_t>::max();
};

struct ProcessStat final {
    QByteArray startTime;
    char state = '\0';
};

struct TrustedInstance final {
    UniqueFd runtimeDirectory;
    UniqueFd hyprDirectory;
    UniqueFd instanceDirectory;
    struct stat runtimeMetadata {};
    struct stat hyprMetadata {};
    struct stat instanceMetadata {};
};

struct SocketConnection final {
    UniqueFd descriptor;
    struct stat nodeMetadata {};
};

[[nodiscard]] QString systemError(const int error)
{
    return QString::fromLocal8Bit(std::strerror(error));
}

[[nodiscard]] bool cleanAbsolutePath(const QString &path)
{
    return !path.isEmpty()
        && !path.contains(QChar::Null)
        && QDir::isAbsolutePath(path)
        && QDir::cleanPath(path) == path;
}

[[nodiscard]] bool validInstanceSignature(const QStringView signature)
{
    if (signature.isEmpty() || signature.size() > 192
        || signature == QStringView(u".")
        || signature == QStringView(u"..")) {
        return false;
    }
    for (const auto character : signature) {
        if (!((character >= u'a' && character <= u'z')
              || (character >= u'A' && character <= u'Z')
              || (character >= u'0' && character <= u'9')
              || character == u'_' || character == u'-'
              || character == u'.')) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool validNonce(const QStringView nonce)
{
    if (nonce.isEmpty()) return true;
    if (nonce.size() < 32 || nonce.size() > 128) return false;
    for (const auto character : nonce) {
        if (!((character >= u'0' && character <= u'9')
              || (character >= u'a' && character <= u'f'))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool sameNode(
    const struct stat &left,
    const struct stat &right
)
{
    return left.st_dev == right.st_dev
        && left.st_ino == right.st_ino
        && left.st_mode == right.st_mode
        && left.st_uid == right.st_uid;
}

[[nodiscard]] bool sameFileSnapshot(
    const struct stat &left,
    const struct stat &right
)
{
    return sameNode(left, right)
        && left.st_nlink == right.st_nlink
        && left.st_size == right.st_size
        && left.st_mtim.tv_sec == right.st_mtim.tv_sec
        && left.st_mtim.tv_nsec == right.st_mtim.tv_nsec
        && left.st_ctim.tv_sec == right.st_ctim.tv_sec
        && left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

[[nodiscard]] bool trustedAncestorMetadata(
    const struct stat &metadata,
    const uid_t effectiveUser
)
{
    if (!S_ISDIR(metadata.st_mode)) return false;
    if (metadata.st_uid != 0 && metadata.st_uid != effectiveUser) return false;
    if ((metadata.st_mode & 0022) == 0) return true;
    return metadata.st_uid == 0 && (metadata.st_mode & S_ISVTX) != 0;
}

[[nodiscard]] bool trustedOwnedDirectoryMetadata(
    const struct stat &metadata,
    const uid_t effectiveUser
)
{
    return S_ISDIR(metadata.st_mode)
        && metadata.st_uid == effectiveUser
        && (metadata.st_mode & 0022) == 0;
}

[[nodiscard]] std::optional<UniqueFd> openTrustedAbsoluteDirectory(
    const QString &path,
    QString &error
)
{
    if (!cleanAbsolutePath(path)) {
        error = QStringLiteral("The runtime root is not a clean absolute path.");
        return std::nullopt;
    }

    UniqueFd current(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (!current.valid()) {
        error = QStringLiteral("The filesystem root could not be opened: %1")
                    .arg(systemError(errno));
        return std::nullopt;
    }

    const auto components = path.split(u'/', Qt::SkipEmptyParts);
    const auto effectiveUser = ::geteuid();
    for (qsizetype index = 0; index < components.size(); ++index) {
        const auto encoded = QFile::encodeName(components.at(index));
        UniqueFd next(::openat(
            current.get(),
            encoded.constData(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        ));
        if (!next.valid()) {
            error = QStringLiteral("A runtime-root directory could not be opened safely: %1")
                        .arg(systemError(errno));
            return std::nullopt;
        }
        struct stat metadata {};
        if (::fstat(next.get(), &metadata) != 0
            || !trustedAncestorMetadata(metadata, effectiveUser)) {
            error = QStringLiteral("The runtime-root directory chain is not trusted.");
            return std::nullopt;
        }
        current = std::move(next);
    }

    struct stat finalMetadata {};
    if (::fstat(current.get(), &finalMetadata) != 0
        || !trustedOwnedDirectoryMetadata(finalMetadata, effectiveUser)) {
        error = QStringLiteral("The runtime root is not a private directory owned by this user.");
        return std::nullopt;
    }
    return current;
}

[[nodiscard]] std::optional<TrustedInstance> openTrustedInstance(
    const QString &runtimeRoot,
    const QString &instanceSignature,
    QString &error
)
{
    auto runtime = openTrustedAbsoluteDirectory(runtimeRoot, error);
    if (!runtime) return std::nullopt;

    TrustedInstance result;
    result.runtimeDirectory = std::move(*runtime);
    if (::fstat(result.runtimeDirectory.get(), &result.runtimeMetadata) != 0) {
        error = QStringLiteral("The runtime root could not be inspected.");
        return std::nullopt;
    }

    result.hyprDirectory.reset(::openat(
        result.runtimeDirectory.get(),
        "hypr",
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!result.hyprDirectory.valid()
        || ::fstat(result.hyprDirectory.get(), &result.hyprMetadata) != 0
        || !trustedOwnedDirectoryMetadata(result.hyprMetadata, ::geteuid())) {
        error = QStringLiteral("The Hyprland runtime directory is not trusted.");
        return std::nullopt;
    }

    const auto encodedSignature = QFile::encodeName(instanceSignature);
    result.instanceDirectory.reset(::openat(
        result.hyprDirectory.get(),
        encodedSignature.constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    ));
    if (!result.instanceDirectory.valid()
        || ::fstat(result.instanceDirectory.get(), &result.instanceMetadata) != 0
        || !trustedOwnedDirectoryMetadata(result.instanceMetadata, ::geteuid())) {
        error = QStringLiteral("The Hyprland instance directory is not trusted.");
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] bool verifyTrustedInstance(
    const TrustedInstance &instance,
    const QString &signature,
    QString &error
)
{
    struct stat runtimeNow {};
    struct stat hyprNow {};
    struct stat hyprAtPath {};
    struct stat instanceNow {};
    struct stat instanceAtPath {};
    const auto encodedSignature = QFile::encodeName(signature);

    if (::fstat(instance.runtimeDirectory.get(), &runtimeNow) != 0
        || ::fstat(instance.hyprDirectory.get(), &hyprNow) != 0
        || ::fstat(instance.instanceDirectory.get(), &instanceNow) != 0
        || ::fstatat(instance.runtimeDirectory.get(), "hypr", &hyprAtPath,
                     AT_SYMLINK_NOFOLLOW) != 0
        || ::fstatat(instance.hyprDirectory.get(), encodedSignature.constData(),
                     &instanceAtPath, AT_SYMLINK_NOFOLLOW) != 0
        || !sameNode(runtimeNow, instance.runtimeMetadata)
        || !sameNode(hyprNow, instance.hyprMetadata)
        || !sameNode(hyprAtPath, instance.hyprMetadata)
        || !sameNode(instanceNow, instance.instanceMetadata)
        || !sameNode(instanceAtPath, instance.instanceMetadata)) {
        error = QStringLiteral("The pinned Hyprland runtime namespace changed.");
        return false;
    }
    return true;
}

[[nodiscard]] bool readBoundedDescriptor(
    const int descriptor,
    const qsizetype maximumBytes,
    QByteArray &bytes,
    QString &error
)
{
    bytes.clear();
    std::array<char, 8192> buffer {};
    while (true) {
        const auto count = ::read(descriptor, buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            error = QStringLiteral("A trusted file could not be read: %1")
                        .arg(systemError(errno));
            return false;
        }
        if (count == 0) return true;
        if (count > maximumBytes - bytes.size()) {
            error = QStringLiteral("A trusted file exceeded its size limit.");
            return false;
        }
        bytes.append(buffer.data(), count);
    }
}

[[nodiscard]] bool readStableRegularAt(
    const int directory,
    const char *name,
    const qsizetype maximumBytes,
    const bool requireOwned,
    QByteArray &bytes,
    QString &error
)
{
    struct stat before {};
    if (::fstatat(directory, name, &before, AT_SYMLINK_NOFOLLOW) != 0
        || !S_ISREG(before.st_mode) || before.st_nlink != 1
        || (requireOwned && before.st_uid != ::geteuid())
        || (requireOwned && (before.st_mode & 0022) != 0)
        || before.st_size < 0 || before.st_size > maximumBytes) {
        error = QStringLiteral("A required trusted file has unsafe metadata.");
        return false;
    }

    UniqueFd descriptor(::openat(
        directory,
        name,
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK
    ));
    struct stat opened {};
    if (!descriptor.valid() || ::fstat(descriptor.get(), &opened) != 0
        || !sameFileSnapshot(before, opened)) {
        error = QStringLiteral("A required trusted file changed while opening.");
        return false;
    }
    if (!readBoundedDescriptor(descriptor.get(), maximumBytes, bytes, error)) {
        return false;
    }

    struct stat afterOpen {};
    struct stat afterPath {};
    if (::fstat(descriptor.get(), &afterOpen) != 0
        || ::fstatat(directory, name, &afterPath, AT_SYMLINK_NOFOLLOW) != 0
        || !sameFileSnapshot(before, afterOpen)
        || !sameFileSnapshot(before, afterPath)) {
        error = QStringLiteral("A required trusted file changed while reading.");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<pid_t> parseLockFile(
    const QByteArrayView bytes,
    QString &error
)
{
    // Hyprland v0.56 writes exactly: PID, newline, Wayland display, newline.
    const auto firstNewline = bytes.indexOf('\n');
    if (firstNewline <= 0 || bytes.isEmpty() || bytes.back() != '\n'
        || bytes.size() < firstNewline + 3
        || bytes.sliced(firstNewline + 1, bytes.size() - firstNewline - 2)
               .contains('\n')) {
        error = QStringLiteral("hyprland.lock does not contain exactly two lines.");
        return std::nullopt;
    }

    const auto pidBytes = bytes.first(firstNewline);
    if (pidBytes.isEmpty() || pidBytes.front() == '0') {
        error = QStringLiteral("hyprland.lock contains an invalid process ID.");
        return std::nullopt;
    }
    quint64 parsedPid = 0;
    for (const auto character : pidBytes) {
        if (character < '0' || character > '9') {
            error = QStringLiteral("hyprland.lock contains an invalid process ID.");
            return std::nullopt;
        }
        const auto digit = static_cast<quint64>(character - '0');
        if (parsedPid > (static_cast<quint64>(std::numeric_limits<pid_t>::max())
                         - digit) / 10) {
            error = QStringLiteral("hyprland.lock contains an invalid process ID.");
            return std::nullopt;
        }
        parsedPid = parsedPid * 10 + digit;
    }
    if (parsedPid == 0) {
        error = QStringLiteral("hyprland.lock contains an invalid process ID.");
        return std::nullopt;
    }

    const auto display = bytes.sliced(
        firstNewline + 1,
        bytes.size() - firstNewline - 2
    );
    if (display.isEmpty() || display.size() > 255) {
        error = QStringLiteral("hyprland.lock contains an invalid Wayland display.");
        return std::nullopt;
    }
    for (const auto character : display) {
        if (!((character >= 'a' && character <= 'z')
              || (character >= 'A' && character <= 'Z')
              || (character >= '0' && character <= '9')
              || character == '_' || character == '-'
              || character == '.')) {
            error = QStringLiteral("hyprland.lock contains an invalid Wayland display.");
            return std::nullopt;
        }
    }
    return static_cast<pid_t>(parsedPid);
}

[[nodiscard]] bool peerCredentials(
    const int descriptor,
    PeerCredentials &credentials,
    QString &error
)
{
    struct {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    } native {};
    socklen_t size = sizeof(native);
    if (::getsockopt(
            descriptor, SOL_SOCKET, SO_PEERCRED, &native, &size
        ) != 0 || size != sizeof(native) || native.pid <= 0) {
        error = QStringLiteral("The Unix-socket peer identity is unavailable.");
        return false;
    }
    credentials = {
        .pid = native.pid,
        .uid = native.uid,
        .gid = native.gid,
    };
    return true;
}

[[nodiscard]] int remainingPollMilliseconds(const QDeadlineTimer &deadline)
{
    const auto remaining = deadline.remainingTime();
    if (remaining <= 0) return 0;
    return static_cast<int>(std::min<qint64>(remaining, INT_MAX));
}

enum class SignatureLookupState {
    Resolved,
    Unavailable,
    Invalid,
};

struct SignatureLookup final {
    SignatureLookupState state = SignatureLookupState::Unavailable;
    QString signature;
    QString error;
};

[[nodiscard]] SignatureLookup systemdInstanceSignature(
    const int timeoutMilliseconds
)
{
    SignatureLookup result;
    if (timeoutMilliseconds <= 0) {
        result.error = QStringLiteral(
            "The activation deadline expired before instance discovery."
        );
        return result;
    }
    const auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        result.error = QStringLiteral("The user D-Bus is unavailable.");
        return result;
    }
    const auto request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.systemd1.Manager"),
        QStringLiteral("GetEnvironment")
    );
    const auto reply = bus.call(
        request, QDBus::Block, timeoutMilliseconds
    );
    if (reply.type() == QDBusMessage::ErrorMessage) {
        result.error = QStringLiteral(
            "The systemd user manager environment is unavailable."
        );
        return result;
    }
    const QDBusReply<QStringList> environment(reply);
    if (!environment.isValid()) {
        result.state = SignatureLookupState::Invalid;
        result.error = QStringLiteral(
            "The systemd user manager returned a malformed environment."
        );
        return result;
    }
    constexpr auto prefix = "HYPRLAND_INSTANCE_SIGNATURE=";
    for (const auto &entry : environment.value()) {
        if (!entry.startsWith(QLatin1StringView(prefix))) continue;
        if (result.state == SignatureLookupState::Resolved) {
            result.state = SignatureLookupState::Invalid;
            result.signature.clear();
            result.error = QStringLiteral(
                "The systemd user manager returned multiple Hyprland signatures."
            );
            return result;
        }
        result.state = SignatureLookupState::Resolved;
        result.signature = entry.sliced(
            static_cast<qsizetype>(std::strlen(prefix))
        );
    }
    if (result.state != SignatureLookupState::Resolved) {
        result.error = QStringLiteral(
            "The systemd user manager has no Hyprland signature."
        );
    }
    return result;
}

[[nodiscard]] bool pollUntil(
    const int descriptor,
    const short events,
    const QDeadlineTimer &deadline,
    short &returnedEvents,
    QString &error
)
{
    while (!deadline.hasExpired()) {
        struct pollfd item {
            .fd = descriptor,
            .events = events,
            .revents = 0,
        };
        const auto result = ::poll(
            &item,
            1,
            remainingPollMilliseconds(deadline)
        );
        if (result > 0) {
            returnedEvents = item.revents;
            return true;
        }
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) {
            error = QStringLiteral("Unix-socket polling failed: %1")
                        .arg(systemError(errno));
            return false;
        }
    }
    error = QStringLiteral("The Hyprland activation deadline expired.");
    return false;
}

[[nodiscard]] bool validateSocketNode(
    const int instanceDirectory,
    const char *name,
    struct stat &metadata,
    QString &error
)
{
    if (::fstatat(instanceDirectory, name, &metadata,
                  AT_SYMLINK_NOFOLLOW) != 0
        || !S_ISSOCK(metadata.st_mode)
        || metadata.st_uid != ::geteuid()) {
        error = QStringLiteral("A Hyprland IPC socket has unsafe metadata.");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<SocketConnection> connectSocket(
    const int instanceDirectory,
    const QString &absolutePath,
    const char *name,
    const pid_t expectedPid,
    const QDeadlineTimer &deadline,
    QString &error
)
{
    struct stat before {};
    if (!validateSocketNode(instanceDirectory, name, before, error)) {
        return std::nullopt;
    }

    const auto encodedPath = QFile::encodeName(absolutePath);
    struct sockaddr_un address {};
    if (encodedPath.isEmpty()
        || encodedPath.size() >= static_cast<qsizetype>(sizeof(address.sun_path))) {
        error = QStringLiteral("The Hyprland IPC socket path is invalid or too long.");
        return std::nullopt;
    }
    address.sun_family = AF_UNIX;
    std::memcpy(
        address.sun_path,
        encodedPath.constData(),
        static_cast<size_t>(encodedPath.size())
    );
    address.sun_path[encodedPath.size()] = '\0';

    SocketConnection result;
    result.descriptor.reset(::socket(
        AF_UNIX,
        SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
        0
    ));
    if (!result.descriptor.valid()) {
        error = QStringLiteral("A Hyprland IPC socket could not be created: %1")
                    .arg(systemError(errno));
        return std::nullopt;
    }

    const auto addressSize = static_cast<socklen_t>(
        offsetof(struct sockaddr_un, sun_path) + encodedPath.size() + 1
    );
    if (::connect(
            result.descriptor.get(),
            reinterpret_cast<const struct sockaddr *>(&address),
            addressSize
        ) != 0) {
        if (errno != EINPROGRESS && errno != EAGAIN) {
            error = QStringLiteral("The Hyprland IPC socket could not be connected: %1")
                        .arg(systemError(errno));
            return std::nullopt;
        }
        short returnedEvents = 0;
        if (!pollUntil(
                result.descriptor.get(), POLLOUT, deadline,
                returnedEvents, error
            )) {
            return std::nullopt;
        }
        int socketError = 0;
        socklen_t socketErrorSize = sizeof(socketError);
        if ((returnedEvents & (POLLERR | POLLHUP | POLLNVAL)) != 0
            || ::getsockopt(
                   result.descriptor.get(), SOL_SOCKET, SO_ERROR,
                   &socketError, &socketErrorSize
               ) != 0
            || socketError != 0) {
            error = QStringLiteral("The Hyprland IPC connection failed.");
            return std::nullopt;
        }
    }

    PeerCredentials peer;
    if (!peerCredentials(result.descriptor.get(), peer, error)
        || peer.uid != ::geteuid() || peer.pid != expectedPid) {
        error = QStringLiteral("The Hyprland IPC socket belongs to a different process.");
        return std::nullopt;
    }

    struct stat afterAt {};
    struct stat afterPath {};
    const auto encodedAbsolute = QFile::encodeName(absolutePath);
    if (!validateSocketNode(instanceDirectory, name, afterAt, error)
        || ::lstat(encodedAbsolute.constData(), &afterPath) != 0
        || !sameNode(before, afterAt) || !sameNode(before, afterPath)) {
        error = QStringLiteral("The Hyprland IPC socket changed during connection.");
        return std::nullopt;
    }
    result.nodeMetadata = before;
    return result;
}

[[nodiscard]] bool parseNulList(
    const QByteArrayView bytes,
    QList<QByteArray> &items,
    QString &error
)
{
    items.clear();
    if (bytes.isEmpty() || bytes.back() != '\0') {
        error = QStringLiteral("A process metadata vector is not NUL terminated.");
        return false;
    }
    qsizetype begin = 0;
    while (begin < bytes.size()) {
        const auto end = bytes.indexOf('\0', begin);
        if (end < begin) {
            error = QStringLiteral("A process metadata vector is malformed.");
            return false;
        }
        if (end > begin) {
            items.append(QByteArray(bytes.sliced(begin, end - begin)));
        }
        begin = end + 1;
    }
    if (items.isEmpty()) {
        error = QStringLiteral("A process metadata vector is empty.");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<ProcessStat> parseProcessStat(
    const QByteArrayView bytes,
    QString &error
)
{
    const auto closingParenthesis = bytes.lastIndexOf(')');
    if (closingParenthesis < 2 || closingParenthesis + 2 >= bytes.size()
        || bytes.at(closingParenthesis + 1) != ' ') {
        error = QStringLiteral("The compositor process stat record is malformed.");
        return std::nullopt;
    }

    auto fields = QByteArray(bytes.sliced(closingParenthesis + 2)).split(' ');
    fields.erase(
        std::remove_if(fields.begin(), fields.end(),
                       [](const QByteArray &value) { return value.isEmpty(); }),
        fields.end()
    );
    // fields[0] is field 3 (state); field 22 (starttime) is fields[19].
    if (fields.size() <= 19 || fields.at(0).size() != 1) {
        error = QStringLiteral("The compositor process stat record is incomplete.");
        return std::nullopt;
    }
    const auto state = fields.at(0).front();
    if (state == 'Z' || state == 'X' || state == 'x') {
        error = QStringLiteral("The compositor process is no longer running.");
        return std::nullopt;
    }
    const auto startTime = fields.at(19);
    if (startTime.isEmpty()) {
        error = QStringLiteral("The compositor process identity is unavailable.");
        return std::nullopt;
    }
    for (const auto character : startTime) {
        if (character < '0' || character > '9') {
            error = QStringLiteral("The compositor process identity is malformed.");
            return std::nullopt;
        }
    }
    return ProcessStat{.startTime = startTime, .state = state};
}

[[nodiscard]] bool hasExplicitConfigArgument(const QList<QByteArray> &arguments)
{
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const auto &argument = arguments.at(index);
        if (argument == "-c" || argument.startsWith("-c")
            || argument == "--config" || argument.startsWith("--config")) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hasSafeModeArgument(const QList<QByteArray> &arguments)
{
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        if (arguments.at(index) == "--safe-mode"
            || arguments.at(index).startsWith("--safe-mode=")) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool parseEnvironment(
    const QList<QByteArray> &entries,
    std::map<QByteArray, QByteArray> &environment,
    QString &error
)
{
    environment.clear();
    for (const auto &entry : entries) {
        const auto separator = entry.indexOf('=');
        if (separator <= 0) {
            error = QStringLiteral("The compositor environment is malformed.");
            return false;
        }
        const auto name = entry.first(separator);
        if (environment.contains(name)) {
            error = QStringLiteral("The compositor environment contains duplicate variables.");
            return false;
        }
        environment.emplace(name, entry.sliced(separator + 1));
    }
    return true;
}

[[nodiscard]] bool resolveDefaultEntrypoint(
    const std::map<QByteArray, QByteArray> &environment,
    QString &resolved,
    QString &error
)
{
    if (environment.contains("HYPRLAND_CONFIG")) {
        error = QStringLiteral("Hyprland was started with HYPRLAND_CONFIG.");
        return false;
    }

    QString configHome;
    const auto xdg = environment.find("XDG_CONFIG_HOME");
    if (xdg != environment.end() && !xdg->second.isEmpty()) {
        configHome = QFile::decodeName(xdg->second);
        if (!cleanAbsolutePath(configHome)) {
            error = QStringLiteral("The compositor has an invalid XDG_CONFIG_HOME.");
            return false;
        }
    } else {
        const auto home = environment.find("HOME");
        if (home == environment.end() || home->second.isEmpty()) {
            error = QStringLiteral("The compositor has no provable default config home.");
            return false;
        }
        const auto homePath = QFile::decodeName(home->second);
        if (!cleanAbsolutePath(homePath)) {
            error = QStringLiteral("The compositor has an invalid HOME.");
            return false;
        }
        configHome = homePath + QStringLiteral("/.config");
    }

    resolved = QDir::cleanPath(
        configHome + QStringLiteral("/hypr/hyprland.lua")
    );
    if (!cleanAbsolutePath(resolved)) {
        error = QStringLiteral("The compositor default config path is not safe.");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<quint32> parseVersionPart(
    const QStringView part
)
{
    if (part.isEmpty() || (part.size() > 1 && part.front() == u'0')) {
        return std::nullopt;
    }
    quint64 value = 0;
    for (const auto character : part) {
        if (character < u'0' || character > u'9') return std::nullopt;
        const auto digit = static_cast<quint64>(character.unicode() - u'0');
        constexpr auto maximum = static_cast<quint64>(
            std::numeric_limits<quint32>::max()
        );
        if (value > (maximum - digit) / 10) return std::nullopt;
        value = value * 10 + digit;
    }
    return static_cast<quint32>(value);
}

[[nodiscard]] bool parseAndCheckVersion(
    const QByteArrayView reply,
    const HyprlandVersionPolicy &policy,
    QString &error
)
{
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        reply, maximumIpcReplyBytes, maximumJsonDepth
    );
    if (!parsed.value) {
        error = QStringLiteral("Hyprland returned malformed version JSON.");
        return false;
    }
    const auto versionValue = parsed.value->value(QStringLiteral("version"));
    if (!versionValue.isString()) {
        error = QStringLiteral("Hyprland returned no strict semantic version.");
        return false;
    }
    const auto versionString = versionValue.toString();
    const auto parts = QStringView(versionString).split(u'.');
    if (parts.size() != 3) {
        error = QStringLiteral("Hyprland returned an unsupported version format.");
        return false;
    }
    const auto major = parseVersionPart(parts.at(0));
    const auto minor = parseVersionPart(parts.at(1));
    const auto patch = parseVersionPart(parts.at(2));
    if (!major || !minor || !patch
        || *major != policy.major || *minor != policy.minor
        || *patch < policy.minimumPatch
        || (policy.maximumPatch && *patch > *policy.maximumPatch)) {
        error = QStringLiteral("The running Hyprland version is outside the reviewed range.");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<QString> parseProvider(
    const QByteArrayView reply,
    QString &error
)
{
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        reply, maximumIpcReplyBytes, maximumJsonDepth
    );
    if (!parsed.value) {
        error = QStringLiteral("Hyprland returned malformed status JSON.");
        return std::nullopt;
    }
    const auto providerValue = parsed.value->value(
        QStringLiteral("configProvider")
    );
    if (!providerValue.isString()) {
        error = QStringLiteral("Hyprland status has no configProvider string.");
        return std::nullopt;
    }
    const auto provider = providerValue.toString();
    if (provider != QStringLiteral("lua")
        && provider != QStringLiteral("hyprlang")) {
        error = QStringLiteral("Hyprland reported an unknown config provider.");
        return std::nullopt;
    }
    return provider;
}

[[nodiscard]] std::optional<QByteArray> parseConfigErrors(
    const QByteArrayView reply,
    QString &error
)
{
    if (reply.size() > maximumIpcReplyBytes
        || (reply.size() >= 3 && reply.at(0) == static_cast<char>(0xef)
            && reply.at(1) == static_cast<char>(0xbb)
            && reply.at(2) == static_cast<char>(0xbf))) {
        error = QStringLiteral("Hyprland returned invalid configerrors JSON.");
        return std::nullopt;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    static_cast<void>(decoder.decode(reply));
    if (decoder.hasError()) {
        error = QStringLiteral("Hyprland returned non-UTF-8 configerrors JSON.");
        return std::nullopt;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(
        QByteArray(reply), &parseError
    );
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        error = QStringLiteral("Hyprland returned malformed configerrors JSON.");
        return std::nullopt;
    }
    const auto array = document.array();
    for (const auto &item : array) {
        if (!item.isString()) {
            error = QStringLiteral("Hyprland configerrors is not a string array.");
            return std::nullopt;
        }
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

[[nodiscard]] bool validProvider(const QStringView provider)
{
    return provider == QStringView(u"lua")
        || provider == QStringView(u"hyprlang");
}

} // namespace

struct HyprlandIpcRuntime::Impl final {
    struct Session final {
        RuntimeSession publicSession;
        RuntimeActivationMode mode = RuntimeActivationMode::ManagedReload;
        QDeadlineTimer deadline;
        TrustedInstance trustedInstance;
        UniqueFd eventSocket;
        struct stat eventSocketNode {};
        std::optional<struct stat> controlSocketNode;
        PeerCredentials peer;
        UniqueFd procRoot;
        UniqueFd procDirectory;
        struct stat procDirectoryMetadata {};
        UniqueFd pidfd;
        QByteArray processStartTime;
        QByteArray processCommandLine;
        QByteArray processEnvironment;
        QByteArray lockContents;
        QByteArray eventBuffer;
        QString instanceSignature;
        std::atomic_bool cancelled = false;
        QMutex operationMutex;
        QMutex cancellationMutex;
        int activeControlDescriptor = -1;
    };

    struct AuthenticatedQueryResult final {
        bool success = false;
        QString errorCode;
        QString errorMessage;
        QString runtimeIdentity;
        QByteArray reply;
    };

    QString runtimeRoot;
    QString startupInstanceSignature;
    QString stableEntrypoint;
    int timeoutMilliseconds = 12000;
    HyprlandIpcRuntime::InstanceSignatureProvider instanceSignatureProvider;
    HyprlandVersionPolicy versionPolicy;
    bool versionPolicyConfigured = false;
    mutable QMutex mutex;
    QMutex prepareMutex;
    std::map<QByteArray, std::shared_ptr<Session>> sessions;

    [[nodiscard]] QString instancePath(const QStringView signature) const
    {
        return runtimeRoot + QStringLiteral("/hypr/") + signature;
    }

    [[nodiscard]] QString socketPath(
        const QStringView signature,
        const char *name
    ) const
    {
        return instancePath(signature) + QLatin1Char('/')
            + QString::fromLatin1(name);
    }

    [[nodiscard]] QString runtimeIdentity(const Session &session) const
    {
        if (!session.controlSocketNode) return {};
        const auto node = [](const auto value) {
            return QString::number(static_cast<qulonglong>(value));
        };
        const auto bytes = Hyprland::JsonSupport::canonicalJson(QJsonObject{
            {QStringLiteral("instanceSignature"), session.instanceSignature},
            {QStringLiteral("peerPid"),
             QString::number(static_cast<qlonglong>(session.peer.pid))},
            {QStringLiteral("processStartTime"),
             QString::fromLatin1(session.processStartTime)},
            {QStringLiteral("instanceDevice"),
             node(session.trustedInstance.instanceMetadata.st_dev)},
            {QStringLiteral("instanceInode"),
             node(session.trustedInstance.instanceMetadata.st_ino)},
            {QStringLiteral("eventDevice"),
             node(session.eventSocketNode.st_dev)},
            {QStringLiteral("eventInode"),
             node(session.eventSocketNode.st_ino)},
            {QStringLiteral("controlDevice"),
             node(session.controlSocketNode->st_dev)},
            {QStringLiteral("controlInode"),
             node(session.controlSocketNode->st_ino)},
        });
        return QString::fromLatin1(
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
        );
    }

    [[nodiscard]] bool openProcess(Session &session, QString &error) const
    {
        // SO_PEERCRED's effective UID is the local trust boundary. A
        // /proc/<pid>/exe basename check would not strengthen it: a same-UID
        // process can execute a renamed copy, while requiring a root-owned
        // inode would reject legitimate user and development installations.
        // A stronger executable gate needs an injected approved inode/hash
        // policy; no such mutable global policy is guessed here.
        session.procRoot.reset(::open(
            "/proc", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        ));
        if (!session.procRoot.valid()) {
            error = QStringLiteral("The proc filesystem is unavailable.");
            return false;
        }
        const auto pidName = QByteArray::number(session.peer.pid);
        session.procDirectory.reset(::openat(
            session.procRoot.get(), pidName.constData(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        ));
        if (!session.procDirectory.valid()
            || ::fstat(session.procDirectory.get(),
                       &session.procDirectoryMetadata) != 0
            || !S_ISDIR(session.procDirectoryMetadata.st_mode)
            || session.procDirectoryMetadata.st_uid != session.peer.uid) {
            error = QStringLiteral("The compositor proc directory is not trusted.");
            return false;
        }

#ifdef SYS_pidfd_open
        const auto descriptor = static_cast<int>(::syscall(
            SYS_pidfd_open, session.peer.pid, 0
        ));
        if (descriptor >= 0) {
            const auto flags = ::fcntl(descriptor, F_GETFD);
            if (flags >= 0) {
                static_cast<void>(::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC));
            }
            session.pidfd.reset(descriptor);
        }
#endif

        QByteArray statBytes;
        if (!readStableRegularAt(
                session.procDirectory.get(), "stat", maximumProcBytes,
                false, statBytes, error
            )) {
            return false;
        }
        const auto parsedStat = parseProcessStat(statBytes, error);
        if (!parsedStat) return false;
        session.processStartTime = parsedStat->startTime;
        return true;
    }

    [[nodiscard]] bool readAndValidateProcessContext(
        Session &session,
        const bool remember,
        QString &error
    ) const
    {
        QByteArray commandLine;
        QByteArray environmentBytes;
        if (!readStableRegularAt(
                session.procDirectory.get(), "cmdline", maximumProcBytes,
                false, commandLine, error
            )
            || !readStableRegularAt(
                session.procDirectory.get(), "environ", maximumProcBytes,
                false, environmentBytes, error
            )) {
            return false;
        }

        QList<QByteArray> arguments;
        QList<QByteArray> environmentEntries;
        if (!parseNulList(commandLine, arguments, error)
            || !parseNulList(environmentBytes, environmentEntries, error)) {
            return false;
        }
        if (hasExplicitConfigArgument(arguments)) {
            error = QStringLiteral("Hyprland was started with an explicit config argument.");
            return false;
        }
        if (hasSafeModeArgument(arguments)) {
            error = QStringLiteral("Hyprland safe mode cannot activate managed Lua.");
            return false;
        }

        std::map<QByteArray, QByteArray> environment;
        if (!parseEnvironment(environmentEntries, environment, error)) {
            return false;
        }
        // HYPRLAND_INSTANCE_SIGNATURE is generated by Hyprland after exec.
        // /proc/<pid>/environ exposes the original exec environment, so it is
        // not a reliable source for that post-start value. The retained
        // instance directory, exact lock PID, SO_PEERCRED peer, and process
        // start identity bind the selected signature instead.
        const auto runtime = environment.find("XDG_RUNTIME_DIR");
        const auto processRuntime = runtime == environment.end()
            ? QString() : QFile::decodeName(runtime->second);
        if (runtime == environment.end()
            || !cleanAbsolutePath(processRuntime)
            || processRuntime != runtimeRoot) {
            error = QStringLiteral("The compositor process has a different runtime root.");
            return false;
        }

        QString resolvedEntrypoint;
        if (!resolveDefaultEntrypoint(environment, resolvedEntrypoint, error)
            || resolvedEntrypoint != stableEntrypoint) {
            error = QStringLiteral("The compositor default entrypoint does not match HyprShelld's managed entrypoint.");
            return false;
        }

        if (remember) {
            session.processCommandLine = commandLine;
            session.processEnvironment = environmentBytes;
        } else if (commandLine != session.processCommandLine
                   || environmentBytes != session.processEnvironment) {
            error = QStringLiteral("The compositor process context changed during activation.");
            return false;
        }
        return true;
    }

    [[nodiscard]] bool verifyLock(
        Session &session,
        const bool remember,
        QString &error
    ) const
    {
        QByteArray lockBytes;
        if (!readStableRegularAt(
                session.trustedInstance.instanceDirectory.get(),
                lockFileName, maximumLockBytes, true, lockBytes, error
            )) {
            return false;
        }
        const auto pid = parseLockFile(lockBytes, error);
        if (!pid || *pid != session.peer.pid) {
            error = QStringLiteral("hyprland.lock does not identify the pinned compositor.");
            return false;
        }
        if (remember) {
            session.lockContents = lockBytes;
        } else if (lockBytes != session.lockContents) {
            error = QStringLiteral("hyprland.lock changed during activation.");
            return false;
        }
        return true;
    }

    [[nodiscard]] bool verifyIdentity(Session &session, QString &error) const
    {
        if (session.cancelled.load(std::memory_order_acquire)) {
            error = QStringLiteral("The Hyprland activation session was cancelled.");
            return false;
        }
        if (session.deadline.hasExpired()) {
            error = QStringLiteral("The Hyprland activation deadline expired.");
            return false;
        }
        if (!verifyTrustedInstance(
                session.trustedInstance, session.instanceSignature, error
            )) {
            return false;
        }

        struct pollfd eventPoll {
            .fd = session.eventSocket.get(),
            .events = POLLIN,
            .revents = 0,
        };
        if (::poll(&eventPoll, 1, 0) < 0
            || (eventPoll.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            error = QStringLiteral("The pinned Hyprland event socket closed.");
            return false;
        }
        PeerCredentials peer;
        if (!peerCredentials(session.eventSocket.get(), peer, error)
            || peer.pid != session.peer.pid || peer.uid != session.peer.uid
            || peer.uid != ::geteuid()) {
            error = QStringLiteral("The pinned Hyprland event peer changed.");
            return false;
        }

        struct stat eventNow {};
        if (!validateSocketNode(
                session.trustedInstance.instanceDirectory.get(),
                eventSocketName, eventNow, error
            ) || !sameNode(eventNow, session.eventSocketNode)) {
            error = QStringLiteral("The pinned Hyprland event endpoint changed.");
            return false;
        }

        if (session.pidfd.valid()) {
            struct pollfd pidPoll {
                .fd = session.pidfd.get(),
                .events = POLLIN,
                .revents = 0,
            };
            if (::poll(&pidPoll, 1, 0) < 0 || pidPoll.revents != 0) {
                error = QStringLiteral("The pinned compositor process exited.");
                return false;
            }
        }

        const auto pidName = QByteArray::number(session.peer.pid);
        struct stat procAtPath {};
        struct stat procNow {};
        if (::fstat(session.procDirectory.get(), &procNow) != 0
            || ::fstatat(session.procRoot.get(), pidName.constData(),
                         &procAtPath, AT_SYMLINK_NOFOLLOW) != 0
            || !sameNode(procNow, session.procDirectoryMetadata)
            || !sameNode(procAtPath, session.procDirectoryMetadata)) {
            error = QStringLiteral("The compositor process identity changed.");
            return false;
        }

        QByteArray statBytes;
        if (!readStableRegularAt(
                session.procDirectory.get(), "stat", maximumProcBytes,
                false, statBytes, error
            )) {
            return false;
        }
        const auto parsedStat = parseProcessStat(statBytes, error);
        if (!parsedStat || parsedStat->startTime != session.processStartTime) {
            error = QStringLiteral("The compositor process start identity changed.");
            return false;
        }
        return verifyLock(session, false, error)
            && readAndValidateProcessContext(session, false, error);
    }

    [[nodiscard]] std::optional<SocketConnection> openControl(
        Session &session,
        QString &error
    ) const
    {
        if (!verifyIdentity(session, error)) return std::nullopt;
        auto connection = connectSocket(
            session.trustedInstance.instanceDirectory.get(),
            socketPath(session.instanceSignature, controlSocketName),
            controlSocketName,
            session.peer.pid, session.deadline, error
        );
        if (!connection) return std::nullopt;
        if (session.controlSocketNode
            && !sameNode(*session.controlSocketNode,
                         connection->nodeMetadata)) {
            error = QStringLiteral("The Hyprland control endpoint changed.");
            return std::nullopt;
        }
        if (!session.controlSocketNode) {
            session.controlSocketNode = connection->nodeMetadata;
        }
        return connection;
    }

    [[nodiscard]] bool sendRequest(
        Session &session,
        SocketConnection connection,
        const QByteArrayView request,
        QByteArray &reply,
        QString &error
    ) const
    {
        if (request.isEmpty() || request.size() > 256
            || request.contains('\0')) {
            error = QStringLiteral("An internal Hyprland IPC request is invalid.");
            return false;
        }

        {
            const QMutexLocker cancellationLocker(
                &session.cancellationMutex
            );
            if (session.cancelled.load(std::memory_order_acquire)) {
                error = QStringLiteral("The Hyprland activation session was cancelled.");
                return false;
            }
            session.activeControlDescriptor = connection.descriptor.get();
        }
        const auto clearActiveControl = qScopeGuard([&session, descriptor =
                                                        connection.descriptor.get()] {
            const QMutexLocker cancellationLocker(
                &session.cancellationMutex
            );
            if (session.activeControlDescriptor == descriptor) {
                session.activeControlDescriptor = -1;
            }
        });

        qsizetype written = 0;
        while (written < request.size()) {
            const auto count = ::send(
                connection.descriptor.get(), request.data() + written,
                static_cast<size_t>(request.size() - written), MSG_NOSIGNAL
            );
            if (count > 0) {
                written += count;
                continue;
            }
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                short events = 0;
                if (!pollUntil(connection.descriptor.get(), POLLOUT,
                               session.deadline, events, error)
                    || (events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                    if (error.isEmpty()) {
                        error = QStringLiteral("The Hyprland request connection closed.");
                    }
                    return false;
                }
                continue;
            }
            error = QStringLiteral("The Hyprland IPC request could not be written.");
            return false;
        }
        if (::shutdown(connection.descriptor.get(), SHUT_WR) != 0) {
            error = QStringLiteral("The Hyprland IPC request could not be completed.");
            return false;
        }

        reply.clear();
        std::array<char, 8192> buffer {};
        while (true) {
            const auto count = ::recv(
                connection.descriptor.get(), buffer.data(), buffer.size(), 0
            );
            if (count > 0) {
                if (count > maximumIpcReplyBytes - reply.size()) {
                    error = QStringLiteral("The Hyprland IPC reply exceeded its size limit.");
                    return false;
                }
                reply.append(buffer.data(), count);
                continue;
            }
            if (count == 0) break;
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                short events = 0;
                if (!pollUntil(connection.descriptor.get(), POLLIN,
                               session.deadline, events, error)) {
                    return false;
                }
                if ((events & POLLNVAL) != 0 || (events & POLLERR) != 0) {
                    error = QStringLiteral("The Hyprland IPC reply connection failed.");
                    return false;
                }
                // POLLHUP may accompany the final readable bytes. Retry recv.
                continue;
            }
            error = QStringLiteral("The Hyprland IPC reply could not be read.");
            return false;
        }
        if (reply.isEmpty()) {
            error = QStringLiteral("Hyprland returned an empty IPC reply.");
            return false;
        }
        return true;
    }

    [[nodiscard]] bool request(
        Session &session,
        const QByteArrayView command,
        QByteArray &reply,
        QString &error
    ) const
    {
        auto connection = openControl(session, error);
        if (!connection) return false;
        return sendRequest(
            session, std::move(*connection), command, reply, error
        );
    }

    [[nodiscard]] AuthenticatedQueryResult authenticatedQuery(
        const QByteArrayView command,
        const int maximumWaitMilliseconds,
        const QStringView deadlineSubject
    )
    {
        AuthenticatedQueryResult result;
        const auto operationTimeout = std::min(
            std::max(timeoutMilliseconds, 0),
            std::max(maximumWaitMilliseconds, 0)
        );
        // The single operation deadline intentionally starts before waiting
        // for the mutex shared with activation preparation.
        const QDeadlineTimer operationDeadline(
            operationTimeout, Qt::PreciseTimer
        );
        const QMutexLocker prepareLocker(&prepareMutex);

        HyprlandVersionPolicy configuredVersionPolicy;
        HyprlandIpcRuntime::InstanceSignatureProvider signatureProvider;
        QString startupSignature;
        {
            const QMutexLocker stateLocker(&mutex);
            if (!cleanAbsolutePath(runtimeRoot)
                || !cleanAbsolutePath(stableEntrypoint)
                || timeoutMilliseconds <= 0
                || !versionPolicyConfigured
                || (versionPolicy.maximumPatch
                    && *versionPolicy.maximumPatch
                           < versionPolicy.minimumPatch)) {
                result.errorCode = QStringLiteral("RuntimeUnavailable");
                result.errorMessage = QStringLiteral(
                    "The Hyprland runtime configuration is invalid."
                );
                return result;
            }
            configuredVersionPolicy = versionPolicy;
            signatureProvider = instanceSignatureProvider;
            startupSignature = startupInstanceSignature;
        }

        auto session = std::make_shared<Session>();
        session->deadline = operationDeadline;
        if (session->deadline.hasExpired()) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = QStringLiteral(
                "The %1 deadline is invalid or expired."
            ).arg(deadlineSubject);
            return result;
        }

        QString error;
        QString resolvedSignature;
        if (signatureProvider) {
            HyprlandIpcRuntime::InstanceSignatureResult provided;
            try {
                provided = signatureProvider(
                    remainingPollMilliseconds(session->deadline)
                );
            } catch (...) {
                provided.errorMessage = QStringLiteral(
                    "The injected Hyprland instance provider failed."
                );
            }
            if (!provided.success
                || !validInstanceSignature(provided.signature)) {
                result.errorCode = QStringLiteral("RuntimeUnavailable");
                result.errorMessage = provided.errorMessage.isEmpty()
                    ? QStringLiteral(
                        "The Hyprland instance provider returned an invalid signature."
                    ) : provided.errorMessage;
                return result;
            }
            resolvedSignature = std::move(provided.signature);
        } else {
            const auto discovered = systemdInstanceSignature(
                remainingPollMilliseconds(session->deadline)
            );
            if (discovered.state == SignatureLookupState::Invalid) {
                result.errorCode = QStringLiteral("RuntimeUnavailable");
                result.errorMessage = discovered.error;
                return result;
            }
            resolvedSignature =
                discovered.state == SignatureLookupState::Resolved
                ? discovered.signature : startupSignature;
            if (!validInstanceSignature(resolvedSignature)) {
                result.errorCode = QStringLiteral("RuntimeUnavailable");
                result.errorMessage = discovered.error.isEmpty()
                    ? QStringLiteral(
                        "No valid Hyprland instance signature is available."
                    ) : discovered.error;
                return result;
            }
        }

        session->instanceSignature = resolvedSignature;
        auto trusted = openTrustedInstance(
            runtimeRoot, session->instanceSignature, error
        );
        if (!trusted) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = std::move(error);
            return result;
        }
        session->trustedInstance = std::move(*trusted);

        QByteArray initialLock;
        if (!readStableRegularAt(
                session->trustedInstance.instanceDirectory.get(), lockFileName,
                maximumLockBytes, true, initialLock, error
            )) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = std::move(error);
            return result;
        }
        const auto lockPid = parseLockFile(initialLock, error);
        if (!lockPid) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = std::move(error);
            return result;
        }
        auto event = connectSocket(
            session->trustedInstance.instanceDirectory.get(),
            socketPath(session->instanceSignature, eventSocketName),
            eventSocketName, *lockPid, session->deadline, error
        );
        if (!event) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = std::move(error);
            return result;
        }
        session->eventSocket = std::move(event->descriptor);
        session->eventSocketNode = event->nodeMetadata;
        if (!peerCredentials(session->eventSocket.get(), session->peer, error)
            || session->peer.uid != ::geteuid()
            || session->peer.pid != *lockPid) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = QStringLiteral(
                "The event peer does not match hyprland.lock."
            );
            return result;
        }
        session->lockContents = initialLock;
        if (!openProcess(*session, error)
            || !readAndValidateProcessContext(*session, true, error)
            || !verifyLock(*session, false, error)) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = std::move(error);
            return result;
        }

        QByteArray reply;
        if (!request(
                *session, QByteArrayLiteral("j/version"), reply, error
            )) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = std::move(error);
            return result;
        }
        if (!parseAndCheckVersion(
                reply, configuredVersionPolicy, error
            )) {
            result.errorCode = QStringLiteral("UnsupportedVersion");
            result.errorMessage = std::move(error);
            return result;
        }
        if (!request(*session, command, reply, error)) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = std::move(error);
            return result;
        }
        if (!verifyIdentity(*session, error)) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = std::move(error);
            return result;
        }
        const auto identity = runtimeIdentity(*session);
        if (identity.isEmpty()) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = QStringLiteral(
                "The authenticated Hyprland runtime identity is incomplete."
            );
            return result;
        }

        result.success = true;
        result.runtimeIdentity = identity;
        result.reply = std::move(reply);
        return result;
    }

    [[nodiscard]] bool drainEvents(Session &session, QString &error) const
    {
        session.eventBuffer.clear();
        qsizetype drained = 0;
        std::array<char, 8192> buffer {};
        while (true) {
            const auto count = ::recv(
                session.eventSocket.get(), buffer.data(), buffer.size(),
                MSG_DONTWAIT
            );
            if (count > 0) {
                if (count > maximumEventBufferBytes - drained) {
                    error = QStringLiteral("The queued Hyprland event stream exceeded its limit.");
                    return false;
                }
                drained += count;
                continue;
            }
            if (count == 0) {
                error = QStringLiteral("The Hyprland event stream closed.");
                return false;
            }
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            error = QStringLiteral("The Hyprland event stream could not be drained.");
            return false;
        }
    }

    [[nodiscard]] bool waitForReloadEvents(
        Session &session,
        const QByteArrayView exactNonceEvent,
        QString &error
    ) const
    {
        bool nonceObserved = exactNonceEvent.isEmpty();
        std::array<char, 8192> buffer {};
        while (!session.deadline.hasExpired()) {
            while (true) {
                const auto newline = session.eventBuffer.indexOf('\n');
                if (newline < 0) break;
                if (newline > maximumEventFrameBytes) {
                    error = QStringLiteral("A Hyprland event frame exceeded its limit.");
                    return false;
                }
                const auto frame = session.eventBuffer.first(newline);
                session.eventBuffer.remove(0, newline + 1);
                if (!nonceObserved && frame == exactNonceEvent) {
                    nonceObserved = true;
                    continue;
                }
                if (frame == QByteArrayLiteral("configreloaded>>")) {
                    if (!nonceObserved) {
                        error = QStringLiteral("Hyprland reloaded before emitting the expected activation nonce.");
                        return false;
                    }
                    return true;
                }
            }
            if (session.eventBuffer.size() > maximumEventFrameBytes) {
                error = QStringLiteral("A partial Hyprland event frame exceeded its limit.");
                return false;
            }

            short events = 0;
            if (!pollUntil(session.eventSocket.get(), POLLIN,
                           session.deadline, events, error)) {
                return false;
            }
            if ((events & (POLLERR | POLLNVAL)) != 0) {
                error = QStringLiteral("The Hyprland event stream failed.");
                return false;
            }
            const auto count = ::recv(
                session.eventSocket.get(), buffer.data(), buffer.size(), 0
            );
            if (count > 0) {
                if (count > maximumEventBufferBytes
                                - session.eventBuffer.size()) {
                    error = QStringLiteral("The Hyprland event buffer exceeded its limit.");
                    return false;
                }
                session.eventBuffer.append(buffer.data(), count);
                continue;
            }
            if (count == 0) {
                error = QStringLiteral("The Hyprland event stream closed.");
                return false;
            }
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            error = QStringLiteral("The Hyprland event stream could not be read.");
            return false;
        }
        error = QStringLiteral("The Hyprland activation deadline expired.");
        return false;
    }

    [[nodiscard]] bool reloadBarrier(
        Session &session,
        const QByteArrayView command,
        const QByteArrayView nonceEvent,
        QString &error
    ) const
    {
        // Open and authenticate the one-shot control connection first. The
        // event drain is deliberately the final operation before the request.
        auto connection = openControl(session, error);
        if (!connection) return false;
        if (!drainEvents(session, error)) return false;

        QByteArray reply;
        if (!sendRequest(
                session, std::move(*connection), command, reply, error
            ) || reply != QByteArrayLiteral("ok")) {
            if (error.isEmpty()) {
                error = QStringLiteral("Hyprland did not acknowledge the reload exactly.");
            }
            return false;
        }
        return waitForReloadEvents(session, nonceEvent, error);
    }
};

HyprlandIpcRuntime::HyprlandIpcRuntime(
    QString runtimeRoot,
    QString instanceSignature,
    QString stableEntrypoint,
    const int timeoutMilliseconds,
    InstanceSignatureProvider instanceSignatureProvider
)
    : impl_(std::make_unique<Impl>())
{
    // Construction is intentionally side-effect free. Runtime and process
    // state are opened only by explicit runtime operations such as prepare()
    // or authenticated discovery.
    impl_->runtimeRoot = std::move(runtimeRoot);
    impl_->startupInstanceSignature = std::move(instanceSignature);
    impl_->stableEntrypoint = std::move(stableEntrypoint);
    impl_->timeoutMilliseconds = timeoutMilliseconds;
    impl_->instanceSignatureProvider = std::move(instanceSignatureProvider);
}

HyprlandIpcRuntime::~HyprlandIpcRuntime() = default;

void HyprlandIpcRuntime::setVersionPolicy(HyprlandVersionPolicy policy)
{
    const QMutexLocker locker(&impl_->mutex);
    impl_->versionPolicy = std::move(policy);
    impl_->versionPolicyConfigured = true;
}

bool HyprlandIpcRuntime::canSatisfy(
    const ActivationRequirement requirement
) const
{
    return requirement == ActivationRequirement::Reload;
}

ConnectedDisplaysResult HyprlandIpcRuntime::connectedDisplays()
{
    return connectedDisplays(impl_->timeoutMilliseconds);
}

ConnectedDisplaysResult HyprlandIpcRuntime::connectedDisplays(
    const int maximumWaitMilliseconds
)
{
    ConnectedDisplaysResult result;
    const auto query = impl_->authenticatedQuery(
        QByteArrayLiteral("j/monitors all"), maximumWaitMilliseconds,
        QStringView(u"connected-display")
    );
    if (!query.success) {
        result.errorCode = query.errorCode;
        result.errorMessage = query.errorMessage;
        return result;
    }

    const auto topology = Hyprland::parseConnectedDisplayTopology(
        QByteArrayView(query.reply)
    );
    if (!topology) {
        result.errorCode = QStringLiteral("VerificationFailed");
        QStringList messages;
        for (const auto &item : topology.errors) {
            messages.append(item.message);
            if (messages.size() == 3) break;
        }
        result.errorMessage = messages.join(QStringLiteral("; "));
        return result;
    }

    result.success = true;
    result.runtimeIdentity = query.runtimeIdentity;
    result.topology = *topology.value;
    return result;
}

ConnectedInputDevicesResult HyprlandIpcRuntime::connectedInputDevices(
    const QByteArrayView serviceEpoch
)
{
    return connectedInputDevices(serviceEpoch, impl_->timeoutMilliseconds);
}

ConnectedInputDevicesResult HyprlandIpcRuntime::connectedInputDevices(
    const QByteArrayView serviceEpoch,
    const int maximumWaitMilliseconds
)
{
    ConnectedInputDevicesResult result;
    if (serviceEpoch.isEmpty() || serviceEpoch.size() > 512) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral(
            "The input-device inventory epoch is invalid."
        );
        return result;
    }

    const auto query = impl_->authenticatedQuery(
        QByteArrayLiteral("j/devices"), maximumWaitMilliseconds,
        QStringView(u"connected-input-device")
    );
    if (!query.success) {
        result.errorCode = query.errorCode;
        result.errorMessage = query.errorMessage;
        return result;
    }

    const auto inventory = Hyprland::parseConnectedInputDeviceInventory(
        QByteArrayView(query.reply), query.runtimeIdentity, serviceEpoch
    );
    if (!inventory) {
        result.errorCode = QStringLiteral("VerificationFailed");
        QStringList messages;
        for (const auto &item : inventory.errors) {
            messages.append(item.message);
            if (messages.size() == 3) break;
        }
        result.errorMessage = messages.join(QStringLiteral("; "));
        return result;
    }

    result.success = true;
    result.runtimeIdentity = query.runtimeIdentity;
    result.inventory = *inventory.value;
    return result;
}
RuntimeSessionResult HyprlandIpcRuntime::prepare(
    const ActivationRequirement requirement,
    const RuntimeActivationMode mode
)
{
    RuntimeSessionResult result;
    const QDeadlineTimer operationDeadline(
        std::max(impl_->timeoutMilliseconds, 0), Qt::PreciseTimer
    );
    if (requirement != ActivationRequirement::Reload) {
        result.errorCode = QStringLiteral("ActivationRequired");
        result.errorMessage = QStringLiteral("The live runtime supports reload activation only.");
        return result;
    }

    const QMutexLocker prepareLocker(&impl_->prepareMutex);
    HyprlandVersionPolicy versionPolicy;
    InstanceSignatureProvider signatureProvider;
    QString startupSignature;
    {
        const QMutexLocker stateLocker(&impl_->mutex);
        if (!cleanAbsolutePath(impl_->runtimeRoot)
            || !cleanAbsolutePath(impl_->stableEntrypoint)
            || impl_->timeoutMilliseconds <= 0
            || !impl_->versionPolicyConfigured
            || (impl_->versionPolicy.maximumPatch
                && *impl_->versionPolicy.maximumPatch
                       < impl_->versionPolicy.minimumPatch)) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = QStringLiteral("The Hyprland runtime configuration is invalid.");
            return result;
        }
        versionPolicy = impl_->versionPolicy;
        signatureProvider = impl_->instanceSignatureProvider;
        startupSignature = impl_->startupInstanceSignature;
    }

    auto pending = std::make_shared<Impl::Session>();
    pending->mode = mode;
    pending->deadline = operationDeadline;
    if (pending->deadline.hasExpired()) {
        result.errorCode = QStringLiteral("RuntimeUnavailable");
        result.errorMessage = QStringLiteral("The Hyprland activation deadline is invalid or expired.");
        return result;
    }

    QString error;
    QString resolvedSignature;
    if (signatureProvider) {
        InstanceSignatureResult provided;
        try {
            provided = signatureProvider(
                remainingPollMilliseconds(pending->deadline)
            );
        } catch (...) {
            provided.errorMessage = QStringLiteral(
                "The injected Hyprland instance provider failed."
            );
        }
        if (!provided.success
            || !validInstanceSignature(provided.signature)) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = provided.errorMessage.isEmpty()
                ? QStringLiteral(
                    "The Hyprland instance provider returned an invalid signature."
                ) : provided.errorMessage;
            return result;
        }
        resolvedSignature = std::move(provided.signature);
    } else {
        const auto discovered = systemdInstanceSignature(
            remainingPollMilliseconds(pending->deadline)
        );
        if (discovered.state == SignatureLookupState::Invalid) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = discovered.error;
            return result;
        }
        resolvedSignature = discovered.state == SignatureLookupState::Resolved
            ? discovered.signature : startupSignature;
        if (!validInstanceSignature(resolvedSignature)) {
            result.errorCode = QStringLiteral("RuntimeUnavailable");
            result.errorMessage = discovered.error.isEmpty()
                ? QStringLiteral("No valid Hyprland instance signature is available.")
                : discovered.error;
            return result;
        }
    }
    if (pending->deadline.hasExpired()) {
        result.errorCode = QStringLiteral("RuntimeUnavailable");
        result.errorMessage = QStringLiteral(
            "The Hyprland activation deadline expired during instance discovery."
        );
        return result;
    }
    pending->instanceSignature = resolvedSignature;
    auto trusted = openTrustedInstance(
        impl_->runtimeRoot, pending->instanceSignature, error
    );
    if (!trusted) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    pending->trustedInstance = std::move(*trusted);

    // The event subscription is pinned before any reload or publication.
    // Its peer establishes the sole UID/PID accepted by this session.
    // Obtain the expected event peer from the trusted lock, then preconnect
    // the event stream. This occurs before every reload or namespace change.
    QByteArray initialLock;
    if (!readStableRegularAt(
            pending->trustedInstance.instanceDirectory.get(), lockFileName,
            maximumLockBytes, true, initialLock, error
        )) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    const auto lockPid = parseLockFile(initialLock, error);
    if (!lockPid) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    auto event = connectSocket(
        pending->trustedInstance.instanceDirectory.get(),
        impl_->socketPath(pending->instanceSignature, eventSocketName),
        eventSocketName,
        *lockPid, pending->deadline, error
    );
    if (!event) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    pending->eventSocket = std::move(event->descriptor);
    pending->eventSocketNode = event->nodeMetadata;
    if (!peerCredentials(pending->eventSocket.get(), pending->peer, error)
        || pending->peer.uid != ::geteuid()
        || pending->peer.pid != *lockPid) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral("The event peer does not match hyprland.lock.");
        return result;
    }
    pending->lockContents = initialLock;

    if (!impl_->openProcess(*pending, error)
        || !impl_->readAndValidateProcessContext(*pending, true, error)
        || !impl_->verifyLock(*pending, false, error)) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }

    QByteArray reply;
    if (!impl_->request(*pending, QByteArrayLiteral("j/version"), reply, error)) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    if (!parseAndCheckVersion(reply, versionPolicy, error)) {
        result.errorCode = QStringLiteral("UnsupportedVersion");
        result.errorMessage = std::move(error);
        return result;
    }

    // A normal reload is a pre-publication barrier. It proves the existing
    // provider is responsive without changing the provider implementation.
    if (!impl_->reloadBarrier(
            *pending, QByteArrayLiteral("reload"), QByteArrayView(), error
        )) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }

    if (!impl_->request(
            *pending, QByteArrayLiteral("j/configerrors"), reply, error
        )) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    const auto baselineErrors = parseConfigErrors(reply, error);
    if (!baselineErrors) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    const auto rollbackMode = mode == RuntimeActivationMode::ManagedRollback
        || mode == RuntimeActivationMode::LegacyRollback;
    if (!rollbackMode && *baselineErrors != QByteArrayLiteral("[]")) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral("The current Hyprland configuration has errors.");
        return result;
    }

    if (!impl_->request(
            *pending, QByteArrayLiteral("j/status"), reply, error
        )) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    const auto provider = parseProvider(reply, error);
    if (!provider) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    if ((mode == RuntimeActivationMode::ManagedReload
         || mode == RuntimeActivationMode::ManagedRollback)
        && *provider != QStringLiteral("lua")) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral("Managed reload requires Hyprland's Lua provider.");
        return result;
    }
    if (!impl_->verifyIdentity(*pending, error)) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }

    RuntimeSession publicSession {
        .token = QUuid::createUuid().toByteArray(QUuid::WithoutBraces),
        .baselineConfigErrors = *baselineErrors,
        .baselineProvider = *provider,
        .runtimeIdentity = impl_->runtimeIdentity(*pending),
    };
    if (publicSession.runtimeIdentity.isEmpty()) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral(
            "The authenticated Hyprland runtime identity is incomplete."
        );
        return result;
    }
    {
        const QMutexLocker stateLocker(&impl_->mutex);
        while (impl_->sessions.contains(publicSession.token)) {
            publicSession.token = QUuid::createUuid().toByteArray(
                QUuid::WithoutBraces
            );
        }
        pending->publicSession = publicSession;
        impl_->sessions.emplace(publicSession.token, pending);
    }
    result.success = true;
    result.session = std::move(publicSession);
    return result;
}

RuntimeProofResult HyprlandIpcRuntime::reloadAndConfirm(
    const RuntimeSession &session,
    const QStringView exactNonce,
    const QByteArrayView expectedConfigErrors,
    const QStringView expectedProvider
)
{
    RuntimeProofResult result;
    std::shared_ptr<Impl::Session> activeSession;
    {
        const QMutexLocker stateLocker(&impl_->mutex);
        const auto found = impl_->sessions.find(session.token);
        if (found == impl_->sessions.end()
            || found->second->publicSession.baselineConfigErrors
                   != session.baselineConfigErrors
            || found->second->publicSession.baselineProvider
                   != session.baselineProvider) {
            result.errorCode = QStringLiteral("VerificationFailed");
            result.errorMessage = QStringLiteral("The Hyprland activation session is invalid.");
            return result;
        }
        activeSession = found->second;
    }
    const QMutexLocker operationLocker(&activeSession->operationMutex);
    auto &active = *activeSession;
    const auto legacyRollback = active.mode
        == RuntimeActivationMode::LegacyRollback;
    const auto nonceMatchesMode = legacyRollback
        ? exactNonce.isEmpty()
        : (!exactNonce.isEmpty() && validNonce(exactNonce));
    const auto providerMatchesMode = validProvider(expectedProvider)
        && (legacyRollback || expectedProvider == QStringView(u"lua"));
    if (!nonceMatchesMode || !providerMatchesMode) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral("The requested activation proof is invalid.");
        return result;
    }

    QString error;
    const auto expectedErrors = parseConfigErrors(
        expectedConfigErrors, error
    );
    if (!expectedErrors) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral("The expected configerrors value is invalid.");
        return result;
    }

    const auto fullReset = active.mode == RuntimeActivationMode::AdoptionFullReset
        || active.mode == RuntimeActivationMode::LegacyRollback;
    const auto command = fullReset
        ? QByteArrayLiteral("reload full-reset")
        : QByteArrayLiteral("reload");
    const auto nonceEvent = exactNonce.isEmpty()
        ? QByteArray()
        : QByteArrayLiteral("custom>>hyprshelld:")
              + exactNonce.toString().toLatin1();

    if (!impl_->reloadBarrier(active, command, nonceEvent, error)) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }

    QByteArray reply;
    if (!impl_->request(
            active, QByteArrayLiteral("j/configerrors"), reply, error
        )) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    const auto observedErrors = parseConfigErrors(reply, error);
    if (!observedErrors || *observedErrors != *expectedErrors) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = observedErrors
            ? QStringLiteral("Hyprland reported unexpected configuration errors.")
            : std::move(error);
        return result;
    }

    if (!impl_->request(
            active, QByteArrayLiteral("j/status"), reply, error
        )) {
        result.errorCode = QStringLiteral("ReloadFailed");
        result.errorMessage = std::move(error);
        return result;
    }
    const auto provider = parseProvider(reply, error);
    if (!provider || *provider != expectedProvider) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = provider
            ? QStringLiteral("Hyprland activated an unexpected config provider.")
            : std::move(error);
        return result;
    }
    if (!impl_->verifyIdentity(active, error)) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = std::move(error);
        return result;
    }

    if (active.cancelled.load(std::memory_order_acquire)) {
        result.errorCode = QStringLiteral("VerificationFailed");
        result.errorMessage = QStringLiteral("The Hyprland activation session was cancelled.");
        return result;
    }
    result.success = true;
    return result;
}

void HyprlandIpcRuntime::cancel(const RuntimeSession &session) noexcept
{
    std::shared_ptr<Impl::Session> active;
    {
        const QMutexLocker stateLocker(&impl_->mutex);
        const auto found = impl_->sessions.find(session.token);
        if (found == impl_->sessions.end()) return;
        active = found->second;
        impl_->sessions.erase(found);
    }

    active->cancelled.store(true, std::memory_order_release);
    if (active->eventSocket.valid()) {
        static_cast<void>(::shutdown(active->eventSocket.get(), SHUT_RDWR));
    }
    const QMutexLocker cancellationLocker(&active->cancellationMutex);
    if (active->activeControlDescriptor >= 0) {
        static_cast<void>(::shutdown(
            active->activeControlDescriptor, SHUT_RDWR
        ));
    }
}

} // namespace HyprShelld::Compositor
