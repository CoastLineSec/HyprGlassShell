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

} // namespace

class CompositorRendererTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalog;
    ActionCatalog actionCatalog;
    QJsonObject defaults;

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
        QCOMPARE(generation.activationRequirement, ActivationRequirement::Reload);
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
                {QStringLiteral("hyprland.decoration.rounding"), 7},
                {QStringLiteral("hyprland.decoration.shadow.enabled"), false},
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
        QVERIFY(decorations.contains("blur = {enabled = false}"));
        QVERIFY(decorations.contains("rounding = 7"));
        QVERIFY(decorations.contains("shadow = {enabled = false}"));

        const auto animations = rendered.value->files
                                    .value(QStringLiteral(
                                        "modules/51-animations.lua"))
                                    .contents;
        QVERIFY(animations.contains("animations = {enabled = false}"));
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
        representative.insert(
            QStringLiteral("overrides"),
            QJsonObject{{QStringLiteral("hyprland.general.border_size"), 2}}
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
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("workspace-one")},
                {QStringLiteral("selector"), QStringLiteral("1")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("monitor"), QString()},
                {QStringLiteral("persistent"), true},
                {QStringLiteral("isDefault"), false},
                {QStringLiteral("layout"), QStringLiteral("dwindle")},
                {QStringLiteral("overrides"), QJsonObject{
                    {QStringLiteral("gaps_in"), QJsonArray{5, 6, 7, 8}},
                    {QStringLiteral("gaps_out"), QJsonArray{9, 10, 11, 12}},
                    {QStringLiteral("float_gaps"),
                     QJsonArray{13, 14, 15, 16}},
                }},
            }}
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

        auto sessionObject = restartObject;
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
        QVERIFY(!rendered.value->files
                     .value(QStringLiteral("modules/70-keybinds.lua"))
                     .contents.contains("hl.bind("));
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
