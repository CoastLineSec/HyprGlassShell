#include "component_inspector_launcher.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QVariant>

#include <utility>

namespace HyprShelld::SystemdInspectorTypes {

struct Property final {
    QString name;
    QVariant value;
};

using Properties = QList<Property>;

struct AuxiliaryUnit final {
    QString name;
    Properties properties;
};

using AuxiliaryUnits = QList<AuxiliaryUnit>;

struct ExecCommand final {
    QString path;
    QStringList arguments;
    bool ignoreFailure = false;
};

using ExecCommands = QList<ExecCommand>;

struct OpenFile final {
    QString path;
    QString descriptorName;
    quint64 flags = 0;
};

using OpenFiles = QList<OpenFile>;

struct BindPath final {
    QString source;
    QString destination;
    bool ignoreMissing = false;
    quint64 recursiveFlags = 0;
};

using BindPaths = QList<BindPath>;

struct StringListFilter final {
    bool allowList = true;
    QStringList values;
};

QDBusArgument &operator<<(QDBusArgument &argument, const Property &property)
{
    argument.beginStructure();
    argument << property.name << QDBusVariant(property.value);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    Property &property
)
{
    QDBusVariant value;
    argument.beginStructure();
    argument >> property.name >> value;
    argument.endStructure();
    property.value = value.variant();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const BindPath &path)
{
    argument.beginStructure();
    argument << path.source << path.destination << path.ignoreMissing
             << path.recursiveFlags;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    BindPath &path
)
{
    argument.beginStructure();
    argument >> path.source >> path.destination >> path.ignoreMissing
             >> path.recursiveFlags;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const AuxiliaryUnit &unit)
{
    argument.beginStructure();
    argument << unit.name << unit.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    AuxiliaryUnit &unit
)
{
    argument.beginStructure();
    argument >> unit.name >> unit.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ExecCommand &command)
{
    argument.beginStructure();
    argument << command.path << command.arguments << command.ignoreFailure;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    ExecCommand &command
)
{
    argument.beginStructure();
    argument >> command.path >> command.arguments >> command.ignoreFailure;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const OpenFile &file)
{
    argument.beginStructure();
    argument << file.path << file.descriptorName << file.flags;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    OpenFile &file
)
{
    argument.beginStructure();
    argument >> file.path >> file.descriptorName >> file.flags;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(
    QDBusArgument &argument,
    const StringListFilter &filter
)
{
    argument.beginStructure();
    argument << filter.allowList << filter.values;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    StringListFilter &filter
)
{
    argument.beginStructure();
    argument >> filter.allowList >> filter.values;
    argument.endStructure();
    return argument;
}

} // namespace HyprShelld::SystemdInspectorTypes

Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::Property)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::Properties)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::AuxiliaryUnit)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::AuxiliaryUnits)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::ExecCommand)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::ExecCommands)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::OpenFile)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::OpenFiles)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::BindPath)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::BindPaths)
Q_DECLARE_METATYPE(HyprShelld::SystemdInspectorTypes::StringListFilter)

namespace HyprShelld {
namespace {

using namespace SystemdInspectorTypes;

constexpr auto systemdService = "org.freedesktop.systemd1";
constexpr auto systemdManagerPath = "/org/freedesktop/systemd1";
constexpr auto systemdManagerInterface = "org.freedesktop.systemd1.Manager";
constexpr quint64 usecPerSecond = 1000ULL * 1000ULL;
constexpr quint64 mebibyte = 1024ULL * 1024ULL;

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

const QRegularExpression &slicePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9_.@:-]+\\.slice$")
    );
    return pattern;
}

void registerSystemdTypes()
{
    static const auto registered = [] {
        qDBusRegisterMetaType<Property>();
        qDBusRegisterMetaType<Properties>();
        qDBusRegisterMetaType<AuxiliaryUnit>();
        qDBusRegisterMetaType<AuxiliaryUnits>();
        qDBusRegisterMetaType<ExecCommand>();
        qDBusRegisterMetaType<ExecCommands>();
        qDBusRegisterMetaType<OpenFile>();
        qDBusRegisterMetaType<OpenFiles>();
        qDBusRegisterMetaType<BindPath>();
        qDBusRegisterMetaType<BindPaths>();
        qDBusRegisterMetaType<StringListFilter>();
        return true;
    }();
    Q_UNUSED(registered)
}

