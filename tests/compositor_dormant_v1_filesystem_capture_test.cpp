#include "compositord/dormant_v1_filesystem_capture.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <array>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace HyprShelld::Compositor;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";
constexpr auto nonceB = "fedcba9876543210fedcba9876543210";
constexpr auto nonceC = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

constexpr std::array<const char *, 16> moduleNames{{
    "00-session.lua",
    "10-monitors.lua",
    "20-environment.lua",
    "30-input.lua",
    "31-gestures.lua",
    "32-cursor.lua",
    "40-general.lua",
    "41-layouts.lua",
    "42-workspaces.lua",
    "43-groups.lua",
    "50-decorations.lua",
    "51-animations.lua",
    "60-rules.lua",
    "70-keybinds.lua",
    "80-permissions.lua",
    "90-advanced.lua",
}};

constexpr std::array<DormantV1CaptureSubject, 6> fixedSubjects{{
    DormantV1CaptureSubject::Desired,
    DormantV1CaptureSubject::LastGood,
    DormantV1CaptureSubject::Applied,
    DormantV1CaptureSubject::Pending,
    DormantV1CaptureSubject::Ownership,
    DormantV1CaptureSubject::Bridge,
}};

[[nodiscard]] QByteArray encoded(const QString &path) {
  return QFile::encodeName(path);
}

[[nodiscard]] bool chmodPath(const QString &path, const mode_t mode) {
  const auto bytes = encoded(path);
  return ::chmod(bytes.constData(), mode) == 0;
}

[[nodiscard]] bool makeDirectory(const QString &path, const mode_t mode) {
  return QDir().mkpath(path) && chmodPath(path, mode);
}

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &bytes,
                             const mode_t mode) {
  // Tests may deliberately rewrite a sealed file at a checkpoint.
  static_cast<void>(chmodPath(path, 0600));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file.write(bytes) != bytes.size() || !file.flush()) {
    return false;
  }
  file.close();
  return chmodPath(path, mode);
}

[[nodiscard]] QString digest(const QByteArray &bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] LegacyReadV1 &record(ReachableV1PreflightInput &input,
                                   const DormantV1CaptureSubject subject) {
  switch (subject) {
  case DormantV1CaptureSubject::Desired:
    return input.desired;
  case DormantV1CaptureSubject::LastGood:
    return input.lastGood;
  case DormantV1CaptureSubject::Applied:
    return input.applied;
  case DormantV1CaptureSubject::Pending:
    return input.pending;
  case DormantV1CaptureSubject::Ownership:
    return input.ownership;
  case DormantV1CaptureSubject::Bridge:
    return input.bridge;
  default:
    return input.desired;
  }
}

[[nodiscard]] const DormantV1FileCapture &
capturedRecord(const DormantV1FixedRecordCapture &capture,
               const DormantV1CaptureSubject subject) {
  switch (subject) {
  case DormantV1CaptureSubject::Desired:
    return capture.desired;
  case DormantV1CaptureSubject::LastGood:
    return capture.lastGood;
  case DormantV1CaptureSubject::Applied:
    return capture.applied;
  case DormantV1CaptureSubject::Pending:
    return capture.pending;
  case DormantV1CaptureSubject::Ownership:
    return capture.ownership;
  case DormantV1CaptureSubject::Bridge:
    return capture.bridge;
  default:
    return capture.desired;
  }
}

[[nodiscard]] QString recordName(const DormantV1CaptureSubject subject) {
  switch (subject) {
  case DormantV1CaptureSubject::Desired:
    return QStringLiteral("desired.json");
  case DormantV1CaptureSubject::LastGood:
    return QStringLiteral("last-good.json");
  case DormantV1CaptureSubject::Applied:
    return QStringLiteral("activation.json");
  case DormantV1CaptureSubject::Pending:
    return QStringLiteral("pending.json");
  case DormantV1CaptureSubject::Ownership:
    return QStringLiteral("entrypoint-ownership.json");
  case DormantV1CaptureSubject::Bridge:
    return QStringLiteral("live-activation.pending.json");
  default:
    return {};
  }
}

struct Fixture final {
  QTemporaryDir temporary;
  QString stateRoot;
  QString configRoot;
  QString managedRoot;
  QString generationsRoot;
  int stateFd = -1;
  int configFd = -1;
  int managedFd = -1;
  int generationsFd = -1;
  bool valid = false;

  explicit Fixture(const qsizetype additionalPathComponents = 0) {
    if (!temporary.isValid())
      return;
    if (!chmodPath(temporary.path(), 0700))
      return;
    auto root = temporary.path();
    for (qsizetype index = 0; index < additionalPathComponents; ++index) {
      root = QDir(root).filePath(QStringLiteral("d"));
    }
    stateRoot = QDir(root).filePath(QStringLiteral("state"));
    configRoot = QDir(root).filePath(QStringLiteral("config"));
    managedRoot = QDir(configRoot).filePath(QStringLiteral("hyprshelld"));
    generationsRoot = QDir(managedRoot).filePath(QStringLiteral("generations"));
    if (!makeDirectory(root, 0700) || !makeDirectory(stateRoot, 0700) ||
        !makeDirectory(configRoot, 0700) || !makeDirectory(managedRoot, 0700) ||
        !makeDirectory(generationsRoot, 0700))
      return;
    stateFd = openDirectory(stateRoot);
    configFd = openDirectory(configRoot);
    managedFd = openDirectory(managedRoot);
    generationsFd = openDirectory(generationsRoot);
    valid =
        stateFd >= 0 && configFd >= 0 && managedFd >= 0 && generationsFd >= 0;
  }

  ~Fixture() {
    if (generationsFd >= 0)
      ::close(generationsFd);
    if (managedFd >= 0)
      ::close(managedFd);
    if (configFd >= 0)
      ::close(configFd);
    if (stateFd >= 0)
      ::close(stateFd);
    if (!temporary.isValid())
      return;
    QStringList directories;
    QDirIterator iterator(temporary.path(), QDir::Dirs | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext())
      directories.append(iterator.next());
    std::sort(directories.begin(), directories.end(),
              [](const QString &left, const QString &right) {
                return left.size() > right.size();
              });
    for (const auto &path : directories) {
      static_cast<void>(chmodPath(path, 0700));
    }
    static_cast<void>(chmodPath(temporary.path(), 0700));
  }

