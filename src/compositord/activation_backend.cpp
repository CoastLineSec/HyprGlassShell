#include "activation_backend.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QScopeGuard>
#include <QSet>

#include <limits>
#include <cerrno>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr qsizetype maximumEntrypointBytes = 16 * 1024 * 1024;

[[nodiscard]] bool cleanAbsolute(const QString &path)
{
    return QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path;
}

[[nodiscard]] bool trustedDirectory(
    const struct stat &metadata,
    const uid_t rootOwner
)
{
    const auto trustedOwner = metadata.st_uid == ::geteuid()
        || metadata.st_uid == rootOwner;
    const auto writable = (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0;
    const auto protectedTemporary = metadata.st_uid == rootOwner
        && (metadata.st_mode & S_ISVTX) != 0;
    return S_ISDIR(metadata.st_mode) && trustedOwner
        && (!writable || protectedTemporary);
}

[[nodiscard]] int openTrustedDirectoryTree(const QString &path)
{
    if (!cleanAbsolute(path)) return -1;
    auto current = ::open(
        "/", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
    if (current < 0) return -1;
    struct stat root {};
    if (::fstat(current, &root) != 0 || !S_ISDIR(root.st_mode)) {
        ::close(current);
        return -1;
    }
    for (const auto &component : path.split(
             QLatin1Char('/'), Qt::SkipEmptyParts
         )) {
        const auto name = QFile::encodeName(component);
        struct stat named {};
        if (name.isEmpty() || name == "." || name == ".."
            || name.contains('/')
            || ::fstatat(current, name.constData(), &named,
                         AT_SYMLINK_NOFOLLOW) != 0
            || !trustedDirectory(named, root.st_uid)) {
            ::close(current);
            return -1;
        }
        const auto next = ::openat(
            current, name.constData(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        );
        struct stat opened {};
        if (next < 0 || ::fstat(next, &opened) != 0
            || opened.st_dev != named.st_dev
            || opened.st_ino != named.st_ino
            || opened.st_mode != named.st_mode
            || opened.st_uid != named.st_uid
            || opened.st_nlink != named.st_nlink) {
            if (next >= 0) ::close(next);
            ::close(current);
            return -1;
        }
        ::close(current);
        current = next;
    }
    return current;
}

enum class SafeFileKind { Missing, Regular, Unsafe };

struct SafeFile final {
    SafeFileKind kind = SafeFileKind::Missing;
    QByteArray bytes;
    QString digest;
};

[[nodiscard]] SafeFile readDeferredEntrypoint(
    const QString &configRoot,
    const QString &stableEntrypoint
)
{
    if (!cleanAbsolute(configRoot) || !cleanAbsolute(stableEntrypoint)
        || stableEntrypoint != QDir(configRoot).filePath(
            QStringLiteral("hyprland.lua")
        )) return {.kind = SafeFileKind::Unsafe};
    const auto configDirectory = openTrustedDirectoryTree(configRoot);
    if (configDirectory < 0) return {.kind = SafeFileKind::Unsafe};
    struct stat named {};
    constexpr auto name = "hyprland.lua";
    if (::fstatat(configDirectory, name, &named, AT_SYMLINK_NOFOLLOW) != 0) {
        const auto missing = errno == ENOENT;
        ::close(configDirectory);
        return missing ? SafeFile{} : SafeFile{
            .kind = SafeFileKind::Unsafe,
        };
    }
    if (!S_ISREG(named.st_mode) || named.st_uid != ::geteuid()
        || named.st_nlink != 1 || named.st_size < 0
        || named.st_size > maximumEntrypointBytes
        || (named.st_mode & (S_IWGRP | S_IWOTH | S_ISUID | S_ISGID
                            | S_ISVTX)) != 0) {
        ::close(configDirectory);
        return {.kind = SafeFileKind::Unsafe};
    }
    const auto descriptor = ::openat(
        configDirectory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    );
    if (descriptor < 0) {
        ::close(configDirectory);
        return {.kind = SafeFileKind::Unsafe};
    }
    QByteArray bytes;
    bytes.resize(static_cast<qsizetype>(named.st_size));
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::read(
            descriptor, bytes.data() + offset,
            static_cast<size_t>(bytes.size() - offset)
        );
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            ::close(descriptor);
            ::close(configDirectory);
            return {.kind = SafeFileKind::Unsafe};
        }
        offset += count;
    }
    struct stat opened {};
    struct stat finalNamed {};
    const auto exact = ::fstat(descriptor, &opened) == 0
        && ::fstatat(configDirectory, name, &finalNamed,
                     AT_SYMLINK_NOFOLLOW) == 0
        && opened.st_dev == named.st_dev && opened.st_ino == named.st_ino
        && opened.st_mode == named.st_mode && opened.st_uid == named.st_uid
        && opened.st_nlink == named.st_nlink
        && opened.st_size == named.st_size
        && finalNamed.st_dev == named.st_dev
        && finalNamed.st_ino == named.st_ino
        && finalNamed.st_mode == named.st_mode
        && finalNamed.st_uid == named.st_uid
        && finalNamed.st_nlink == named.st_nlink
        && finalNamed.st_size == named.st_size;
    ::close(descriptor);
    const auto canonical = openTrustedDirectoryTree(configRoot);
    struct stat retainedRoot {};
    struct stat canonicalRoot {};
    const auto rootExact = canonical >= 0
        && ::fstat(configDirectory, &retainedRoot) == 0
        && ::fstat(canonical, &canonicalRoot) == 0
        && retainedRoot.st_dev == canonicalRoot.st_dev
        && retainedRoot.st_ino == canonicalRoot.st_ino
        && retainedRoot.st_mode == canonicalRoot.st_mode
        && retainedRoot.st_uid == canonicalRoot.st_uid
        && retainedRoot.st_nlink == canonicalRoot.st_nlink;
    if (canonical >= 0) ::close(canonical);
    ::close(configDirectory);
    if (!exact || !rootExact) return {.kind = SafeFileKind::Unsafe};
    return {
        .kind = SafeFileKind::Regular,
        .bytes = bytes,
        .digest = QString::fromLatin1(
            QCryptographicHash::hash(
                bytes, QCryptographicHash::Sha256
            ).toHex()
        ),
    };
}

[[nodiscard]] ManagementStatus conflictStatus(const SafeFile &stable)
{
    return {
        .state = ManagementState::Conflict,
        .entrypointKind = stable.kind == SafeFileKind::Missing
            ? EntrypointKind::Absent
            : stable.kind == SafeFileKind::Regular
                ? EntrypointKind::Regular : EntrypointKind::Unsafe,
        .entrypointDigest = stable.kind == SafeFileKind::Regular
            ? stable.digest : QString(),
    };
}

[[nodiscard]] ActivationResult unavailableResult(
    const ManagementStatus &status
)
{
    return {
        .success = false,
        .errorCode = QStringLiteral("ActivationRequired"),
        .errorMessage = QStringLiteral(
            "Live Hyprland activation is unavailable in this service build"
        ),
        .status = status,
    };
}

[[nodiscard]] QSet<QString> objectKeys(const QJsonObject &object)
{
    QSet<QString> result;
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) result.insert(iterator.key());
    return result;
}

