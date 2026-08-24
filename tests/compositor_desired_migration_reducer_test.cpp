#include "compositord/desired_migration_reducer.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <array>
#include <limits>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString authorityId =
    QStringLiteral("0123456789abcdef0123456789abcdef");
const QString oldCatalogA = QStringLiteral(
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0"
);
const QString oldCatalogB = QStringLiteral(
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388"
);
const QString oldAction = QStringLiteral(
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2"
);

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] QJsonObject objectFromBytes(const QByteArrayView bytes)
{
    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumDesiredStateBytes, 64
    );
    return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QByteArray canonicalBytes(const QJsonObject &object)
{
    auto bytes = JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QJsonObject bindingRecord()
{
    return {
        {QStringLiteral("id"), QStringLiteral("binding-terminal")},
        {QStringLiteral("modifiers"),
         QJsonArray{QStringLiteral("super")}},
        {QStringLiteral("key"), QStringLiteral("F8")},
        {QStringLiteral("actionType"), QStringLiteral("defaultApp")},
        {QStringLiteral("action"), QStringLiteral("defaultApp.terminal")},
        {QStringLiteral("arguments"), QJsonObject{}},
        {QStringLiteral("description"), QStringLiteral("Terminal")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("submap"), QString()},
        {QStringLiteral("options"),
         QJsonObject{
             {QStringLiteral("repeating"), false},
             {QStringLiteral("locked"), false},
             {QStringLiteral("release"), false},
             {QStringLiteral("nonConsuming"), false},
             {QStringLiteral("autoConsuming"), false},
             {QStringLiteral("transparent"), false},
             {QStringLiteral("ignoreMods"), false},
             {QStringLiteral("dontInhibit"), false},
             {QStringLiteral("longPress"), false},
             {QStringLiteral("submapUniversal"), false},
             {QStringLiteral("click"), false},
             {QStringLiteral("drag"), false},
             {QStringLiteral("allowInputCapture"), false},
         }},
    };
}

} // namespace

class CompositorDesiredMigrationReducerTest final : public QObject {
    Q_OBJECT

private:
    Catalog catalogV1_;
    ActionCatalog actionsV1_;
    Catalog catalogV2_;
    ActionCatalog actionsV2_;
    QByteArray migrationManifest_;
    QByteArray sourceManifestV2_;

    [[nodiscard]] QByteArray source(
        const QString &target = QStringLiteral("0.56.1"),
        const quint64 revision = 0,
        const QString &catalogDigestValue =
            QLatin1String(reviewedCatalogDigest),
        const QString &actionDigestValue =
            QLatin1String(reviewedActionCatalogDigest),
        const bool protectedRule = true
    ) const
    {
        auto state = defaultDesiredState(catalogV1_, actionsV1_);
        state.targetHyprland = target;
        state.revision = revision;
        state.catalogDigest = catalogDigestValue;
        state.actionCatalogDigest = actionDigestValue;
        if (!protectedRule) {
            state.workspaceRules.clear();
        }
        return serializeDesiredState(state);
    }

    [[nodiscard]] DesiredMigrationPlan inspect(
        const QByteArrayView bytes,
        const QByteArrayView migrationManifest,
        const QByteArrayView sourceManifest,
        const Catalog &catalogV2,
        const ActionCatalog &actionsV2
    ) const
    {
        return inspectDesiredV1ToV2Migration(
            bytes,
            migrationManifest,
            sourceManifest,
            catalogV1_,
            actionsV1_,
            catalogV2,
            actionsV2
        );
    }

    [[nodiscard]] DesiredMigrationPlan inspect(
        const QByteArrayView bytes
    ) const
    {
        return inspect(
            bytes,
            migrationManifest_,
            sourceManifestV2_,
            catalogV2_,
            actionsV2_
        );
    }

    [[nodiscard]] std::optional<DesiredMigrationMaterialization> materialize(
        const DesiredMigrationPlan &plan,
        const QString &id = authorityId
    ) const
    {
        return materializeDesiredV1ToV2Migration(
            plan,
            id,
            migrationManifest_,
            sourceManifestV2_,
            catalogV1_,
            actionsV1_,
            catalogV2_,
            actionsV2_
        );
    }