  [[nodiscard]] static int openDirectory(const QString &path) {
    const auto bytes = encoded(path);
    return ::open(bytes.constData(),
                  O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  }

  [[nodiscard]] BorrowedDormantV1FilesystemRoots roots() const {
    return {
        .stateDirectoryFd = stateFd,
        .configDirectoryFd = configFd,
        .managedDirectoryFd = managedFd,
        .generationsDirectoryFd = generationsFd,
        .stateRoot = stateRoot,
        .configRoot = configRoot,
        .managedConfigRoot = managedRoot,
    };
  }

  [[nodiscard]] QString fixedPath(const DormantV1CaptureSubject subject) const {
    const auto root = subject == DormantV1CaptureSubject::Ownership ||
                              subject == DormantV1CaptureSubject::Bridge
                          ? managedRoot
                          : stateRoot;
    return QDir(root).filePath(recordName(subject));
  }

  [[nodiscard]] QString generationRoot(const QString &nonce) const {
    return QDir(generationsRoot).filePath(nonce);
  }

  [[nodiscard]] bool installRecord(ReachableV1PreflightInput &input,
                                   const DormantV1CaptureSubject subject,
                                   const QByteArray &bytes) const {
    if (!writeFile(fixedPath(subject), bytes, 0600))
      return false;
    record(input, subject) = {
        .kind = LegacyReadKindV1::ExactRegular,
        .bytes = bytes,
    };
    return true;
  }
};

[[nodiscard]] LegacyGenerationEvidenceV1 evidenceFor(const Fixture &fixture,
                                                     const QString &nonce) {
  LegacyGenerationEvidenceV1 evidence;
  evidence.desiredBytes =
      QByteArrayLiteral("desired-evidence:") + nonce.toLatin1();
  evidence.manifestBytes = QByteArrayLiteral("manifest:") + nonce.toLatin1();
  evidence.files.insert(QStringLiteral("hyprland.lua"),
                        QByteArrayLiteral("hyprland:") + nonce.toLatin1());
  for (const auto *name : moduleNames) {
    const auto path = QStringLiteral("modules/") + QString::fromLatin1(name);
    evidence.files.insert(path, QByteArrayLiteral("module:") +
                                    QByteArray(name) + ':' + nonce.toLatin1());
  }
  evidence.expected.activationNonce = nonce;
  evidence.expected.generationRoot = fixture.generationRoot(nonce);
  evidence.expected.userCustomPath =
      QDir(fixture.configRoot).filePath(QStringLiteral("user-custom.lua"));
  return evidence;
}

[[nodiscard]] bool
installGeneration(const Fixture &fixture,
                  const LegacyGenerationEvidenceV1 &evidence) {
  const auto root = evidence.expected.generationRoot;
  const auto modules = QDir(root).filePath(QStringLiteral("modules"));
  if (!makeDirectory(root, 0700) || !makeDirectory(modules, 0700) ||
      !writeFile(QDir(root).filePath(QStringLiteral("manifest.json")),
                 evidence.manifestBytes, 0400))
    return false;
  for (auto iterator = evidence.files.constBegin();
       iterator != evidence.files.constEnd(); ++iterator) {
    if (!writeFile(QDir(root).filePath(iterator.key()), iterator.value(), 0400))
      return false;
  }
  return chmodPath(modules, 0500) && chmodPath(root, 0500);
}

void compareScrubbed(const DormantV1FileCapture &capture) {
  QCOMPARE(capture.kind, DormantV1FileCaptureKind::Missing);
  QVERIFY(capture.bytes.isEmpty());
  QVERIFY(capture.sha256.isEmpty());
  QCOMPARE(capture.identity, DormantV1FileIdentity{});
}

void compareFailure(
    const DormantV1FilesystemCaptureResult &result,
    const DormantV1CaptureReason reason,
    const DormantV1CaptureSubject subject = DormantV1CaptureSubject::None,
    const qsizetype generationIndex = -1) {
  QCOMPARE(result.disposition, DormantV1CaptureDisposition::FailedClosed);
  QCOMPARE(result.reason, reason);
  QCOMPARE(result.subject, subject);
  QCOMPARE(result.generationIndex, generationIndex);
  QVERIFY(!result.capture.has_value());
}

} // namespace

class CompositorDormantV1FilesystemCaptureTest final : public QObject {
  Q_OBJECT

private slots:
  void cleanup() { DormantV1CaptureTestSupport::clearCheckpointHook(); }

  void publicBoundsAreFrozen();
  void capturesPathWithMoreThan64Components();
  void capturesZeroOneOrTwoTrees_data();
  void capturesZeroOneOrTwoTrees();
  void returnedStorageOwnsCallerIndependentData();
  void tentativeFailureOwnsRawBackedNonce();
  void rejectsExpectedReadGrammar();
  void rejectsGenerationInputBoundsAndAliases();
  void rejectsFixedRecordSafetyClasses();
  void rejectsRootSafetyClasses();
  void rejectsGenerationTreeSafetyClasses();
  void ignoresOutOfScopeStableAndArtifactNames();
  void aggregatePassOrderingIsDeterministic();
  void passBDetectsFixedRecordMutation();
  void passBDetectsGenerationIdentityMutation();
  void passACompletesEarlierTreeBeforeLaterTree();
  void namedFileRaceFailsClosed();
  void finalRootDriftOverridesLocalMismatch();
};

void CompositorDormantV1FilesystemCaptureTest::publicBoundsAreFrozen() {
  QCOMPARE(maximumDormantV1MetadataCaptureBytes, 4 * 1024 * 1024);
  QCOMPARE(maximumDormantV1GeneratedFileCaptureBytes, 16 * 1024 * 1024);
  QCOMPARE(maximumDormantV1CapturedGenerations, 2);
  QCOMPARE(maximumDormantV1RetainedPayloadBytes, 603979776ULL);
  QCOMPARE(maximumDormantV1TwoPassPayloadReadBytes, 1207959552ULL);
  QCOMPARE(maximumDormantV1CallerPayloadBytes, 612368384ULL);
  QCOMPARE(dormantV1CaptureReadBufferBytes, 64 * 1024);
  QCOMPARE(maximumDormantV1UniqueFiles, 42);
  QCOMPARE(maximumDormantV1IdentityReceipts, 50);
  QCOMPARE(maximumDormantV1ExpectedDirents, 76);
  QCOMPARE(maximumDormantV1ObservedDirents, 84);
  QCOMPARE(maximumDormantV1DirentNameBytes, 1048576ULL);
  QCOMPARE(maximumDormantV1EnumerationCalls, 128ULL);
  QCOMPARE(maximumDormantV1SyscallAttempts, 143481ULL);
  QCOMPARE(dormantV1RootCleanupAttemptReserve, 8200ULL);
  QCOMPARE(dormantV1BodyCleanupAttemptReserve, 512ULL);
  QCOMPARE(dormantV1InitialRootProofAttemptLimit, 49205ULL);
  QCOMPARE(dormantV1FinalRootProofAttemptLimit, 49204ULL);
  QCOMPARE(dormantV1InitialRootAttemptReserve, 57405ULL);
  QCOMPARE(dormantV1BodyAttemptLimit, 28672ULL);
  QCOMPARE(dormantV1FinalRootAttemptReserve, 57404ULL);
  QCOMPARE(maximumDormantV1PathUtf8Bytes, 4096);
  QCOMPARE(maximumDormantV1PathCodeUnits, 4096);
  QCOMPARE(maximumDormantV1PathComponents, 2048);
  QCOMPARE(maximumDormantV1RelativePathUtf8Bytes, 64);
  QCOMPARE(maximumDormantV1RelativePathCodeUnits, 64);
}

