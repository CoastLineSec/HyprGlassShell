#pragma once

#include "renderer.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <memory>
#include <optional>

namespace HyprShelld::Compositor {

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
