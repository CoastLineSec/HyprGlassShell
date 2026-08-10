#include "activation_backend.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

#include <array>
#include <chrono>
#include <cstring>
#include <future>
#include <memory>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

using namespace HyprShelld::Compositor;
using namespace std::chrono_literals;

namespace {

constexpr auto signature = "hyprshelld-ipc-test";
constexpr auto nonce =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

[[nodiscard]] QByteArray validMonitorReply()
{
    return QByteArrayLiteral(R"json([{
        "id":7,"name":"DP-1","description":"Acme Panel DP-1",
        "make":"Acme","model":"Panel","serial":"serial-DP-1",
        "width":2560,"height":1440,"physicalWidth":600,
        "physicalHeight":340,"refreshRate":143.9987,"x":0,"y":0,
        "activeWorkspace":{"id":1,"name":"1"},
        "specialWorkspace":{"id":0,"name":""},
        "reserved":[0,0,0,0],"scale":1.25,"transform":0,
        "focused":true,"dpmsStatus":true,"vrr":false,
        "solitary":"0","solitaryBlockedBy":null,
        "activelyTearing":false,"tearingBlockedBy":null,
        "directScanoutTo":"0","directScanoutBlockedBy":null,
        "disabled":false,"currentFormat":"XRGB8888","mirrorOf":"none",
        "availableModes":["2560x1440@144.00Hz","1920x1080@60.00Hz"],
        "colorManagementPreset":"srgb","sdrBrightness":1.0,
        "sdrSaturation":1.0,"sdrMinLuminance":0.2,
        "sdrMaxLuminance":80,"hardwareCursorsInUse":true
    }])json");
}

[[nodiscard]] QByteArray environmentValue(
    const char *name,
    const QByteArray &fallback = {}
)
{
    const auto value = qgetenv(name);
    return value.isEmpty() ? fallback : value;
}