void CompositorDormantV1FilesystemCaptureTest::
    capturesPathWithMoreThan64Components() {
  Fixture fixture(70);
  QVERIFY(fixture.valid);
  QVERIFY(fixture.stateRoot.split(QLatin1Char('/'), Qt::SkipEmptyParts).size() >
          64);
  const auto result = captureDormantReachableV1Filesystem(fixture.roots(), {});
  QCOMPARE(result.disposition, DormantV1CaptureDisposition::Captured);
  QVERIFY(result.capture.has_value());
}

void CompositorDormantV1FilesystemCaptureTest::
    capturesZeroOneOrTwoTrees_data() {
  QTest::addColumn<int>("count");
  QTest::newRow("zero") << 0;
  QTest::newRow("one") << 1;
  QTest::newRow("two-reversed") << 2;
}

void CompositorDormantV1FilesystemCaptureTest::capturesZeroOneOrTwoTrees() {
  QFETCH(int, count);
  Fixture fixture;
  QVERIFY(fixture.valid);
  ReachableV1PreflightInput input;
  QVERIFY(
      fixture.installRecord(input, DormantV1CaptureSubject::Desired,
                            QByteArrayLiteral("arbitrary desired bytes\n")));
  QVERIFY(fixture.installRecord(input, DormantV1CaptureSubject::Bridge,
                                QByteArrayLiteral("not parsed as a bridge\n")));
  if (count >= 1) {
    auto first = evidenceFor(fixture, count == 2 ? QString::fromLatin1(nonceB)
                                                 : QString::fromLatin1(nonceA));
    QVERIFY(installGeneration(fixture, first));
    input.referencedGenerations.append(first);
  }
  if (count == 2) {
    auto second = evidenceFor(fixture, QString::fromLatin1(nonceA));
    QVERIFY(installGeneration(fixture, second));
    input.referencedGenerations.append(second);
  }

  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QCOMPARE(result.disposition, DormantV1CaptureDisposition::Captured);
  QCOMPARE(result.reason, DormantV1CaptureReason::None);
  QCOMPARE(result.subject, DormantV1CaptureSubject::None);
  QCOMPARE(result.generationIndex, -1);
  QVERIFY(result.validatedNonce.isEmpty());
  QVERIFY(result.relativePath.isEmpty());
  QVERIFY(result.capture.has_value());
  const auto &capture = *result.capture;
  QCOMPARE(capture.generations.size(), count);
  QCOMPARE(capture.roots.state.mode, 0700U);
  QCOMPARE(capture.roots.config.mode, 0700U);
  QCOMPARE(capture.roots.managed.mode, 0700U);
  QCOMPARE(capture.roots.generations.mode, 0700U);
  QVERIFY(capture.roots.state.device != 0);
  QVERIFY(capture.roots.state.inode != 0);
  QVERIFY(capture.roots.state.linkCount >= 2);
  QCOMPARE(capture.records.desired.bytes, input.desired.bytes);
  QCOMPARE(capture.records.desired.sha256, digest(input.desired.bytes));
  QCOMPARE(capture.records.desired.identity.mode, 0600U);
  compareScrubbed(capture.records.lastGood);
  compareScrubbed(capture.records.applied);
  compareScrubbed(capture.records.pending);
  compareScrubbed(capture.records.ownership);
  QCOMPARE(capture.records.bridge.bytes, input.bridge.bytes);
  if (count == 2) {
    QCOMPARE(capture.generations.at(0).activationNonce,
             QString::fromLatin1(nonceA));
    QCOMPARE(capture.generations.at(1).activationNonce,
             QString::fromLatin1(nonceB));
  }
  for (const auto &tree : capture.generations) {
    const auto &expected = *std::find_if(
        input.referencedGenerations.cbegin(),
        input.referencedGenerations.cend(),
        [&](const LegacyGenerationEvidenceV1 &value) {
          return value.expected.activationNonce == tree.activationNonce;
        });
    QCOMPARE(tree.rootIdentity.mode, 0500U);
    QCOMPARE(tree.modulesIdentity.mode, 0500U);
    QCOMPARE(tree.rootIdentity.device, capture.roots.generations.device);
    QCOMPARE(tree.modulesIdentity.device, tree.rootIdentity.device);
    QCOMPARE(tree.manifest.bytes, expected.manifestBytes);
    QCOMPARE(tree.manifest.sha256, digest(expected.manifestBytes));
    QCOMPARE(tree.manifest.identity.mode, 0400U);
    QCOMPARE(tree.files.size(), 17);
    for (auto iterator = tree.files.constBegin();
         iterator != tree.files.constEnd(); ++iterator) {
      QCOMPARE(iterator->bytes, expected.files.value(iterator.key()));
      QCOMPARE(iterator->sha256, digest(iterator->bytes));
      QCOMPARE(iterator->identity.mode, 0400U);
    }
  }
}