[[nodiscard]] bool parseUint32(const QJsonValue &value, quint32 &result)
{
    if (!value.isDouble()) return false;
    const auto integer = value.toInteger(-1);
    if (integer < 0
        || static_cast<quint64>(integer)
            > std::numeric_limits<quint32>::max()) return false;
    result = static_cast<quint32>(integer);
    return true;
}

[[nodiscard]] std::optional<HyprlandVersionPolicy> manifestPolicy(
    const ActivationGeneration &prepared
)
{
    const auto parsed = Hyprland::JsonSupport::parseStrictObject(
        prepared.manifest, 4 * 1024 * 1024, 32
    );
    if (!parsed) return std::nullopt;
    auto canonical = Hyprland::JsonSupport::canonicalJson(*parsed.value);
    canonical.append('\n');
    if (canonical != prepared.manifest) return std::nullopt;
    const auto compatibleValue = parsed.value->value(
        QStringLiteral("compatibleHyprland")
    );
    if (!compatibleValue.isObject()) return std::nullopt;
    const auto compatible = compatibleValue.toObject();
    static const QSet<QString> expected {
        QStringLiteral("major"), QStringLiteral("minor"),
        QStringLiteral("reviewedVersion"), QStringLiteral("minimumPatch"),
        QStringLiteral("maximumPatch"),
    };
    HyprlandVersionPolicy policy;
    if (objectKeys(compatible) != expected
        || !parseUint32(compatible.value(QStringLiteral("major")), policy.major)
        || !parseUint32(compatible.value(QStringLiteral("minor")), policy.minor)
        || !parseUint32(
            compatible.value(QStringLiteral("minimumPatch")),
            policy.minimumPatch
        )
        || compatible.value(QStringLiteral("reviewedVersion"))
            .toString().isEmpty()) return std::nullopt;
    const auto maximum = compatible.value(QStringLiteral("maximumPatch"));
    if (!maximum.isNull()) {
        quint32 patch = 0;
        if (!parseUint32(maximum, patch) || patch < policy.minimumPatch) {
            return std::nullopt;
        }
        policy.maximumPatch = patch;
    }
    return policy;
}

