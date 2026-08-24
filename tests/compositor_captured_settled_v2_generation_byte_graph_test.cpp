#include "compositord/captured_settled_v2_generation_byte_graph.h"

#include "compositord/authority_records.h"
#include "compositord/ordinary_pending_record.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <array>
#include <cerrno>
#include <optional>
#include <type_traits>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
#error "Captured generation adapter tests must use the normal capture archive"
#endif

#if defined(HYPRSHELLD_MIGRATION_MANIFEST_FILE) ||                             \
    defined(HYPRSHELLD_SOURCE_MANIFEST_V2_FILE) ||                             \
    defined(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_FILE) ||                       \
    defined(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_SCHEMA_FILE) ||                \
    defined(HYPRSHELLD_HYPRLAND_V2_SOURCE_MANIFEST_FILE) ||                    \
    defined(HYPRSHELLD_HYPRLAND_GENERATION_MANIFEST_FIXTURE_FILE) ||           \
    defined(HYPRSHELLD_HYPRLAND_GENERATION_MANIFEST_SCHEMA_FILE) ||            \
    defined(HYPRSHELLD_HYPRLAND_V2_GENERATION_MANIFEST_FILE)
#error "Captured generation adapter must not receive manifest fixtures"
#endif

#if defined(HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE)
#error "Captured generation adapter must not receive a renderer template"
#endif

#if defined(HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE) ||                            \
    defined(HYPRSHELLD_HYPRLAND_V1_ACTION_CATALOG_FILE) ||                     \
    defined(HYPRSHELLD_HYPRLAND_V1_CONFIG_SCHEMA_FILE) ||                      \
    defined(HYPRSHELLD_HYPRLAND_V1_ACTION_FILE) ||                             \
    defined(HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE)
#error "Captured generation adapter must not receive v1 parser fixtures"
#endif

#if !defined(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE) ||                           \
    !defined(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE) ||                    \
    !defined(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE)
#error "The three typed v2 parser-authority fixtures are required"
#endif

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString authorityId = QStringLiteral("11111111111111111111111111111111");
const QByteArray ownerPendingBytes =
    QByteArrayLiteral("{\"formatVersion\":2,\"kind\":\"restart\"}\n");

enum RecordIndex : std::size_t {
  Authority,
  Desired,
  LastGood,
  Applied,
  Pending,
  RecordCount,
};

constexpr std::array<const char *, RecordCount> recordNames{{
    "authority.json",
    "desired.json",
    "last-good.json",
    "activation.json",
    "pending.json",
}};

using ExpectedAdapterSignature = SettledV2GenerationByteGraphResult (*)(
    const DormantFixedRecordCapture &,
    const QVector<SettledV2GenerationEvidence> &, const Catalog &,
    const ActionCatalog &);

static_assert(std::is_same_v<
              decltype(&classifyCapturedSettledV2GenerationContentByteGraph),
              ExpectedAdapterSignature>);
static_assert(std::is_invocable_r_v<
              SettledV2GenerationByteGraphResult, ExpectedAdapterSignature,
              const DormantFixedRecordCapture &,
              const QVector<SettledV2GenerationEvidence> &, const Catalog &,
              const ActionCatalog &>);
static_assert(!std::is_invocable_v<ExpectedAdapterSignature,
                                   const DormantFixedRecordCaptureResult &,
                                   const QVector<SettledV2GenerationEvidence> &,
                                   const Catalog &, const ActionCatalog &>);
static_assert(
    !std::is_invocable_v<ExpectedAdapterSignature,
                         const std::optional<DormantFixedRecordCapture> &,
                         const QVector<SettledV2GenerationEvidence> &,
                         const Catalog &, const ActionCatalog &>);
static_assert(!std::is_invocable_v<ExpectedAdapterSignature, int,
                                   const QVector<SettledV2GenerationEvidence> &,
                                   const Catalog &, const ActionCatalog &>);