void CompositorDormantV1FilesystemCaptureTest::
    returnedStorageOwnsCallerIndependentData() {
  Fixture fixture;
  QVERIFY(fixture.valid);

  const auto fixedOriginal = QByteArrayLiteral("raw-backed-fixed");
  const auto originalNonce = QString::fromLatin1(nonceA);
  auto evidence = evidenceFor(fixture, originalNonce);
  const auto manifestOriginal = evidence.manifestBytes;
  const auto payloadOriginal =
      evidence.files.value(QStringLiteral("hyprland.lua"));

  std::array<char, 256> fixedBacking{};
  std::array<char, 256> manifestBacking{};
  std::array<char, 256> payloadBacking{};
  std::array<QChar, 128> nonceBacking{};
  QVERIFY(fixedOriginal.size() <= static_cast<qsizetype>(fixedBacking.size()));
  QVERIFY(manifestOriginal.size() <=
          static_cast<qsizetype>(manifestBacking.size()));
  QVERIFY(payloadOriginal.size() <=
          static_cast<qsizetype>(payloadBacking.size()));
  std::copy(fixedOriginal.cbegin(), fixedOriginal.cend(), fixedBacking.begin());
  std::copy(manifestOriginal.cbegin(), manifestOriginal.cend(),
            manifestBacking.begin());
  std::copy(payloadOriginal.cbegin(), payloadOriginal.cend(),
            payloadBacking.begin());
  std::copy(originalNonce.cbegin(), originalNonce.cend(), nonceBacking.begin());

  ReachableV1PreflightInput input;
  QVERIFY(writeFile(fixture.fixedPath(DormantV1CaptureSubject::Desired),
                    fixedOriginal, 0600));
  input.desired = {
      .kind = LegacyReadKindV1::ExactRegular,
      .bytes =
          QByteArray::fromRawData(fixedBacking.data(), fixedOriginal.size()),
  };
  evidence.manifestBytes =
      QByteArray::fromRawData(manifestBacking.data(), manifestOriginal.size());
  evidence.files[QStringLiteral("hyprland.lua")] =
      QByteArray::fromRawData(payloadBacking.data(), payloadOriginal.size());
  evidence.expected.activationNonce =
      QString::fromRawData(nonceBacking.data(), originalNonce.size());
  QVERIFY(installGeneration(fixture, evidence));
  input.referencedGenerations.append(evidence);

  auto mutated = false;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (mutated ||
            event.checkpoint !=
                DormantV1CaptureTestSupport::Checkpoint::BeforeFinalRootGuard)
          return;
        mutated = true;
        fixedBacking.front() = 'X';
        manifestBacking.front() = 'X';
        payloadBacking.front() = 'X';
        nonceBacking.front() = u'f';
      });
  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QVERIFY(mutated);
  QCOMPARE(result.disposition, DormantV1CaptureDisposition::Captured);
  QVERIFY(result.capture.has_value());
  QCOMPARE(result.capture->records.desired.bytes, fixedOriginal);
  QCOMPARE(result.capture->records.desired.sha256, digest(fixedOriginal));
  QCOMPARE(result.capture->generations.size(), 1);
  const auto &captured = result.capture->generations.constFirst();
  QCOMPARE(captured.activationNonce, originalNonce);
  QCOMPARE(captured.manifest.bytes, manifestOriginal);
  QCOMPARE(captured.manifest.sha256, digest(manifestOriginal));
  QCOMPARE(captured.files.value(QStringLiteral("hyprland.lua")).bytes,
           payloadOriginal);
  QCOMPARE(captured.files.value(QStringLiteral("hyprland.lua")).sha256,
           digest(payloadOriginal));

  DormantV1CaptureTestSupport::clearCheckpointHook();
  std::array<QChar, 128> failureNonceBacking{};
  std::array<QChar, 64> failurePathBacking{};
  const auto failurePath = QStringLiteral("hyprland.lua");
  std::copy(originalNonce.cbegin(), originalNonce.cend(),
            failureNonceBacking.begin());
  std::copy(failurePath.cbegin(), failurePath.cend(),
            failurePathBacking.begin());
  DormantV1FilesystemCaptureResult failureResult;
  {
    auto failureEvidence = evidenceFor(fixture, originalNonce);
    failureEvidence.expected.activationNonce =
        QString::fromRawData(failureNonceBacking.data(), originalNonce.size());
    failureEvidence.files.remove(failurePath);
    failureEvidence.files.insert(
        QString::fromRawData(failurePathBacking.data(), failurePath.size()),
        QByteArray(maximumDormantV1GeneratedFileCaptureBytes + 1, 'x'));
    ReachableV1PreflightInput failureInput;
    failureInput.referencedGenerations.append(failureEvidence);
    failureResult =
        captureDormantReachableV1Filesystem(fixture.roots(), failureInput);
  }
  failureNonceBacking.front() = u'f';
  failurePathBacking.front() = u'X';
  compareFailure(failureResult,
                 DormantV1CaptureReason::GenerationEvidenceOversized,
                 DormantV1CaptureSubject::GenerationPayload, 0);
  QCOMPARE(failureResult.validatedNonce, originalNonce);
  QCOMPARE(failureResult.relativePath, failurePath);
}

void CompositorDormantV1FilesystemCaptureTest::
    tentativeFailureOwnsRawBackedNonce() {
  Fixture fixture;
  QVERIFY(fixture.valid);

  const auto originalNonce = QString::fromLatin1(nonceA);
  std::array<QChar, 128> nonceBacking{};
  std::copy(originalNonce.cbegin(), originalNonce.cend(), nonceBacking.begin());

  auto evidence = evidenceFor(fixture, originalNonce);
  evidence.expected.activationNonce =
      QString::fromRawData(nonceBacking.data(), originalNonce.size());
  QVERIFY(installGeneration(fixture, evidence));
  QVERIFY(writeFile(QDir(evidence.expected.generationRoot)
                        .filePath(QStringLiteral("hyprland.lua")),
                    QByteArrayLiteral("filesystem-mismatch"), 0400));

  ReachableV1PreflightInput input;
  input.referencedGenerations.append(evidence);
  auto mutated = false;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (mutated ||
            event.checkpoint !=
                DormantV1CaptureTestSupport::Checkpoint::BeforeFinalRootGuard)
          return;
        mutated = true;
        nonceBacking.front() = u'f';
      });

  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QVERIFY(mutated);
  compareFailure(result, DormantV1CaptureReason::GenerationFileMismatch,
                 DormantV1CaptureSubject::GenerationPayload, 0);
  QCOMPARE(result.validatedNonce, originalNonce);
  QCOMPARE(result.relativePath, QStringLiteral("hyprland.lua"));
}

