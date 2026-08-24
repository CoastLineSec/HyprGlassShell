#include "compositord/authority_cas_reducer.h"

#include <QtTest>

#include <array>
#include <limits>
#include <type_traits>

using namespace HyprShelld::Compositor;

namespace {

const QString authorityA = QStringLiteral("11111111111111111111111111111111");
const QString authorityB = QStringLiteral("22222222222222222222222222222222");
const QString catalogA = QString(64, QLatin1Char('a'));
const QString catalogB = QString(64, QLatin1Char('b'));
const QString actionsA = QString(64, QLatin1Char('c'));
const QString actionsB = QString(64, QLatin1Char('d'));

[[nodiscard]] QString replacingCharacter(QString value, const qsizetype index,
                                         const QChar replacement) {
  value[index] = replacement;
  return value;
}

[[nodiscard]] std::array<QString, 10> invalidIdentifiers() {
  return {
      QString(),
      QStringLiteral(""),
      QString(31, QLatin1Char('1')),
      QString(33, QLatin1Char('1')),
      replacingCharacter(authorityA, 7, QChar(u'\0')),
      replacingCharacter(authorityA, 7, QChar(u'g')),
      replacingCharacter(authorityA, 7, QChar(u'\uff11')),
      QString(32, QLatin1Char('A')),
      QString(32, QLatin1Char('0')),
      QString(4096, QLatin1Char('1')),
  };
}

[[nodiscard]] std::array<QString, 9> invalidDigests() {
  return {
      QString(),
      QStringLiteral(""),
      QString(63, QLatin1Char('a')),
      QString(65, QLatin1Char('a')),
      replacingCharacter(catalogA, 19, QChar(u'\0')),
      replacingCharacter(catalogA, 19, QChar(u'g')),
      replacingCharacter(catalogA, 19, QChar(u'\uff41')),
      QString(64, QLatin1Char('A')),
      QString(4096, QLatin1Char('a')),
  };
}

struct CurrentTuple final {
  AuthorityAccess access = AuthorityAccess::Writable;
  QString authorityId = authorityA;
  quint64 revision = 41;
  QString catalogDigest = catalogA;
  QString actionCatalogDigest = actionsA;

  [[nodiscard]] CurrentAuthorityCasV2View view() const {
    return {
        .access = access,
        .authorityId = authorityId,
        .revision = revision,
        .catalogDigest = catalogDigest,
        .actionCatalogDigest = actionCatalogDigest,
    };
  }
};

struct ExpectedTuple final {
  QString authorityId = authorityA;
  quint64 revision = 41;
  QString catalogDigest = catalogA;
  QString actionCatalogDigest = actionsA;

  [[nodiscard]] ExpectedAuthorityCasV2View view() const {
    return {
        .authorityId = authorityId,
        .revision = revision,
        .catalogDigest = catalogDigest,
        .actionCatalogDigest = actionCatalogDigest,
    };
  }
};

[[nodiscard]] AuthorityCasDecision
expectedDecision(const AuthorityAccess access, const int mismatchMask) {
  switch (access) {
  case AuthorityAccess::Unavailable:
    return AuthorityCasDecision::Unavailable;
  case AuthorityAccess::ReadOnly:
    return AuthorityCasDecision::ReadOnly;
  case AuthorityAccess::Writable:
    break;
  default:
    return AuthorityCasDecision::Unavailable;
  }
  if ((mismatchMask & 1) != 0) {
    return AuthorityCasDecision::StaleAuthority;
  }
  if ((mismatchMask & (2 | 4)) != 0) {
    return AuthorityCasDecision::StaleCatalogDigest;
  }
  if ((mismatchMask & 8) != 0) {
    return AuthorityCasDecision::StaleRevision;
  }
  return AuthorityCasDecision::Proceed;
}

} // namespace

class CompositorAuthorityCasReducerTest final : public QObject {
  Q_OBJECT

private slots:
  void mismatchMaskAndAccessPrecedenceAreExhaustive();
  void malformedCurrentTupleIsUnavailableBeforeReadOnly();
  void malformedExpectedFieldsUseTheirMismatchClass();
  void allZeroIdentifierAndDigestsRemainDistinct();
  void borrowedSubstringViewsNeedNoTerminator();
  void namedCatalogAndRevisionRows_data();
  void namedCatalogAndRevisionRows();
  void revisionEndpointsAreExact();
};

