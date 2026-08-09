#pragma once

#include "validation_result.h"

#include <QByteArrayView>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Components {

inline constexpr auto builtinIdPrefix =
    "io.github.coastlinesec.hyprshelld.";
inline constexpr auto workspaceSwitcherId =
    "io.github.coastlinesec.hyprshelld.workspace-switcher";
inline constexpr auto workspaceSwitcherFactory = "workspace-switcher";
inline constexpr auto currentComponentApiVersion = "1.0";
inline constexpr auto workspacesReadCapability = "shell.workspaces.read";
inline constexpr auto workspacesActivateCapability =
    "shell.workspaces.activate";

enum class ComponentOrigin {
    System,
    User,
};

enum class ComponentType {
    BarWidget,
    DesktopWidget,
    ShellApplication,
    ShellService,
};

enum class RuntimeKind {
    BuiltinV1,
    DeclarativeV1,
    QmlFullTrustV1,
    ProcessV1,
};

struct ComponentAuthor final {
    QString name;
    std::optional<QString> email;
    std::optional<QString> homepage;

    friend bool operator==(
        const ComponentAuthor &,
        const ComponentAuthor &
    ) = default;
};

struct CapabilityRequest final {
    QString id;
    QString reason;

    friend bool operator==(
        const CapabilityRequest &,
        const CapabilityRequest &
    ) = default;
};

struct ComponentDependency final {
    QString id;
    QString versionRequirement;

    friend bool operator==(
        const ComponentDependency &,
        const ComponentDependency &
    ) = default;
};

struct ComponentRuntime final {
    RuntimeKind kind = RuntimeKind::BuiltinV1;
    QString factory;
    QString entrypoint;
    QStringList arguments;

    friend bool operator==(
        const ComponentRuntime &,
        const ComponentRuntime &
    ) = default;
};

struct ComponentManifest final {
    quint32 manifestVersion = 1;
    ComponentOrigin origin = ComponentOrigin::User;
    QString id;
    QString version;
    ComponentType type = ComponentType::BarWidget;
    QString name;
    QString description;
    QVector<ComponentAuthor> authors;
    QString license;
    std::optional<QString> homepage;
    std::optional<QString> source;
    std::optional<QString> issues;
    QString componentApiVersion;
    ComponentRuntime runtime;
    std::optional<QString> settingsSchema;
    QVector<CapabilityRequest> requestedCapabilities;
    QVector<ComponentDependency> dependencies;

    friend bool operator==(
        const ComponentManifest &,
        const ComponentManifest &
    ) = default;
};

[[nodiscard]] QString toString(ComponentOrigin origin);
[[nodiscard]] QString toString(ComponentType type);
[[nodiscard]] QString toString(RuntimeKind kind);

[[nodiscard]] std::optional<ComponentType> componentTypeFromString(
    const QString &value
);
[[nodiscard]] std::optional<RuntimeKind> runtimeKindFromString(
    const QString &value
);

[[nodiscard]] bool isValidComponentId(const QString &id);
[[nodiscard]] bool isValidCapabilityId(const QString &id);
[[nodiscard]] bool isReservedBuiltinId(const QString &id);
[[nodiscard]] bool isStrictSemanticVersion(const QString &version);

[[nodiscard]] ValidationResult<ComponentManifest> parseComponentManifest(
    QByteArrayView bytes,
    ComponentOrigin origin
);

// The current host supports one protected built-in factory and the deliberately
// data-only declarative-v1 bar runtime. Structural parsing remains separate so
// reserved executable runtimes do not become usable by implication.
[[nodiscard]] ValidationErrors validateCurrentHostSupport(
    const ComponentManifest &manifest
);

} // namespace HyprShelld::Components
