#include "config_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>

#include <algorithm>
#include <utility>

namespace HyprShelld {
namespace {

constexpr auto formatVersion = 3;
constexpr auto borderFormatVersion = 2;
constexpr auto legacyFormatVersion = 1;
constexpr qsizetype maximumSnapshotBytes = 64 * 1024;

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
    std::optional<LegacyWorkspaceSettings> legacyWorkspaceSettings;
    bool hadLegacyWorkspaceBlock = false;
    bool requiresMigration = false;
    QString error;
};

QByteArray serialize(
    const ConfigState &state,
    const std::optional<LegacyWorkspaceSettings> &legacyWorkspaceSettings
)
{
    QJsonObject object;
    object.insert(QStringLiteral("formatVersion"), formatVersion);
    object.insert(QStringLiteral("revision"), QString::number(state.revision));
    object.insert(QStringLiteral("barHeight"), static_cast<qint64>(state.barHeight));
    object.insert(
        QStringLiteral("shellBorderEnabled"),
        state.shellBorderEnabled
    );
    object.insert(
        QStringLiteral("shellBorderWidth"),
        static_cast<qint64>(state.shellBorderWidth)
    );
    object.insert(
        QStringLiteral("shellBorderRadius"),
        static_cast<qint64>(state.shellBorderRadius)
    );
    object.insert(
        QStringLiteral("syncHyprlandWindowBorders"),
        state.syncHyprlandWindowBorders
    );
    object.insert(
        QStringLiteral("shellInnerSpacing"),
        static_cast<qint64>(state.shellInnerSpacing)
    );
    object.insert(
        QStringLiteral("shellOuterSpacing"),
        static_cast<qint64>(state.shellOuterSpacing)
    );
    object.insert(
        QStringLiteral("syncHyprlandWindowSpacing"),
        state.syncHyprlandWindowSpacing
    );
    if (legacyWorkspaceSettings) {
        object.insert(
            QStringLiteral("workspaceSwitcher"),
            QJsonObject{
                {
                    QStringLiteral("labelMode"),
                    legacyWorkspaceSettings->labelMode,
                },
                {
                    QStringLiteral("showApplications"),
                    legacyWorkspaceSettings->showApplications,
                },
                {
                    QStringLiteral("maximumApplications"),
                    static_cast<qint64>(
                        legacyWorkspaceSettings->maximumApplications
                    ),
                },
                {
                    QStringLiteral("occupiedOnly"),
                    legacyWorkspaceSettings->occupiedOnly,
                },
                {
                    QStringLiteral("scrollMode"),
                    legacyWorkspaceSettings->scrollMode,
                },
            }
        );
    }

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

std::optional<LegacyWorkspaceSettings> parseLegacyWorkspaceSettings(
    const QJsonValue &value
)
{
    if (!value.isObject()) {
        return std::nullopt;
    }

    const auto workspace = value.toObject();
    const auto labelMode = workspace.value(QStringLiteral("labelMode"));
    const auto showApplications = workspace.value(
        QStringLiteral("showApplications")
    );
    const auto maximumApplications = workspace.value(
        QStringLiteral("maximumApplications")
    );
    const auto occupiedOnly = workspace.value(QStringLiteral("occupiedOnly"));
    const auto scrollMode = workspace.value(QStringLiteral("scrollMode"));

    const auto label = labelMode.toString();
    const auto scroll = scrollMode.toString();
    const auto maximum = maximumApplications.toInteger(-1);
    const auto validLabel = label == QStringLiteral("compact")
        || label == QStringLiteral("numbers")
        || label == QStringLiteral("names");
    const auto validScroll = scroll == QStringLiteral("disabled")
        || scroll == QStringLiteral("normal")
        || scroll == QStringLiteral("reversed");

    if (!labelMode.isString() || !validLabel
        || !showApplications.isBool()
        || !maximumApplications.isDouble()
        || maximumApplications.toDouble() != static_cast<double>(maximum)
        || maximum < 1 || maximum > 5
        || !occupiedOnly.isBool()
        || !scrollMode.isString() || !validScroll) {
        return std::nullopt;
    }

    return LegacyWorkspaceSettings{
        .labelMode = label,
        .showApplications = showApplications.toBool(),
        .maximumApplications = static_cast<quint32>(maximum),
        .occupiedOnly = occupiedOnly.toBool(),
        .scrollMode = scroll,
    };
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

    const auto size = file.size();
    if (size < 0 || size > maximumSnapshotBytes) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Configuration exceeds the size limit in %1")
                         .arg(path),
        };
    }

