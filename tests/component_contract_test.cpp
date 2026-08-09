#include "component/component_contract.h"
#include "component/strict_json.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>

using namespace HyprShelld::Components;

namespace {

[[nodiscard]] QByteArray readTestData(const char *relativePath)
{
    const auto path = QFINDTESTDATA(relativePath);
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

[[nodiscard]] QByteArray workspaceManifest()
{
    return readTestData(
        "../data/components/"
        "io.github.coastlinesec.hyprshelld.workspace-switcher/manifest.json"
    );
}

} // namespace

class ComponentContractTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesProtectedWorkspaceManifest()
    {
        const auto bytes = workspaceManifest();
        QVERIFY(!bytes.isEmpty());
        const auto result = parseComponentManifest(
            bytes,
            ComponentOrigin::System
        );
        QVERIFY2(result.ok(), "The shipped workspace manifest must validate");
        QCOMPARE(result.value->id, QLatin1StringView(workspaceSwitcherId));
        QVERIFY(result.value->origin == ComponentOrigin::System);
        QVERIFY(result.value->type == ComponentType::BarWidget);
        QVERIFY(result.value->runtime.kind == RuntimeKind::BuiltinV1);
        QCOMPARE(
            result.value->runtime.factory,
            QLatin1StringView(workspaceSwitcherFactory)
        );
        QCOMPARE(
            result.value->componentApiVersion,
            QLatin1StringView(currentComponentApiVersion)
        );
        QCOMPARE(result.value->requestedCapabilities.size(), 2);
        QVERIFY(validateCurrentHostSupport(*result.value).isEmpty());
    }

