#pragma once

#include "configuration_authority.h"
#include "renderer.h"

#include <QByteArray>
#include <QString>
#include <QStringView>

#include <memory>
#include <optional>
#include <functional>

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
    // Internal ownership tuple. Only entrypointDigest is exported on D-Bus.
    QString managedGeneration;
    QString managedNonce;

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
    // custom event, the following native configreloaded event, and an empty
    // configerrors result from the same compositor process.
    bool activationMayHaveOccurred = false;
    QString errorCode;
    QString errorMessage;
    QString generation;
    ActivationRequirement confirmedRequirement =
        ActivationRequirement::Reload;
    ActivationReceipt receipt;
    ManagementStatus status;
};

struct BackendResult final {
    bool success = false;
    QString errorCode;
    QString errorMessage;
    ManagementStatus status;
};

struct HyprlandVersionPolicy final {
    quint32 major = 0;
    quint32 minor = 0;
    quint32 minimumPatch = 0;
    std::optional<quint32> maximumPatch;
};

// Opaque, bounded event subscription prepared before an entrypoint namespace
// mutation. Implementations must pin all control connections to the same
// compositor UID/PID as this subscription.
struct RuntimeSession final {
    QByteArray token;
    QByteArray baselineConfigErrors;
    QString baselineProvider;
};

enum class RuntimeActivationMode {
    ManagedReload,
    AdoptionFullReset,
    ManagedRollback,
    LegacyRollback,
};

struct RuntimeSessionResult final {
    bool success = false;
    QString errorCode;
    QString errorMessage;
    std::optional<RuntimeSession> session;
};

struct RuntimeProofResult final {
    bool success = false;
    QString errorCode;
    QString errorMessage;
};

class HyprlandActivationRuntime {
public:
    virtual ~HyprlandActivationRuntime() = default;

    virtual void setVersionPolicy(HyprlandVersionPolicy policy) = 0;
    [[nodiscard]] virtual bool canSatisfy(
        ActivationRequirement requirement
    ) const = 0;
    [[nodiscard]] virtual RuntimeSessionResult prepare(
        ActivationRequirement requirement,
        RuntimeActivationMode mode
    ) = 0;
    // An empty nonce is used only for rollback of a pre-adoption user file;
    // it still requires the native configreloaded event and empty errors.
    [[nodiscard]] virtual RuntimeProofResult reloadAndConfirm(
        const RuntimeSession &session,
        QStringView exactNonce,
        QByteArrayView expectedConfigErrors,
        QStringView expectedProvider
    ) = 0;
    virtual void cancel(const RuntimeSession &session) noexcept = 0;
};

struct EntrypointPublishResult final {
    bool success = false;
    bool namespaceMayHaveChanged = false;
    QString errorCode;
    QString errorMessage;
    ActivationReceipt receipt;
    ManagementStatus status;
};

enum class EntrypointFaultPoint {
    BeforeOriginalEntrypointSync,
    AfterTargetFileSyncBeforeDirectorySync,
    BeforeJournalRename,
    AfterJournalRenameBeforeDirectorySync,
    BeforeReadyJournalRename,
    AfterReadyJournalRenameBeforeDirectorySync,
    BeforeEntrypointExchange,
    AfterEntrypointExchangeBeforeDirectorySync,
    BeforeOwnershipRename,
    AfterOwnershipRenameBeforeDirectorySync,
    BeforeBridgeRemoval,
    AfterBridgeRemovalBeforeDirectorySync,
};

struct EntrypointReconciliation final {
    bool pending = false;
    QString targetGeneration;
    QString priorGeneration;
    QString priorNonce;
    QByteArray baselineConfigErrors;
    QString baselineProvider;
    ActivationReceipt receipt;
};

struct EntrypointReconciliationResult final {
    bool success = false;
    QString errorCode;
    QString errorMessage;
    std::optional<EntrypointReconciliation> value;
};

struct EntrypointRollbackResult final {
    bool success = false;
    bool namespaceMayHaveChanged = false;
    QString errorCode;
    QString errorMessage;
    QString proofNonce;
    QByteArray baselineConfigErrors;
    QString baselineProvider;
    ManagementStatus status;
};

class EntrypointPublisher {
public:
    virtual ~EntrypointPublisher() = default;

    // The context contains duplicated, authority-owned root descriptors. It
    // is supplied only after the authority holds its lifetime store lease.
    [[nodiscard]] virtual BackendResult initialize(
        ActivationFilesystemContext context
    ) = 0;
    [[nodiscard]] virtual ManagementStatus status() const = 0;
    [[nodiscard]] virtual QString managementWatchPath() const = 0;
    [[nodiscard]] virtual EntrypointPublishResult publish(
        const ActivationGeneration &prepared,
        bool adoption,
        QStringView expectedEntrypointDigest,
        QByteArrayView baselineConfigErrors,
        QStringView baselineProvider
    ) = 0;
    [[nodiscard]] virtual EntrypointReconciliationResult
    pendingReconciliation() const = 0;
    [[nodiscard]] virtual EntrypointRollbackResult rollback(
        const ActivationReceipt &receipt
    ) = 0;
    [[nodiscard]] virtual BackendResult finalize(
        const ActivationReceipt &receipt,
        bool target
    ) = 0;
    [[nodiscard]] virtual BackendResult verifyTransition(
        const ActivationReceipt &receipt,
        bool target
    ) const = 0;
};