    const auto bytes = file.read(maximumSnapshotBytes + 1);
    if (bytes.size() > maximumSnapshotBytes) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Configuration exceeds the size limit in %1")
                         .arg(path),
        };
    }
    if (file.error() != QFileDevice::NoError) {
        return {
            .status = FileStatus::Unreadable,
            .error = QStringLiteral("Cannot read %1: %2")
                         .arg(path, file.errorString()),
        };
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
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
    if (version != legacyFormatVersion
        && version != borderFormatVersion
        && version != formatVersion) {
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
    if (height < ConfigValues::minimumBarHeight
        || height > ConfigValues::maximumBarHeight) {
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

    ConfigState state;
    state.barHeight = static_cast<quint32>(height);
    if (version == legacyFormatVersion) {
        state.shellBorderEnabled = true;
        state.shellBorderWidth = ConfigValues::defaultShellBorderWidth;
        state.shellBorderRadius = std::min(
            16U,
            static_cast<quint32>(height) * 3U / 8U
        );
        state.syncHyprlandWindowBorders = false;
    } else {
        const auto enabledValue = object.value(
            QStringLiteral("shellBorderEnabled")
        );
        const auto widthValue = object.value(
            QStringLiteral("shellBorderWidth")
        );
        const auto radiusValue = object.value(
            QStringLiteral("shellBorderRadius")
        );
        const auto syncValue = object.value(
            QStringLiteral("syncHyprlandWindowBorders")
        );
        const auto width = widthValue.toInteger(-1);
        const auto radius = radiusValue.toInteger(-1);
        if (!enabledValue.isBool()
            || !widthValue.isDouble()
            || width < ConfigValues::minimumShellBorderWidth
            || width > ConfigValues::maximumShellBorderWidth
            || !radiusValue.isDouble()
            || radius < ConfigValues::minimumShellBorderRadius
            || radius > ConfigValues::maximumShellBorderRadius
            || !syncValue.isBool()) {
            return {
                .status = FileStatus::Damaged,
                .error = QStringLiteral("Invalid shared border in %1").arg(path),
            };
        }

        state.shellBorderEnabled = enabledValue.toBool();
        state.shellBorderWidth = static_cast<quint32>(width);
        state.shellBorderRadius = static_cast<quint32>(radius);
        state.syncHyprlandWindowBorders = syncValue.toBool();
    }

    if (version < formatVersion) {
        state.shellInnerSpacing = ConfigValues::defaultShellInnerSpacing;
        state.shellOuterSpacing = ConfigValues::defaultShellOuterSpacing;
        state.syncHyprlandWindowSpacing = false;
    } else {
        const auto innerValue = object.value(
            QStringLiteral("shellInnerSpacing")
        );
        const auto outerValue = object.value(
            QStringLiteral("shellOuterSpacing")
        );
        const auto syncValue = object.value(
            QStringLiteral("syncHyprlandWindowSpacing")
        );
        const auto inner = innerValue.toInteger(-1);
        const auto outer = outerValue.toInteger(-1);
        if (!innerValue.isDouble()
            || inner < ConfigValues::minimumShellSpacing
            || inner > ConfigValues::maximumShellSpacing
            || !outerValue.isDouble()
            || outer < ConfigValues::minimumShellSpacing
            || outer > ConfigValues::maximumShellSpacing
            || !syncValue.isBool()) {
            return {
                .status = FileStatus::Damaged,
                .error = QStringLiteral("Invalid shared spacing in %1")
                             .arg(path),
            };
        }
        state.shellInnerSpacing = static_cast<quint32>(inner);
        state.shellOuterSpacing = static_cast<quint32>(outer);
        state.syncHyprlandWindowSpacing = syncValue.toBool();
    }
    state.revision = revision;

    return {
        .status = FileStatus::Valid,
        .state = state,
        .legacyWorkspaceSettings = object.contains(
            QStringLiteral("workspaceSwitcher")
        ) ? parseLegacyWorkspaceSettings(object.value(
                QStringLiteral("workspaceSwitcher")
            ))
          : std::nullopt,
        .hadLegacyWorkspaceBlock = object.contains(
            QStringLiteral("workspaceSwitcher")
        ),
        .requiresMigration = version != formatVersion,
    };
}

bool writeSnapshot(
    const QString &path,
    const ConfigState &state,
    const std::optional<LegacyWorkspaceSettings> &legacyWorkspaceSettings,
    QString &error
)
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

    const auto data = serialize(state, legacyWorkspaceSettings);
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

bool rewriteSnapshots(
    const ConfigPaths &paths,
    const ConfigState &state,
    const std::optional<LegacyWorkspaceSettings> &legacyWorkspaceSettings,
    QString &error
)
{
    if (!writeSnapshot(
            paths.recoveryFile,
            state,
            legacyWorkspaceSettings,
            error
        )) {
        return false;
    }
    return writeSnapshot(
        paths.activeFile,
        state,
        legacyWorkspaceSettings,
        error
    );
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
        if (active.requiresMigration) {
            QString error;
            if (!rewriteSnapshots(
                    paths_,
                    active.state,
                    active.legacyWorkspaceSettings,
                    error
                )) {
                return {.error = error};
            }
        } else if (recovery.status != FileStatus::Valid
            || recovery.requiresMigration
            || recovery.state != active.state
            || recovery.legacyWorkspaceSettings
                != active.legacyWorkspaceSettings) {
            QString error;
            if (!writeSnapshot(
                    paths_.recoveryFile,
                    active.state,
                    active.legacyWorkspaceSettings,
                    error
                )) {
                return {.error = error};
            }
        }

        return {
            .success = true,
            .state = active.state,
            .recoveryState = ConfigRecoveryState::Normal,
            .legacyWorkspaceSettings = active.legacyWorkspaceSettings,
            .legacyWorkspaceRetirementPending =
                active.hadLegacyWorkspaceBlock,
        };
    }

    if (recovery.status == FileStatus::Valid) {
        QString error;
        const auto repaired = recovery.requiresMigration
            ? rewriteSnapshots(
                  paths_,
                  recovery.state,
                  recovery.legacyWorkspaceSettings,
                  error
              )
            : writeSnapshot(
                  paths_.activeFile,
                  recovery.state,
                  recovery.legacyWorkspaceSettings,
                  error
              );
        if (!repaired) {
            return {.error = error};
        }

        return {
            .success = true,
            .state = recovery.state,
            .recoveryState = ConfigRecoveryState::Recovered,
            .legacyWorkspaceSettings = recovery.legacyWorkspaceSettings,
            .legacyWorkspaceRetirementPending =
                recovery.hadLegacyWorkspaceBlock,
        };
    }

    const ConfigState defaults;
    QString error;
    if (!rewriteSnapshots(paths_, defaults, std::nullopt, error)) {
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
    const std::optional<LegacyWorkspaceSettings> &legacyWorkspaceSettings,
    QString &error
) const
{
    if (!writeSnapshot(
            paths_.recoveryFile,
            current,
            legacyWorkspaceSettings,
            error
        )) {
        return false;
    }
    if (!writeSnapshot(
            paths_.activeFile,
            next,
            legacyWorkspaceSettings,
            error
        )) {
        return false;
    }

    QString recoveryError;
    if (!writeSnapshot(
            paths_.recoveryFile,
            next,
            legacyWorkspaceSettings,
            recoveryError
        )) {
        qWarning().noquote()
            << QStringLiteral("Failed to refresh recovery settings: %1")
                   .arg(recoveryError);
    }

    return true;
}

bool ConfigStore::retireLegacyWorkspaceSettings(
    const ConfigState &state,
    QString &error
) const
{
    if (!writeSnapshot(paths_.recoveryFile, state, std::nullopt, error)) {
        return false;
    }
    return writeSnapshot(paths_.activeFile, state, std::nullopt, error);
}

} // namespace HyprShelld