    void derivesOriginAndProtectsBuiltinNamespace()
    {
        const auto result = parseComponentManifest(
            workspaceManifest(),
            ComponentOrigin::User
        );
        QVERIFY(!result.ok());
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("manifest.reserved-component-id")
        ));
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("manifest.builtin-runtime-forbidden")
        ));
    }

    void detectsDuplicateKeysAfterEscapeDecoding()
    {
        const auto result = parseStrictJsonObject(
            R"({"name":"first","na\u006de":"second"})"
        );
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("json.duplicate-key")));
    }

    void rejectsInvalidUtf8AndNonJsonNumbers()
    {
        QByteArray invalidUtf8{"{\"value\":\""};
        invalidUtf8.append(char(0xc3));
        invalidUtf8.append(char(0x28));
        invalidUtf8.append("\"}");
        auto result = parseStrictJsonObject(invalidUtf8);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("json.invalid-utf8")));

        result = parseStrictJsonObject(R"({"value":01})");
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("json.invalid-number")));
    }

    void rejectsInvalidTypeRuntimeCombination()
    {
        auto bytes = workspaceManifest();
        bytes.replace("\"bar-widget\"", "\"shell-service\"");
        const auto result = parseComponentManifest(
            bytes,
            ComponentOrigin::System
        );
        QVERIFY(!result.ok());
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("manifest.invalid-type-runtime-combination")
        ));
    }

    void rejectsPaddedAndControlPresentationMetadata()
    {
        auto padded = workspaceManifest();
        padded.replace(
            "\"name\": \"Workspace Switcher\"",
            "\"name\": \" Workspace Switcher\""
        );
        auto result = parseComponentManifest(padded, ComponentOrigin::System);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("manifest.invalid-string")));

        auto tabbed = workspaceManifest();
        tabbed.replace(
            "\"name\": \"Workspace Switcher\"",
            "\"name\": \"Workspace\\tSwitcher\""
        );
        result = parseComponentManifest(tabbed, ComponentOrigin::System);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("manifest.invalid-string")));

        auto deleted = workspaceManifest();
        deleted.replace(
            "\"name\": \"Workspace Switcher\"",
            "\"name\": \"Workspace\\u007fSwitcher\""
        );
        result = parseComponentManifest(deleted, ComponentOrigin::System);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("manifest.invalid-string")));

        auto c1Control = workspaceManifest();
        c1Control.replace(
            "\"name\": \"Workspace Switcher\"",
            "\"name\": \"Workspace\\u0085Switcher\""
        );
        result = parseComponentManifest(c1Control, ComponentOrigin::System);
        QVERIFY(!result.ok());
        QVERIFY(hasCode(result.errors, QStringLiteral("manifest.invalid-string")));
    }

    void rejectsUnicodeFormatControlsInUserPresentationMetadata()
    {
        const auto userManifest = [](const QString &name) {
            return QStringLiteral(R"({
                "manifestVersion": 1,
                "id": "org.example.review-safe",
                "version": "1.0.0",
                "type": "bar-widget",
                "name": "%1",
                "description": "A review-safe fixture.",
                "authors": [{"name": "Fixture Author"}],
                "license": "MIT",
                "componentApiVersion": "1.0",
                "runtime": {
                    "kind": "declarative-v1",
                    "entrypoint": "payload/widget.json"
                },
                "requestedCapabilities": []
            })").arg(name).toUtf8();
        };

        const auto baseline = parseComponentManifest(
            userManifest(QStringLiteral("Safe Name")),
            ComponentOrigin::User
        );
        QVERIFY2(baseline.ok(), "The user manifest fixture must be valid");

        const auto rightToLeftOverride = parseComponentManifest(
            userManifest(QStringLiteral("Safe\u202Eeman")),
            ComponentOrigin::User
        );
        QVERIFY(!rightToLeftOverride.ok());
        QVERIFY(hasCode(
            rightToLeftOverride.errors,
            QStringLiteral("manifest.invalid-string")
        ));

        const auto zeroWidthJoiner = parseComponentManifest(
            userManifest(QStringLiteral("Safe\u200DName")),
            ComponentOrigin::User
        );
        QVERIFY(!zeroWidthJoiner.ok());
        QVERIFY(hasCode(
            zeroWidthJoiner.errors,
            QStringLiteral("manifest.invalid-string")
        ));

        const auto supplementaryFormat = parseComponentManifest(
            userManifest(QStringLiteral("Safe\U000E0001Name")),
            ComponentOrigin::User
        );
        QVERIFY(!supplementaryFormat.ok());
        QVERIFY(hasCode(
            supplementaryFormat.errors,
            QStringLiteral("manifest.invalid-string")
        ));
    }

    void supportGatePinsCapabilitiesAndDependencies()
    {
        auto unknownCapability = workspaceManifest();
        unknownCapability.replace(
            "shell.workspaces.read",
            "shell.workspaces.raad"
        );
        auto parsed = parseComponentManifest(
            unknownCapability,
            ComponentOrigin::System
        );
        QVERIFY(parsed.ok());
        auto errors = validateCurrentHostSupport(*parsed.value);
        QVERIFY(hasCode(
            errors,
            QStringLiteral("component.unsupported-capability")
        ));
        QVERIFY(hasCode(
            errors,
            QStringLiteral("component.required-capabilities-missing")
        ));

        auto dependency = workspaceManifest();
        dependency.replace(
            "\n  ]\n}",
            "\n  ],\n  \"dependencies\": ["
            "{\"id\":\"com.example.helper\",\"version\":\"1.0.0\"}]\n}"
        );
        parsed = parseComponentManifest(dependency, ComponentOrigin::System);
        QVERIFY(parsed.ok());
        errors = validateCurrentHostSupport(*parsed.value);
        QVERIFY(hasCode(
            errors,
            QStringLiteral("component.unsupported-dependencies")
        ));
    }

    void capabilityLabelsCannotEndWithHyphens()
    {
        auto manifest = workspaceManifest();
        manifest.replace(
            "shell.workspaces.read",
            "shell-.workspaces.read"
        );
        const auto parsed = parseComponentManifest(
            manifest,
            ComponentOrigin::System
        );
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringLiteral("manifest.invalid-capability-id")
        ));
    }

    void acceptsOnlyStrictReverseDnsAndSemanticVersions_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<bool>("validId");
        QTest::addColumn<bool>("validVersion");

        QTest::newRow("component-id") << "com.example.widget" << true << false;
        QTest::newRow("uppercase-id") << "Com.example.widget" << false << false;
        QTest::newRow("short-id") << "example.widget" << false << false;
        QTest::newRow("semver") << "1.2.3-beta.1+build" << false << true;
        QTest::newRow("leading-zero") << "01.2.3" << false << false;
        QTest::newRow("partial-version") << "1.2" << false << false;
    }

    void acceptsOnlyStrictReverseDnsAndSemanticVersions()
    {
        QFETCH(QString, value);
        QFETCH(bool, validId);
        QFETCH(bool, validVersion);
        QCOMPARE(isValidComponentId(value), validId);
        QCOMPARE(isStrictSemanticVersion(value), validVersion);
    }

    void publishedSchemasEncodeClosedSecurityShape()
    {
        const auto settingsBytes = readTestData(
            "../interfaces/components/v1/settings.schema.json"
        );
        const auto settingsJson = parseStrictJsonObject(
            settingsBytes,
            {.maximumBytes = 512 * 1024, .maximumDepth = 64}
        );
        QVERIFY(settingsJson.ok());
        const auto settingDefinition = settingsJson.value->value(
            QStringLiteral("$defs")
        ).toObject().value(QStringLiteral("setting")).toObject();
        const auto branches = settingDefinition.value(
            QStringLiteral("oneOf")
        ).toArray();
        QCOMPARE(branches.size(), 9);
        for (const auto &branch : branches) {
            QCOMPARE(
                branch.toObject().value(
                    QStringLiteral("unevaluatedProperties")
                ).toBool(),
                false
            );
        }

        const auto integrityBytes = readTestData(
            "../interfaces/components/v1/integrity.schema.json"
        );
        const auto integrityJson = parseStrictJsonObject(
            integrityBytes,
            {.maximumBytes = 128 * 1024, .maximumDepth = 32}
        );
        QVERIFY(integrityJson.ok());
        const auto exclusions = integrityJson.value->value(
            QStringLiteral("properties")
        ).toObject().value(QStringLiteral("files")).toObject().value(
            QStringLiteral("propertyNames")
        ).toObject().value(QStringLiteral("allOf")).toArray().at(0).toObject()
            .value(QStringLiteral("not")).toObject().value(
                QStringLiteral("enum")
            ).toArray();
        QVERIFY(exclusions.contains(QStringLiteral("integrity.json")));
        QVERIFY(exclusions.contains(QStringLiteral("signature.json")));
    }
};

QTEST_APPLESS_MAIN(ComponentContractTest)

#include "component_contract_test.moc"
