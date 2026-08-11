#include "compositor_service.h"

#include "hyprland/catalog.h"

#include <QCryptographicHash>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <array>
#include <climits>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

const QString interfaceName = QStringLiteral("org.hyprshelld.Compositor1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Compositor1");
const QString errorPrefix = QStringLiteral("org.hyprshelld.Compositor1.Error.");
constexpr int displayOwnerProbeMaximumMilliseconds = 250;

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

QString displayConfirmationToken() {
  std::array<quint32, 4> randomWords{};
  QRandomGenerator::system()->fillRange(
      randomWords.data(), randomWords.size()
  );
  return QString::fromLatin1(
      QByteArray(
          reinterpret_cast<const char *>(randomWords.data()),
          static_cast<qsizetype>(sizeof(randomWords))
      ).toHex()
  );
}

ManagementStatus authorityBoundManagement(
    const AuthoritySnapshot &authority,
    ManagementStatus management,
    const QStringView previewGeneration = {}) {
  if (!previewGeneration.isEmpty()) {
    const auto exactPreview = authority.available &&
        management.state == ManagementState::Managed &&
        management.managedGeneration == previewGeneration;
    if (exactPreview) {
      management.state = ManagementState::Preview;
      return management;
    }
    management.state = ManagementState::Conflict;
    management.managedGeneration.clear();
    management.managedNonce.clear();
    return management;
  }
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
    QDBusConnection connection, QObject *parent,
    DisplayDeadlineRemaining displayDeadlineRemaining,
    DisplayOwnerPresent displayOwnerPresent,
    std::unique_ptr<SharedBorderSource> sharedBorderSource)
    : QObject(parent), activationBackend_(std::move(activationBackend)),
      sharedBorderSource_(std::move(sharedBorderSource)),
      connection_(std::move(connection)),
      displayDeadlineRemaining_(std::move(displayDeadlineRemaining)),
      displayOwnerPresent_(std::move(displayOwnerPresent)) {
  Q_ASSERT(activationBackend_);
  if (!sharedBorderSource_) {
    sharedBorderSource_ = std::make_unique<DbusSharedBorderSource>(connection_);
  }
  if (!displayDeadlineRemaining_) {
    displayDeadlineRemaining_ = [](const QDeadlineTimer &deadline) {
      return deadline.remainingTime();
    };
  }
  managementPollTimer_.setInterval(1000);
  managementPollTimer_.setSingleShot(false);
  displayConfirmationTimer_.setSingleShot(true);
  displayConfirmationTimer_.setTimerType(Qt::PreciseTimer);
  connect(&managementPollTimer_, &QTimer::timeout, this,
          &CompositorService::refreshManagementStatus);
  connect(&managementWatcher_, &QFileSystemWatcher::fileChanged, this,
          [this] { refreshManagementStatus(); });
  connect(&managementWatcher_, &QFileSystemWatcher::directoryChanged, this,
          [this] { refreshManagementStatus(); });
  connect(
      &displayConfirmationTimer_, &QTimer::timeout, this,
      &CompositorService::handleDisplayConfirmationTimeout
  );
  connect(
      sharedBorderSource_.get(), &SharedBorderSource::changed, this,
      &CompositorService::sharedBorderSourceChanged
  );
}

void CompositorService::handleDisplayConfirmationTimeout() {
  if (!displayConfirmation_) return;
  QString error;
  static_cast<void>(revertDisplayConfirmation(
      DisplayTerminalAction::Expired, error
  ));
}

