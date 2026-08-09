#pragma once

#include "renderer.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <memory>
#include <optional>
#include <utility>

#include <unistd.h>

namespace HyprShelld::Compositor {

// Descriptor identity shared by the durable authority and the live
// entrypoint publisher. The authority duplicates these descriptors only after
// it owns its lifetime lease; the backend must operate through them and also
// verify that the canonical paths still resolve to the same inodes.
struct ActivationFilesystemContext final {
    int stateDirectoryFd = -1;
    int configDirectoryFd = -1;
    int managedDirectoryFd = -1;
    int generationsDirectoryFd = -1;
    QString stateRoot;
    QString configRoot;
    QString managedConfigRoot;
    QString stableEntrypoint;

    ActivationFilesystemContext() = default;
    ~ActivationFilesystemContext() { reset(); }

    ActivationFilesystemContext(const ActivationFilesystemContext &) = delete;
    ActivationFilesystemContext &operator=(
        const ActivationFilesystemContext &
    ) = delete;

    ActivationFilesystemContext(ActivationFilesystemContext &&other) noexcept
    {
        *this = std::move(other);
    }

    ActivationFilesystemContext &operator=(
        ActivationFilesystemContext &&other
    ) noexcept
    {
        if (this == &other) return *this;
        reset();
        stateDirectoryFd = std::exchange(other.stateDirectoryFd, -1);
        configDirectoryFd = std::exchange(other.configDirectoryFd, -1);
        managedDirectoryFd = std::exchange(other.managedDirectoryFd, -1);
        generationsDirectoryFd = std::exchange(
            other.generationsDirectoryFd, -1
        );
        stateRoot = std::move(other.stateRoot);
        configRoot = std::move(other.configRoot);
        managedConfigRoot = std::move(other.managedConfigRoot);
        stableEntrypoint = std::move(other.stableEntrypoint);
        return *this;
    }

    [[nodiscard]] bool complete() const
    {
        return stateDirectoryFd >= 0 && configDirectoryFd >= 0
            && managedDirectoryFd >= 0 && generationsDirectoryFd >= 0
            && !stateRoot.isEmpty() && !configRoot.isEmpty()
            && !managedConfigRoot.isEmpty() && !stableEntrypoint.isEmpty();
    }

    void reset() noexcept
    {
        if (generationsDirectoryFd >= 0) ::close(generationsDirectoryFd);
        if (managedDirectoryFd >= 0) ::close(managedDirectoryFd);
        if (configDirectoryFd >= 0) ::close(configDirectoryFd);
        if (stateDirectoryFd >= 0) ::close(stateDirectoryFd);
        generationsDirectoryFd = -1;
        managedDirectoryFd = -1;
        configDirectoryFd = -1;
        stateDirectoryFd = -1;
    }
};

struct FilesystemContextResult final {
    bool success = false;
    QString errorCode;
    QString errorMessage;
    std::optional<ActivationFilesystemContext> context;
};

struct AuthoritySnapshot final {
  bool available = false;
  bool writable = false;
  QByteArray desiredState;
  quint64 revision = 0;
  QString catalogDigest;
  QString actionCatalogDigest;
  QString loadState = QStringLiteral("unavailable");
  quint64 appliedRevision = 0;
  QString applyState = QStringLiteral("unavailable");
  std::optional<ActivationRequirement> requiredActivation;
  QString generationDigest;

  friend bool operator==(const AuthoritySnapshot &,
                         const AuthoritySnapshot &) = default;
};

struct ActivationGeneration final {
  QString id;
  QString nonce;
  QString snapshotDigest;
  quint64 revision = 0;
  QString directory;
  QString entrypoint;
  QByteArray manifest;
  ActivationRequirement requirement = ActivationRequirement::Reload;
};

struct AuthorityResult final {
  bool success = false;
  // commitApply sets this only after its one-way `committing` journal and
  // parent-directory sync are durable. True means startup reconciliation
  // must own every subsequent failure.
  bool commitDecisionDurable = false;
  // True once the committing rename may be visible, even if its directory
  // fsync failed. Rollback is safe only when this remains false.
  bool commitDecisionMayExist = false;
  QString errorCode;
  QString errorMessage;
  AuthoritySnapshot snapshot;
  std::optional<ActivationGeneration> prepared;
};

class ConfigurationAuthority {
public:
  virtual ~ConfigurationAuthority() = default;

  // This is the first call permitted to acquire a lease or touch persistent
  // state. main invokes it only after owning the public D-Bus name.
    [[nodiscard]] virtual AuthorityResult initialize() = 0;
    // Returns duplicated CLOEXEC descriptors for the exact roots retained by
    // this initialized authority. Test authorities and non-filesystem
    // implementations may return success with no context; a live backend will
    // then fail closed during startup reconciliation.
    [[nodiscard]] virtual FilesystemContextResult
    duplicateActivationFilesystemContext() const
    {
        return {.success = true};
    }
  [[nodiscard]] virtual AuthoritySnapshot snapshot() const = 0;
  [[nodiscard]] virtual AuthorityResult
  replaceSnapshot(quint64 expectedRevision, const QByteArray &candidate) = 0;
  [[nodiscard]] virtual AuthorityResult
  prepareApply(quint64 expectedRevision, const QString &activationNonce,
               const QDateTime &createdAtUtc) = 0;
  [[nodiscard]] virtual AuthorityResult
  prepareRecovery(quint64 expectedRevision, const QString &activationNonce,
                  const QDateTime &createdAtUtc) = 0;
  // prepareRecovery stages N+1 and its immutable generation without changing
  // authoritative desired/applied files. commitApply publishes both only
  // after an exact positive activation proof; abortApply leaves N unchanged.
  [[nodiscard]] virtual AuthorityResult
  commitApply(const QString &generation) = 0;
  [[nodiscard]] virtual AuthorityResult
  abortApply(const QString &generation) = 0;
};

} // namespace HyprShelld::Compositor