void CompositorDormantV1FilesystemCaptureTest::rejectsExpectedReadGrammar() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  for (const auto subject : fixedSubjects) {
    ReachableV1PreflightInput input;
    record(input, subject).kind = LegacyReadKindV1::Unsafe;
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::UnsafeExpectedRead, subject);
    QVERIFY(result.validatedNonce.isEmpty());
    QVERIFY(result.relativePath.isEmpty());
  }
  {
    ReachableV1PreflightInput input;
    input.desired.bytes = QByteArrayLiteral("bytes on Missing");
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::InvalidExpectedRead,
                   DormantV1CaptureSubject::Desired);
  }
  {
    ReachableV1PreflightInput input;
    input.desired.kind = LegacyReadKindV1::ExactRegular;
    input.desired.bytes =
        QByteArray(maximumDormantV1MetadataCaptureBytes + 1, 'x');
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::ExpectedRecordOversized,
                   DormantV1CaptureSubject::Desired);
  }
  {
    ReachableV1PreflightInput input;
    input.desired.kind = static_cast<LegacyReadKindV1>(99);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::InvalidExpectedRead,
                   DormantV1CaptureSubject::Desired);
  }
}

void CompositorDormantV1FilesystemCaptureTest::
    rejectsGenerationInputBoundsAndAliases() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  {
    ReachableV1PreflightInput input;
    input.referencedGenerations.resize(3);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::TooManyGenerationEvidences);
  }
  {
    ReachableV1PreflightInput input;
    auto evidence = evidenceFor(fixture, QStringLiteral("not-a-nonce"));
    input.referencedGenerations.append(evidence);
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::InvalidGenerationReference,
                   DormantV1CaptureSubject::None, 0);
    QVERIFY(result.validatedNonce.isEmpty());
  }
  {
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
    input.referencedGenerations = {evidence, evidence};
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::DuplicateGenerationReference,
                   DormantV1CaptureSubject::None, 1);
    QVERIFY(result.validatedNonce.isEmpty());
  }
  {
    ReachableV1PreflightInput input;
    auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
    evidence.expected.generationRoot.append(QStringLiteral("/wrong"));
    input.referencedGenerations.append(evidence);
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::InvalidGenerationPaths,
                   DormantV1CaptureSubject::None, 0);
    QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceA));
  }
  {
    ReachableV1PreflightInput input;
    auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
    evidence.expected.userCustomPath =
        QDir(fixture.configRoot).filePath(QStringLiteral("other.lua"));
    input.referencedGenerations.append(evidence);
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::InvalidGenerationPaths,
                   DormantV1CaptureSubject::None, 0);
    QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceA));
  }
  {
    ReachableV1PreflightInput input;
    auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
    evidence.files.remove(QStringLiteral("hyprland.lua"));
    input.referencedGenerations.append(evidence);
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::InvalidGenerationInventory,
                   DormantV1CaptureSubject::None, 0);
    QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceA));
  }
  {
    ReachableV1PreflightInput input;
    auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
    evidence.files.remove(QStringLiteral("hyprland.lua"));
    evidence.files.insert(QString(65, QLatin1Char('x')), QByteArray{});
    input.referencedGenerations.append(evidence);
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::InvalidGenerationInventory,
                   DormantV1CaptureSubject::None, 0);
    QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceA));
    QVERIFY(result.relativePath.isEmpty());
  }
  {
    ReachableV1PreflightInput input;
    auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
    evidence.manifestBytes =
        QByteArray(maximumDormantV1MetadataCaptureBytes + 1, 'm');
    input.referencedGenerations.append(evidence);
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::GenerationEvidenceOversized,
                   DormantV1CaptureSubject::None, 0);
    QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceA));
  }
  {
    ReachableV1PreflightInput input;
    auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
    evidence.files[QStringLiteral("hyprland.lua")] =
        QByteArray(maximumDormantV1GeneratedFileCaptureBytes + 1, 'p');
    input.referencedGenerations.append(evidence);
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::GenerationEvidenceOversized,
                   DormantV1CaptureSubject::GenerationPayload, 0);
    QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceA));
    QCOMPARE(result.relativePath, QStringLiteral("hyprland.lua"));
  }
}

void CompositorDormantV1FilesystemCaptureTest::
    rejectsFixedRecordSafetyClasses() {
  for (const auto subject : fixedSubjects) {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    QVERIFY(fixture.installRecord(input, subject, QByteArrayLiteral("x")));
    QVERIFY(chmodPath(fixture.fixedPath(subject), 0400));
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::FixedRecordUnsafe, subject);
  }

  for (const auto kind : {0, 1, 2, 3, 4}) {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto path = fixture.fixedPath(DormantV1CaptureSubject::Desired);
    QVERIFY(fixture.installRecord(input, DormantV1CaptureSubject::Desired,
                                  QByteArrayLiteral("expected")));
    const auto pathBytes = encoded(path);
    if (kind == 0) {
      QVERIFY(::unlink(pathBytes.constData()) == 0);
      const auto target = QDir(fixture.temporary.path())
                              .filePath(QStringLiteral("symlink-target"));
      QVERIFY(writeFile(target, QByteArrayLiteral("expected"), 0600));
      const auto targetBytes = encoded(target);
      QVERIFY(::symlink(targetBytes.constData(), pathBytes.constData()) == 0);
    } else if (kind == 1) {
      QVERIFY(::unlink(pathBytes.constData()) == 0);
      QVERIFY(makeDirectory(path, 0700));
    } else if (kind == 2) {
      const auto alias = QDir(fixture.temporary.path())
                             .filePath(QStringLiteral("hardlink-alias"));
      const auto aliasBytes = encoded(alias);
      QVERIFY(::link(pathBytes.constData(), aliasBytes.constData()) == 0);
    } else if (kind == 3) {
      QVERIFY(writeFile(path, QByteArrayLiteral("different"), 0600));
    } else {
      const auto descriptor =
          ::open(pathBytes.constData(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
      QVERIFY(descriptor >= 0);
      QVERIFY(::ftruncate(descriptor,
                          maximumDormantV1MetadataCaptureBytes + 1) == 0);
      QVERIFY(::close(descriptor) == 0);
    }
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result,
                   kind == 3 ? DormantV1CaptureReason::FixedRecordMismatch
                             : DormantV1CaptureReason::FixedRecordUnsafe,
                   DormantV1CaptureSubject::Desired);
  }
}

