#pragma once

#include <QMap>
#include <QString>
#include <QtTypes>

#include <functional>

namespace HyprShelld {

struct ComponentRuntimeHealthRecord final {
    QString componentId;
    QString packageDigest;
    QString state;
    QString reason;
    quint32 failureCount = 0;

    friend bool operator==(
        const ComponentRuntimeHealthRecord &,
        const ComponentRuntimeHealthRecord &
    ) = default;
};

struct ComponentRuntimePendingActivation final {
    QString instanceId;
    QString componentId;
    QString packageDigest;

    friend bool operator==(
        const ComponentRuntimePendingActivation &,
        const ComponentRuntimePendingActivation &
    ) = default;
};

struct ComponentRuntimeHealthState final {
    quint64 revision = 0;
    bool safeMode = false;
    QMap<QString, ComponentRuntimeHealthRecord> records;
    QMap<QString, ComponentRuntimePendingActivation> pending;

    friend bool operator==(
        const ComponentRuntimeHealthState &,
        const ComponentRuntimeHealthState &
    ) = default;
};

struct ComponentRuntimeHealthPaths final {
    QString activeFile;
    QString recoveryFile;

    [[nodiscard]] static ComponentRuntimeHealthPaths standard();
};

struct ComponentRuntimeHealthLoadResult final {
    bool success = false;
    ComponentRuntimeHealthState state;
    QString error;
};

enum class ComponentRuntimeHealthPersistDurability {
    NotDurable,
    RecoveryDurable,
    Mirrored,
};

struct ComponentRuntimeHealthPersistResult final {
    ComponentRuntimeHealthPersistDurability durability =
        ComponentRuntimeHealthPersistDurability::NotDurable;
    QString error;

    [[nodiscard]] bool durable() const
    {
        return durability
            != ComponentRuntimeHealthPersistDurability::NotDurable;
    }
};

enum class ComponentRuntimeHealthPersistPhase {
    BeforeRecoveryCommit,
    BeforeActiveMirror,
};

class ComponentRuntimeHealthStore final {
public:
    using PersistFaultInjector = std::function<bool(
        ComponentRuntimeHealthPersistPhase
    )>;

    explicit ComponentRuntimeHealthStore(
        ComponentRuntimeHealthPaths paths,
        PersistFaultInjector faultInjector = {}
    );

    [[nodiscard]] ComponentRuntimeHealthLoadResult load() const;
    [[nodiscard]] ComponentRuntimeHealthPersistResult persist(
        const ComponentRuntimeHealthState &state
    ) const;

    [[nodiscard]] static QString recordKey(
        const QString &componentId,
        const QString &packageDigest
    );

private:
    ComponentRuntimeHealthPaths paths_;
    PersistFaultInjector faultInjector_;
};

} // namespace HyprShelld
