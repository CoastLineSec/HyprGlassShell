#include "hyprland/keyboard_shortcut_reference.h"

#include "hyprland/json_support.h"

#include <QFile>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>

using namespace HyprShelld::Hyprland;

namespace {

[[nodiscard]] QByteArray artifactBytes()
{
    QFile file(QString::fromUtf8(HYPRSHELLD_LEGACY_SHORTCUT_REFERENCE_FILE));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] bool hasCode(
    const ValidationErrors &errors,
    const QStringView code
)
{
    return std::ranges::any_of(errors, [code](const ValidationError &error) {
        return error.code == code;
    });
}

[[nodiscard]] QString describeErrors(const ValidationErrors &errors)
{
    QStringList descriptions;
    for (const auto &error : errors) {
        descriptions.append(error.path + QLatin1Char(':') + error.code);
    }
    return descriptions.join(QStringLiteral(", "));
}

[[nodiscard]] QByteArray canonical(const QJsonObject &root)
{
    auto bytes = JsonSupport::canonicalJson(root);
    bytes.append('\n');
    return bytes;
}

} // namespace

class KeyboardShortcutReferenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void embeddedReferenceMatchesCompiledReceiptAndSourceOrder()
    {
        const auto parsed = embeddedKeyboardShortcutReference();
        QVERIFY2(parsed.ok(), qPrintable(describeErrors(parsed.errors)));
        QCOMPARE(
            parsed.value->sourceDigest,
            QString::fromLatin1(legacyShortcutSourceDigest)
        );
        QCOMPARE(
            parsed.value->artifactDigest,
            QString::fromLatin1(legacyShortcutArtifactDigest)
        );
        QCOMPARE(parsed.value->rows.size(), legacyShortcutReferenceRows);

        const auto &terminal = parsed.value->rows.at(0);
        QCOMPARE(terminal.ordinal, 1);
        QCOMPARE(terminal.sourceLine, 8);
        QCOMPARE(terminal.chord, QStringLiteral("SUPER + T"));
        QCOMPARE(
            terminal.action,
            QStringLiteral("hl.dsp.exec_cmd(\"hgs-terminal\")")
        );

        const auto &settings = parsed.value->rows.at(5);
        QCOMPARE(settings.ordinal, 6);
        QCOMPARE(settings.chord, QStringLiteral("SUPER + comma"));
        QCOMPARE(
            settings.action,
            QStringLiteral(
                "hl.dsp.exec_cmd(\"hgs ipc call settings focusOrToggle\")"
            )
        );
        QVERIFY(!settings.options.locked.has_value());
        QVERIFY(!settings.options.repeating.has_value());
        QVERIFY(!settings.options.mouse.has_value());
        QVERIFY(!settings.options.description.has_value());

        const auto &first = parsed.value->rows.at(49);
        QCOMPARE(first.chord, QStringLiteral("SUPER + Home"));
        QCOMPARE(
            first.action,
            QStringLiteral("hl.dsp.focus({ window = \"first\" })")
        );
        const auto &last = parsed.value->rows.at(50);
        QCOMPARE(last.chord, QStringLiteral("SUPER + End"));
        QCOMPARE(
            last.action,
            QStringLiteral("hl.dsp.focus({ window = \"last\" })")
        );

        const auto &dpms = parsed.value->rows.constLast();
        QCOMPARE(dpms.ordinal, 117);
        QCOMPARE(dpms.sourceLine, 171);
        QCOMPARE(dpms.chord, QStringLiteral("SUPER + SHIFT + P"));
        QCOMPARE(
            dpms.action,
            QStringLiteral("hl.dsp.dpms({ action = \"toggle\" })")
        );
    }

    void preservesEveryAuthoredOptionWithoutInventingDefaults()
    {
        const auto parsed = embeddedKeyboardShortcutReference();
        QVERIFY2(parsed.ok(), qPrintable(describeErrors(parsed.errors)));

        int locked = 0;
        int repeating = 0;
        int both = 0;
        int mouse = 0;
        int descriptions = 0;
        for (const auto &row : parsed.value->rows) {
            const auto isLocked = row.options.locked.value_or(false);
            const auto isRepeating = row.options.repeating.value_or(false);
            locked += isLocked ? 1 : 0;
            repeating += isRepeating ? 1 : 0;
            both += isLocked && isRepeating ? 1 : 0;
            mouse += row.options.mouse.value_or(false) ? 1 : 0;
            descriptions += row.options.description.has_value() ? 1 : 0;
        }
        QCOMPARE(locked, 12);
        QCOMPARE(repeating, 10);
        QCOMPARE(both, 6);
        QCOMPARE(mouse, 2);
        QCOMPARE(descriptions, 4);

        const auto &drag = parsed.value->rows.at(104);
        QCOMPARE(drag.chord, QStringLiteral("SUPER + mouse:272"));
        QVERIFY(drag.options.mouse == std::optional<bool>(true));
        QVERIFY(
            drag.options.description
            == std::optional<QString>(QStringLiteral("Move window"))
        );
        const auto &resize = parsed.value->rows.at(105);
        QCOMPARE(resize.chord, QStringLiteral("SUPER + mouse:273"));
        QVERIFY(resize.options.mouse == std::optional<bool>(true));
        QVERIFY(
            resize.options.description
            == std::optional<QString>(QStringLiteral("Resize window"))
        );
    }

    void sourceArtifactAndEmbeddedResourceAreIdentical()
    {
        const auto bytes = artifactBytes();
        QCOMPARE(bytes.size(), legacyShortcutArtifactBytes);
        const auto parsed = parseKeyboardShortcutReference(bytes);
        QVERIFY2(parsed.ok(), qPrintable(describeErrors(parsed.errors)));
        const auto embedded = embeddedKeyboardShortcutReference();
        QVERIFY2(embedded.ok(), qPrintable(describeErrors(embedded.errors)));
        QVERIFY(parsed.value == embedded.value);
    }

    void rejectsAnyTamperedArtifactEvenWhenTheMutationIsWellFormed()
    {
        const auto document = QJsonDocument::fromJson(artifactBytes());
        QVERIFY(document.isObject());
        auto root = document.object();
        auto rows = root.value(QStringLiteral("rows")).toArray();
        auto first = rows.at(0).toObject();
        first.insert(QStringLiteral("chord"), QStringLiteral("SUPER + X"));
        rows.replace(0, first);
        root.insert(QStringLiteral("rows"), rows);

        const auto mutated = canonical(root);
        QVERIFY(mutated != artifactBytes());
        QVERIFY(
            QCryptographicHash::hash(mutated, QCryptographicHash::Sha256).toHex()
            != QByteArray(legacyShortcutArtifactDigest)
        );
        QVERIFY(
            QCryptographicHash::hash(
                QByteArrayView(mutated),
                QCryptographicHash::Sha256
            ).toHex()
            != QByteArray(legacyShortcutArtifactDigest)
        );
        QCOMPARE(
            root.value(QStringLiteral("rows")).toArray().at(0).toObject()
                .value(QStringLiteral("chord")).toString(),
            QStringLiteral("SUPER + X")
        );
        const auto parsed = parseKeyboardShortcutReference(mutated);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.artifact-digest-mismatch")
        ));
    }

    void rejectsUnknownMissingReorderedAndNonCanonicalRepresentations()
    {
        const auto document = QJsonDocument::fromJson(artifactBytes());
        QVERIFY(document.isObject());

        auto unknown = document.object();
        unknown.insert(QStringLiteral("unknown"), true);
        auto parsed = parseKeyboardShortcutReference(canonical(unknown));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-fields")
        ));

        auto falseOption = document.object();
        auto falseRows = falseOption.value(QStringLiteral("rows")).toArray();
        auto brightness = falseRows.at(25).toObject();
        auto options = brightness.value(QStringLiteral("options")).toObject();
        options.insert(QStringLiteral("locked"), false);
        brightness.insert(QStringLiteral("options"), options);
        falseRows.replace(25, brightness);
        falseOption.insert(QStringLiteral("rows"), falseRows);
        parsed = parseKeyboardShortcutReference(canonical(falseOption));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-option")
        ));

        auto missing = document.object();
        auto rows = missing.value(QStringLiteral("rows")).toArray();
        auto first = rows.at(0).toObject();
        first.remove(QStringLiteral("action"));
        rows.replace(0, first);
        missing.insert(QStringLiteral("rows"), rows);
        parsed = parseKeyboardShortcutReference(canonical(missing));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-fields")
        ));

        auto reordered = document.object();
        rows = reordered.value(QStringLiteral("rows")).toArray();
        const auto row0 = rows.at(0);
        rows.replace(0, rows.at(1));
        rows.replace(1, row0);
        reordered.insert(QStringLiteral("rows"), rows);
        parsed = parseKeyboardShortcutReference(canonical(reordered));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-order")
        ));

        auto nonCanonical = artifactBytes();
        nonCanonical.append(' ');
        parsed = parseKeyboardShortcutReference(nonCanonical);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.non-canonical")
        ));

        const QByteArray oversized(64 * 1024 + 1, ' ');
        parsed = parseKeyboardShortcutReference(oversized);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(parsed.errors, QStringView(u"json.size-limit")));
        QVERIFY(!hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.artifact-digest-mismatch")
        ));
    }

    void rejectsInvalidVersionReceiptCountTypesTextAndDuplicateKeys()
    {
        const auto document = QJsonDocument::fromJson(artifactBytes());
        QVERIFY(document.isObject());

        auto version = document.object();
        version.insert(QStringLiteral("contractVersion"), 2);
        auto parsed = parseKeyboardShortcutReference(canonical(version));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.unsupported-version")
        ));

        auto receipt = document.object();
        auto source = receipt.value(QStringLiteral("source")).toObject();
        source.insert(QStringLiteral("sha256"), QString(64, QLatin1Char('0')));
        receipt.insert(QStringLiteral("source"), source);
        parsed = parseKeyboardShortcutReference(canonical(receipt));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-source")
        ));

        auto count = document.object();
        auto rows = count.value(QStringLiteral("rows")).toArray();
        rows.removeLast();
        count.insert(QStringLiteral("rows"), rows);
        parsed = parseKeyboardShortcutReference(canonical(count));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-count")
        ));

        auto type = document.object();
        rows = type.value(QStringLiteral("rows")).toArray();
        auto first = rows.at(0).toObject();
        first.insert(QStringLiteral("section"), false);
        rows.replace(0, first);
        type.insert(QStringLiteral("rows"), rows);
        parsed = parseKeyboardShortcutReference(canonical(type));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-text")
        ));

        auto excessive = document.object();
        rows = excessive.value(QStringLiteral("rows")).toArray();
        first = rows.at(0).toObject();
        first.insert(QStringLiteral("action"), QString(2049, QLatin1Char('x')));
        rows.replace(0, first);
        excessive.insert(QStringLiteral("rows"), rows);
        parsed = parseKeyboardShortcutReference(canonical(excessive));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-text")
        ));

        auto control = document.object();
        rows = control.value(QStringLiteral("rows")).toArray();
        first = rows.at(0).toObject();
        first.insert(QStringLiteral("chord"), QStringLiteral("SUPER\nT"));
        rows.replace(0, first);
        control.insert(QStringLiteral("rows"), rows);
        parsed = parseKeyboardShortcutReference(canonical(control));
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.invalid-text")
        ));

        auto duplicate = artifactBytes();
        QVERIFY(duplicate.contains(QByteArray("\"contractVersion\":1")));
        duplicate.replace(
            QByteArray("\"contractVersion\":1"),
            QByteArray("\"contractVersion\":1,\"contractVersion\":1")
        );
        parsed = parseKeyboardShortcutReference(duplicate);
        QVERIFY(!parsed.ok());
        QVERIFY(hasCode(parsed.errors, QStringView(u"json.duplicate-key")));

        parsed = parseKeyboardShortcutReference(QByteArrayView("{"));
        QVERIFY(!parsed.ok());
        QVERIFY(!parsed.errors.isEmpty());
        QVERIFY(!hasCode(
            parsed.errors,
            QStringView(u"legacy-shortcuts.artifact-digest-mismatch")
        ));
    }
};

QTEST_MAIN(KeyboardShortcutReferenceTest)

#include "keyboard_shortcut_reference_test.moc"
