#pragma once

#include "configuration_authority.h"
#include "generation.h"
#include "store.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"

#include <memory>

namespace HyprShelld::Compositor {

// Persistent desired/applied authority plus immutable generation transaction.
// Construction is side-effect free; initialize() owns the lifetime lease and
// is valid only after compositord owns its public D-Bus name.
class ConfigurationTransaction final : public ConfigurationAuthority {
public:
    ConfigurationTransaction(
        StorePaths paths,
        Hyprland::Catalog catalog,
        Hyprland::ActionCatalog actionCatalog
    );
    ~ConfigurationTransaction() override;

    ConfigurationTransaction(const ConfigurationTransaction &) = delete;
    ConfigurationTransaction &operator=(const ConfigurationTransaction &)
        = delete;

    [[nodiscard]] AuthorityResult initialize() override;
    [[nodiscard]] FilesystemContextResult
    duplicateActivationFilesystemContext() const override;
    [[nodiscard]] AuthoritySnapshot snapshot() const override;
    [[nodiscard]] QByteArray optionCatalog() const override;
    [[nodiscard]] AuthorityResult replaceSnapshot(
        quint64 expectedRevision,
        const QByteArray &candidate
    ) override;
    [[nodiscard]] AuthorityResult prepareApply(
        quint64 expectedRevision,
        const QString &activationNonce,
        const QDateTime &createdAtUtc
    ) override;
    [[nodiscard]] AuthorityResult prepareRecovery(
        quint64 expectedRevision,
        const QString &activationNonce,
        const QDateTime &createdAtUtc
    ) override;
    [[nodiscard]] AuthorityResult prepareDisplayApply(
        quint64 expectedRevision,
        const Hyprland::DisplayProfile &profile,
        const Hyprland::ConnectedDisplayTopology &topology,
        const QString &activationNonce,
        const QDateTime &createdAtUtc
    ) override;
    [[nodiscard]] AuthorityResult commitApply(
        const QString &generation
    ) override;
    [[nodiscard]] AuthorityResult abortApply(
        const QString &generation
    ) override;

    [[nodiscard]] GenerationResult verifyGeneration(
        const QString &activationNonce
    ) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace HyprShelld::Compositor
