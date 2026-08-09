#include "component_runtime_health_store.h"

#include "component/component_configuration.h"
#include "component/component_contract.h"
#include "component/strict_json.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <array>
#include <utility>

namespace HyprShelld {
namespace {

constexpr auto healthFormatVersion = 1;
constexpr qsizetype maximumHealthBytes = 512 * 1024;
constexpr qsizetype maximumHealthRecords = 512;

enum class ReadStatus {
    Missing,
    Valid,
    Damaged,
    Unsupported,
    Unreadable,
};

struct ReadResult final {
    ReadStatus status = ReadStatus::Missing;
    ComponentRuntimeHealthState state;
    QString error;
};

ReadResult readFailure(const ReadStatus status, QString error)
{
    return {
        .status = status,
        .state = {},
        .error = std::move(error),
    };
}

ReadResult readSuccess(ComponentRuntimeHealthState state)
{
    return {
        .status = ReadStatus::Valid,
        .state = std::move(state),
        .error = {},
    };
}

bool exactKeys(const QJsonObject &object, QStringList expected)
{
    auto actual = object.keys();
    actual.sort();
    expected.sort();
    return actual == expected;
}

bool parseRevision(const QString &text, quint64 &revision)
{
    if (text.isEmpty() || (text.size() > 1 && text.front() == u'0')) {
        return false;
    }
    for (const auto character : text) {
        if (character < u'0' || character > u'9') {
            return false;
        }
    }
    bool ok = false;
    revision = text.toULongLong(&ok, 10);
    return ok;
}

bool isKnownQuarantineReason(const QString &reason)
{
    return reason == QStringLiteral("incomplete-startup")
        || reason == QStringLiteral("timeout")
        || reason == QStringLiteral("render-failed")
        || reason == QStringLiteral("protocol-invalid");
}

bool validatePathAncestry(const QString &path, QString &error)
{
    const auto absolute = QFileInfo(path).absoluteFilePath();
    const auto pieces = absolute.split(
        QLatin1Char('/'),
        Qt::SkipEmptyParts
    );
    QString current = QStringLiteral("/");
    for (const auto &piece : pieces) {
        current = QDir(current).filePath(piece);
        const auto encoded = QFile::encodeName(current);
        struct stat status {};
        if (::lstat(encoded.constData(), &status) != 0) {
            continue;
        }
        if (S_ISLNK(status.st_mode) || !S_ISDIR(status.st_mode)) {
            error = QStringLiteral(
                "Runtime health path has unsafe ancestry: %1"
            ).arg(current);
            return false;
        }
    }
    return true;
}

bool prepareParent(const QString &path, QString &error)
{
    const auto parent = QFileInfo(path).absolutePath();
    if (!validatePathAncestry(QFileInfo(parent).absolutePath(), error)) {
        return false;
    }
    if (!QDir().mkpath(parent) || !validatePathAncestry(parent, error)) {
        if (error.isEmpty()) {
            error = QStringLiteral(
                "Cannot create runtime health directory %1"
            ).arg(parent);
        }
        return false;
    }

    const auto encoded = QFile::encodeName(parent);
    struct stat status {};
    if (::lstat(encoded.constData(), &status) != 0
        || !S_ISDIR(status.st_mode)
        || status.st_uid != ::geteuid()
        || ::chmod(encoded.constData(), 0700) != 0) {
        error = QStringLiteral(
            "Runtime health directory is not private user-owned storage: %1"
        ).arg(parent);
        return false;
    }
    return true;
}

bool privateImmediateParent(const QString &path, QString &error)
{
    const auto parent = QFileInfo(path).absolutePath();
    const auto encoded = QFile::encodeName(parent);
    struct stat status {};
    if (::lstat(encoded.constData(), &status) != 0
        || !S_ISDIR(status.st_mode)
        || status.st_uid != ::geteuid()
        || (status.st_mode & 0077) != 0) {
        error = QStringLiteral(
            "Runtime health directory is not private user-owned storage: %1"
        ).arg(parent);
        return false;
    }
    return true;
}

bool safeRegularFile(const QString &path, QString &error)
{
    const auto encoded = QFile::encodeName(path);
    struct stat status {};
    if (::lstat(encoded.constData(), &status) != 0) {
        return true;
    }
    if (!S_ISREG(status.st_mode) || status.st_uid != ::geteuid()
        || (status.st_mode & 0077) != 0) {
        error = QStringLiteral(
            "Runtime health file is not a regular user-owned file: %1"
        ).arg(path);
        return false;
    }
    return true;
}

QByteArray serialize(const ComponentRuntimeHealthState &state)
{
    QJsonArray records;
    for (const auto &record : state.records) {
        records.append(QJsonObject{
            {QStringLiteral("componentId"), record.componentId},
            {QStringLiteral("packageDigest"), record.packageDigest},
            {QStringLiteral("state"), record.state},
            {QStringLiteral("reason"), record.reason},
            {
                QStringLiteral("failureCount"),
                static_cast<qint64>(record.failureCount)
            },
        });
    }
    QJsonArray pending;
    for (const auto &activation : state.pending) {
        pending.append(QJsonObject{
            {QStringLiteral("instanceId"), activation.instanceId},
            {QStringLiteral("componentId"), activation.componentId},
            {QStringLiteral("packageDigest"), activation.packageDigest},
        });
    }
    auto bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("formatVersion"), healthFormatVersion},
        {QStringLiteral("revision"), QString::number(state.revision)},
        {QStringLiteral("safeMode"), state.safeMode},
        {QStringLiteral("records"), records},
        {QStringLiteral("pending"), pending},
    }).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