void CompositorAuthorityCasReducerTest::
    mismatchMaskAndAccessPrecedenceAreExhaustive() {
  using AccessValue = std::underlying_type_t<AuthorityAccess>;
  const std::array accesses{
      AuthorityAccess::Unavailable,
      AuthorityAccess::ReadOnly,
      AuthorityAccess::Writable,
      static_cast<AuthorityAccess>(-1),
      static_cast<AuthorityAccess>(3),
      static_cast<AuthorityAccess>(std::numeric_limits<AccessValue>::max()),
  };
  for (const auto access : accesses) {
    for (int mask = 0; mask < 16; ++mask) {
      CurrentTuple current;
      current.access = access;
      ExpectedTuple expected;
      if ((mask & 1) != 0) {
        expected.authorityId = authorityB;
      }
      if ((mask & 2) != 0) {
        expected.catalogDigest = catalogB;
      }
      if ((mask & 4) != 0) {
        expected.actionCatalogDigest = actionsB;
      }
      if ((mask & 8) != 0) {
        expected.revision = 42;
      }
      QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
               expectedDecision(access, mask));
    }
  }
}

void CompositorAuthorityCasReducerTest::
    malformedCurrentTupleIsUnavailableBeforeReadOnly() {
  enum class Field {
    Authority,
    Catalog,
    ActionCatalog,
  };
  const std::array accesses{
      AuthorityAccess::ReadOnly,
      AuthorityAccess::Writable,
  };
  ExpectedTuple expected;
  expected.authorityId = authorityB;
  expected.catalogDigest = catalogB;
  expected.actionCatalogDigest = actionsB;
  expected.revision = 42;
  const auto identifiers = invalidIdentifiers();
  const auto digests = invalidDigests();
  QVERIFY(identifiers.at(0).isNull());
  QVERIFY(!identifiers.at(1).isNull());
  QVERIFY(digests.at(0).isNull());
  QVERIFY(!digests.at(1).isNull());

  for (const auto access : accesses) {
    for (const auto &value : identifiers) {
      CurrentTuple current;
      current.access = access;
      current.authorityId = value;
      QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
               AuthorityCasDecision::Unavailable);
    }
    for (const auto field : {Field::Catalog, Field::ActionCatalog}) {
      for (const auto &value : digests) {
        CurrentTuple current;
        current.access = access;
        if (field == Field::Catalog) {
          current.catalogDigest = value;
        } else {
          current.actionCatalogDigest = value;
        }
        QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
                 AuthorityCasDecision::Unavailable);
      }
    }
  }
}

void CompositorAuthorityCasReducerTest::
    malformedExpectedFieldsUseTheirMismatchClass() {
  const CurrentTuple current;
  const auto identifiers = invalidIdentifiers();
  const auto digests = invalidDigests();
  QVERIFY(identifiers.at(0).isNull());
  QVERIFY(!identifiers.at(1).isNull());
  QVERIFY(digests.at(0).isNull());
  QVERIFY(!digests.at(1).isNull());

  for (const auto &invalid : identifiers) {
    ExpectedTuple expected;
    expected.authorityId = invalid;
    expected.catalogDigest.clear();
    expected.actionCatalogDigest.clear();
    expected.revision = 42;
    QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
             AuthorityCasDecision::StaleAuthority);
  }

  for (const auto &invalid : digests) {
    ExpectedTuple expected;
    expected.catalogDigest = invalid;
    expected.revision = 42;
    QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
             AuthorityCasDecision::StaleCatalogDigest);

    expected = ExpectedTuple{};
    expected.actionCatalogDigest = invalid;
    expected.revision = 42;
    QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
             AuthorityCasDecision::StaleCatalogDigest);
  }
}

void CompositorAuthorityCasReducerTest::
    allZeroIdentifierAndDigestsRemainDistinct() {
  const QString zeroIdentifier(32, QLatin1Char('0'));
  const QString zeroDigest(64, QLatin1Char('0'));

  CurrentTuple malformedCurrent;
  malformedCurrent.authorityId = zeroIdentifier;
  const ExpectedTuple ordinaryExpected;
  QCOMPARE(
      reduceAuthorityCasV2(malformedCurrent.view(), ordinaryExpected.view()),
      AuthorityCasDecision::Unavailable);

  const CurrentTuple ordinaryCurrent;
  ExpectedTuple malformedExpected;
  malformedExpected.authorityId = zeroIdentifier;
  QCOMPARE(
      reduceAuthorityCasV2(ordinaryCurrent.view(), malformedExpected.view()),
      AuthorityCasDecision::StaleAuthority);

  CurrentTuple zeroDigestCurrent;
  zeroDigestCurrent.catalogDigest = zeroDigest;
  zeroDigestCurrent.actionCatalogDigest = zeroDigest;
  ExpectedTuple zeroDigestExpected;
  zeroDigestExpected.catalogDigest = zeroDigest;
  zeroDigestExpected.actionCatalogDigest = zeroDigest;
  QCOMPARE(
      reduceAuthorityCasV2(zeroDigestCurrent.view(), zeroDigestExpected.view()),
      AuthorityCasDecision::Proceed);
}