void CompositorDormantV1FilesystemCaptureTest::rejectsRootSafetyClasses() {
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    auto roots = fixture.roots();
    roots.stateDirectoryFd = -1;
    compareFailure(captureDormantReachableV1Filesystem(roots, {}),
                   DormantV1CaptureReason::InvalidRootDescriptors,
                   DormantV1CaptureSubject::StateRoot);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    const auto flags = ::fcntl(fixture.configFd, F_GETFD);
    QVERIFY(flags >= 0);
    QVERIFY(::fcntl(fixture.configFd, F_SETFD, flags & ~FD_CLOEXEC) == 0);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), {}),
                   DormantV1CaptureReason::InvalidRootDescriptors,
                   DormantV1CaptureSubject::ConfigRoot);
    QVERIFY(::fcntl(fixture.configFd, F_SETFD, flags) == 0);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    auto roots = fixture.roots();
    roots.stateRoot.append(QStringLiteral("/."));
    compareFailure(captureDormantReachableV1Filesystem(roots, {}),
                   DormantV1CaptureReason::InvalidRootPath,
                   DormantV1CaptureSubject::StateRoot);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    auto roots = fixture.roots();
    roots.managedConfigRoot =
        QDir(fixture.configRoot).filePath(QStringLiteral("other"));
    compareFailure(captureDormantReachableV1Filesystem(roots, {}),
                   DormantV1CaptureReason::InvalidRootLayout,
                   DormantV1CaptureSubject::ManagedRoot);
  }
  struct ModeCase final {
    DormantV1CaptureSubject subject;
    mode_t mode;
  };
  const std::array<ModeCase, 4> modeCases{{
      {DormantV1CaptureSubject::StateRoot, 0755},
      {DormantV1CaptureSubject::ConfigRoot, 0770},
      {DormantV1CaptureSubject::ManagedRoot, 0755},
      {DormantV1CaptureSubject::GenerationsRoot, 0755},
  }};
  for (const auto &test : modeCases) {
    Fixture fixture;
    QVERIFY(fixture.valid);
    QString path;
    switch (test.subject) {
    case DormantV1CaptureSubject::StateRoot:
      path = fixture.stateRoot;
      break;
    case DormantV1CaptureSubject::ConfigRoot:
      path = fixture.configRoot;
      break;
    case DormantV1CaptureSubject::ManagedRoot:
      path = fixture.managedRoot;
      break;
    case DormantV1CaptureSubject::GenerationsRoot:
      path = fixture.generationsRoot;
      break;
    default:
      break;
    }
    QVERIFY(chmodPath(path, test.mode));
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), {}),
                   DormantV1CaptureReason::UnsafeRootMetadata, test.subject);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    auto roots = fixture.roots();
    roots.stateRoot = fixture.configRoot;
    compareFailure(captureDormantReachableV1Filesystem(roots, {}),
                   DormantV1CaptureReason::RootIdentityMismatch,
                   DormantV1CaptureSubject::StateRoot);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    auto roots = fixture.roots();
    roots.stateRoot = fixture.configRoot;
    roots.stateDirectoryFd = fixture.configFd;
    compareFailure(captureDormantReachableV1Filesystem(roots, {}),
                   DormantV1CaptureReason::RootIdentityMismatch,
                   DormantV1CaptureSubject::ConfigRoot);
  }
}

void CompositorDormantV1FilesystemCaptureTest::
    rejectsGenerationTreeSafetyClasses() {
  const auto nonce = QString::fromLatin1(nonceA);
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    input.referencedGenerations.append(evidenceFor(fixture, nonce));
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    compareFailure(result, DormantV1CaptureReason::GenerationRootMissing,
                   DormantV1CaptureSubject::GenerationRoot, 0);
    QCOMPARE(result.validatedNonce, nonce);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, nonce);
    QVERIFY(installGeneration(fixture, evidence));
    QVERIFY(chmodPath(evidence.expected.generationRoot, 0700));
    input.referencedGenerations.append(evidence);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::GenerationRootUnsafe,
                   DormantV1CaptureSubject::GenerationRoot, 0);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, nonce);
    QVERIFY(installGeneration(fixture, evidence));
    const auto modules = QDir(evidence.expected.generationRoot)
                             .filePath(QStringLiteral("modules"));
    QVERIFY(chmodPath(evidence.expected.generationRoot, 0700));
    QVERIFY(chmodPath(modules, 0700));
    QVERIFY(QDir(modules).removeRecursively());
    QVERIFY(chmodPath(evidence.expected.generationRoot, 0500));
    input.referencedGenerations.append(evidence);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::GenerationModulesMissing,
                   DormantV1CaptureSubject::GenerationModules, 0);
  }
  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, nonce);
    QVERIFY(installGeneration(fixture, evidence));
    const auto modules = QDir(evidence.expected.generationRoot)
                             .filePath(QStringLiteral("modules"));
    QVERIFY(chmodPath(modules, 0700));
    input.referencedGenerations.append(evidence);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::GenerationModulesUnsafe,
                   DormantV1CaptureSubject::GenerationModules, 0);
  }

  for (const auto inModules : {false, true}) {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, nonce);
    QVERIFY(installGeneration(fixture, evidence));
    const auto directory = inModules ? QDir(evidence.expected.generationRoot)
                                           .filePath(QStringLiteral("modules"))
                                     : evidence.expected.generationRoot;
    QVERIFY(chmodPath(directory, 0700));
    QVERIFY(writeFile(QDir(directory).filePath(QStringLiteral("unexpected")),
                      QByteArrayLiteral("extra"), 0400));
    QVERIFY(chmodPath(directory, 0500));
    input.referencedGenerations.append(evidence);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::GenerationInventoryMismatch,
                   inModules ? DormantV1CaptureSubject::GenerationModules
                             : DormantV1CaptureSubject::GenerationRoot,
                   0);
  }

  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, nonce);
    QVERIFY(installGeneration(fixture, evidence));
    QVERIFY(chmodPath(QDir(evidence.expected.generationRoot)
                          .filePath(QStringLiteral("manifest.json")),
                      0600));
    input.referencedGenerations.append(evidence);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   DormantV1CaptureReason::GenerationFileUnsafe,
                   DormantV1CaptureSubject::GenerationManifest, 0);
  }

  for (const auto kind : {0, 1, 2, 3}) {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, nonce);
    QVERIFY(installGeneration(fixture, evidence));
    const auto path = QDir(evidence.expected.generationRoot)
                          .filePath(QStringLiteral("hyprland.lua"));
    const auto pathBytes = encoded(path);
    if (kind == 0) {
      QVERIFY(chmodPath(evidence.expected.generationRoot, 0700));
      QVERIFY(::unlink(pathBytes.constData()) == 0);
      const auto target = QDir(fixture.temporary.path())
                              .filePath(QStringLiteral("generation-target"));
      QVERIFY(writeFile(
          target, evidence.files.value(QStringLiteral("hyprland.lua")), 0400));
      const auto targetBytes = encoded(target);
      QVERIFY(::symlink(targetBytes.constData(), pathBytes.constData()) == 0);
      QVERIFY(chmodPath(evidence.expected.generationRoot, 0500));
    } else if (kind == 1) {
      const auto alias = QDir(fixture.temporary.path())
                             .filePath(QStringLiteral("generation-hardlink"));
      const auto aliasBytes = encoded(alias);
      QVERIFY(::link(pathBytes.constData(), aliasBytes.constData()) == 0);
    } else if (kind == 2) {
      QVERIFY(writeFile(path, QByteArrayLiteral("different"), 0400));
    } else {
      QVERIFY(chmodPath(path, 0600));
      const auto descriptor =
          ::open(pathBytes.constData(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
      QVERIFY(descriptor >= 0);
      QVERIFY(::ftruncate(descriptor,
                          maximumDormantV1GeneratedFileCaptureBytes + 1) == 0);
      QVERIFY(::close(descriptor) == 0);
      QVERIFY(chmodPath(path, 0400));
    }
    input.referencedGenerations.append(evidence);
    compareFailure(captureDormantReachableV1Filesystem(fixture.roots(), input),
                   kind == 2 ? DormantV1CaptureReason::GenerationFileMismatch
                             : DormantV1CaptureReason::GenerationFileUnsafe,
                   DormantV1CaptureSubject::GenerationPayload, 0);
  }

  {
    Fixture fixture;
    QVERIFY(fixture.valid);
    ReachableV1PreflightInput input;
    const auto evidence = evidenceFor(fixture, nonce);
    QVERIFY(installGeneration(fixture, evidence));
    input.referencedGenerations.append(evidence);
    auto removed = false;
    DormantV1CaptureTestSupport::setCheckpointHook(
        [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
          if (removed ||
              event.checkpoint != DormantV1CaptureTestSupport::Checkpoint::
                                      AfterGenerationInventory ||
              event.pass != DormantV1CaptureTestSupport::Pass::First ||
              event.generationIndex != 0)
            return;
          const auto path = QDir(evidence.expected.generationRoot)
                                .filePath(QStringLiteral("manifest.json"));
          const auto bytes = encoded(path);
          removed = chmodPath(evidence.expected.generationRoot, 0700) &&
                    ::unlink(bytes.constData()) == 0 &&
                    chmodPath(evidence.expected.generationRoot, 0500);
        });
    const auto result =
        captureDormantReachableV1Filesystem(fixture.roots(), input);
    QVERIFY(removed);
    compareFailure(result, DormantV1CaptureReason::GenerationFileMissing,
                   DormantV1CaptureSubject::GenerationManifest, 0);
    QCOMPARE(result.validatedNonce, nonce);
    QCOMPARE(result.relativePath, QStringLiteral("manifest.json"));
  }
}