[[nodiscard]] bool samePolicy(
    const HyprlandVersionPolicy &left,
    const HyprlandVersionPolicy &right
)
{
    return left.major == right.major && left.minor == right.minor
        && left.minimumPatch == right.minimumPatch
        && left.maximumPatch == right.maximumPatch;
}

[[nodiscard]] ActivationResult activationFailure(
    const QString &code,
    const QString &message,
    const ManagementStatus &status,
    const bool mayHaveOccurred = false,
    const ActivationReceipt &receipt = {}
)
{
    return {
        .success = false,
        .activationMayHaveOccurred = mayHaveOccurred,
        .errorCode = code,
        .errorMessage = message,
        .receipt = receipt,
        .status = status,
    };
}

[[nodiscard]] BackendResult backendFailure(
    const QString &code,
    const QString &message,
    ManagementStatus status
)
{
    status.state = ManagementState::Conflict;
    status.managedGeneration.clear();
    status.managedNonce.clear();
    return {
        .success = false,
        .errorCode = code,
        .errorMessage = message,
        .status = status,
    };
}

} // namespace

BackendResult ActivationBackend::reconcileStartup(QStringView)
{
    return {.success = true, .status = status()};
}

DeferredActivationBackend::DeferredActivationBackend(
    QString configRoot,
    QString stableEntrypoint
)
    : configRoot_(std::move(configRoot))
    , stableEntrypoint_(std::move(stableEntrypoint))
{
}

ManagementStatus DeferredActivationBackend::status() const
{
    const auto stable = readDeferredEntrypoint(configRoot_, stableEntrypoint_);
    if (stable.kind == SafeFileKind::Unsafe) return conflictStatus(stable);
    return {
        .state = ManagementState::Unmanaged,
        .entrypointKind = stable.kind == SafeFileKind::Missing
            ? EntrypointKind::Absent : EntrypointKind::Regular,
        .entrypointDigest = stable.digest,
    };
}

bool DeferredActivationBackend::canSatisfy(ActivationRequirement) const
{
    return false;
}

ActivationResult DeferredActivationBackend::adopt(
    const ActivationGeneration &,
    QStringView
)
{
    return unavailableResult(status());
}

ActivationResult DeferredActivationBackend::activate(
    const ActivationGeneration &
)
{
    return unavailableResult(status());
}

ActivationResult DeferredActivationBackend::rollback(
    const ActivationReceipt &
)
{
    return unavailableResult(status());
}

LiveActivationBackend::LiveActivationBackend(
    std::unique_ptr<EntrypointPublisher> publisher,
    std::unique_ptr<HyprlandActivationRuntime> runtime
)
    : publisher_(std::move(publisher))
    , runtime_(std::move(runtime))
{
    Q_ASSERT(publisher_);
    Q_ASSERT(runtime_);
}

BackendResult LiveActivationBackend::bindFilesystemContext(
    ActivationFilesystemContext context
)
{
    if (filesystemBound_) {
        return backendFailure(
            QStringLiteral("PersistenceFailed"),
            QStringLiteral("The activation filesystem was already bound"),
            status()
        );
    }
    auto initialized = publisher_->initialize(std::move(context));
    filesystemBound_ = initialized.success;
    return initialized;
}

ManagementStatus LiveActivationBackend::status() const
{
    auto current = publisher_->status();
    const auto exactManaged = current.state == ManagementState::Managed
        && !committedGeneration_.isEmpty()
        && current.managedGeneration == committedGeneration_;
    const auto exactUnmanaged = current.state == ManagementState::Unmanaged
        && committedGeneration_.isEmpty();
    if (finalizationFailed_ || (!exactManaged && !exactUnmanaged
                               && current.state != ManagementState::Conflict)) {
        current.state = ManagementState::Conflict;
        current.managedGeneration.clear();
        current.managedNonce.clear();
    }
    return current;
}

