#include "component/settings_schema.h"

#include <QFile>
#include <QtTest>

#include <algorithm>

using namespace HyprShelld::Components;

namespace {

[[nodiscard]] QByteArray workspaceSettingsSchema()
{
    const auto path = QFINDTESTDATA(
        "../data/components/"
        "io.github.coastlinesec.hyprshelld.workspace-switcher/"
        "settings.schema.json"
    );
    if (path.isEmpty()) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] bool hasCode(
    const ValidationErrors &errors,
    const QString &code
)
{
    return std::ranges::any_of(errors, [&code](const ValidationError &error) {
        return error.code == code;
    });
}

} // namespace

class ComponentSettingsSchemaTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesAndNormalizesWorkspaceDefaults()
    {
        const auto parsed = parseSettingsSchema(workspaceSettingsSchema());
        QVERIFY2(parsed.ok(), "The shipped workspace settings schema must validate");
        QCOMPARE(parsed.value->schemaVersion, 1U);
        QCOMPARE(parsed.value->settings.size(), 6);

        const auto *maximum = parsed.value->find(
            QStringLiteral("maximumApplications")
        );
        QVERIFY(maximum != nullptr);
        QVERIFY(maximum->visibleWhen.has_value());
        QCOMPARE(
            maximum->visibleWhen->key,
            QStringLiteral("showApplications")
        );
        QCOMPARE(maximum->visibleWhen->equals, QJsonValue(true));

        const auto normalized = normalizeSettings(
            *parsed.value,
            SettingScope::Instance,
            {}
        );
        QVERIFY(normalized.ok());
        QCOMPARE(normalized.value->size(), 6);
        QCOMPARE(
            normalized.value->value(QStringLiteral("showIdentifiers")).toBool(),
            true
        );
        QCOMPARE(
            normalized.value->value(QStringLiteral("showNames")).toBool(),
            false
        );
        QCOMPARE(
            normalized.value->value(
                QStringLiteral("maximumApplications")
            ).toInt(),
            3
        );
        QCOMPARE(
            normalized.value->value(QStringLiteral("scrollMode")).toString(),
            QStringLiteral("disabled")
        );
    }

    void rejectsUnknownWrongScopeAndOutOfRangeValues()
    {
        const auto parsed = parseSettingsSchema(workspaceSettingsSchema());
        QVERIFY(parsed.ok());

        const auto normalized = normalizeSettings(
            *parsed.value,
            SettingScope::Instance,
            {
                {QStringLiteral("maximumApplications"), 6},
                {QStringLiteral("unknown"), true},
            }
        );
        QVERIFY(!normalized.ok());
        QVERIFY(hasCode(
            normalized.errors,
            QStringLiteral("settings-value.number-out-of-range")
        ));
        QVERIFY(hasCode(
            normalized.errors,
            QStringLiteral("settings-value.unknown-or-wrong-scope")
        ));
    }

    void rejectsDuplicateDefinitionKeysAndForwardConditions()
    {
        const QByteArray duplicates = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"same","scope":"instance","type":"boolean","label":"One","description":"One setting.","group":"general","order":1,"default":false},
            {"key":"same","scope":"instance","type":"boolean","label":"Two","description":"Another setting.","group":"general","order":2,"default":true}
          ]
        })";
        auto result = parseSettingsSchema(duplicates);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("settings-schema.duplicate-key")
        ));

        const QByteArray forward = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"dependent","scope":"instance","type":"boolean","label":"Dependent","description":"Depends on later.","group":"general","order":1,"default":false,"visibleWhen":{"key":"later","equals":true}},
            {"key":"later","scope":"instance","type":"boolean","label":"Later","description":"Declared later.","group":"general","order":2,"default":true}
          ]
        })";
        result = parseSettingsSchema(forward);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("settings-schema.invalid-condition-reference")
        ));
    }

    void rejectsWrongTypeFieldsAndInvalidDefaults()
    {
        const QByteArray wrongField = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"enabled","scope":"component","type":"boolean","label":"Enabled","description":"Whether enabled.","group":"general","order":1,"default":false,"minimum":0}
          ]
        })";
        auto result = parseSettingsSchema(wrongField);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("settings-schema.unknown-field")
        ));

        const QByteArray badDefault = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"count","scope":"instance","type":"integer","label":"Count","description":"Bounded count.","group":"general","order":1,"default":9,"minimum":1,"maximum":5}
          ]
        })";
        result = parseSettingsSchema(badDefault);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("settings-value.number-out-of-range")
        ));
    }

    void rejectsControlsAndFormatCharactersInSchemas()
    {
        const QByteArray c1Label = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"title","scope":"component","type":"string","label":"Tit\u0085le","description":"Visible title.","group":"general","order":1,"default":"Clean","maximumLength":32}
          ]
        })";
        auto parsed = parseSettingsSchema(c1Label);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("settings-schema.invalid-string")
        ));

        const QByteArray bidiLabel = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"title","scope":"component","type":"string","label":"Safe\u202eeman","description":"Visible title.","group":"general","order":1,"default":"Clean","maximumLength":32}
          ]
        })";
        parsed = parseSettingsSchema(bidiLabel);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("settings-schema.invalid-string")
        ));

        const QByteArray joinerOption = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"mode","scope":"component","type":"enum","label":"Mode","description":"Visible mode.","group":"general","order":1,"default":"safe","options":[{"value":"safe","label":"Safe\u200dMode"}]}
          ]
        })";
        parsed = parseSettingsSchema(joinerOption);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("settings-schema.invalid-string")
        ));

        const QByteArray supplementaryFormatLabel = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"title","scope":"component","type":"string","label":"Safe\udb40\udc01Name","description":"Visible title.","group":"general","order":1,"default":"Clean","maximumLength":32}
          ]
        })";
        parsed = parseSettingsSchema(supplementaryFormatLabel);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("settings-schema.invalid-string")
        ));

        const QByteArray cleanSchema = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"title","scope":"component","type":"string","label":"Title","description":"Visible title.","group":"general","order":1,"default":"Clean","maximumLength":32}
          ]
        })";
        parsed = parseSettingsSchema(cleanSchema);
        QVERIFY(parsed.ok());
        const auto normalized = normalizeSettings(
            *parsed.value,
            SettingScope::Component,
            {{QStringLiteral("title"), QString(QChar(0x009b))}}
        );
        QVERIFY(!normalized.ok());
        QVERIFY(hasCode(
            normalized.errors,
            QStringLiteral("settings-value.invalid-string")
        ));
    }

    void enforcesExactJsonSafeIntegerRange()
    {
        const QByteArray safeSchema = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"count","scope":"component","type":"integer","label":"Count","description":"Exact bounded count.","group":"general","order":1,"default":-9007199254740991,"minimum":-9007199254740991,"maximum":9007199254740991,"step":1}
          ]
        })";
        auto parsed = parseSettingsSchema(safeSchema);
        QVERIFY(parsed.ok());

        auto normalized = normalizeSettings(
            *parsed.value,
            SettingScope::Component,
            {{QStringLiteral("count"), QJsonValue(-9007199254740991.0)}}
        );
        QVERIFY(normalized.ok());
        QCOMPARE(
            normalized.value->value(QStringLiteral("count")).toDouble(),
            -9007199254740991.0
        );

        normalized = normalizeSettings(
            *parsed.value,
            SettingScope::Component,
            {{QStringLiteral("count"), QJsonValue(9007199254740991.0)}}
        );
        QVERIFY(normalized.ok());
        QCOMPARE(
            normalized.value->value(QStringLiteral("count")).toDouble(),
            9007199254740991.0
        );

        normalized = normalizeSettings(
            *parsed.value,
            SettingScope::Component,
            {{QStringLiteral("count"), QJsonValue(9007199254740992.0)}}
        );
        QVERIFY(!normalized.ok());
        QVERIFY(hasCode(
            normalized.errors,
            QStringLiteral("settings-value.invalid-number")
        ));

        const QByteArray roundedAuthoredInteger = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"count","scope":"component","type":"integer","label":"Count","description":"Must not round.","group":"general","order":1,"default":9007199254740993,"minimum":0,"maximum":9007199254740993}
          ]
        })";
        parsed = parseSettingsSchema(roundedAuthoredInteger);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("json.lossy-number")
        ));
    }

    void normalizesColorAndKeybinding()
    {
        const QByteArray schemaBytes = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"accent","scope":"component","type":"color","label":"Accent","description":"Accent color.","group":"general","order":1,"default":"#aabbcc"},
            {"key":"shortcut","scope":"component","type":"keybinding","label":"Shortcut","description":"Activation shortcut.","group":"general","order":2,"default":{"modifiers":["super","ctrl"],"key":"K"}}
          ]
        })";
        const auto schema = parseSettingsSchema(schemaBytes);
        QVERIFY(schema.ok());
        const auto normalized = normalizeSettings(
            *schema.value,
            SettingScope::Component,
            {}
        );
        QVERIFY(normalized.ok());
        QCOMPARE(
            normalized.value->value(QStringLiteral("accent")).toString(),
            QStringLiteral("#AABBCC")
        );
        const auto modifiers = normalized.value->value(
            QStringLiteral("shortcut")
        ).toObject().value(QStringLiteral("modifiers")).toArray();
        QCOMPARE(modifiers, QJsonArray({QStringLiteral("ctrl"), QStringLiteral("super")}));
    }

    void keybindingModifiersMatchPublishedLowercaseVocabulary()
    {
        const QByteArray schemaBytes = R"({
          "schemaVersion":1,
          "settings":[
            {"key":"shortcut","scope":"component","type":"keybinding","label":"Shortcut","description":"Activation shortcut.","group":"general","order":1,"default":{"modifiers":["Super"],"key":"K"}}
          ]
        })";
        const auto schema = parseSettingsSchema(schemaBytes);
        QVERIFY(!schema.ok());
        QVERIFY(hasCode(
            schema.errors,
            QStringLiteral("settings-value.invalid-modifier")
        ));
    }

    void duplicateJsonKeysCannotBeCollapsed()
    {
        const auto result = parseSettingsSchema(
            R"({"schemaVersion":1,"schemaVersion":1,"settings":[]})"
        );
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("json.duplicate-key")));
    }
};

QTEST_APPLESS_MAIN(ComponentSettingsSchemaTest)

#include "component_settings_schema_test.moc"