class ActivationBackend {
public:
    virtual ~ActivationBackend() = default;

    [[nodiscard]] virtual ManagementStatus status() const = 0;
    [[nodiscard]] virtual bool canSatisfy(
        ActivationRequirement requirement
    ) const = 0;

    virtual void setVersionPolicy(HyprlandVersionPolicy) {}
    [[nodiscard]] virtual QString managementWatchPath() const { return {}; }
    [[nodiscard]] virtual BackendResult bindFilesystemContext(
        ActivationFilesystemContext
    ) { return {.success = true, .status = status()}; }

    // Called only after the public D-Bus name and the authority's lifetime
    // store lease are held. The authority has already removed a Prepared
    // journal or rolled a Committing journal forward, so the committed
    // generation is the unambiguous crash-reconciliation decision.
    [[nodiscard]] virtual BackendResult reconcileStartup(
        QStringView committedGeneration
    );

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

    // Called only after the authority durably commits. A retained bridge is
    // safe and will be finalized on the next startup, so cleanup failure must
    // never trigger a contradictory live rollback.
    [[nodiscard]] virtual BackendResult finalizeCommitted(
        const ActivationReceipt &,
        QStringView
    ) { return {.success = true, .status = status()}; }
};

// The compatibility implementation remains available to narrow tests and
// recovery builds. It never publishes or reloads live state.
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

class AtomicEntrypointPublisher final : public EntrypointPublisher {
public:
    AtomicEntrypointPublisher(
        QString stateRoot,
        QString configRoot,
        QString managedConfigRoot,
        QString stableEntrypoint,
        QString ownershipRecord,
        std::function<bool(EntrypointFaultPoint)> faultHook = {}
    );
    ~AtomicEntrypointPublisher() override;

    [[nodiscard]] BackendResult initialize(
        ActivationFilesystemContext context
    ) override;
    [[nodiscard]] ManagementStatus status() const override;
    [[nodiscard]] QString managementWatchPath() const override;
    [[nodiscard]] EntrypointPublishResult publish(
        const ActivationGeneration &prepared,
        bool adoption,
        QStringView expectedEntrypointDigest,
        QByteArrayView baselineConfigErrors,
        QStringView baselineProvider
    ) override;
    [[nodiscard]] EntrypointReconciliationResult
    pendingReconciliation() const override;
    [[nodiscard]] EntrypointRollbackResult rollback(
        const ActivationReceipt &receipt
    ) override;
    [[nodiscard]] BackendResult finalize(
        const ActivationReceipt &receipt,
        bool target
    ) override;
    [[nodiscard]] BackendResult verifyTransition(
        const ActivationReceipt &receipt,
        bool target
    ) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class HyprlandIpcRuntime final : public HyprlandActivationRuntime {
public:
    struct InstanceSignatureResult final {
        bool success = false;
        QString signature;
        QString errorMessage;
    };
    using InstanceSignatureProvider = std::function<
        InstanceSignatureResult(int timeoutMilliseconds)
    >;

    HyprlandIpcRuntime(
        QString runtimeRoot,
        QString instanceSignature,
        QString stableEntrypoint,
        int timeoutMilliseconds = 12000,
        InstanceSignatureProvider instanceSignatureProvider = {}
    );
    ~HyprlandIpcRuntime() override;

    void setVersionPolicy(HyprlandVersionPolicy policy) override;
    [[nodiscard]] bool canSatisfy(
        ActivationRequirement requirement
    ) const override;
    [[nodiscard]] RuntimeSessionResult prepare(
        ActivationRequirement requirement,
        RuntimeActivationMode mode
    ) override;
    [[nodiscard]] RuntimeProofResult reloadAndConfirm(
        const RuntimeSession &session,
        QStringView exactNonce,
        QByteArrayView expectedConfigErrors,
        QStringView expectedProvider
    ) override;
    void cancel(const RuntimeSession &session) noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class LiveActivationBackend final : public ActivationBackend {
public:
    LiveActivationBackend(
        std::unique_ptr<EntrypointPublisher> publisher,
        std::unique_ptr<HyprlandActivationRuntime> runtime
    );

    [[nodiscard]] ManagementStatus status() const override;
    [[nodiscard]] bool canSatisfy(
        ActivationRequirement requirement
    ) const override;
    [[nodiscard]] BackendResult bindFilesystemContext(
        ActivationFilesystemContext context
    ) override;
    void setVersionPolicy(HyprlandVersionPolicy policy) override;
    [[nodiscard]] QString managementWatchPath() const override;
    [[nodiscard]] BackendResult reconcileStartup(
        QStringView committedGeneration
    ) override;
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
    [[nodiscard]] BackendResult finalizeCommitted(
        const ActivationReceipt &receipt,
        QStringView committedGeneration
    ) override;

private:
    [[nodiscard]] ActivationResult publishAndActivate(
        const ActivationGeneration &prepared,
        bool adoption,
        QStringView expectedEntrypointDigest
    );

    std::unique_ptr<EntrypointPublisher> publisher_;
    std::unique_ptr<HyprlandActivationRuntime> runtime_;
    HyprlandVersionPolicy versionPolicy_;
    bool versionPolicyConfigured_ = false;
    bool filesystemBound_ = false;
    QString committedGeneration_;
    bool finalizationFailed_ = false;
};

[[nodiscard]] QString managementStateName(ManagementState state);

} // namespace HyprShelld::Compositor