Property property(QString name, QVariant value)
{
    return {.name = std::move(name), .value = std::move(value)};
}

Properties hardenedProperties(
    const ComponentInspectorLaunchRequest &request,
    const QString &executable,
    const QString &sliceName
)
{
    // These flag values are systemd's OpenFile= D-Bus contract: read-only and
    // truncate. systemd opens all three paths before entering the sandbox and
    // passes them as sd-listen descriptors named below.
    constexpr quint64 openReadOnly = 1ULL << 0;
    constexpr quint64 openTruncate = 1ULL << 2;

    const ExecCommands commands{{
        .path = executable,
        .arguments = {
            executable,
            QStringLiteral("--inspect"),
            QStringLiteral("--expected-token=%1").arg(request.token),
            QStringLiteral("--expected-archive-digest=%1")
                .arg(request.archiveDigest),
        },
        .ignoreFailure = false,
    }};
    const OpenFiles openFiles{
        {
            .path = request.spoolPath,
            .descriptorName = QStringLiteral("package"),
            .flags = openReadOnly,
        },
        {
            .path = request.reportPath,
            .descriptorName = QStringLiteral("result"),
            .flags = openTruncate,
        },
        {
            .path = request.materializedPath,
            .descriptorName = QStringLiteral("materialized"),
            .flags = openTruncate,
        },
    };

    return {
        property(
            QStringLiteral("Description"),
            QStringLiteral("HyprShelld local component package inspection")
        ),
        property(QStringLiteral("Slice"), sliceName),
        property(QStringLiteral("Type"), QStringLiteral("oneshot")),
        property(QStringLiteral("ExecStart"), QVariant::fromValue(commands)),
        property(QStringLiteral("OpenFile"), QVariant::fromValue(openFiles)),
        property(
            QStringLiteral("CollectMode"),
            QStringLiteral("inactive-or-failed")
        ),
        property(QStringLiteral("TimeoutStartUSec"), 10ULL * usecPerSecond),
        property(QStringLiteral("TimeoutStopUSec"), 2ULL * usecPerSecond),
        property(QStringLiteral("KillMode"), QStringLiteral("mixed")),
        property(QStringLiteral("UMask"), uint(0077)),
        property(QStringLiteral("LimitCORE"), quint64(0)),
        property(QStringLiteral("LimitNOFILE"), quint64(32)),
        property(QStringLiteral("TasksMax"), quint64(4)),
        property(QStringLiteral("MemoryMax"), 256ULL * mebibyte),
        property(QStringLiteral("MemorySwapMax"), quint64(0)),
        property(QStringLiteral("CPUQuotaPerSecUSec"), usecPerSecond / 2),
        property(QStringLiteral("Nice"), 10),
        property(QStringLiteral("NoNewPrivileges"), true),
        property(QStringLiteral("PrivateUsers"), true),
        property(QStringLiteral("PrivateTmp"), true),
        property(QStringLiteral("PrivateDevices"), true),
        property(QStringLiteral("PrivateNetwork"), true),
        property(QStringLiteral("PrivateIPC"), true),
        property(QStringLiteral("PrivatePIDs"), QStringLiteral("yes")),
        property(QStringLiteral("KeyringMode"), QStringLiteral("private")),
        property(QStringLiteral("ProtectSystem"), QStringLiteral("strict")),
        property(QStringLiteral("ProtectHome"), QStringLiteral("yes")),
        property(
            QStringLiteral("BindReadOnlyPaths"),
            QVariant::fromValue(BindPaths{{
                .source = executable,
                .destination = executable,
                .ignoreMissing = false,
                .recursiveFlags = 0,
            }})
        ),
        property(QStringLiteral("ProtectProc"), QStringLiteral("invisible")),
        property(QStringLiteral("ProcSubset"), QStringLiteral("pid")),
        property(QStringLiteral("ProtectClock"), true),
        property(QStringLiteral("ProtectControlGroups"), true),
        property(QStringLiteral("ProtectHostname"), true),
        property(QStringLiteral("ProtectKernelLogs"), true),
        property(QStringLiteral("ProtectKernelModules"), true),
        property(QStringLiteral("ProtectKernelTunables"), true),
        property(QStringLiteral("RestrictSUIDSGID"), true),
        property(QStringLiteral("RestrictRealtime"), true),
        property(QStringLiteral("RestrictNamespaces"), quint64(0)),
        property(QStringLiteral("LockPersonality"), true),
        property(QStringLiteral("MemoryDenyWriteExecute"), true),
        property(QStringLiteral("CapabilityBoundingSet"), quint64(0)),
        property(
            QStringLiteral("RestrictAddressFamilies"),
            QVariant::fromValue(StringListFilter{
                .allowList = true,
                .values = {},
            })
        ),
        property(
            QStringLiteral("SystemCallArchitectures"),
            QStringList{QStringLiteral("native")}
        ),
        property(
            QStringLiteral("SystemCallFilter"),
            QVariant::fromValue(StringListFilter{
                .allowList = true,
                .values = {QStringLiteral("@system-service")},
            })
        ),
        property(QStringLiteral("SystemCallErrorNumber"), int(1)),
        property(
            QStringLiteral("InaccessiblePaths"),
            QStringList{QStringLiteral("/run/user")}
        ),
        property(QStringLiteral("StandardInput"), QStringLiteral("null")),
        property(QStringLiteral("StandardOutput"), QStringLiteral("null")),
        property(QStringLiteral("StandardError"), QStringLiteral("null")),
        property(QStringLiteral("WorkingDirectory"), QStringLiteral("/")),
        property(
            QStringLiteral("Environment"),
            QStringList{
                QStringLiteral("LC_ALL=C.UTF-8"),
                QStringLiteral("HOME=/nonexistent"),
                QStringLiteral("PATH=/usr/bin:/bin"),
                QStringLiteral("XDG_CACHE_HOME=/tmp"),
                QStringLiteral("XDG_CONFIG_HOME=/nonexistent"),
                QStringLiteral("XDG_DATA_HOME=/nonexistent"),
                QStringLiteral("XDG_RUNTIME_DIR=/nonexistent"),
            }
        ),
        property(
            QStringLiteral("UnsetEnvironment"),
            QStringList{
                QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                QStringLiteral("DISPLAY"),
                QStringLiteral("WAYLAND_DISPLAY"),
                QStringLiteral("SSH_AUTH_SOCK"),
                QStringLiteral("XAUTHORITY"),
                QStringLiteral("QT_PLUGIN_PATH"),
                QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"),
                QStringLiteral("QT_IMAGEIO_PLUGIN_PATH"),
                QStringLiteral("QT_DEBUG_PLUGINS"),
                QStringLiteral("QML2_IMPORT_PATH"),
                QStringLiteral("QML_IMPORT_PATH"),
                QStringLiteral("LD_AUDIT"),
                QStringLiteral("LD_DEBUG_OUTPUT"),
                QStringLiteral("LD_LIBRARY_PATH"),
                QStringLiteral("LD_PRELOAD"),
                QStringLiteral("GCONV_PATH"),
                QStringLiteral("LOCPATH"),
                QStringLiteral("NLSPATH"),
                QStringLiteral("GLIBC_TUNABLES"),
                QStringLiteral("MALLOC_TRACE"),
                QStringLiteral("NODE_OPTIONS"),
                QStringLiteral("PERL5LIB"),
                QStringLiteral("PYTHONHOME"),
                QStringLiteral("PYTHONPATH"),
                QStringLiteral("RUBYLIB"),
            }
        ),
    };
}