void CompositorService::handleDisplayOwnerLoss(const QString &owner) {
  if (!displayConfirmation_ || displayConfirmation_->owner != owner) return;
  QString error;
  static_cast<void>(revertDisplayConfirmation(
      DisplayTerminalAction::Reverted, error
  ));
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
  sharedBorderSource_->start();
  scheduleSharedBorderReconcile();
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

QString CompositorService::displayConfirmationState() const {
  return displayConfirmationState_;
}

qulonglong CompositorService::displayConfirmationRevision() const {
  return displayConfirmation_
          && displayConfirmationState_ == QStringLiteral("awaiting-confirmation")
      ? displayConfirmation_->previewRevision : 0;
}

qulonglong CompositorService::displayConfirmationDeadlineMs() const {
  return displayConfirmation_
          && displayConfirmationState_ == QStringLiteral("awaiting-confirmation")
      ? displayConfirmation_->deadlineMs : 0;
}

QString CompositorService::displayConfirmationGeneration() const {
  return displayConfirmation_
          && displayConfirmationState_ == QStringLiteral("awaiting-confirmation")
      ? displayConfirmation_->generation : QString();
}

QString CompositorService::sharedBorderSyncState() const {
  return sharedBorderSyncState_;
}

qulonglong CompositorService::sharedBorderSourceRevision() const {
  return sharedBorderSourceRevision_;
}

QString CompositorService::sharedBorderSyncError() const {
  return sharedBorderSyncError_;
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

QByteArray CompositorService::GetOptionCatalog(
    QString &optionCatalogDigest
) const {
  optionCatalogDigest.clear();
  if (!snapshot_.available || !authority_) {
    reportError(QStringLiteral("Unavailable"),
                QStringLiteral("Compositor configuration is unavailable"));
    return {};
  }

  const auto catalog = authority_->optionCatalog();
  if (catalog.isEmpty() || catalog.size() > Hyprland::maximumCatalogBytes) {
    reportError(
        QStringLiteral("Unavailable"),
        QStringLiteral("The compositor option catalog is unavailable"));
    return {};
  }
  const auto digest = QString::fromLatin1(
      QCryptographicHash::hash(catalog, QCryptographicHash::Sha256).toHex());
  if (digest != snapshot_.catalogDigest) {
    reportError(
        QStringLiteral("Unavailable"),
        QStringLiteral("The compositor option catalog is unavailable"));
    return {};
  }

  optionCatalogDigest = snapshot_.catalogDigest;
  return catalog;
}

qulonglong
CompositorService::ReplaceSnapshot(const qulonglong expectedRevision,
                                   const QString &expectedCatalogDigest,
                                   const QString &expectedActionCatalogDigest,
                                   const QByteArray &candidateSnapshot) {
  if (!checkNoDisplayConfirmation()) return revision();
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
  QString controlledError;
  if (!sharedBorderReplacementAllowed(candidateSnapshot, controlledError)) {
    reportError(
        QStringLiteral("ControlledByHyprShelld"),
        controlledError.isEmpty()
            ? QStringLiteral(
                  "Window border geometry is controlled by shared visual settings"
              )
            : controlledError
    );
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
  if (!checkNoDisplayConfirmation()) return appliedRevision();
  if (!checkMutationAuthority(expectedRevision, expectedCatalogDigest,
                              expectedActionCatalogDigest)) {
    return appliedRevision();
  }
  QString controlledError;
  if (!sharedBorderActivationAllowed(snapshot_.desiredState,
                                     controlledError)) {
    reportError(
        QStringLiteral("ControlledByHyprShelld"),
        controlledError.isEmpty()
            ? QStringLiteral(
                  "Window border geometry is controlled by shared visual settings"
              )
            : controlledError
    );
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
  if (!checkNoDisplayConfirmation()) return revision();
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
  if (!checkNoDisplayConfirmation()) return appliedRevision();
  if (!checkMutationAuthority(expectedRevision, expectedCatalogDigest,
                              expectedActionCatalogDigest)) {
    return appliedRevision();
  }
  QString controlledError;
  if (!sharedBorderActivationAllowed(snapshot_.desiredState,
                                     controlledError)) {
    reportError(
        QStringLiteral("ControlledByHyprShelld"),
        controlledError.isEmpty()
            ? QStringLiteral(
                  "Window border geometry is controlled by shared visual settings"
              )
            : controlledError
    );
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

QByteArray CompositorService::GetConnectedDisplays(
    qulonglong &observedAtMs
) {
  observedAtMs = 0;
  if (displayConfirmation_
      && displayConfirmationState_ == QStringLiteral("awaiting-confirmation")) {
    observedAtMs = displayConfirmation_->cachedTopologyObservedAtMs;
    return displayConfirmation_->cachedTopologyDocument;
  }
  if (displayConfirmation_) {
    reportError(
        QStringLiteral("Unavailable"),
        QStringLiteral("Display discovery is unavailable during reconciliation")
    );
    return {};
  }
  if (!snapshot_.available || !authority_) {
    reportError(QStringLiteral("Unavailable"),
                QStringLiteral("Compositor configuration is unavailable"));
    return {};
  }
  const auto connected = activationBackend_->connectedDisplays();
  if (!connected.success || !connected.topology) {
    reportError(
        boundedErrorCode(connected.errorCode,
                         QStringLiteral("RuntimeUnavailable")),
        connected.errorMessage
    );
    return {};
  }
  observedAtMs = static_cast<qulonglong>(
      QDateTime::currentMSecsSinceEpoch()
  );
  return connected.topology->document;
}

qulonglong CompositorService::PreviewDisplayConfiguration(
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QString &expectedActionCatalogDigest,
    const QByteArray &profileBytes,
    const uint timeoutSeconds,
    QString &confirmationToken,
    qulonglong &deadlineMs,
    QString &previewGenerationDigest
) {
  confirmationToken.clear();
  deadlineMs = 0;
  previewGenerationDigest.clear();
  if (!checkNoDisplayConfirmation()) return revision();
  if (timeoutSeconds < 10 || timeoutSeconds > 30) {
    reportError(QStringLiteral("InvalidDisplayProfile"),
                QStringLiteral("Display confirmation must last 10 to 30 seconds"));
    return revision();
  }
  const auto owner = callerIdentity();
  if (owner.isEmpty()) {
    reportError(QStringLiteral("InvalidCaller"),
                QStringLiteral("The display preview caller is unavailable"));
    return revision();
  }
  if (!checkMutationAuthority(expectedRevision, expectedCatalogDigest,
                              expectedActionCatalogDigest)) {
    return revision();
  }

  const auto liveManagement = activationBackend_->status();
  if (liveManagement != management_) acceptState(snapshot_, liveManagement);
  const auto exactBaseline = management_.state == ManagementState::Managed
      && snapshot_.applyState == QStringLiteral("current")
      && snapshot_.revision == snapshot_.appliedRevision
      && !snapshot_.generationDigest.isEmpty()
      && !snapshot_.requiredActivation.has_value();
  if (!exactBaseline) {
    reportError(
        QStringLiteral("DisplayScopeConflict"),
        QStringLiteral(
            "A display preview requires the exact current managed baseline"
        )
    );
    return revision();
  }
  const auto profile = Hyprland::parseDisplayProfile(
      QByteArrayView(profileBytes)
  );
  if (!profile) {
    reportError(QStringLiteral("InvalidDisplayProfile"),
                QStringLiteral("The display profile is invalid or non-canonical"));
    return revision();
  }
  const auto beforeTopology = activationBackend_->connectedDisplays();
  if (!beforeTopology.success || !beforeTopology.topology
      || beforeTopology.runtimeIdentity.isEmpty()) {
    reportError(
        boundedErrorCode(beforeTopology.errorCode,
                         QStringLiteral("RuntimeUnavailable")),
        beforeTopology.errorMessage
    );
    return revision();
  }
  const auto topologyErrors = Hyprland::validateDisplayProfileTopology(
      *profile.value, *beforeTopology.topology
  );
  if (!topologyErrors.isEmpty()) {
    reportError(QStringLiteral("DisplayTopologyChanged"),
                topologyErrors.front().message);
    return revision();
  }

  auto prepared = authority_->prepareDisplayApply(
      expectedRevision, *profile.value, *beforeTopology.topology,
      activationNonce(), QDateTime::currentDateTimeUtc()
  );
  QString verificationError;
  if (!prepared.success) {
    if (!prepared.snapshot.available
        || prepared.snapshot.applyState == QStringLiteral("failed")) {
      acceptUnreconciled(
          prepared.snapshot, activationBackend_->status()
      );
    } else if (prepared.snapshot != snapshot_) {
      acceptState(prepared.snapshot, activationBackend_->status());
    }
    reportError(
        boundedErrorCode(prepared.errorCode,
                         QStringLiteral("InvalidDisplayProfile")),
        prepared.errorMessage
    );
    return revision();
  }
  if (!validatePreparedGeneration(prepared, verificationError)) {
    AuthorityResult aborted;
    const auto canAbort = prepared.prepared.has_value();
    if (canAbort) {
      aborted = authority_->abortApply(prepared.prepared->id);
    }
    if (canAbort && aborted.success) {
      acceptState(aborted.snapshot, activationBackend_->status());
    } else {
      acceptUnreconciled(
          canAbort ? aborted.snapshot : prepared.snapshot,
          activationBackend_->status()
      );
    }
    reportError(
        canAbort && aborted.success
            ? QStringLiteral("VerificationFailed")
            : QStringLiteral("ApplyFailed"),
        canAbort && !aborted.success && !aborted.errorMessage.isEmpty()
            ? aborted.errorMessage : verificationError
    );
    return revision();
  }
  const auto generation = *prepared.prepared;
  const auto abortPrepared = [this, &generation]() {
    return authority_->abortApply(generation.id);
  };
  if (generation.requirement != ActivationRequirement::Reload
      || !activationBackend_->canSatisfy(generation.requirement)) {
    const auto aborted = abortPrepared();
    if (aborted.success) {
      acceptState(aborted.snapshot, activationBackend_->status());
    } else {
      acceptUnreconciled(aborted.snapshot, activationBackend_->status());
    }
    reportError(
        aborted.success ? QStringLiteral("ActivationRequired")
                        : QStringLiteral("ApplyFailed"),
        aborted.success
            ? QStringLiteral("Display preview requires exact reload activation")
            : aborted.errorMessage
    );
    return revision();
  }

  auto activated = activationBackend_->activate(generation);
  struct PreviewCleanup final {
    bool success = false;
    AuthoritySnapshot snapshot;
    ManagementStatus management;
    QString error;
  };
  const auto rollbackAndAbort = [this, &activated, &abortPrepared]() {
    PreviewCleanup cleanup{
        .snapshot = snapshot_,
        .management = activated.status,
    };
    if (activated.activationMayHaveOccurred) {
      const auto rolledBack = activationBackend_->rollback(activated.receipt);
      cleanup.management = rolledBack.status;
      if (!rolledBack.success) {
        cleanup.error = rolledBack.errorMessage.isEmpty()
            ? QStringLiteral("The display preview could not be rolled back")
            : rolledBack.errorMessage;
        return cleanup;
      }
    }
    const auto aborted = abortPrepared();
    cleanup.snapshot = aborted.snapshot;
    if (!aborted.success) {
      cleanup.error = aborted.errorMessage.isEmpty()
          ? QStringLiteral("The display preview journal could not be aborted")
          : aborted.errorMessage;
      return cleanup;
    }
    cleanup.success = true;
    return cleanup;
  };
  if (!activated.success || !activated.activationMayHaveOccurred
      || activated.generation != generation.id
      || activated.confirmedRequirement != ActivationRequirement::Reload
      || activated.runtimeIdentity != beforeTopology.runtimeIdentity) {
    const auto cleanup = rollbackAndAbort();
    if (cleanup.success) {
      acceptState(cleanup.snapshot, cleanup.management);
    } else {
      acceptUnreconciled(cleanup.snapshot, cleanup.management);
    }
    reportError(
        cleanup.success
            ? boundedErrorCode(activated.errorCode,
                               QStringLiteral("VerificationFailed"))
            : QStringLiteral("ApplyFailed"),
        cleanup.success && !activated.errorMessage.isEmpty()
            ? activated.errorMessage
            : !cleanup.error.isEmpty()
                ? cleanup.error
                : QStringLiteral("The display preview could not be safely activated")
    );
    return revision();
  }

  DisplayConfirmation confirmation{
      .owner = owner,
      .generation = generation.id,
      .runtimeIdentity = beforeTopology.runtimeIdentity,
      .topologyDigest = beforeTopology.topology->topologyDigest,
      .baseRevision = expectedRevision,
      .previewRevision = generation.revision,
      .receipt = activated.receipt,
      .profile = *profile.value,
  };
  Hyprland::ConnectedDisplayTopology provedTopology;
  if (!displayTopologyStillExact(
          confirmation, verificationError, -1, &provedTopology
      )) {
    const auto cleanup = rollbackAndAbort();
    if (cleanup.success) {
      acceptState(cleanup.snapshot, cleanup.management);
    } else {
      acceptUnreconciled(cleanup.snapshot, cleanup.management);
    }
    reportError(
        cleanup.success ? QStringLiteral("DisplayTopologyChanged")
                        : QStringLiteral("ApplyFailed"),
        cleanup.success ? verificationError
                        : !cleanup.error.isEmpty()
                            ? cleanup.error
                            : QStringLiteral("The unsafe preview could not be rolled back")
    );
    return revision();
  }
  confirmation.cachedTopologyDocument = provedTopology.document;
  confirmation.cachedTopologyObservedAtMs = static_cast<quint64>(
      QDateTime::currentMSecsSinceEpoch()
  );

  // The confirmation capability and both deadlines are created only after
  // target proof, fresh topology/realization proof, and receipt-bound target
  // verification all succeed.
  do {
    confirmation.token = displayConfirmationToken();
  } while (displayTerminal_
           && displayTerminal_->token == confirmation.token);
  confirmation.deadline = QDeadlineTimer(
      static_cast<qint64>(timeoutSeconds) * 1000, Qt::PreciseTimer
  );
  confirmation.deadlineMs = static_cast<quint64>(
      QDateTime::currentMSecsSinceEpoch()
      + static_cast<qint64>(timeoutSeconds) * 1000
  );
  displayTerminal_.reset();
  displayConfirmation_ = std::move(confirmation);
  displayConfirmationState_ = QStringLiteral("awaiting-confirmation");
  const auto installedToken = displayConfirmation_->token;
  if (owner != QStringLiteral("in-process")) {
    displayOwnerWatcher_ = std::make_unique<QDBusServiceWatcher>(
        owner, connection_, QDBusServiceWatcher::WatchForUnregistration, this
    );
    connect(
        displayOwnerWatcher_.get(),
        &QDBusServiceWatcher::serviceUnregistered, this,
        &CompositorService::handleDisplayOwnerLoss
    );
  }
  const auto remainingForOwner = displayDeadlineRemaining_(
      displayConfirmation_->deadline
  );
  if (remainingForOwner <= 0) {
    QString rollbackError;
    const auto reverted = revertDisplayConfirmation(
        DisplayTerminalAction::Expired, rollbackError
    );
    reportError(
        reverted ? QStringLiteral("ConfirmationExpired")
                 : QStringLiteral("ApplyFailed"),
        reverted || rollbackError.isEmpty()
            ? QStringLiteral("The display confirmation expired")
            : rollbackError
    );
    return revision();
  }
  const auto ownerStillPresent = displayOwnerStillPresent(
      owner,
      static_cast<int>(std::min<qint64>(
          remainingForOwner, displayOwnerProbeMaximumMilliseconds
      ))
  );
  const auto capabilityStillInstalled = displayConfirmation_
      && displayConfirmation_->token == installedToken
      && !displayConfirmation_->terminationStarted;
  if (!ownerStillPresent) {
    QString rollbackError;
    const auto reverted = capabilityStillInstalled
        ? revertDisplayConfirmation(
              DisplayTerminalAction::Reverted, rollbackError
          )
        : true;
    reportError(
        reverted ? QStringLiteral("InvalidCaller")
                 : QStringLiteral("ApplyFailed"),
        reverted
            ? QStringLiteral("The display preview caller disconnected")
            : rollbackError
    );
    return revision();
  }
  if (!capabilityStillInstalled) {
    const auto expired = displayTerminal_
        && displayTerminal_->token == installedToken
        && displayTerminal_->action == DisplayTerminalAction::Expired;
    reportError(
        expired ? QStringLiteral("ConfirmationExpired")
                : QStringLiteral("ApplyFailed"),
        expired
            ? QStringLiteral("The display confirmation expired")
            : QStringLiteral(
                  "The display preview ended during caller verification"
              )
    );
    return revision();
  }
  const auto remainingForTimer = displayDeadlineRemaining_(
      displayConfirmation_->deadline
  );
  if (remainingForTimer <= 0) {
    QString rollbackError;
    const auto reverted = revertDisplayConfirmation(
        DisplayTerminalAction::Expired, rollbackError
    );
    reportError(
        reverted ? QStringLiteral("ConfirmationExpired")
                 : QStringLiteral("ApplyFailed"),
        reverted || rollbackError.isEmpty()
            ? QStringLiteral("The display confirmation expired")
            : rollbackError
    );
    return revision();
  }
  displayConfirmationTimer_.start(
      static_cast<int>(std::min<qint64>(remainingForTimer, INT_MAX))
  );
  auto previewStatus = activated.status;
  acceptState(prepared.snapshot, previewStatus, true);

  confirmationToken = displayConfirmation_->token;
  deadlineMs = displayConfirmation_->deadlineMs;
  previewGenerationDigest = displayConfirmation_->generation;
  return displayConfirmation_->previewRevision;
}

QString CompositorService::GetPendingDisplayConfirmation(
    qulonglong &previewRevision,
    qulonglong &deadlineMs,
    QString &previewGenerationDigest
) {
  previewRevision = 0;
  deadlineMs = 0;
  previewGenerationDigest.clear();
  const auto owner = callerIdentity();
  if (!displayConfirmation_ || owner.isEmpty()
      || displayConfirmation_->owner != owner) {
    reportError(QStringLiteral("NoDisplayConfirmation"),
                QStringLiteral("No display confirmation is available"));
    return {};
  }
  previewRevision = displayConfirmation_->previewRevision;
  deadlineMs = displayConfirmation_->deadlineMs;
  previewGenerationDigest = displayConfirmation_->generation;
  return displayConfirmation_->token;
}

qulonglong CompositorService::ConfirmDisplayConfiguration(
    const QString &confirmationToken,
    QString &confirmedGenerationDigest
) {
  confirmedGenerationDigest.clear();
  const auto owner = callerIdentity();
  if (displayTerminal_ && displayTerminal_->token == confirmationToken
      && displayTerminal_->owner == owner
      && displayTerminal_->action == DisplayTerminalAction::Confirmed) {
    confirmedGenerationDigest = displayTerminal_->generation;
    return displayTerminal_->revision;
  }
  if (!displayConfirmation_ || owner.isEmpty()
      || displayConfirmation_->owner != owner
      || displayConfirmation_->token != confirmationToken) {
    reportError(QStringLiteral("NoDisplayConfirmation"),
                QStringLiteral("No matching display confirmation is available"));
    return revision();
  }
  if (displayConfirmation_->terminationStarted) {
    reportError(
        QStringLiteral("NoDisplayConfirmation"),
        QStringLiteral("The display preview is already being reverted")
    );
    return revision();
  }
  if (displayDeadlineRemaining_(displayConfirmation_->deadline) <= 0) {
    QString error;
    const auto reverted = revertDisplayConfirmation(
        DisplayTerminalAction::Expired, error
    );
    reportError(
        reverted ? QStringLiteral("ConfirmationExpired")
                 : QStringLiteral("ApplyFailed"),
        error.isEmpty() ? QStringLiteral("The display confirmation expired")
                        : error
    );
    return revision();
  }
  QString verificationError;
  const auto remainingForProof = displayDeadlineRemaining_(
      displayConfirmation_->deadline
  );
  if (remainingForProof <= 0) {
    QString error;
    const auto reverted = revertDisplayConfirmation(
        DisplayTerminalAction::Expired, error
    );
    reportError(
        reverted ? QStringLiteral("ConfirmationExpired")
                 : QStringLiteral("ApplyFailed"),
        error.isEmpty() ? QStringLiteral("The display confirmation expired")
                        : error
    );
    return revision();
  }
  const auto topologyIsExact =
      displayTopologyStillExact(
          *displayConfirmation_, verificationError,
          static_cast<int>(std::min<qint64>(remainingForProof, INT_MAX))
      );
  const auto remainingAfterProof = displayDeadlineRemaining_(
      displayConfirmation_->deadline
  );
  const auto expiredAfterProof = remainingAfterProof <= 0;
  if (!topologyIsExact || expiredAfterProof) {
    const auto failureMessage = expiredAfterProof
        ? QStringLiteral("The display confirmation expired")
        : verificationError;
    QString rollbackError;
    const auto reverted = revertDisplayConfirmation(
        expiredAfterProof ? DisplayTerminalAction::Expired
                          : DisplayTerminalAction::Reverted,
        rollbackError
    );
    reportError(
        !reverted ? QStringLiteral("ApplyFailed")
        : expiredAfterProof ? QStringLiteral("ConfirmationExpired")
                            : QStringLiteral("DisplayTopologyChanged"),
        !reverted && !rollbackError.isEmpty()
            ? rollbackError
            : !failureMessage.isEmpty() ? failureMessage : rollbackError
    );
    return revision();
  }

  // The synchronous topology proof can occupy the service thread, delaying
  // QDBusServiceWatcher's owner-loss delivery. Re-query the unique name after
  // proof and immediately before the one-way authority decision.
  const auto installedToken = displayConfirmation_->token;
  const auto ownerStillPresent = displayOwnerStillPresent(
      owner,
      static_cast<int>(std::min<qint64>(
          remainingAfterProof, displayOwnerProbeMaximumMilliseconds
      ))
  );
  const auto capabilityStillInstalled = displayConfirmation_
      && displayConfirmation_->token == installedToken
      && !displayConfirmation_->terminationStarted;
  if (!capabilityStillInstalled) {
    const auto expired = displayTerminal_
        && displayTerminal_->token == installedToken
        && displayTerminal_->action == DisplayTerminalAction::Expired;
    const auto cleanupFailed = displayConfirmation_
        && displayConfirmation_->token == installedToken
        && displayConfirmation_->terminationStarted;
    reportError(
        cleanupFailed ? QStringLiteral("ApplyFailed")
        : expired ? QStringLiteral("ConfirmationExpired")
                  : QStringLiteral("NoDisplayConfirmation"),
        cleanupFailed
            ? QStringLiteral(
                  "The display preview could not be safely reconciled"
              )
        : expired
            ? QStringLiteral("The display confirmation expired")
            : QStringLiteral("The display confirmation is no longer active")
    );
    return revision();
  }
  if (!ownerStillPresent) {
    QString rollbackError;
    const auto reverted = revertDisplayConfirmation(
        DisplayTerminalAction::Reverted, rollbackError
    );
    reportError(
        reverted ? QStringLiteral("NoDisplayConfirmation")
                 : QStringLiteral("ApplyFailed"),
        reverted
            ? QStringLiteral("The display preview caller disconnected")
            : rollbackError
    );
    return revision();
  }
  if (displayDeadlineRemaining_(displayConfirmation_->deadline) <= 0) {
    QString rollbackError;
    const auto reverted = revertDisplayConfirmation(
        DisplayTerminalAction::Expired, rollbackError
    );
    reportError(
        reverted ? QStringLiteral("ConfirmationExpired")
                 : QStringLiteral("ApplyFailed"),
        rollbackError.isEmpty()
            ? QStringLiteral("The display confirmation expired")
            : rollbackError
    );
    return revision();
  }

  const auto confirmation = *displayConfirmation_;
  auto committed = authority_->commitApply(confirmation.generation);
  const auto commitDecisionMayExist =
      committed.success || committed.commitDecisionMayExist
      || committed.commitDecisionDurable;
  if (commitDecisionMayExist) {
    // commitApply is the one-way authority boundary. Once it succeeds, or
    // even reports that its decision may be visible, startup reconciliation
    // owns the target. Revoke every rollback path before finalization: a
    // timer, owner-loss signal, late Revert call, or management poll must
    // never contradict a possibly committed N+1 decision.
    displayConfirmationTimer_.stop();
    clearDisplayOwnerWatch();
    displayConfirmation_.reset();
    displayConfirmationState_ = QStringLiteral("committing");
    publishDisplayProperties();
  }

  if (!committed.success) {
    QString rollbackError;
    bool reverted = true;
    if (!commitDecisionMayExist) {
      reverted = revertDisplayConfirmation(
          DisplayTerminalAction::Reverted, rollbackError
      );
    } else {
      displayConfirmationState_ = QStringLiteral("failed");
      acceptUnreconciled(committed.snapshot, management_);
    }
    reportError(
        QStringLiteral("ApplyFailed"),
        !reverted && !rollbackError.isEmpty()
            ? rollbackError
            : committed.errorMessage.isEmpty()
            ? QStringLiteral(
                  "The confirmed display generation requires startup reconciliation"
              )
            : committed.errorMessage
    );
    return revision();
  }
  const auto finalized = activationBackend_->finalizeCommitted(
      confirmation.receipt, confirmation.generation
  );
  if (!finalized.success
      || finalized.status.state != ManagementState::Managed
      || finalized.status.managedGeneration != confirmation.generation) {
    displayConfirmationState_ = QStringLiteral("failed");
    acceptUnreconciled(committed.snapshot, finalized.status);
    reportError(QStringLiteral("ApplyFailed"),
                finalized.errorMessage.isEmpty()
                    ? QStringLiteral("The confirmed display generation requires startup reconciliation")
                    : finalized.errorMessage);
    return revision();
  }

  displayConfirmationState_ = QStringLiteral("idle");
  displayTerminal_ = DisplayTerminal{
      .token = confirmation.token,
      .owner = confirmation.owner,
      .action = DisplayTerminalAction::Confirmed,
      .revision = committed.snapshot.revision,
      .generation = confirmation.generation,
  };
  acceptState(committed.snapshot, finalized.status, true);
  confirmedGenerationDigest = confirmation.generation;
  return committed.snapshot.revision;
}

qulonglong CompositorService::RevertDisplayConfiguration(
    const QString &confirmationToken
) {
  const auto owner = callerIdentity();
  if (displayTerminal_ && displayTerminal_->token == confirmationToken
      && displayTerminal_->owner == owner
      && displayTerminal_->action == DisplayTerminalAction::Reverted) {
    return displayTerminal_->revision;
  }
  if (!displayConfirmation_ || owner.isEmpty()
      || displayConfirmation_->owner != owner
      || displayConfirmation_->token != confirmationToken) {
    reportError(QStringLiteral("NoDisplayConfirmation"),
                QStringLiteral("No matching display confirmation is available"));
    return revision();
  }
  QString error;
  if (!revertDisplayConfirmation(DisplayTerminalAction::Reverted, error)) {
    reportError(QStringLiteral("ApplyFailed"), error);
  }
  return revision();
}

void CompositorService::RetrySharedBorderSync() {
  clearFailedSharedBorderAttempt();
  forceSharedBorderRetry_ = true;
  if (sharedBorderSource_) sharedBorderSource_->requestRefresh();
  scheduleSharedBorderReconcile();
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

bool CompositorService::hasDisplayConfirmation() const {
  return displayConfirmation_.has_value();
}

QString CompositorService::callerIdentity() const {
  if (!calledFromDBus()) return QStringLiteral("in-process");
  const auto service = message().service();
  return service.startsWith(QLatin1Char(':')) ? service : QString();
}

bool CompositorService::displayOwnerStillPresent(
    const QString &owner,
    const int maximumWaitMilliseconds
) const {
  if (owner.isEmpty() || maximumWaitMilliseconds <= 0) return false;
  const auto boundedWait = std::min(
      maximumWaitMilliseconds, displayOwnerProbeMaximumMilliseconds
  );
  if (displayOwnerPresent_) {
    try {
      return displayOwnerPresent_(owner, boundedWait);
    } catch (...) {
      return false;
    }
  }
  if (owner == QStringLiteral("in-process")) return true;
  auto request = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("NameHasOwner")
  );
  request.setArguments({owner});
  const QDBusReply<bool> reply(
      connection_.call(request, QDBus::Block, boundedWait)
  );
  return reply.isValid() && reply.value();
}

bool CompositorService::checkNoDisplayConfirmation() {
  if (!hasDisplayConfirmation()) return true;
  reportError(QStringLiteral("ConfirmationPending"),
              QStringLiteral("A display confirmation is pending"));
  return false;
}

bool CompositorService::validatePreparedGeneration(
    const AuthorityResult &prepared,
    QString &error
) const {
  static const QRegularExpression digestExpression(
      QStringLiteral("^[0-9a-f]{64}$")
  );
  static const QRegularExpression nonceExpression(
      QStringLiteral("^[0-9a-f]{32,128}$")
  );
  if (!prepared.prepared) {
    error = QStringLiteral("The display preview generation is missing");
    return false;
  }
  const auto &generation = *prepared.prepared;
  QJsonObject manifest;
  if (!generation.manifest.isEmpty()
      && generation.manifest.size() <= Hyprland::maximumDesiredStateBytes) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(
        generation.manifest, &parseError
    );
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
      manifest = document.object();
    }
  }
  const auto complete = digestExpression.match(generation.id).hasMatch()
      && nonceExpression.match(generation.nonce).hasMatch()
      && digestExpression.match(generation.snapshotDigest).hasMatch()
      && generation.revision == prepared.snapshot.revision + 1
      && QDir::isAbsolutePath(generation.directory)
      && QDir::cleanPath(generation.directory) == generation.directory
      && QFileInfo(generation.directory).fileName() == generation.nonce
      && QFileInfo(generation.directory).dir().dirName()
             == QStringLiteral("generations")
      && generation.entrypoint
             == QDir(generation.directory).filePath(
                    QStringLiteral("hyprland.lua"))
      && manifest.value(QStringLiteral("generation")).toString()
             == generation.id
      && manifest.value(QStringLiteral("activationNonce")).toString()
             == generation.nonce
      && manifest.value(QStringLiteral("snapshotDigest")).toString()
             == generation.snapshotDigest
      && manifest.value(QStringLiteral("revision")).toString()
             == QString::number(generation.revision)
      && manifest.value(QStringLiteral("entrypoint")).toString()
             == QStringLiteral("hyprland.lua");
  if (!complete) {
    error = QStringLiteral("The display preview generation is incomplete");
  }
  return complete;
}

bool CompositorService::displayTopologyStillExact(
    const DisplayConfirmation &confirmation,
    QString &error,
    const int maximumWaitMilliseconds,
    Hyprland::ConnectedDisplayTopology *observedTopology
) {
  const auto connected = maximumWaitMilliseconds < 0
      ? activationBackend_->connectedDisplays()
      : activationBackend_->connectedDisplays(maximumWaitMilliseconds);
  if (!connected.success || !connected.topology) {
    error = connected.errorMessage.isEmpty()
        ? QStringLiteral("The connected-display topology is unavailable")
        : connected.errorMessage;
    return false;
  }
  if (connected.runtimeIdentity != confirmation.runtimeIdentity
      || connected.topology->topologyDigest != confirmation.topologyDigest) {
    error = QStringLiteral(
        "The compositor instance or connected-display topology changed"
    );
    return false;
  }
  const auto profileErrors = Hyprland::validateDisplayProfileTopology(
      confirmation.profile, *connected.topology
  );
  if (!profileErrors.isEmpty()) {
    error = profileErrors.front().message;
    return false;
  }
  const auto realizationErrors = Hyprland::validateDisplayRealization(
      confirmation.profile, *connected.topology
  );
  if (!realizationErrors.isEmpty()) {
    error = realizationErrors.front().message;
    return false;
  }
  const auto target = activationBackend_->verifyPendingTarget(
      confirmation.receipt, confirmation.generation
  );
  if (!target.success) {
    error = target.errorMessage.isEmpty()
        ? QStringLiteral("The pending display target changed")
        : target.errorMessage;
    return false;
  }
  if (observedTopology) *observedTopology = *connected.topology;
  return true;
}

bool CompositorService::revertDisplayConfirmation(
    const DisplayTerminalAction action,
    QString &error
) {
  error.clear();
  if (!displayConfirmation_) {
    error = QStringLiteral("No display confirmation is pending");
    return false;
  }
  displayConfirmation_->terminationStarted = true;
  displayConfirmationState_ = QStringLiteral("reverting");
  publishDisplayProperties();
  auto &pending = *displayConfirmation_;
  ManagementStatus restoredStatus = pending.rolledBackStatus.value_or(
      management_
  );
  if (!pending.liveRolledBack) {
    const auto rolledBack = activationBackend_->rollback(pending.receipt);
    restoredStatus = rolledBack.status;
    if (!rolledBack.success) {
      displayConfirmationState_ = QStringLiteral("failed");
      error = rolledBack.errorMessage.isEmpty()
          ? QStringLiteral("The display preview could not be rolled back")
          : rolledBack.errorMessage;
      acceptUnreconciled(snapshot_, rolledBack.status);
      return false;
    }
    pending.liveRolledBack = true;
    pending.rolledBackStatus = rolledBack.status;
  }
  const auto aborted = authority_->abortApply(pending.generation);
  if (!aborted.success) {
    displayConfirmationState_ = QStringLiteral("failed");
    error = aborted.errorMessage.isEmpty()
        ? QStringLiteral("The display preview journal could not be aborted")
        : aborted.errorMessage;
    acceptUnreconciled(aborted.snapshot, restoredStatus);
    return false;
  }

  const auto terminal = DisplayTerminal{
      .token = pending.token,
      .owner = pending.owner,
      .action = action,
      .revision = aborted.snapshot.revision,
      .generation = aborted.snapshot.generationDigest,
  };
  displayConfirmationTimer_.stop();
  clearDisplayOwnerWatch();
  displayConfirmation_.reset();
  displayConfirmationState_ = QStringLiteral("idle");
  displayTerminal_ = terminal;
  acceptState(aborted.snapshot, restoredStatus, true);
  return true;
}

void CompositorService::clearDisplayOwnerWatch() {
  if (auto *watcher = displayOwnerWatcher_.release()) {
    // Owner loss can call this from the watcher's own signal. Deferred
    // destruction avoids deleting the signal sender on its active stack.
    watcher->deleteLater();
  }
}

void CompositorService::appendDisplayProperties(QVariantMap &changed) const {
  changed.insert(
      QStringLiteral("DisplayConfirmationState"), displayConfirmationState()
  );
  changed.insert(
      QStringLiteral("DisplayConfirmationRevision"),
      QVariant::fromValue<qulonglong>(displayConfirmationRevision())
  );
  changed.insert(
      QStringLiteral("DisplayConfirmationDeadlineMs"),
      QVariant::fromValue<qulonglong>(displayConfirmationDeadlineMs())
  );
  changed.insert(
      QStringLiteral("DisplayConfirmationGeneration"),
      displayConfirmationGeneration()
  );
}

void CompositorService::publishDisplayProperties() {
  QVariantMap changed;
  appendDisplayProperties(changed);
  publishProperties(changed);
  scheduleSharedBorderReconcile();
}

void CompositorService::scheduleSharedBorderReconcile() {
  if (sharedBorderReconcileScheduled_) return;
  sharedBorderReconcileScheduled_ = true;
  QTimer::singleShot(0, this, &CompositorService::reconcileSharedBorder);
}

void CompositorService::sharedBorderSourceChanged() {
  if (sharedBorderSource_->available()) {
    const auto next = sharedBorderSource_->projection();
    const auto effectivePolicyChanged =
        !lastSharedBorderProjection_
        || lastSharedBorderProjection_->syncWindowBorders
            != next.syncWindowBorders
        || sharedBorderReconciler_.valuesFor(*lastSharedBorderProjection_)
            != sharedBorderReconciler_.valuesFor(next);
    if (effectivePolicyChanged) {
      clearFailedSharedBorderAttempt();
      pendingSharedBorderApplyRevision_.reset();
    }
    if (!next.syncWindowBorders) {
      pendingSharedBorderApplyRevision_.reset();
    }
    lastSharedBorderProjection_ = next;
  }
  scheduleSharedBorderReconcile();
}

void CompositorService::setSharedBorderStatus(
    const QString &state,
    const qulonglong sourceRevision,
    const QString &error
) {
  auto boundedError = error;
  if (boundedError.size() > 1024) boundedError.truncate(1024);
  QVariantMap changed;
  if (state != sharedBorderSyncState_) {
    sharedBorderSyncState_ = state;
    changed.insert(QStringLiteral("SharedBorderSyncState"), state);
  }
  if (sourceRevision != sharedBorderSourceRevision_) {
    sharedBorderSourceRevision_ = sourceRevision;
    changed.insert(
        QStringLiteral("SharedBorderSourceRevision"),
        QVariant::fromValue<qulonglong>(sourceRevision)
    );
  }
  if (boundedError != sharedBorderSyncError_) {
    sharedBorderSyncError_ = boundedError;
    changed.insert(QStringLiteral("SharedBorderSyncError"), boundedError);
  }
  publishProperties(changed);
}

QString CompositorService::sharedBorderAttemptKey() const {
  QStringList fields{
      sharedBorderSource_ && sharedBorderSource_->available()
          ? QStringLiteral("source") : QStringLiteral("no-source"),
      QString::number(snapshot_.available),
      QString::number(snapshot_.writable),
      QString::number(snapshot_.revision),
      QString::number(snapshot_.appliedRevision),
      snapshot_.catalogDigest,
      snapshot_.applyState,
      requiredActivationName(snapshot_.requiredActivation),
      snapshot_.generationDigest,
      managementStateName(management_.state),
      management_.managedGeneration,
      displayConfirmationState_,
      QString::number(sharedBorderAuthorityGeneration_),
  };
  if (sharedBorderSource_ && sharedBorderSource_->available()) {
    const auto &projection = sharedBorderSource_->projection();
    fields.append(QString::number(projection.borderEnabled));
    fields.append(QString::number(projection.borderWidth));
    fields.append(QString::number(projection.borderRadius));
    fields.append(QString::number(projection.syncWindowBorders));
  }
  return fields.join(QLatin1Char('|'));
}

bool CompositorService::ensureSharedBorderCatalog(QString &error) {
  if (sharedBorderReconciler_.configuredFor(snapshot_.catalogDigest)) {
    error.clear();
    return true;
  }
  if (!authority_ || snapshot_.catalogDigest.isEmpty()) {
    error = QStringLiteral("The compositor option catalog is unavailable");
    return false;
  }
  return sharedBorderReconciler_.configure(
      authority_->optionCatalog(), snapshot_.catalogDigest, error
  );
}

bool CompositorService::sharedBorderReplacementAllowed(
    const QByteArray &candidate,
    QString &error
) {
  error.clear();
  std::optional<SharedBorderProjection> policy;
  if (sharedBorderSource_ && sharedBorderSource_->available()) {
    policy = sharedBorderSource_->projection();
  } else if (lastSharedBorderProjection_) {
    policy = *lastSharedBorderProjection_;
  }
  if (policy && !policy->syncWindowBorders) {
    return true;
  }

  QString catalogError;
  if (!ensureSharedBorderCatalog(catalogError)) {
    error = catalogError;
    return false;
  }

  std::optional<SharedBorderValues> controlledValues;
  if (policy) {
    controlledValues = sharedBorderReconciler_.valuesFor(*policy);
  } else {
    controlledValues = sharedBorderReconciler_.resolvedValues(
        snapshot_.desiredState, error
    );
    if (!controlledValues) {
      error = QStringLiteral(
          "Window border geometry cannot be safely replaced while shared visual policy is unavailable"
      );
      return false;
    }
  }

  if (!sharedBorderReconciler_.replacementPreserves(
          candidate, *controlledValues, error
      )) {
    error = QStringLiteral(
        "Window border geometry is controlled by shared visual settings"
    );
    return false;
  }
  return true;
}

bool CompositorService::sharedBorderActivationAllowed(
    const QByteArray &candidate,
    QString &error
) {
  if (!sharedBorderSource_ || !sharedBorderSource_->available()) {
    error = QStringLiteral(
        "Shared visual settings are unavailable or have not been verified"
    );
    return false;
  }
  return sharedBorderReplacementAllowed(candidate, error);
}

void CompositorService::clearFailedSharedBorderAttempt() {
  failedSharedBorderAttempt_.clear();
  failedSharedBorderError_.clear();
}

void CompositorService::failSharedBorder(const QString &error) {
  failedSharedBorderAttempt_ = sharedBorderAttemptKey();
  failedSharedBorderError_ = error.isEmpty()
      ? QStringLiteral("Shared window border synchronization failed")
      : error;
  if (failedSharedBorderError_.size() > 1024) {
    failedSharedBorderError_.truncate(1024);
  }
  setSharedBorderStatus(
      QStringLiteral("failed"),
      sharedBorderSource_ && sharedBorderSource_->available()
          ? sharedBorderSource_->projection().revision : 0,
      failedSharedBorderError_
  );
}

void CompositorService::reconcileSharedBorder() {
  sharedBorderReconcileScheduled_ = false;
  if (sharedBorderReconcileRunning_) {
    scheduleSharedBorderReconcile();
    return;
  }
  sharedBorderReconcileRunning_ = true;
  struct ResetRunning final {
    bool &value;
    ~ResetRunning() { value = false; }
  } resetRunning{sharedBorderReconcileRunning_};

  const auto force = std::exchange(forceSharedBorderRetry_, false);
  if (!sharedBorderSource_ || !sharedBorderSource_->available()) {
    setSharedBorderStatus(
        QStringLiteral("unavailable"), 0,
        sharedBorderSource_
            ? sharedBorderSource_->error()
            : QStringLiteral("Shared visual settings are unavailable")
    );
    return;
  }
  const auto projection = sharedBorderSource_->projection();
  lastSharedBorderProjection_ = projection;
  if (!projection.syncWindowBorders) {
    pendingSharedBorderApplyRevision_.reset();
    clearFailedSharedBorderAttempt();
    setSharedBorderStatus(
        QStringLiteral("override"), projection.revision
    );
    return;
  }
  if (!authority_ || !snapshot_.available) {
    setSharedBorderStatus(
        QStringLiteral("unavailable"), projection.revision,
        QStringLiteral("Compositor configuration is unavailable")
    );
    return;
  }
  if (displayConfirmationState_ != QStringLiteral("idle")) {
    setSharedBorderStatus(
        QStringLiteral("pending"), projection.revision
    );
    return;
  }

  const auto liveBaseManagement = authorityBoundManagement(
      snapshot_, activationBackend_->status()
  );
  if (liveBaseManagement != management_) {
    acceptState(snapshot_, liveBaseManagement);
  }
  const auto baseWasExactCurrent =
      management_.state == ManagementState::Managed
      && snapshot_.applyState == QStringLiteral("current")
      && snapshot_.appliedRevision == snapshot_.revision
      && !snapshot_.requiredActivation.has_value()
      && !snapshot_.generationDigest.isEmpty()
      && management_.managedGeneration == snapshot_.generationDigest;

  const auto attemptKey = sharedBorderAttemptKey();
  if (!force && !failedSharedBorderAttempt_.isEmpty()
      && failedSharedBorderAttempt_ == attemptKey) {
    setSharedBorderStatus(
        QStringLiteral("failed"),
        projection.revision,
        failedSharedBorderError_
    );
    return;
  }
  clearFailedSharedBorderAttempt();
  if (!snapshot_.writable) {
    failSharedBorder(QStringLiteral("Compositor configuration is read-only"));
    return;
  }
  setSharedBorderStatus(QStringLiteral("pending"), projection.revision);

  QString error;
  if (!ensureSharedBorderCatalog(error)) {
    failSharedBorder(error);
    return;
  }
  const auto edit = sharedBorderReconciler_.edit(
      snapshot_.desiredState,
      snapshot_.revision,
      snapshot_.catalogDigest,
      projection,
      error
  );
  if (!edit) {
    failSharedBorder(error);
    return;
  }
  if (edit->changed) {
    const auto replaced = authority_->replaceSnapshot(
        snapshot_.revision, edit->candidate
    );
    if (!replaced.success) {
      if (replaced.snapshot.available
          || replaced.snapshot.applyState == QStringLiteral("failed")) {
        acceptState(replaced.snapshot, activationBackend_->status());
      }
      failSharedBorder(replaced.errorMessage);
      return;
    }
    acceptState(replaced.snapshot, activationBackend_->status());
    if (baseWasExactCurrent) {
      pendingSharedBorderApplyRevision_ = replaced.snapshot.revision;
    } else {
      pendingSharedBorderApplyRevision_.reset();
    }
  }

  const auto resolved = sharedBorderReconciler_.resolvedValues(
      snapshot_.desiredState, error
  );
  if (!resolved
      || *resolved != sharedBorderReconciler_.valuesFor(projection)) {
    failSharedBorder(error);
    return;
  }

  const auto liveManagement = authorityBoundManagement(
      snapshot_, activationBackend_->status()
  );
  if (liveManagement != management_) {
    acceptState(snapshot_, liveManagement);
  }
  const auto exactCurrent = management_.state == ManagementState::Managed
      && snapshot_.applyState == QStringLiteral("current")
      && snapshot_.appliedRevision == snapshot_.revision
      && !snapshot_.requiredActivation.has_value()
      && !snapshot_.generationDigest.isEmpty()
      && management_.managedGeneration == snapshot_.generationDigest;
  if (exactCurrent) {
    pendingSharedBorderApplyRevision_.reset();
    setSharedBorderStatus(
        QStringLiteral("current"), projection.revision
    );
    return;
  }

  const auto ownsPendingRevision = pendingSharedBorderApplyRevision_
      && *pendingSharedBorderApplyRevision_ == snapshot_.revision;
  if (!ownsPendingRevision
      || management_.state != ManagementState::Managed
      || !snapshot_.requiredActivation.has_value()
      || *snapshot_.requiredActivation != ActivationRequirement::Reload
      || !activationBackend_->canSatisfy(
          *snapshot_.requiredActivation
      )) {
    setSharedBorderStatus(
        QStringLiteral("saved"), projection.revision
    );
    return;
  }

  auto completion = completePrepared(
      authority_->prepareApply(
          snapshot_.revision,
          activationNonce(),
          QDateTime::currentDateTimeUtc()
      ),
      false,
      {},
      snapshot_.requiredActivation
  );
  if (!completion.success) {
    if (completion.snapshot.available
        || completion.snapshot.applyState == QStringLiteral("failed")) {
      acceptState(completion.snapshot, completion.management);
    }
    failSharedBorder(completion.errorMessage);
    return;
  }
  acceptState(completion.snapshot, completion.management);
  pendingSharedBorderApplyRevision_.reset();
  setSharedBorderStatus(
      QStringLiteral("current"), projection.revision
  );
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

void CompositorService::acceptUnreconciled(
    const AuthoritySnapshot &authoritySnapshot,
    ManagementStatus liveStatus
) {
  auto unavailable = authoritySnapshot;
  unavailable.available = false;
  unavailable.writable = false;
  unavailable.loadState = QStringLiteral("unavailable");
  unavailable.applyState = QStringLiteral("failed");
  liveStatus.state = ManagementState::Conflict;
  liveStatus.entrypointKind = EntrypointKind::Unsafe;
  liveStatus.entrypointDigest.clear();
  liveStatus.managedGeneration.clear();
  liveStatus.managedNonce.clear();
  displayConfirmationState_ = QStringLiteral("failed");
  acceptState(unavailable, liveStatus, true);
}

void CompositorService::acceptState(const AuthoritySnapshot &next,
                                    const ManagementStatus &nextManagement,
                                    const bool includeDisplayProperties) {
  const auto boundManagement =
      authorityBoundManagement(
          next, nextManagement,
          displayConfirmation_
              ? QStringView(displayConfirmation_->generation) : QStringView()
      );
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
  if (includeDisplayProperties) appendDisplayProperties(changed);

  if (next != snapshot_ || boundManagement != management_) {
    ++sharedBorderAuthorityGeneration_;
  }
  if (pendingSharedBorderApplyRevision_
      && next.revision != *pendingSharedBorderApplyRevision_) {
    pendingSharedBorderApplyRevision_.reset();
  }
  snapshot_ = next;
  management_ = boundManagement;
  publishProperties(changed);
  scheduleSharedBorderReconcile();
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
  if (displayConfirmation_) {
    const auto target = activationBackend_->verifyPendingTarget(
        displayConfirmation_->receipt, displayConfirmation_->generation
    );
    if (!target.success) {
      QString error;
      static_cast<void>(revertDisplayConfirmation(
          DisplayTerminalAction::Reverted, error
      ));
      rearmManagementWatch();
      return;
    }
    const auto live = authorityBoundManagement(
        snapshot_, target.status, displayConfirmation_->generation
    );
    if (live != management_) acceptState(snapshot_, target.status);
    rearmManagementWatch();
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
      QStringLiteral("RuntimeUnavailable"),
      QStringLiteral("UnsupportedVersion"),
      QStringLiteral("InvalidDisplayProfile"),
      QStringLiteral("DisplayScopeConflict"),
      QStringLiteral("ConfirmationPending"),
      QStringLiteral("DisplayTopologyChanged"),
      QStringLiteral("InvalidCaller"),
      QStringLiteral("NoDisplayConfirmation"),
      QStringLiteral("ConfirmationExpired"),
      QStringLiteral("ControlledByHyprShelld"),
  };
  return allowed.contains(code) ? code : fallback;
}

} // namespace HyprShelld::Compositor