bool LiveActivationBackend::canSatisfy(
    const ActivationRequirement requirement
) const
{
    return filesystemBound_ && versionPolicyConfigured_
        && !finalizationFailed_ && runtime_->canSatisfy(requirement);
}

void LiveActivationBackend::setVersionPolicy(HyprlandVersionPolicy policy)
{
    versionPolicy_ = policy;
    versionPolicyConfigured_ = true;
    runtime_->setVersionPolicy(policy);
}

QString LiveActivationBackend::managementWatchPath() const
{
    return publisher_->managementWatchPath();
}

BackendResult LiveActivationBackend::reconcileStartup(
    const QStringView committedGeneration
)
{
    if (!filesystemBound_ || !versionPolicyConfigured_) {
        return backendFailure(
            QStringLiteral("PersistenceFailed"),
            QStringLiteral(
                "The live backend lacks its authority filesystem or version policy"
            ), publisher_->status()
        );
    }
    const auto pending = publisher_->pendingReconciliation();
    if (!pending.success) {
        return backendFailure(
            pending.errorCode.isEmpty() ? QStringLiteral("PersistenceFailed")
                                        : pending.errorCode,
            pending.errorMessage, publisher_->status()
        );
    }
    if (!pending.value.has_value()) {
        committedGeneration_ = committedGeneration.toString();
        const auto current = status();
        const auto exact = committedGeneration_.isEmpty()
            ? current.state == ManagementState::Unmanaged
            : current.state == ManagementState::Managed
                && current.managedGeneration == committedGeneration_;
        if (!exact) {
            return backendFailure(
                QStringLiteral("EntrypointChanged"),
                QStringLiteral(
                    "The entrypoint ownership does not match authority state"
                ), current
            );
        }
        return {.success = true, .status = current};
    }

    const auto &bridge = *pending.value;
    if (bridge.targetGeneration == committedGeneration) {
        committedGeneration_ = committedGeneration.toString();
        const auto finalized = publisher_->finalize(bridge.receipt, true);
        if (!finalized.success) {
            finalizationFailed_ = true;
            return backendFailure(
                finalized.errorCode, finalized.errorMessage, finalized.status
            );
        }
        const auto current = status();
        if (current.state != ManagementState::Managed
            || current.managedGeneration != committedGeneration_) {
            return backendFailure(
                QStringLiteral("VerificationFailed"),
                QStringLiteral(
                    "Startup target finalization did not bind authority ownership"
                ), current
            );
        }
        return {.success = true, .status = current};
    }

    if (bridge.priorGeneration != committedGeneration) {
        return backendFailure(
            QStringLiteral("EntrypointChanged"),
            QStringLiteral(
                "Neither side of the activation bridge matches authority state"
            ), publisher_->status()
        );
    }
    committedGeneration_ = committedGeneration.toString();
    const auto rolledBack = rollback(bridge.receipt);
    if (!rolledBack.success) {
        return backendFailure(
            rolledBack.errorCode, rolledBack.errorMessage, rolledBack.status
        );
    }
    const auto current = status();
    const auto exact = committedGeneration_.isEmpty()
        ? current.state == ManagementState::Unmanaged
        : current.state == ManagementState::Managed
            && current.managedGeneration == committedGeneration_;
    if (!exact) {
        return backendFailure(
            QStringLiteral("VerificationFailed"),
            QStringLiteral(
                "Startup rollback did not restore authority-bound ownership"
            ), current
        );
    }
    return {.success = true, .status = current};
}