ReadResult parseState(const QByteArray &bytes, const QString &path)
{
    const auto parsed = Components::parseStrictJsonObject(
        QByteArrayView(bytes),
        {.maximumBytes = maximumHealthBytes, .maximumDepth = 8}
    );
    if (!parsed) {
        return readFailure(
            ReadStatus::Damaged,
            QStringLiteral("Invalid runtime health JSON in %1").arg(path)
        );
    }
    const auto &root = *parsed.value;
    const auto version = root.value(QStringLiteral("formatVersion"));
    if (!version.isDouble() || version.toDouble() != version.toInteger(-1)) {
        return readFailure(
            ReadStatus::Damaged,
            QStringLiteral("Invalid runtime health version in %1").arg(path)
        );
    }
    if (version.toInteger() > healthFormatVersion) {
        return readFailure(
            ReadStatus::Unsupported,
            QStringLiteral("Unsupported runtime health version in %1").arg(path)
        );
    }
    if (version.toInteger() != healthFormatVersion) {
        return readFailure(
            ReadStatus::Damaged,
            QStringLiteral("Invalid runtime health version in %1").arg(path)
        );
    }
    if (!exactKeys(root, {
            QStringLiteral("formatVersion"),
            QStringLiteral("revision"),
            QStringLiteral("safeMode"),
            QStringLiteral("records"),
            QStringLiteral("pending"),
        })) {
        return readFailure(
            ReadStatus::Damaged,
            QStringLiteral("Invalid runtime health fields in %1").arg(path)
        );
    }

    ComponentRuntimeHealthState state;
    const auto revision = root.value(QStringLiteral("revision"));
    const auto safeMode = root.value(QStringLiteral("safeMode"));
    const auto records = root.value(QStringLiteral("records"));
    const auto pending = root.value(QStringLiteral("pending"));
    if (!revision.isString()
        || !parseRevision(revision.toString(), state.revision)
        || !safeMode.isBool()
        || !records.isArray()
        || records.toArray().size() > maximumHealthRecords
        || !pending.isArray()
        || pending.toArray().size() > maximumHealthRecords) {
        return readFailure(
            ReadStatus::Damaged,
            QStringLiteral("Invalid runtime health values in %1").arg(path)
        );
    }
    state.safeMode = safeMode.toBool();

    for (const auto &value : records.toArray()) {
        if (!value.isObject()) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral("Invalid runtime health record in %1").arg(path)
            );
        }
        const auto object = value.toObject();
        if (!exactKeys(object, {
                QStringLiteral("componentId"),
                QStringLiteral("packageDigest"),
                QStringLiteral("state"),
                QStringLiteral("reason"),
                QStringLiteral("failureCount"),
            })) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral(
                    "Invalid runtime health record fields in %1"
                ).arg(path)
            );
        }
        const auto componentId = object.value(QStringLiteral("componentId"));
        const auto packageDigest = object.value(QStringLiteral("packageDigest"));
        const auto recordState = object.value(QStringLiteral("state"));
        const auto reason = object.value(QStringLiteral("reason"));
        const auto failures = object.value(QStringLiteral("failureCount"));
        const auto failureCount = failures.toInteger(-1);
        const auto validState = recordState.toString()
            == QStringLiteral("quarantined");
        if (!componentId.isString()
            || !Components::isValidComponentId(componentId.toString())
            || !packageDigest.isString()
            || !Components::isFullSha256Digest(packageDigest.toString())
            || !recordState.isString() || !validState
            || !reason.isString()
            || !isKnownQuarantineReason(reason.toString())
            || !failures.isDouble()
            || failures.toDouble() != static_cast<double>(failureCount)
            || failureCount < 1
            || failureCount > 1000000) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral(
                    "Invalid runtime health record value in %1"
                ).arg(path)
            );
        }
        ComponentRuntimeHealthRecord record{
            .componentId = componentId.toString(),
            .packageDigest = packageDigest.toString(),
            .state = recordState.toString(),
            .reason = reason.toString(),
            .failureCount = static_cast<quint32>(failureCount),
        };
        const auto key = ComponentRuntimeHealthStore::recordKey(
            record.componentId,
            record.packageDigest
        );
        if (state.records.contains(key)) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral(
                    "Duplicate runtime health record in %1"
                ).arg(path)
            );
        }
        state.records.insert(key, std::move(record));
    }

    for (const auto &value : pending.toArray()) {
        if (!value.isObject()) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral("Invalid pending activation in %1").arg(path)
            );
        }
        const auto object = value.toObject();
        if (!exactKeys(object, {
                QStringLiteral("instanceId"),
                QStringLiteral("componentId"),
                QStringLiteral("packageDigest"),
            })) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral(
                    "Invalid pending activation fields in %1"
                ).arg(path)
            );
        }
        const auto instanceId = object.value(QStringLiteral("instanceId"));
        const auto componentId = object.value(QStringLiteral("componentId"));
        const auto packageDigest = object.value(QStringLiteral("packageDigest"));
        if (!instanceId.isString()
            || !Components::isLowercaseUuidV4(instanceId.toString())
            || !componentId.isString()
            || !Components::isValidComponentId(componentId.toString())
            || !packageDigest.isString()
            || !Components::isFullSha256Digest(packageDigest.toString())) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral(
                    "Invalid pending activation value in %1"
                ).arg(path)
            );
        }
        ComponentRuntimePendingActivation activation{
            .instanceId = instanceId.toString(),
            .componentId = componentId.toString(),
            .packageDigest = packageDigest.toString(),
        };
        if (state.pending.contains(activation.instanceId)) {
            return readFailure(
                ReadStatus::Damaged,
                QStringLiteral("Duplicate pending activation in %1").arg(path)
            );
        }
        state.pending.insert(activation.instanceId, std::move(activation));
    }
    QSet<QString> packageStates;
    for (auto iterator = state.records.cbegin();
         iterator != state.records.cend(); ++iterator) {
        packageStates.insert(iterator.key());
    }
    for (const auto &activation : std::as_const(state.pending)) {
        packageStates.insert(ComponentRuntimeHealthStore::recordKey(
            activation.componentId,
            activation.packageDigest
        ));
    }
    if (packageStates.size() > maximumHealthRecords) {
        return readFailure(
            ReadStatus::Damaged,
            QStringLiteral(
                "Too many runtime package states in %1"
            ).arg(path)
        );
    }
    return readSuccess(std::move(state));
}