bool isAbsolutePath(const QString &path)
{
    return !path.isEmpty() && QFileInfo(path).isAbsolute()
        && !path.contains(QChar::Null);
}

ComponentInspectorLaunchResult launchError(
    QString name,
    QString message
)
{
    if (name.isEmpty()) {
        name = QStringLiteral(
            "org.hyprshelld.ComponentInspector.Error.LaunchFailed"
        );
    }
    if (message.isEmpty()) {
        message = QStringLiteral("The systemd inspector launch failed");
    }
    return {
        .success = false,
        .errorName = std::move(name),
        .errorMessage = std::move(message),
    };
}

} // namespace

struct SystemdComponentInspectorLauncher::Private final {
    enum class SubscriptionState {
        NotStarted,
        Pending,
        Ready,
        Failed,
    };

    struct Pending final {
        ComponentInspectorLaunchRequest request;
        Completion completion;
        QString unitName;
        QString jobPath;
        bool startSubmitted = false;
    };

    QDBusConnection connection;
    QString executable;
    QString sliceName;
    bool signalConnected = false;
    SubscriptionState subscription = SubscriptionState::NotStarted;
    QHash<QString, Pending> pendingByToken;
    QHash<QString, QString> tokenByJobPath;
};

SystemdComponentInspectorLauncher::SystemdComponentInspectorLauncher(
    QDBusConnection connection,
    QString inspectorExecutable,
    QString sliceName,
    QObject *parent
)
    : ComponentInspectorLauncher(parent)
    , d_(std::make_unique<Private>(Private{
          .connection = std::move(connection),
          .executable = std::move(inspectorExecutable),
          .sliceName = std::move(sliceName),
          .signalConnected = false,
          .subscription = Private::SubscriptionState::NotStarted,
          .pendingByToken = {},
          .tokenByJobPath = {},
      }))
{
    registerSystemdTypes();
    d_->signalConnected = d_->connection.connect(
        QString::fromLatin1(systemdService),
        QString::fromLatin1(systemdManagerPath),
        QString::fromLatin1(systemdManagerInterface),
        QStringLiteral("JobRemoved"),
        this,
        SLOT(jobRemoved(quint32,QDBusObjectPath,QString,QString))
    );
}

