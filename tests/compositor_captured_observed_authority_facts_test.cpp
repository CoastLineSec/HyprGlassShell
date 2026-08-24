#include "compositord/captured_observed_authority_facts.h"

#include "compositord/authority_records.h"

#include "hyprland/desired_state.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <array>
#include <cerrno>
#include <type_traits>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
#error "Captured authority adapter tests must use the normal capture archive"
#endif

#if defined(HYPRSHELLD_MIGRATION_MANIFEST_FILE) ||                             \
    defined(HYPRSHELLD_SOURCE_MANIFEST_V2_FILE) ||                             \
    defined(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_FILE)
#error "Captured authority adapter must not receive migration/source manifests"
#endif

#if !defined(HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE) ||                           \
    !defined(HYPRSHELLD_HYPRLAND_V1_ACTION_CATALOG_FILE) ||                    \
    !defined(HYPRSHELLD_HYPRLAND_V1_CONFIG_SCHEMA_FILE) ||                     \
    !defined(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE) ||                           \
    !defined(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE) ||                    \
    !defined(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE)
#error "The six typed parser-authority fixtures are required"
#endif

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString authorityId = QStringLiteral("11111111111111111111111111111111");

constexpr std::array<const char *, 5> recordNames{{
    "authority.json",
    "desired.json",
    "last-good.json",
    "activation.json",
    "pending.json",
}};

using ExpectedAdapterSignature = ObservedAuthorityFactsResult (*)(
    const DormantFixedRecordCapture &, const Catalog &, const ActionCatalog &,
    const Catalog &, const ActionCatalog &);

static_assert(std::is_same_v<decltype(&buildCapturedObservedAuthorityFacts),
                             ExpectedAdapterSignature>);
static_assert(std::is_invocable_r_v<
              ObservedAuthorityFactsResult, ExpectedAdapterSignature,
              const DormantFixedRecordCapture &, const Catalog &,
              const ActionCatalog &, const Catalog &, const ActionCatalog &>);
static_assert(!std::is_invocable_v<ExpectedAdapterSignature,
                                   const DormantFixedRecordCaptureResult &,
                                   const Catalog &, const ActionCatalog &,
                                   const Catalog &, const ActionCatalog &>);