    [[nodiscard]] QByteArray representativeSource() const
    {
        auto root = objectFromBytes(source(QStringLiteral("0.56.0"), 17));
        root.insert(
            QStringLiteral("overrides"),
            QJsonObject{
                {QStringLiteral("hyprland.general.layout"),
                 QStringLiteral("master")},
            }
        );
        root.insert(
            QStringLiteral("monitors"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("monitor-one")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("mode"), QStringLiteral("preferred")},
                {QStringLiteral("position"), QStringLiteral("auto")},
                {QStringLiteral("scale"), 1.0},
                {QStringLiteral("reserved"), QJsonArray{0, 0, 0, 0}},
                {QStringLiteral("transform"), 0},
                {QStringLiteral("mirror"), QString()},
                {QStringLiteral("bitdepth"), 8},
                {QStringLiteral("cm"), QStringLiteral("auto")},
                {QStringLiteral("sdrEotf"), QStringLiteral("default")},
                {QStringLiteral("sdrBrightness"), 1.0},
                {QStringLiteral("sdrSaturation"), 1.0},
                {QStringLiteral("vrr"), 0},
                {QStringLiteral("icc"), QString()},
                {QStringLiteral("supportsWideColor"), 0},
                {QStringLiteral("supportsHdr"), 0},
                {QStringLiteral("sdrMinLuminance"), 0.2},
                {QStringLiteral("sdrMaxLuminance"), 80},
                {QStringLiteral("minLuminance"), -1.0},
                {QStringLiteral("maxLuminance"), -1},
                {QStringLiteral("maxAvgLuminance"), -1},
            }}
        );
        root.insert(
            QStringLiteral("devices"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("keyboard-main")},
                {QStringLiteral("selector"),
                 QStringLiteral("name:Main Keyboard")},
                {QStringLiteral("kind"), QStringLiteral("keyboard")},
                {QStringLiteral("enabled"), false},
                {QStringLiteral("overrides"),
                 QJsonObject{
                     {QStringLiteral("kb_layout"), QStringLiteral("us")},
                     {QStringLiteral("repeat_rate"), 40},
                 }},
            }}
        );
        root.insert(
            QStringLiteral("curves"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("curve-bezier")},
                    {QStringLiteral("name"), QStringLiteral("ease-custom")},
                    {QStringLiteral("type"), QStringLiteral("bezier")},
                    {QStringLiteral("points"),
                     QJsonArray{
                         QJsonArray{0.2, 0.0}, QJsonArray{0.8, 1.0}
                     }},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("curve-spring")},
                    {QStringLiteral("name"), QStringLiteral("spring-custom")},
                    {QStringLiteral("type"), QStringLiteral("spring")},
                    {QStringLiteral("stiffness"), 100.0},
                    {QStringLiteral("dampening"), 12.0},
                    {QStringLiteral("mass"), 1.5},
                },
            }
        );
        root.insert(
            QStringLiteral("animations"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("animation-windows")},
                {QStringLiteral("name"), QStringLiteral("windows")},
                {QStringLiteral("enabled"), false},
                {QStringLiteral("speed"), 6.0},
                {QStringLiteral("curve"), QStringLiteral("spring-custom")},
                {QStringLiteral("style"), QStringLiteral("slide")},
            }}
        );
        root.insert(
            QStringLiteral("gestures"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("gesture-special")},
                {QStringLiteral("fingers"), 4},
                {QStringLiteral("direction"), QStringLiteral("left")},
                {QStringLiteral("modifiers"),
                 QJsonArray{QStringLiteral("super")}},
                {QStringLiteral("scale"), 1.25},
                {QStringLiteral("disableInhibit"), false},
                {QStringLiteral("action"),
                 QJsonObject{
                     {QStringLiteral("type"), QStringLiteral("special")},
                     {QStringLiteral("workspace"), QStringLiteral("magic")},
                 }},
            }}
        );
        const auto protectedRule =
            root.value(QStringLiteral("workspaceRules")).toArray().first();
        root.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("workspace-one")},
                    {QStringLiteral("selector"), QStringLiteral("1")},
                    {QStringLiteral("enabled"), false},
                    {QStringLiteral("monitor"), QString()},
                    {QStringLiteral("persistent"), false},
                    {QStringLiteral("isDefault"), false},
                    {QStringLiteral("layout"), QString()},
                    {QStringLiteral("overrides"), QJsonObject{}},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("workspace-two")},
                    {QStringLiteral("selector"), QStringLiteral("2")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("monitor"), QString()},
                    {QStringLiteral("persistent"), true},
                    {QStringLiteral("isDefault"), false},
                    {QStringLiteral("layout"), QStringLiteral("dwindle")},
                    {QStringLiteral("overrides"),
                     QJsonObject{
                         {QStringLiteral("gaps_in"), QJsonArray{4, 4, 4, 4}},
                     }},
                },
                protectedRule,
            }
        );
        root.insert(
            QStringLiteral("windowRules"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("window-browser")},
                {QStringLiteral("name"), QStringLiteral("Browser")},
                {QStringLiteral("enabled"), false},
                {QStringLiteral("match"),
                 QJsonObject{
                     {QStringLiteral("class"), QStringLiteral("^firefox$")},
                     {QStringLiteral("float"), false},
                 }},
                {QStringLiteral("effects"),
                 QJsonObject{
                     {QStringLiteral("float"), true},
                     {QStringLiteral("rounding"), 0},
                 }},
            }}
        );
        root.insert(
            QStringLiteral("layerRules"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("layer-panel")},
                {QStringLiteral("name"), QStringLiteral("Panel")},
                {QStringLiteral("enabled"), false},
                {QStringLiteral("match"),
                 QJsonObject{
                     {QStringLiteral("namespace"), QStringLiteral("^panel$")},
                 }},
                {QStringLiteral("effects"),
                 QJsonObject{
                     {QStringLiteral("ignore_alpha"), 0.5},
                     {QStringLiteral("above_lock"), 0},
                 }},
            }}
        );
        root.insert(
            QStringLiteral("submaps"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("submap-dormant")},
                {QStringLiteral("name"), QStringLiteral("dormant")},
                {QStringLiteral("reset"), QString()},
                {QStringLiteral("enabled"), false},
            }}
        );
        root.insert(
            QStringLiteral("bindings"), QJsonArray{bindingRecord()}
        );
        root.insert(
            QStringLiteral("permissions"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("permission-one")},
                {QStringLiteral("binary"), QStringLiteral("^/usr/bin/foo$")},
                {QStringLiteral("type"), QStringLiteral("screencopy")},
                {QStringLiteral("mode"), QStringLiteral("deny")},
            }}
        );
        root.insert(
            QStringLiteral("environment"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("env-empty")},
                    {QStringLiteral("name"), QStringLiteral("D052_EMPTY")},
                    {QStringLiteral("value"), QString()},
                    {QStringLiteral("scope"), QStringLiteral("hyprland")},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("env-value")},
                    {QStringLiteral("name"), QStringLiteral("D052_VALUE")},
                    {QStringLiteral("value"), QStringLiteral("yes")},
                    {QStringLiteral("scope"), QStringLiteral("uwsm")},
                },
            }
        );
        return canonicalBytes(root);
    }

