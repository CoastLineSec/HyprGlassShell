#pragma once

#include "activation_requirement.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/validation.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

namespace HyprShelld::Compositor {

inline constexpr quint32 currentRendererVersion = 1;
inline constexpr quint32 dormantGenerationV2FormatVersion = 2;
inline constexpr quint32 dormantGenerationV2ContractVersion = 2;
inline constexpr quint32 dormantRendererV2Version = 2;
inline constexpr auto managedWarningLine =
    "-- Managed by HyprShelld. Manual changes will be overwritten.";

struct GeneratedFile final {
    QString path;
    QByteArray contents;
    QString sha256;
    quint64 size = 0;

    friend bool operator==(const GeneratedFile &, const GeneratedFile &)
        = default;
};

struct RenderedGeneration final {
    QString generation;
    QString snapshotDigest;
    QString activationNonce;
    QString createdAt;
    QString entrypoint = QStringLiteral("hyprland.lua");
    QMap<QString, GeneratedFile> files;
    QJsonObject manifest;
    QByteArray manifestBytes;
    ActivationRequirement activationRequirement =
        ActivationRequirement::Reload;
};

struct RenderResult final {
    std::optional<RenderedGeneration> value;
    Hyprland::ValidationErrors errors;

    [[nodiscard]] explicit operator bool() const
    {
        return value.has_value() && errors.isEmpty();
    }
};

// Dormant qualification output only. Keeping this type separate from
// RenderedGeneration makes it ineligible for the active v1 GenerationStore.
struct DormantRenderedGenerationV2 final {
    QString authorityId;
    QString generation;
    QString snapshotDigest;
    QString sourceManifestDigest;
    QString activationNonce;
    QString createdAt;
    QString entrypoint = QStringLiteral("hyprland.lua");
    QMap<QString, GeneratedFile> files;
    QJsonObject manifest;
    QByteArray manifestBytes;
    ActivationRequirement activationRequirement =
        ActivationRequirement::Reload;
};

using DormantRenderResultV2 =
    Hyprland::ValidationResult<DormantRenderedGenerationV2>;

[[nodiscard]] QStringList managedModulePaths();

// All arguments that would otherwise make a manifest nondeterministic are
// explicit. Equal validated inputs produce byte-identical Lua and manifest
// bytes. The custom file is referenced by path but is never read or hashed.
[[nodiscard]] RenderResult renderGeneration(
    const Hyprland::DesiredState &state,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actionCatalog,
    const QString &generationRoot,
    const QString &userCustomPath,
    const QString &activationNonce,
    const QDateTime &createdAtUtc
);

// Builds a v2 authority-bound qualification artifact without selecting it for
// publication or changing the active v1 renderer contract.
[[nodiscard]] DormantRenderResultV2 renderDormantGenerationV2(
    const Hyprland::DesiredStateV2 &state,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2,
    const QString &generationRoot,
    const QString &userCustomPath,
    const QString &activationNonce,
    const QDateTime &createdAtUtc
);

} // namespace HyprShelld::Compositor
