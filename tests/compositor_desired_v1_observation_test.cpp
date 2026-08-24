#include "compositord/desired_migration_reducer.h"

#include "hyprland/json_support.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <array>
#include <limits>
#include <type_traits>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString oldCatalogA = QStringLiteral(
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0");
const QString oldCatalogB = QStringLiteral(
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388");
const QString oldAction = QStringLiteral(
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2");

static_assert(std::is_trivially_copyable_v<ExactDesiredV1Observation>);

[[nodiscard]] QByteArray readBytes(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

[[nodiscard]] QJsonObject strictObject(const QByteArrayView bytes) {
  const auto parsed =
      JsonSupport::parseStrictObject(bytes, maximumDesiredStateBytes, 64);
  return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QByteArray canonicalObject(const QJsonObject &object) {
  auto bytes = JsonSupport::canonicalJson(object);
  bytes.append('\n');
  return bytes;
}

} // namespace

class CompositorDesiredV1ObservationTest final : public QObject {
  Q_OBJECT

private:
  Catalog catalog_;
  ActionCatalog actions_;

  [[nodiscard]] QByteArray source(
      const quint32 patch = 1, const quint64 revision = 7,
      const QString &catalogDigestValue = QLatin1String(reviewedCatalogDigest),
      const QString &actionDigestValue =
          QLatin1String(reviewedActionCatalogDigest),
      const bool protectedRule = true) const {
    auto state = defaultDesiredState(catalog_, actions_);
    state.targetHyprland = QStringLiteral("0.56.%1").arg(patch);
    state.revision = revision;
    state.catalogDigest = catalogDigestValue;
    state.actionCatalogDigest = actionDigestValue;
    if (!protectedRule) {
      state.workspaceRules.clear();
    }
    return serializeDesiredState(state);
  }

  [[nodiscard]] std::optional<ExactDesiredV1Observation>
  inspect(const QByteArrayView bytes, const Catalog &catalog,
          const ActionCatalog &actions) const {
    return inspectExactDesiredV1Observation(bytes, catalog, actions);
  }

  [[nodiscard]] std::optional<ExactDesiredV1Observation>
  inspect(const QByteArrayView bytes) const {
    return inspect(bytes, catalog_, actions_);
  }

private slots:
  void initTestCase() {
    const auto parsedCatalog = parseCatalog(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE)));
    const auto parsedActions = parseActionCatalog(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_ACTION_FILE)),
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE)));
    QVERIFY(parsedCatalog);
    QVERIFY(parsedActions);
    catalog_ = *parsedCatalog.value;
    actions_ = *parsedActions.value;
  }

  void allFrozenLineagePairsAreExact() {
    const std::array catalogs{
        oldCatalogA,
        oldCatalogB,
        QString::fromLatin1(reviewedCatalogDigest),
    };
    const std::array actions{
        oldAction,
        QString::fromLatin1(reviewedActionCatalogDigest),
    };

    for (const auto &catalog : catalogs) {
      for (const auto &action : actions) {
        const auto observed = inspect(source(1, 17, catalog, action, false));
        QVERIFY(observed);
        QCOMPARE(observed->revision, quint64{17});
        QCOMPARE(observed->sourcePatch, quint32{1});
      }
    }
  }

  void everyPatchAndRevisionEndpointIsReturnedExactly() {
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

    for (const auto patch : patches) {
      for (const auto revision : revisions) {
        const auto observed = inspect(source(patch, revision));
        QVERIFY(observed);
        QCOMPARE(observed->revision, revision);
        QCOMPARE(observed->sourcePatch, patch);
      }
    }
  }

  void unknownLineageAndOldActionProtectedSelectorFailClosed() {
    QVERIFY(
        !inspect(source(1, 7, QString(64, QLatin1Char('a')),
                        QLatin1String(reviewedActionCatalogDigest), false)));
    QVERIFY(!inspect(source(1, 7, QLatin1String(reviewedCatalogDigest),
                            QString(64, QLatin1Char('b')), false)));

    QVERIFY(!inspect(
        source(1, 7, QLatin1String(reviewedCatalogDigest), oldAction, true)));
    QVERIFY(inspect(
        source(1, 7, QLatin1String(reviewedCatalogDigest), oldAction, false)));

    auto root = strictObject(
        source(1, 7, QLatin1String(reviewedCatalogDigest), oldAction, false));
    root.insert(QStringLiteral("workspaceRules"),
                QJsonArray{QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("ordinary")},
                    {QStringLiteral("selector"),
                     QLatin1String(sharedSpacingWorkspaceRuleSelector)},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("monitor"), QString()},
                    {QStringLiteral("persistent"), false},
                    {QStringLiteral("isDefault"), false},
                    {QStringLiteral("layout"), QString()},
                    {QStringLiteral("overrides"), QJsonObject{}},
                }});
    QVERIFY(!inspect(canonicalObject(root)));
  }

  void malformedAndOverflowTargetsAndRevisionsFailClosed() {
    auto root = strictObject(source());
    for (const auto &target : {
             QStringLiteral("0.56"),
             QStringLiteral("0.56.01"),
             QStringLiteral("0.56.+1"),
             QStringLiteral("0.56.-1"),
             QStringLiteral(" 0.56.1"),
             QStringLiteral("0.56.1 "),
             QStringLiteral("0.56.4294967296"),
             QStringLiteral("0.57.1"),
         }) {
      root.insert(QStringLiteral("targetHyprland"), target);
      QVERIFY(!inspect(canonicalObject(root)));
    }

    root = strictObject(source());
    for (const auto &revision : {
             QStringLiteral("00"),
             QStringLiteral("+1"),
             QStringLiteral("-1"),
             QStringLiteral(" 1"),
             QStringLiteral("1 "),
             QStringLiteral("18446744073709551616"),
         }) {
      root.insert(QStringLiteral("revision"), revision);
      QVERIFY(!inspect(canonicalObject(root)));
    }
    root.insert(QStringLiteral("revision"), 1);
    QVERIFY(!inspect(canonicalObject(root)));

    root = strictObject(source());
    root.insert(QStringLiteral("unknown"), true);
    QVERIFY(!inspect(canonicalObject(root)));
  }

  void onlyExactCanonicalStoredBytesAreAccepted() {
    const auto exact = source();
    auto duplicateKey = exact;
    duplicateKey.insert(1, QByteArrayLiteral("\"revision\":\"7\","));
    auto noncanonicalEscape = exact;
    const auto canonicalTarget =
        QByteArrayLiteral("\"targetHyprland\":\"0.56.1\"");
    QVERIFY(noncanonicalEscape.contains(canonicalTarget));
    noncanonicalEscape.replace(
        canonicalTarget,
        QByteArrayLiteral("\"targetHyprland\":\"0.56.\\u0031\""));

    const std::array variants{
        exact.first(exact.size() - 1),
        exact + QByteArrayLiteral("\n"),
        QByteArrayLiteral(" ") + exact,
        exact + QByteArrayLiteral(" "),
        QByteArrayLiteral("\xEF\xBB\xBF") + exact,
        QJsonDocument(strictObject(exact)).toJson(QJsonDocument::Indented),
        duplicateKey,
        noncanonicalEscape,
        QByteArrayLiteral("{"),
    };
    for (const auto &variant : variants) {
      QVERIFY(!inspect(variant));
    }
    QVERIFY(inspect(exact));
  }

  void invalidViewMetadataAndBoundsFailBeforeInspection() {
    QVERIFY(!inspect(QByteArrayView{}));
    const QByteArray oversized(maximumDesiredStateBytes + 1, 'x');
    QVERIFY(!inspect(QByteArrayView(oversized)));

    const char byte = 'x';
    // QT_NO_DEBUG is scoped to this test target so both malformed public
    // view shapes can be expressed. The library itself keeps normal flags.
    const std::array invalidViews{
        QByteArrayView(&byte, qsizetype{-1}),
        QByteArrayView(static_cast<const char *>(nullptr), qsizetype{1}),
    };
    for (const auto invalid : invalidViews) {
      QVERIFY(!inspect(invalid));
    }
  }

  void typedAuthoritiesAreReparsedAndCorruptionFailsClosed() {
    const auto bytes = source();

    auto badCatalog = catalog_;
    badCatalog.canonicalDocument.insert(QStringLiteral("contractVersion"), 2);
    QVERIFY(!inspect(bytes, badCatalog, actions_));

    auto badActions = actions_;
    badActions.canonicalDocument.insert(QStringLiteral("contractVersion"), 2);
    QVERIFY(!inspect(bytes, catalog_, badActions));

    badActions = actions_;
    badActions.configSchemaDocument = QByteArrayLiteral("{}");
    QVERIFY(!inspect(bytes, catalog_, badActions));
  }

  void borrowedNonterminatedInputDoesNotEscapeTheCall() {
    const auto bytes = source(3, std::numeric_limits<quint64>::max());
    QByteArray backing = QByteArrayLiteral("!");
    backing.append(bytes);
    backing.append('?');
    const auto view = QByteArrayView(backing.constData() + 1, bytes.size());

    const auto observed = inspect(view);
    QVERIFY(observed);
    backing.fill('x');
    QCOMPARE(observed->revision, std::numeric_limits<quint64>::max());
    QCOMPARE(observed->sourcePatch, quint32{3});
  }
};

QTEST_APPLESS_MAIN(CompositorDesiredV1ObservationTest)

#include "compositor_desired_v1_observation_test.moc"