[[nodiscard]] bool writeAll(const int descriptor, const QByteArrayView bytes)
{
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::send(
            descriptor, bytes.data() + offset,
            static_cast<size_t>(bytes.size() - offset), MSG_NOSIGNAL
        );
        if (count > 0) {
            offset += count;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

[[nodiscard]] int createListener(const QString &path)
{
    const auto encoded = QFile::encodeName(path);
    if (encoded.isEmpty()
        || encoded.size() >= static_cast<qsizetype>(sizeof(sockaddr_un::sun_path))) {
        return -1;
    }
    const auto descriptor = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (descriptor < 0) return -1;
    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::memcpy(
        address.sun_path, encoded.constData(),
        static_cast<size_t>(encoded.size())
    );
    address.sun_path[encoded.size()] = '\0';
    const auto length = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + encoded.size() + 1
    );
    if (::bind(
            descriptor, reinterpret_cast<const sockaddr *>(&address), length
        ) != 0 || ::listen(descriptor, 16) != 0) {
        const auto failure = errno;
        ::dprintf(
            STDERR_FILENO, "listen failure for %s: %s\n",
            encoded.constData(), std::strerror(failure)
        );
        ::close(descriptor);
        return -1;
    }
    return descriptor;
}

[[nodiscard]] bool replaceListener(const QString &path, int &listener)
{
    const auto encoded = QFile::encodeName(path);
    if (::unlink(encoded.constData()) != 0) return false;
    const auto replacement = createListener(path);
    if (replacement < 0) return false;
    ::close(listener);
    listener = replacement;
    return true;
}

[[nodiscard]] QByteArray readRequest(const int descriptor)
{
    QByteArray result;
    std::array<char, 512> buffer {};
    while (result.size() <= 1024) {
        const auto count = ::read(descriptor, buffer.data(), buffer.size());
        if (count > 0) {
            result.append(buffer.data(), count);
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        break;
    }
    return result;
}

void appendCommand(const QString &path, const QByteArrayView command)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    file.write(command.data(), command.size());
    file.write("\n");
    file.flush();
}

[[nodiscard]] bool sendEvent(const int descriptor, const QByteArray &bytes)
{
    return bytes.isEmpty() || writeAll(descriptor, bytes);
}

[[nodiscard]] QByteArray finalEvents(const QByteArray &mode)
{
    const auto exact = QByteArrayLiteral("custom>>hyprshelld:")
        + environmentValue("HYPRSHELLD_FAKE_NONCE", nonce);
    if (mode == "proper") {
        return exact + QByteArrayLiteral("\nconfigreloaded>>\n");
    }
    if (mode == "generic-before-nonce") {
        return QByteArrayLiteral("configreloaded>>\n") + exact + '\n';
    }
    if (mode == "generic-only") {
        return QByteArrayLiteral("configreloaded>>\n");
    }
    if (mode == "wrong-nonce") {
        return QByteArrayLiteral(
            "custom>>hyprshelld:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            "configreloaded>>\n"
        );
    }
    if (mode == "legacy") {
        return QByteArrayLiteral("configreloaded>>\n");
    }
    if (mode == "oversized") {
        return QByteArray(64 * 1024 + 1, 'x') + '\n';
    }
    if (mode == "partial") {
        return exact;
    }
    return {};
}

[[nodiscard]] int runFakeHyprland()
{
    ::umask(0077);
    const auto runtimeRoot = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
    const auto fakeSignature = QFile::decodeName(
        environmentValue("HYPRSHELLD_FAKE_SIGNATURE", signature)
    );
    const auto instanceRoot = QDir(runtimeRoot).filePath(
        QStringLiteral("hypr/%1").arg(fakeSignature)
    );
    if (!QDir().mkpath(instanceRoot)
        || ::chmod(QFile::encodeName(runtimeRoot).constData(), 0700) != 0
        || ::chmod(QFile::encodeName(QDir(runtimeRoot).filePath(
                         QStringLiteral("hypr"))).constData(), 0700) != 0
        || ::chmod(QFile::encodeName(instanceRoot).constData(), 0700) != 0) {
        return 20;
    }

    // Hyprland sets this only after exec. It is intentionally absent from the
    // initial process environment exposed by /proc/<pid>/environ.
    qputenv("HYPRLAND_INSTANCE_SIGNATURE", fakeSignature.toLocal8Bit());

    QFile lock(QDir(instanceRoot).filePath(QStringLiteral("hyprland.lock")));
    if (!lock.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return 21;
    const auto lockMode = environmentValue("HYPRSHELLD_FAKE_LOCK", "exact");
    QByteArray lockBytes;
    if (lockMode == "malformed") {
        lockBytes = QByteArrayLiteral("1\n");
    } else {
        auto lockPid = static_cast<qint64>(::getpid());
        if (lockMode == "wrong-pid") ++lockPid;
        lockBytes = QByteArray::number(lockPid)
            + QByteArrayLiteral("\nwayland-test\n");
    }
    if (lock.write(lockBytes) != lockBytes.size() || !lock.flush()) return 22;
    lock.close();
    QFile::setPermissions(
        lock.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner
    );

    const auto controlPath = QDir(instanceRoot).filePath(
        QStringLiteral(".socket.sock")
    );
    const auto eventPath = QDir(instanceRoot).filePath(
        QStringLiteral(".socket2.sock")
    );
    auto controlListener = createListener(controlPath);
    auto eventListener = createListener(eventPath);
    if (controlListener < 0 || eventListener < 0) return 23;

    static constexpr char ready[] = "READY HIS_POST_EXEC\n";
    if (::write(STDOUT_FILENO, ready, sizeof(ready) - 1)
        != static_cast<ssize_t>(sizeof(ready) - 1)) {
        return 24;
    }

    auto event = ::accept4(eventListener, nullptr, nullptr, SOCK_CLOEXEC);
    if (event < 0) return 25;
    const auto version = environmentValue(
        "HYPRSHELLD_FAKE_VERSION", R"({"version":"0.56.1"})"
    );
    const auto baselineErrors = environmentValue(
        "HYPRSHELLD_FAKE_BASELINE_ERRORS", "[]"
    );
    const auto baselineProvider = environmentValue(
        "HYPRSHELLD_FAKE_BASELINE_PROVIDER", "lua"
    );
    const auto observedErrors = environmentValue(
        "HYPRSHELLD_FAKE_FINAL_ERRORS", "[]"
    );
    const auto observedProvider = environmentValue(
        "HYPRSHELLD_FAKE_FINAL_PROVIDER", "lua"
    );
    const auto eventMode = environmentValue(
        "HYPRSHELLD_FAKE_FINAL_EVENTS", "proper"
    );
    const auto reloadReply = environmentValue(
        "HYPRSHELLD_FAKE_RELOAD_REPLY", "ok"
    );
    auto monitorsReply = environmentValue(
        "HYPRSHELLD_FAKE_MONITORS", validMonitorReply()
    );
    const auto monitorReplyBytes = environmentValue(
        "HYPRSHELLD_FAKE_MONITOR_BYTES", "0"
    ).toInt();
    if (monitorReplyBytes > 0) {
        monitorsReply = QByteArray(monitorReplyBytes, 'x');
    }
    const auto logPath = QFile::decodeName(qgetenv("HYPRSHELLD_FAKE_LOG"));
    const auto delay = environmentValue("HYPRSHELLD_FAKE_DELAY_MS", "0")
                           .toInt();
    const auto stale = environmentValue("HYPRSHELLD_FAKE_STALE") == "1";
    const auto replaceEndpoint = environmentValue(
        "HYPRSHELLD_FAKE_REPLACE_ENDPOINT"
    );
    const auto exitAfter = environmentValue("HYPRSHELLD_FAKE_EXIT_AFTER");
    int reloadCount = 0;
    int errorCount = 0;
    int statusCount = 0;
    bool endpointReplaced = false;

    while (true) {
        const auto client = ::accept4(
            controlListener, nullptr, nullptr, SOCK_CLOEXEC
        );
        if (client < 0) {
            if (errno == EINTR) continue;
            break;
        }
        const auto command = readRequest(client);
        appendCommand(logPath, command);
        if (delay > 0) ::usleep(static_cast<useconds_t>(delay) * 1000U);

        QByteArray reply;
        QByteArray events;
        bool preflightReload = false;
        if (command == "j/version") {
            reply = version;
        } else if (command == "j/monitors all") {
            reply = monitorsReply;
        } else if (command == "j/configerrors") {
            reply = errorCount++ == 0 ? baselineErrors : observedErrors;
        } else if (command == "j/status") {
            const auto provider = statusCount++ == 0
                ? baselineProvider : observedProvider;
            if (provider.startsWith('{')) {
                reply = provider;
            } else {
                reply = QByteArrayLiteral("{\"configProvider\":\"")
                    + provider + QByteArrayLiteral("\"}");
            }
        } else if (command == "reload"
                   || command == "reload full-reset") {
            const auto preflight = reloadCount++ == 0;
            preflightReload = preflight;
            reply = preflight ? QByteArrayLiteral("ok") : reloadReply;
            if (preflight) {
                events = QByteArrayLiteral("configreloaded>>\n");
                if (stale) {
                    events += QByteArrayLiteral("custom>>hyprshelld:")
                        + environmentValue("HYPRSHELLD_FAKE_NONCE", nonce)
                        + QByteArrayLiteral("\nconfigreloaded>>\n");
                }
            } else {
                events = finalEvents(eventMode);
            }
        } else {
            reply = QByteArrayLiteral("unknown request");
        }

        if (!endpointReplaced
            && ((replaceEndpoint == "control" && command == "j/version")
                || (replaceEndpoint == "event" && preflightReload)
                || (replaceEndpoint == "event-query"
                    && command == "j/version"))) {
            auto &listener = replaceEndpoint == "control"
                ? controlListener : eventListener;
            const auto &path = replaceEndpoint == "control"
                ? controlPath : eventPath;
            if (!replaceListener(path, listener)) break;
            endpointReplaced = true;
        }

        if (!endpointReplaced && replaceEndpoint == "lock"
            && command == "j/monitors all") {
            QFile replacement(lock.fileName());
            if (!replacement.open(QIODevice::WriteOnly | QIODevice::Truncate)
                || replacement.write(
                       QByteArray::number(static_cast<qint64>(::getpid()) + 1)
                       + QByteArrayLiteral("\nwayland-test\n")
                   ) <= 0
                || !replacement.flush()) {
                break;
            }
            replacement.close();
            endpointReplaced = true;
        }
        if (!endpointReplaced && replaceEndpoint == "instance"
            && command == "j/monitors all") {
            const auto moved = instanceRoot + QStringLiteral(".replaced");
            if (!QDir().rename(instanceRoot, moved)
                || !QDir().mkpath(instanceRoot)
                || ::chmod(QFile::encodeName(instanceRoot).constData(), 0700)
                    != 0) {
                break;
            }
            endpointReplaced = true;
        }

        const auto replied = writeAll(client, reply);
        ::shutdown(client, SHUT_WR);
        ::close(client);
        if (command == exitAfter) break;
        if (eventMode == "eof" && reloadCount >= 2
            && (command == "reload" || command == "reload full-reset")) {
            ::shutdown(event, SHUT_RDWR);
            ::close(event);
            event = -1;
        }
        if (!replied || (event >= 0 && !sendEvent(event, events))) break;
    }

    if (event >= 0) ::close(event);
    ::close(eventListener);
    ::close(controlListener);
    return 0;
}

struct FakeOptions final {
    QByteArray version = R"({"version":"0.56.1"})";
    QByteArray baselineErrors = "[]";
    QByteArray baselineProvider = "lua";
    QByteArray finalErrors = "[]";
    QByteArray finalProvider = "lua";
    QByteArray finalEvents = "proper";
    QByteArray reloadReply = "ok";
    QByteArray monitors = validMonitorReply();
    int monitorReplyBytes = 0;
    QByteArray lock = "exact";
    QByteArray replaceEndpoint;
    QByteArray exitAfter;
    bool stale = false;
    bool explicitEnvironmentConfig = false;
    bool safeMode = false;
    int delayMilliseconds = 0;
};

class FakePeer final {
public:
    FakePeer()
        : root_(QStringLiteral("/tmp/hyprshelld-ipc-runtime-XXXXXX"))
    {
        runtimeRoot = QDir(root_.path()).filePath(QStringLiteral("runtime"));
        configRoot = QDir(root_.path()).filePath(QStringLiteral("config"));
        logPath = QDir(root_.path()).filePath(QStringLiteral("commands.log"));
        stableEntrypoint = QDir(configRoot).filePath(
            QStringLiteral("hypr/hyprland.lua")
        );
    }

    ~FakePeer() { stop(); }

    [[nodiscard]] bool start(const FakeOptions &options = {})
    {
        if (!root_.isValid() || !QDir().mkpath(runtimeRoot)
            || !QDir().mkpath(QFileInfo(stableEntrypoint).absolutePath())) {
            error = QStringLiteral("fixture directories could not be created");
            return false;
        }
        ::chmod(QFile::encodeName(root_.path()).constData(), 0700);
        ::chmod(QFile::encodeName(runtimeRoot).constData(), 0700);
        ::chmod(QFile::encodeName(configRoot).constData(), 0700);

        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE"));
        environment.remove(QStringLiteral("HYPRLAND_CONFIG"));
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeRoot);
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configRoot);
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_HYPRLAND_CHILD"),
            QStringLiteral("1")
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_SIGNATURE"),
            QString::fromLatin1(signature)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_NONCE"),
            QString::fromLatin1(nonce)
        );
        environment.insert(QStringLiteral("HYPRSHELLD_FAKE_LOG"), logPath);
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_VERSION"),
            QString::fromUtf8(options.version)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_BASELINE_ERRORS"),
            QString::fromUtf8(options.baselineErrors)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_BASELINE_PROVIDER"),
            QString::fromUtf8(options.baselineProvider)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_FINAL_ERRORS"),
            QString::fromUtf8(options.finalErrors)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_FINAL_PROVIDER"),
            QString::fromUtf8(options.finalProvider)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_FINAL_EVENTS"),
            QString::fromLatin1(options.finalEvents)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_RELOAD_REPLY"),
            QString::fromLatin1(options.reloadReply)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_MONITORS"),
            QString::fromUtf8(options.monitors)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_MONITOR_BYTES"),
            QString::number(options.monitorReplyBytes)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_LOCK"),
            QString::fromLatin1(options.lock)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_STALE"),
            options.stale ? QStringLiteral("1") : QStringLiteral("0")
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_REPLACE_ENDPOINT"),
            QString::fromLatin1(options.replaceEndpoint)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_EXIT_AFTER"),
            QString::fromLatin1(options.exitAfter)
        );
        environment.insert(
            QStringLiteral("HYPRSHELLD_FAKE_DELAY_MS"),
            QString::number(options.delayMilliseconds)
        );
        if (options.explicitEnvironmentConfig) {
            environment.insert(
                QStringLiteral("HYPRLAND_CONFIG"),
                QDir(configRoot).filePath(QStringLiteral("alternate.lua"))
            );
        }
        process_.setProcessEnvironment(environment);
        process_.setProgram(QCoreApplication::applicationFilePath());
        process_.setArguments(
            options.safeMode ? QStringList({QStringLiteral("--safe-mode")})
                             : QStringList()
        );
        process_.setProcessChannelMode(QProcess::SeparateChannels);
        process_.start();
        if (!process_.waitForStarted(3000)
            || !process_.waitForReadyRead(3000)) {
            error = QStringLiteral("fake peer did not become ready: %1 %2")
                .arg(process_.errorString(),
                     QString::fromLocal8Bit(process_.readAllStandardError()));
            return false;
        }
        const auto ready = process_.readLine();
        if (ready != QByteArrayLiteral("READY HIS_POST_EXEC\n")) {
            error = QStringLiteral("unexpected fake-peer readiness: %1; stderr=%2")
                .arg(QString::fromLocal8Bit(ready),
                     QString::fromLocal8Bit(process_.readAllStandardError()));
            return false;
        }
        return true;
    }

    void stop()
    {
        if (process_.state() == QProcess::NotRunning) return;
        process_.terminate();
        if (!process_.waitForFinished(1000)) {
            process_.kill();
            process_.waitForFinished(1000);
        }
    }

    [[nodiscard]] QStringList commands() const
    {
        QFile file(logPath);
        if (!file.open(QIODevice::ReadOnly)) return {};
        const auto lines = file.readAll().split('\n');
        QStringList result;
        for (const auto &line : lines) {
            if (!line.isEmpty()) result.append(QString::fromLatin1(line));
        }
        return result;
    }

    [[nodiscard]] std::unique_ptr<HyprlandIpcRuntime> runtime(
        const int timeoutMilliseconds = 2500,
        const QString &stableOverride = {}
    ) const
    {
        auto result = std::make_unique<HyprlandIpcRuntime>(
            runtimeRoot, QString::fromLatin1(signature),
            stableOverride.isEmpty() ? stableEntrypoint : stableOverride,
            timeoutMilliseconds,
            [](const int timeout) {
                return HyprlandIpcRuntime::InstanceSignatureResult{
                    .success = timeout > 0,
                    .signature = QString::fromLatin1(signature),
                };
            }
        );
        result->setVersionPolicy({
            .major = 0,
            .minor = 56,
            .minimumPatch = 1,
            .maximumPatch = 9,
        });
        return result;
    }

    QString runtimeRoot;
    QString configRoot;
    QString stableEntrypoint;
    QString logPath;
    QString error;

