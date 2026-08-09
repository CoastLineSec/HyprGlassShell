#include "component_store.h"

#include "component/component_contract.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld {
namespace {

enum class FileStatus {
    Missing,
    Valid,
    Damaged,
    Unsupported,
    Unreadable,
};

struct ReadResult final {
    FileStatus status = FileStatus::Missing;
    Components::ComponentConfiguration state;
    QString error;
};

class FileDescriptor final {
public:
    explicit FileDescriptor(const int value)
        : value_(value)
    {
    }

    ~FileDescriptor()
    {
        if (value_ >= 0) {
            ::close(value_);
        }
    }

    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;

    [[nodiscard]] int get() const
    {
        return value_;
    }

private:
    int value_ = -1;
};

QString openError(const QString &path, const int errorNumber)
{
    return QStringLiteral("Cannot safely read %1: %2")
        .arg(path, QString::fromLocal8Bit(std::strerror(errorNumber)));
}

QString describeErrors(const Components::ValidationErrors &errors)
{
    QStringList details;
    details.reserve(errors.size());
    for (const auto &error : errors) {
        details.append(
            QStringLiteral("%1 [%2]: %3")
                .arg(error.path, error.code, error.message)
        );
    }
    return details.join(QStringLiteral("; "));
}

bool hasUnsupportedFormat(const Components::ValidationErrors &errors)
{
    return std::ranges::any_of(errors, [](const auto &error) {
        return error.code == QStringLiteral("component-config.unsupported-format");
    });
}

ReadResult readSnapshot(
    const QString &path,
    const Components::ConfigurationCatalog &catalog
)
{
    const auto encodedPath = QFile::encodeName(path);
    const auto rawDescriptor = ::open(
        encodedPath.constData(),
        O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW
    );
    if (rawDescriptor < 0) {
        const auto errorNumber = errno;
        if (errorNumber == ENOENT || errorNumber == ENOTDIR) {
            return {};
        }
        return {
            .status = FileStatus::Unreadable,
            .error = openError(path, errorNumber),
        };
    }
    const FileDescriptor descriptor(rawDescriptor);

    struct stat metadata {};
    if (::fstat(descriptor.get(), &metadata) != 0) {
        return {
            .status = FileStatus::Unreadable,
            .error = openError(path, errno),
        };
    }
    if (!S_ISREG(metadata.st_mode)) {
        return {
            .status = FileStatus::Unreadable,
            .error = QStringLiteral(
                "Component configuration is not a regular file: %1"
            ).arg(path),
        };
    }

    const auto size = metadata.st_size;
    if (size < 0
        || size > Components::maximumComponentConfigurationBytes) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Component configuration exceeds the size limit: %1")
                         .arg(path),
        };
    }

    QFile file;
    if (!file.open(
            descriptor.get(),
            QIODevice::ReadOnly,
            QFileDevice::DontCloseHandle
        )) {
        return {
            .status = FileStatus::Unreadable,
            .error = QStringLiteral("Cannot read %1: %2")
                         .arg(path, file.errorString()),
        };
    }
    const auto bytes = file.read(
        Components::maximumComponentConfigurationBytes + 1
    );
    if (bytes.size() > Components::maximumComponentConfigurationBytes) {
        return {
            .status = FileStatus::Damaged,
            .error = QStringLiteral("Component configuration exceeds the size limit: %1")
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

    auto parsed = Components::parseComponentConfiguration(
        QByteArrayView(bytes),
        catalog
    );
    if (!parsed) {
        return {
            .status = hasUnsupportedFormat(parsed.errors)
                ? FileStatus::Unsupported
                : FileStatus::Damaged,
            .error = QStringLiteral("Invalid component configuration in %1: %2")
                         .arg(path, describeErrors(parsed.errors)),
        };
    }
    return {
        .status = FileStatus::Valid,
        .state = std::move(*parsed.value),
    };
}

bool writeSnapshot(
    const QString &path,
    const Components::ComponentConfiguration &state,
    QString &error
)
{
    const auto parent = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(parent)) {
        error = QStringLiteral("Cannot create component configuration directory %1")
                    .arg(parent);
        return false;
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("Cannot write %1: %2")
                    .arg(path, file.errorString());
        return false;
    }
    const auto bytes = Components::serializeComponentConfiguration(state);
    if (file.write(bytes) != bytes.size()) {
        error = QStringLiteral("Cannot write %1: %2")
                    .arg(path, file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = QStringLiteral("Cannot commit %1: %2")
                    .arg(path, file.errorString());
        return false;
    }
    return true;
}

bool protectedDefaultMatchesCatalog(
    const Components::ComponentConfiguration &state,
    const Components::ConfigurationCatalog &catalog,
    QString &error
)
{
    const auto componentId = QString::fromLatin1(
        Components::workspaceSwitcherId
    );
    const auto desired = state.components.constFind(componentId);
    const auto catalogEntry = catalog.entries.constFind(componentId);
    if (desired == state.components.cend()
        || catalogEntry == catalog.entries.cend()
        || desired->packageDigest != catalogEntry->packageDigest) {
        error = QStringLiteral(
            "Protected component defaults do not match the workspace catalog digest"
        );
        return false;
    }
    const auto instanceId = QString::fromLatin1(
        Components::workspaceSwitcherDefaultInstanceId
    );
    const auto instance = state.instances.constFind(instanceId);
    const auto layout = state.bars.constFind(QString::fromLatin1(
        Components::defaultBarLayoutId
    ));
    if (state.revision != 0 || state.components.size() != 1
        || !desired->enabled || !desired->grantedCapabilities.isEmpty()
        || !desired->settings.isEmpty() || state.instances.size() != 1
        || instance == state.instances.cend()
        || instance->componentId != componentId || !instance->enabled
        || instance->settings != Components::workspaceSwitcherDefaultSettings()
        || state.bars.size() != 1 || layout == state.bars.cend()
        || layout->outputs.mode != QStringLiteral("all")
        || !layout->outputs.names.isEmpty()
        || layout->start != QStringList{instanceId}
        || !layout->center.isEmpty() || !layout->end.isEmpty()) {
        error = QStringLiteral(
            "Protected component defaults do not match the compiled first-run composition"
        );
        return false;
    }
    return true;
}

std::optional<QJsonObject> componentSettingsFromLegacy(
    const LegacyWorkspaceSettings &legacy
)
{
    QJsonObject settings{
        {QStringLiteral("labelMode"), legacy.labelMode},
        {QStringLiteral("showApplications"), legacy.showApplications},
        {
            QStringLiteral("maximumApplications"),
            static_cast<qint64>(legacy.maximumApplications),
        },
        {QStringLiteral("occupiedOnly"), legacy.occupiedOnly},
        {QStringLiteral("scrollMode"), legacy.scrollMode},
    };
    if (!Components::isValidWorkspaceSwitcherSettings(settings)) {
        return std::nullopt;
    }
    return settings;
}

} // namespace