private slots:
    void initTestCase()
    {
        const auto v1Catalog = parseCatalog(readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE
        )));
        const auto v1Actions = parseActionCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_ACTION_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE))
        );
        const auto v2Catalog = parseDormantCatalogV2(readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE
        )));
        const auto v2Actions = parseDormantActionCatalogV2(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_SCHEMA_FILE))
        );
        QVERIFY(v1Catalog);
        QVERIFY(v1Actions);
        QVERIFY(v2Catalog);
        QVERIFY(v2Actions);
        catalogV1_ = *v1Catalog.value;
        actionsV1_ = *v1Actions.value;
        catalogV2_ = *v2Catalog.value;
        actionsV2_ = *v2Actions.value;
        migrationManifest_ = readBytes(QStringLiteral(
            HYPRSHELLD_MIGRATION_MANIFEST_FILE
        ));
        sourceManifestV2_ = readBytes(QStringLiteral(
            HYPRSHELLD_SOURCE_MANIFEST_V2_FILE
        ));
        QCOMPARE(migrationManifest_.size(), qsizetype(37215));
        QCOMPARE(sourceManifestV2_.size(), qsizetype(73262));
    }

    void evidenceIsExactAndSourceValidityHasPrecedence()
    {
        const auto bytes = source();
        QCOMPARE(inspect(bytes).disposition(),
                 DesiredMigrationDisposition::Eligible);

        auto badMigration = migrationManifest_;
        badMigration[0] = badMigration[0] == 'x' ? 'y' : 'x';
        QCOMPARE(inspect(bytes, badMigration, sourceManifestV2_, catalogV2_,
                         actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);
        QCOMPARE(inspect(bytes, QByteArrayView{}, sourceManifestV2_,
                         catalogV2_, actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);
        QCOMPARE(inspect(bytes, migrationManifest_.first(100),
                         sourceManifestV2_, catalogV2_,
                         actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);
        badMigration.append('x');
        QCOMPARE(inspect(bytes, badMigration, sourceManifestV2_, catalogV2_,
                         actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);

        auto badSourceManifest = sourceManifestV2_;
        badSourceManifest[0] = badSourceManifest[0] == 'x' ? 'y' : 'x';
        QCOMPARE(inspect(bytes, migrationManifest_, badSourceManifest,
                         catalogV2_, actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);
        QCOMPARE(inspect(bytes, migrationManifest_, QByteArrayView{},
                         catalogV2_, actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);
        QCOMPARE(inspect(bytes, migrationManifest_,
                         sourceManifestV2_.first(100), catalogV2_,
                         actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);

        auto badV2Catalog = catalogV2_;
        badV2Catalog.canonicalDocument.insert(
            QStringLiteral("sourceManifestDigest"), QString(64, '0')
        );
        QCOMPARE(inspect(bytes, migrationManifest_, sourceManifestV2_,
                         badV2Catalog, actionsV2_).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);
        auto badV2Actions = actionsV2_;
        badV2Actions.canonicalDocument.insert(
            QStringLiteral("sourceManifestDigest"), QString(64, '0')
        );
        QCOMPARE(inspect(bytes, migrationManifest_, sourceManifestV2_,
                         catalogV2_, badV2Actions).disposition(),
                 DesiredMigrationDisposition::UnqualifiedEvidence);

        QCOMPARE(inspect(QByteArray("{"), badMigration, badSourceManifest,
                         badV2Catalog, ActionCatalog{}).disposition(),
                 DesiredMigrationDisposition::InvalidV1);
        const QByteArray oversized(maximumDesiredStateBytes + 1, 'x');
        const auto oversizedPlan = inspect(oversized);
        QCOMPARE(oversizedPlan.disposition(),
                 DesiredMigrationDisposition::InvalidV1);
        QVERIFY(oversizedPlan.sourceBytes().isEmpty());
        QVERIFY(oversizedPlan.sourceBytesSha256().isEmpty());
    }

    void allFinitePredecessorDigestCrossProductsAreEligible()
    {
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
                const auto bytes = source(
                    QStringLiteral("0.56.1"), 9, catalog, action, false
                );
                const auto original = bytes;
                const auto plan = inspect(bytes);
                QCOMPARE(plan.disposition(),
                         DesiredMigrationDisposition::Eligible);
                QCOMPARE(plan.route(), DesiredMigrationRoute::M12);
                QCOMPARE(plan.sourceBytes(), original);
                QCOMPARE(bytes, original);
                const auto result = materialize(plan);
                QVERIFY(result);
                QCOMPARE(result->state.semanticState.workspaceRules.size(), 0);
                QCOMPARE(bytes, original);
            }
        }
    }

    void exactTargetsDispatchOnlyThroughFrozenRoutes()
    {
        const std::array rows{
            std::pair{QStringLiteral("0.56.0"),
                      DesiredMigrationRoute::M01ThenM12},
            std::pair{QStringLiteral("0.56.1"),
                      DesiredMigrationRoute::M12},
            std::pair{QStringLiteral("0.56.2"),
                      DesiredMigrationRoute::M22},
        };
        for (const auto &[target, route] : rows) {
            const auto bytes = source(target, 42);
            const auto plan = inspect(bytes);
            QCOMPARE(plan.disposition(),
                     DesiredMigrationDisposition::Eligible);
            QCOMPARE(plan.route(), route);
            const auto result = materialize(plan);
            QVERIFY(result);
            QCOMPARE(result->route, route);
            QCOMPARE(result->state.semanticState.targetHyprland,
                     QStringLiteral("0.56.2"));
            QCOMPARE(result->state.semanticState.compatibility,
                     CompatibilityDecision::Exact);
            QCOMPARE(result->state.semanticState.revision, quint64(42));
        }
    }

    void fullyValidFutureTargetsRetainBytesWithoutDestinationConsultation()
    {
        for (const auto &target : {
                 QStringLiteral("0.56.3"),
                 QStringLiteral("0.56.4294967295"),
             }) {
            const auto bytes = source(target, 7);
            const auto plan = inspect(
                bytes, QByteArrayView{}, QByteArrayView{}, Catalog{},
                ActionCatalog{}
            );
            QCOMPARE(plan.disposition(),
                     DesiredMigrationDisposition::UnsupportedNewerPatch);
            QCOMPARE(plan.route(), DesiredMigrationRoute::None);
            QCOMPARE(plan.sourceBytes(), bytes);
            QCOMPARE(plan.sourceBytesSha256(), sha256(bytes));
            QVERIFY(!materialize(plan));
        }
    }

    void malformedAndIncompatibleTargetSpellingsAreInvalid()
    {
        for (const auto &target : {
                 QStringLiteral("0.56"), QStringLiteral("0.56.00"),
                 QStringLiteral("0.56.01"), QStringLiteral("0.56.+1"),
                 QStringLiteral("0.56.-1"), QStringLiteral(" 0.56.1"),
                 QStringLiteral("0.56.1 "), QStringLiteral("0.56. 1"),
                 QStringLiteral("0.56.4294967296"),
                 QStringLiteral("1.56.1"), QStringLiteral("0.55.1"),
                 QStringLiteral("0.57.1"), QStringLiteral("v0.56.1"),
             }) {
            const auto plan = inspect(source(target));
            QCOMPARE(plan.disposition(),
                     DesiredMigrationDisposition::InvalidV1);
            QCOMPARE(plan.route(), DesiredMigrationRoute::None);
            QVERIFY(!materialize(plan));
        }

        auto root = objectFromBytes(source());
        for (const auto &revision : {
                 QStringLiteral("00"),
                 QStringLiteral("18446744073709551616"),
             }) {
            root.insert(QStringLiteral("revision"), revision);
            const auto plan = inspect(canonicalBytes(root));
            QCOMPARE(plan.disposition(),
                     DesiredMigrationDisposition::InvalidV1);
            QVERIFY(!materialize(plan));
        }
    }

    void canonicalStoredV1BytesAreRequiredExactly()
    {
        const auto bytes = source();
        std::array<QByteArray, 7> variants{
            bytes.first(bytes.size() - 1),
            bytes + QByteArray("\n"),
            bytes + QByteArray(" "),
            QByteArray(" ") + bytes,
            QByteArray("\xEF\xBB\xBF") + bytes,
            QJsonDocument(objectFromBytes(bytes)).toJson(
                QJsonDocument::Indented
            ),
            bytes,
        };
        variants.back().insert(1, "\"revision\":\"1\",");
        for (const auto &variant : variants) {
            QCOMPARE(inspect(variant).disposition(),
                     DesiredMigrationDisposition::InvalidV1);
        }
    }

    void snapshotDigestDomainsAreVersionDispatchedExactly()
    {
        const auto input = source();
        QCOMPARE(input.size(), qsizetype(614));
        QCOMPARE(
            sha256(input),
            QStringLiteral(
                "28f154a94be1aa09760689184367cc81f1f5da28756c66af4770e5d78bc17b41"
            )
        );
        QCOMPARE(
            sha256(input.first(input.size() - 1)),
            QStringLiteral(
                "1b1a29e72e33f1d4aca6651c398987123cac3263bbc7a8169529ff45610bc712"
            )
        );

        const auto result = materialize(inspect(input));
        QVERIFY(result);
        QCOMPARE(
            result->sourceV1StoredBytesSha256,
            QStringLiteral(
                "28f154a94be1aa09760689184367cc81f1f5da28756c66af4770e5d78bc17b41"
            )
        );
        QCOMPARE(result->bytes.size(), qsizetype(663));
        QCOMPARE(
            result->destinationV2StoredBytesSha256,
            QStringLiteral(
                "335a5d5f002fcce75ff406696b64aa40579e989e0f0d55d322369b69a0bdd2a2"
            )
        );
        QCOMPARE(
            result->destinationV2SnapshotDigest,
            QStringLiteral(
                "d0ba9d6640ee281789416bbed615ec64dee418175a09dab58393feb542db20cd"
            )
        );
        QCOMPARE(
            result->destinationV2SnapshotDigest,
            sha256(result->bytes.first(result->bytes.size() - 1))
        );
        QVERIFY(result->sourceV1StoredBytesSha256
                != result->destinationV2SnapshotDigest);
        QVERIFY(result->destinationV2StoredBytesSha256
                != result->destinationV2SnapshotDigest);

        auto oneBitWrong = result->sourceV1StoredBytesSha256;
        oneBitWrong[0] = oneBitWrong[0] == QLatin1Char('0')
            ? QLatin1Char('1')
            : QLatin1Char('0');
        QVERIFY(oneBitWrong != result->sourceV1StoredBytesSha256);
        QVERIFY(
            sha256(input.first(input.size() - 1))
            != result->sourceV1StoredBytesSha256
        );
    }

    void unknownLineageAndPreSharedProtectedRecordsAreInvalidV1()
    {
        QCOMPARE(inspect(source(
                     QStringLiteral("0.56.1"), 0, QString(64, 'a')
                 )).disposition(),
                 DesiredMigrationDisposition::InvalidV1);
        QCOMPARE(inspect(source(
                     QStringLiteral("0.56.1"), 0,
                     QLatin1String(reviewedCatalogDigest), QString(64, 'b')
                 )).disposition(),
                 DesiredMigrationDisposition::InvalidV1);

        QCOMPARE(inspect(source(
                     QStringLiteral("0.56.1"), 0,
                     QLatin1String(reviewedCatalogDigest), oldAction, true
                 )).disposition(),
                 DesiredMigrationDisposition::InvalidV1);

        auto root = objectFromBytes(source(
            QStringLiteral("0.56.1"), 0,
            QLatin1String(reviewedCatalogDigest), oldAction, false
        ));
        root.insert(
            QStringLiteral("workspaceRules"),
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
            }}
        );
        QCOMPARE(inspect(canonicalBytes(root)).disposition(),
                 DesiredMigrationDisposition::InvalidV1);

        root = objectFromBytes(source(
            QStringLiteral("0.56.1"), 0,
            QLatin1String(reviewedCatalogDigest), oldAction, false
        ));
        root.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"),
                 QLatin1String(sharedSpacingWorkspaceRuleId)},
                {QStringLiteral("selector"), QStringLiteral("1")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("monitor"), QString()},
                {QStringLiteral("persistent"), false},
                {QStringLiteral("isDefault"), false},
                {QStringLiteral("layout"), QString()},
                {QStringLiteral("overrides"), QJsonObject{}},
            }}
        );
        QCOMPARE(inspect(canonicalBytes(root)).disposition(),
                 DesiredMigrationDisposition::InvalidV1);
    }

    void materializeAcceptsOnlyCanonicalCallerSuppliedAuthorityId()
    {
        const auto plan = inspect(source());
        const auto first = materialize(plan);
        QVERIFY(first);
        const QString secondAuthority =
            QStringLiteral("fedcba9876543210fedcba9876543210");
        const auto second = materialize(plan, secondAuthority);
        QVERIFY(second);
        QCOMPARE(first->state.semanticState, second->state.semanticState);
        QCOMPARE(first->state.authorityId, authorityId);
        QCOMPARE(second->state.authorityId, secondAuthority);
        QVERIFY(first->bytes != second->bytes);
        QVERIFY(first->destinationV2StoredBytesSha256
                != second->destinationV2StoredBytesSha256);
        QVERIFY(first->destinationV2SnapshotDigest
                != second->destinationV2SnapshotDigest);
        auto firstObject = objectFromBytes(first->bytes);
        auto secondObject = objectFromBytes(second->bytes);
        firstObject.remove(QStringLiteral("authorityId"));
        secondObject.remove(QStringLiteral("authorityId"));
        QCOMPARE(firstObject, secondObject);
        const auto firstParsed = parseDormantDesiredStateV2(
            first->bytes, catalogV2_, actionsV2_
        );
        const auto secondParsed = parseDormantDesiredStateV2(
            second->bytes, catalogV2_, actionsV2_
        );
        QVERIFY(firstParsed);
        QVERIFY(secondParsed);
        QCOMPARE(*firstParsed.value, first->state);
        QCOMPARE(*secondParsed.value, second->state);
        for (const auto &invalid : {
                 QString(), QString(32, '0'), QString(31, 'a'),
                 QString(33, 'a'), QString(32, 'A'),
                 QStringLiteral("g123456789abcdef0123456789abcdef"),
                 QStringLiteral(" 123456789abcdef0123456789abcdef"),
             }) {
            QVERIFY(!materialize(plan, invalid));
        }
    }

    void revisionBoundariesAndDeterministicValuesArePreservedExactly()
    {
        const std::array<quint64, 8> revisions{
            0,
            1,
            std::numeric_limits<quint64>::max(),
            UINT64_C(0x0123456789abcdef),
            UINT64_C(0xfedcba9876543210),
            UINT64_C(1234567890123456789),
            UINT64_C(9223372036854775807),
            UINT64_C(9223372036854775808),
        };
        for (const auto revision : revisions) {
            const auto input = source(QStringLiteral("0.56.2"), revision);
            const auto plan = inspect(input);
            const auto result = materialize(plan);
            QVERIFY(result);
            QCOMPARE(result->state.semanticState.revision, revision);
            QCOMPARE(
                objectFromBytes(result->bytes)
                    .value(QStringLiteral("revision")).toString(),
                QString::number(revision)
            );
            QCOMPARE(result->sourceV1StoredBytesSha256, sha256(input));
            QCOMPARE(
                result->destinationV2StoredBytesSha256,
                sha256(result->bytes)
            );
            QVERIFY(input.endsWith('\n'));
            QVERIFY(result->bytes.endsWith('\n'));
            QVERIFY(result->sourceV1StoredBytesSha256
                    != sha256(input.first(input.size() - 1)));
            QCOMPARE(
                result->destinationV2SnapshotDigest,
                sha256(result->bytes.first(result->bytes.size() - 1))
            );
            QVERIFY(result->destinationV2StoredBytesSha256
                    != result->destinationV2SnapshotDigest);
        }
    }

    void representativeSemanticsAndAllCollectionOrderAreExact()
    {
        const auto input = representativeSource();
        const auto parsedSource = parseDesiredState(
            input, catalogV1_, actionsV1_
        );
        QVERIFY2(parsedSource,
                 qPrintable(parsedSource.errors.isEmpty()
                                ? QStringLiteral("no parser product")
                                : parsedSource.errors.constFirst().path
                                    + QLatin1Char(':')
                                    + parsedSource.errors.constFirst().code));
        QCOMPARE(parsedSource.value->monitors.size(), 1);
        QCOMPARE(parsedSource.value->devices.size(), 1);
        QCOMPARE(parsedSource.value->curves.size(), 2);
        QCOMPARE(parsedSource.value->animations.size(), 1);
        QCOMPARE(parsedSource.value->gestures.size(), 1);
        QCOMPARE(parsedSource.value->workspaceRules.size(), 3);
        QCOMPARE(parsedSource.value->windowRules.size(), 1);
        QCOMPARE(parsedSource.value->layerRules.size(), 1);
        QCOMPARE(parsedSource.value->submaps.size(), 1);
        QCOMPARE(parsedSource.value->bindings.size(), 1);
        QCOMPARE(parsedSource.value->permissions.size(), 1);
        QCOMPARE(parsedSource.value->environment.size(), 2);

        const auto original = input;
        const auto plan = inspect(input);
        QCOMPARE(plan.disposition(), DesiredMigrationDisposition::Eligible);
        QCOMPARE(plan.route(), DesiredMigrationRoute::M01ThenM12);
        const auto result = materialize(plan);
        QVERIFY(result);
        QCOMPARE(input, original);

        auto expected = *parsedSource.value;
        expected.targetHyprland = QStringLiteral("0.56.2");
        expected.catalogDigest =
            QLatin1String(dormantReviewedCatalogV2Digest);
        expected.actionCatalogDigest =
            QLatin1String(dormantReviewedActionCatalogV2Digest);
        expected.compatibility = CompatibilityDecision::Exact;
        expected.readOnly = false;
        expected.opaqueFutureDocument.reset();
        QCOMPARE(result->state.semanticState, expected);
        QCOMPARE(result->state.authorityId, authorityId);
        QVERIFY(!result->bytes.contains("float_gaps"));
        QCOMPARE(
            result->state.semanticState.workspaceRules.at(0).id,
            QStringLiteral("workspace-one")
        );
        QCOMPARE(
            result->state.semanticState.workspaceRules.at(1).id,
            QStringLiteral("workspace-two")
        );
        QCOMPARE(
            result->state.semanticState.workspaceRules.at(2).id,
            QLatin1String(sharedSpacingWorkspaceRuleId)
        );
        QVERIFY(!result->state.semanticState.devices.constFirst().enabled);
        QVERIFY(result->state.semanticState.environment.constFirst()
                    .value.isEmpty());
    }

    void outputIsAuthoritativeV2AndPlanIsDefensivelyRechecked()
    {
        const auto input = source(QStringLiteral("0.56.2"), 6);
        const auto plan = inspect(input);
        const auto result = materialize(plan);
        QVERIFY(result);
        const auto parsed = parseDormantDesiredStateV2(
            result->bytes, catalogV2_, actionsV2_
        );
        QVERIFY(parsed);
        QCOMPARE(*parsed.value, result->state);
        const auto serialized = serializeDormantDesiredStateV2(*parsed.value);
        QVERIFY(serialized);
        QCOMPARE(*serialized.value, result->bytes);

        auto badManifest = migrationManifest_;
        badManifest[0] = badManifest[0] == 'x' ? 'y' : 'x';
        QVERIFY(!materializeDesiredV1ToV2Migration(
            plan, authorityId, badManifest, sourceManifestV2_, catalogV1_,
            actionsV1_, catalogV2_, actionsV2_
        ));
        auto badV1 = catalogV1_;
        badV1.canonicalDocument.insert(
            QStringLiteral("contractVersion"), 2
        );
        QVERIFY(!materializeDesiredV1ToV2Migration(
            plan, authorityId, migrationManifest_, sourceManifestV2_, badV1,
            actionsV1_, catalogV2_, actionsV2_
        ));
    }
};

QTEST_MAIN(CompositorDesiredMigrationReducerTest)

#include "compositor_desired_migration_reducer_test.moc"