ReadResult readFile(const QString &path)
{
    QString ancestryError;
    if (!validatePathAncestry(QFileInfo(path).absolutePath(), ancestryError)) {
        return readFailure(ReadStatus::Unreadable, ancestryError);
    }
    const auto encoded = QFile::encodeName(path);
    const auto descriptor = ::open(
        encoded.constData(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK
    );
    if (descriptor < 0) {
        if (errno == ENOENT) {
            return {};
        }
        return readFailure(
            ReadStatus::Unreadable,
            QStringLiteral("Cannot read runtime health state %1").arg(path)
        );
    }
    struct stat status {};
    if (::fstat(descriptor, &status) != 0
        || !S_ISREG(status.st_mode)
        || status.st_uid != ::geteuid()
        || (status.st_mode & 0077) != 0
        || status.st_size < 0
        || status.st_size > maximumHealthBytes) {
        ::close(descriptor);
        return readFailure(
            ReadStatus::Unreadable,
            QStringLiteral("Unsafe runtime health state %1").arg(path)
        );
    }
    QString parentError;
    if (!privateImmediateParent(path, parentError)) {
        ::close(descriptor);
        return readFailure(ReadStatus::Unreadable, parentError);
    }
    QByteArray bytes;
    bytes.reserve(static_cast<qsizetype>(status.st_size));
    std::array<char, 4096> buffer {};
    while (bytes.size() <= maximumHealthBytes) {
        const auto count = ::read(descriptor, buffer.data(), buffer.size());
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            ::close(descriptor);
            return readFailure(
                ReadStatus::Unreadable,
                QStringLiteral("Cannot read runtime health state %1").arg(path)
            );
        }
        bytes.append(buffer.data(), static_cast<qsizetype>(count));
    }
    ::close(descriptor);
    if (bytes.size() > maximumHealthBytes) {
        return readFailure(
            ReadStatus::Unreadable,
            QStringLiteral("Oversized runtime health state %1").arg(path)
        );
    }
    return parseState(bytes, path);
}

