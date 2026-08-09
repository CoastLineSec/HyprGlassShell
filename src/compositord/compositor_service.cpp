#include "compositor_service.h"

#include <QDBusMessage>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <utility>

namespace HyprShelld::Compositor {
namespace {

const QString interfaceName = QStringLiteral("org.hyprshelld.Compositor1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Compositor1");
const QString errorPrefix = QStringLiteral("org.hyprshelld.Compositor1.Error.");

QString
requiredActivationName(const std::optional<ActivationRequirement> requirement) {
  if (!requirement.has_value()) {
    return QStringLiteral("none");
  }
  switch (*requirement) {
  case ActivationRequirement::None:
    return QStringLiteral("none");
  case ActivationRequirement::Reload:
    return QStringLiteral("reload");
  case ActivationRequirement::Restart:
    return QStringLiteral("restart");
  case ActivationRequirement::Session:
    return QStringLiteral("session");
  }
  Q_UNREACHABLE_RETURN(QString());
}

QString activationNonce() {
  auto value = QUuid::createUuid().toString(QUuid::WithoutBraces);
  value.remove(QLatin1Char('-'));
  return value;
}

ManagementStatus authorityBoundManagement(
    const AuthoritySnapshot &authority, ManagementStatus management) {
  const auto exactManagedAuthority =
      authority.available && !authority.generationDigest.isEmpty() &&
      management.state == ManagementState::Managed &&
      management.managedGeneration == authority.generationDigest;
  const auto inactiveUnmanagedAuthority =
      authority.available && authority.generationDigest.isEmpty() &&
      management.state == ManagementState::Unmanaged;
  if (!exactManagedAuthority && !inactiveUnmanagedAuthority &&
      management.state != ManagementState::Conflict) {
    management.state = ManagementState::Conflict;
    management.managedGeneration.clear();
    management.managedNonce.clear();
  }
  return management;
}

} // namespace

CompositorService::CompositorService(
    std::unique_ptr<ActivationBackend> activationBackend,
    QDBusConnection connection, QObject *parent)
    : QObject(parent), activationBackend_(std::move(activationBackend)),
      connection_(std::move(connection)) {
  Q_ASSERT(activationBackend_);
  managementPollTimer_.setInterval(1000);
  managementPollTimer_.setSingleShot(false);
  connect(&managementPollTimer_, &QTimer::timeout, this,
          &CompositorService::refreshManagementStatus);
  connect(&managementWatcher_, &QFileSystemWatcher::fileChanged, this,
          [this] { refreshManagementStatus(); });
  connect(&managementWatcher_, &QFileSystemWatcher::directoryChanged, this,
          [this] { refreshManagementStatus(); });
}

bool CompositorService::initializeAuthority(
    std::unique_ptr<ConfigurationAuthority> authority, QString &error) {
  if (authority_) {
    error = QStringLiteral("The compositor authority was already initialized");
    return false;
  }
  if (!authority) {
    error = QStringLiteral("No compositor authority was provided");
    return false;
  }

  authority_ = std::move(authority);
  const auto initialized = authority_->initialize();
  if (!initialized.success || !initialized.snapshot.available) {
    error = initialized.errorMessage.isEmpty()
                ? QStringLiteral("The compositor authority is unavailable")
                : initialized.errorMessage;
    return false;
  }

  BackendResult startup;
  auto filesystem = authority_->duplicateActivationFilesystemContext();
  if (!filesystem.success) {
    startup = {
        .success = false,
        .errorCode = filesystem.errorCode,
        .errorMessage = filesystem.errorMessage,
        .status = activationBackend_->status(),
    };
  } else {
    ActivationFilesystemContext context;
    if (filesystem.context.has_value()) {
      context = std::move(*filesystem.context);
    }
    startup = activationBackend_->bindFilesystemContext(std::move(context));
    if (startup.success) {
      startup = activationBackend_->reconcileStartup(
          initialized.snapshot.generationDigest);
    }
  }
  if (!startup.success) {
    auto unavailable = initialized.snapshot;
    unavailable.available = false;
    unavailable.writable = false;
    unavailable.loadState = QStringLiteral("unavailable");
    unavailable.applyState = QStringLiteral("failed");
    auto conflict = startup.status;
    conflict.state = ManagementState::Conflict;
    acceptState(unavailable, conflict);
  } else {
    acceptState(initialized.snapshot, startup.status);
  }
  configureManagementMonitoring();
  return true;
}

bool CompositorService::available() const { return snapshot_.available; }

bool CompositorService::writable() const {
  return snapshot_.available && snapshot_.writable;
}

qulonglong CompositorService::revision() const {
  return snapshot_.available ? snapshot_.revision : 0;
}

QString CompositorService::loadState() const { return snapshot_.loadState; }

QString CompositorService::managementState() const {
  return managementStateName(management_.state);
}

QString CompositorService::entrypointDigest() const {
  return management_.entrypointDigest;
}

QString CompositorService::catalogDigest() const {
  return snapshot_.catalogDigest;
}

QString CompositorService::actionCatalogDigest() const {
  return snapshot_.actionCatalogDigest;
}

qulonglong CompositorService::appliedRevision() const {
  return snapshot_.appliedRevision;
}

QString CompositorService::applyState() const { return snapshot_.applyState; }

QString CompositorService::requiredActivation() const {
  return requiredActivationName(snapshot_.requiredActivation);
}

QString CompositorService::generationDigest() const {
  return snapshot_.generationDigest;
}

QByteArray
CompositorService::GetSnapshot(qulonglong &snapshotRevision,
                               QString &snapshotCatalogDigest,
                               QString &snapshotActionCatalogDigest) const {
  if (!snapshot_.available) {
    snapshotRevision = 0;
    snapshotCatalogDigest.clear();
    snapshotActionCatalogDigest.clear();
    reportError(QStringLiteral("Unavailable"),
                QStringLiteral("Compositor configuration is unavailable"));
    return {};
  }
  snapshotRevision = snapshot_.revision;
  snapshotCatalogDigest = snapshot_.catalogDigest;
  snapshotActionCatalogDigest = snapshot_.actionCatalogDigest;
  return snapshot_.desiredState;
}

qulonglong
CompositorService::ReplaceSnapshot(const qulonglong expectedRevision,
                                   const QString &expectedCatalogDigest,
                                   const QString &expectedActionCatalogDigest,
                                   const QByteArray &candidateSnapshot) {
  // The current token and the immediately preceding token are delegated for
  // Replace. The authority distinguishes an exact lost-response retry from a
  // conflicting candidate. Other mutating methods require only the current
  // revision at this D-Bus boundary.
  if (!checkMutationCatalogAuthority(expectedCatalogDigest,
                                     expectedActionCatalogDigest)) {
    return revision();
  }
  const auto immediatelyPrecedesCurrent =
      snapshot_.revision > 0 && expectedRevision == snapshot_.revision - 1;
  if (expectedRevision != snapshot_.revision && !immediatelyPrecedesCurrent) {
    reportError(
        QStringLiteral("StaleRevision"),
        QStringLiteral("Compositor configuration changed; read it again"));
    return revision();
  }

  auto result =
      authority_->replaceSnapshot(expectedRevision, candidateSnapshot);
  if (!result.success) {
    // Most rejected candidates leave the current authority tuple intact.
    // A post-publication durability failure instead marks that authority
    // unavailable; publish the fail-closed tuple before returning so a
    // client cannot continue from the stale writable revision.
    if (result.snapshot.available ||
        result.snapshot.applyState == QStringLiteral("failed")) {
      acceptState(result.snapshot, activationBackend_->status());
    }
    reportError(
        boundedErrorCode(result.errorCode, QStringLiteral("PersistenceFailed")),
        result.errorMessage);
    return revision();
  }
  acceptState(result.snapshot, activationBackend_->status());
  return revision();
}

qulonglong CompositorService::Apply(const qulonglong expectedRevision,
                                    const QString &expectedCatalogDigest,
                                    const QString &expectedActionCatalogDigest,
                                    QString &appliedGenerationDigest) {
  appliedGenerationDigest = snapshot_.generationDigest;
  if (!checkMutationAuthority(expectedRevision, expectedCatalogDigest,
                              expectedActionCatalogDigest)) {
    return appliedRevision();
  }

  const auto liveManagement = activationBackend_->status();
  if (liveManagement != management_) {
    acceptState(snapshot_, liveManagement);
  }
  if (management_.state == ManagementState::Unmanaged) {
    reportError(
        QStringLiteral("AdoptionRequired"),
        QStringLiteral("Explicit compositor entrypoint adoption is required"));
    return appliedRevision();
  }
  if (management_.state != ManagementState::Managed) {
    reportError(QStringLiteral("EntrypointChanged"),
                QStringLiteral("The managed compositor entrypoint changed"));
    return appliedRevision();
  }
  if (!snapshot_.requiredActivation.has_value() ||
      *snapshot_.requiredActivation == ActivationRequirement::None) {
    if (snapshot_.applyState == QStringLiteral("current") &&
        !snapshot_.generationDigest.isEmpty()) {
      return appliedRevision();
    }
    reportError(QStringLiteral("VerificationFailed"),
                QStringLiteral("The applied compositor tuple is inconsistent"));
    return appliedRevision();
  }
  if (!activationBackend_->canSatisfy(*snapshot_.requiredActivation)) {
    reportError(
        QStringLiteral("ActivationRequired"),
        QStringLiteral("The required live activation cannot be confirmed"));
    return appliedRevision();
  }

  auto completion = completePrepared(
      authority_->prepareApply(expectedRevision, activationNonce(),
                               QDateTime::currentDateTimeUtc()),
      false, {}, snapshot_.requiredActivation);
  if (!completion.success) {
    if (completion.snapshot.available ||
        completion.snapshot.applyState == QStringLiteral("failed")) {
      acceptState(completion.snapshot, completion.management);
    }
    reportError(completion.errorCode, completion.errorMessage);
    return appliedRevision();
  }
  acceptState(completion.snapshot, completion.management);
  appliedGenerationDigest = snapshot_.generationDigest;
  return appliedRevision();
}

qulonglong CompositorService::Recover(
    const qulonglong expectedRevision, const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    qulonglong &recoveredAppliedRevision, QString &recoveredGenerationDigest) {
  recoveredAppliedRevision = appliedRevision();
  recoveredGenerationDigest = snapshot_.generationDigest;
  if (!checkMutationAuthority(expectedRevision, expectedCatalogDigest,
                              expectedActionCatalogDigest)) {
    return revision();
  }

  const auto liveManagement = activationBackend_->status();
  if (liveManagement != management_) {
    acceptState(snapshot_, liveManagement);
  }
  if (management_.state == ManagementState::Unmanaged) {
    reportError(
        QStringLiteral("AdoptionRequired"),
        QStringLiteral("Recovery cannot adopt an unmanaged entrypoint"));
    return revision();
  }
  if (management_.state != ManagementState::Managed) {
    reportError(QStringLiteral("EntrypointChanged"),
                QStringLiteral("The managed compositor entrypoint changed"));
    return revision();
  }
  auto completion = completePrepared(
      authority_->prepareRecovery(expectedRevision, activationNonce(),
                                  QDateTime::currentDateTimeUtc()),
      false);
  if (!completion.success) {
    if (completion.snapshot.available ||
        completion.snapshot.applyState == QStringLiteral("failed")) {
      acceptState(completion.snapshot, completion.management);
    }
    const auto recoveryCode =
        completion.errorCode == QStringLiteral("ApplyFailed")
            ? QStringLiteral("RecoveryFailed")
            : boundedErrorCode(completion.errorCode,
                               QStringLiteral("RecoveryFailed"));
    reportError(recoveryCode, completion.errorMessage);
    return revision();
  }
  acceptState(completion.snapshot, completion.management);
  recoveredAppliedRevision = appliedRevision();
  recoveredGenerationDigest = snapshot_.generationDigest;
  return revision();
}

qulonglong CompositorService::AdoptManagedConfiguration(
    const qulonglong expectedRevision, const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const QString &expectedEntrypointDigest, QString &adoptedGenerationDigest,
    QString &adoptedEntrypointDigest) {
  adoptedGenerationDigest = snapshot_.generationDigest;
  adoptedEntrypointDigest = management_.entrypointDigest;
  if (!checkMutationAuthority(expectedRevision, expectedCatalogDigest,
                              expectedActionCatalogDigest)) {
    return appliedRevision();
  }

  const auto liveManagement = activationBackend_->status();
  if (liveManagement != management_) {
    acceptState(snapshot_, liveManagement);
  }
  const auto expectationMatches =
      expectedEntrypointDigest.isEmpty()
          ? management_.entrypointKind == EntrypointKind::Absent
          : management_.entrypointKind == EntrypointKind::Regular &&
                management_.entrypointDigest == expectedEntrypointDigest;
  if (management_.state != ManagementState::Unmanaged || !expectationMatches) {
    reportError(
        QStringLiteral("EntrypointChanged"),
        QStringLiteral(
            "The compositor entrypoint does not match the adoption proof"));
    return appliedRevision();
  }
  if (!snapshot_.requiredActivation.has_value() ||
      *snapshot_.requiredActivation == ActivationRequirement::None ||
      !activationBackend_->canSatisfy(*snapshot_.requiredActivation)) {
    reportError(QStringLiteral("ActivationRequired"),
                QStringLiteral("Managed entrypoint adoption is unavailable"));
    return appliedRevision();
  }

  auto completion = completePrepared(
      authority_->prepareApply(expectedRevision, activationNonce(),
                               QDateTime::currentDateTimeUtc()),
      true, expectedEntrypointDigest, snapshot_.requiredActivation);
  if (!completion.success) {
    if (completion.snapshot.available ||
        completion.snapshot.applyState == QStringLiteral("failed")) {
      acceptState(completion.snapshot, completion.management);
    }
    reportError(completion.errorCode, completion.errorMessage);
    return appliedRevision();
  }
  acceptState(completion.snapshot, completion.management);
  adoptedGenerationDigest = snapshot_.generationDigest;
  adoptedEntrypointDigest = management_.entrypointDigest;
  return appliedRevision();
}

bool CompositorService::checkMutationAuthority(
    const qulonglong expectedRevision, const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest) const {
  if (!checkMutationCatalogAuthority(expectedCatalogDigest,
                                     expectedActionCatalogDigest)) {
    return false;
  }
  if (expectedRevision != snapshot_.revision) {
    reportError(
        QStringLiteral("StaleRevision"),
        QStringLiteral("Compositor configuration changed; read it again"));
    return false;
  }
  return true;
}

bool CompositorService::checkMutationCatalogAuthority(
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest) const {
  if (!snapshot_.available || !authority_) {
    reportError(QStringLiteral("Unavailable"),
                QStringLiteral("Compositor configuration is unavailable"));
    return false;
  }
  if (!snapshot_.writable) {
    reportError(QStringLiteral("ReadOnly"),
                QStringLiteral("Compositor configuration is read-only"));
    return false;
  }
  if (expectedCatalogDigest != snapshot_.catalogDigest ||
      expectedActionCatalogDigest != snapshot_.actionCatalogDigest) {
    reportError(
        QStringLiteral("StaleCatalogDigest"),
        QStringLiteral("A compositor contract catalog changed; read it again"));
    return false;
  }
  return true;
}

CompositorService::Completion CompositorService::completePrepared(
    AuthorityResult prepared, const bool adoption,
    const QString &expectedEntrypointDigest,
    const std::optional<ActivationRequirement> expectedRequirement) {
  auto completion = Completion{
      .success = false,
      .snapshot = prepared.snapshot,
      .management = activationBackend_->status(),
  };
  if (!prepared.success) {
    completion.errorCode =
        boundedErrorCode(prepared.errorCode, QStringLiteral("ApplyFailed"));
    completion.errorMessage = prepared.errorMessage;
    return completion;
  }
  static const QRegularExpression digestExpression(
      QStringLiteral("^[0-9a-f]{64}$"));
  static const QRegularExpression nonceExpression(
      QStringLiteral("^[0-9a-f]{32,128}$"));
  QJsonObject preparedManifest;
  if (prepared.prepared.has_value() && !prepared.prepared->manifest.isEmpty() &&
      prepared.prepared->manifest.size() <=
          Hyprland::maximumDesiredStateBytes) {
    QJsonParseError parseError;
    const auto document =
        QJsonDocument::fromJson(prepared.prepared->manifest, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
      preparedManifest = document.object();
    }
  }
  const auto generationComplete =
      prepared.prepared.has_value() &&
      digestExpression.match(prepared.prepared->id).hasMatch() &&
      nonceExpression.match(prepared.prepared->nonce).hasMatch() &&
      digestExpression.match(prepared.prepared->snapshotDigest).hasMatch() &&
      QDir::isAbsolutePath(prepared.prepared->directory) &&
      QDir::cleanPath(prepared.prepared->directory) ==
          prepared.prepared->directory &&
      QFileInfo(prepared.prepared->directory).fileName() ==
          prepared.prepared->nonce &&
      QFileInfo(prepared.prepared->directory).dir().dirName() ==
          QStringLiteral("generations") &&
      QDir::isAbsolutePath(prepared.prepared->entrypoint) &&
      QDir::cleanPath(prepared.prepared->entrypoint) ==
          prepared.prepared->entrypoint &&
      prepared.prepared->entrypoint ==
          QDir(prepared.prepared->directory)
              .filePath(QStringLiteral("hyprland.lua")) &&
      !preparedManifest.isEmpty() &&
      preparedManifest.value(QStringLiteral("generation")).toString() ==
          prepared.prepared->id &&
      preparedManifest.value(QStringLiteral("activationNonce")).toString() ==
          prepared.prepared->nonce &&
      preparedManifest.value(QStringLiteral("snapshotDigest")).toString() ==
          prepared.prepared->snapshotDigest &&
      preparedManifest.value(QStringLiteral("revision")).toString() ==
          QString::number(prepared.prepared->revision) &&
      preparedManifest.value(QStringLiteral("entrypoint")).toString() ==
          QStringLiteral("hyprland.lua");
  if (!generationComplete) {
    completion.errorCode = QStringLiteral("VerificationFailed");
    completion.errorMessage =
        QStringLiteral("The prepared compositor generation is incomplete");
    completion.snapshot.available = false;
    completion.snapshot.writable = false;
    completion.snapshot.loadState = QStringLiteral("unavailable");
    completion.snapshot.applyState = QStringLiteral("failed");
    return completion;
  }
  const auto &generation = *prepared.prepared;
  const auto markUnreconciled = [&completion] {
    completion.snapshot.available = false;
    completion.snapshot.writable = false;
    completion.snapshot.loadState = QStringLiteral("unavailable");
    completion.snapshot.applyState = QStringLiteral("failed");
    completion.management.state = ManagementState::Conflict;
    completion.management.entrypointKind = EntrypointKind::Unsafe;
    completion.management.entrypointDigest.clear();
  };
  const auto abortPrepared = [this, &completion, &generation,
                              &markUnreconciled] {
    const auto aborted = authority_->abortApply(generation.id);
    completion.snapshot = aborted.snapshot;
    if (!aborted.success) {
      markUnreconciled();
      return false;
    }
    return true;
  };
  if (expectedRequirement.has_value() &&
      generation.requirement != *expectedRequirement) {
    const auto aborted = abortPrepared();
    completion.errorCode = aborted ? QStringLiteral("VerificationFailed")
                                   : QStringLiteral("ApplyFailed");
    completion.errorMessage =
        aborted
            ? QStringLiteral(
                  "The prepared activation requirement changed unexpectedly")
            : QStringLiteral(
                  "The inconsistent prepared generation could not be aborted");
    return completion;
  }
  if (!activationBackend_->canSatisfy(generation.requirement)) {
    const auto aborted = abortPrepared();
    completion.errorCode = QStringLiteral("ActivationRequired");
    completion.errorMessage =
        aborted
            ? QStringLiteral(
                  "The prepared generation requires an unsupported activation")
            : QStringLiteral(
                  "The unsupported activation could not be safely aborted");
    if (!aborted) {
      completion.errorCode = QStringLiteral("ApplyFailed");
    }
    return completion;
  }

  auto activated =
      adoption ? activationBackend_->adopt(generation, expectedEntrypointDigest)
               : activationBackend_->activate(generation);
  completion.management = activated.status;
  if (!activated.success) {
    if (activated.activationMayHaveOccurred) {
      const auto rolledBack = activationBackend_->rollback(activated.receipt);
      completion.management = rolledBack.status;
      if (!rolledBack.success) {
        markUnreconciled();
        completion.errorCode = QStringLiteral("ApplyFailed");
        completion.errorMessage = QStringLiteral(
            "Activation failed and live state could not be reconciled");
        return completion;
      }
    }
    if (!abortPrepared()) {
      completion.errorCode = QStringLiteral("ApplyFailed");
      completion.errorMessage = QStringLiteral(
          "Activation failed and its pending transaction could not be aborted");
      return completion;
    }
    completion.errorCode =
        boundedErrorCode(activated.errorCode, QStringLiteral("ApplyFailed"));
    completion.errorMessage = activated.errorMessage;
    return completion;
  }
  if (!activated.activationMayHaveOccurred ||
      activated.generation != generation.id ||
      activated.confirmedRequirement != generation.requirement) {
    const auto rolledBack = activationBackend_->rollback(activated.receipt);
    completion.management = rolledBack.status;
    if (!rolledBack.success) {
      markUnreconciled();
      completion.errorCode = QStringLiteral("ApplyFailed");
      completion.errorMessage = QStringLiteral(
          "A mismatched activation proof could not be rolled back");
      return completion;
    }
    if (!abortPrepared()) {
      completion.errorCode = QStringLiteral("ApplyFailed");
      completion.errorMessage = QStringLiteral(
          "A mismatched activation proof could not be reconciled");
      return completion;
    }
    completion.errorCode = QStringLiteral("VerificationFailed");
    completion.errorMessage = QStringLiteral(
        "The activation proof did not match the prepared generation");
    return completion;
  }

  auto committed = authority_->commitApply(generation.id);
  if (!committed.success) {
    completion.snapshot = committed.snapshot;
    if (!committed.commitDecisionMayExist && !committed.commitDecisionDurable) {
      // The committing marker was definitely not published. Restore the
      // prior live state before removing the still-abortable prepared
      // journal; either failure leaves the authority unavailable. A
      // published-but-not-directory-synced marker is intentionally not
      // rolled back because startup must reconcile that uncertain
      // one-way decision.
      const auto rolledBack = activationBackend_->rollback(activated.receipt);
      completion.management = rolledBack.status;
      if (!rolledBack.success) {
        markUnreconciled();
        completion.errorCode = QStringLiteral("ApplyFailed");
        completion.errorMessage =
            QStringLiteral("The commit decision failed and live state could "
                           "not be rolled back");
        return completion;
      }
      if (!abortPrepared()) {
        completion.errorCode = QStringLiteral("ApplyFailed");
        completion.errorMessage = QStringLiteral(
            "The rolled-back transaction could not be safely aborted");
        return completion;
      }
      completion.errorCode = QStringLiteral("ApplyFailed");
      completion.errorMessage =
          committed.errorMessage.isEmpty()
              ? QStringLiteral(
                    "The compositor commit decision could not be persisted")
              : committed.errorMessage;
      return completion;
    }

    // Once the authoritative one-way decision exists, rolling live state
    // back could diverge from startup roll-forward. Preserve the journal
    // and expose an explicit unavailable/conflict tuple instead.
    markUnreconciled();
    completion.errorCode = QStringLiteral("ApplyFailed");
    completion.errorMessage =
        committed.errorMessage.isEmpty()
            ? QStringLiteral(
                  "The confirmed generation requires startup reconciliation")
            : committed.errorMessage;
    return completion;
  }

  const auto finalized = activationBackend_->finalizeCommitted(
      activated.receipt, generation.id);
  completion.snapshot = committed.snapshot;
  completion.management = finalized.status;
  const auto finalizationMatchesAuthority =
      finalized.success && finalized.status.state == ManagementState::Managed &&
      finalized.status.managedGeneration == generation.id;
  if (!finalizationMatchesAuthority) {
    // The authority decision is already durable and therefore one-way. Keep
    // the backend's bridge for startup finalization and never contradict the
    // committed tuple by rolling live state back or aborting the transaction.
    markUnreconciled();
    completion.errorCode = QStringLiteral("ApplyFailed");
    completion.errorMessage =
        finalized.errorMessage.isEmpty()
            ? QStringLiteral("The committed compositor generation could not be "
                             "durably finalized")
            : finalized.errorMessage;
    return completion;
  }

  completion.success = true;
  return completion;
}

void CompositorService::acceptState(const AuthoritySnapshot &next,
                                    const ManagementStatus &nextManagement) {
  const auto boundManagement =
      authorityBoundManagement(next, nextManagement);
  QVariantMap changed;
  const auto insert = [&changed](const QString &name, const QVariant &value) {
    changed.insert(name, value);
  };

  const auto currentAvailable = available();
  const auto currentWritable = writable();
  const auto currentRevision = revision();
  const auto nextAvailable = next.available;
  const auto nextWritable = next.available && next.writable;
  const auto nextRevision = next.available ? next.revision : 0;

  if (nextAvailable != currentAvailable) {
    insert(QStringLiteral("Available"), next.available);
  }
  if (nextWritable != currentWritable) {
    insert(QStringLiteral("Writable"), nextWritable);
  }
  if (nextRevision != currentRevision) {
    insert(QStringLiteral("Revision"),
           QVariant::fromValue<qulonglong>(nextRevision));
  }
  if (next.loadState != snapshot_.loadState) {
    insert(QStringLiteral("LoadState"), next.loadState);
  }
  if (next.catalogDigest != snapshot_.catalogDigest) {
    insert(QStringLiteral("CatalogDigest"), next.catalogDigest);
  }
  if (next.actionCatalogDigest != snapshot_.actionCatalogDigest) {
    insert(QStringLiteral("ActionCatalogDigest"), next.actionCatalogDigest);
  }
  if (next.appliedRevision != snapshot_.appliedRevision) {
    insert(QStringLiteral("AppliedRevision"),
           QVariant::fromValue<qulonglong>(next.appliedRevision));
  }
  if (next.applyState != snapshot_.applyState) {
    insert(QStringLiteral("ApplyState"), next.applyState);
  }
  if (next.requiredActivation != snapshot_.requiredActivation) {
    insert(QStringLiteral("RequiredActivation"),
           requiredActivationName(next.requiredActivation));
  }
  if (next.generationDigest != snapshot_.generationDigest) {
    insert(QStringLiteral("GenerationDigest"), next.generationDigest);
  }
  if (boundManagement.state != management_.state) {
    insert(QStringLiteral("ManagementState"),
           managementStateName(boundManagement.state));
  }
  if (boundManagement.entrypointDigest != management_.entrypointDigest) {
    insert(QStringLiteral("EntrypointDigest"), boundManagement.entrypointDigest);
  }

  snapshot_ = next;
  management_ = boundManagement;
  publishProperties(changed);
}

void CompositorService::configureManagementMonitoring() {
  managementWatchPath_ = activationBackend_->managementWatchPath();
  if (managementWatchPath_.isEmpty()) {
    return;
  }
  managementWatchPath_ = QDir::cleanPath(managementWatchPath_);
  if (!QDir::isAbsolutePath(managementWatchPath_)) {
    managementWatchPath_.clear();
    return;
  }
  rearmManagementWatch();
  managementPollTimer_.start();
}

void CompositorService::rearmManagementWatch() {
  if (managementWatchPath_.isEmpty()) {
    return;
  }
  const auto addFile = [this](const QString &path) {
    if (QFileInfo(path).isFile() &&
        !managementWatcher_.files().contains(path)) {
      managementWatcher_.addPath(path);
    }
  };
  const auto addDirectory = [this](const QString &path) {
    if (QFileInfo(path).isDir() &&
        !managementWatcher_.directories().contains(path)) {
      managementWatcher_.addPath(path);
    }
  };

  addFile(managementWatchPath_);
  const auto configRoot = QFileInfo(managementWatchPath_).dir().absolutePath();
  addDirectory(configRoot);
  // Keep a surviving watch across replacement of the config root itself.
  addDirectory(QFileInfo(configRoot).dir().absolutePath());
}

void CompositorService::refreshManagementStatus() {
  if (!authority_) {
    return;
  }
  const auto live = authorityBoundManagement(
      snapshot_, activationBackend_->status());
  if (live != management_) {
    acceptState(snapshot_, live);
  }
  rearmManagementWatch();
}

void CompositorService::publishProperties(const QVariantMap &changed) {
  if (changed.isEmpty()) {
    return;
  }
  auto signal = QDBusMessage::createSignal(
      objectPath, QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("PropertiesChanged"));
  signal.setArguments({interfaceName, changed, QStringList()});
  connection_.send(signal);
  emit propertiesPublished(changed);
}

void CompositorService::reportError(const QString &code,
                                    const QString &message) const {
  if (calledFromDBus()) {
    sendErrorReply(
        errorPrefix + boundedErrorCode(code, QStringLiteral("ApplyFailed")),
        message.isEmpty() ? QStringLiteral("The operation failed") : message);
  }
}

QString CompositorService::boundedErrorCode(const QString &code,
                                            const QString &fallback) {
  static const QSet<QString> allowed{
      QStringLiteral("Unavailable"),
      QStringLiteral("ReadOnly"),
      QStringLiteral("StaleRevision"),
      QStringLiteral("StaleCatalogDigest"),
      QStringLiteral("InvalidSnapshot"),
      QStringLiteral("RevisionExhausted"),
      QStringLiteral("PersistenceFailed"),
      QStringLiteral("AdoptionRequired"),
      QStringLiteral("EntrypointChanged"),
      QStringLiteral("ActivationRequired"),
      QStringLiteral("VerificationFailed"),
      QStringLiteral("ReloadFailed"),
      QStringLiteral("ApplyFailed"),
      QStringLiteral("RecoveryUnavailable"),
      QStringLiteral("RecoveryFailed"),
  };
  return allowed.contains(code) ? code : fallback;
}

} // namespace HyprShelld::Compositor
