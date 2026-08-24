#include "compositord/generation.h"
#include "compositord/renderer.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

#include <sys/stat.h>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";
constexpr auto nonceB = "fedcba9876543210fedcba9876543210";
constexpr auto authorityA = "11111111111111111111111111111111";
constexpr auto authorityB = "22222222222222222222222222222222";

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] bool writeFile(
    const QString &path,
    const QByteArrayView bytes
)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(bytes.data(), bytes.size()) != bytes.size()
        || !file.flush()) {
        return false;
    }
    file.close();
    return file.error() == QFileDevice::NoError;
}

[[nodiscard]] QJsonObject readObject(const QString &path)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(readBytes(path), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

[[nodiscard]] QByteArray encode(QJsonObject object)
{
    auto bytes = QJsonDocument(std::move(object)).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] QString describeErrors(const ValidationErrors &errors)
{
    QStringList descriptions;
    for (const auto &error : errors) {
        descriptions.append(error.path + QLatin1Char(':') + error.code);
    }
    return descriptions.join(QStringLiteral(", "));
}

[[nodiscard]] bool hasCode(
    const ValidationErrors &errors,
    const QString &code
)
{
    return std::ranges::any_of(
        errors,
        [&code](const ValidationError &error) { return error.code == code; }
    );
}

[[nodiscard]] qsizetype occurrences(
    const QByteArrayView haystack,
    const QByteArrayView needle
)
{
    if (needle.isEmpty()) {
        return 0;
    }
    qsizetype count = 0;
    qsizetype offset = 0;
    while ((offset = haystack.indexOf(needle, offset)) >= 0) {
        ++count;
        offset += needle.size();
    }
    return count;
}

[[nodiscard]] QByteArray luaString(const QString &value)
{
    QByteArray result{"\""};
    for (const auto byte : value.toUtf8()) {
        if (byte == '\\' || byte == '"') {
            result.append('\\');
        }
        result.append(byte);
    }
    result.append('"');
    return result;
}

[[nodiscard]] QDateTime fixedTime()
{
    return QDateTime::fromString(
        QStringLiteral("2026-08-09T12:34:56.789Z"),
        Qt::ISODateWithMs
    );
}

[[nodiscard]] QJsonObject bindingOptions()
{
    return {
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
    };
}

[[nodiscard]] QJsonObject dispatcherBinding(
    const QString &id,
    const QString &key,
    const QString &submap = {}
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("modifiers"), QJsonArray{QStringLiteral("super")}},
        {QStringLiteral("key"), key},
        {QStringLiteral("actionType"), QStringLiteral("dispatcher")},
        {QStringLiteral("action"), QStringLiteral("cursor.move")},
        {QStringLiteral("arguments"), QJsonObject{
            {QStringLiteral("x"), 10},
            {QStringLiteral("y"), 20},
        }},
        {QStringLiteral("description"), QStringLiteral("Move the cursor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("submap"), submap},
        {QStringLiteral("options"), bindingOptions()},
    };
}

[[nodiscard]] QJsonObject brokerBinding(const bool enabled)
{
    return {
        {QStringLiteral("id"), QStringLiteral("launch-terminal")},
        {QStringLiteral("modifiers"), QJsonArray{QStringLiteral("super")}},
        {QStringLiteral("key"), QStringLiteral("F1")},
        {QStringLiteral("actionType"), QStringLiteral("defaultApp")},
        {QStringLiteral("action"), QStringLiteral("defaultApp.terminal")},
        {QStringLiteral("arguments"), QJsonObject{}},
        {QStringLiteral("description"), QStringLiteral("Open a terminal")},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("submap"), QString()},
        {QStringLiteral("options"), bindingOptions()},
    };
}

[[nodiscard]] QJsonObject groupbarOverrides()
{
    const auto gradient = [](const QString &first,
                             const QString &second,
                             const int angle) {
        return QJsonObject{
            {QStringLiteral("colors"), QJsonArray{first, second}},
            {QStringLiteral("angle"), angle},
        };
    };
    return {
        {QStringLiteral("hyprland.group.groupbar.enabled"), false},
        {
            QStringLiteral("hyprland.group.groupbar.disable_when_only"),
            true,
        },
        {
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QStringLiteral("Fira Sans"),
        },
        {
            QStringLiteral("hyprland.group.groupbar.font_weight_active"),
            650,
        },
        {
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"),
            325,
        },
        {QStringLiteral("hyprland.group.groupbar.font_size"), 17},
        {QStringLiteral("hyprland.group.groupbar.gradients"), true},
        {QStringLiteral("hyprland.group.groupbar.height"), 23},
        {QStringLiteral("hyprland.group.groupbar.indicator_gap"), 4},
        {QStringLiteral("hyprland.group.groupbar.indicator_height"), 5},
        {QStringLiteral("hyprland.group.groupbar.stacked"), true},
        {QStringLiteral("hyprland.group.groupbar.priority"), 6},
        {QStringLiteral("hyprland.group.groupbar.render_titles"), false},
        {QStringLiteral("hyprland.group.groupbar.scrolling"), false},
        {
            QStringLiteral("hyprland.group.groupbar.middle_click_close"),
            false,
        },
        {QStringLiteral("hyprland.group.groupbar.rounding"), 7},
        {
            QStringLiteral("hyprland.group.groupbar.rounding_power"),
            2.573,
        },
        {
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"),
            9,
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.gradient_rounding_power"
            ),
            3.14159,
        },
        {
            QStringLiteral("hyprland.group.groupbar.round_only_edges"),
            false,
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.gradient_round_only_edges"
            ),
            false,
        },
        {QStringLiteral("hyprland.group.groupbar.gaps_out"), 8},
        {QStringLiteral("hyprland.group.groupbar.gaps_in"), 9},
        {QStringLiteral("hyprland.group.groupbar.keep_upper_gap"), false},
        {QStringLiteral("hyprland.group.groupbar.text_offset"), -3},
        {QStringLiteral("hyprland.group.groupbar.text_padding"), 6},
        {QStringLiteral("hyprland.group.groupbar.blur"), true},
        {
            QStringLiteral("hyprland.group.groupbar.col.active"),
            gradient(
                QStringLiteral("0xFF102030"),
                QStringLiteral("0xFF405060"),
                11
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.col.inactive"),
            gradient(
                QStringLiteral("0xFF112233"),
                QStringLiteral("0xFF445566"),
                22
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.col.locked_active"),
            gradient(
                QStringLiteral("0xFF213243"),
                QStringLiteral("0xFF546576"),
                33
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.col.locked_inactive"),
            gradient(
                QStringLiteral("0xFF314253"),
                QStringLiteral("0xFF647586"),
                44
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.text_color"),
            QStringLiteral("0xFF718293"),
        },
        {
            QStringLiteral("hyprland.group.groupbar.text_color_inactive"),
            QStringLiteral("0xFF8293A4"),
        },
        {
            QStringLiteral("hyprland.group.groupbar.text_color_locked_active"),
            QStringLiteral("0xFF93A4B5"),
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.text_color_locked_inactive"
            ),
            QStringLiteral("0xFFA4B5C6"),
        },
    };
}

} // namespace

class CompositorRendererTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalog;
    ActionCatalog actionCatalog;
    Catalog dormantCatalogV2;
    ActionCatalog dormantActionCatalogV2;
    QJsonObject defaults;
    QJsonObject dormantTemplateV2;
    QJsonObject dormantSourceManifestV2;

    [[nodiscard]] ValidationResult<DesiredState> parseState(
        QJsonObject object
    ) const
    {
        return parseDesiredState(
            encode(std::move(object)), catalog, actionCatalog
        );
    }

    [[nodiscard]] RenderResult render(
        const DesiredState &state,
        const QString &generationRoot,
        const QString &customPath,
        const QString &nonce = QString::fromLatin1(nonceA),
        const QDateTime &createdAt = fixedTime()
    ) const
    {
        return renderGeneration(
            state,
            catalog,
            actionCatalog,
            generationRoot,
            customPath,
            nonce,
            createdAt
        );
    }

    [[nodiscard]] ValidationResult<DesiredStateV2> dormantState(
        const QString &authorityId = QString::fromLatin1(authorityA)
    ) const
    {
        auto object = dormantTemplateV2;
        object.insert(QStringLiteral("authorityId"), authorityId);
        return parseDormantDesiredStateV2(
            encode(std::move(object)),
            dormantCatalogV2,
            dormantActionCatalogV2
        );
    }

    [[nodiscard]] DormantRenderResultV2 renderDormant(
        const DesiredStateV2 &state,
        const QString &generationRoot,
        const QString &customPath,
        const QString &nonce = QString::fromLatin1(nonceA),
        const QDateTime &createdAt = fixedTime()
    ) const
    {
        return renderDormantGenerationV2(
            state,
            dormantCatalogV2,
            dormantActionCatalogV2,
            generationRoot,
            customPath,
            nonce,
            createdAt
        );
    }