bool writeFile(
    const QString &path,
    const ComponentRuntimeHealthState &state,
    QString &error
)
{
    if (!prepareParent(path, error) || !safeRegularFile(path, error)) {
        return false;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        error = QStringLiteral("Cannot open runtime health state %1").arg(path);
        return false;
    }
    const auto bytes = serialize(state);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        error = QStringLiteral("Cannot commit runtime health state %1").arg(path);
        return false;
    }
    // The private mode was applied to the temporary inode above and survives
    // QSaveFile's rename. Keep commit as the final fallible operation so a
    // successfully renamed recovery snapshot is never reported as non-durable.
    if (!file.commit()) {
        error = QStringLiteral("Cannot commit runtime health state %1").arg(path);
        return false;
    }
    return true;
}

} // namespace

ComponentRuntimeHealthPaths ComponentRuntimeHealthPaths::standard()
{
    const auto stateRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericStateLocation
    );
    return {
        .activeFile = QDir(stateRoot).filePath(
            QStringLiteral("hyprshelld/component-runtime-health.json")
        ),
        .recoveryFile = QDir(stateRoot).filePath(
            QStringLiteral("hyprshelld/component-runtime-health.last-good.json")
        ),
    };
}

ComponentRuntimeHealthStore::ComponentRuntimeHealthStore(
    ComponentRuntimeHealthPaths paths,
    PersistFaultInjector faultInjector
)
    : paths_(std::move(paths))
    , faultInjector_(std::move(faultInjector))
{
}