void CompositorAuthorityCasReducerTest::
    borrowedSubstringViewsNeedNoTerminator() {
  const QString currentAuthorityStorage =
      QLatin1Char('g') + authorityA + QLatin1Char('g');
  const QString currentCatalogStorage =
      QLatin1Char('g') + catalogA + QLatin1Char('g');
  const QString currentActionStorage =
      QLatin1Char('g') + actionsA + QLatin1Char('g');
  const QString expectedAuthorityStorage =
      QLatin1Char('x') + authorityA + QLatin1Char('x');
  const QString expectedCatalogStorage =
      QLatin1Char('x') + catalogA + QLatin1Char('x');
  const QString expectedActionStorage =
      QLatin1Char('x') + actionsA + QLatin1Char('x');
  const auto currentAuthority =
      QStringView(currentAuthorityStorage).sliced(1, 32);
  const auto currentCatalog = QStringView(currentCatalogStorage).sliced(1, 64);
  const auto currentAction = QStringView(currentActionStorage).sliced(1, 64);
  const auto expectedAuthority =
      QStringView(expectedAuthorityStorage).sliced(1, 32);
  const auto expectedCatalog =
      QStringView(expectedCatalogStorage).sliced(1, 64);
  const auto expectedAction = QStringView(expectedActionStorage).sliced(1, 64);
  const CurrentAuthorityCasV2View current{
      .access = AuthorityAccess::Writable,
      .authorityId = currentAuthority,
      .revision = 41,
      .catalogDigest = currentCatalog,
      .actionCatalogDigest = currentAction,
  };
  const ExpectedAuthorityCasV2View expected{
      .authorityId = expectedAuthority,
      .revision = 41,
      .catalogDigest = expectedCatalog,
      .actionCatalogDigest = expectedAction,
  };
  QCOMPARE(reduceAuthorityCasV2(current, expected),
           AuthorityCasDecision::Proceed);
  const ExpectedAuthorityCasV2View aliasedExpected{
      .authorityId = currentAuthority,
      .revision = 41,
      .catalogDigest = currentCatalog,
      .actionCatalogDigest = currentAction,
  };
  QCOMPARE(reduceAuthorityCasV2(current, aliasedExpected),
           AuthorityCasDecision::Proceed);
}

void CompositorAuthorityCasReducerTest::namedCatalogAndRevisionRows_data() {
  QTest::addColumn<bool>("catalogMismatch");
  QTest::addColumn<bool>("actionMismatch");
  QTest::addColumn<bool>("revisionMismatch");
  QTest::addColumn<int>("decision");

  QTest::newRow("catalog") << true << false << false
                           << static_cast<int>(
                                  AuthorityCasDecision::StaleCatalogDigest);
  QTest::newRow("action-catalog")
      << false << true << false
      << static_cast<int>(AuthorityCasDecision::StaleCatalogDigest);
  QTest::newRow("both-catalogs")
      << true << true << false
      << static_cast<int>(AuthorityCasDecision::StaleCatalogDigest);
  QTest::newRow("catalog-before-revision")
      << true << false << true
      << static_cast<int>(AuthorityCasDecision::StaleCatalogDigest);
  QTest::newRow("action-catalog-before-revision")
      << false << true << true
      << static_cast<int>(AuthorityCasDecision::StaleCatalogDigest);
  QTest::newRow("both-catalogs-before-revision")
      << true << true << true
      << static_cast<int>(AuthorityCasDecision::StaleCatalogDigest);
  QTest::newRow("revision-only")
      << false << false << true
      << static_cast<int>(AuthorityCasDecision::StaleRevision);
}

void CompositorAuthorityCasReducerTest::namedCatalogAndRevisionRows() {
  QFETCH(bool, catalogMismatch);
  QFETCH(bool, actionMismatch);
  QFETCH(bool, revisionMismatch);
  QFETCH(int, decision);

  const CurrentTuple current;
  ExpectedTuple expected;
  if (catalogMismatch) {
    expected.catalogDigest = catalogB;
  }
  if (actionMismatch) {
    expected.actionCatalogDigest = actionsB;
  }
  if (revisionMismatch) {
    expected.revision = 42;
  }
  QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
           static_cast<AuthorityCasDecision>(decision));
}

void CompositorAuthorityCasReducerTest::revisionEndpointsAreExact() {
  constexpr std::array endpoints{
      quint64{0},
      std::numeric_limits<quint64>::max(),
  };
  for (const auto revision : endpoints) {
    CurrentTuple current;
    current.revision = revision;
    ExpectedTuple expected;
    expected.revision = revision;
    QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
             AuthorityCasDecision::Proceed);

    expected.revision =
        revision == 0 ? std::numeric_limits<quint64>::max() : quint64{0};
    QCOMPARE(reduceAuthorityCasV2(current.view(), expected.view()),
             AuthorityCasDecision::StaleRevision);
  }
}

QTEST_APPLESS_MAIN(CompositorAuthorityCasReducerTest)

#include "compositor_authority_cas_reducer_test.moc"