void CompositorDormantV1FilesystemCaptureTest::
    ignoresOutOfScopeStableAndArtifactNames() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  const auto stable =
      QDir(fixture.configRoot).filePath(QStringLiteral("hyprland.lua"));
  const auto stableBytes = encoded(stable);
  QVERIFY(::symlink("/etc/passwd", stableBytes.constData()) == 0);
  QVERIFY(writeFile(
      QDir(fixture.configRoot).filePath(QStringLiteral("hyprland.lua~")),
      QByteArrayLiteral("backup"), 0666));
  QVERIFY(
      writeFile(QDir(fixture.managedRoot).filePath(QStringLiteral("swap.tmp")),
                QByteArrayLiteral("temporary"), 0666));
  QVERIFY(writeFile(
      QDir(fixture.stateRoot).filePath(QStringLiteral("unrelated.lock")),
      QByteArrayLiteral("unrelated"), 0666));
  const auto result = captureDormantReachableV1Filesystem(fixture.roots(), {});
  QCOMPARE(result.disposition, DormantV1CaptureDisposition::Captured);
  QVERIFY(result.capture.has_value());
  for (const auto subject : fixedSubjects) {
    compareScrubbed(capturedRecord(result.capture->records, subject));
  }
}

void CompositorDormantV1FilesystemCaptureTest::
    aggregatePassOrderingIsDeterministic() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  ReachableV1PreflightInput input;
  const auto evidenceB = evidenceFor(fixture, QString::fromLatin1(nonceB));
  const auto evidenceA = evidenceFor(fixture, QString::fromLatin1(nonceA));
  QVERIFY(installGeneration(fixture, evidenceB));
  QVERIFY(installGeneration(fixture, evidenceA));
  input.referencedGenerations = {evidenceB, evidenceA};

  using Checkpoint = DormantV1CaptureTestSupport::Checkpoint;
  using Pass = DormantV1CaptureTestSupport::Pass;
  struct Event final {
    Checkpoint checkpoint;
    Pass pass;
    DormantV1CaptureSubject subject;
    qsizetype generationIndex;
    QString relativePath;
  };
  QVector<Event> events;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (event.checkpoint == Checkpoint::AfterFixedRecord ||
            event.checkpoint == Checkpoint::AfterGenerationFile ||
            event.checkpoint == Checkpoint::BetweenPasses) {
          events.append({
              event.checkpoint,
              event.pass,
              event.subject,
              event.generationIndex,
              event.relativePath,
          });
        }
      });
  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QCOMPARE(result.disposition, DormantV1CaptureDisposition::Captured);

  qsizetype between = -1;
  for (qsizetype index = 0; index < events.size(); ++index) {
    if (events.at(index).checkpoint == Checkpoint::BetweenPasses) {
      between = index;
      break;
    }
  }
  QVERIFY(between > 0);
  qsizetype firstFirstTree = -1;
  qsizetype firstSecondTree = -1;
  QVector<qsizetype> firstManifestOrder;
  for (qsizetype index = 0; index < events.size(); ++index) {
    const auto &event = events.at(index);
    if (event.checkpoint == Checkpoint::AfterGenerationFile &&
        event.pass == Pass::First) {
      if (firstFirstTree < 0)
        firstFirstTree = index;
      if (event.relativePath == QStringLiteral("manifest.json")) {
        firstManifestOrder.append(event.generationIndex);
      }
    }
    if (event.checkpoint == Checkpoint::AfterGenerationFile &&
        event.pass == Pass::Second && firstSecondTree < 0) {
      firstSecondTree = index;
    }
  }
  QCOMPARE(firstManifestOrder, QVector<qsizetype>({1, 0}));
  QVERIFY(firstFirstTree > 5);
  QVERIFY(firstFirstTree < between);
  QVERIFY(firstSecondTree > between);
  for (qsizetype index = 0; index < firstFirstTree; ++index) {
    QCOMPARE(events.at(index).checkpoint, Checkpoint::AfterFixedRecord);
    QCOMPARE(events.at(index).pass, Pass::First);
  }
  for (qsizetype index = between + 1; index < firstSecondTree; ++index) {
    QCOMPARE(events.at(index).checkpoint, Checkpoint::AfterFixedRecord);
    QCOMPARE(events.at(index).pass, Pass::Second);
  }
}