ComponentRuntimeHealthLoadResult ComponentRuntimeHealthStore::load() const
{
    const auto active = readFile(paths_.activeFile);
    const auto recovery = readFile(paths_.recoveryFile);
    if (active.status == ReadStatus::Missing
        && recovery.status == ReadStatus::Missing) {
        return {.success = true, .state = {}, .error = {}};
    }

    const auto activeUnsafe = active.status == ReadStatus::Unsupported
        || active.status == ReadStatus::Unreadable;
    const auto recoveryUnsafe = recovery.status == ReadStatus::Unsupported
        || recovery.status == ReadStatus::Unreadable;
    if (activeUnsafe || recoveryUnsafe) {
        ComponentRuntimeHealthLoadResult result;
        result.state.safeMode = true;
        result.error = !active.error.isEmpty() ? active.error : recovery.error;
        return result;
    }

    const ComponentRuntimeHealthState *selected = nullptr;
    bool repairActive = false;
    bool repairRecovery = false;
    if (active.status == ReadStatus::Valid
        && recovery.status == ReadStatus::Valid) {
        if (active.state.revision == recovery.state.revision
            && active.state != recovery.state) {
            return {
                .success = false,
                .state = {
                    .revision = 0,
                    .safeMode = true,
                    .records = {},
                    .pending = {},
                },
                .error = QStringLiteral(
                    "Runtime health snapshots disagree at one revision"
                ),
            };
        }
        if (recovery.state.revision > active.state.revision) {
            selected = &recovery.state;
            repairActive = true;
        } else {
            selected = &active.state;
            repairRecovery = recovery.state != active.state;
        }
    } else if (active.status == ReadStatus::Valid) {
        selected = &active.state;
        repairRecovery = true;
    } else if (recovery.status == ReadStatus::Valid) {
        selected = &recovery.state;
        repairActive = true;
    } else {
        return {
            .success = false,
            .state = {
                .revision = 0,
                .safeMode = true,
                .records = {},
                .pending = {},
            },
            .error = !active.error.isEmpty()
                ? active.error : recovery.error,
        };
    }

    QString repairError;
    if ((repairActive
            && !writeFile(paths_.activeFile, *selected, repairError))
        || (repairRecovery
            && !writeFile(paths_.recoveryFile, *selected, repairError))) {
        return {
            .success = false,
            .state = {
                .revision = 0,
                .safeMode = true,
                .records = {},
                .pending = {},
            },
            .error = repairError,
        };
    }
    return {.success = true, .state = *selected, .error = {}};
}

ComponentRuntimeHealthPersistResult ComponentRuntimeHealthStore::persist(
    const ComponentRuntimeHealthState &state
) const
{
    QString error;
    const auto validated = parseState(
        serialize(state),
        QStringLiteral("<memory>")
    );
    if (validated.status != ReadStatus::Valid
        || validated.state != state) {
        return {
            .error = QStringLiteral(
                "Runtime health state violates storage bounds"
            ),
        };
    }

    // Validate both targets before creating a newer durable snapshot. Once the
    // recovery write succeeds it is the commit point; load() selects its newer
    // revision and repairs an interrupted active mirror.
    if (!prepareParent(paths_.recoveryFile, error)
        || !safeRegularFile(paths_.recoveryFile, error)
        || !prepareParent(paths_.activeFile, error)
        || !safeRegularFile(paths_.activeFile, error)) {
        return {.error = error};
    }
    if (faultInjector_
        && faultInjector_(
            ComponentRuntimeHealthPersistPhase::BeforeRecoveryCommit
        )) {
        return {
            .error = QStringLiteral(
                "Injected failure before runtime health recovery commit"
            ),
        };
    }
    if (!writeFile(paths_.recoveryFile, state, error)) {
        return {.error = error};
    }
    if ((faultInjector_
            && faultInjector_(
                ComponentRuntimeHealthPersistPhase::BeforeActiveMirror
            ))
        || !writeFile(paths_.activeFile, state, error)) {
        return {
            .durability =
                ComponentRuntimeHealthPersistDurability::RecoveryDurable,
            .error = error.isEmpty()
                ? QStringLiteral(
                    "Injected failure before runtime health active mirror"
                )
                : error,
        };
    }
    return {
        .durability = ComponentRuntimeHealthPersistDurability::Mirrored,
        .error = {},
    };
}

QString ComponentRuntimeHealthStore::recordKey(
    const QString &componentId,
    const QString &packageDigest
)
{
    return componentId + QLatin1Char('/') + packageDigest;
}

} // namespace HyprShelld