private:
    QTemporaryDir root_;
    QProcess process_;
};

[[nodiscard]] RuntimeSession prepareOrReport(
    HyprlandIpcRuntime &runtime,
    const RuntimeActivationMode mode
)
{
    const auto prepared = runtime.prepare(ActivationRequirement::Reload, mode);
    if (!prepared.success || !prepared.session) {
        qWarning().noquote() << prepared.errorCode << prepared.errorMessage;
        return {};
    }
    return *prepared.session;
}

} // namespace

class HyprlandIpcRuntimeTest final : public QObject {
    Q_OBJECT

private slots:
    void connectedDisplaysUsesExactAuthenticatedRawIpc()
    {
        FakePeer peer;
        QVERIFY2(peer.start(), qPrintable(peer.error));
        auto runtime = peer.runtime();
        const auto connected = runtime->connectedDisplays();
        QVERIFY2(connected.success, qPrintable(connected.errorMessage));
        QVERIFY(connected.topology.has_value());
        QCOMPARE(connected.runtimeIdentity.size(), 64);
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                    .match(connected.runtimeIdentity).hasMatch());
        QCOMPARE(connected.topology->outputs.size(), 1);
        const auto &output = connected.topology->outputs.front();
        QCOMPARE(output.upstreamId, qint64(7));
        QCOMPARE(output.selector, QStringLiteral("DP-1"));
        QCOMPARE(output.width, qint32(2560));
        QCOMPARE(output.height, qint32(1440));
        QCOMPARE(output.scale, 1.25);
        QCOMPARE(output.currentFormat, QStringLiteral("XRGB8888"));
        QCOMPARE(output.modes.size(), 2);
        QVERIFY(connected.topology->document.endsWith('\n'));
        QVERIFY(connected.topology->document.contains(
            QByteArrayLiteral("\"topologyDigest\"")
        ));
        QVERIFY(!connected.topology->document.contains(
            QByteArrayLiteral("solitary")
        ));
        peer.stop();
        QCOMPARE(peer.commands(), QStringList({
            QStringLiteral("j/version"),
            QStringLiteral("j/monitors all"),
        }));
    }

    void connectedDisplaysRejectsMalformedAndOversizeReplies_data()
    {
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<int>("replyBytes");
        QTest::addColumn<QString>("errorCode");
        QTest::newRow("not-an-array")
            << QByteArrayLiteral("{}")
            << 0
            << QStringLiteral("VerificationFailed");
        QTest::newRow("malformed-json")
            << QByteArrayLiteral("[{]")
            << 0
            << QStringLiteral("VerificationFailed");
        auto missingPinnedField = validMonitorReply();
        missingPinnedField.replace(
            QByteArrayLiteral("\"reserved\":[0,0,0,0],"), QByteArray()
        );
        QTest::newRow("missing-pinned-field")
            << missingPinnedField
            << 0
            << QStringLiteral("VerificationFailed");
        QTest::newRow("reply-over-512-kib")
            << QByteArray()
            << (512 * 1024 + 1)
            << QStringLiteral("RuntimeUnavailable");
    }

    void connectedDisplaysRejectsMalformedAndOversizeReplies()
    {
        QFETCH(QByteArray, reply);
        QFETCH(int, replyBytes);
        QFETCH(QString, errorCode);
        FakePeer peer;
        FakeOptions options;
        options.monitors = reply;
        options.monitorReplyBytes = replyBytes;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(1500);
        const auto connected = runtime->connectedDisplays();
        QVERIFY(!connected.success);
        QCOMPARE(connected.errorCode, errorCode);
        QVERIFY(!connected.topology.has_value());
        QVERIFY(connected.runtimeIdentity.isEmpty());
    }

    void connectedDisplaysHonorsOneClampedDeadline()
    {
        FakePeer peer;
        FakeOptions options;
        options.delayMilliseconds = 80;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(2500);
        QElapsedTimer elapsed;
        elapsed.start();
        const auto connected = runtime->connectedDisplays(120);
        QVERIFY(!connected.success);
        QCOMPARE(connected.errorCode, QStringLiteral("RuntimeUnavailable"));
        QVERIFY(elapsed.elapsed() < 700);

        elapsed.restart();
        const auto expired = runtime->connectedDisplays(0);
        QVERIFY(!expired.success);
        QCOMPARE(expired.errorCode, QStringLiteral("RuntimeUnavailable"));
        QVERIFY(elapsed.elapsed() < 100);
    }

    void connectedDisplaysRevalidatesEveryPinnedIdentity_data()
    {
        QTest::addColumn<QByteArray>("lockMode");
        QTest::addColumn<QByteArray>("replacement");
        QTest::addColumn<QByteArray>("exitAfter");
        QTest::newRow("lock-pid-mismatch")
            << QByteArray("wrong-pid") << QByteArray() << QByteArray();
        QTest::newRow("control-socket-replaced")
            << QByteArray("exact") << QByteArray("control") << QByteArray();
        QTest::newRow("event-socket-replaced")
            << QByteArray("exact") << QByteArray("event-query") << QByteArray();
        QTest::newRow("lock-pid-changed-after-query")
            << QByteArray("exact") << QByteArray("lock") << QByteArray();
        QTest::newRow("instance-directory-replaced")
            << QByteArray("exact") << QByteArray("instance") << QByteArray();
        QTest::newRow("peer-dies-after-version")
            << QByteArray("exact") << QByteArray()
            << QByteArray("j/version");
        QTest::newRow("peer-dies-after-monitors")
            << QByteArray("exact") << QByteArray()
            << QByteArray("j/monitors all");
    }

    void connectedDisplaysRevalidatesEveryPinnedIdentity()
    {
        QFETCH(QByteArray, lockMode);
        QFETCH(QByteArray, replacement);
        QFETCH(QByteArray, exitAfter);
        FakePeer peer;
        FakeOptions options;
        options.lock = lockMode;
        options.replaceEndpoint = replacement;
        options.exitAfter = exitAfter;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(1200);
        const auto connected = runtime->connectedDisplays();
        QVERIFY(!connected.success);
        QVERIFY(
            connected.errorCode == QStringLiteral("VerificationFailed")
            || connected.errorCode == QStringLiteral("RuntimeUnavailable")
        );
        QVERIFY(!connected.topology.has_value());
    }

    void connectedDisplaysRejectsInvalidFreshInstanceSignature()
    {
        FakePeer peer;
        QVERIFY2(peer.start(), qPrintable(peer.error));
        for (const auto &provided : {
                 QString(), QStringLiteral("first\nsecond"),
                 QStringLiteral("../instance")}) {
            HyprlandIpcRuntime runtime(
                peer.runtimeRoot, QString::fromLatin1(signature),
                peer.stableEntrypoint, 1000,
                [provided](const int) {
                    return HyprlandIpcRuntime::InstanceSignatureResult{
                        .success = true,
                        .signature = provided,
                    };
                }
            );
            runtime.setVersionPolicy({
                .major = 0, .minor = 56, .minimumPatch = 1,
                .maximumPatch = 9,
            });
            const auto connected = runtime.connectedDisplays();
            QVERIFY(!connected.success);
            QCOMPARE(connected.errorCode, QStringLiteral("RuntimeUnavailable"));
        }
        QVERIFY(peer.commands().isEmpty());
    }

    void managedReloadProvesOrderedNonceBoundary()
    {
        FakePeer peer;
        FakeOptions options;
        options.stale = true;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime();
        const auto session = prepareOrReport(
            *runtime, RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!session.token.isEmpty());
        QCOMPARE(session.baselineConfigErrors, QByteArray("[]"));
        QCOMPARE(session.baselineProvider, QStringLiteral("lua"));

        const auto proof = runtime->reloadAndConfirm(
            session, QString::fromLatin1(nonce), QByteArrayLiteral("[]"),
            QStringLiteral("lua")
        );
        QVERIFY2(proof.success, qPrintable(proof.errorMessage));
        runtime->cancel(session);
        peer.stop();
        QCOMPARE(peer.commands(), QStringList({
            QStringLiteral("j/version"), QStringLiteral("reload"),
            QStringLiteral("j/configerrors"), QStringLiteral("j/status"),
            QStringLiteral("reload"), QStringLiteral("j/configerrors"),
            QStringLiteral("j/status"),
        }));
    }

    void adoptionAndLegacyRollbackSelectFullReset()
    {
        {
            FakePeer peer;
            FakeOptions options;
            options.baselineProvider = "hyprlang";
            QVERIFY2(peer.start(options), qPrintable(peer.error));
            auto runtime = peer.runtime();
            const auto session = prepareOrReport(
                *runtime, RuntimeActivationMode::AdoptionFullReset
            );
            QVERIFY(!session.token.isEmpty());
            const auto proof = runtime->reloadAndConfirm(
                session, QString::fromLatin1(nonce), QByteArrayLiteral("[]"),
                QStringLiteral("lua")
            );
            QVERIFY2(proof.success, qPrintable(proof.errorMessage));
            runtime->cancel(session);
            peer.stop();
            QCOMPARE(peer.commands().value(4), QStringLiteral("reload full-reset"));
        }
        {
            FakePeer peer;
            FakeOptions options;
            options.baselineErrors = R"(["broken candidate"])";
            options.finalProvider = "hyprlang";
            options.finalEvents = "legacy";
            QVERIFY2(peer.start(options), qPrintable(peer.error));
            auto runtime = peer.runtime();
            const auto session = prepareOrReport(
                *runtime, RuntimeActivationMode::LegacyRollback
            );
            QVERIFY(!session.token.isEmpty());
            const auto proof = runtime->reloadAndConfirm(
                session, QStringView(), QByteArrayLiteral("[]"),
                QStringLiteral("hyprlang")
            );
            QVERIFY2(proof.success, qPrintable(proof.errorMessage));
            runtime->cancel(session);
            peer.stop();
            QCOMPARE(peer.commands().value(4), QStringLiteral("reload full-reset"));
        }
    }

    void staleOrMisorderedNonceCannotSatisfyProof_data()
    {
        QTest::addColumn<QByteArray>("events");
        QTest::addColumn<bool>("stale");
        QTest::newRow("stale-target-drained") << QByteArray("generic-only") << true;
        QTest::newRow("generic-before-target")
            << QByteArray("generic-before-nonce") << false;
        QTest::newRow("wrong-target") << QByteArray("wrong-nonce") << false;
    }

    void staleOrMisorderedNonceCannotSatisfyProof()
    {
        QFETCH(QByteArray, events);
        QFETCH(bool, stale);
        FakePeer peer;
        FakeOptions options;
        options.finalEvents = events;
        options.stale = stale;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(1000);
        const auto session = prepareOrReport(
            *runtime, RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!session.token.isEmpty());
        const auto proof = runtime->reloadAndConfirm(
            session, QString::fromLatin1(nonce), QByteArrayLiteral("[]"),
            QStringLiteral("lua")
        );
        QVERIFY(!proof.success);
        QCOMPARE(proof.errorCode, QStringLiteral("ReloadFailed"));
        runtime->cancel(session);
    }

    void strictVersionStatusAndErrorGates_data()
    {
        QTest::addColumn<QByteArray>("version");
        QTest::addColumn<QByteArray>("errors");
        QTest::addColumn<QByteArray>("provider");
        QTest::addColumn<QString>("errorCode");
        QTest::newRow("duplicate-version-key")
            << QByteArray(R"({"version":"0.56.1","version":"0.56.1"})")
            << QByteArray("[]") << QByteArray("lua")
            << QStringLiteral("UnsupportedVersion");
        QTest::newRow("newer-unreviewed-minor")
            << QByteArray(R"({"version":"0.57.0"})")
            << QByteArray("[]") << QByteArray("lua")
            << QStringLiteral("UnsupportedVersion");
        QTest::newRow("overflowing-patch")
            << QByteArray(R"({"version":"0.56.999999999999999999999"})")
            << QByteArray("[]") << QByteArray("lua")
            << QStringLiteral("UnsupportedVersion");
        QTest::newRow("non-string-configerror")
            << QByteArray(R"({"version":"0.56.1"})")
            << QByteArray("[1]") << QByteArray("lua")
            << QStringLiteral("VerificationFailed");
        QTest::newRow("nonempty-managed-baseline")
            << QByteArray(R"({"version":"0.56.1"})")
            << QByteArray(R"(["existing error"])") << QByteArray("lua")
            << QStringLiteral("VerificationFailed");
        QTest::newRow("duplicate-provider-key")
            << QByteArray(R"({"version":"0.56.1"})")
            << QByteArray("[]")
            << QByteArray(R"({"configProvider":"lua","configProvider":"lua"})")
            << QStringLiteral("VerificationFailed");
    }

    void strictVersionStatusAndErrorGates()
    {
        QFETCH(QByteArray, version);
        QFETCH(QByteArray, errors);
        QFETCH(QByteArray, provider);
        QFETCH(QString, errorCode);
        FakePeer peer;
        FakeOptions options;
        options.version = version;
        options.baselineErrors = errors;
        options.baselineProvider = provider;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(1200);
        const auto prepared = runtime->prepare(
            ActivationRequirement::Reload,
            RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!prepared.success);
        QCOMPARE(prepared.errorCode, errorCode);
    }

    void finalProofRejectsErrorsProviderReplyAndFrames_data()
    {
        QTest::addColumn<QByteArray>("errors");
        QTest::addColumn<QByteArray>("provider");
        QTest::addColumn<QByteArray>("events");
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<QString>("errorCode");
        QTest::addColumn<int>("timeoutMilliseconds");

        QTest::newRow("nonempty-candidate-errors")
            << QByteArray(R"(["candidate failed"])") << QByteArray("lua")
            << QByteArray("proper") << QByteArray("ok")
            << QStringLiteral("VerificationFailed") << 1200;
        QTest::newRow("malformed-candidate-errors")
            << QByteArray("[1]") << QByteArray("lua")
            << QByteArray("proper") << QByteArray("ok")
            << QStringLiteral("VerificationFailed") << 1200;
        QTest::newRow("wrong-final-provider")
            << QByteArray("[]") << QByteArray("hyprlang")
            << QByteArray("proper") << QByteArray("ok")
            << QStringLiteral("VerificationFailed") << 1200;
        QTest::newRow("malformed-final-provider")
            << QByteArray("[]")
            << QByteArray(R"({"configProvider":"lua","configProvider":"lua"})")
            << QByteArray("proper") << QByteArray("ok")
            << QStringLiteral("VerificationFailed") << 1200;
        QTest::newRow("nonexact-final-reload-reply")
            << QByteArray("[]") << QByteArray("lua")
            << QByteArray("proper") << QByteArray("ok\n")
            << QStringLiteral("ReloadFailed") << 1200;
        QTest::newRow("oversized-event-frame")
            << QByteArray("[]") << QByteArray("lua")
            << QByteArray("oversized") << QByteArray("ok")
            << QStringLiteral("ReloadFailed") << 1200;
        QTest::newRow("partial-event-frame")
            << QByteArray("[]") << QByteArray("lua")
            << QByteArray("partial") << QByteArray("ok")
            << QStringLiteral("ReloadFailed") << 700;
        QTest::newRow("event-stream-eof")
            << QByteArray("[]") << QByteArray("lua")
            << QByteArray("eof") << QByteArray("ok")
            << QStringLiteral("ReloadFailed") << 1200;
    }

    void finalProofRejectsErrorsProviderReplyAndFrames()
    {
        QFETCH(QByteArray, errors);
        QFETCH(QByteArray, provider);
        QFETCH(QByteArray, events);
        QFETCH(QByteArray, reply);
        QFETCH(QString, errorCode);
        QFETCH(int, timeoutMilliseconds);

        FakePeer peer;
        FakeOptions options;
        options.finalErrors = errors;
        options.finalProvider = provider;
        options.finalEvents = events;
        options.reloadReply = reply;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(timeoutMilliseconds);
        const auto session = prepareOrReport(
            *runtime, RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!session.token.isEmpty());

        const auto proof = runtime->reloadAndConfirm(
            session, QString::fromLatin1(nonce), QByteArrayLiteral("[]"),
            QStringLiteral("lua")
        );
        QVERIFY(!proof.success);
        QCOMPARE(proof.errorCode, errorCode);
        runtime->cancel(session);
    }

    void perPrepareSignatureRefreshIsAuthoritativeAndSessionPinned()
    {
        for (const auto &startup : {QString(), QStringLiteral("stale-instance")}) {
            FakePeer peer;
            QVERIFY2(peer.start(), qPrintable(peer.error));
            int calls = 0;
            HyprlandIpcRuntime runtime(
                peer.runtimeRoot, startup, peer.stableEntrypoint, 2500,
                [&calls](const int timeout) {
                    ++calls;
                    return HyprlandIpcRuntime::InstanceSignatureResult{
                        .success = timeout > 0,
                        .signature = calls == 1
                            ? QString::fromLatin1(signature)
                            : QStringLiteral("changed-after-prepare"),
                    };
                }
            );
            runtime.setVersionPolicy({
                .major = 0, .minor = 56, .minimumPatch = 1,
                .maximumPatch = 9,
            });
            const auto session = prepareOrReport(
                runtime, RuntimeActivationMode::ManagedReload
            );
            QVERIFY(!session.token.isEmpty());
            const auto proof = runtime.reloadAndConfirm(
                session, QString::fromLatin1(nonce), QByteArrayLiteral("[]"),
                QStringLiteral("lua")
            );
            QVERIFY2(proof.success, qPrintable(proof.errorMessage));
            QCOMPARE(calls, 1);
            runtime.cancel(session);
        }

        for (const auto &provided : {
                 QString(), QStringLiteral("first\nsecond"),
                 QStringLiteral("../instance")}) {
            FakePeer peer;
            QVERIFY2(peer.start(), qPrintable(peer.error));
            HyprlandIpcRuntime runtime(
                peer.runtimeRoot, QString::fromLatin1(signature),
                peer.stableEntrypoint, 1000,
                [provided](const int) {
                    return HyprlandIpcRuntime::InstanceSignatureResult{
                        .success = true,
                        .signature = provided,
                    };
                }
            );
            runtime.setVersionPolicy({
                .major = 0, .minor = 56, .minimumPatch = 1,
                .maximumPatch = 9,
            });
            const auto prepared = runtime.prepare(
                ActivationRequirement::Reload,
                RuntimeActivationMode::ManagedReload
            );
            QVERIFY(!prepared.success);
            QCOMPARE(prepared.errorCode, QStringLiteral("RuntimeUnavailable"));
        }
    }

    void processAndLockBindingFailClosed_data()
    {
        QTest::addColumn<QByteArray>("lock");
        QTest::addColumn<bool>("explicitConfig");
        QTest::addColumn<bool>("wrongStablePath");
        QTest::addColumn<bool>("safeMode");
        QTest::newRow("malformed-two-line-lock")
            << QByteArray("malformed") << false << false << false;
        QTest::newRow("lock-peer-pid-mismatch")
            << QByteArray("wrong-pid") << false << false << false;
        QTest::newRow("HYPRLAND_CONFIG")
            << QByteArray("exact") << true << false << false;
        QTest::newRow("different-default-entrypoint")
            << QByteArray("exact") << false << true << false;
        QTest::newRow("safe-mode")
            << QByteArray("exact") << false << false << true;
    }

    void processAndLockBindingFailClosed()
    {
        QFETCH(QByteArray, lock);
        QFETCH(bool, explicitConfig);
        QFETCH(bool, wrongStablePath);
        QFETCH(bool, safeMode);
        FakePeer peer;
        FakeOptions options;
        options.lock = lock;
        options.explicitEnvironmentConfig = explicitConfig;
        options.safeMode = safeMode;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        const auto alternate = wrongStablePath
            ? QDir(peer.configRoot).filePath(QStringLiteral("other/hyprland.lua"))
            : QString();
        auto runtime = peer.runtime(1000, alternate);
        const auto prepared = runtime->prepare(
            ActivationRequirement::Reload,
            RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!prepared.success);
        QVERIFY(prepared.errorCode == QStringLiteral("VerificationFailed")
                || prepared.errorCode == QStringLiteral("ReloadFailed"));
    }

    void socketPathReplacementFailsClosed_data()
    {
        QTest::addColumn<QByteArray>("endpoint");
        QTest::newRow("control") << QByteArray("control");
        QTest::newRow("event") << QByteArray("event");
    }

    void socketPathReplacementFailsClosed()
    {
        QFETCH(QByteArray, endpoint);
        FakePeer peer;
        FakeOptions options;
        options.replaceEndpoint = endpoint;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(1200);
        const auto prepared = runtime->prepare(
            ActivationRequirement::Reload,
            RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!prepared.success);
        QCOMPARE(prepared.errorCode, QStringLiteral("ReloadFailed"));
    }

    void transactionUsesOneAbsoluteDeadline()
    {
        FakePeer peer;
        FakeOptions options;
        options.delayMilliseconds = 80;
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(230);
        QElapsedTimer elapsed;
        elapsed.start();
        const auto prepared = runtime->prepare(
            ActivationRequirement::Reload,
            RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!prepared.success);
        QVERIFY(elapsed.elapsed() < 900);
    }

    void cancellationInterruptsBlockedEventProof()
    {
        FakePeer peer;
        FakeOptions options;
        options.finalEvents = "none";
        QVERIFY2(peer.start(options), qPrintable(peer.error));
        auto runtime = peer.runtime(5000);
        const auto session = prepareOrReport(
            *runtime, RuntimeActivationMode::ManagedReload
        );
        QVERIFY(!session.token.isEmpty());
        auto future = std::async(std::launch::async, [&] {
            return runtime->reloadAndConfirm(
                session, QString::fromLatin1(nonce), QByteArrayLiteral("[]"),
                QStringLiteral("lua")
            );
        });
        QTRY_VERIFY_WITH_TIMEOUT(
            peer.commands().count(QStringLiteral("reload")) >= 2, 1500
        );
        QElapsedTimer elapsed;
        elapsed.start();
        runtime->cancel(session);
        QVERIFY(future.wait_for(1200ms) == std::future_status::ready);
        QVERIFY(!future.get().success);
        QVERIFY(elapsed.elapsed() < 1200);
    }
};

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsSet("HYPRSHELLD_FAKE_HYPRLAND_CHILD")) {
        return runFakeHyprland();
    }
    QCoreApplication application(argc, argv);
    HyprlandIpcRuntimeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "hyprland_ipc_runtime_test.moc"