ActivationResult LiveActivationBackend::publishAndActivate(
    const ActivationGeneration &prepared,
    const bool adoption,
    const QStringView expectedEntrypointDigest
)
{
    if (!filesystemBound_ || finalizationFailed_) {
        return activationFailure(
            QStringLiteral("ApplyFailed"),
            QStringLiteral("The live activation backend is unavailable"),
            status()
        );
    }
    if (prepared.requirement != ActivationRequirement::Reload
        || !runtime_->canSatisfy(prepared.requirement)) {
        return activationFailure(
            QStringLiteral("ActivationRequired"),
            QStringLiteral("Only exact live reload activation is supported"),
            status()
        );
    }
    const auto policy = manifestPolicy(prepared);
    if (!policy || !versionPolicyConfigured_
        || !samePolicy(*policy, versionPolicy_)) {
        return activationFailure(
            QStringLiteral("VerificationFailed"),
            QStringLiteral(
                "The prepared generation has an unqualified Hyprland range"
            ), status()
        );
    }
    runtime_->setVersionPolicy(*policy);

    const auto current = status();
    const auto stateValid = adoption
        ? current.state == ManagementState::Unmanaged
            && committedGeneration_.isEmpty()
        : current.state == ManagementState::Managed
            && current.managedGeneration == committedGeneration_
            && !committedGeneration_.isEmpty();
    if (!stateValid) {
        return activationFailure(
            adoption ? QStringLiteral("EntrypointChanged")
                     : QStringLiteral("ApplyFailed"),
            QStringLiteral(
                "The entrypoint is not bound to the expected authority side"
            ), current
        );
    }

    const auto mode = adoption ? RuntimeActivationMode::AdoptionFullReset
                               : RuntimeActivationMode::ManagedReload;
    const auto preparedRuntime = runtime_->prepare(
        prepared.requirement, mode
    );
    if (!preparedRuntime.success || !preparedRuntime.session) {
        return activationFailure(
            preparedRuntime.errorCode.isEmpty()
                ? QStringLiteral("ReloadFailed")
                : preparedRuntime.errorCode,
            preparedRuntime.errorMessage, current
        );
    }
    const auto &session = *preparedRuntime.session;
    const auto cancel = qScopeGuard([this, &session] {
        runtime_->cancel(session);
    });
    const auto published = publisher_->publish(
        prepared, adoption, expectedEntrypointDigest,
        session.baselineConfigErrors, session.baselineProvider
    );
    if (!published.success) {
        const auto needsReconciliation =
            published.namespaceMayHaveChanged
            || !published.receipt.rollbackToken.isEmpty();
        return activationFailure(
            published.errorCode.isEmpty() ? QStringLiteral("ApplyFailed")
                                          : published.errorCode,
            published.errorMessage, published.status, needsReconciliation,
            published.receipt
        );
    }

    const auto proof = runtime_->reloadAndConfirm(
        session, prepared.nonce, QByteArrayLiteral("[]"),
        QStringLiteral("lua")
    );
    if (!proof.success) {
        return activationFailure(
            proof.errorCode.isEmpty() ? QStringLiteral("ReloadFailed")
                                      : proof.errorCode,
            proof.errorMessage, published.status, true, published.receipt
        );
    }
    const auto exact = publisher_->verifyTransition(
        published.receipt, true
    );
    if (!exact.success) {
        return activationFailure(
            exact.errorCode.isEmpty() ? QStringLiteral("VerificationFailed")
                                      : exact.errorCode,
            exact.errorMessage, exact.status, true, published.receipt
        );
    }
    return {
        .success = true,
        .activationMayHaveOccurred = true,
        .generation = prepared.id,
        .confirmedRequirement = prepared.requirement,
        .receipt = published.receipt,
        .status = exact.status,
    };
}

ActivationResult LiveActivationBackend::adopt(
    const ActivationGeneration &prepared,
    const QStringView expectedEntrypointDigest
)
{
    return publishAndActivate(prepared, true, expectedEntrypointDigest);
}

ActivationResult LiveActivationBackend::activate(
    const ActivationGeneration &prepared
)
{
    return publishAndActivate(prepared, false, {});
}

