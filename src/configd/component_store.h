#pragma once

#include "component/component_configuration.h"
#include "legacy_workspace_settings.h"

#include <QString>

#include <functional>
#include <optional>

namespace HyprShelld {

enum class ComponentLoadState {
    Normal,
    Recovered,
    Defaulted,
    Unsupported,
    Unavailable,
};

struct ComponentPaths final {
    QString activeFile;
    QString recoveryFile;
    QString defaultsFile;

    [[nodiscard]] static ComponentPaths standard(const QString &defaultsFile);
};

struct ComponentLoadResult final {
    bool available = false;
    bool writable = false;
    Components::ComponentConfiguration state;
    ComponentLoadState loadState = ComponentLoadState::Unavailable;
    QString error;
};

struct ComponentMigrationResult final {
    bool changed = false;
    bool writable = true;
    Components::ComponentConfiguration state;
    QString error;
};

class ComponentStore final {
public:
    using SnapshotWriter = std::function<bool(
        const QString &,
        const Components::ComponentConfiguration &,
        QString &
    )>;

    explicit ComponentStore(
        ComponentPaths paths,
        SnapshotWriter writer = {}
    );

    // Legacy settings are considered only when both managed snapshots are absent.
    [[nodiscard]] ComponentLoadResult load(
        const Components::ConfigurationCatalog &catalog,
        const std::optional<LegacyWorkspaceSettings> &legacyWorkspaceSettings =
            std::nullopt
    ) const;
    // Applies narrowly recognized built-in package migrations. A migrated
    // snapshot is committed recovery-first so an interrupted update can be
    // completed without reverting user state on the next load.
    [[nodiscard]] ComponentMigrationResult migrate(
        const Components::ComponentConfiguration &current,
        const Components::ConfigurationCatalog &catalog
    ) const;
    // Confirms that candidate is exactly the pinned migration of current.
    [[nodiscard]] bool recognizesMigration(
        const Components::ComponentConfiguration &current,
        const Components::ComponentConfiguration &candidate,
        const Components::ConfigurationCatalog &catalog
    ) const;
    [[nodiscard]] bool persist(
        const Components::ComponentConfiguration &current,
        const Components::ComponentConfiguration &next,
        QString &error
    ) const;

private:
    [[nodiscard]] bool write(
        const QString &path,
        const Components::ComponentConfiguration &state,
        QString &error
    ) const;

    ComponentPaths paths_;
    SnapshotWriter writer_;
};

[[nodiscard]] QString componentLoadStateName(ComponentLoadState state);

} // namespace HyprShelld
