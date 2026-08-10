#include "compositor_option_catalog.h"
#include "compositor_snapshot_editor.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <limits>

namespace {

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

[[nodiscard]] QJsonObject readObject(const QString &path)
{
    return QJsonDocument::fromJson(readBytes(path)).object();
}

[[nodiscard]] HyprShelld::CompositorOptionCatalog trustedCatalog()
{
    const auto parsed = HyprShelld::Hyprland::parseCatalog(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
    );
    if (!parsed) return {};
    const auto bytes = HyprShelld::Hyprland::canonicalCatalogJson(
        *parsed.value
    );
    QString error;
    const auto catalog = HyprShelld::CompositorOptionCatalog::fromBytes(
        bytes,
        parsed.value->digest,
        parsed.value->digest,
        error
    );
    return catalog ? *catalog : HyprShelld::CompositorOptionCatalog{};
}

[[nodiscard]] QJsonObject baselineSnapshot()
{
    auto object = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE)
    );
    object.insert(QStringLiteral("revision"), QStringLiteral("7"));
    object.insert(
        QStringLiteral("environment"),
        QJsonArray{QJsonObject{
            {QStringLiteral("id"), QStringLiteral("environment-one")},
            {QStringLiteral("name"), QStringLiteral("EXAMPLE")},
            {QStringLiteral("value"), QStringLiteral("preserved")},
            {QStringLiteral("scope"), QStringLiteral("hyprland")},
        }}
    );
    object.insert(
        QStringLiteral("overrides"),
        QJsonObject{
            {QStringLiteral("hyprland.misc.disable_hyprland_logo"), true},
            {QStringLiteral("hyprland.animations.enabled"), false},
        }
    );
    return object;
}

} // namespace

class CompositorAppearanceHelpersTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsOnlyTheExactCanonicalReviewedCatalog()
    {
        const auto parsed = HyprShelld::Hyprland::parseCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
        );
        QVERIFY(parsed);
        const auto canonical = HyprShelld::Hyprland::canonicalCatalogJson(
            *parsed.value
        );
        QString error;
        const auto accepted =
            HyprShelld::CompositorOptionCatalog::fromBytes(
                canonical,
                parsed.value->digest,
                parsed.value->digest,
                error
            );
        QVERIFY2(accepted, qPrintable(error));
        QCOMPARE(accepted->appearanceOptions().size(), 8);
        QCOMPARE(
            accepted->appearanceOptionIds(),
            QStringList({
                QStringLiteral("hyprland.general.border_size"),
                QStringLiteral("hyprland.decoration.rounding"),
                QStringLiteral("hyprland.decoration.blur.enabled"),
                QStringLiteral("hyprland.decoration.shadow.enabled"),
                QStringLiteral("hyprland.animations.enabled"),
                QStringLiteral("hyprland.general.layout"),
                QStringLiteral("hyprland.general.resize_on_border"),
                QStringLiteral("hyprland.general.snap.enabled"),
            })
        );
        const auto border = accepted->appearanceOptions().constFirst().toMap();
        QCOMPARE(border.value(QStringLiteral("type")).toString(),
                 QStringLiteral("integer"));
        QCOMPARE(border.value(QStringLiteral("control")).toString(),
                 QStringLiteral("spinBox"));
        QCOMPARE(border.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(border.value(QStringLiteral("max")).toInt(), 20);

        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE)),
            parsed.value->digest,
            parsed.value->digest,
            error
        ));
        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            canonical,
            QString(64, QLatin1Char('a')),
            parsed.value->digest,
            error
        ));
        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            canonical,
            parsed.value->digest,
            QString(64, QLatin1Char('a')),
            error
        ));
        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            QByteArray(HyprShelld::Hyprland::maximumCatalogBytes + 1, 'x'),
            parsed.value->digest,
            parsed.value->digest,
            error
        ));
    }

    void editsOnlyTheEightCuratedOverridesAndPreservesEveryOtherSurface()
    {
        const auto catalog = trustedCatalog();
        QCOMPARE(catalog.appearanceOptions().size(), 8);
        const auto snapshot = baselineSnapshot();
        QString error;
        auto values = catalog.appearanceValues(snapshot, error);
        QVERIFY2(values, qPrintable(error));
        values->insert(QStringLiteral("hyprland.general.border_size"), 6);
        values->insert(QStringLiteral("hyprland.decoration.rounding"), 12);
        values->insert(
            QStringLiteral("hyprland.decoration.blur.enabled"), false
        );
        values->insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("master")
        );
        values->insert(QStringLiteral("hyprland.animations.enabled"), true);

        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot,
            7,
            snapshot.value(QStringLiteral("catalogDigest")).toString(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog,
            *values,
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->candidate.endsWith('\n'));
        QVERIFY(edit->candidate.size()
                <= HyprShelld::Hyprland::maximumDesiredStateBytes);
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        QCOMPARE(candidate.value(QStringLiteral("revision")).toString(),
                 QStringLiteral("7"));
        QCOMPARE(candidate.value(QStringLiteral("environment")),
                 snapshot.value(QStringLiteral("environment")));
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }
        const auto overrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.disable_hyprland_logo")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.border_size")
        ).toInt(), 6);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.rounding")
        ).toInt(), 12);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.enabled")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.layout")
        ).toString(), QStringLiteral("master"));
        QVERIFY(!overrides.contains(
            QStringLiteral("hyprland.animations.enabled")
        ));
    }

    void rejectsPartialUnknownInvalidStaleAndMalformedEdits()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        QString error;
        auto values = catalog.appearanceValues(snapshot, error);
        QVERIFY(values);

        auto invalid = *values;
        invalid.remove(QStringLiteral("hyprland.animations.enabled"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, invalid, error
        ));

        invalid = *values;
        invalid.insert(QStringLiteral("hyprland.unknown"), true);
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, invalid, error
        ));

        invalid = *values;
        invalid.insert(QStringLiteral("hyprland.general.border_size"), 21);
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, invalid, error
        ));

        invalid = *values;
        invalid.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("plugin-layout")
        );
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, invalid, error
        ));

        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 6, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, *values, error
        ));
        snapshot.insert(QStringLiteral("revision"), QStringLiteral("07"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, *values, error
        ));
        snapshot.insert(QStringLiteral("revision"), QString::number(
            std::numeric_limits<qulonglong>::max()
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, std::numeric_limits<qulonglong>::max(), catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, *values, error
        ));
        snapshot.insert(QStringLiteral("revision"), QStringLiteral("7"));
        snapshot.remove(QStringLiteral("devices"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, *values, error
        ));
    }
};

QTEST_GUILESS_MAIN(CompositorAppearanceHelpersTest)

#include "compositor_appearance_helpers_test.moc"