private slots:
    void initTestCase()
    {
        const auto parsedCatalog = parseCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
        );
        QVERIFY2(parsedCatalog, qPrintable(describeErrors(parsedCatalog.errors)));
        catalog = *parsedCatalog.value;

        const auto parsedActions = parseActionCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE))
        );
        QVERIFY2(parsedActions, qPrintable(describeErrors(parsedActions.errors)));
        actionCatalog = *parsedActions.value;

        defaults = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE));
        QVERIFY(!defaults.isEmpty());

        const auto parsedDormantCatalog = parseDormantCatalogV2(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE))
        );
        QVERIFY2(
            parsedDormantCatalog,
            qPrintable(describeErrors(parsedDormantCatalog.errors))
        );
        dormantCatalogV2 = *parsedDormantCatalog.value;

        const auto parsedDormantActions = parseDormantActionCatalogV2(
            readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE
            )),
            readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE
            ))
        );
        QVERIFY2(
            parsedDormantActions,
            qPrintable(describeErrors(parsedDormantActions.errors))
        );
        dormantActionCatalogV2 = *parsedDormantActions.value;

        dormantTemplateV2 = readObject(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE
        ));
        dormantSourceManifestV2 = readObject(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V2_SOURCE_MANIFEST_FILE
        ));
        QVERIFY(!dormantTemplateV2.isEmpty());
        QVERIFY(!dormantSourceManifestV2.isEmpty());
        QVERIFY(fixedTime().isValid());
    }

    void rendersExactPrivateTreeContract()
    {
        const QStringList expectedModules{
            QStringLiteral("modules/00-session.lua"),
            QStringLiteral("modules/10-monitors.lua"),
            QStringLiteral("modules/20-environment.lua"),
            QStringLiteral("modules/30-input.lua"),
            QStringLiteral("modules/31-gestures.lua"),
            QStringLiteral("modules/32-cursor.lua"),
            QStringLiteral("modules/40-general.lua"),
            QStringLiteral("modules/41-layouts.lua"),
            QStringLiteral("modules/42-workspaces.lua"),
            QStringLiteral("modules/43-groups.lua"),
            QStringLiteral("modules/50-decorations.lua"),
            QStringLiteral("modules/51-animations.lua"),
            QStringLiteral("modules/60-rules.lua"),
            QStringLiteral("modules/70-keybinds.lua"),
            QStringLiteral("modules/80-permissions.lua"),
            QStringLiteral("modules/90-advanced.lua"),
        };
        QCOMPARE(managedModulePaths(), expectedModules);

        const auto parsed = parseState(defaults);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto rendered = render(
            *parsed.value, generationRoot, customPath
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto &generation = *rendered.value;

        QCOMPARE(generation.entrypoint, QStringLiteral("hyprland.lua"));
        QCOMPARE(generation.files.size(), expectedModules.size() + 1);
        QStringList expectedFiles = expectedModules;
        expectedFiles.append(generation.entrypoint);
        std::ranges::sort(expectedFiles);
        QCOMPARE(generation.files.keys(), expectedFiles);

        const auto expectedSecondLine = QByteArrayLiteral(
            "-- Put custom configuration in "
        ) + customPath.toUtf8() + QByteArrayLiteral("; it is loaded last.");
        for (auto iterator = generation.files.constBegin();
             iterator != generation.files.constEnd(); ++iterator) {
            const auto lines = iterator->contents.split('\n');
            QVERIFY2(lines.size() >= 3, qPrintable(iterator.key()));
            QCOMPARE(lines.at(0), QByteArray(managedWarningLine));
            QCOMPARE(lines.at(1), expectedSecondLine);
            QCOMPARE(iterator->path, iterator.key());
            QCOMPARE(iterator->size, quint64(iterator->contents.size()));
            QCOMPARE(iterator->sha256, sha256(iterator->contents));
        }

        const auto loader = generation.files.value(generation.entrypoint).contents;
        qsizetype previous = -1;
        for (const auto &module : expectedModules) {
            const auto absolutePath = QDir(generationRoot).filePath(module);
            const auto statement = QByteArrayLiteral("require(")
                + luaString(absolutePath) + QByteArrayLiteral(")");
            const auto position = loader.indexOf(statement);
            QVERIFY2(position > previous, qPrintable(module));
            QCOMPARE(occurrences(loader, statement), qsizetype(1));
            previous = position;
        }
        QVERIFY(!loader.contains("require(\"modules/"));
        const auto customStatement = QByteArrayLiteral("require(")
            + luaString(customPath) + QByteArrayLiteral(")");
        QCOMPARE(occurrences(loader, customStatement), qsizetype(1));
        QVERIFY(loader.trimmed().endsWith(customStatement));
        QVERIFY(loader.indexOf(customStatement) > previous);
        for (const auto &module : expectedModules) {
            QVERIFY(!generation.files.value(module).contents.contains(
                customStatement
            ));
        }
        QVERIFY(!generation.files
                     .value(QStringLiteral("modules/40-general.lua"))
                     .contents.contains("hl.config("));
        QCOMPARE(
            generation.files
                .value(QStringLiteral("modules/51-animations.lua"))
                .contents,
            QByteArray(managedWarningLine) + '\n'
                + QByteArrayLiteral("-- Put custom configuration in ")
                + customPath.toUtf8()
                + QByteArrayLiteral(
                    "; it is loaded last.\n\n"
                    "hl.curve(\"default\", { points = { { 0, 0.75 }, { 0.15, 1 } }, type = \"bezier\" })\n"
                    "hl.curve(\"default\", { dampening = 25, mass = 1, stiffness = 250, type = \"spring\" })\n"
                    "hl.curve(\"linear\", { points = { { 0, 0 }, { 1, 1 } }, type = \"bezier\" })\n"
                )
        );
        QCOMPARE(generation.activationRequirement, ActivationRequirement::Reload);
    }

    void rendersBuiltinPreludeThenTypedCustomShadowsThenAnimations()
    {
        auto stateObject = defaults;
        stateObject.insert(
            QStringLiteral("curves"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("curve-default")},
                    {QStringLiteral("name"), QStringLiteral("default")},
                    {QStringLiteral("type"), QStringLiteral("bezier")},
                    {QStringLiteral("points"), QJsonArray{
                        QJsonArray{0.12, 0.72}, QJsonArray{0.22, 0.98},
                    }},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("curve-linear")},
                    {QStringLiteral("name"), QStringLiteral("linear")},
                    {QStringLiteral("type"), QStringLiteral("spring")},
                    {QStringLiteral("stiffness"), 275.5},
                    {QStringLiteral("dampening"), 27.5},
                    {QStringLiteral("mass"), 1.25},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("curve-custom")},
                    {QStringLiteral("name"), QStringLiteral("custom")},
                    {QStringLiteral("type"), QStringLiteral("bezier")},
                    {QStringLiteral("points"), QJsonArray{
                        QJsonArray{0.2, 0.1}, QJsonArray{0.8, 0.9},
                    }},
                },
            }
        );
        stateObject.insert(
            QStringLiteral("animations"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("animation-fade")},
                    {QStringLiteral("name"), QStringLiteral("fade")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("speed"), 2.5},
                    {QStringLiteral("curve"), QStringLiteral("default")},
                    {QStringLiteral("style"), QString{}},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("animation-windows")},
                    {QStringLiteral("name"), QStringLiteral("windows")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("speed"), 6.0},
                    {QStringLiteral("curve"), QStringLiteral("linear")},
                    {QStringLiteral("style"), QStringLiteral("slide")},
                },
            }
        );
        const auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto rendered = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            QDir(temporary.path()).filePath(QStringLiteral("user-custom.lua"))
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto contents = rendered.value->files
            .value(QStringLiteral("modules/51-animations.lua")).contents;
        const QList<QByteArray> prelude{
            QByteArrayLiteral(
                "hl.curve(\"default\", { points = { { 0, 0.75 }, { 0.15, 1 } }, type = \"bezier\" })"
            ),
            QByteArrayLiteral(
                "hl.curve(\"default\", { dampening = 25, mass = 1, stiffness = 250, type = \"spring\" })"
            ),
            QByteArrayLiteral(
                "hl.curve(\"linear\", { points = { { 0, 0 }, { 1, 1 } }, type = \"bezier\" })"
            ),
        };
        qsizetype previous = -1;
        for (const auto &statement : prelude) {
            const auto position = contents.indexOf(statement);
            QVERIFY2(position > previous, statement.constData());
            QCOMPARE(occurrences(contents, statement), qsizetype(1));
            previous = position;
        }
        const auto lines = contents.split('\n');
        const auto customDefault = std::ranges::find(
            lines,
            QByteArrayLiteral(
                "hl.curve(\"default\", {points = {{0.12, 0.71999999999999997}, {0.22, 0.97999999999999998}}, type = \"bezier\"})"
            )
        );
        const auto customLinear = std::ranges::find(
            lines,
            QByteArrayLiteral(
                "hl.curve(\"linear\", {dampening = 27.5, mass = 1.25, stiffness = 275.5, type = \"spring\"})"
            )
        );
        const auto custom = std::ranges::find(
            lines,
            QByteArrayLiteral(
                "hl.curve(\"custom\", {points = {{0.20000000000000001, 0.10000000000000001}, {0.80000000000000004, 0.90000000000000002}}, type = \"bezier\"})"
            )
        );
        const auto fade = std::ranges::find(
            lines,
            QByteArrayLiteral(
                "hl.animation({bezier = \"default\", enabled = true, leaf = \"fade\", speed = 2.5})"
            )
        );
        const auto windows = std::ranges::find(
            lines,
            QByteArrayLiteral(
                "hl.animation({enabled = true, leaf = \"windows\", speed = 6, spring = \"linear\", style = \"slide\"})"
            )
        );
        QVERIFY(customDefault != lines.end());
        QVERIFY(customLinear != lines.end());
        QVERIFY(custom != lines.end());
        QVERIFY(fade != lines.end());
        QVERIFY(windows != lines.end());
        QVERIFY(customDefault->contains("type = \"bezier\""));
        QVERIFY(customLinear->contains("type = \"spring\""));
        QVERIFY(customLinear->contains("dampening = 27.5"));
        QVERIFY(customLinear->contains("mass = 1.25"));
        QVERIFY(custom->contains("type = \"bezier\""));
        QVERIFY(fade->contains("bezier = \"default\""));
        QVERIFY(!fade->contains("spring ="));
        QVERIFY(windows->contains("spring = \"linear\""));
        QVERIFY(!windows->contains("bezier ="));
        QVERIFY(customDefault < customLinear);
        QVERIFY(customLinear < custom);
        QVERIFY(custom < fade);
        QVERIFY(fade < windows);
    }

    void rendersInputPointerBehaviorInOwnedModulesAndElidesDefaults()
    {
        const auto modulePaths = managedModulePaths();
        const auto inputPath = QStringLiteral("modules/30-input.lua");
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        const auto inputIndex = modulePaths.indexOf(inputPath);
        const auto advancedIndex = modulePaths.indexOf(advancedPath);
        QVERIFY(inputIndex >= 0);
        QVERIFY(advancedIndex > inputIndex);
        QCOMPARE(advancedIndex, modulePaths.size() - 1);

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        const auto defaultState = parseState(defaults);
        QVERIFY2(
            defaultState,
            qPrintable(describeErrors(defaultState.errors))
        );
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultInput = defaultGeneration.value->files
            .value(inputPath).contents;
        const auto defaultAdvanced = defaultGeneration.value->files
            .value(advancedPath).contents;
        QVERIFY(!defaultInput.contains("force_no_accel"));
        QVERIFY(!defaultInput.contains("rotation"));
        QVERIFY(!defaultAdvanced.contains("middle_click_paste"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.input.force_no_accel"), true
        );
        overrides.insert(QStringLiteral("hyprland.input.rotation"), 137);
        overrides.insert(
            QStringLiteral("hyprland.misc.middle_click_paste"), false
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );

        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");
        QCOMPARE(
            configuredGeneration.value->files.value(inputPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({input = {force_no_accel = true, rotation = 137}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({misc = {middle_click_paste = false}})\n"
                )
        );

        const auto loader = configuredGeneration.value->files
            .value(configuredGeneration.value->entrypoint).contents;
        const auto inputRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(inputPath))
            + QByteArrayLiteral(")");
        const auto advancedRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(advancedPath))
            + QByteArrayLiteral(")");
        const auto customRequire = QByteArrayLiteral("require(")
            + luaString(customPath) + QByteArrayLiteral(")");
        QVERIFY(loader.indexOf(inputRequire) < loader.indexOf(advancedRequire));
        QVERIFY(loader.indexOf(advancedRequire) < loader.indexOf(customRequire));
    }

    void rendersActiveLayoutShortcutFallbackAndPreservesExactDeviceValue()
    {
        const auto inputPath = QStringLiteral("modules/30-input.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultInput = defaultGeneration.value->files
            .value(inputPath).contents;
        QVERIFY(!defaultInput.contains("resolve_binds_by_sym"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto configuredInput = configuredGeneration.value->files
            .value(inputPath).contents;
        QCOMPARE(
            configuredInput,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({input = {resolve_binds_by_sym = true}})\n"
                )
        );
        QCOMPARE(
            occurrences(configuredInput, "resolve_binds_by_sym = true"),
            1
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );

        auto deviceObject = configuredObject;
        deviceObject.insert(
            QStringLiteral("devices"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("keyboard-main")},
                {QStringLiteral("selector"),
                 QStringLiteral("Main Keyboard")},
                {QStringLiteral("kind"), QStringLiteral("keyboard")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("overrides"), QJsonObject{
                    {QStringLiteral("resolve_binds_by_sym"), false},
                }},
            }}
        );
        const auto deviceState = parseState(deviceObject);
        QVERIFY2(deviceState, qPrintable(describeErrors(deviceState.errors)));
        const auto deviceGeneration = render(
            *deviceState.value, generationRoot, customPath
        );
        QVERIFY2(
            deviceGeneration,
            qPrintable(describeErrors(deviceGeneration.errors))
        );
        const auto deviceInput = deviceGeneration.value->files
            .value(inputPath).contents;
        QCOMPARE(
            deviceInput,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({input = {resolve_binds_by_sym = true}})\n"
                    "hl.device({enabled = true, name = \"Main Keyboard\", "
                    "resolve_binds_by_sym = false})\n"
                )
        );
        QCOMPARE(
            occurrences(deviceInput, "resolve_binds_by_sym = true"),
            1
        );
        QCOMPARE(
            occurrences(deviceInput, "resolve_binds_by_sym = false"),
            1
        );
        QCOMPARE(
            deviceGeneration.value->activationRequirement,
            ActivationRequirement::Restart
        );
    }

    void rendersWindowsFollowMouseThresholdInInputAndElidesDefault()
    {
        const auto inputPath = QStringLiteral("modules/30-input.lua");
        const auto generalPath = QStringLiteral("modules/40-general.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        QVERIFY(!defaultGeneration.value->files.value(inputPath).contents
                     .contains("follow_mouse_threshold"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            123456.7890625
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");
        QCOMPARE(
            configuredGeneration.value->files.value(inputPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({input = {follow_mouse_threshold = "
                    "123456.7890625}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );

        const auto loader = configuredGeneration.value->files
            .value(configuredGeneration.value->entrypoint).contents;
        const auto inputRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(inputPath))
            + QByteArrayLiteral(")");
        const auto generalRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(generalPath))
            + QByteArrayLiteral(")");
        const auto customRequire = QByteArrayLiteral("require(")
            + luaString(customPath) + QByteArrayLiteral(")");
        QVERIFY(loader.indexOf(inputRequire) < loader.indexOf(generalRequire));
        QVERIFY(loader.indexOf(generalRequire) < loader.indexOf(customRequire));
    }

    void rendersTouchAndTabletInputAsNestedLuaAndElidesDefaults()
    {
        const auto inputPath = QStringLiteral("modules/30-input.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultInput = defaultGeneration.value->files
            .value(inputPath).contents;
        QVERIFY(!defaultInput.contains("touchdevice"));
        QVERIFY(!defaultInput.contains("tablet"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.input.touchdevice.enabled"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 5
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.relative_input"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.left_handed"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.region_position"),
            QJsonArray{123.125, -456.875}
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            ),
            true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QJsonArray{-99.5, 0}
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);

        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");
        const auto configuredInput = configuredGeneration.value->files
            .value(inputPath).contents;
        QCOMPARE(
            configuredInput,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({input = {tablet = {absolute_region_position "
                    "= true, left_handed = true, region_position = {123.125, "
                    "-456.875}, region_size = {-99.5, 0}, relative_input = "
                    "true, transform = 6}, touchdevice = "
                    "{enabled = false, transform = 5}}})\n"
                )
        );
        QCOMPARE(occurrences(configuredInput, "tablet ="), 1);
        QCOMPARE(occurrences(configuredInput, "touchdevice ="), 1);
        QCOMPARE(occurrences(configuredInput, "region_position = {"), 1);
        QCOMPARE(occurrences(configuredInput, "region_size = {"), 1);
        QCOMPARE(
            occurrences(configuredInput, "absolute_region_position ="), 1
        );
    }

    void rendersInputGestureBehaviorAndOrderedTypedActions()
    {
        const auto gesturesPath = QStringLiteral("modules/31-gestures.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultGestures = defaultGeneration.value->files
            .value(gesturesPath).contents;
        QVERIFY(!defaultGestures.contains("close_max_timeout"));
        QVERIFY(!defaultGestures.contains("hl.gesture("));

        auto stateObject = defaults;
        auto overrides = stateObject.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1375
        );
        stateObject.insert(QStringLiteral("overrides"), overrides);
        stateObject.insert(
            QStringLiteral("gestures"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("gesture-workspace")},
                    {QStringLiteral("fingers"), 3},
                    {QStringLiteral("direction"), QStringLiteral("left")},
                    {QStringLiteral("modifiers"), QJsonArray{
                        QStringLiteral("ctrl"), QStringLiteral("super")
                    }},
                    {QStringLiteral("scale"), 1.5},
                    {QStringLiteral("disableInhibit"), true},
                    {QStringLiteral("action"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("workspace")},
                    }},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("gesture-zoom")},
                    {QStringLiteral("fingers"), 4},
                    {QStringLiteral("direction"), QStringLiteral("pinchIn")},
                    {QStringLiteral("modifiers"), QJsonArray{}},
                    {QStringLiteral("scale"), 1.0},
                    {QStringLiteral("disableInhibit"), false},
                    {QStringLiteral("action"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("cursorZoom")},
                        {QStringLiteral("zoomLevel"), 1.25},
                        {QStringLiteral("mode"), QStringLiteral("live")},
                    }},
                },
            }
        );
        const auto configuredState = parseState(stateObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto contents = configuredGeneration.value->files
            .value(gesturesPath).contents;
        QVERIFY(contents.contains(
            QByteArrayLiteral(
                "hl.config({gestures = {close_max_timeout = 1375}})"
            )
        ));
        const auto workspace = contents.indexOf(QByteArrayLiteral(
            "hl.gesture({action = \"workspace\""
        ));
        const auto zoom = contents.indexOf(QByteArrayLiteral(
            "hl.gesture({action = \"cursorZoom\""
        ));
        QVERIFY(workspace >= 0);
        QVERIFY(zoom > workspace);
        QVERIFY(contents.mid(workspace).contains(
            QByteArrayLiteral("disable_inhibit = true")
        ));
        QVERIFY(contents.mid(workspace).contains(
            QByteArrayLiteral("mods = \"CTRL SUPER\"")
        ));
        QVERIFY(contents.mid(zoom).contains(
            QByteArrayLiteral("mode = \"live\"")
        ));
        QVERIFY(contents.mid(zoom).contains(
            QByteArrayLiteral("zoom_level = 1.25")
        ));
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
        const auto loader = configuredGeneration.value->files
            .value(configuredGeneration.value->entrypoint).contents;
        const auto gestureRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(gesturesPath))
            + QByteArrayLiteral(")");
        const auto customRequire = QByteArrayLiteral("require(")
            + luaString(customPath) + QByteArrayLiteral(")");
        QVERIFY(loader.indexOf(gestureRequire) < loader.indexOf(customRequire));
    }

    void rendersWorkspaceSwitchingHistoryAndPointerPlacementScalars()
    {
        const auto cursorPath = QStringLiteral("modules/32-cursor.lua");
        const auto keybindsPath = QStringLiteral("modules/70-keybinds.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultCursor = defaultGeneration.value->files
            .value(cursorPath).contents;
        const auto defaultKeybinds = defaultGeneration.value->files
            .value(keybindsPath).contents;
        QVERIFY(!defaultCursor.contains("warp_on_change_workspace"));
        QVERIFY(!defaultCursor.contains("warp_on_toggle_special"));
        QVERIFY(!defaultKeybinds.contains("allow_workspace_cycles"));
        QVERIFY(!defaultKeybinds.contains("hide_special_on_workspace_change"));
        QVERIFY(!defaultKeybinds.contains("workspace_back_and_forth"));
        QVERIFY(!defaultKeybinds.contains("workspace_center_on"));

        auto stateObject = defaults;
        auto overrides = stateObject.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.binds.allow_workspace_cycles"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.binds.hide_special_on_workspace_change"),
            true
        );
        overrides.insert(
            QStringLiteral("hyprland.binds.workspace_back_and_forth"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.binds.workspace_center_on"), 0
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.warp_on_change_workspace"), 1
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.warp_on_toggle_special"), 2
        );
        stateObject.insert(QStringLiteral("overrides"), overrides);

        const auto configuredState = parseState(stateObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");
        QCOMPARE(
            configuredGeneration.value->files.value(keybindsPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({binds = {allow_workspace_cycles = true, "
                    "hide_special_on_workspace_change = true, "
                    "workspace_back_and_forth = true, "
                    "workspace_center_on = 0}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->files.value(cursorPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({cursor = {warp_on_change_workspace = 1, "
                    "warp_on_toggle_special = 2}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );

        const auto loader = configuredGeneration.value->files
            .value(configuredGeneration.value->entrypoint).contents;
        const auto cursorRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(cursorPath))
            + QByteArrayLiteral(")");
        const auto keybindsRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(keybindsPath))
            + QByteArrayLiteral(")");
        const auto customRequire = QByteArrayLiteral("require(")
            + luaString(customPath) + QByteArrayLiteral(")");
        QVERIFY(loader.indexOf(cursorRequire) < loader.indexOf(keybindsRequire));
        QVERIFY(loader.indexOf(keybindsRequire) < loader.indexOf(customRequire));
    }

    void rendersInputCursorBehaviorInTheCursorModuleAndElidesDefaults()
    {
        const auto cursorPath = QStringLiteral("modules/32-cursor.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultCursor = defaultGeneration.value->files
            .value(cursorPath).contents;
        const QList<QByteArray> cursorLeaves{
            QByteArrayLiteral("hide_on_key_press"),
            QByteArrayLiteral("hide_on_touch"),
            QByteArrayLiteral("hide_on_tablet"),
            QByteArrayLiteral("inactive_timeout"),
            QByteArrayLiteral("hotspot_padding"),
            QByteArrayLiteral("no_warps"),
            QByteArrayLiteral("persistent_warps"),
            QByteArrayLiteral("warp_back_after_non_mouse_input"),
        };
        for (const auto &leaf : cursorLeaves) {
            QVERIFY2(!defaultCursor.contains(leaf), leaf.constData());
        }

        auto stateObject = defaults;
        auto overrides = stateObject.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.cursor.hide_on_key_press"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.hide_on_touch"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.hide_on_tablet"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.inactive_timeout"), 2.37
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.hotspot_padding"), 13
        );
        overrides.insert(QStringLiteral("hyprland.cursor.no_warps"), true);
        overrides.insert(
            QStringLiteral("hyprland.cursor.persistent_warps"), true
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.cursor.warp_back_after_non_mouse_input"
            ),
            true
        );
        stateObject.insert(QStringLiteral("overrides"), overrides);

        const auto configuredState = parseState(stateObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");
        const auto configuredCursor = configuredGeneration.value->files
            .value(cursorPath).contents;
        QCOMPARE(
            configuredCursor,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({cursor = {hide_on_key_press = true, "
                    "hide_on_tablet = true, hide_on_touch = false, "
                    "hotspot_padding = 13, inactive_timeout = "
                    "2.3700000000000001, no_warps = true, "
                    "persistent_warps = true, "
                    "warp_back_after_non_mouse_input = true}})\n"
                )
        );
        for (const auto &leaf : cursorLeaves) {
            QCOMPARE(occurrences(configuredCursor, leaf), qsizetype(1));
        }
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );

        const auto loader = configuredGeneration.value->files
            .value(configuredGeneration.value->entrypoint).contents;
        const auto cursorRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(cursorPath))
            + QByteArrayLiteral(")");
        const auto customRequire = QByteArrayLiteral("require(")
            + luaString(customPath) + QByteArrayLiteral(")");
        QCOMPARE(occurrences(loader, cursorRequire), qsizetype(1));
        QVERIFY(loader.indexOf(cursorRequire) < loader.indexOf(customRequire));
    }

    void rendersWindowsNavigationAndFullscreenBindScalars()
    {
        const auto keybindsPath = QStringLiteral("modules/70-keybinds.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        auto baselineObject = defaults;
        baselineObject.insert(
            QStringLiteral("submaps"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("resize-submap")},
                {QStringLiteral("name"), QStringLiteral("resize")},
                {QStringLiteral("reset"), QString()},
                {QStringLiteral("enabled"), true},
            }}
        );
        baselineObject.insert(
            QStringLiteral("bindings"),
            QJsonArray{
                dispatcherBinding(
                    QStringLiteral("top-level"), QStringLiteral("F7")
                ),
                dispatcherBinding(
                    QStringLiteral("inside-submap"),
                    QStringLiteral("F8"),
                    QStringLiteral("resize")
                ),
            }
        );
        const auto defaultState = parseState(baselineObject);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultKeybinds = defaultGeneration.value->files
            .value(keybindsPath).contents;
        for (const auto setting : {
                 QByteArrayLiteral("allow_pin_fullscreen"),
                 QByteArrayLiteral("focus_preferred_method"),
                 QByteArrayLiteral("ignore_group_lock"),
                 QByteArrayLiteral("movefocus_cycles_fullscreen"),
                 QByteArrayLiteral("movefocus_cycles_groupfirst"),
                 QByteArrayLiteral("window_direction_monitor_fallback"),
             }) {
            QVERIFY(!defaultKeybinds.contains(setting));
        }
        QCOMPARE(
            occurrences(defaultKeybinds, QByteArrayLiteral("hl.define_submap(")),
            qsizetype(1)
        );
        QCOMPARE(
            occurrences(defaultKeybinds, QByteArrayLiteral("hl.bind(")),
            qsizetype(2)
        );

        auto stateObject = baselineObject;
        auto overrides = stateObject.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.binds.allow_pin_fullscreen"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.binds.focus_preferred_method"), 1
        );
        overrides.insert(
            QStringLiteral("hyprland.binds.ignore_group_lock"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.binds.movefocus_cycles_fullscreen"),
            true
        );
        overrides.insert(
            QStringLiteral("hyprland.binds.movefocus_cycles_groupfirst"),
            true
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.binds.window_direction_monitor_fallback"
            ),
            false
        );
        stateObject.insert(QStringLiteral("overrides"), overrides);

        const auto configuredState = parseState(stateObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");
        QVERIFY(defaultKeybinds.startsWith(expectedHeader));
        const auto defaultBindingStatements = defaultKeybinds.mid(
            expectedHeader.size()
        );
        const auto configuredKeybinds = configuredGeneration.value->files
            .value(keybindsPath).contents;
        QCOMPARE(
            configuredKeybinds,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({binds = {allow_pin_fullscreen = true, "
                    "focus_preferred_method = 1, ignore_group_lock = true, "
                    "movefocus_cycles_fullscreen = true, "
                    "movefocus_cycles_groupfirst = true, "
                    "window_direction_monitor_fallback = false}})\n"
                )
                + defaultBindingStatements
        );
        QCOMPARE(
            occurrences(configuredKeybinds, QByteArrayLiteral("hl.define_submap(")),
            qsizetype(1)
        );
        QCOMPARE(
            occurrences(configuredKeybinds, QByteArrayLiteral("hl.bind(")),
            qsizetype(2)
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Restart
        );

        const auto loader = configuredGeneration.value->files
            .value(configuredGeneration.value->entrypoint).contents;
        const auto keybindsRequire = QByteArrayLiteral("require(")
            + luaString(QDir(generationRoot).filePath(keybindsPath))
            + QByteArrayLiteral(")");
        const auto customRequire = QByteArrayLiteral("require(")
            + luaString(customPath) + QByteArrayLiteral(")");
        QVERIFY(loader.indexOf(keybindsRequire) < loader.indexOf(customRequire));
    }

    void rendersWindowsMiscScalarsInTheAdvancedModule()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultAdvanced = defaultGeneration.value->files
            .value(advancedPath).contents;
        QVERIFY(!defaultAdvanced.contains("enable_anr_dialog"));
        QVERIFY(!defaultAdvanced.contains("anr_missed_pings"));
        QVERIFY(!defaultAdvanced.contains("size_limits_tiled"));
        QVERIFY(!defaultAdvanced.contains("always_follow_on_dnd"));
        QVERIFY(!defaultAdvanced.contains("focus_on_activate"));
        QVERIFY(!defaultAdvanced.contains("mouse_move_focuses_monitor"));
        QVERIFY(!defaultAdvanced.contains("on_focus_under_fullscreen"));
        QVERIFY(!defaultAdvanced.contains("exit_window_retains_fullscreen"));
        QVERIFY(!defaultAdvanced.contains("enable_swallow"));
        QVERIFY(!defaultAdvanced.contains("swallow_regex"));
        QVERIFY(!defaultAdvanced.contains("swallow_exception_regex"));
        QVERIFY(!defaultAdvanced.contains("allow_session_lock_restore"));
        QVERIFY(!defaultAdvanced.contains("lockdead_screen_delay"));
        QVERIFY(!defaultAdvanced.contains("disable_scale_notification"));
        QVERIFY(!defaultAdvanced.contains("render_unfocused_fps"));
        QVERIFY(!defaultAdvanced.contains("screencopy_force_8b"));

        auto stateObject = defaults;
        auto overrides = stateObject.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.misc.enable_anr_dialog"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.anr_missed_pings"), 7
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.size_limits_tiled"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.always_follow_on_dnd"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.focus_on_activate"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.mouse_move_focuses_monitor"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen"), 1
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.exit_window_retains_fullscreen"),
            true
        );
        overrides.insert(QStringLiteral("hyprland.misc.enable_swallow"), true);
        overrides.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("^(kitty|Alacritty)$")
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QStringLiteral("^scratch$")
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.allow_session_lock_restore"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.lockdead_screen_delay"), 2750
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.disable_scale_notification"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.render_unfocused_fps"), 37
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.screencopy_force_8b"), false
        );
        stateObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(stateObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({misc = {allow_session_lock_restore = true, "
                    "always_follow_on_dnd = false, anr_missed_pings = 7, "
                    "disable_scale_notification = true, "
                    "enable_anr_dialog = false, "
                    "enable_swallow = true, "
                    "exit_window_retains_fullscreen = true, "
                    "focus_on_activate = true, "
                    "lockdead_screen_delay = 2750, "
                    "mouse_move_focuses_monitor = false, "
                    "on_focus_under_fullscreen = 1, "
                    "render_unfocused_fps = 37, "
                    "screencopy_force_8b = false, "
                    "size_limits_tiled = true, "
                    "swallow_exception_regex = \"^scratch$\", "
                    "swallow_regex = \"^(kitty|Alacritty)$\"}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersPreservedMiscCompatibilityOverridesInExactAdvancedOrder()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultAdvanced = defaultGeneration.value->files
            .value(advancedPath).contents;
        QVERIFY(!defaultAdvanced.contains("animate_manual_resizes"));
        QVERIFY(!defaultAdvanced.contains("animate_mouse_windowdragging"));
        QVERIFY(!defaultAdvanced.contains("layers_hog_keyboard_focus"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.misc.animate_manual_resizes"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.animate_mouse_windowdragging"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.layers_hog_keyboard_focus"), false
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({misc = {animate_manual_resizes = true, "
                    "animate_mouse_windowdragging = true, "
                    "layers_hog_keyboard_focus = false}})\n"
                )
        );
        for (auto iterator = configuredGeneration.value->files.constBegin();
             iterator != configuredGeneration.value->files.constEnd();
             ++iterator) {
            if (iterator.key() == advancedPath) continue;
            QVERIFY(!iterator->contents.contains("animate_manual_resizes"));
            QVERIFY(!iterator->contents.contains(
                "animate_mouse_windowdragging"
            ));
            QVERIFY(!iterator->contents.contains(
                "layers_hog_keyboard_focus"
            ));
        }
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersAdvancedLockAndBrandingScalarsInMiscAndElidesDefaults()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultAdvanced = defaultGeneration.value->files
            .value(advancedPath).contents;
        QVERIFY(!defaultAdvanced.contains("disable_hyprland_logo"));
        QVERIFY(!defaultAdvanced.contains("disable_splash_rendering"));
        QVERIFY(!defaultAdvanced.contains("session_lock_xray"));
        QVERIFY(!defaultAdvanced.contains("session_lock_blur"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.misc.disable_hyprland_logo"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.disable_splash_rendering"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.session_lock_xray"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.session_lock_blur"), true
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({misc = {disable_hyprland_logo = true, "
                    "disable_splash_rendering = true, session_lock_blur = "
                    "true, session_lock_xray = true}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );

        auto retainedBlurObject = defaults;
        auto retainedBlurOverrides = retainedBlurObject.value(
            QStringLiteral("overrides")
        ).toObject();
        retainedBlurOverrides.insert(
            QStringLiteral("hyprland.misc.session_lock_blur"), true
        );
        retainedBlurObject.insert(
            QStringLiteral("overrides"), retainedBlurOverrides
        );
        const auto retainedBlurState = parseState(retainedBlurObject);
        QVERIFY2(
            retainedBlurState,
            qPrintable(describeErrors(retainedBlurState.errors))
        );
        const auto retainedBlurGeneration = render(
            *retainedBlurState.value, generationRoot, customPath
        );
        QVERIFY2(
            retainedBlurGeneration,
            qPrintable(describeErrors(retainedBlurGeneration.errors))
        );
        QCOMPARE(
            retainedBlurGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({misc = {session_lock_blur = true}})\n"
                )
        );
    }

    void rendersAdvancedXWaylandNearestNeighborAndElidesDefault()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        QVERIFY(!defaultGeneration.value->files.value(advancedPath).contents
                     .contains("use_nearest_neighbor"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor"), false
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({xwayland = {use_nearest_neighbor = false}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersAdvancedExpandedUndersizedTexturesAndElidesDefault()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        QVERIFY(!defaultGeneration.value->files.value(advancedPath).contents
                     .contains("expand_undersized_textures"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.render.expand_undersized_textures"),
            false
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({render = {expand_undersized_textures = false}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersAdvancedDirectScanoutAndElidesDefault()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        QVERIFY(!defaultGeneration.value->files.value(advancedPath).contents
                     .contains("direct_scanout"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(QStringLiteral("hyprland.render.direct_scanout"), 2);
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({render = {direct_scanout = 2}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersAdvancedFp16SdrTransferFunctionAndElidesDefault()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        QVERIFY(!defaultGeneration.value->files.value(advancedPath).contents
                     .contains("fp16_sdr_tf"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(QStringLiteral("hyprland.render.fp16_sdr_tf"), 1);
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({render = {fp16_sdr_tf = 1}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersAdvancedXpModeAndElidesDefault()
    {
        const auto advancedPath = QStringLiteral("modules/90-advanced.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        QVERIFY(!defaultGeneration.value->files.value(advancedPath).contents
                     .contains("xp_mode"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(QStringLiteral("hyprland.render.xp_mode"), true);
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(advancedPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({render = {xp_mode = true}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersAdvancedInputCapturePoliciesInInputAndElidesDefaults()
    {
        const auto inputPath = QStringLiteral("modules/30-input.lua");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto generationRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto expectedHeader = QByteArray(managedWarningLine) + '\n'
            + QByteArrayLiteral("-- Put custom configuration in ")
            + customPath.toUtf8()
            + QByteArrayLiteral("; it is loaded last.\n\n");

        const auto defaultState = parseState(defaults);
        QVERIFY2(defaultState, qPrintable(describeErrors(defaultState.errors)));
        const auto defaultGeneration = render(
            *defaultState.value, generationRoot, customPath
        );
        QVERIFY2(
            defaultGeneration,
            qPrintable(describeErrors(defaultGeneration.errors))
        );
        const auto defaultInput = defaultGeneration.value->files
            .value(inputPath).contents;
        QVERIFY(!defaultInput.contains("capture_modifiers"));
        QVERIFY(!defaultInput.contains("enforce_barriers"));

        auto configuredObject = defaults;
        auto overrides = configuredObject.value(
            QStringLiteral("overrides")
        ).toObject();
        overrides.insert(
            QStringLiteral("hyprland.input-capture.capture_modifiers"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.input-capture.enforce_barriers"), false
        );
        configuredObject.insert(QStringLiteral("overrides"), overrides);
        const auto configuredState = parseState(configuredObject);
        QVERIFY2(
            configuredState,
            qPrintable(describeErrors(configuredState.errors))
        );
        const auto configuredGeneration = render(
            *configuredState.value, generationRoot, customPath
        );
        QVERIFY2(
            configuredGeneration,
            qPrintable(describeErrors(configuredGeneration.errors))
        );
        QCOMPARE(
            configuredGeneration.value->files.value(inputPath).contents,
            expectedHeader
                + QByteArrayLiteral(
                    "hl.config({input_capture = {capture_modifiers = true, enforce_barriers = false}})\n"
                )
        );
        QCOMPARE(
            configuredGeneration.value->activationRequirement,
            ActivationRequirement::Reload
        );
    }

    void rendersWindowGroupBehaviorWithoutDroppingGroupVisualOverrides()
    {
        const auto modulePaths = managedModulePaths();
        const auto groupModulePath = QStringLiteral("modules/43-groups.lua");
        const auto groupModuleIndex = modulePaths.indexOf(groupModulePath);
        QVERIFY(groupModuleIndex > 0);
        QCOMPARE(
            modulePaths.at(groupModuleIndex - 1),
            QStringLiteral("modules/42-workspaces.lua")
        );
        QCOMPARE(
            modulePaths.at(groupModuleIndex + 1),
            QStringLiteral("modules/50-decorations.lua")
        );

        auto stateObject = defaults;
        auto overrides = groupbarOverrides();
        QCOMPARE(overrides.size(), 35);
        overrides.insert(
            QStringLiteral("hyprland.group.merge_groups_on_groupbar"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.group.drag_into_group"), 2
        );
        overrides.insert(QStringLiteral("hyprland.group.auto_group"), false);
        overrides.insert(
            QStringLiteral("hyprland.group.col.border_active"),
            QJsonObject{
                {QStringLiteral("colors"),
                 QJsonArray{
                     QStringLiteral("0xFF112233"),
                     QStringLiteral("0xFF445566"),
                 }},
                {QStringLiteral("angle"), 37},
            }
        );
        overrides.insert(
            QStringLiteral("hyprland.group.insert_after_current"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.group.group_on_movetoworkspace"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.group.focus_removed_window"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.group.merge_groups_on_drag"), false
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.group.merge_floated_into_tiled_on_groupbar"
            ),
            true
        );
        stateObject.insert(QStringLiteral("overrides"), overrides);
        const auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto customPath = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto rendered = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            customPath
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));

        QCOMPARE(
            rendered.value->files.value(groupModulePath).contents,
            QByteArray(managedWarningLine) + '\n'
                + QByteArrayLiteral("-- Put custom configuration in ")
                + customPath.toUtf8()
                + QByteArrayLiteral(
                    "; it is loaded last.\n\n"
                    "hl.config({group = {auto_group = false, "
                    "col = {border_active = {angle = 37, colors = "
                    "{\"0xFF112233\", \"0xFF445566\"}}}, "
                    "drag_into_group = 2, focus_removed_window = false, "
                    "group_on_movetoworkspace = true, "
                    "groupbar = {blur = true, col = {active = {angle = 11, "
                    "colors = {\"0xFF102030\", \"0xFF405060\"}}, "
                    "inactive = {angle = 22, colors = {\"0xFF112233\", "
                    "\"0xFF445566\"}}, locked_active = {angle = 33, "
                    "colors = {\"0xFF213243\", \"0xFF546576\"}}, "
                    "locked_inactive = {angle = 44, colors = "
                    "{\"0xFF314253\", \"0xFF647586\"}}}, "
                    "disable_when_only = true, enabled = false, "
                    "font_family = \"Fira Sans\", font_size = 17, "
                    "font_weight_active = 650, font_weight_inactive = 325, "
                    "gaps_in = 9, gaps_out = 8, "
                    "gradient_round_only_edges = false, "
                    "gradient_rounding = 9, gradient_rounding_power = "
                    "3.1415899999999999, gradients = true, height = 23, "
                    "indicator_gap = 4, indicator_height = 5, "
                    "keep_upper_gap = false, middle_click_close = false, "
                    "priority = 6, render_titles = false, "
                    "round_only_edges = false, rounding = 7, "
                    "rounding_power = 2.573, scrolling = false, "
                    "stacked = true, text_color = \"0xFF718293\", "
                    "text_color_inactive = \"0xFF8293A4\", "
                    "text_color_locked_active = \"0xFF93A4B5\", "
                    "text_color_locked_inactive = \"0xFFA4B5C6\", "
                    "text_offset = -3, text_padding = 6}, "
                    "insert_after_current = false, "
                    "merge_floated_into_tiled_on_groupbar = true, "
                    "merge_groups_on_drag = false, "
                    "merge_groups_on_groupbar = false}})\n"
                )
        );
    }

    void manifestCommitsToEveryPayloadByte()
    {
        const auto parsed = parseState(defaults);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto rendered = render(*parsed.value, root, custom);
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto &generation = *rendered.value;

        QCOMPARE(
            generation.manifestBytes,
            JsonSupport::canonicalJson(generation.manifest) + '\n'
        );
        auto withoutGeneration = generation.manifest;
        withoutGeneration.remove(QStringLiteral("generation"));
        QCOMPARE(
            generation.generation,
            sha256(JsonSupport::canonicalJson(withoutGeneration))
        );
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                    .match(generation.generation)
                    .hasMatch());
        QCOMPARE(
            generation.manifest.value(QStringLiteral("generation")).toString(),
            generation.generation
        );
        QCOMPARE(
            generation.snapshotDigest,
            sha256(serializeDesiredState(*parsed.value))
        );
        QCOMPARE(
            generation.manifest.value(QStringLiteral("snapshotDigest")).toString(),
            generation.snapshotDigest
        );
        QCOMPARE(
            generation.manifest.value(QStringLiteral("catalogDigest")).toString(),
            parsed.value->catalogDigest
        );
        QCOMPARE(
            generation.manifest.value(QStringLiteral("actionCatalogDigest")).toString(),
            parsed.value->actionCatalogDigest
        );
        QCOMPARE(
            generation.manifest.value(QStringLiteral("revision")).toString(),
            QString::number(parsed.value->revision)
        );
        QCOMPARE(
            generation.manifest.value(QStringLiteral("activationNonce")).toString(),
            QString::fromLatin1(nonceA)
        );
        QCOMPARE(
            generation.manifest.value(QStringLiteral("createdAt")).toString(),
            QStringLiteral("2026-08-09T12:34:56.789Z")
        );
        QCOMPARE(
            generation.manifest.value(QStringLiteral("entrypoint")).toString(),
            generation.entrypoint
        );

        const auto manifestFiles = generation.manifest
                                       .value(QStringLiteral("files"))
                                       .toObject();
        QCOMPARE(manifestFiles.size(), generation.files.size());
        for (auto iterator = generation.files.constBegin();
             iterator != generation.files.constEnd(); ++iterator) {
            QVERIFY(manifestFiles.contains(iterator.key()));
            const auto metadata = manifestFiles.value(iterator.key()).toObject();
            QCOMPARE(metadata.size(), 2);
            QCOMPARE(metadata.value(QStringLiteral("sha256")).toString(),
                     sha256(iterator->contents));
            QCOMPARE(metadata.value(QStringLiteral("size")).toInteger(),
                     qint64(iterator->contents.size()));
        }
    }

    void outputIsDeterministicAndNonceKeyed()
    {
        const auto parsed = parseState(defaults);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto rootA = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto first = render(*parsed.value, rootA, custom);
        const auto retry = render(*parsed.value, rootA, custom);
        QVERIFY(first);
        QVERIFY(retry);
        QCOMPARE(first.value->files, retry.value->files);
        QCOMPARE(first.value->manifestBytes, retry.value->manifestBytes);
        QCOMPARE(first.value->generation, retry.value->generation);

        const auto rootB = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceB)
        );
        const auto otherNonce = render(
            *parsed.value,
            rootB,
            custom,
            QString::fromLatin1(nonceB)
        );
        QVERIFY(otherNonce);
        QVERIFY(first.value->generation != otherNonce.value->generation);
        QVERIFY(first.value->files.value(QStringLiteral("hyprland.lua"))
                    != otherNonce.value->files.value(QStringLiteral("hyprland.lua")));
        for (const auto &module : managedModulePaths()) {
            QCOMPARE(first.value->files.value(module),
                     otherNonce.value->files.value(module));
        }

        const auto later = render(
            *parsed.value,
            rootA,
            custom,
            QString::fromLatin1(nonceA),
            fixedTime().addSecs(1)
        );
        QVERIFY(later);
        QCOMPARE(first.value->files, later.value->files);
        QVERIFY(first.value->manifestBytes != later.value->manifestBytes);
        QVERIFY(first.value->generation != later.value->generation);
    }

    void curatedAppearanceOverridesUsePinnedModules()
    {
        auto stateObject = defaults;
        stateObject.insert(
            QStringLiteral("overrides"),
            QJsonObject{
                {QStringLiteral("hyprland.animations.enabled"), false},
                {QStringLiteral("hyprland.decoration.blur.enabled"), false},
                {QStringLiteral("hyprland.decoration.blur.size"), 24},
                {QStringLiteral("hyprland.decoration.blur.passes"), 5},
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.ignore_opacity"
                    ),
                    false,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.new_optimizations"
                    ),
                    false,
                },
                {QStringLiteral("hyprland.decoration.blur.xray"), true},
                {QStringLiteral("hyprland.decoration.blur.special"), true},
                {QStringLiteral("hyprland.decoration.blur.popups"), true},
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.popups_ignorealpha"
                    ),
                    0.35,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.input_methods"
                    ),
                    true,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.input_methods_ignorealpha"
                    ),
                    0.45,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.brightness"),
                    1.23456789012345,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.contrast"),
                    0.87654321098765,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.noise"),
                    0.012345678901234,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.vibrancy"),
                    0.23456789012345,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.vibrancy_darkness"
                    ),
                    0.34567890123456,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.border_part_of_window"
                    ),
                    false,
                },
                {QStringLiteral("hyprland.decoration.dim_inactive"), true},
                {QStringLiteral("hyprland.decoration.dim_strength"), 0.65},
                {QStringLiteral("hyprland.decoration.active_opacity"), 0.83},
                {QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61},
                {QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74},
                {QStringLiteral("hyprland.decoration.dim_modal"), false},
                {QStringLiteral("hyprland.decoration.dim_special"), 0.37},
                {QStringLiteral("hyprland.decoration.dim_around"), 0.43},
                {QStringLiteral("hyprland.decoration.rounding"), 7},
                {
                    QStringLiteral("hyprland.decoration.rounding_power"),
                    2.573,
                },
                {QStringLiteral("hyprland.decoration.shadow.enabled"), false},
                {QStringLiteral("hyprland.decoration.shadow.range"), 17},
                {
                    QStringLiteral(
                        "hyprland.decoration.shadow.render_power"
                    ),
                    4,
                },
                {QStringLiteral("hyprland.decoration.shadow.sharp"), true},
                {
                    QStringLiteral("hyprland.decoration.shadow.offset"),
                    QJsonArray{125.5, -80.25},
                },
                {QStringLiteral("hyprland.decoration.shadow.scale"), 0.75},
                {QStringLiteral("hyprland.decoration.glow.enabled"), true},
                {QStringLiteral("hyprland.decoration.glow.range"), 11},
                {
                    QStringLiteral("hyprland.decoration.glow.render_power"),
                    2,
                },
                {QStringLiteral("hyprland.general.border_size"), 4},
                {QStringLiteral("hyprland.general.layout"),
                 QStringLiteral("master")},
                {QStringLiteral("hyprland.general.resize_on_border"), true},
                {QStringLiteral("hyprland.general.snap.enabled"), true},
            }
        );
        const auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto parsedDefaults = parseState(defaults);
        QVERIFY2(
            parsedDefaults,
            qPrintable(describeErrors(parsedDefaults.errors))
        );
        const auto defaultRendered = render(
            *parsedDefaults.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceB)),
            QDir(temporary.path()).filePath(
                QStringLiteral("user-custom.lua")),
            QString::fromLatin1(nonceB)
        );
        QVERIFY2(
            defaultRendered,
            qPrintable(describeErrors(defaultRendered.errors))
        );
        QCOMPARE(defaultRendered.value->activationRequirement,
                 ActivationRequirement::Reload);
        const auto defaultDecorations = defaultRendered.value->files
            .value(QStringLiteral("modules/50-decorations.lua")).contents;
        QVERIFY(!defaultDecorations.contains("dim_inactive"));
        QVERIFY(!defaultDecorations.contains("dim_strength"));
        QVERIFY(!defaultDecorations.contains("active_opacity"));
        QVERIFY(!defaultDecorations.contains("inactive_opacity"));
        QVERIFY(!defaultDecorations.contains("fullscreen_opacity"));
        QVERIFY(!defaultDecorations.contains("dim_modal"));
        QVERIFY(!defaultDecorations.contains("dim_special"));
        QVERIFY(!defaultDecorations.contains("dim_around"));
        QVERIFY(!defaultDecorations.contains("blur ="));
        QVERIFY(!defaultDecorations.contains("ignore_opacity"));
        QVERIFY(!defaultDecorations.contains("input_methods"));
        QVERIFY(!defaultDecorations.contains("new_optimizations"));
        QVERIFY(!defaultDecorations.contains("passes ="));
        QVERIFY(!defaultDecorations.contains("popups"));
        QVERIFY(!defaultDecorations.contains("size ="));
        QVERIFY(!defaultDecorations.contains("brightness ="));
        QVERIFY(!defaultDecorations.contains("contrast ="));
        QVERIFY(!defaultDecorations.contains("noise ="));
        QVERIFY(!defaultDecorations.contains("vibrancy ="));
        QVERIFY(!defaultDecorations.contains("vibrancy_darkness ="));
        QVERIFY(!defaultDecorations.contains("border_part_of_window ="));
        QVERIFY(!defaultDecorations.contains("rounding_power ="));
        QVERIFY(!defaultDecorations.contains("shadow ="));
        QVERIFY(!defaultDecorations.contains("glow ="));
        QVERIFY(!defaultDecorations.contains("range ="));
        QVERIFY(!defaultDecorations.contains("render_power ="));
        QVERIFY(!defaultDecorations.contains("sharp ="));
        QVERIFY(!defaultDecorations.contains("offset ="));
        QVERIFY(!defaultDecorations.contains("scale ="));
        QVERIFY(!defaultDecorations.contains("xray"));

        const auto rendered = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            QDir(temporary.path()).filePath(
                QStringLiteral("user-custom.lua"))
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        QCOMPARE(rendered.value->activationRequirement,
                 ActivationRequirement::Reload);

        const auto general = rendered.value->files
                                 .value(QStringLiteral("modules/40-general.lua"))
                                 .contents;
        QVERIFY(general.contains("border_size = 4"));
        QVERIFY(general.contains("layout = \"master\""));
        QVERIFY(general.contains("resize_on_border = true"));
        QVERIFY(general.contains("snap = {enabled = true}"));

        const auto decorations = rendered.value->files
                                     .value(QStringLiteral(
                                         "modules/50-decorations.lua"))
                                     .contents;
        QVERIFY(decorations.contains(QByteArrayLiteral(
            "hl.config({decoration = {active_opacity = "
            "0.82999999999999996, blur = {brightness = 1.23456789012345, "
            "contrast = 0.87654321098764998, enabled = false, ignore_opacity = "
            "false, input_methods = true, input_methods_ignorealpha = "
            "0.45000000000000001, new_optimizations = false, noise = "
            "0.012345678901233999, passes = 5, popups = true, "
            "popups_ignorealpha = 0.34999999999999998, size = 24, special = "
            "true, vibrancy = 0.23456789012345, vibrancy_darkness = "
            "0.34567890123456002, xray = true}, border_part_of_window = "
            "false, dim_around = "
            "0.42999999999999999, dim_inactive = true, dim_modal = false, "
            "dim_special = 0.37, dim_strength = 0.65000000000000002, "
            "fullscreen_opacity = 0.73999999999999999, glow = {enabled = "
            "true, range = 11, render_power = 2}, inactive_opacity = "
            "0.60999999999999999, rounding = 7, rounding_power = 2.573, "
            "shadow = {enabled = false, offset = {125.5, -80.25}, range = "
            "17, render_power = 4, scale = 0.75, sharp = true}}})"
        )));
        QVERIFY(decorations.contains(QByteArrayLiteral(
            "blur = {brightness = 1.23456789012345, contrast = "
            "0.87654321098764998, enabled = false, ignore_opacity = false, "
            "input_methods = true, input_methods_ignorealpha = "
            "0.45000000000000001, new_optimizations = false, noise = "
            "0.012345678901233999, passes = 5, popups = true, "
            "popups_ignorealpha = 0.34999999999999998, size = 24, special = "
            "true, vibrancy = 0.23456789012345, vibrancy_darkness = "
            "0.34567890123456002, xray = true}"
        )));
        QVERIFY(decorations.contains("dim_inactive = true"));
        QVERIFY(decorations.contains("dim_strength = 0.65000000000000002"));
        QVERIFY(decorations.contains(
            "active_opacity = 0.82999999999999996"
        ));
        QVERIFY(decorations.contains(
            "inactive_opacity = 0.60999999999999999"
        ));
        QVERIFY(decorations.contains(
            "fullscreen_opacity = 0.73999999999999999"
        ));
        QVERIFY(decorations.contains("dim_modal = false"));
        QVERIFY(decorations.contains("dim_special = 0.37"));
        QVERIFY(decorations.contains("dim_around = 0.42999999999999999"));
        QVERIFY(decorations.contains("border_part_of_window = false"));
        QVERIFY(decorations.contains("rounding = 7"));
        QVERIFY(decorations.contains("rounding_power = 2.573"));
        QVERIFY(decorations.contains(QByteArrayLiteral(
            "shadow = {enabled = false, offset = {125.5, -80.25}, range = "
            "17, render_power = 4, scale = 0.75, sharp = true}"
        )));
        QVERIFY(decorations.contains(QByteArrayLiteral(
            "glow = {enabled = true, range = 11, render_power = 2}"
        )));
        QCOMPARE(occurrences(decorations, "dim_inactive = true"), 1);
        QCOMPARE(occurrences(
            decorations, "dim_strength = 0.65000000000000002"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "active_opacity = 0.82999999999999996"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "inactive_opacity = 0.60999999999999999"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "fullscreen_opacity = 0.73999999999999999"
        ), 1);
        QCOMPARE(occurrences(decorations, "dim_modal = false"), 1);
        QCOMPARE(occurrences(decorations, "dim_special = 0.37"), 1);
        QCOMPARE(occurrences(
            decorations, "dim_around = 0.42999999999999999"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "border_part_of_window = false"
        ), 1);
        QCOMPARE(occurrences(decorations, "rounding_power = 2.573"), 1);
        QCOMPARE(occurrences(decorations, "range = 17"), 1);
        QCOMPARE(occurrences(decorations, "render_power = 4"), 1);
        QCOMPARE(occurrences(decorations, "sharp = true"), 1);
        QCOMPARE(
            occurrences(decorations, "offset = {125.5, -80.25}"), 1
        );
        QCOMPARE(occurrences(decorations, "scale = 0.75"), 1);
        QCOMPARE(occurrences(
            decorations,
            "glow = {enabled = true, range = 11, render_power = 2}"
        ), 1);
        QCOMPARE(occurrences(decorations, "ignore_opacity = false"), 1);
        QCOMPARE(occurrences(decorations, "input_methods = true"), 1);
        QCOMPARE(occurrences(
            decorations, "input_methods_ignorealpha = 0.45000000000000001"
        ), 1);
        QCOMPARE(occurrences(decorations, "new_optimizations = false"), 1);
        QCOMPARE(occurrences(decorations, "passes = 5"), 1);
        QCOMPARE(occurrences(decorations, "popups = true"), 1);
        QCOMPARE(occurrences(
            decorations, "popups_ignorealpha = 0.34999999999999998"
        ), 1);
        QCOMPARE(occurrences(decorations, "size = 24"), 1);
        QCOMPARE(occurrences(decorations, "special = true"), 1);
        QCOMPARE(occurrences(decorations, "xray = true"), 1);
        QCOMPARE(occurrences(
            decorations, "brightness = 1.23456789012345"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "contrast = 0.87654321098764998"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "noise = 0.012345678901233999"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "vibrancy = 0.23456789012345"
        ), 1);
        QCOMPARE(occurrences(
            decorations, "vibrancy_darkness = 0.34567890123456002"
        ), 1);
        QVERIFY(!decorations.contains("dim = {"));

        const auto animations = rendered.value->files
                                    .value(QStringLiteral(
                                        "modules/51-animations.lua"))
                                    .contents;
        QVERIFY(animations.contains("animations = {enabled = false}"));

        auto dormantStateObject = defaults;
        dormantStateObject.insert(
            QStringLiteral("overrides"),
            QJsonObject{
                {QStringLiteral("hyprland.decoration.dim_strength"), 0.8},
                {QStringLiteral("hyprland.decoration.active_opacity"), 0.83},
                {QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61},
                {QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74},
                {QStringLiteral("hyprland.decoration.dim_modal"), false},
                {QStringLiteral("hyprland.decoration.dim_special"), 0.37},
                {QStringLiteral("hyprland.decoration.dim_around"), 0.43},
                {QStringLiteral("hyprland.decoration.blur.enabled"), false},
                {QStringLiteral("hyprland.decoration.blur.size"), 24},
                {QStringLiteral("hyprland.decoration.blur.passes"), 5},
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.ignore_opacity"
                    ),
                    false,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.new_optimizations"
                    ),
                    false,
                },
                {QStringLiteral("hyprland.decoration.blur.xray"), true},
                {QStringLiteral("hyprland.decoration.blur.special"), true},
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.popups_ignorealpha"
                    ),
                    0.35,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.input_methods_ignorealpha"
                    ),
                    0.45,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.brightness"),
                    1.23456789012345,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.contrast"),
                    0.87654321098765,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.noise"),
                    0.012345678901234,
                },
                {
                    QStringLiteral("hyprland.decoration.blur.vibrancy"),
                    0.23456789012345,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.blur.vibrancy_darkness"
                    ),
                    0.34567890123456,
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.border_part_of_window"
                    ),
                    false,
                },
                {
                    QStringLiteral("hyprland.decoration.rounding_power"),
                    2.573,
                },
                {QStringLiteral("hyprland.decoration.shadow.enabled"), false},
                {QStringLiteral("hyprland.decoration.shadow.range"), 19},
                {
                    QStringLiteral(
                        "hyprland.decoration.shadow.render_power"
                    ),
                    4,
                },
                {QStringLiteral("hyprland.decoration.shadow.sharp"), true},
                {
                    QStringLiteral("hyprland.decoration.shadow.offset"),
                    QJsonArray{-125.5, 80.25},
                },
                {QStringLiteral("hyprland.decoration.shadow.scale"), 0.0},
                {QStringLiteral("hyprland.decoration.glow.range"), 9},
                {
                    QStringLiteral("hyprland.decoration.glow.render_power"),
                    2,
                },
            }
        );
        const auto dormantParsed = parseState(dormantStateObject);
        QVERIFY2(
            dormantParsed,
            qPrintable(describeErrors(dormantParsed.errors))
        );
        const auto dormantRendered = render(
            *dormantParsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceB)),
            QDir(temporary.path()).filePath(
                QStringLiteral("user-custom.lua")),
            QString::fromLatin1(nonceB)
        );
        QVERIFY2(
            dormantRendered,
            qPrintable(describeErrors(dormantRendered.errors))
        );
        QCOMPARE(dormantRendered.value->activationRequirement,
                 ActivationRequirement::Reload);
        const auto dormantDecorations = dormantRendered.value->files
            .value(QStringLiteral("modules/50-decorations.lua")).contents;
        QVERIFY(!dormantDecorations.contains("dim_inactive"));
        QVERIFY(dormantDecorations.contains(QByteArrayLiteral(
            "hl.config({decoration = {active_opacity = "
            "0.82999999999999996, blur = {brightness = 1.23456789012345, "
            "contrast = 0.87654321098764998, enabled = false, ignore_opacity = "
            "false, input_methods_ignorealpha = 0.45000000000000001, "
            "new_optimizations = false, noise = 0.012345678901233999, "
            "passes = 5, popups_ignorealpha = 0.34999999999999998, size = 24, "
            "special = true, vibrancy = 0.23456789012345, "
            "vibrancy_darkness = 0.34567890123456002, xray = true}, "
            "border_part_of_window = false, dim_around = "
            "0.42999999999999999, dim_modal = false, "
            "dim_special = 0.37, dim_strength = "
            "0.80000000000000004, fullscreen_opacity = "
            "0.73999999999999999, glow = {range = 9, render_power = 2}, "
            "inactive_opacity = 0.60999999999999999, "
            "rounding_power = 2.573, shadow = {enabled = false, offset = "
            "{-125.5, 80.25}, range = 19, render_power = 4, scale = 0, "
            "sharp = true}}})"
        )));
        QVERIFY(dormantDecorations.contains(QByteArrayLiteral(
            "blur = {brightness = 1.23456789012345, contrast = "
            "0.87654321098764998, enabled = false, ignore_opacity = false, "
            "input_methods_ignorealpha = 0.45000000000000001, "
            "new_optimizations = false, noise = 0.012345678901233999, "
            "passes = 5, popups_ignorealpha = 0.34999999999999998, size = 24, "
            "special = true, vibrancy = 0.23456789012345, "
            "vibrancy_darkness = 0.34567890123456002, xray = true}"
        )));
        QVERIFY(!dormantDecorations.contains("input_methods ="));
        QVERIFY(!dormantDecorations.contains("popups ="));
        QVERIFY(dormantDecorations.contains(
            "dim_strength = 0.80000000000000004"
        ));
        QCOMPARE(occurrences(
            dormantDecorations, "dim_strength = 0.80000000000000004"
        ), 1);
        QVERIFY(dormantDecorations.contains(
            "active_opacity = 0.82999999999999996"
        ));
        QVERIFY(dormantDecorations.contains(
            "inactive_opacity = 0.60999999999999999"
        ));
        QVERIFY(dormantDecorations.contains(
            "fullscreen_opacity = 0.73999999999999999"
        ));
        QVERIFY(dormantDecorations.contains("dim_modal = false"));
        QVERIFY(dormantDecorations.contains("dim_special = 0.37"));
        QVERIFY(dormantDecorations.contains(
            "dim_around = 0.42999999999999999"
        ));
        QVERIFY(dormantDecorations.contains(
            "border_part_of_window = false"
        ));
        QVERIFY(dormantDecorations.contains("rounding_power = 2.573"));
        QVERIFY(dormantDecorations.contains(QByteArrayLiteral(
            "shadow = {enabled = false, offset = {-125.5, 80.25}, range = "
            "19, render_power = 4, scale = 0, sharp = true}"
        )));
        QVERIFY(dormantDecorations.contains(QByteArrayLiteral(
            "glow = {range = 9, render_power = 2}"
        )));
        QVERIFY(!dormantDecorations.contains(QByteArrayLiteral(
            "glow = {enabled ="
        )));
        QCOMPARE(occurrences(dormantDecorations,
                             "active_opacity = 0.82999999999999996"), 1);
        QCOMPARE(occurrences(dormantDecorations,
                             "inactive_opacity = 0.60999999999999999"), 1);
        QCOMPARE(occurrences(dormantDecorations,
                             "fullscreen_opacity = 0.73999999999999999"), 1);
        QCOMPARE(occurrences(dormantDecorations, "dim_modal = false"), 1);
        QCOMPARE(occurrences(dormantDecorations, "dim_special = 0.37"), 1);
        QCOMPARE(occurrences(
            dormantDecorations, "dim_around = 0.42999999999999999"
        ), 1);
        QCOMPARE(occurrences(
            dormantDecorations, "border_part_of_window = false"
        ), 1);
        QCOMPARE(
            occurrences(dormantDecorations, "rounding_power = 2.573"),
            1
        );
        QCOMPARE(occurrences(dormantDecorations, "range = 19"), 1);
        QCOMPARE(occurrences(dormantDecorations, "render_power = 4"), 1);
        QCOMPARE(occurrences(dormantDecorations, "sharp = true"), 1);
        QCOMPARE(
            occurrences(dormantDecorations, "offset = {-125.5, 80.25}"),
            1
        );
        QCOMPARE(occurrences(dormantDecorations, "scale = 0"), 1);
        QCOMPARE(occurrences(
            dormantDecorations, "brightness = 1.23456789012345"
        ), 1);
        QCOMPARE(occurrences(
            dormantDecorations, "contrast = 0.87654321098764998"
        ), 1);
        QCOMPARE(occurrences(
            dormantDecorations, "noise = 0.012345678901233999"
        ), 1);
        QCOMPARE(occurrences(
            dormantDecorations, "vibrancy = 0.23456789012345"
        ), 1);
        QCOMPARE(occurrences(
            dormantDecorations,
            "vibrancy_darkness = 0.34567890123456002"
        ), 1);
    }

    void rendersForeignGlowColorsAlongsideTheManagedTrio()
    {
        auto stateObject = defaults;
        stateObject.insert(
            QStringLiteral("overrides"),
            QJsonObject{
                {
                    QStringLiteral("hyprland.decoration.glow.color"),
                    QJsonObject{
                        {
                            QStringLiteral("colors"),
                            QJsonArray{
                                QStringLiteral("0xEE33CCFF"),
                                QStringLiteral("0xAA224466"),
                            },
                        },
                        {QStringLiteral("angle"), 19},
                    },
                },
                {
                    QStringLiteral(
                        "hyprland.decoration.glow.color_inactive"
                    ),
                    QJsonObject{
                        {
                            QStringLiteral("colors"),
                            QJsonArray{QStringLiteral("0x99446688")},
                        },
                        {QStringLiteral("angle"), 0},
                    },
                },
                {QStringLiteral("hyprland.decoration.glow.enabled"), true},
                {QStringLiteral("hyprland.decoration.glow.range"), 11},
                {
                    QStringLiteral("hyprland.decoration.glow.render_power"),
                    2,
                },
            }
        );
        const auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto rendered = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            QDir(temporary.path()).filePath(QStringLiteral("user-custom.lua"))
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto decorations = rendered.value->files.value(
            QStringLiteral("modules/50-decorations.lua")
        ).contents;
        const auto exactGlow = QByteArrayLiteral(
            "glow = {color = {angle = 19, colors = {\"0xEE33CCFF\", "
            "\"0xAA224466\"}}, color_inactive = {angle = 0, colors = "
            "{\"0x99446688\"}}, enabled = true, range = 11, "
            "render_power = 2}"
        );
        QVERIFY(decorations.contains(exactGlow));
        QCOMPARE(occurrences(decorations, exactGlow), 1);
    }

    void rejectsUnsafeOrAmbiguousPaths()
    {
        const auto parsed = parseState(defaults);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto goodRoot = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto goodCustom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );

        auto result = render(
            *parsed.value,
            QStringLiteral("relative/") + QString::fromLatin1(nonceA),
            goodCustom
        );
        QVERIFY(!result);
        QVERIFY(hasCode(result.errors, QStringLiteral("renderer.invalid-path")));

        result = render(
            *parsed.value,
            temporary.path() + QStringLiteral("/child/../")
                + QString::fromLatin1(nonceA),
            goodCustom
        );
        QVERIFY(!result);
        QVERIFY(hasCode(result.errors, QStringLiteral("renderer.invalid-path")));

        result = render(
            *parsed.value,
            temporary.path() + QStringLiteral("/e\u0301/")
                + QString::fromLatin1(nonceA),
            goodCustom
        );
        QVERIFY(!result);
        QVERIFY(hasCode(result.errors, QStringLiteral("renderer.invalid-path")));

        for (const auto separator : {QChar(0x2028), QChar(0x2029)}) {
            result = render(
                *parsed.value,
                goodRoot,
                temporary.path() + QLatin1Char('/') + separator
                    + QStringLiteral("user-custom.lua")
            );
            QVERIFY(!result);
            QVERIFY(hasCode(result.errors, QStringLiteral("renderer.invalid-path")));
        }

        result = render(
            *parsed.value,
            goodRoot,
            QDir(goodRoot).filePath(QStringLiteral("user-custom.lua"))
        );
        QVERIFY(!result);
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("renderer.custom-inside-generation")
        ));

        result = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceB)),
            goodCustom
        );
        QVERIFY(!result);
        QVERIFY(hasCode(
            result.errors,
            QStringLiteral("renderer.generation-root-mismatch")
        ));

        result = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QStringLiteral("ABC")),
            goodCustom,
            QStringLiteral("ABC")
        );
        QVERIFY(!result);
        QVERIFY(hasCode(result.errors, QStringLiteral("renderer.invalid-nonce")));

        result = render(
            *parsed.value,
            goodRoot,
            goodCustom,
            QString::fromLatin1(nonceA),
            QDateTime{}
        );
        QVERIFY(!result);
        QVERIFY(hasCode(result.errors, QStringLiteral("renderer.invalid-time")));
    }

    void escapesLuaStringsAndProducesValidLua()
    {
        auto stateObject = defaults;
        const auto authoredValue = QStringLiteral(
            "\"; _G.pwned = true; --\n\\tail\tSnowman: \u2603"
        );
        stateObject.insert(
            QStringLiteral("environment"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("hostile-value")},
                {QStringLiteral("name"), QStringLiteral("HOSTILE_VALUE")},
                {QStringLiteral("value"), authoredValue},
                {QStringLiteral("scope"), QStringLiteral("hyprland")},
            }}
        );
        const auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto hostileParent = QDir(temporary.path()).filePath(
            QStringLiteral("quoted \" path\\segment")
        );
        const auto root = QDir(hostileParent).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("custom \" file\\name.lua")
        );
        const auto rendered = render(*parsed.value, root, custom);
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));

        const auto environment = rendered.value->files
                                     .value(QStringLiteral("modules/20-environment.lua"))
                                     .contents;
        QVERIFY(environment.contains("\\\"; _G.pwned = true; --\\n\\\\tail\\t"));
        QVERIFY(!environment.contains("\n_G.pwned = true"));
        const auto loader = rendered.value->files
                                .value(QStringLiteral("hyprland.lua"))
                                .contents;
        QCOMPARE(
            occurrences(
                loader,
                QByteArrayLiteral("require(") + luaString(custom)
                    + QByteArrayLiteral(")")
            ),
            qsizetype(1)
        );

        const auto materializedRoot = QDir(temporary.path()).filePath(
            QStringLiteral("materialized")
        );
        for (auto iterator = rendered.value->files.constBegin();
             iterator != rendered.value->files.constEnd(); ++iterator) {
            const auto path = QDir(materializedRoot).filePath(iterator.key());
            QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::NewOnly));
            QCOMPARE(file.write(iterator->contents), iterator->contents.size());
            file.close();
            QCOMPARE(file.error(), QFileDevice::NoError);
            QProcess compiler;
            compiler.start(
                QStringLiteral(HYPRSHELLD_LUAC_EXECUTABLE),
                {QStringLiteral("-p"), path}
            );
            QVERIFY(compiler.waitForFinished());
            QCOMPARE(compiler.exitStatus(), QProcess::NormalExit);
            QCOMPARE(compiler.exitCode(), 0);
        }
    }

    void pinnedHyprlandAcceptsMaterializedGeneration()
    {
        auto representative = defaults;
        auto representativeOverrides = groupbarOverrides();
        QCOMPARE(representativeOverrides.size(), 35);
        representativeOverrides.insert(
            QStringLiteral("hyprland.general.border_size"), 2
        );
        representativeOverrides.insert(
            QStringLiteral("hyprland.general.col.active_border"),
            QJsonObject{
                {QStringLiteral("colors"),
                 QJsonArray{
                     QStringLiteral("0xFF112233"),
                     QStringLiteral("0xFF445566"),
                 }},
                {QStringLiteral("angle"), 37},
            }
        );
        representativeOverrides.insert(
            QStringLiteral("hyprland.general.gaps_in"),
            QJsonArray{1, 2, 3, 4}
        );
        representativeOverrides.insert(
            QStringLiteral("hyprland.general.gaps_out"),
            QJsonArray{0, 0, 0, 0}
        );
        representativeOverrides.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QJsonArray{8, 9, 10, 0}
        );
        representativeOverrides.insert(
            QStringLiteral("hyprland.decoration.shadow.offset"),
            QJsonArray{6, 7}
        );
        representative.insert(
            QStringLiteral("overrides"),
            representativeOverrides
        );
        representative.insert(
            QStringLiteral("monitors"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("monitor-one")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("mode"), QStringLiteral("preferred")},
                {QStringLiteral("position"), QStringLiteral("auto")},
                {QStringLiteral("scale"), 1.0},
                {QStringLiteral("reserved"), QJsonArray{1, 2, 3, 4}},
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
        representative.insert(
            QStringLiteral("devices"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("pointer-main")},
                {QStringLiteral("selector"), QStringLiteral("Main Pointer")},
                {QStringLiteral("kind"), QStringLiteral("pointer")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("overrides"), QJsonObject{
                    {QStringLiteral("sensitivity"), 0.25},
                }},
            }}
        );
        representative.insert(
            QStringLiteral("gestures"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("gesture-special")},
                {QStringLiteral("fingers"), 4},
                {QStringLiteral("direction"), QStringLiteral("left")},
                {QStringLiteral("modifiers"), QJsonArray{
                    QStringLiteral("super")
                }},
                {QStringLiteral("scale"), 1.25},
                {QStringLiteral("disableInhibit"), false},
                {QStringLiteral("action"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("special")},
                    {QStringLiteral("workspace"), QStringLiteral("magic")},
                }},
            }}
        );
        representative.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("workspace-one")},
                    {QStringLiteral("selector"), QStringLiteral("1")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("monitor"), QStringLiteral("DP-1")},
                    {QStringLiteral("persistent"), true},
                    {QStringLiteral("isDefault"), true},
                    {QStringLiteral("layout"), QStringLiteral("scrolling")},
                    {QStringLiteral("overrides"), QJsonObject{
                        {QStringLiteral("gaps_in"), QJsonArray{5, 6, 7, 8}},
                        {QStringLiteral("gaps_out"), QJsonArray{9, 10, 11, 12}},
                        {QStringLiteral("float_gaps"),
                         QJsonArray{13, 14, 15, 16}},
                        {QStringLiteral("border_size"), 9007199254740991.0},
                        {QStringLiteral("no_border"), true},
                        {QStringLiteral("no_rounding"), false},
                        {QStringLiteral("decorate"), true},
                        {QStringLiteral("no_shadow"), false},
                        {QStringLiteral("default_name"),
                         QStringLiteral("Authored workspace")},
                        {QStringLiteral("animation"),
                         QStringLiteral("slidefadevert left 37%")},
                        {QStringLiteral("layout_opts"), QJsonObject{
                            {QStringLiteral("orientation"),
                             QStringLiteral("center")},
                            {QStringLiteral("direction"),
                             QStringLiteral("up")},
                        }},
                    }},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("workspace-two")},
                    {QStringLiteral("selector"),
                     QStringLiteral("special:music")},
                    {QStringLiteral("enabled"), false},
                    {QStringLiteral("monitor"), QString()},
                    {QStringLiteral("persistent"), false},
                    {QStringLiteral("isDefault"), false},
                    {QStringLiteral("layout"), QString()},
                    {QStringLiteral("overrides"), QJsonObject{}},
                },
                QJsonObject{
                    {QStringLiteral("id"),
                     QStringLiteral(
                         "hyprshelld.internal.shared-spacing.maximized"
                     )},
                    {QStringLiteral("selector"), QStringLiteral("f[1]")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("monitor"), QString()},
                    {QStringLiteral("persistent"), false},
                    {QStringLiteral("isDefault"), false},
                    {QStringLiteral("layout"), QString()},
                    {QStringLiteral("overrides"), QJsonObject{
                        {QStringLiteral("gaps_out"), QJsonArray{0, 0, 0, 0}},
                    }},
                },
            }
        );
        representative.insert(
            QStringLiteral("curves"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("curve-bezier")},
                {QStringLiteral("name"), QStringLiteral("ease-custom")},
                {QStringLiteral("type"), QStringLiteral("bezier")},
                {QStringLiteral("points"), QJsonArray{
                    QJsonArray{0.2, 0.0},
                    QJsonArray{0.8, 1.0},
                }},
            }}
        );
        representative.insert(
            QStringLiteral("animations"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("animation-windows")},
                {QStringLiteral("name"), QStringLiteral("windows")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("speed"), 6.0},
                {QStringLiteral("curve"), QStringLiteral("ease-custom")},
                {QStringLiteral("style"), QStringLiteral("slide")},
            }}
        );
        representative.insert(
            QStringLiteral("windowRules"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("window-browser")},
                {QStringLiteral("name"), QStringLiteral("Browser")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("match"), QJsonObject{
                    {QStringLiteral("class"), QStringLiteral("^(firefox)$")},
                }},
                {QStringLiteral("effects"), QJsonObject{
                    {QStringLiteral("float"), true},
                    {QStringLiteral("rounding"), 4},
                }},
            }}
        );
        representative.insert(
            QStringLiteral("layerRules"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("layer-panel")},
                {QStringLiteral("name"), QStringLiteral("Panel")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("match"), QJsonObject{
                    {QStringLiteral("namespace"), QStringLiteral("^panel$")},
                }},
                {QStringLiteral("effects"), QJsonObject{
                    {QStringLiteral("ignore_alpha"), 0.5},
                    {QStringLiteral("above_lock"), 1},
                }},
            }}
        );
        representative.insert(
            QStringLiteral("submaps"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("resize-submap")},
                {QStringLiteral("name"), QStringLiteral("resize")},
                {QStringLiteral("reset"), QString()},
                {QStringLiteral("enabled"), true},
            }}
        );
        representative.insert(
            QStringLiteral("bindings"),
            QJsonArray{
                dispatcherBinding(
                    QStringLiteral("move-cursor"), QStringLiteral("F7")
                ),
                dispatcherBinding(
                    QStringLiteral("resize-move"),
                    QStringLiteral("F8"),
                    QStringLiteral("resize")
                ),
            }
        );
        representative.insert(
            QStringLiteral("permissions"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("permission-one")},
                {QStringLiteral("binary"), QStringLiteral("^/usr/bin/foo$")},
                {QStringLiteral("type"), QStringLiteral("screencopy")},
                {QStringLiteral("mode"), QStringLiteral("deny")},
            }}
        );
        representative.insert(
            QStringLiteral("environment"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("cursor-size")},
                {QStringLiteral("name"), QStringLiteral("XCURSOR_SIZE")},
                {QStringLiteral("value"), QStringLiteral("24")},
                {QStringLiteral("scope"), QStringLiteral("hyprland")},
            }}
        );
        const auto parsed = parseState(representative);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto rendered = render(*parsed.value, root, custom);
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const QVector<QPair<QString, QByteArray>> expectedApis{
            {QStringLiteral("modules/10-monitors.lua"), QByteArrayLiteral("hl.monitor(")},
            {QStringLiteral("modules/20-environment.lua"), QByteArrayLiteral("hl.env(")},
            {QStringLiteral("modules/30-input.lua"), QByteArrayLiteral("hl.device(")},
            {QStringLiteral("modules/31-gestures.lua"), QByteArrayLiteral("hl.gesture(")},
            {QStringLiteral("modules/40-general.lua"), QByteArrayLiteral("hl.config(")},
            {QStringLiteral("modules/42-workspaces.lua"), QByteArrayLiteral("hl.workspace_rule(")},
            {QStringLiteral("modules/43-groups.lua"), QByteArrayLiteral("hl.config(")},
            {QStringLiteral("modules/51-animations.lua"), QByteArrayLiteral("hl.curve(")},
            {QStringLiteral("modules/51-animations.lua"), QByteArrayLiteral("hl.animation(")},
            {QStringLiteral("modules/60-rules.lua"), QByteArrayLiteral("hl.window_rule(")},
            {QStringLiteral("modules/60-rules.lua"), QByteArrayLiteral("hl.layer_rule(")},
            {QStringLiteral("modules/70-keybinds.lua"), QByteArrayLiteral("hl.define_submap(")},
            {QStringLiteral("modules/70-keybinds.lua"), QByteArrayLiteral("hl.bind(")},
            {QStringLiteral("modules/80-permissions.lua"), QByteArrayLiteral("hl.permission(")},
        };
        for (const auto &[path, api] : expectedApis) {
            QVERIFY2(rendered.value->files.value(path).contents.contains(api),
                     qPrintable(path));
        }
        const auto monitorModule = rendered.value->files
                                       .value(QStringLiteral(
                                           "modules/10-monitors.lua"
                                       ))
                                       .contents;
        QVERIFY(monitorModule.contains(
            "reserved = {bottom = 3, left = 4, right = 2, top = 1}"
        ));
        const auto generalModule = rendered.value->files
                                       .value(QStringLiteral(
                                           "modules/40-general.lua"
                                       ))
                                       .contents;
        QCOMPARE(
            generalModule,
            QByteArray(managedWarningLine) + '\n'
                + QByteArrayLiteral("-- Put custom configuration in ")
                + custom.toUtf8()
                + QByteArrayLiteral(
                    "; it is loaded last.\n\n"
                    "hl.config({general = {border_size = 2, "
                    "col = {active_border = {angle = 37, colors = "
                    "{\"0xFF112233\", \"0xFF445566\"}}}, "
                    "float_gaps = {bottom = 10, left = 0, right = 9, top = 8}, "
                    "gaps_in = {bottom = 3, left = 4, right = 2, top = 1}, "
                    "gaps_out = {bottom = 0, left = 0, right = 0, top = 0}}})\n"
                )
        );
        const auto decorationModule = rendered.value->files
                                          .value(QStringLiteral(
                                              "modules/50-decorations.lua"
                                          ))
                                          .contents;
        QCOMPARE(
            decorationModule,
            QByteArray(managedWarningLine) + '\n'
                + QByteArrayLiteral("-- Put custom configuration in ")
                + custom.toUtf8()
                + QByteArrayLiteral(
                    "; it is loaded last.\n\n"
                    "hl.config({decoration = {shadow = {offset = {6, 7}}}})\n"
                )
        );
        const auto workspaceModule = rendered.value->files
                                         .value(QStringLiteral(
                                             "modules/42-workspaces.lua"
                                         ))
                                         .contents;
        QVERIFY(workspaceModule.contains(
            "float_gaps = {bottom = 15, left = 16, right = 14, top = 13}"
        ));
        QVERIFY(workspaceModule.contains(
            "gaps_in = {bottom = 7, left = 8, right = 6, top = 5}"
        ));
        QVERIFY(workspaceModule.contains(
            "gaps_out = {bottom = 11, left = 12, right = 10, top = 9}"
        ));
        QCOMPARE(
            occurrences(workspaceModule, QByteArrayLiteral("hl.workspace_rule(")),
            qsizetype(3)
        );
        const auto firstWorkspace = workspaceModule.indexOf(
            QByteArrayLiteral("workspace = \"1\"")
        );
        const auto secondWorkspace = workspaceModule.indexOf(
            QByteArrayLiteral("workspace = \"special:music\"")
        );
        const auto protectedWorkspace = workspaceModule.indexOf(
            QByteArrayLiteral("workspace = \"f[1]\"")
        );
        QVERIFY(firstWorkspace >= 0);
        QVERIFY(secondWorkspace > firstWorkspace);
        QVERIFY(protectedWorkspace > secondWorkspace);
        QVERIFY(workspaceModule.contains(
            QByteArrayLiteral("animation = \"slidefadevert left 37%\"")
        ));
        QVERIFY(workspaceModule.contains(
            QByteArrayLiteral("border_size = 9007199254740991")
        ));
        QVERIFY(workspaceModule.contains(
            QByteArrayLiteral("default_name = \"Authored workspace\"")
        ));
        QVERIFY(workspaceModule.contains(
            QByteArrayLiteral("decorate = true")
        ));
        QVERIFY(workspaceModule.contains(
            QByteArrayLiteral(
                "layout_opts = {direction = \"up\", orientation = \"center\"}"
            )
        ));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("no_border = true")));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("no_rounding = false")));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("no_shadow = false")));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("default = true")));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("layout = \"scrolling\"")));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("monitor = \"DP-1\"")));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("persistent = true")));
        QVERIFY(workspaceModule.contains(QByteArrayLiteral("enabled = false")));
        const auto groupModule = rendered.value->files
                                     .value(QStringLiteral(
                                         "modules/43-groups.lua"
                                     ))
                                     .contents;
        QVERIFY(groupModule.contains(
            QByteArrayLiteral("font_weight_active = 650")
        ));
        QVERIFY(groupModule.contains(
            QByteArrayLiteral("font_weight_inactive = 325")
        ));
        QVERIFY(groupModule.contains(
            QByteArrayLiteral("rounding_power = 2.573")
        ));
        QVERIFY(groupModule.contains(
            QByteArrayLiteral("text_color_locked_inactive = \"0xFFA4B5C6\"")
        ));
        QVERIFY(groupModule.contains(
            QByteArrayLiteral(
                "locked_inactive = {angle = 44, colors = "
                "{\"0xFF314253\", \"0xFF647586\"}}"
            )
        ));
        const auto protectedGap = workspaceModule.lastIndexOf(
            QByteArrayLiteral(
                "gaps_out = {bottom = 0, left = 0, right = 0, top = 0}"
            )
        );
        QVERIFY(protectedGap > secondWorkspace);
        QVERIFY(protectedGap < protectedWorkspace);

        QVERIFY(QDir().mkpath(root));
        for (auto iterator = rendered.value->files.constBegin();
             iterator != rendered.value->files.constEnd(); ++iterator) {
            const auto path = QDir(root).filePath(iterator.key());
            QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::NewOnly));
            QCOMPARE(file.write(iterator->contents), iterator->contents.size());
            file.close();
            QCOMPARE(file.error(), QFileDevice::NoError);
        }
        QFile customFile(custom);
        QVERIFY(customFile.open(QIODevice::WriteOnly | QIODevice::NewOnly));
        customFile.close();
        QCOMPARE(customFile.error(), QFileDevice::NoError);

        const auto exactEntrypoint = rendered.value->files
                                         .value(QStringLiteral("hyprland.lua"))
                                         .contents;
        QVERIFY(exactEntrypoint.contains(
            "if not cmdline then return true end"
        ));
        QVERIFY(exactEntrypoint.contains(
            "if not arguments or not closed then return true end"
        ));
        QVERIFY(exactEntrypoint.contains(
            "if #arguments == 0 or string.byte(arguments, -1) ~= 0 then return true end"
        ));
        QVERIFY(exactEntrypoint.contains("local parsed = 0"));
        QVERIFY(exactEntrypoint.contains("parsed = parsed + 1"));
        QVERIFY(exactEntrypoint.contains("return parsed == 0"));
        QVERIFY(exactEntrypoint.contains(
            "if argument == \"--verify-config\" then return true end"
        ));
        QVERIFY(exactEntrypoint.contains(
            "if hyprshelld_is_verifier() or not hyprshelld_runtime_ready() then return end"
        ));
        QVERIFY(exactEntrypoint.contains(
            "local function hyprshelld_read_bounded(path)"
        ));
        QVERIFY(exactEntrypoint.contains(
            "local function hyprshelld_runtime_ready()"
        ));
        QVERIFY(exactEntrypoint.contains(
            "runtime .. \"/hypr/\" .. signature .. \"/hyprland.lock\""
        ));
        QVERIFY(exactEntrypoint.contains(
            "local pid = string.match(stat, \"^(%d+) %(\")"
        ));
        QVERIFY(exactEntrypoint.contains(
            "local lock_pid = string.match(lock, \"^(%d+)\\n\")"
        ));
        QCOMPARE(
            occurrences(
                exactEntrypoint,
                QByteArrayLiteral("hl.dsp.event(\"hyprshelld:")
                    + QByteArray(nonceA) + QByteArrayLiteral("\")")
            ),
            qsizetype(1)
        );

        const auto guardStart = exactEntrypoint.indexOf(
            "local function hyprshelld_is_verifier()"
        );
        const auto guardEnd = exactEntrypoint.indexOf(
            "\n\nhl.on(\"config.reloaded\"",
            guardStart
        );
        QVERIFY(guardStart >= 0);
        QVERIFY(guardEnd > guardStart);
        const auto guard = exactEntrypoint.mid(
            guardStart,
            guardEnd - guardStart
        );
        const auto guardScript = QDir(temporary.path()).filePath(
            QStringLiteral("guard.lua")
        );
        QVERIFY(writeFile(guardScript, guard + "\nprint(hyprshelld_is_verifier())\n"));
        const auto runGuard = [&guardScript](const QStringList &arguments) {
            QProcess process;
            process.start(
                QStringLiteral(HYPRSHELLD_LUA_EXECUTABLE),
                QStringList{guardScript} + arguments
            );
            if (!process.waitForFinished(10000)
                || process.exitStatus() != QProcess::NormalExit
                || process.exitCode() != 0) {
                return QByteArray{};
            }
            return process.readAllStandardOutput().trimmed();
        };
        QCOMPARE(runGuard({}), QByteArrayLiteral("false"));
        QCOMPARE(runGuard({QStringLiteral("--verify-config")}),
                 QByteArrayLiteral("true"));
        QCOMPARE(runGuard({QStringLiteral("prefix--verify-config")}),
                 QByteArrayLiteral("false"));
        QCOMPARE(runGuard({QStringLiteral("/tmp/--verify-config")}),
                 QByteArrayLiteral("false"));

        const auto failedInspectionScript = QDir(temporary.path()).filePath(
            QStringLiteral("guard-failed-inspection.lua")
        );
        QVERIFY(writeFile(
            failedInspectionScript,
            QByteArrayLiteral("io.open = function() return nil end\n")
                + guard + QByteArrayLiteral(
                    "\nprint(hyprshelld_is_verifier())\n"
                )
        ));
        QProcess failedInspection;
        failedInspection.start(
            QStringLiteral(HYPRSHELLD_LUA_EXECUTABLE),
            {failedInspectionScript}
        );
        QVERIFY(failedInspection.waitForFinished(10000));
        QCOMPARE(failedInspection.exitCode(), 0);
        QCOMPARE(failedInspection.readAllStandardOutput().trimmed(),
                 QByteArrayLiteral("true"));

        const auto runGuardWithPayload = [
            &guard,
            &temporary
        ](const QString &name, const QByteArray &payloadExpression) {
            const auto script = QDir(temporary.path()).filePath(name);
            const auto source = QByteArrayLiteral("local payload = ")
                + payloadExpression
                + QByteArrayLiteral(
                    "\nio.open = function()\n"
                    "    return {\n"
                    "        read = function() return payload end,\n"
                    "        close = function() return true end,\n"
                    "    }\n"
                    "end\n"
                )
                + guard
                + QByteArrayLiteral(
                    "\nprint(hyprshelld_is_verifier())\n"
                );
            if (!writeFile(script, source)) {
                return QByteArray{};
            }
            QProcess process;
            process.start(QStringLiteral(HYPRSHELLD_LUA_EXECUTABLE), {script});
            if (!process.waitForFinished(10000)
                || process.exitStatus() != QProcess::NormalExit
                || process.exitCode() != 0) {
                return QByteArray{};
            }
            return process.readAllStandardOutput().trimmed();
        };
        QCOMPARE(runGuardWithPayload(
                     QStringLiteral("guard-empty-cmdline.lua"),
                     QByteArrayLiteral("\"\"")
                 ),
                 QByteArrayLiteral("true"));
        QCOMPARE(runGuardWithPayload(
                     QStringLiteral("guard-unterminated-cmdline.lua"),
                     QByteArrayLiteral("\"Hyprland --verify-config\"")
                 ),
                 QByteArrayLiteral("true"));
        QCOMPARE(runGuardWithPayload(
                     QStringLiteral("guard-zero-argv.lua"),
                     QByteArrayLiteral("string.char(0)")
                 ),
                 QByteArrayLiteral("true"));

        QProcess verifier;
        verifier.setProcessChannelMode(QProcess::MergedChannels);
        auto environment = QProcessEnvironment::systemEnvironment();
        const auto home = QDir(temporary.path()).filePath(QStringLiteral("home"));
        const auto runtime = QDir(temporary.path()).filePath(
            QStringLiteral("runtime")
        );
        const auto state = QDir(temporary.path()).filePath(QStringLiteral("state"));
        const auto cache = QDir(temporary.path()).filePath(QStringLiteral("cache"));
        const auto config = QDir(temporary.path()).filePath(
            QStringLiteral("config")
        );
        for (const auto &path : {home, runtime, state, cache, config}) {
            QVERIFY(QDir().mkpath(path));
            QVERIFY(::chmod(QFile::encodeName(path).constData(), 0700) == 0);
        }
        environment.insert(QStringLiteral("HOME"), home);
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
        environment.insert(QStringLiteral("XDG_STATE_HOME"), state);
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), cache);
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
        verifier.setProcessEnvironment(environment);
        verifier.start(
            QStringLiteral(HYPRSHELLD_HYPRLAND_EXECUTABLE),
            {
                QStringLiteral("--verify-config"),
                QStringLiteral("--config"),
                QDir(root).filePath(QStringLiteral("hyprland.lua")),
            }
        );
        QVERIFY(verifier.waitForFinished(30000));
        const auto output = verifier.readAll();
        QVERIFY2(
            verifier.exitStatus() == QProcess::NormalExit,
            output.constData()
        );
        QVERIFY2(verifier.exitCode() == 0, output.constData());
        QVERIFY2(!output.contains("Config error"), output.constData());
    }

    void preservesWindowThenLayerRuleOrderAndEmitsDisabledRecordsWithoutIds()
    {
        auto stateObject = defaults;
        stateObject.insert(
            QStringLiteral("windowRules"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("window-first-id")},
                    {QStringLiteral("name"), QStringLiteral("Window first")},
                    {QStringLiteral("enabled"), false},
                    {QStringLiteral("match"), QJsonObject{
                        {QStringLiteral("class"), QStringLiteral("^first$")},
                    }},
                    {QStringLiteral("effects"), QJsonObject{
                        {QStringLiteral("float"), true},
                    }},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("window-second-id")},
                    {QStringLiteral("name"), QStringLiteral("Window second")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("match"), QJsonObject{
                        {QStringLiteral("class"), QStringLiteral("^second$")},
                    }},
                    {QStringLiteral("effects"), QJsonObject{
                        {QStringLiteral("maximize"), true},
                    }},
                },
            }
        );
        stateObject.insert(
            QStringLiteral("layerRules"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("layer-first-id")},
                    {QStringLiteral("name"), QStringLiteral("Layer first")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("match"), QJsonObject{
                        {QStringLiteral("namespace"), QStringLiteral("^first$")},
                    }},
                    {QStringLiteral("effects"), QJsonObject{
                        {QStringLiteral("blur"), true},
                    }},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("layer-second-id")},
                    {QStringLiteral("name"), QStringLiteral("Layer second")},
                    {QStringLiteral("enabled"), false},
                    {QStringLiteral("match"), QJsonObject{
                        {QStringLiteral("namespace"), QStringLiteral("^second$")},
                    }},
                    {QStringLiteral("effects"), QJsonObject{
                        {QStringLiteral("order"), 2},
                    }},
                },
            }
        );
        const auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto rendered = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            QDir(temporary.path()).filePath(QStringLiteral("user-custom.lua"))
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto rules = rendered.value->files.value(
            QStringLiteral("modules/60-rules.lua")
        ).contents;
        const auto windowFirst = rules.indexOf("name = \"Window first\"");
        const auto windowSecond = rules.indexOf("name = \"Window second\"");
        const auto layerFirst = rules.indexOf("name = \"Layer first\"");
        const auto layerSecond = rules.indexOf("name = \"Layer second\"");
        QVERIFY(windowFirst >= 0);
        QVERIFY(windowFirst < windowSecond);
        QVERIFY(windowSecond < layerFirst);
        QVERIFY(layerFirst < layerSecond);
        QCOMPARE(occurrences(rules, "hl.window_rule("), qsizetype(2));
        QCOMPARE(occurrences(rules, "hl.layer_rule("), qsizetype(2));
        QCOMPARE(occurrences(rules, "enabled = false"), qsizetype(2));
        QVERIFY(!rules.contains("window-first-id"));
        QVERIFY(!rules.contains("window-second-id"));
        QVERIFY(!rules.contains("layer-first-id"));
        QVERIFY(!rules.contains("layer-second-id"));
    }

    void activationNonceWaitsForExactRuntimeLock()
    {
        const auto parsed = parseState(defaults);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto rendered = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            QDir(temporary.path()).filePath(QStringLiteral("user-custom.lua"))
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto loader = rendered.value->files
                                .value(QStringLiteral("hyprland.lua"))
                                .contents;
        const auto helperStart = loader.indexOf(
            "local function hyprshelld_is_verifier()"
        );
        const auto callbackEnd = loader.indexOf(
            "\nend)\n\n", loader.indexOf("hl.on(\"config.reloaded\"")
        );
        QVERIFY(helperStart >= 0);
        QVERIFY(callbackEnd > helperStart);
        const auto guardedCallback = loader.mid(
            helperStart, callbackEnd + qsizetype(7) - helperStart
        );

        const auto runGuard = [
            &temporary,
            &guardedCallback
        ](
            const QString &name,
            const QString &runtime,
            const QString &signature,
            const QByteArray &statExpression,
            const QByteArray &lockExpression
        ) {
            const auto script = QDir(temporary.path()).filePath(name);
            auto source = QByteArrayLiteral("local fake_runtime = ")
                + luaString(runtime)
                + QByteArrayLiteral("\nlocal fake_signature = ")
                + luaString(signature)
                + QByteArrayLiteral("\nlocal fake_stat = ")
                + statExpression
                + QByteArrayLiteral("\nlocal fake_lock = ")
                + lockExpression
                + QByteArrayLiteral(
                    "\nlocal fake_cmdline = \"lua\" .. string.char(0)\n"
                    "os.getenv = function(name)\n"
                    "    if name == \"XDG_RUNTIME_DIR\" then return fake_runtime end\n"
                    "    if name == \"HYPRLAND_INSTANCE_SIGNATURE\" then return fake_signature end\n"
                    "    return nil\n"
                    "end\n"
                    "io.open = function(path, mode)\n"
                    "    local payload = nil\n"
                    "    if path == \"/proc/self/cmdline\" then payload = fake_cmdline\n"
                    "    elseif path == \"/proc/self/stat\" then payload = fake_stat\n"
                    "    elseif path == fake_runtime .. \"/hypr/\" .. fake_signature .. \"/hyprland.lock\" then payload = fake_lock end\n"
                    "    if payload == nil then return nil end\n"
                    "    return {\n"
                    "        read = function(_, limit)\n"
                    "            if type(limit) == \"number\" then return string.sub(payload, 1, limit) end\n"
                    "            return payload\n"
                    "        end,\n"
                    "        close = function() return true end,\n"
                    "    }\n"
                    "end\n"
                    "local callback = nil\n"
                    "local emitted = nil\n"
                    "hl = {\n"
                    "    on = function(name, value) if name == \"config.reloaded\" then callback = value end end,\n"
                    "    dispatch = function(value) emitted = value end,\n"
                    "    dsp = {event = function(value) return value end},\n"
                    "}\n"
                )
                + guardedCallback
                + QByteArrayLiteral(
                    "callback()\n"
                    "if emitted then print(emitted) end\n"
                );
            if (!writeFile(script, source)) return QByteArray{};
            QProcess process;
            process.start(QStringLiteral(HYPRSHELLD_LUA_EXECUTABLE), {script});
            if (!process.waitForFinished(10000)
                || process.exitStatus() != QProcess::NormalExit
                || process.exitCode() != 0) {
                return QByteArrayLiteral("process-failed:")
                    + process.readAllStandardError();
            }
            return process.readAllStandardOutput().trimmed();
        };

        const auto stat = QByteArrayLiteral("\"4242 (Hyprland) S\"");
        const auto exactLock = QByteArrayLiteral(
            "\"4242\" .. string.char(10) .. \"wayland-1\" .. string.char(10)"
        );
        const auto expected = QByteArrayLiteral("hyprshelld:")
            + QByteArray(nonceA);
        QCOMPARE(
            runGuard(
                QStringLiteral("runtime-ready.lua"),
                QStringLiteral("/run/user/1000"),
                QStringLiteral("instance_1"), stat, exactLock
            ),
            expected
        );
        QCOMPARE(
            runGuard(
                QStringLiteral("runtime-no-lock.lua"),
                QStringLiteral("/run/user/1000"),
                QStringLiteral("instance_1"), stat, QByteArrayLiteral("nil")
            ),
            QByteArray{}
        );
        QCOMPARE(
            runGuard(
                QStringLiteral("runtime-wrong-pid.lua"),
                QStringLiteral("/run/user/1000"),
                QStringLiteral("instance_1"), stat,
                QByteArrayLiteral(
                    "\"4243\" .. string.char(10) .. \"wayland-1\" .. string.char(10)"
                )
            ),
            QByteArray{}
        );
        QCOMPARE(
            runGuard(
                QStringLiteral("runtime-unsafe-root.lua"),
                QStringLiteral("/run/user/../1000"),
                QStringLiteral("instance_1"), stat, exactLock
            ),
            QByteArray{}
        );
        QCOMPARE(
            runGuard(
                QStringLiteral("runtime-unsafe-signature.lua"),
                QStringLiteral("/run/user/1000"),
                QStringLiteral("../instance"), stat, exactLock
            ),
            QByteArray{}
        );
        QCOMPARE(
            runGuard(
                QStringLiteral("runtime-oversized-lock.lua"),
                QStringLiteral("/run/user/1000"),
                QStringLiteral("instance_1"), stat,
                QByteArrayLiteral("string.rep(\"4\", 4097)")
            ),
            QByteArray{}
        );
    }

    void declaresSubmapsBeforeTopLevelBindings()
    {
        auto stateObject = defaults;
        stateObject.insert(
            QStringLiteral("submaps"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("resize-submap")},
                {QStringLiteral("name"), QStringLiteral("resize")},
                {QStringLiteral("reset"), QString()},
                {QStringLiteral("enabled"), true},
            }}
        );
        stateObject.insert(
            QStringLiteral("bindings"),
            QJsonArray{
                dispatcherBinding(
                    QStringLiteral("top-level"), QStringLiteral("F7")
                ),
                dispatcherBinding(
                    QStringLiteral("inside-submap"),
                    QStringLiteral("F8"),
                    QStringLiteral("resize")
                ),
            }
        );
        const auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto rendered = render(
            *parsed.value,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            QDir(temporary.path()).filePath(QStringLiteral("user-custom.lua"))
        );
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto keybinds = rendered.value->files
                                  .value(QStringLiteral("modules/70-keybinds.lua"))
                                  .contents;
        const auto declaration = keybinds.indexOf("hl.define_submap(\"resize\"");
        const auto topLevel = keybinds.indexOf("hl.bind(\"SUPER + F7\"");
        QVERIFY(declaration >= 0);
        QVERIFY(topLevel >= 0);
        QVERIFY(declaration < topLevel);
        QVERIFY(keybinds.indexOf("hl.bind(\"SUPER + F8\"", declaration)
                < topLevel);
        QCOMPARE(
            rendered.value->activationRequirement,
            ActivationRequirement::Restart
        );
    }

    void activationRequirementIsStrongestAndNeverNone()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );

        auto parsed = parseState(defaults);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        auto rendered = render(*parsed.value, root, custom);
        QVERIFY(rendered);
        QCOMPARE(rendered.value->activationRequirement,
                 ActivationRequirement::Reload);

        auto deviceObject = defaults;
        deviceObject.insert(
            QStringLiteral("devices"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("device-a")},
                {QStringLiteral("selector"),
                 QStringLiteral("main-keyboard")},
                {QStringLiteral("kind"), QStringLiteral("keyboard")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("overrides"),
                 QJsonObject{{QStringLiteral("sensitivity"), 0.25}}},
            }}
        );
        parsed = parseState(deviceObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        rendered = render(*parsed.value, root, custom);
        QVERIFY(rendered);
        QCOMPARE(rendered.value->activationRequirement,
                 ActivationRequirement::Restart);

        auto bindingObject = defaults;
        bindingObject.insert(
            QStringLiteral("bindings"),
            QJsonArray{dispatcherBinding(
                QStringLiteral("close-window"), QStringLiteral("F7")
            )}
        );
        parsed = parseState(bindingObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        rendered = render(*parsed.value, root, custom);
        QVERIFY(rendered);
        QCOMPARE(rendered.value->activationRequirement,
                 ActivationRequirement::Restart);

        auto restartObject = defaults;
        restartObject.insert(
            QStringLiteral("permissions"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("portal-copy")},
                {QStringLiteral("binary"), QStringLiteral("xdg-desktop-portal")},
                {QStringLiteral("type"), QStringLiteral("screencopy")},
                {QStringLiteral("mode"), QStringLiteral("ask")},
            }}
        );
        parsed = parseState(restartObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        rendered = render(*parsed.value, root, custom);
        QVERIFY(rendered);
        QCOMPARE(rendered.value->activationRequirement,
                 ActivationRequirement::Restart);

        auto sessionObject = deviceObject;
        sessionObject.insert(
            QStringLiteral("environment"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("desktop")},
                {QStringLiteral("name"), QStringLiteral("XDG_CURRENT_DESKTOP")},
                {QStringLiteral("value"), QStringLiteral("Hyprland")},
                {QStringLiteral("scope"), QStringLiteral("hyprland")},
            }}
        );
        parsed = parseState(sessionObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        rendered = render(*parsed.value, root, custom);
        QVERIFY(rendered);
        QCOMPARE(rendered.value->activationRequirement,
                 ActivationRequirement::Session);
        QVERIFY(rendered.value->files
                    .value(QStringLiteral("modules/20-environment.lua"))
                    .contents.contains("hl.env(\"XDG_CURRENT_DESKTOP\", \"Hyprland\")"));
    }

    void deferredUwsmAndBrokerActionsFailClosed()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );

        auto stateObject = defaults;
        stateObject.insert(
            QStringLiteral("environment"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("uwsm-only")},
                {QStringLiteral("name"), QStringLiteral("SSH_AUTH_SOCK")},
                {QStringLiteral("value"), QStringLiteral("/run/user/1000/agent")},
                {QStringLiteral("scope"), QStringLiteral("uwsm")},
            }}
        );
        auto parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        auto rendered = render(*parsed.value, root, custom);
        QVERIFY(!rendered);
        QVERIFY(hasCode(
            rendered.errors,
            QStringLiteral("renderer.uwsm-unavailable")
        ));

        stateObject = defaults;
        stateObject.insert(
            QStringLiteral("bindings"),
            QJsonArray{brokerBinding(true)}
        );
        parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        rendered = render(*parsed.value, root, custom);
        QVERIFY(!rendered);
        QVERIFY(hasCode(
            rendered.errors,
            QStringLiteral("renderer.broker-unavailable")
        ));

        stateObject.insert(
            QStringLiteral("bindings"),
            QJsonArray{brokerBinding(false)}
        );
        parsed = parseState(stateObject);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        rendered = render(*parsed.value, root, custom);
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        QCOMPARE(
            rendered.value->activationRequirement,
            ActivationRequirement::Restart
        );
        QVERIFY(!rendered.value->files
                     .value(QStringLiteral("modules/70-keybinds.lua"))
                     .contents.contains("hl.bind("));
    }

    void dormantV2ManifestBindsExactAuthorityClosure()
    {
        QCOMPARE(currentRendererVersion, quint32(1));
        QCOMPARE(dormantGenerationV2FormatVersion, quint32(2));
        QCOMPARE(dormantGenerationV2ContractVersion, quint32(2));
        QCOMPARE(dormantRendererV2Version, quint32(2));

        const auto v1State = parseState(defaults);
        const auto v2State = dormantState();
        QVERIFY2(v1State, qPrintable(describeErrors(v1State.errors)));
        QVERIFY2(v2State, qPrintable(describeErrors(v2State.errors)));

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto active = render(*v1State.value, root, custom);
        const auto dormant = renderDormant(*v2State.value, root, custom);
        QVERIFY2(active, qPrintable(describeErrors(active.errors)));
        QVERIFY2(dormant, qPrintable(describeErrors(dormant.errors)));
        QVERIFY(dormant.value->files == active.value->files);
        QCOMPARE(dormant.value->files.size(), 17);
        QCOMPARE(
            dormant.value->activationRequirement,
            active.value->activationRequirement
        );

        const auto &manifest = dormant.value->manifest;
        QCOMPARE(manifest.size(), 16);
        QCOMPARE(manifest.value(QStringLiteral("formatVersion")).toInt(), 2);
        QCOMPARE(manifest.value(QStringLiteral("contractVersion")).toInt(), 2);
        QCOMPARE(manifest.value(QStringLiteral("rendererVersion")).toInt(), 2);
        QCOMPARE(
            manifest.value(QStringLiteral("authorityId")).toString(),
            QString::fromLatin1(authorityA)
        );
        QCOMPARE(
            manifest.value(QStringLiteral("targetHyprland")).toString(),
            QStringLiteral("0.56.2")
        );
        const auto compatible = manifest
                                    .value(QStringLiteral("compatibleHyprland"))
                                    .toObject();
        QCOMPARE(compatible.size(), 5);
        QCOMPARE(compatible.value(QStringLiteral("major")).toInt(), 0);
        QCOMPARE(compatible.value(QStringLiteral("minor")).toInt(), 56);
        QCOMPARE(
            compatible.value(QStringLiteral("reviewedVersion")).toString(),
            QStringLiteral("0.56.2")
        );
        QCOMPARE(compatible.value(QStringLiteral("minimumPatch")).toInt(), 2);
        QCOMPARE(compatible.value(QStringLiteral("maximumPatch")).toInt(), 2);

        const auto expectedSource = QStringLiteral(
            "f67f9214c47770268e66dd43d94b3af68dcbcac701312edcd52eedffb157f60d"
        );
        const auto expectedCatalog = QStringLiteral(
            "3158d318945aeb03728426412933d737b5cf9cbd1dc384e296c11a9628ff6b88"
        );
        const auto expectedActions = QStringLiteral(
            "3625e37617810539823ae829de80eb5488b06b0e71d4c0be1f0356e00d019db8"
        );
        QCOMPARE(dormantCatalogV2.sourceManifestDigest, expectedSource);
        QCOMPARE(dormantActionCatalogV2.sourceManifestDigest, expectedSource);
        QCOMPARE(dormant.value->sourceManifestDigest, expectedSource);
        QCOMPARE(
            manifest.value(QStringLiteral("sourceManifestDigest")).toString(),
            expectedSource
        );
        QCOMPARE(dormantCatalogV2.digest, expectedCatalog);
        QCOMPARE(dormantActionCatalogV2.digest, expectedActions);
        QCOMPARE(
            sha256(readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE
            ))),
            QStringLiteral(
                "00121b3294fc7845a2fd3aa4740dd5251d789cbdbf4aef57bf8f646adbec2ebd"
            )
        );
        QCOMPARE(
            sha256(JsonSupport::canonicalJson(dormantTemplateV2)),
            QStringLiteral(
                "e126e8a7164823b745e30067b7c2f4f8936ec96d92f33e82f5decb75c2cd8ccb"
            )
        );
        QCOMPARE(
            manifest.value(QStringLiteral("catalogDigest")).toString(),
            expectedCatalog
        );
        QCOMPARE(
            manifest.value(QStringLiteral("actionCatalogDigest")).toString(),
            expectedActions
        );
        QCOMPARE(
            sha256(JsonSupport::canonicalJson(
                dormantSourceManifestV2
            )),
            expectedSource
        );
        QCOMPARE(
            sha256(readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_V2_SOURCE_MANIFEST_FILE
            ))),
            QStringLiteral(
                "09965e7626da69910c1d16e856baba3859cf06d9f8a14896a9b8a6e06cfe4619"
            )
        );

        const auto dispatcherSha = QStringLiteral(
            "a109eeb982856e0fe2ac9d88c29115a09984511787e19a20e7b4804e14a9d4de"
        );
        QCOMPARE(
            dormantActionCatalogV2.source.path,
            QStringLiteral(
                "src/config/lua/bindings/LuaBindingsDispatchers.cpp"
            )
        );
        QCOMPARE(dormantActionCatalogV2.source.sha256, dispatcherSha);
        bool qualifiedSourceMatched = false;
        for (const auto &value : dormantSourceManifestV2
                                     .value(QStringLiteral("qualifiedSources"))
                                     .toArray()) {
            const auto object = value.toObject();
            if (object.value(QStringLiteral("path")).toString()
                == dormantActionCatalogV2.source.path) {
                QCOMPARE(
                    object.value(QStringLiteral("upstreamSha256")).toString(),
                    dispatcherSha
                );
                QCOMPARE(
                    object.value(QStringLiteral("effectiveSha256")).toString(),
                    dispatcherSha
                );
                qualifiedSourceMatched = true;
            }
        }
        QVERIFY(qualifiedSourceMatched);
        bool changedPathMatched = false;
        for (const auto &value : dormantSourceManifestV2
                                     .value(QStringLiteral("changedPaths"))
                                     .toArray()) {
            const auto object = value.toObject();
            if (object.value(QStringLiteral("path")).toString()
                == dormantActionCatalogV2.source.path) {
                QCOMPARE(
                    object.value(QStringLiteral("postimageSha256")).toString(),
                    dispatcherSha
                );
                changedPathMatched = true;
            }
        }
        QVERIFY(changedPathMatched);
        QVERIFY(dispatcherSha != expectedSource);

        const auto serialized = serializeDormantDesiredStateV2(*v2State.value);
        QVERIFY2(serialized, qPrintable(describeErrors(serialized.errors)));
        auto canonicalDesired = *serialized.value;
        QVERIFY(canonicalDesired.endsWith('\n'));
        const auto digestIncludingNewline = sha256(canonicalDesired);
        canonicalDesired.chop(1);
        QCOMPARE(dormant.value->snapshotDigest, sha256(canonicalDesired));
        QVERIFY(dormant.value->snapshotDigest != digestIncludingNewline);

        auto generationInput = manifest;
        generationInput.remove(QStringLiteral("generation"));
        QCOMPARE(
            dormant.value->generation,
            sha256(JsonSupport::canonicalJson(generationInput))
        );
        auto canonicalManifest =
            JsonSupport::canonicalJson(manifest);
        canonicalManifest.append('\n');
        QCOMPARE(dormant.value->manifestBytes, canonicalManifest);
        const auto validation = validateDormantGenerationV2(
            *dormant.value,
            *v2State.value,
            dormantCatalogV2,
            dormantActionCatalogV2
        );
        QVERIFY2(validation.isEmpty(), qPrintable(describeErrors(validation)));

        const auto &protectedRule =
            v2State.value->semanticState.workspaceRules.constFirst();
        QVERIFY(protectedRule.overrides.contains(QStringLiteral("gaps_out")));
        QVERIFY(!protectedRule.overrides.contains(
            QStringLiteral("float_gaps")
        ));
        const auto workspaces = dormant.value->files
                                    .value(QStringLiteral(
                                        "modules/42-workspaces.lua"
                                    ))
                                    .contents;
        QVERIFY(workspaces.contains("gaps_out"));
        QVERIFY(!workspaces.contains("float_gaps"));

        const auto activeRejectsV2Authorities = renderGeneration(
            v2State.value->semanticState,
            dormantCatalogV2,
            dormantActionCatalogV2,
            root,
            custom,
            QString::fromLatin1(nonceA),
            fixedTime()
        );
        QVERIFY(!activeRejectsV2Authorities);
        QVERIFY(hasCode(
            activeRejectsV2Authorities.errors,
            QStringLiteral("state.active-v1-catalog-authority-required")
        ));
        QVERIFY(hasCode(
            activeRejectsV2Authorities.errors,
            QStringLiteral("state.active-v1-action-authority-required")
        ));
        QCOMPARE(active.value->manifest.value(
            QStringLiteral("formatVersion")
        ).toInt(), 1);
        QVERIFY(!active.value->manifest.contains(QStringLiteral("authorityId")));
        QVERIFY(!active.value->manifest.contains(
            QStringLiteral("sourceManifestDigest")
        ));
    }

    void dormantV2AuthorityChangesSnapshotAndGenerationOnly()
    {
        const auto stateA = dormantState(QString::fromLatin1(authorityA));
        const auto stateB = dormantState(QString::fromLatin1(authorityB));
        QVERIFY2(stateA, qPrintable(describeErrors(stateA.errors)));
        QVERIFY2(stateB, qPrintable(describeErrors(stateB.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto renderedA = renderDormant(*stateA.value, root, custom);
        const auto renderedB = renderDormant(*stateB.value, root, custom);
        QVERIFY2(renderedA, qPrintable(describeErrors(renderedA.errors)));
        QVERIFY2(renderedB, qPrintable(describeErrors(renderedB.errors)));
        QVERIFY(renderedA.value->files == renderedB.value->files);
        QCOMPARE(
            renderedA.value->sourceManifestDigest,
            renderedB.value->sourceManifestDigest
        );
        QCOMPARE(
            renderedA.value->manifest.value(
                QStringLiteral("catalogDigest")
            ),
            renderedB.value->manifest.value(
                QStringLiteral("catalogDigest")
            )
        );
        QCOMPARE(
            renderedA.value->manifest.value(
                QStringLiteral("actionCatalogDigest")
            ),
            renderedB.value->manifest.value(
                QStringLiteral("actionCatalogDigest")
            )
        );
        QVERIFY(renderedA.value->snapshotDigest
                != renderedB.value->snapshotDigest);
        QVERIFY(renderedA.value->generation != renderedB.value->generation);
        QCOMPARE(renderedA.value->activationNonce,
                 QString::fromLatin1(nonceA));
        QCOMPARE(renderedB.value->activationNonce,
                 QString::fromLatin1(nonceA));
        QVERIFY(renderedA.value->authorityId
                != renderedA.value->activationNonce);
        QVERIFY(validateDormantGenerationV2(
            *renderedA.value,
            *stateA.value,
            dormantCatalogV2,
            dormantActionCatalogV2
        ).isEmpty());
        QVERIFY(validateDormantGenerationV2(
            *renderedB.value,
            *stateB.value,
            dormantCatalogV2,
            dormantActionCatalogV2
        ).isEmpty());
    }

    void dormantV2ValidatorRejectsEveryBoundMutation()
    {
        const auto state = dormantState();
        QVERIFY2(state, qPrintable(describeErrors(state.errors)));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = QDir(temporary.path()).filePath(
            QString::fromLatin1(nonceA)
        );
        const auto custom = QDir(temporary.path()).filePath(
            QStringLiteral("user-custom.lua")
        );
        const auto rendered = renderDormant(*state.value, root, custom);
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));
        const auto rejects = [&](
                                 const DormantRenderedGenerationV2 &candidate,
                                 const DesiredStateV2 &candidateState
                             ) {
            return !validateDormantGenerationV2(
                        candidate,
                        candidateState,
                        dormantCatalogV2,
                        dormantActionCatalogV2
                    )
                        .isEmpty();
        };
        const auto recanonicalize = [](DormantRenderedGenerationV2 &value) {
            value.manifestBytes =
                JsonSupport::canonicalJson(value.manifest);
            value.manifestBytes.append('\n');
        };

        for (const auto &field : {
                 QStringLiteral("formatVersion"),
                 QStringLiteral("contractVersion"),
                 QStringLiteral("rendererVersion"),
             }) {
            auto changed = *rendered.value;
            changed.manifest.insert(field, 1);
            recanonicalize(changed);
            QVERIFY(rejects(changed, *state.value));
        }
        for (const auto &field : {
                 QStringLiteral("sourceManifestDigest"),
                 QStringLiteral("catalogDigest"),
                 QStringLiteral("actionCatalogDigest"),
                 QStringLiteral("snapshotDigest"),
                 QStringLiteral("generation"),
             }) {
            auto changed = *rendered.value;
            changed.manifest.insert(field, QString(64, QLatin1Char('0')));
            recanonicalize(changed);
            QVERIFY(rejects(changed, *state.value));
        }
        auto changed = *rendered.value;
        changed.manifest.insert(
            QStringLiteral("authorityId"), QString::fromLatin1(authorityB)
        );
        recanonicalize(changed);
        QVERIFY(rejects(changed, *state.value));

        changed = *rendered.value;
        changed.manifest.insert(
            QStringLiteral("activationNonce"), QString(32, QLatin1Char('0'))
        );
        recanonicalize(changed);
        QVERIFY(rejects(changed, *state.value));

        changed = *rendered.value;
        changed.manifest.insert(
            QStringLiteral("targetHyprland"), QStringLiteral("0.56.x")
        );
        recanonicalize(changed);
        QVERIFY(rejects(changed, *state.value));

        changed = *rendered.value;
        auto compatible = changed.manifest
                              .value(QStringLiteral("compatibleHyprland"))
                              .toObject();
        compatible.insert(QStringLiteral("maximumPatch"), QJsonValue::Null);
        changed.manifest.insert(
            QStringLiteral("compatibleHyprland"), compatible
        );
        recanonicalize(changed);
        QVERIFY(rejects(changed, *state.value));

        changed = *rendered.value;
        auto files = changed.manifest.value(QStringLiteral("files")).toObject();
        const auto firstPath = files.constBegin().key();
        auto metadata = files.value(firstPath).toObject();
        metadata.insert(
            QStringLiteral("size"),
            metadata.value(QStringLiteral("size")).toInteger() + 1
        );
        files.insert(firstPath, metadata);
        changed.manifest.insert(QStringLiteral("files"), files);
        recanonicalize(changed);
        QVERIFY(rejects(changed, *state.value));

        changed = *rendered.value;
        changed.files[firstPath].contents.append('x');
        QVERIFY(rejects(changed, *state.value));

        changed = *rendered.value;
        changed.manifestBytes = QJsonDocument(changed.manifest).toJson(
            QJsonDocument::Indented
        );
        QVERIFY(rejects(changed, *state.value));

        const auto otherState = dormantState(
            QString::fromLatin1(authorityB)
        );
        QVERIFY(otherState);
        QVERIFY(rejects(*rendered.value, *otherState.value));

        auto changedCatalog = dormantCatalogV2;
        changedCatalog.sourceManifestDigest = QString(64, QLatin1Char('0'));
        QVERIFY(!validateDormantGenerationV2(
            *rendered.value,
            *state.value,
            changedCatalog,
            dormantActionCatalogV2
        ).isEmpty());
        changedCatalog = dormantCatalogV2;
        changedCatalog.digest = QString(64, QLatin1Char('0'));
        QVERIFY(!validateDormantGenerationV2(
            *rendered.value,
            *state.value,
            changedCatalog,
            dormantActionCatalogV2
        ).isEmpty());

        auto changedActions = dormantActionCatalogV2;
        changedActions.sourceManifestDigest = QString(64, QLatin1Char('0'));
        QVERIFY(!validateDormantGenerationV2(
            *rendered.value,
            *state.value,
            dormantCatalogV2,
            changedActions
        ).isEmpty());
        changedActions = dormantActionCatalogV2;
        changedActions.digest = QString(64, QLatin1Char('0'));
        QVERIFY(!validateDormantGenerationV2(
            *rendered.value,
            *state.value,
            dormantCatalogV2,
            changedActions
        ).isEmpty());
        changedActions = dormantActionCatalogV2;
        changedActions.source.sha256 = QString(64, QLatin1Char('0'));
        QVERIFY(!validateDormantGenerationV2(
            *rendered.value,
            *state.value,
            dormantCatalogV2,
            changedActions
        ).isEmpty());

        for (const auto &invalidNonce : {
                 QString(32, QLatin1Char('0')),
                 QString(64, QLatin1Char('a')),
                 QStringLiteral("0123456789ABCDEF0123456789ABCDEF"),
             }) {
            const auto invalidRoot = QDir(temporary.path()).filePath(
                invalidNonce
            );
            const auto invalid = renderDormant(
                *state.value, invalidRoot, custom, invalidNonce
            );
            QVERIFY(!invalid);
            QVERIFY(hasCode(
                invalid.errors, QStringLiteral("renderer-v2.invalid-nonce")
            ));
        }
    }

    void dormantV2GenerationFixtureHasExactCanonicalDigestChain()
    {
        const auto rawFixture = readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V2_GENERATION_MANIFEST_FILE
        ));
        QCOMPARE(
            sha256(rawFixture),
            QStringLiteral(
                "bd2760fa05f81cb169e1df923c471b8caf6790567f4876384df39c1ca7210fce"
            )
        );
        const auto fixtureObject = readObject(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V2_GENERATION_MANIFEST_FILE
        ));
        QVERIFY(!fixtureObject.isEmpty());
        QCOMPARE(
            fixtureObject.value(QStringLiteral("snapshotDigest")).toString(),
            QStringLiteral(
                "d0ba9d6640ee281789416bbed615ec64dee418175a09dab58393feb542db20cd"
            )
        );
        QCOMPARE(
            fixtureObject.value(QStringLiteral("generation")).toString(),
            QStringLiteral(
                "d5a4ab2f71d396d2a16b0dfa2ad3c647b583bf4c5f0c6f9f24a0e1b3934957f6"
            )
        );
        auto generationInput = fixtureObject;
        generationInput.remove(QStringLiteral("generation"));
        QCOMPARE(
            fixtureObject.value(QStringLiteral("generation")).toString(),
            sha256(JsonSupport::canonicalJson(generationInput))
        );

        const auto authorityId = fixtureObject
                                     .value(QStringLiteral("authorityId"))
                                     .toString();
        const auto state = dormantState(authorityId);
        QVERIFY2(state, qPrintable(describeErrors(state.errors)));
        DormantRenderedGenerationV2 fixture;
        fixture.authorityId = authorityId;
        fixture.generation = fixtureObject
                                 .value(QStringLiteral("generation"))
                                 .toString();
        fixture.snapshotDigest = fixtureObject
                                     .value(QStringLiteral("snapshotDigest"))
                                     .toString();
        fixture.sourceManifestDigest = fixtureObject
                                           .value(QStringLiteral(
                                               "sourceManifestDigest"
                                           ))
                                           .toString();
        fixture.activationNonce = fixtureObject
                                      .value(QStringLiteral("activationNonce"))
                                      .toString();
        fixture.createdAt = fixtureObject
                                .value(QStringLiteral("createdAt"))
                                .toString();
        fixture.entrypoint = fixtureObject
                                 .value(QStringLiteral("entrypoint"))
                                 .toString();
        fixture.manifest = fixtureObject;
        fixture.manifestBytes =
            JsonSupport::canonicalJson(fixtureObject);
        fixture.manifestBytes.append('\n');
        QVERIFY(fixture.manifestBytes != rawFixture);
        const auto fixtureFiles = fixtureObject
                                      .value(QStringLiteral("files"))
                                      .toObject();
        QCOMPARE(fixtureFiles.size(), 1);
        QCOMPARE(fixture.entrypoint, QStringLiteral("hyprland.lua"));
        GeneratedFile emptyEntrypoint{
            .path = fixture.entrypoint,
            .contents = {},
            .sha256 = sha256(QByteArrayView{}),
            .size = 0,
        };
        fixture.files.insert(fixture.entrypoint, emptyEntrypoint);
        const auto validation = validateDormantGenerationV2(
            fixture,
            *state.value,
            dormantCatalogV2,
            dormantActionCatalogV2
        );
        QVERIFY2(validation.isEmpty(), qPrintable(describeErrors(validation)));
    }

    void readOnlyCompatibilityStateCannotRender()
    {
        const auto parsed = parseState(defaults);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        auto readOnly = *parsed.value;
        readOnly.readOnly = true;
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto rendered = render(
            readOnly,
            QDir(temporary.path()).filePath(QString::fromLatin1(nonceA)),
            QDir(temporary.path()).filePath(QStringLiteral("user-custom.lua"))
        );
        QVERIFY(!rendered);
        QVERIFY(hasCode(
            rendered.errors,
            QStringLiteral("renderer.read-only-state")
        ));
    }
};

QTEST_MAIN(CompositorRendererTest)

#include "compositor_renderer_test.moc"