static_assert(!std::is_invocable_v<
              ExpectedAdapterSignature,
              const std::optional<DormantFixedRecordCapture> &, const Catalog &,
              const ActionCatalog &, const Catalog &, const ActionCatalog &>);

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

  [[nodiscard]] QString file(const size_t index) const {
    return QDir(path).filePath(QString::fromLatin1(recordNames[index]));
  }

  [[nodiscard]] bool write(const size_t index, const QByteArray &bytes) const {
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

  [[nodiscard]] bool remove(const size_t index) const {
    const auto encoded = QFile::encodeName(file(index));
    return ::unlink(encoded.constData()) == 0 || errno == ENOENT;
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

void compareClassified(const ObservedAuthorityFactsResult &result,
                       const ObservedAuthorityKind kind,
                       const QString &expectedAuthorityId = {},
                       const quint64 revision = 0) {
  QCOMPARE(result.status(), ObservedAuthorityFactsStatus::Classified);
  QVERIFY(result.tuple());
  QVERIFY(isValidObservedAuthorityTuple(*result.tuple()));
  QCOMPARE(result.tuple()->kind, kind);
  QCOMPARE(result.tuple()->authorityId, expectedAuthorityId);
  QCOMPARE(result.tuple()->revision, revision);
}

void compareInvalidAuthorities(const ObservedAuthorityFactsResult &result) {
  QCOMPARE(result.status(),
           ObservedAuthorityFactsStatus::InvalidParserAuthorities);
  QVERIFY(!result.tuple());
}

} // namespace

class CompositorCapturedObservedAuthorityFactsTest final : public QObject {
  Q_OBJECT

private:
  Catalog catalogV1_;
  ActionCatalog actionCatalogV1_;
  Catalog catalogV2_;
  ActionCatalog actionCatalogV2_;

  [[nodiscard]] QByteArray authorityBytes() const {
    const auto encoded = serializeAuthorityRecordV2({authorityId});
    return encoded ? *encoded.value : QByteArray{};
  }

  [[nodiscard]] QByteArray desiredV1Bytes(const quint64 revision = 7) const {
    auto state = defaultDesiredState(catalogV1_, actionCatalogV1_);
    state.targetHyprland = QStringLiteral("0.56.1");
    state.revision = revision;
    state.workspaceRules.clear();
    return serializeDesiredState(state);
  }

  [[nodiscard]] QByteArray desiredV2Bytes(const quint64 revision = 7) const {
    const auto initial =
        defaultDormantDesiredStateV2(catalogV2_, actionCatalogV2_, authorityId);
    if (!initial)
      return {};
    auto state = *initial.value;
    state.semanticState.revision = revision;
    const auto encoded = serializeDormantDesiredStateV2(state);
    return encoded ? *encoded.value : QByteArray{};
  }

  [[nodiscard]] ObservedAuthorityFactsResult
  classify(const DormantFixedRecordCapture &capture) const {
    return buildCapturedObservedAuthorityFacts(
        capture, catalogV1_, actionCatalogV1_, catalogV2_, actionCatalogV2_);
  }

  [[nodiscard]] ObservedAuthorityFactsResult
  classify(const DormantFixedRecordCapture &capture, const Catalog &catalogV1,
           const ActionCatalog &actionsV1, const Catalog &catalogV2,
           const ActionCatalog &actionsV2) const {
    return buildCapturedObservedAuthorityFacts(capture, catalogV1, actionsV1,
                                               catalogV2, actionsV2);
  }

private slots:
  void initTestCase();
  void exactSignatureRejectsResultAndFailedCaptureHasNoEntry();
  void allMissingIsAbsent();
  void missingAuthorityAndExactV1DesiredIsV1();
  void exactMatchingV2PairIsV2();
  void presentEmptyAtEitherPositionIsUnreadableNotAbsent();
  void everyParserAuthorityFailurePrecedesCapturedRecords();
  void lastGoodAppliedAndPendingBytesAreIrrelevant();
  void historicalCaptureSurvivesDiskMutationAndFreshRecaptureChanges();
};

void CompositorCapturedObservedAuthorityFactsTest::initTestCase() {
  const auto catalogV1 = parseCatalog(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE)));
  const auto actionsV1 = parseActionCatalog(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_ACTION_CATALOG_FILE)),
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_CONFIG_SCHEMA_FILE)));
  const auto catalogV2 = parseDormantCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE)));
  const auto actionsV2 = parseDormantActionCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE)),
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE)));
  QVERIFY(catalogV1);
  QVERIFY(actionsV1);
  QVERIFY(catalogV2);
  QVERIFY(actionsV2);
  catalogV1_ = *catalogV1.value;
  actionCatalogV1_ = *actionsV1.value;
  catalogV2_ = *catalogV2.value;
  actionCatalogV2_ = *actionsV2.value;

  QVERIFY(!authorityBytes().isEmpty());
  QVERIFY(!desiredV1Bytes().isEmpty());
  QVERIFY(!desiredV2Bytes().isEmpty());
}

void CompositorCapturedObservedAuthorityFactsTest::
    exactSignatureRejectsResultAndFailedCaptureHasNoEntry() {
  StateDirectory root;
  QVERIFY(root.valid);
  const auto successful = captureDormantFixedRecords(root.descriptor);
  QCOMPARE(successful.disposition(),
           DormantFixedRecordCaptureDisposition::Captured);
  QVERIFY(successful.capture());
  compareClassified(classify(*successful.capture()),
                    ObservedAuthorityKind::Absent);

  const auto failed = captureDormantFixedRecords(-1);
  QCOMPARE(failed.disposition(),
           DormantFixedRecordCaptureDisposition::FailedClosed);
  QVERIFY(!failed.capture());
}

void CompositorCapturedObservedAuthorityFactsTest::allMissingIsAbsent() {
  StateDirectory root;
  QVERIFY(root.valid);
  const auto captured = captureDormantFixedRecords(root.descriptor);
  QVERIFY(captured.capture());
  compareClassified(classify(*captured.capture()),
                    ObservedAuthorityKind::Absent);
}

void CompositorCapturedObservedAuthorityFactsTest::
    missingAuthorityAndExactV1DesiredIsV1() {
  StateDirectory root;
  QVERIFY(root.valid);
  QVERIFY(root.write(1, desiredV1Bytes(19)));
  const auto captured = captureDormantFixedRecords(root.descriptor);
  QVERIFY(captured.capture());
  compareClassified(classify(*captured.capture()), ObservedAuthorityKind::V1,
                    {}, 19);
}

void CompositorCapturedObservedAuthorityFactsTest::exactMatchingV2PairIsV2() {
  StateDirectory root;
  QVERIFY(root.valid);
  QVERIFY(root.write(0, authorityBytes()));
  QVERIFY(root.write(1, desiredV2Bytes(23)));
  const auto captured = captureDormantFixedRecords(root.descriptor);
  QVERIFY(captured.capture());
  compareClassified(classify(*captured.capture()), ObservedAuthorityKind::V2,
                    authorityId, 23);
}