QString componentLoadStateName(const ComponentLoadState state)
{
    switch (state) {
    case ComponentLoadState::Normal: return QStringLiteral("normal");
    case ComponentLoadState::Recovered: return QStringLiteral("recovered");
    case ComponentLoadState::Defaulted: return QStringLiteral("defaulted");
    case ComponentLoadState::Unsupported: return QStringLiteral("unsupported");
    case ComponentLoadState::Unavailable: return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

ComponentPaths ComponentPaths::standard(const QString &defaultsFile)
{
    const auto configRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation
    );
    const auto stateRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericStateLocation
    );
    return {
        .activeFile = QDir(configRoot).filePath(
            QStringLiteral("hyprshelld/components.json")
        ),
        .recoveryFile = QDir(stateRoot).filePath(
            QStringLiteral("hyprshelld/components.last-good.json")
        ),
        .defaultsFile = defaultsFile,
    };
}

ComponentStore::ComponentStore(
    ComponentPaths paths,
    SnapshotWriter writer
)
    : paths_(std::move(paths))
    , writer_(std::move(writer))
{
}

bool ComponentStore::write(
    const QString &path,
    const Components::ComponentConfiguration &state,
    QString &error
) const
{
    return writer_ ? writer_(path, state, error)
                   : writeSnapshot(path, state, error);
}