ActivationResult LiveActivationBackend::rollback(
    const ActivationReceipt &receipt
)
{
    if (!filesystemBound_ || receipt.rollbackToken.isEmpty()) {
        return activationFailure(
            QStringLiteral("ApplyFailed"),
            QStringLiteral("No authoritative rollback receipt is available"),
            status()
        );
    }
    const auto pending = publisher_->pendingReconciliation();
    if (!pending.success || !pending.value
        || pending.value->receipt.rollbackToken != receipt.rollbackToken) {
        return activationFailure(
            QStringLiteral("EntrypointChanged"),
            pending.errorMessage.isEmpty()
                ? QStringLiteral("The rollback bridge changed")
                : pending.errorMessage,
            publisher_->status()
        );
    }
    if (pending.value->priorGeneration != committedGeneration_) {
        return activationFailure(
            QStringLiteral("EntrypointChanged"),
            QStringLiteral(
                "The rollback prior side is not authority-bound"
            ), publisher_->status(), true, receipt
        );
    }
    const auto legacy = pending.value->priorNonce.isEmpty();
    const auto mode = legacy ? RuntimeActivationMode::LegacyRollback
                             : RuntimeActivationMode::ManagedRollback;
    const auto preparedRuntime = runtime_->prepare(
        ActivationRequirement::Reload, mode
    );
    if (!preparedRuntime.success || !preparedRuntime.session) {
        return activationFailure(
            preparedRuntime.errorCode.isEmpty()
                ? QStringLiteral("ReloadFailed")
                : preparedRuntime.errorCode,
            preparedRuntime.errorMessage, publisher_->status(), true, receipt
        );
    }
    const auto &session = *preparedRuntime.session;
    const auto cancel = qScopeGuard([this, &session] {
        runtime_->cancel(session);
    });
    const auto restored = publisher_->rollback(receipt);
    if (!restored.success) {
        return activationFailure(
            restored.errorCode.isEmpty() ? QStringLiteral("ReloadFailed")
                                         : restored.errorCode,
            restored.errorMessage, restored.status, true, receipt
        );
    }
    const auto proof = runtime_->reloadAndConfirm(
        session, restored.proofNonce, restored.baselineConfigErrors,
        restored.baselineProvider
    );
    if (!proof.success) {
        return activationFailure(
            proof.errorCode.isEmpty() ? QStringLiteral("ReloadFailed")
                                      : proof.errorCode,
            proof.errorMessage, restored.status, true, receipt
        );
    }
    const auto exact = publisher_->verifyTransition(receipt, false);
    if (!exact.success) {
        return activationFailure(
            exact.errorCode.isEmpty() ? QStringLiteral("VerificationFailed")
                                      : exact.errorCode,
            exact.errorMessage, exact.status, true, receipt
        );
    }
    const auto finalized = publisher_->finalize(receipt, false);
    if (!finalized.success) {
        return activationFailure(
            finalized.errorCode.isEmpty()
                ? QStringLiteral("PersistenceFailed")
                : finalized.errorCode,
            finalized.errorMessage, finalized.status, true, receipt
        );
    }
    return {
        .success = true,
        .activationMayHaveOccurred = false,
        .confirmedRequirement = ActivationRequirement::Reload,
        .status = finalized.status,
    };
}

BackendResult LiveActivationBackend::finalizeCommitted(
    const ActivationReceipt &receipt,
    const QStringView committedGeneration
)
{
    committedGeneration_ = committedGeneration.toString();
    if (receipt.rollbackToken.isEmpty() || committedGeneration_.isEmpty()) {
        finalizationFailed_ = true;
        return backendFailure(
            QStringLiteral("PersistenceFailed"),
            QStringLiteral("The committed activation receipt is incomplete"),
            publisher_->status()
        );
    }
    const auto pending = publisher_->pendingReconciliation();
    if (!pending.success || !pending.value
        || pending.value->receipt.rollbackToken != receipt.rollbackToken
        || pending.value->targetGeneration != committedGeneration_) {
        finalizationFailed_ = true;
        return backendFailure(
            pending.errorCode.isEmpty() ? QStringLiteral("VerificationFailed")
                                        : pending.errorCode,
            pending.errorMessage.isEmpty()
                ? QStringLiteral("The committed generation no longer matches the live bridge")
                : pending.errorMessage,
            publisher_->status()
        );
    }
    const auto finalized = publisher_->finalize(receipt, true);
    if (!finalized.success) {
        finalizationFailed_ = true;
        return backendFailure(
            finalized.errorCode, finalized.errorMessage, finalized.status
        );
    }
    const auto current = status();
    if (current.state != ManagementState::Managed
        || current.managedGeneration != committedGeneration_) {
        finalizationFailed_ = true;
        return backendFailure(
            QStringLiteral("VerificationFailed"),
            QStringLiteral(
                "The finalized live ownership is not authority-bound"
            ), current
        );
    }
    return {.success = true, .status = current};
}

QString managementStateName(const ManagementState state)
{
    switch (state) {
    case ManagementState::Unmanaged:
        return QStringLiteral("unmanaged");
    case ManagementState::Managed:
        return QStringLiteral("managed");
    case ManagementState::Conflict:
        return QStringLiteral("conflict");
    }
    Q_UNREACHABLE_RETURN(QString());
}

} // namespace HyprShelld::Compositor