void CompositorCapturedObservedAuthorityFactsTest::
    presentEmptyAtEitherPositionIsUnreadableNotAbsent() {
  StateDirectory root;
  QVERIFY(root.valid);
  QVERIFY(root.write(0, {}));
  auto emptyAuthority = captureDormantFixedRecords(root.descriptor);
  QVERIFY(emptyAuthority.capture());
  QCOMPARE(emptyAuthority.capture()->authority().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QVERIFY(emptyAuthority.capture()->authority().bytes().isEmpty());
  compareClassified(classify(*emptyAuthority.capture()),
                    ObservedAuthorityKind::Unreadable);

  QVERIFY(root.remove(0));
  QVERIFY(root.write(1, {}));
  auto emptyDesired = captureDormantFixedRecords(root.descriptor);
  QVERIFY(emptyDesired.capture());
  QCOMPARE(emptyDesired.capture()->desired().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QVERIFY(emptyDesired.capture()->desired().bytes().isEmpty());
  compareClassified(classify(*emptyDesired.capture()),
                    ObservedAuthorityKind::Unreadable);
}

void CompositorCapturedObservedAuthorityFactsTest::
    everyParserAuthorityFailurePrecedesCapturedRecords() {
  StateDirectory root;
  QVERIFY(root.valid);
  // Present-empty records would otherwise classify Unreadable, demonstrating
  // that parser-authority failure retains outer precedence.
  QVERIFY(root.write(0, {}));
  QVERIFY(root.write(1, {}));
  const auto captured = captureDormantFixedRecords(root.descriptor);
  QVERIFY(captured.capture());

  auto badCatalogV1 = catalogV1_;
  badCatalogV1.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  compareInvalidAuthorities(classify(*captured.capture(), badCatalogV1,
                                     actionCatalogV1_, catalogV2_,
                                     actionCatalogV2_));

  auto badActionsV1 = actionCatalogV1_;
  badActionsV1.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  compareInvalidAuthorities(classify(*captured.capture(), catalogV1_,
                                     badActionsV1, catalogV2_,
                                     actionCatalogV2_));

  auto badCatalogV2 = catalogV2_;
  badCatalogV2.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  compareInvalidAuthorities(classify(*captured.capture(), catalogV1_,
                                     actionCatalogV1_, badCatalogV2,
                                     actionCatalogV2_));

  auto badActionsV2 = actionCatalogV2_;
  badActionsV2.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  compareInvalidAuthorities(classify(*captured.capture(), catalogV1_,
                                     actionCatalogV1_, catalogV2_,
                                     badActionsV2));
}

void CompositorCapturedObservedAuthorityFactsTest::
    lastGoodAppliedAndPendingBytesAreIrrelevant() {
  StateDirectory root;
  QVERIFY(root.valid);
  QVERIFY(root.write(0, authorityBytes()));
  QVERIFY(root.write(1, desiredV2Bytes(29)));
  QVERIFY(root.write(2, {}));
  QVERIFY(root.write(3, QByteArrayLiteral("arbitrary-applied")));
  QVERIFY(root.write(4, QByteArrayLiteral("arbitrary-pending")));
  const auto captured = captureDormantFixedRecords(root.descriptor);
  QVERIFY(captured.capture());
  QCOMPARE(captured.capture()->lastGood().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QVERIFY(captured.capture()->lastGood().bytes().isEmpty());
  QCOMPARE(captured.capture()->applied().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  QCOMPARE(captured.capture()->pending().kind(),
           DormantFixedRecordFieldKind::PresentBytes);
  compareClassified(classify(*captured.capture()), ObservedAuthorityKind::V2,
                    authorityId, 29);
}

void CompositorCapturedObservedAuthorityFactsTest::
    historicalCaptureSurvivesDiskMutationAndFreshRecaptureChanges() {
  StateDirectory root;
  QVERIFY(root.valid);
  QVERIFY(root.write(1, desiredV1Bytes(31)));
  const auto historical = captureDormantFixedRecords(root.descriptor);
  QVERIFY(historical.capture());
  compareClassified(classify(*historical.capture()), ObservedAuthorityKind::V1,
                    {}, 31);

  QVERIFY(root.write(0, authorityBytes()));
  QVERIFY(root.write(1, desiredV2Bytes(32)));
  compareClassified(classify(*historical.capture()), ObservedAuthorityKind::V1,
                    {}, 31);

  const auto fresh = captureDormantFixedRecords(root.descriptor);
  QVERIFY(fresh.capture());
  compareClassified(classify(*fresh.capture()), ObservedAuthorityKind::V2,
                    authorityId, 32);
}

QTEST_APPLESS_MAIN(CompositorCapturedObservedAuthorityFactsTest)

#include "compositor_captured_observed_authority_facts_test.moc"
