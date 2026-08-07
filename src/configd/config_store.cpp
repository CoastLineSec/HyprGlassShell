#include "config_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>

#include <utility>

namespace HyprShelld {
namespace {

constexpr auto formatVersion = 1;

enum class FileStatus {
    Missing,
    Valid,
    Damaged,
    Unsupported,
    Unreadable,
};

struct ReadResult final {
    FileStatus status = FileStatus::Missing;
    ConfigState state;
    QString error;
};

QByteArray serialize(const ConfigState &state)
{
    QJsonObject object;
    object.insert(QStringLiteral("formatVersion"), formatVersion);
    object.insert(QStringLiteral("revision"), QString::number(state.revision));
    object.insert(QStringLiteral("barHeight"), static_cast<qint64>(state.barHeight));

    auto data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    data.append('\n');
    return data;
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

    bool converted = false;
    revision = text.toULongLong(&converted, 10);
    return converted;
}

ReadResult readSnapshot(const QString &path)
{
    if (!QFileInfo::exists(path)) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {
            .status = FileStatus::Unreadable,
            .error = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString()),
        };
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Invalid JSON in %1").arg(path),
        };
    }

    const auto object = document.object();
    const auto versionValue = object.value(QStringLiteral("formatVersion"));
    if (!versionValue.isDouble()) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Missing format version in %1").arg(path),
        };
    }

    const auto version = versionValue.toInteger(-1);
    if (version > formatVersion) {
        return {
            .status = FileStatus::Unsupported,
            .error = QStringLiteral("Unsupported format version in %1").arg(path),
        };
    }
    if (version != formatVersion) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Invalid format version in %1").arg(path),
        };
    }

    const auto heightValue = object.value(QStringLiteral("barHeight"));
    if (!heightValue.isDouble()) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Invalid bar height in %1").arg(path),
        };
    }

    const auto height = heightValue.toInteger(-1);
    if (height < 32 || height > 96) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Invalid bar height in %1").arg(path),
        };
    }

    const auto revisionValue = object.value(QStringLiteral("revision"));
    quint64 revision = 0;
    if (!revisionValue.isString() || !parseRevision(revisionValue.toString(), revision)) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Invalid revision in %1").arg(path),
        };
    }

    return {
        .status = FileStatus::Valid,
        .state = {
            .barHeight = static_cast<quint32>(height),
            .revision = revision,
        },
    };
}

bool writeSnapshot(const QString &path, const ConfigState &state, QString &error)
{
    const auto parent = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(parent)) {
        error = QStringLiteral("Cannot create configuration directory %1").arg(parent);
        return false;
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }

    const auto data = serialize(state);
    if (file.write(data) != data.size()) {
        error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        error = QStringLiteral("Cannot commit %1: %2").arg(path, file.errorString());
        return false;
    }

    return true;
}

bool initializeSnapshots(
    const ConfigPaths &paths,
    const ConfigState &state,
    QString &error
)
{
    if (!writeSnapshot(paths.recoveryFile, state, error)) {
        return false;
    }
    return writeSnapshot(paths.activeFile, state, error);
}

} // namespace

ConfigPaths ConfigPaths::standard()
{
    const auto configRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation
    );
    const auto stateRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericStateLocation
    );

    return {
        .activeFile = QDir(configRoot).filePath(
            QStringLiteral("hyprshelld/settings.json")
        ),
        .recoveryFile = QDir(stateRoot).filePath(
            QStringLiteral("hyprshelld/settings.last-good.json")
        ),
    };
}

ConfigStore::ConfigStore(ConfigPaths paths)
    : paths_(std::move(paths))
{
}

ConfigLoadResult ConfigStore::load() const
{
    const auto active = readSnapshot(paths_.activeFile);
    const auto recovery = readSnapshot(paths_.recoveryFile);

    if (active.status == FileStatus::Unsupported) {
        return {.error = active.error};
    }
    if (active.status == FileStatus::Unreadable) {
        return {.error = active.error};
    }
    if (recovery.status == FileStatus::Unsupported) {
        return {.error = recovery.error};
    }
    if (recovery.status == FileStatus::Unreadable) {
        return {.error = recovery.error};
    }

    if (active.status == FileStatus::Valid) {
        if (recovery.status != FileStatus::Valid || recovery.state != active.state) {
            QString error;
            if (!writeSnapshot(paths_.recoveryFile, active.state, error)) {
                return {.error = error};
            }
        }

        return {
            .success = true,
            .state = active.state,
            .recoveryState = ConfigRecoveryState::Normal,
        };
    }

    if (recovery.status == FileStatus::Valid) {
        QString error;
        if (!writeSnapshot(paths_.activeFile, recovery.state, error)) {
            return {.error = error};
        }

        return {
            .success = true,
            .state = recovery.state,
            .recoveryState = ConfigRecoveryState::Recovered,
        };
    }

    const ConfigState defaults;
    QString error;
    if (!initializeSnapshots(paths_, defaults, error)) {
        return {.error = error};
    }

    const auto firstRun = active.status == FileStatus::Missing
        && recovery.status == FileStatus::Missing;
    return {
        .success = true,
        .state = defaults,
        .recoveryState = firstRun
            ? ConfigRecoveryState::Normal
            : ConfigRecoveryState::Defaulted,
    };
}

bool ConfigStore::persist(
    const ConfigState &current,
    const ConfigState &next,
    QString &error
) const
{
    if (!writeSnapshot(paths_.recoveryFile, current, error)) {
        return false;
    }
    if (!writeSnapshot(paths_.activeFile, next, error)) {
        return false;
    }

    QString recoveryError;
    if (!writeSnapshot(paths_.recoveryFile, next, recoveryError)) {
        qWarning().noquote()
            << QStringLiteral("Failed to refresh recovery settings: %1")
                   .arg(recoveryError);
    }

    return true;
}

} // namespace HyprShelld
