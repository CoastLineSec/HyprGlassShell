#include "compositord/observed_authority_facts.h"

#include "compositord/authority_records.h"

#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <array>
#include <limits>
#include <type_traits>
#include <utility>

#if defined(HYPRSHELLD_MIGRATION_MANIFEST_FILE) ||                             \
    defined(HYPRSHELLD_SOURCE_MANIFEST_V2_FILE) ||                             \
    defined(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_FILE)
#error "Observed authority facts must not receive migration/source manifests"
#endif

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString authorityA = QStringLiteral("11111111111111111111111111111111");
const QString authorityB = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
const QString oldCatalogA = QStringLiteral(
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0");
const QString oldCatalogB = QStringLiteral(
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388");
const QString oldAction = QStringLiteral(
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2");

static_assert(
    std::is_same_v<
        decltype(std::declval<const ObservedAuthorityFactsResult &>().status()),
        ObservedAuthorityFactsStatus>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const ObservedAuthorityFactsResult &>().tuple()),
        const std::optional<ObservedAuthorityTuple> &>);
static_assert(!std::is_convertible_v<ObservedAuthorityFactsResult, QByteArray>);
static_assert(
    !std::is_convertible_v<ObservedAuthorityFactsResult, DesiredState>);
static_assert(
    !std::is_convertible_v<ObservedAuthorityFactsResult, DesiredStateV2>);

[[nodiscard]] QByteArray readBytes(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

[[nodiscard]] BorrowedObservedAuthorityRead missingRead() {
  return {.kind = ObservedAuthorityReadKind::Missing, .bytes = {}};
}

[[nodiscard]] BorrowedObservedAuthorityRead
presentRead(const QByteArrayView bytes) {
  return {.kind = ObservedAuthorityReadKind::PresentBytes, .bytes = bytes};
}

[[nodiscard]] BorrowedObservedAuthorityRead unsafeRead() {
  return {.kind = ObservedAuthorityReadKind::Unsafe, .bytes = {}};
}

void compareClassified(const ObservedAuthorityFactsResult &result,
                       const ObservedAuthorityKind kind,
                       const QString &authorityId = {},
                       const quint64 revision = 0) {
  QCOMPARE(result.status(), ObservedAuthorityFactsStatus::Classified);
  QVERIFY(result.tuple());
  QVERIFY(isValidObservedAuthorityTuple(*result.tuple()));
  QCOMPARE(result.tuple()->kind, kind);
  QCOMPARE(result.tuple()->authorityId, authorityId);
  QCOMPARE(result.tuple()->revision, revision);
}

void compareInvalidAuthorities(const ObservedAuthorityFactsResult &result) {
  QCOMPARE(result.status(),
           ObservedAuthorityFactsStatus::InvalidParserAuthorities);
  QVERIFY(!result.tuple());
}

[[nodiscard]] QByteArray canonicalObjectWithLf(const QJsonObject &object) {
  auto bytes = JsonSupport::canonicalJson(object);
  bytes.append('\n');
  return bytes;
}

} // namespace

class CompositorObservedAuthorityFactsTest final : public QObject {
  Q_OBJECT

private:
  Catalog catalogV1_;
  ActionCatalog actionCatalogV1_;
  Catalog catalogV2_;
  ActionCatalog actionCatalogV2_;

  [[nodiscard]] QByteArray
  authorityBytes(const QString &authorityId = authorityA) const {
    const auto encoded = serializeAuthorityRecordV2({authorityId});
    return encoded ? *encoded.value : QByteArray{};
  }

  [[nodiscard]] QByteArray desiredV1Bytes(
      const quint32 patch = 1, const quint64 revision = 7,
      const QString &catalogDigestValue = QLatin1String(reviewedCatalogDigest),
      const QString &actionDigestValue =
          QLatin1String(reviewedActionCatalogDigest)) const {
    auto state = defaultDesiredState(catalogV1_, actionCatalogV1_);
    state.targetHyprland = QStringLiteral("0.56.%1").arg(patch);
    state.revision = revision;
    state.catalogDigest = catalogDigestValue;
    state.actionCatalogDigest = actionDigestValue;
    // Every frozen pair permits the empty rule set. This also avoids the one
    // pre-shared selector excluded by the oldest action authority.
    state.workspaceRules.clear();
    return serializeDesiredState(state);
  }

  [[nodiscard]] QByteArray
  desiredV2Bytes(const quint64 revision = 7,
                 const QString &authorityId = authorityA) const {
    const auto initial =
        defaultDormantDesiredStateV2(catalogV2_, actionCatalogV2_, authorityId);
    if (!initial) {
      return {};
    }
    auto state = *initial.value;
    state.semanticState.revision = revision;
    const auto encoded = serializeDormantDesiredStateV2(state);
    return encoded ? *encoded.value : QByteArray{};
  }

  [[nodiscard]] ObservedAuthorityFactsResult
  classify(const BorrowedObservedAuthorityRead authority,
           const BorrowedObservedAuthorityRead desired) const {
    return classify(authority, desired, catalogV1_, actionCatalogV1_,
                    catalogV2_, actionCatalogV2_);
  }

  [[nodiscard]] ObservedAuthorityFactsResult
  classify(const BorrowedObservedAuthorityRead authority,
           const BorrowedObservedAuthorityRead desired,
           const Catalog &catalogV1, const ActionCatalog &actionsV1,
           const Catalog &catalogV2, const ActionCatalog &actionsV2) const {
    return buildObservedAuthorityFacts(
        {.authority = authority, .desired = desired}, catalogV1, actionsV1,
        catalogV2, actionsV2);
  }

private slots:
  void initTestCase();
  void fullRawObservationTableIsExact();
  void v2IdentityAndRevisionEndpointsAreExact();
  void everyFrozenV1LineagePatchAndRevisionIsExact();
  void exactV1AndV2FormatsAreMutuallyExclusive();
  void missingEmptyAndUnsafeRemainDistinct();
  void invalidReadTagsAndContradictoryPayloadsAreUnreadable();
  void malformedAndNoncanonicalRecordsAreUnreadable();
  void recordBoundsAreClosedAtBothPositions();
  void invalidViewMetadataIsRejectedAtBothPositions();
  void borrowedSharedNonterminatedViewsDoNotEscape();
  void everyTypedParserAuthorityIsIndependentlyRequired();
  void parserAuthorityFailurePrecedesEveryRecordClassification();
  void nonauthoritativeTypedCachesCannotAffectClassification();
};

void CompositorObservedAuthorityFactsTest::initTestCase() {
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

void CompositorObservedAuthorityFactsTest::fullRawObservationTableIsExact() {
  const auto authority = authorityBytes();
  const auto desiredV1 = desiredV1Bytes(1, 19);
  const auto desiredV2 = desiredV2Bytes(19);
  const QByteArray invalid;

  enum class AnchorCase { Missing, PresentInvalid, ExactV2 };
  enum class DesiredCase { Missing, PresentInvalid, ExactV1, ExactV2 };
  struct AnchorRow final {
    AnchorCase kind;
    BorrowedObservedAuthorityRead read;
  };
  struct DesiredRow final {
    DesiredCase kind;
    BorrowedObservedAuthorityRead read;
  };
  const std::array anchors{
      AnchorRow{AnchorCase::Missing, missingRead()},
      AnchorRow{AnchorCase::PresentInvalid, presentRead(invalid)},
      AnchorRow{AnchorCase::ExactV2, presentRead(authority)},
  };
  const std::array desireds{
      DesiredRow{DesiredCase::Missing, missingRead()},
      DesiredRow{DesiredCase::PresentInvalid, presentRead(invalid)},
      DesiredRow{DesiredCase::ExactV1, presentRead(desiredV1)},
      DesiredRow{DesiredCase::ExactV2, presentRead(desiredV2)},
  };

  for (const auto &anchor : anchors) {
    for (const auto &desired : desireds) {
      const auto result = classify(anchor.read, desired.read);
      if (anchor.kind == AnchorCase::Missing &&
          desired.kind == DesiredCase::Missing) {
        compareClassified(result, ObservedAuthorityKind::Absent);
      } else if (anchor.kind == AnchorCase::Missing &&
                 desired.kind == DesiredCase::ExactV1) {
        compareClassified(result, ObservedAuthorityKind::V1, {}, 19);
      } else if (anchor.kind == AnchorCase::ExactV2 &&
                 desired.kind == DesiredCase::ExactV2) {
        compareClassified(result, ObservedAuthorityKind::V2, authorityA, 19);
      } else {
        compareClassified(result, ObservedAuthorityKind::Unreadable);
      }
    }
  }
}

void CompositorObservedAuthorityFactsTest::
    v2IdentityAndRevisionEndpointsAreExact() {
  const auto authority = authorityBytes();
  for (const auto revision : {
           quint64{0},
           std::numeric_limits<quint64>::max(),
       }) {
    const auto desired = desiredV2Bytes(revision);
    compareClassified(classify(presentRead(authority), presentRead(desired)),
                      ObservedAuthorityKind::V2, authorityA, revision);
  }

  const auto mismatched = desiredV2Bytes(23, authorityB);
  compareClassified(classify(presentRead(authority), presentRead(mismatched)),
                    ObservedAuthorityKind::Unreadable);
}

void CompositorObservedAuthorityFactsTest::
    everyFrozenV1LineagePatchAndRevisionIsExact() {
  const std::array catalogs{
      oldCatalogA,
      oldCatalogB,
      QString::fromLatin1(reviewedCatalogDigest),
  };
  const std::array actions{
      oldAction,
      QString::fromLatin1(reviewedActionCatalogDigest),
  };
  const std::array patches{
      quint32{0},
      quint32{1},
      quint32{2},
      quint32{3},
      std::numeric_limits<quint32>::max(),
  };
  const std::array revisions{
      quint64{0},
      std::numeric_limits<quint64>::max(),
  };

  for (const auto &catalog : catalogs) {
    for (const auto &action : actions) {
      for (const auto patch : patches) {
        for (const auto revision : revisions) {
          const auto desired = desiredV1Bytes(patch, revision, catalog, action);
          compareClassified(classify(missingRead(), presentRead(desired)),
                            ObservedAuthorityKind::V1, {}, revision);
        }
      }
    }
  }
}

void CompositorObservedAuthorityFactsTest::
    exactV1AndV2FormatsAreMutuallyExclusive() {
  const auto authority = authorityBytes();
  const auto desiredV1 = desiredV1Bytes(2, 31);
  const auto desiredV2 = desiredV2Bytes(31);

  compareClassified(classify(missingRead(), presentRead(desiredV1)),
                    ObservedAuthorityKind::V1, {}, 31);
  compareClassified(classify(presentRead(authority), presentRead(desiredV1)),
                    ObservedAuthorityKind::Unreadable);
  compareClassified(classify(presentRead(authority), presentRead(desiredV2)),
                    ObservedAuthorityKind::V2, authorityA, 31);
  compareClassified(classify(missingRead(), presentRead(desiredV2)),
                    ObservedAuthorityKind::Unreadable);
}

void CompositorObservedAuthorityFactsTest::
    missingEmptyAndUnsafeRemainDistinct() {
  const QByteArray empty;
  compareClassified(classify(missingRead(), missingRead()),
                    ObservedAuthorityKind::Absent);
  compareClassified(classify(presentRead(empty), missingRead()),
                    ObservedAuthorityKind::Unreadable);
  compareClassified(classify(missingRead(), presentRead(empty)),
                    ObservedAuthorityKind::Unreadable);
  compareClassified(classify(unsafeRead(), missingRead()),
                    ObservedAuthorityKind::Unreadable);
  compareClassified(classify(missingRead(), unsafeRead()),
                    ObservedAuthorityKind::Unreadable);
}

void CompositorObservedAuthorityFactsTest::
    invalidReadTagsAndContradictoryPayloadsAreUnreadable() {
  const auto authority = authorityBytes();
  const auto desired = desiredV2Bytes();
  const auto invalidKind = static_cast<ObservedAuthorityReadKind>(999);
  const std::array records{
      BorrowedObservedAuthorityRecords{
          .authority = {.kind = invalidKind, .bytes = {}},
          .desired = missingRead(),
      },
      BorrowedObservedAuthorityRecords{
          .authority = missingRead(),
          .desired = {.kind = invalidKind, .bytes = {}},
      },
      BorrowedObservedAuthorityRecords{
          .authority = {.kind = ObservedAuthorityReadKind::Missing,
                        .bytes = authority},
          .desired = missingRead(),
      },
      BorrowedObservedAuthorityRecords{
          .authority = missingRead(),
          .desired = {.kind = ObservedAuthorityReadKind::Missing,
                      .bytes = desired},
      },
      BorrowedObservedAuthorityRecords{
          .authority = {.kind = ObservedAuthorityReadKind::Unsafe,
                        .bytes = authority},
          .desired = missingRead(),
      },
      BorrowedObservedAuthorityRecords{
          .authority = missingRead(),
          .desired = {.kind = ObservedAuthorityReadKind::Unsafe,
                      .bytes = desired},
      },
  };
  for (const auto &record : records) {
    compareClassified(buildObservedAuthorityFacts(record, catalogV1_,
                                                  actionCatalogV1_, catalogV2_,
                                                  actionCatalogV2_),
                      ObservedAuthorityKind::Unreadable);
  }
}

void CompositorObservedAuthorityFactsTest::
    malformedAndNoncanonicalRecordsAreUnreadable() {
  const auto authority = authorityBytes();
  const auto desired = desiredV2Bytes();

  auto authorityRoot = JsonSupport::parseStrictObject(
      authority, maximumAuthorityRecordV2Bytes, 8);
  auto desiredRoot =
      JsonSupport::parseStrictObject(desired, maximumDesiredStateBytes, 64);
  QVERIFY(authorityRoot);
  QVERIFY(desiredRoot);

  auto authorityWithNul = authority;
  authorityWithNul.insert(authorityWithNul.size() - 1, '\0');
  auto unknownAuthority = *authorityRoot.value;
  unknownAuthority.insert(QStringLiteral("unknown"), true);
  const std::array invalidAuthorities{
      authorityWithNul,
      QByteArrayLiteral("\xEF\xBB\xBF") + authority,
      QByteArrayLiteral(" ") + authority,
      authority.chopped(1),
      authority + QByteArrayLiteral("\n"),
      canonicalObjectWithLf(unknownAuthority),
      QJsonDocument(*authorityRoot.value).toJson(QJsonDocument::Indented),
      authority.first(authority.size() / 2),
  };
  for (const auto &bytes : invalidAuthorities) {
    compareClassified(classify(presentRead(bytes), missingRead()),
                      ObservedAuthorityKind::Unreadable);
  }

  auto desiredWithNul = desired;
  desiredWithNul.insert(desiredWithNul.size() - 1, '\0');
  auto unknownDesired = *desiredRoot.value;
  unknownDesired.insert(QStringLiteral("unknown"), true);
  const std::array invalidDesired{
      desiredWithNul,
      QByteArrayLiteral("\xEF\xBB\xBF") + desired,
      QByteArrayLiteral(" ") + desired,
      desired.chopped(1),
      desired + QByteArrayLiteral("\n"),
      canonicalObjectWithLf(unknownDesired),
      QJsonDocument(*desiredRoot.value).toJson(QJsonDocument::Indented),
      desired.first(desired.size() / 2),
  };
  for (const auto &bytes : invalidDesired) {
    compareClassified(classify(missingRead(), presentRead(bytes)),
                      ObservedAuthorityKind::Unreadable);
  }
}

void CompositorObservedAuthorityFactsTest::
    recordBoundsAreClosedAtBothPositions() {
  const QByteArray authorityAtMaximum(maximumAuthorityRecordV2Bytes, 'x');
  const QByteArray authorityOverMaximum(maximumAuthorityRecordV2Bytes + 1, 'x');
  const QByteArray desiredAtMaximum(maximumDesiredStateBytes, 'x');
  const QByteArray desiredOverMaximum(maximumDesiredStateBytes + 1, 'x');

  for (const auto &bytes : {authorityAtMaximum, authorityOverMaximum}) {
    compareClassified(classify(presentRead(bytes), missingRead()),
                      ObservedAuthorityKind::Unreadable);
  }
  for (const auto &bytes : {desiredAtMaximum, desiredOverMaximum}) {
    compareClassified(classify(missingRead(), presentRead(bytes)),
                      ObservedAuthorityKind::Unreadable);
  }
}

void CompositorObservedAuthorityFactsTest::
    invalidViewMetadataIsRejectedAtBothPositions() {
  const char byte = 'x';
  // QT_NO_DEBUG is scoped to this focused executable. The dormant archive
  // retains normal assertions and receives these malformed public views.
  const std::array invalidViews{
      QByteArrayView(&byte, qsizetype{-1}),
      QByteArrayView(static_cast<const char *>(nullptr), qsizetype{1}),
  };
  for (const auto invalid : invalidViews) {
    compareClassified(classify(presentRead(invalid), missingRead()),
                      ObservedAuthorityKind::Unreadable);
    compareClassified(classify(missingRead(), presentRead(invalid)),
                      ObservedAuthorityKind::Unreadable);
  }
}

void CompositorObservedAuthorityFactsTest::
    borrowedSharedNonterminatedViewsDoNotEscape() {
  const auto authority = authorityBytes();
  const auto desired = desiredV2Bytes(std::numeric_limits<quint64>::max());
  QByteArray backing = QByteArrayLiteral("!");
  const auto authorityOffset = backing.size();
  backing.append(authority);
  backing.append('@');
  const auto desiredOffset = backing.size();
  backing.append(desired);
  backing.append('?');

  const auto authorityView =
      QByteArrayView(backing.constData() + authorityOffset, authority.size());
  const auto desiredView =
      QByteArrayView(backing.constData() + desiredOffset, desired.size());
  const auto result =
      classify(presentRead(authorityView), presentRead(desiredView));
  backing.fill('x');

  compareClassified(result, ObservedAuthorityKind::V2, authorityA,
                    std::numeric_limits<quint64>::max());
}

void CompositorObservedAuthorityFactsTest::
    everyTypedParserAuthorityIsIndependentlyRequired() {
  const auto expectInvalid = [this](const Catalog &catalogV1,
                                    const ActionCatalog &actionsV1,
                                    const Catalog &catalogV2,
                                    const ActionCatalog &actionsV2) {
    compareInvalidAuthorities(classify(missingRead(), missingRead(), catalogV1,
                                       actionsV1, catalogV2, actionsV2));
  };

  auto badCatalogV1 = catalogV1_;
  badCatalogV1.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  expectInvalid(badCatalogV1, actionCatalogV1_, catalogV2_, actionCatalogV2_);

  auto badActionsV1 = actionCatalogV1_;
  badActionsV1.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  expectInvalid(catalogV1_, badActionsV1, catalogV2_, actionCatalogV2_);

  auto badRetainedSchemaV1 = actionCatalogV1_;
  badRetainedSchemaV1.configSchemaDocument = QByteArrayLiteral("{}");
  expectInvalid(catalogV1_, badRetainedSchemaV1, catalogV2_, actionCatalogV2_);

  auto badCanonicalSchemaV1 = actionCatalogV1_;
  badCanonicalSchemaV1.canonicalConfigSchema.insert(QStringLiteral("unknown"),
                                                    true);
  expectInvalid(catalogV1_, badCanonicalSchemaV1, catalogV2_, actionCatalogV2_);

  auto badCatalogV2 = catalogV2_;
  badCatalogV2.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  expectInvalid(catalogV1_, actionCatalogV1_, badCatalogV2, actionCatalogV2_);

  auto badActionsV2 = actionCatalogV2_;
  badActionsV2.canonicalDocument.insert(QStringLiteral("contractVersion"), 99);
  expectInvalid(catalogV1_, actionCatalogV1_, catalogV2_, badActionsV2);

  auto badRetainedSchemaV2 = actionCatalogV2_;
  badRetainedSchemaV2.configSchemaDocument = QByteArrayLiteral("{}");
  expectInvalid(catalogV1_, actionCatalogV1_, catalogV2_, badRetainedSchemaV2);

  auto badCanonicalSchemaV2 = actionCatalogV2_;
  badCanonicalSchemaV2.canonicalConfigSchema.insert(QStringLiteral("unknown"),
                                                    true);
  expectInvalid(catalogV1_, actionCatalogV1_, catalogV2_, badCanonicalSchemaV2);
}

void CompositorObservedAuthorityFactsTest::
    parserAuthorityFailurePrecedesEveryRecordClassification() {
  auto badCatalogV1 = catalogV1_;
  badCatalogV1.canonicalDocument = {};

  compareInvalidAuthorities(classify(missingRead(), missingRead(), badCatalogV1,
                                     actionCatalogV1_, catalogV2_,
                                     actionCatalogV2_));
  compareInvalidAuthorities(classify(unsafeRead(), unsafeRead(), badCatalogV1,
                                     actionCatalogV1_, catalogV2_,
                                     actionCatalogV2_));

  const char byte = 'x';
  const auto negative = QByteArrayView(&byte, qsizetype{-1});
  const auto nullPositive =
      QByteArrayView(static_cast<const char *>(nullptr), qsizetype{1});
  compareInvalidAuthorities(
      classify(presentRead(negative), presentRead(nullPositive), badCatalogV1,
               actionCatalogV1_, catalogV2_, actionCatalogV2_));
}

void CompositorObservedAuthorityFactsTest::
    nonauthoritativeTypedCachesCannotAffectClassification() {
  auto catalogV1 = catalogV1_;
  auto actionsV1 = actionCatalogV1_;
  auto catalogV2 = catalogV2_;
  auto actionsV2 = actionCatalogV2_;
  catalogV1.digest = QStringLiteral("not-authoritative");
  catalogV1.options.clear();
  actionsV1.digest = QStringLiteral("not-authoritative");
  actionsV1.dispatcherActions.clear();
  catalogV2.digest = QStringLiteral("not-authoritative");
  catalogV2.options.clear();
  actionsV2.digest = QStringLiteral("not-authoritative");
  actionsV2.dispatcherActions.clear();

  const auto authority = authorityBytes();
  const auto desiredV2 = desiredV2Bytes(47);
  compareClassified(classify(presentRead(authority), presentRead(desiredV2),
                             catalogV1, actionsV1, catalogV2, actionsV2),
                    ObservedAuthorityKind::V2, authorityA, 47);

  const auto desiredV1 = desiredV1Bytes(1, 48);
  compareClassified(classify(missingRead(), presentRead(desiredV1), catalogV1,
                             actionsV1, catalogV2, actionsV2),
                    ObservedAuthorityKind::V1, {}, 48);
}

QTEST_APPLESS_MAIN(CompositorObservedAuthorityFactsTest)

#include "compositor_observed_authority_facts_test.moc"