SystemdComponentInspectorLauncher::~SystemdComponentInspectorLauncher() =
    default;

QString SystemdComponentInspectorLauncher::unitNameForToken(
    const QString &token
)
{
    if (!tokenPattern().match(token).hasMatch()) {
        return {};
    }
    return QStringLiteral("hyprshelld-component-inspect-%1.service")
        .arg(token);
}

ComponentInspectorSystemdContract
SystemdComponentInspectorLauncher::sandboxContractForTesting(
    const ComponentInspectorLaunchRequest &request,
    const QString &inspectorExecutable,
    const QString &sliceName
)
{
    registerSystemdTypes();
    ComponentInspectorSystemdContract contract;
    contract.startTransientArgumentSignature = QByteArrayLiteral("ss")
        + QDBusMetaType::typeToSignature(
            QMetaType::fromType<SystemdInspectorTypes::Properties>()
        )
        + QDBusMetaType::typeToSignature(
            QMetaType::fromType<SystemdInspectorTypes::AuxiliaryUnits>()
        );
    const auto properties = hardenedProperties(
        request,
        inspectorExecutable,
        sliceName
    );
    for (const auto &entry : properties) {
        const auto *signature = QDBusMetaType::typeToSignature(
            entry.value.metaType()
        );
        contract.propertySignatures.insert(
            entry.name,
            signature == nullptr ? QByteArray() : QByteArray(signature)
        );
        contract.propertyValues.insert(entry.name, entry.value);
        if (entry.name == QStringLiteral("ExecStart")) {
            const auto commands = entry.value.value<
                SystemdInspectorTypes::ExecCommands
            >();
            if (!commands.isEmpty()) {
                contract.arguments = commands.first().arguments;
            }
        } else if (entry.name == QStringLiteral("OpenFile")) {
            const auto files = entry.value.value<
                SystemdInspectorTypes::OpenFiles
            >();
            for (const auto &file : files) {
                contract.openFilePaths.insert(
                    file.descriptorName,
                    file.path
                );
                contract.openFileFlags.insert(
                    file.descriptorName,
                    file.flags
                );
            }
        } else if (entry.name == QStringLiteral("BindReadOnlyPaths")) {
            const auto paths = entry.value.value<
                SystemdInspectorTypes::BindPaths
            >();
            for (const auto &path : paths) {
                contract.readOnlyBindPaths.insert(
                    path.destination,
                    path.source
                );
            }
        }
    }
    return contract;
}

