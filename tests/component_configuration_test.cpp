#include "component_config_test_fixture.h"
#include "componentd/system_catalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include <algorithm>

using namespace HyprShelld;

namespace {

QByteArray defaultsBytes()
{
    return Tests::readBytes(QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE));
}

Components::ConfigurationCatalog catalog()
{
    return Tests::configurationCatalog(
        QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE),
        QStringLiteral(HYPRSHELLD_WORKSPACE_SCHEMA_FILE)
    );
}

QByteArray encode(QJsonObject object)
{
    auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

} // namespace

class ComponentConfigurationTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesProtectedDefaults()
    {
        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(defaultsBytes()),
            catalog()
        );
        QVERIFY2(parsed, qPrintable(parsed.errors.isEmpty()
            ? QString() : parsed.errors.front().message));
        QCOMPARE(parsed.value->revision, quint64(0));
        QCOMPARE(parsed.value->components.size(), 1);
        QCOMPARE(parsed.value->instances.size(), 1);
        QCOMPARE(parsed.value->bars.size(), 1);
        QCOMPARE(
            parsed.value->instances.first().settings,
            Components::workspaceSwitcherDefaultSettings()
        );
        QCOMPARE(
            Components::workspaceSwitcherDefaultSettings(),
            parsed.value->instances.first().settings
        );
        const auto canonical = Components::serializeComponentConfiguration(
            *parsed.value
        );
        const auto reparsed = Components::parseComponentConfiguration(
            QByteArrayView(canonical), catalog()
        );
        QVERIFY(reparsed);
        QCOMPARE(*reparsed.value, *parsed.value);
    }

    void protectedDefaultDigestMatchesSystemCatalog()
    {
        const auto system = Components::SystemCatalog::load(
            QStringLiteral(HYPRSHELLD_SOURCE_COMPONENT_DIR)
        );
        QVERIFY2(system.ok(), qPrintable(system.error));
        const auto *entry = system.catalog->find(QString::fromLatin1(
            Components::workspaceSwitcherId
        ));
        QVERIFY(entry != nullptr);
        const auto desired = QJsonDocument::fromJson(defaultsBytes())
                                 .object()
                                 .value(QStringLiteral("components"))
                                 .toObject()
                                 .value(QString::fromLatin1(
                                     Components::workspaceSwitcherId
                                 ))
                                 .toObject();
        QCOMPARE(
            desired.value(QStringLiteral("packageDigest")).toString(),
            entry->packageDigest
        );
    }

    void normalizesMatchingSchemaDefaults()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto instances = root.value(QStringLiteral("instances")).toObject();
        const auto instanceId = instances.begin().key();
        auto instance = instances.value(instanceId).toObject();
        instance.insert(QStringLiteral("settings"), QJsonObject());
        instances.insert(instanceId, instance);
        root.insert(QStringLiteral("instances"), instances);

        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(encode(root)),
            catalog()
        );
        QVERIFY(parsed);
        QCOMPARE(
            parsed.value->instances.first().settings,
            Components::workspaceSwitcherDefaultSettings()
        );
    }

    void rejectsFormatVersion_data()
    {
        QTest::addColumn<QJsonValue>("version");
        QTest::addColumn<QString>("code");
        QTest::newRow("missing") << QJsonValue(QJsonValue::Undefined)
                                  << QStringLiteral("component-config.invalid-format");
        QTest::newRow("string") << QJsonValue(QStringLiteral("1"))
                                 << QStringLiteral("component-config.invalid-format");
        QTest::newRow("zero") << QJsonValue(0)
                               << QStringLiteral("component-config.invalid-format");
        QTest::newRow("fractional") << QJsonValue(1.5)
                                     << QStringLiteral("component-config.invalid-format");
        QTest::newRow("future") << QJsonValue(2)
                                 << QStringLiteral("component-config.unsupported-format");
    }

    void rejectsFormatVersion()
    {
        QFETCH(QJsonValue, version);
        QFETCH(QString, code);
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        if (version.isUndefined()) {
            root.remove(QStringLiteral("formatVersion"));
        } else {
            root.insert(QStringLiteral("formatVersion"), version);
        }
        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(encode(root)),
            catalog()
        );
        QVERIFY(!parsed);
        QVERIFY(std::ranges::any_of(parsed.errors, [&code](const auto &error) {
            return error.code == code;
        }));
    }

    void rejectsDuplicateJsonKeys()
    {
        auto bytes = defaultsBytes();
        bytes.replace(
            QByteArrayLiteral("\"formatVersion\": 1,"),
            QByteArrayLiteral("\"formatVersion\":1,\"formatVersion\":1,")
        );
        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(bytes), catalog()
        );
        QVERIFY(!parsed);
        QCOMPARE(parsed.errors.front().code, QStringLiteral("json.duplicate-key"));
    }

    void rejectsInvalidRevisions_data()
    {
        QTest::addColumn<QJsonValue>("revision");
        QTest::newRow("leading-zero") << QJsonValue(QStringLiteral("01"));
        QTest::newRow("overflow")
            << QJsonValue(QStringLiteral("18446744073709551616"));
        QTest::newRow("fractional") << QJsonValue(1.5);
        QTest::newRow("number") << QJsonValue(1);
    }

    void rejectsInvalidRevisions()
    {
        QFETCH(QJsonValue, revision);
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        root.insert(QStringLiteral("revision"), revision);
        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        );
        QVERIFY(!parsed);
        QVERIFY(std::ranges::any_of(parsed.errors, [](const auto &error) {
            return error.code == QStringLiteral("component-config.invalid-revision");
        }));
    }

    void rejectsInvalidInstanceIds_data()
    {
        QTest::addColumn<QString>("instanceId");
        QTest::newRow("uppercase")
            << QStringLiteral("7B4E2329-4320-4E15-894D-218FA690D782");
        QTest::newRow("wrong-version")
            << QStringLiteral("7b4e2329-4320-1e15-894d-218fa690d782");
        QTest::newRow("wrong-variant")
            << QStringLiteral("7b4e2329-4320-4e15-094d-218fa690d782");
    }

    void rejectsInvalidInstanceIds()
    {
        QFETCH(QString, instanceId);
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto instances = root.value(QStringLiteral("instances")).toObject();
        const auto instance = instances.begin().value();
        instances = QJsonObject{{instanceId, instance}};
        root.insert(QStringLiteral("instances"), instances);
        auto layouts = root.value(QStringLiteral("layouts")).toObject();
        auto bars = layouts.value(QStringLiteral("bars")).toObject();
        auto layout = bars.value(QStringLiteral("main")).toObject();
        auto regions = layout.value(QStringLiteral("regions")).toObject();
        regions.insert(QStringLiteral("start"), QJsonArray{instanceId});
        layout.insert(QStringLiteral("regions"), regions);
        bars.insert(QStringLiteral("main"), layout);
        layouts.insert(QStringLiteral("bars"), bars);
        root.insert(QStringLiteral("layouts"), layouts);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));
    }

    void validatesNamedOutputSelectors()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto layouts = root.value(QStringLiteral("layouts")).toObject();
        auto bars = layouts.value(QStringLiteral("bars")).toObject();
        auto layout = bars.value(QStringLiteral("main")).toObject();
        layout.insert(
            QStringLiteral("outputs"),
            QJsonObject{
                {QStringLiteral("mode"), QStringLiteral("named")},
                {QStringLiteral("names"), QJsonArray{
                    QStringLiteral("DP-1"), QStringLiteral("DP-2")}},
            }
        );
        bars.insert(QStringLiteral("main"), layout);
        layouts.insert(QStringLiteral("bars"), bars);
        root.insert(QStringLiteral("layouts"), layouts);
        QVERIFY(Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));

        auto outputs = layout.value(QStringLiteral("outputs")).toObject();
        outputs.insert(
            QStringLiteral("names"),
            QJsonArray{QStringLiteral("DP-2"), QStringLiteral("DP-1")}
        );
        layout.insert(QStringLiteral("outputs"), outputs);
        bars.insert(QStringLiteral("main"), layout);
        layouts.insert(QStringLiteral("bars"), bars);
        root.insert(QStringLiteral("layouts"), layouts);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));
    }

    void rejectsUnknownFieldsAndWrongSettingScopes()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto components = root.value(QStringLiteral("components")).toObject();
        const auto componentId = components.begin().key();
        auto desired = components.value(componentId).toObject();
        desired.insert(QStringLiteral("callerSchema"), QJsonObject());
        desired.insert(
            QStringLiteral("settings"),
            QJsonObject{{QStringLiteral("labelMode"), QStringLiteral("numbers")}}
        );
        components.insert(componentId, desired);
        root.insert(QStringLiteral("components"), components);
        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        );
        QVERIFY(!parsed);
        QVERIFY(parsed.errors.size() >= 2);
    }

    void boundsDormantSettingsAndRecordCounts()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto components = root.value(QStringLiteral("components")).toObject();
        const auto componentId = components.begin().key();
        auto desired = components.value(componentId).toObject();
        desired.insert(QStringLiteral("packageDigest"), QString(64, QLatin1Char('b')));
        desired.insert(
            QStringLiteral("settings"),
            QJsonObject{{QStringLiteral("futureValue"), QString(4097, QLatin1Char('x'))}}
        );
        components.insert(componentId, desired);
        root.insert(QStringLiteral("components"), components);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));

        QJsonObject tooMany;
        for (int index = 0; index < 513; ++index) {
            tooMany.insert(
                QStringLiteral("org.example.component%1").arg(index),
                QJsonObject()
            );
        }
        root.insert(QStringLiteral("components"), tooMany);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));
    }

    void boundsDocumentSize()
    {
        const QByteArray oversized(
            Components::maximumComponentConfigurationBytes + 1,
            ' '
        );
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(oversized), catalog()
        ));
    }

    void boundsDocumentDepth()
    {
        QByteArray deeplyNested;
        for (int depth = 0; depth < 40; ++depth) {
            deeplyNested.append("{\"value\":");
        }
        deeplyNested.append("{}");
        deeplyNested.append(QByteArray(40, '}'));
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(deeplyNested), catalog()
        ));
    }

    void boundsInstanceCount()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        const auto existingInstances = root.value(
            QStringLiteral("instances")
        ).toObject();
        const auto prototype = existingInstances.begin().value();
        QJsonObject tooManyInstances;
        for (int index = 0; index < 1025; ++index) {
            tooManyInstances.insert(
                QStringLiteral("00000000-0000-4000-8000-%1")
                    .arg(index, 12, 16, QLatin1Char('0')),
                prototype
            );
        }
        root.insert(QStringLiteral("instances"), tooManyInstances);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));
    }

    void boundsLayoutCount()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto layouts = root.value(QStringLiteral("layouts")).toObject();
        const auto existingLayouts = layouts.value(
            QStringLiteral("bars")
        ).toObject();
        const auto prototypeLayout = existingLayouts.begin().value();
        QJsonObject tooManyLayouts;
        for (int index = 0; index < 33; ++index) {
            tooManyLayouts.insert(
                QStringLiteral("layout-%1").arg(index), prototypeLayout
            );
        }
        layouts.insert(QStringLiteral("bars"), tooManyLayouts);
        root.insert(QStringLiteral("layouts"), layouts);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));
    }

    void rejectsMalformedDormantData()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto components = root.value(QStringLiteral("components")).toObject();
        const auto id = components.begin().key();
        auto desired = components.value(id).toObject();
        desired.insert(QStringLiteral("packageDigest"), QString(64, QLatin1Char('b')));
        desired.insert(
            QStringLiteral("grantedCapabilities"),
            QJsonArray{QStringLiteral("invalid-")}
        );
        desired.insert(
            QStringLiteral("settings"),
            QJsonObject{{QStringLiteral("InvalidKey"), QStringLiteral("value")}}
        );
        components.insert(id, desired);
        root.insert(QStringLiteral("components"), components);
        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        );
        QVERIFY(!parsed);
        QVERIFY(parsed.errors.size() >= 2);
    }

    void retainsWellFormedDigestMismatchAsInert()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto components = root.value(QStringLiteral("components")).toObject();
        const auto id = components.begin().key();
        auto desired = components.value(id).toObject();
        desired.insert(QStringLiteral("packageDigest"), QString(64, QLatin1Char('b')));
        desired.insert(
            QStringLiteral("settings"),
            QJsonObject{{QStringLiteral("futureValue"), true}}
        );
        components.insert(id, desired);
        root.insert(QStringLiteral("components"), components);
        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        );
        QVERIFY(parsed);
        QCOMPARE(parsed.value->components.first().settings.size(), 1);
    }

    void preservesBoundedDormantFractionalNumbers()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto components = root.value(QStringLiteral("components")).toObject();
        const auto id = components.begin().key();
        auto desired = components.value(id).toObject();
        desired.insert(
            QStringLiteral("packageDigest"),
            QString(64, QLatin1Char('b'))
        );
        desired.insert(
            QStringLiteral("settings"),
            QJsonObject{{QStringLiteral("futureValue"), 1.25}}
        );
        components.insert(id, desired);
        root.insert(QStringLiteral("components"), components);

        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        );
        QVERIFY(parsed);
        const auto canonical = Components::serializeComponentConfiguration(
            *parsed.value
        );
        const auto reparsed = Components::parseComponentConfiguration(
            QByteArrayView(canonical), catalog()
        );
        QVERIFY(reparsed);
        QCOMPARE(*reparsed.value, *parsed.value);
        QCOMPARE(
            reparsed.value->components.first().settings.value(
                QStringLiteral("futureValue")
            ).toDouble(),
            1.25
        );
    }

    void rejectsDormantNumbersThatCannotRoundTripSafely_data()
    {
        QTest::addColumn<QByteArray>("number");
        QTest::addColumn<QString>("code");
        QTest::newRow("above-safe-range")
            << QByteArrayLiteral("9007199254740993")
            << QStringLiteral("json.lossy-number");
        QTest::newRow("exact-but-outside-safe-range")
            << QByteArrayLiteral("9007199254740992")
            << QStringLiteral("component-config.invalid-setting-number");
        QTest::newRow("in-range-halfway-rounding")
            << QByteArrayLiteral("9007199254740990.5")
            << QStringLiteral("json.lossy-number");
        QTest::newRow("positive-underflow")
            << QByteArrayLiteral("1e-400")
            << QStringLiteral("json.invalid-number");
        QTest::newRow("negative-underflow")
            << QByteArrayLiteral("-1e-400")
            << QStringLiteral("json.invalid-number");
    }

    void rejectsDormantNumbersThatCannotRoundTripSafely()
    {
        QFETCH(QByteArray, number);
        QFETCH(QString, code);

        auto bytes = defaultsBytes();
        const auto oldSettings = QByteArrayLiteral(
            "\"settings\": {\n        \"labelMode\": \"numbers\","
        );
        const auto newSettings = QByteArrayLiteral(
            "\"settings\": {\n        \"futureValue\": "
        ) + number + QByteArrayLiteral(",\n        \"labelMode\": \"numbers\",");
        QVERIFY(bytes.contains(oldSettings));
        bytes.replace(oldSettings, newSettings);
        bytes.replace(
            QByteArrayLiteral(
                "\"packageDigest\": \""
            ) + catalog().entries.first().packageDigest.toLatin1()
                + QByteArrayLiteral("\""),
            QByteArrayLiteral(
                "\"packageDigest\": \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\""
            )
        );

        const auto parsed = Components::parseComponentConfiguration(
            QByteArrayView(bytes), catalog()
        );
        QVERIFY(!parsed);
        QVERIFY(std::ranges::any_of(
            parsed.errors,
            [&code](const auto &error) { return error.code == code; }
        ));
    }

    void rejectsDuplicateAndMissingPlacements()
    {
        auto root = QJsonDocument::fromJson(defaultsBytes()).object();
        auto layouts = root.value(QStringLiteral("layouts")).toObject();
        auto bars = layouts.value(QStringLiteral("bars")).toObject();
        auto layout = bars.value(QStringLiteral("main")).toObject();
        auto regions = layout.value(QStringLiteral("regions")).toObject();
        const auto id = regions.value(QStringLiteral("start")).toArray().first();
        regions.insert(QStringLiteral("center"), QJsonArray{id});
        layout.insert(QStringLiteral("regions"), regions);
        bars.insert(QStringLiteral("main"), layout);
        layouts.insert(QStringLiteral("bars"), bars);
        root.insert(QStringLiteral("layouts"), layouts);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));

        regions.insert(QStringLiteral("start"), QJsonArray());
        regions.insert(QStringLiteral("center"), QJsonArray());
        layout.insert(QStringLiteral("regions"), regions);
        bars.insert(QStringLiteral("main"), layout);
        layouts.insert(QStringLiteral("bars"), bars);
        root.insert(QStringLiteral("layouts"), layouts);
        QVERIFY(!Components::parseComponentConfiguration(
            QByteArrayView(encode(root)), catalog()
        ));
    }
};

QTEST_GUILESS_MAIN(ComponentConfigurationTest)
#include "component_configuration_test.moc"
