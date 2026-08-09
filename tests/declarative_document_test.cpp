#include "component/declarative_document.h"
#include "component/settings_schema.h"
#include "component/strict_json.h"

#include <QFile>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>

using namespace HyprShelld::Components;

namespace {

[[nodiscard]] bool hasCode(
    const ValidationErrors &errors,
    const QString &code
)
{
    return std::ranges::any_of(errors, [&code](const ValidationError &error) {
        return error.code == code;
    });
}

[[nodiscard]] SettingsSchema settingsSchema()
{
    const auto parsed = parseSettingsSchema(R"({
      "schemaVersion": 1,
      "settings": [
        {"key":"displayMode","scope":"component","type":"string","label":"Display mode","description":"Text shown in the pill.","group":"general","order":1,"default":"Clock","minimumLength":1,"maximumLength":64},
        {"key":"instanceLabel","scope":"instance","type":"string","label":"Instance label","description":"Per-instance text.","group":"general","order":2,"default":"Clock","maximumLength":64},
        {"key":"enabled","scope":"component","type":"boolean","label":"Enabled","description":"Whether it is enabled.","group":"general","order":3,"default":true}
      ]
    })");
    Q_ASSERT(parsed);
    return *parsed.value;
}

} // namespace

class DeclarativeDocumentTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsAndCanonicalizesLiteralPill()
    {
        const auto parsed = parseDeclarativeDocument(R"({
          "tooltip":"Local time",
          "text":{"literal":"12:34"},
          "type":"text-pill",
          "maximumWidth":160,
          "documentVersion":1
        })");
        QVERIFY(parsed);
        QCOMPARE(parsed.value->text.kind, DeclarativeTextSourceKind::Literal);
        QCOMPARE(parsed.value->text.value, QStringLiteral("12:34"));
        QVERIFY(parsed.value->tooltip.has_value());
        QCOMPARE(*parsed.value->tooltip, QStringLiteral("Local time"));
        QVERIFY(parsed.value->maximumWidth.has_value());
        QCOMPARE(*parsed.value->maximumWidth, quint32(160));

        const auto canonical = serializeDeclarativeDocument(*parsed.value);
        QVERIFY(!canonical.endsWith('\n'));
        const auto reparsed = parseDeclarativeDocument(canonical);
        QVERIFY(reparsed);
        QCOMPARE(*reparsed.value, *parsed.value);
        QCOMPARE(
            serializeDeclarativeDocument(*reparsed.value),
            canonical
        );
    }

    void acceptsLowerCamelCaseComponentSetting()
    {
        const auto schema = settingsSchema();
        const auto parsed = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"setting":"displayMode"}})",
            &schema
        );
        QVERIFY(parsed);
        QCOMPARE(
            parsed.value->text.kind,
            DeclarativeTextSourceKind::ComponentSetting
        );
        QCOMPARE(parsed.value->text.value, QStringLiteral("displayMode"));
    }

    void settingMustResolveToSupportedComponentSetting()
    {
        const auto schema = settingsSchema();
        for (const auto &[document, code] : {
                 std::pair{
                     QByteArray(R"({"documentVersion":1,"type":"text-pill","text":{"setting":"missing"}})"),
                     QStringLiteral("declarative.unknown-setting")
                 },
                 std::pair{
                     QByteArray(R"({"documentVersion":1,"type":"text-pill","text":{"setting":"instanceLabel"}})"),
                     QStringLiteral("declarative.unsupported-setting")
                 },
                 std::pair{
                     QByteArray(R"({"documentVersion":1,"type":"text-pill","text":{"setting":"enabled"}})"),
                     QStringLiteral("declarative.unsupported-setting")
                 },
             }) {
            const auto parsed = parseDeclarativeDocument(document, &schema);
            QVERIFY(!parsed);
            QVERIFY(hasCode(parsed.errors, code));
        }

        const auto withoutSchema = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"setting":"displayMode"}})"
        );
        QVERIFY(!withoutSchema);
        QVERIFY(hasCode(
            withoutSchema.errors,
            QStringLiteral("declarative.unknown-setting")
        ));

        const auto oversizedSchema = parseSettingsSchema(R"({
          "schemaVersion":1,
          "settings":[
            {"key":"longText","scope":"component","type":"string","label":"Long text","description":"Too wide for the declarative runtime.","group":"general","order":1,"default":"Clock","minimumLength":1,"maximumLength":256}
          ]
        })");
        QVERIFY(oversizedSchema);
        const auto oversized = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"setting":"longText"}})",
            &*oversizedSchema.value
        );
        QVERIFY(!oversized);
        QVERIFY(hasCode(
            oversized.errors,
            QStringLiteral("declarative.invalid-setting-text-bounds")
        ));

        const auto emptySchema = parseSettingsSchema(R"({
          "schemaVersion":1,
          "settings":[
            {"key":"optionalText","scope":"component","type":"string","label":"Optional text","description":"May be empty.","group":"general","order":1,"default":"","maximumLength":64}
          ]
        })");
        QVERIFY(emptySchema);
        const auto empty = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"setting":"optionalText"}})",
            &*emptySchema.value
        );
        QVERIFY(!empty);
        QVERIFY(hasCode(
            empty.errors,
            QStringLiteral("declarative.invalid-setting-text-bounds")
        ));
    }

    void boundSettingsMustAlwaysResolveToRendererSafeText()
    {
        const auto paddedDefaultSchema = parseSettingsSchema(R"({
          "schemaVersion":1,
          "settings":[
            {"key":"label","scope":"component","type":"string","label":"Label","description":"Displayed text.","group":"general","order":1,"default":" padded ","minimumLength":1,"maximumLength":64}
          ]
        })");
        QVERIFY(paddedDefaultSchema);
        const auto paddedDefault = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"setting":"label"}})",
            &*paddedDefaultSchema.value
        );
        QVERIFY(!paddedDefault);
        QVERIFY(hasCode(
            paddedDefault.errors,
            QStringLiteral("declarative.invalid-setting-default")
        ));

        const auto unsafeEnumSchema = parseSettingsSchema(R"({
          "schemaVersion":1,
          "settings":[
            {"key":"mode","scope":"component","type":"enum","label":"Mode","description":"Displayed mode.","group":"general","order":1,"default":"safe","options":[{"value":"safe","label":"Safe"},{"value":"bad\u2028value","label":"Unsafe value"}]}
          ]
        })");
        QVERIFY(unsafeEnumSchema);
        const auto unsafeEnum = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"setting":"mode"}})",
            &*unsafeEnumSchema.value
        );
        QVERIFY(!unsafeEnum);
        QVERIFY(hasCode(
            unsafeEnum.errors,
            QStringLiteral("declarative.invalid-setting-options")
        ));
    }

    void rejectsAnythingBeyondTheTrustedPrimitive()
    {
        for (const auto document : {
                 QByteArray(R"({"documentVersion":1,"type":"text-pill","text":{"literal":"Clock"},"action":{"command":"date"}})"),
                 QByteArray(R"({"documentVersion":1,"type":"text-pill","text":{"literal":"Clock","qml":"Item {}"}})"),
                 QByteArray(R"({"documentVersion":1,"type":"image","text":{"literal":"remote-resource"}})"),
             }) {
            const auto parsed = parseDeclarativeDocument(document);
            QVERIFY(!parsed);
        }
    }

    void rejectsAmbiguousOrUnboundedText()
    {
        auto parsed = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"literal":"Clock","setting":"displayMode"}})"
        );
        QVERIFY(!parsed);
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("declarative.exactly-one-text-source")
        ));

        parsed = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"literal":" Clock"}})"
        );
        QVERIFY(!parsed);
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("declarative.invalid-string")
        ));

        const auto longLiteral = QString(129, QLatin1Char('a'));
        parsed = parseDeclarativeDocument(
            QStringLiteral(
                R"({"documentVersion":1,"type":"text-pill","text":{"literal":"%1"}})"
            ).arg(longLiteral).toUtf8()
        );
        QVERIFY(!parsed);
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("declarative.invalid-string")
        ));
    }

    void widthAndJsonGrammarAreStrict()
    {
        for (const auto width : {47, 513}) {
            const auto parsed = parseDeclarativeDocument(
                QStringLiteral(
                    R"({"documentVersion":1,"type":"text-pill","text":{"literal":"Clock"},"maximumWidth":%1})"
                ).arg(width).toUtf8()
            );
            QVERIFY(!parsed);
            QVERIFY(hasCode(
                parsed.errors,
                QStringLiteral("declarative.invalid-maximum-width")
            ));
        }

        const auto duplicate = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","type":"text-pill","text":{"literal":"Clock"}})"
        );
        QVERIFY(!duplicate);
        QVERIFY(hasCode(duplicate.errors, QStringLiteral("json.duplicate-key")));
    }

    void resolvedTextBoundaryRejectsDisplaySpoofing()
    {
        QVERIFY(isValidDeclarativeResolvedText(QStringLiteral("Clock 12:34")));
        QVERIFY(!isValidDeclarativeResolvedText(QStringLiteral(" Clock")));
        QVERIFY(!isValidDeclarativeResolvedText(QStringLiteral("Clock\n12:34")));
        QVERIFY(!isValidDeclarativeResolvedText(
            QStringLiteral("Clock\u202E43:21")
        ));
        QVERIFY(!isValidDeclarativeResolvedText(
            QStringLiteral("Clock\u2028Tomorrow")
        ));
    }

    void publishedSchemaIsClosed()
    {
        const auto path = QFINDTESTDATA(
            "../interfaces/components/v1/declarative.schema.json"
        );
        QVERIFY(!path.isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto parsed = parseStrictJsonObject(
            file.readAll(),
            {.maximumBytes = 64 * 1024, .maximumDepth = 32}
        );
        QVERIFY(parsed);
        QCOMPARE(
            parsed.value->value(QStringLiteral("additionalProperties"))
                .toBool(true),
            false
        );
    }
};

QTEST_APPLESS_MAIN(DeclarativeDocumentTest)

#include "declarative_document_test.moc"