bool SystemdComponentInspectorLauncher::start(
    const ComponentInspectorLaunchRequest &request,
    Completion completion,
    QString &error
)
{
    error.clear();
    if (!d_->connection.isConnected() || !d_->signalConnected) {
        error = QStringLiteral("The systemd user manager bus is unavailable");
        return false;
    }
    if (!isAbsolutePath(d_->executable)
        || !slicePattern().match(d_->sliceName).hasMatch()
        || !tokenPattern().match(request.token).hasMatch()
        || !digestPattern().match(request.archiveDigest).hasMatch()
        || !isAbsolutePath(request.spoolPath)
        || !isAbsolutePath(request.reportPath)
        || !isAbsolutePath(request.materializedPath)
        || request.spoolPath == request.reportPath
        || request.spoolPath == request.materializedPath
        || request.reportPath == request.materializedPath
        || !completion || d_->pendingByToken.contains(request.token)) {
        error = QStringLiteral("The inspector launch request is invalid");
        return false;
    }
    if (d_->subscription == Private::SubscriptionState::Failed) {
        error = QStringLiteral(
            "The systemd user manager signal subscription failed"
        );
        return false;
    }

    d_->pendingByToken.insert(request.token, {
        .request = request,
        .completion = std::move(completion),
        .unitName = unitNameForToken(request.token),
        .jobPath = {},
        .startSubmitted = false,
    });

    if (d_->subscription == Private::SubscriptionState::Ready) {
        submitStart(request.token);
    } else {
        ensureSubscribed();
    }
    return true;
}

void SystemdComponentInspectorLauncher::complete(
    const QString &token,
    ComponentInspectorLaunchResult result
)
{
    const auto found = d_->pendingByToken.find(token);
    if (found == d_->pendingByToken.end()) {
        return;
    }
    auto completion = std::move(found->completion);
    if (!found->jobPath.isEmpty()) {
        d_->tokenByJobPath.remove(found->jobPath);
    }
    d_->pendingByToken.erase(found);
    if (completion) {
        completion(std::move(result));
    }
}

void SystemdComponentInspectorLauncher::submitStart(const QString &token)
{
    auto found = d_->pendingByToken.find(token);
    if (found == d_->pendingByToken.end() || found->startSubmitted) {
        return;
    }
    found->startSubmitted = true;

    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(systemdService),
        QString::fromLatin1(systemdManagerPath),
        QString::fromLatin1(systemdManagerInterface),
        QStringLiteral("StartTransientUnit")
    );
    message.setArguments({
        found->unitName,
        QStringLiteral("fail"),
        QVariant::fromValue(hardenedProperties(
            found->request,
            d_->executable,
            d_->sliceName
        )),
        QVariant::fromValue(SystemdInspectorTypes::AuxiliaryUnits{}),
    });

    auto *watcher = new QDBusPendingCallWatcher(
        d_->connection.asyncCall(message, 15000),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, token] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            auto current = d_->pendingByToken.find(token);
            if (current == d_->pendingByToken.end()) {
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() != QDBusMessage::ReplyMessage
                || arguments.size() != 1
                || !arguments.first().canConvert<QDBusObjectPath>()) {
                complete(
                    token,
                    launchError(
                        reply.errorName(),
                        reply.errorMessage().isEmpty()
                            ? QStringLiteral(
                                "StartTransientUnit returned an invalid reply"
                            )
                            : reply.errorMessage()
                    )
                );
                return;
            }
            const auto jobPath = arguments.first()
                                     .value<QDBusObjectPath>()
                                     .path();
            if (jobPath.isEmpty()) {
                complete(
                    token,
                    launchError(
                        {},
                        QStringLiteral(
                            "StartTransientUnit returned an empty job path"
                        )
                    )
                );
                return;
            }
            current->jobPath = jobPath;
            d_->tokenByJobPath.insert(jobPath, token);
        }
    );
}