void CompositorDormantV1FilesystemCaptureTest::
    passBDetectsFixedRecordMutation() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  ReachableV1PreflightInput input;
  QVERIFY(fixture.installRecord(input, DormantV1CaptureSubject::Desired,
                                QByteArrayLiteral("pass-a")));
  auto mutated = false;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (mutated ||
            event.checkpoint !=
                DormantV1CaptureTestSupport::Checkpoint::BetweenPasses)
          return;
        mutated = writeFile(fixture.fixedPath(DormantV1CaptureSubject::Desired),
                            QByteArrayLiteral("pass-b"), 0600);
      });
  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QVERIFY(mutated);
  compareFailure(result, DormantV1CaptureReason::FixedRecordMismatch,
                 DormantV1CaptureSubject::Desired);
}

void CompositorDormantV1FilesystemCaptureTest::
    passBDetectsGenerationIdentityMutation() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  ReachableV1PreflightInput input;
  const auto evidence = evidenceFor(fixture, QString::fromLatin1(nonceA));
  QVERIFY(installGeneration(fixture, evidence));
  input.referencedGenerations.append(evidence);
  auto mutated = false;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (mutated ||
            event.checkpoint !=
                DormantV1CaptureTestSupport::Checkpoint::BetweenPasses)
          return;
        // Rewrite identical bytes: pass B must compare rich identity too.
        const auto path = QDir(evidence.expected.generationRoot)
                              .filePath(QStringLiteral("hyprland.lua"));
        const std::array<timespec, 2> timestamps{{
            {0, UTIME_OMIT},
            {1, 0},
        }};
        const auto bytes = encoded(path);
        mutated =
            writeFile(path,
                      evidence.files.value(QStringLiteral("hyprland.lua")),
                      0400) &&
            ::utimensat(AT_FDCWD, bytes.constData(), timestamps.data(),
                        AT_SYMLINK_NOFOLLOW) == 0;
      });
  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QVERIFY(mutated);
  compareFailure(result, DormantV1CaptureReason::GenerationFileMismatch,
                 DormantV1CaptureSubject::GenerationPayload, 0);
  QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceA));
  QCOMPARE(result.relativePath, QStringLiteral("hyprland.lua"));
}

void CompositorDormantV1FilesystemCaptureTest::
    passACompletesEarlierTreeBeforeLaterTree() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  ReachableV1PreflightInput input;
  const auto evidenceB = evidenceFor(fixture, QString::fromLatin1(nonceB));
  const auto evidenceA = evidenceFor(fixture, QString::fromLatin1(nonceA));
  QVERIFY(installGeneration(fixture, evidenceB));
  QVERIFY(installGeneration(fixture, evidenceA));
  input.referencedGenerations = {evidenceB, evidenceA};
  auto mutated = false;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (mutated ||
            event.checkpoint !=
                DormantV1CaptureTestSupport::Checkpoint::AfterGenerationFile ||
            event.pass != DormantV1CaptureTestSupport::Pass::First ||
            event.generationIndex != 1 ||
            event.relativePath != QStringLiteral("manifest.json")) {
          return;
        }
        mutated = writeFile(QDir(evidenceB.expected.generationRoot)
                                .filePath(QStringLiteral("hyprland.lua")),
                            QByteArrayLiteral("mutated-before-pass-a"), 0400);
      });
  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QVERIFY(mutated);
  compareFailure(result, DormantV1CaptureReason::GenerationFileMismatch,
                 DormantV1CaptureSubject::GenerationPayload, 0);
  QCOMPARE(result.validatedNonce, QString::fromLatin1(nonceB));
  QCOMPARE(result.relativePath, QStringLiteral("hyprland.lua"));
}

void CompositorDormantV1FilesystemCaptureTest::namedFileRaceFailsClosed() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  ReachableV1PreflightInput input;
  QVERIFY(fixture.installRecord(input, DormantV1CaptureSubject::Desired,
                                QByteArrayLiteral("race")));
  auto mutated = false;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (mutated ||
            event.checkpoint !=
                DormantV1CaptureTestSupport::Checkpoint::AfterFstatat ||
            event.pass != DormantV1CaptureTestSupport::Pass::First ||
            event.subject != DormantV1CaptureSubject::Desired)
          return;
        mutated = chmodPath(fixture.fixedPath(DormantV1CaptureSubject::Desired),
                            0400);
      });
  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QVERIFY(mutated);
  compareFailure(result, DormantV1CaptureReason::FixedRecordUnsafe,
                 DormantV1CaptureSubject::Desired);
}

void CompositorDormantV1FilesystemCaptureTest::
    finalRootDriftOverridesLocalMismatch() {
  Fixture fixture;
  QVERIFY(fixture.valid);
  ReachableV1PreflightInput input;
  input.desired = {
      .kind = LegacyReadKindV1::ExactRegular,
      .bytes = QByteArrayLiteral("expected-but-missing"),
  };
  const auto moved = fixture.stateRoot + QStringLiteral(".moved");
  auto drifted = false;
  DormantV1CaptureTestSupport::setCheckpointHook(
      [&](const DormantV1CaptureTestSupport::CheckpointEvent &event) {
        if (drifted ||
            event.checkpoint !=
                DormantV1CaptureTestSupport::Checkpoint::BeforeFinalRootGuard)
          return;
        drifted = QDir().rename(fixture.stateRoot, moved) &&
                  makeDirectory(fixture.stateRoot, 0700);
      });
  const auto result =
      captureDormantReachableV1Filesystem(fixture.roots(), input);
  QVERIFY(drifted);
  compareFailure(result, DormantV1CaptureReason::RootsChanged,
                 DormantV1CaptureSubject::StateRoot);
}

QTEST_APPLESS_MAIN(CompositorDormantV1FilesystemCaptureTest)

#include "compositor_dormant_v1_filesystem_capture_test.moc"
