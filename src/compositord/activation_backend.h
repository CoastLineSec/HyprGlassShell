#pragma once

#include "renderer.h"
#include "configuration_authority.h"

#include <QByteArray>
#include <QString>
#include <QStringView>

namespace HyprShelld::Compositor {

enum class ManagementState {
    Unmanaged,
    Managed,
    Conflict,
};

enum class EntrypointKind {
    Absent,
    Regular,
    Unsafe,
};

struct ManagementStatus final {
    ManagementState state = ManagementState::Unmanaged;
    EntrypointKind entrypointKind = EntrypointKind::Absent;
    QString entrypointDigest;

    friend bool operator==(const ManagementStatus &, const ManagementStatus &)
        = default;
};

struct ActivationReceipt final {
    QByteArray rollbackToken;
};

struct ActivationResult final {
    bool success = false;
    // True means rollback is mandatory even when success is false. A backend
    // must not return success until it observed the exact generation nonce's
    // config.reloaded event and an empty configerrors result.
    bool activationMayHaveOccurred = false;
    QString errorCode;
    QString errorMessage;
    QString generation;
    ActivationRequirement confirmedRequirement =
        ActivationRequirement::Reload;
    ActivationReceipt receipt;
    ManagementStatus status;
};

class ActivationBackend {
public:
    virtual ~ActivationBackend() = default;

    [[nodiscard]] virtual ManagementStatus status() const = 0;
    [[nodiscard]] virtual bool canSatisfy(
        ActivationRequirement requirement
    ) const = 0;

    // The implementation must independently re-probe the entrypoint. An empty
    // expected digest is an absence assertion, never a wildcard.
    [[nodiscard]] virtual ActivationResult adopt(
        const ActivationGeneration &prepared,
        QStringView expectedEntrypointDigest
    ) = 0;
    [[nodiscard]] virtual ActivationResult activate(
        const ActivationGeneration &prepared
    ) = 0;
    [[nodiscard]] virtual ActivationResult rollback(
        const ActivationReceipt &receipt
    ) = 0;
};

// Slice 2 deliberately keeps live Hyprland activation fail-closed until an
// executor can prove the exact config.reloaded nonce and empty configerrors.
// It still reports a bounded, no-follow view of the existing entrypoint so an
// absent file cannot be confused with an unsafe path.
class DeferredActivationBackend final : public ActivationBackend {
public:
    DeferredActivationBackend(QString configRoot, QString stableEntrypoint);

    [[nodiscard]] ManagementStatus status() const override;
    [[nodiscard]] bool canSatisfy(
        ActivationRequirement requirement
    ) const override;
    [[nodiscard]] ActivationResult adopt(
        const ActivationGeneration &prepared,
        QStringView expectedEntrypointDigest
    ) override;
    [[nodiscard]] ActivationResult activate(
        const ActivationGeneration &prepared
    ) override;
    [[nodiscard]] ActivationResult rollback(
        const ActivationReceipt &receipt
    ) override;

private:
    QString configRoot_;
    QString stableEntrypoint_;
};

[[nodiscard]] QString managementStateName(ManagementState state);

} // namespace HyprShelld::Compositor