ComponentLoadResult ComponentStore::load(
    const Components::ConfigurationCatalog &catalog,
    const std::optional<LegacyWorkspaceSettings> &legacyWorkspaceSettings
) const
{
    const auto active = readSnapshot(paths_.activeFile, catalog);
    const auto recovery = readSnapshot(paths_.recoveryFile, catalog);
    const auto firstRun = active.status == FileStatus::Missing
        && recovery.status == FileStatus::Missing;

    if (active.status == FileStatus::Valid) {
        if (recovery.status == FileStatus::Unsupported) {
            return {
                .available = true,
                .writable = false,
                .state = active.state,
                .loadState = ComponentLoadState::Unsupported,
                .error = recovery.error,
            };
        }
        if (recovery.status == FileStatus::Unreadable) {
            return {
                .available = true,
                .writable = false,
                .state = active.state,
                .loadState = ComponentLoadState::Unavailable,
                .error = recovery.error,
            };
        }
        if (recovery.status != FileStatus::Valid
            || recovery.state != active.state) {
            QString error;
            if (!write(paths_.recoveryFile, active.state, error)) {
                return {
                    .available = true,
                    .writable = false,
                    .state = active.state,
                    .loadState = ComponentLoadState::Unavailable,
                    .error = error,
                };
            }
        }
        return {
            .available = true,
            .writable = true,
            .state = active.state,
            .loadState = ComponentLoadState::Normal,
        };
    }

    if (active.status == FileStatus::Unsupported) {
        return {
            .loadState = ComponentLoadState::Unsupported,
            .error = active.error,
        };
    }
    if (active.status == FileStatus::Unreadable) {
        return {
            .loadState = ComponentLoadState::Unavailable,
            .error = active.error,
        };
    }

    if (recovery.status == FileStatus::Unsupported) {
        return {
            .loadState = ComponentLoadState::Unsupported,
            .error = recovery.error,
        };
    }
    if (recovery.status == FileStatus::Unreadable) {
        return {
            .loadState = ComponentLoadState::Unavailable,
            .error = recovery.error,
        };
    }

    if (recovery.status == FileStatus::Valid) {
        QString error;
        if (!write(paths_.activeFile, recovery.state, error)) {
            return {
                .available = true,
                .writable = false,
                .state = recovery.state,
                .loadState = ComponentLoadState::Unavailable,
                .error = error,
            };
        }
        return {
            .available = true,
            .writable = true,
            .state = recovery.state,
            .loadState = ComponentLoadState::Recovered,
        };
    }

    const auto defaults = readSnapshot(paths_.defaultsFile, catalog);
    if (defaults.status != FileStatus::Valid) {
        return {
            .loadState = defaults.status == FileStatus::Unsupported
                ? ComponentLoadState::Unsupported
                : ComponentLoadState::Unavailable,
            .error = defaults.error.isEmpty()
                ? QStringLiteral("Protected component defaults are unavailable")
                : defaults.error,
        };
    }
    QString error;
    if (!protectedDefaultMatchesCatalog(defaults.state, catalog, error)) {
        return {
            .loadState = ComponentLoadState::Unavailable,
            .error = error,
        };
    }

    auto initial = defaults.state;
    if (firstRun && legacyWorkspaceSettings) {
        const auto migrated = componentSettingsFromLegacy(
            *legacyWorkspaceSettings
        );
        if (migrated) {
            const auto instanceId = QString::fromLatin1(
                Components::workspaceSwitcherDefaultInstanceId
            );
            initial.instances[instanceId].settings = *migrated;
        }
    }

    if (!write(paths_.recoveryFile, initial, error)
        || !write(paths_.activeFile, initial, error)) {
        return {
            .loadState = ComponentLoadState::Unavailable,
            .error = error,
        };
    }
    return {
        .available = true,
        .writable = true,
        .state = initial,
        .loadState = firstRun
            ? ComponentLoadState::Normal
            : ComponentLoadState::Defaulted,
    };
}

bool ComponentStore::persist(
    const Components::ComponentConfiguration &current,
    const Components::ComponentConfiguration &next,
    QString &error
) const
{
    if (!write(paths_.recoveryFile, current, error)
        || !write(paths_.activeFile, next, error)) {
        return false;
    }

    QString recoveryError;
    if (!write(paths_.recoveryFile, next, recoveryError)) {
        qWarning().noquote()
            << QStringLiteral("Failed to refresh component recovery state: %1")
                   .arg(recoveryError);
    }
    return true;
}

} // namespace HyprShelld