void SystemdComponentInspectorLauncher::ensureSubscribed()
{
    if (d_->subscription != Private::SubscriptionState::NotStarted) {
        return;
    }
    d_->subscription = Private::SubscriptionState::Pending;

    const auto subscribe = QDBusMessage::createMethodCall(
        QString::fromLatin1(systemdService),
        QString::fromLatin1(systemdManagerPath),
        QString::fromLatin1(systemdManagerInterface),
        QStringLiteral("Subscribe")
    );
    auto *watcher = new QDBusPendingCallWatcher(
        d_->connection.asyncCall(subscribe, 5000),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (reply.type() != QDBusMessage::ReplyMessage
                || !reply.arguments().isEmpty()) {
                d_->subscription = Private::SubscriptionState::Failed;
                const auto tokens = d_->pendingByToken.keys();
                for (const auto &token : tokens) {
                    complete(
                        token,
                        launchError(
                            reply.errorName(),
                            reply.errorMessage().isEmpty()
                                ? QStringLiteral(
                                    "The systemd subscription reply was invalid"
                                )
                                : reply.errorMessage()
                        )
                    );
                }
                return;
            }

            d_->subscription = Private::SubscriptionState::Ready;
            const auto tokens = d_->pendingByToken.keys();
            for (const auto &token : tokens) {
                submitStart(token);
            }
        }
    );
}

void SystemdComponentInspectorLauncher::cancel(const QString &token)
{
    const auto found = d_->pendingByToken.find(token);
    if (found == d_->pendingByToken.end()) {
        return;
    }
    const auto unitName = found->unitName;
    const auto startSubmitted = found->startSubmitted;
    if (!found->jobPath.isEmpty()) {
        d_->tokenByJobPath.remove(found->jobPath);
    }
    d_->pendingByToken.erase(found);

    if (!startSubmitted) {
        return;
    }
    auto stop = QDBusMessage::createMethodCall(
        QString::fromLatin1(systemdService),
        QString::fromLatin1(systemdManagerPath),
        QString::fromLatin1(systemdManagerInterface),
        QStringLiteral("StopUnit")
    );
    stop.setArguments({unitName, QStringLiteral("replace")});
    d_->connection.asyncCall(stop, 5000);
}

void SystemdComponentInspectorLauncher::jobRemoved(
    const quint32 jobId,
    const QDBusObjectPath &jobPath,
    const QString &unitName,
    const QString &result
)
{
    Q_UNUSED(jobId)

    auto token = d_->tokenByJobPath.take(jobPath.path());
    if (token.isEmpty()) {
        // A very short oneshot can finish before the StartTransientUnit reply
        // reaches us. Unit identity is fixed by the random token, so it is a
        // safe fallback for closing that signal/reply race.
        for (auto iterator = d_->pendingByToken.cbegin();
             iterator != d_->pendingByToken.cend(); ++iterator) {
            if (iterator->unitName == unitName) {
                token = iterator.key();
                break;
            }
        }
    }
    if (token.isEmpty()) {
        return;
    }

    const auto success = result == QStringLiteral("done");
    complete(token, {
        .success = success,
        .errorName = success
            ? QString()
            : QStringLiteral(
                "org.hyprshelld.ComponentInspector.Error.HelperFailed"
            ),
        .errorMessage = success
            ? QString()
            : QStringLiteral("The inspector unit finished with result %1")
                  .arg(result),
    });
}

} // namespace HyprShelld