class StateDirectory final {
public:
  StateDirectory() {
    valid = temporary.isValid();
    path = QDir(temporary.path()).filePath(QStringLiteral("state"));
    valid = valid && QDir().mkdir(path) &&
            ::chmod(QFile::encodeName(path).constData(), 0700) == 0;
    if (valid) {
      descriptor = ::open(QFile::encodeName(path).constData(),
                          O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
      valid = descriptor >= 0;
    }
  }

  ~StateDirectory() {
    if (descriptor >= 0)
      ::close(descriptor);
  }

  StateDirectory(const StateDirectory &) = delete;
  StateDirectory &operator=(const StateDirectory &) = delete;

  [[nodiscard]] QString file(const std::size_t index) const {
    return QDir(path).filePath(QString::fromLatin1(recordNames.at(index)));
  }

  [[nodiscard]] bool write(const std::size_t index,
                           const QByteArray &bytes) const {
    const auto encoded = QFile::encodeName(file(index));
    auto fileDescriptor = ::open(
        encoded.constData(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fileDescriptor < 0)
      return false;
    qsizetype offset = 0;
    auto okay = true;
    while (offset < bytes.size()) {
      const auto count = ::write(fileDescriptor, bytes.constData() + offset,
                                 static_cast<size_t>(bytes.size() - offset));
      if (count < 0 && errno == EINTR)
        continue;
      if (count <= 0) {
        okay = false;
        break;
      }
      offset += static_cast<qsizetype>(count);
    }
    const auto closeResult = ::close(fileDescriptor);
    return okay && closeResult == 0 && ::chmod(encoded.constData(), 0600) == 0;
  }

  [[nodiscard]] bool remove(const std::size_t index) const {
    const auto encoded = QFile::encodeName(file(index));
    if (::unlink(encoded.constData()) == 0)
      return true;
    return errno == ENOENT;
  }

  QTemporaryDir temporary;
  QString path;
  int descriptor = -1;
  bool valid = false;
};

[[nodiscard]] QByteArray readBytes(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

[[nodiscard]] const DormantFixedRecordField &
fieldAt(const DormantFixedRecordCapture &capture, const std::size_t index) {
  switch (index) {
  case Authority:
    return capture.authority();
  case Desired:
    return capture.desired();
  case LastGood:
    return capture.lastGood();
  case Applied:
    return capture.applied();
  case Pending:
    return capture.pending();
  }
  return capture.authority();
}

[[nodiscard]] QByteArrayView
directRequiredView(const DormantFixedRecordField &field) {
  switch (field.kind()) {
  case DormantFixedRecordFieldKind::Missing:
    return {};
  case DormantFixedRecordFieldKind::PresentBytes:
    return QByteArrayView(field.bytes());
  }
  return {};
}

[[nodiscard]] std::optional<QByteArrayView>
directOptionalView(const DormantFixedRecordField &field) {
  switch (field.kind()) {
  case DormantFixedRecordFieldKind::Missing:
    return std::nullopt;
  case DormantFixedRecordFieldKind::PresentBytes:
    return QByteArrayView(field.bytes());
  }
  return QByteArrayView{};
}

} // namespace

class CompositorCapturedSettledV2GenerationByteGraphTest final
    : public QObject {
  Q_OBJECT

private:
  Catalog catalogV2_;
  ActionCatalog actionCatalogV2_;
  QByteArray authorityBytes_;
  DesiredStateV2 desiredState_;
  QByteArray desiredBytes_;

  static constexpr auto coherent =
      SettledV2GenerationByteGraphResult::GenerationContentByteCoherent;
  static constexpr auto delegate =
      SettledV2GenerationByteGraphResult::DelegatePendingOwner;
  static constexpr auto incoherent =
      SettledV2GenerationByteGraphResult::Incoherent;

  [[nodiscard]] bool seedCoherent(StateDirectory &state) const {
    for (std::size_t index = 0; index < RecordCount; ++index) {
      if (!state.remove(index))
        return false;
    }
    return state.write(Authority, authorityBytes_) &&
           state.write(Desired, desiredBytes_);
  }

  [[nodiscard]] DormantFixedRecordCapture
  capture(const StateDirectory &state) const {
    const auto result = captureDormantFixedRecords(state.descriptor);
    if (result.disposition() !=
            DormantFixedRecordCaptureDisposition::Captured ||
        !result.capture()) {
      qFatal("real fixed-record capture failed in adapter test");
    }
    return *result.capture();
  }

  [[nodiscard]] SettledV2GenerationByteGraphResult
  classify(const DormantFixedRecordCapture &captured,
           const QVector<SettledV2GenerationEvidence> &evidence = {}) const {
    return classifyCapturedSettledV2GenerationContentByteGraph(
        captured, evidence, catalogV2_, actionCatalogV2_);
  }

  [[nodiscard]] SettledV2GenerationByteGraphResult classifyDirect(
      const DormantFixedRecordCapture &captured,
      const QVector<SettledV2GenerationEvidence> &evidence = {}) const {
    const SettledV2CurrentRecordBytes current{
        .authority = directRequiredView(captured.authority()),
        .desired = directRequiredView(captured.desired()),
        .lastGood = directOptionalView(captured.lastGood()),
        .applied = directOptionalView(captured.applied()),
    };
    return classifySettledV2GenerationContentByteGraph(
        current, directOptionalView(captured.pending()), evidence, catalogV2_,
        actionCatalogV2_);
  }

  [[nodiscard]] QString desiredDigest() const {
    return QString::fromLatin1(
        QCryptographicHash::hash(
            QByteArrayView(desiredBytes_).first(desiredBytes_.size() - 1),
            QCryptographicHash::Sha256)
            .toHex());
  }

  [[nodiscard]] QByteArray appliedBytes(const AppliedRecordV2 &record) const {
    const auto serialized = serializeAppliedRecordV2(record);
    if (!serialized)
      qFatal("failed to serialize captured-adapter Applied fixture");
    return *serialized.value;
  }

  [[nodiscard]] QByteArray
  ordinaryPendingBytes(const AppliedRecordV2 &before,
                       const AppliedRecordV2 &after) const {
    const OrdinaryPendingRecordV2 record{
        .authorityId = authorityId,
        .kind = OrdinaryPendingKind::Apply,
        .phase = OrdinaryPendingPhase::Prepared,
        .expectedRevision = desiredState_.semanticState.revision,
        .beforeDesiredDigest = desiredDigest(),
        .beforeActivationDesired =
            OrdinaryPendingDesiredMaterialV2{
                .state = desiredState_,
                .bytes = desiredBytes_,
            },
        .candidateSnapshot = desiredState_,
        .candidateSnapshotBytes = desiredBytes_,
        .snapshotDigest = desiredDigest(),
        .afterActivation = after,
        .beforeActivation = before,
    };
    const auto serialized =
        serializeOrdinaryPendingRecordV2(record, catalogV2_, actionCatalogV2_);
    if (!serialized)
      qFatal("failed to serialize captured-adapter Pending fixture");
    return *serialized.value;
  }

private slots:
  void initTestCase();
  void exactSignatureRejectsResultFdAndFailedCaptureHasNoEntry();
  void coherentExactV2WithOtherRecordsMissingAndZeroEvidence();
  void missingVersusPresentEmptyForEveryField();
  void emptyAndNonordinaryPendingDelegate();
  void evidenceCardinalityPrecedesPendingOwnership();
  void historicalCaptureSurvivesPendingDiskMutation();
  void capturedOrdinaryPendingUsesExactPriorDesired();
  void matchesDirectRawDelegateForEveryOutcome();
};

void CompositorCapturedSettledV2GenerationByteGraphTest::initTestCase() {
  const auto catalog = parseDormantCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE)));
  const auto actions = parseDormantActionCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE)),
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE)));
  QVERIFY(catalog);
  QVERIFY(actions);
  catalogV2_ = *catalog.value;
  actionCatalogV2_ = *actions.value;

  const auto authority = serializeAuthorityRecordV2({authorityId});
  QVERIFY(authority);
  authorityBytes_ = *authority.value;

  const auto initial =
      defaultDormantDesiredStateV2(catalogV2_, actionCatalogV2_, authorityId);
  QVERIFY(initial);
  desiredState_ = *initial.value;
  desiredState_.semanticState.revision = 7;
  const auto serialized = serializeDormantDesiredStateV2(desiredState_);
  QVERIFY(serialized);
  desiredBytes_ = *serialized.value;
  QVERIFY(!authorityBytes_.isEmpty());
  QVERIFY(!desiredBytes_.isEmpty());
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    exactSignatureRejectsResultFdAndFailedCaptureHasNoEntry() {
  const auto failed = captureDormantFixedRecords(-1);
  QCOMPARE(failed.disposition(),
           DormantFixedRecordCaptureDisposition::FailedClosed);
  QVERIFY(!failed.capture());
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    coherentExactV2WithOtherRecordsMissingAndZeroEvidence() {
  StateDirectory state;
  QVERIFY(state.valid);
  QVERIFY(seedCoherent(state));
  const auto captured = capture(state);
  QCOMPARE(captured.authority().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QCOMPARE(captured.desired().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QCOMPARE(captured.lastGood().kind(), DormantFixedRecordFieldKind::Missing);
  QCOMPARE(captured.applied().kind(), DormantFixedRecordFieldKind::Missing);
  QCOMPARE(captured.pending().kind(), DormantFixedRecordFieldKind::Missing);
  QCOMPARE(classify(captured), coherent);
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    missingVersusPresentEmptyForEveryField() {
  const std::array<SettledV2GenerationByteGraphResult, RecordCount>
      missingExpected{{incoherent, incoherent, coherent, coherent, coherent}};
  const std::array<SettledV2GenerationByteGraphResult, RecordCount>
      emptyExpected{{incoherent, incoherent, incoherent, incoherent, delegate}};

  StateDirectory state;
  QVERIFY(state.valid);
  for (std::size_t index = 0; index < RecordCount; ++index) {
    QVERIFY(seedCoherent(state));
    QVERIFY(state.remove(index));
    const auto missing = capture(state);
    QCOMPARE(fieldAt(missing, index).kind(),
             DormantFixedRecordFieldKind::Missing);
    QVERIFY(fieldAt(missing, index).bytes().isEmpty());
    QCOMPARE(classify(missing), missingExpected.at(index));

    QVERIFY(state.write(index, QByteArray{}));
    const auto presentEmpty = capture(state);
    QCOMPARE(fieldAt(presentEmpty, index).kind(),
             DormantFixedRecordFieldKind::PresentBytes);
    QVERIFY(fieldAt(presentEmpty, index).bytes().isEmpty());
    QCOMPARE(classify(presentEmpty), emptyExpected.at(index));
  }
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    emptyAndNonordinaryPendingDelegate() {
  StateDirectory state;
  QVERIFY(state.valid);
  QVERIFY(seedCoherent(state));
  QVERIFY(state.write(Pending, QByteArray{}));
  const auto empty = capture(state);
  QCOMPARE(classify(empty), delegate);

  QVERIFY(state.write(Pending, ownerPendingBytes));
  const auto present = capture(state);
  QCOMPARE(present.pending().kind(), DormantFixedRecordFieldKind::PresentBytes);
  QCOMPARE(present.pending().bytes(), ownerPendingBytes);
  QCOMPARE(classify(present), delegate);
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    evidenceCardinalityPrecedesPendingOwnership() {
  StateDirectory state;
  QVERIFY(state.valid);
  QVERIFY(seedCoherent(state));
  QVERIFY(state.write(Authority, QByteArray{}));
  QVERIFY(state.write(Desired, QByteArray{}));
  QVERIFY(state.write(Pending, ownerPendingBytes));
  const auto captured = capture(state);
  QCOMPARE(captured.authority().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QVERIFY(captured.authority().bytes().isEmpty());
  QCOMPARE(captured.desired().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QVERIFY(captured.desired().bytes().isEmpty());

  QVector<SettledV2GenerationEvidence> withinBound;
  withinBound.resize(maximumSettledV2GenerationEvidence);
  QCOMPARE(classify(captured, withinBound), delegate);
  QCOMPARE(classify(captured, withinBound),
           classifyDirect(captured, withinBound));

  auto overBound = withinBound;
  overBound.append(SettledV2GenerationEvidence{});
  QCOMPARE(overBound.size(), maximumSettledV2GenerationEvidence + 1);
  QCOMPARE(classify(captured, overBound), incoherent);
  QCOMPARE(classify(captured, overBound), classifyDirect(captured, overBound));
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    historicalCaptureSurvivesPendingDiskMutation() {
  StateDirectory state;
  QVERIFY(state.valid);
  QVERIFY(seedCoherent(state));
  const auto historical = capture(state);
  QCOMPARE(historical.pending().kind(), DormantFixedRecordFieldKind::Missing);
  QCOMPARE(classify(historical), coherent);

  QVERIFY(state.write(Pending, ownerPendingBytes));
  QCOMPARE(classify(historical), coherent);
  const auto fresh = capture(state);
  QCOMPARE(fresh.pending().kind(), DormantFixedRecordFieldKind::PresentBytes);
  QCOMPARE(classify(fresh), delegate);
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    capturedOrdinaryPendingUsesExactPriorDesired() {
  StateDirectory state;
  QVERIFY(state.valid);
  QVERIFY(seedCoherent(state));

  const auto digest = desiredDigest();
  const AppliedRecordV2 before{
      .authorityId = authorityId,
      .revision = desiredState_.semanticState.revision,
      .snapshotDigest = digest,
      .generation = QString(64, QLatin1Char('b')),
      .activationNonce = QString(32, QLatin1Char('2')),
      .entrypoint = QStringLiteral("hyprland.lua"),
      .requiredActivation = ActivationRequirement::Reload,
  };
  auto after = before;
  after.generation = QString(64, QLatin1Char('c'));
  after.activationNonce = QString(32, QLatin1Char('3'));

  QVERIFY(state.write(LastGood, desiredBytes_));
  QVERIFY(state.write(Applied, appliedBytes(before)));
  QVERIFY(state.write(Pending, ordinaryPendingBytes(before, after)));
  const auto captured = capture(state);

  // Exact ordinary syntax is consumed by the byte graph. With both referenced
  // generation products deliberately absent, it is incoherent rather than
  // delegated to the nonordinary Pending owner.
  QCOMPARE(classify(captured), incoherent);
  QCOMPARE(classify(captured), classifyDirect(captured));
}

void CompositorCapturedSettledV2GenerationByteGraphTest::
    matchesDirectRawDelegateForEveryOutcome() {
  StateDirectory state;
  QVERIFY(state.valid);
  QVERIFY(seedCoherent(state));
  const auto coherentCapture = capture(state);
  QCOMPARE(classify(coherentCapture), coherent);
  QCOMPARE(classify(coherentCapture), classifyDirect(coherentCapture));

  QVERIFY(state.write(Authority, QByteArray{}));
  const auto incoherentCapture = capture(state);
  QCOMPARE(classify(incoherentCapture), incoherent);
  QCOMPARE(classify(incoherentCapture), classifyDirect(incoherentCapture));

  QVERIFY(seedCoherent(state));
  QVERIFY(state.write(Pending, ownerPendingBytes));
  const auto delegateCapture = capture(state);
  QCOMPARE(classify(delegateCapture), delegate);
  QCOMPARE(classify(delegateCapture), classifyDirect(delegateCapture));
}

QTEST_MAIN(CompositorCapturedSettledV2GenerationByteGraphTest)

#include "compositor_captured_settled_v2_generation_byte_graph_test.moc"
