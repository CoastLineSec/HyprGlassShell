#include "hyprland/input_device_inventory.h"

#include "hyprland/json_support.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtTest>

#include <algorithm>
#include <optional>

using namespace HyprShelld::Hyprland;

namespace {

constexpr auto runtimeIdentity = "runtime-a";
constexpr auto serviceEpoch = "epoch-a";

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

[[nodiscard]] QByteArray compact(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

[[nodiscard]] QByteArray canonicalDocument(const QJsonObject &object)
{
    auto result = JsonSupport::canonicalJson(object);
    result.append('\n');
    return result;
}

[[nodiscard]] QJsonObject pointerRecord(
    const QString &address = QStringLiteral("0x20"),
    const QString &selector = QStringLiteral("pointer-main")
)
{
    return {
        {QStringLiteral("address"), address},
        {QStringLiteral("name"), selector},
        {QStringLiteral("defaultSpeed"), 0.25},
        {QStringLiteral("scrollFactor"), 1.0},
    };
}

[[nodiscard]] QJsonObject keyboardRecord(
    const QString &address = QStringLiteral("0x10"),
    const QString &selector = QStringLiteral("keyboard-main"),
    const QJsonValue &layoutIndex = QJsonValue(QJsonValue::Null),
    const QString &activeKeymap = QStringLiteral("English (US)")
)
{
    return {
        {QStringLiteral("address"), address},
        {QStringLiteral("name"), selector},
        {QStringLiteral("rules"), QString()},
        {QStringLiteral("model"), QString()},
        {QStringLiteral("layout"), QStringLiteral("us")},
        {QStringLiteral("variant"), QString()},
        {QStringLiteral("options"), QString()},
        {QStringLiteral("active_layout_index"), layoutIndex},
        {QStringLiteral("active_keymap"), activeKeymap},
        {QStringLiteral("capsLock"), false},
        {QStringLiteral("numLock"), true},
        {QStringLiteral("main"), true},
    };
}

[[nodiscard]] QJsonObject tabletRecord(
    const QString &address = QStringLiteral("0x30"),
    const QString &selector = QStringLiteral("tablet-main")
)
{
    return {
        {QStringLiteral("address"), address},
        {QStringLiteral("name"), selector},
    };
}

[[nodiscard]] QJsonObject tabletPadRecord(
    const QString &address = QStringLiteral("0x31"),
    const QString &parentAddress = QStringLiteral("0x30"),
    const QString &parentSelector = QStringLiteral("tablet-main")
)
{
    return {
        {QStringLiteral("address"), address},
        {QStringLiteral("type"), QStringLiteral("tabletPad")},
        {QStringLiteral("belongsTo"), QJsonObject{
             {QStringLiteral("address"), parentAddress},
             {QStringLiteral("name"), parentSelector},
         }},
    };
}

[[nodiscard]] QJsonObject tabletToolRecord(
    const QString &address = QStringLiteral("0x32")
)
{
    return {
        {QStringLiteral("address"), address},
        {QStringLiteral("type"), QStringLiteral("tabletTool")},
    };
}

[[nodiscard]] QJsonObject touchRecord(
    const QString &address = QStringLiteral("0x40"),
    const QString &selector = QStringLiteral("touch-main")
)
{
    return {
        {QStringLiteral("address"), address},
        {QStringLiteral("name"), selector},
    };
}

[[nodiscard]] QJsonObject switchRecord(
    const QString &address = QStringLiteral("0x50"),
    const QString &name = QStringLiteral("lid-switch")
)
{
    return {
        {QStringLiteral("address"), address},
        {QStringLiteral("name"), name},
    };
}

[[nodiscard]] QJsonObject emptyRoot()
{
    return {
        {QStringLiteral("mice"), QJsonArray{}},
        {QStringLiteral("keyboards"), QJsonArray{}},
        {QStringLiteral("tablets"), QJsonArray{}},
        {QStringLiteral("touch"), QJsonArray{}},
        {QStringLiteral("switches"), QJsonArray{}},
    };
}

[[nodiscard]] QJsonObject fullRoot()
{
    auto root = emptyRoot();
    root.insert(QStringLiteral("mice"), QJsonArray{pointerRecord()});
    root.insert(QStringLiteral("keyboards"), QJsonArray{keyboardRecord()});
    root.insert(QStringLiteral("tablets"), QJsonArray{
        tabletToolRecord(), tabletPadRecord(), tabletRecord(),
    });
    root.insert(QStringLiteral("touch"), QJsonArray{touchRecord()});
    root.insert(QStringLiteral("switches"), QJsonArray{switchRecord()});
    return root;
}

[[nodiscard]] ValidationResult<ConnectedInputDeviceInventory> parse(
    const QByteArrayView reply,
    const QStringView identity = QStringView(u"runtime-a"),
    const QByteArrayView epoch = QByteArrayView("epoch-a")
)
{
    return parseConnectedInputDeviceInventory(reply, identity, epoch);
}

[[nodiscard]] QJsonObject publicRecord(
    const QString &selector,
    const QString &kind,
    const QJsonValue &activeKeymap = QJsonValue(QJsonValue::Null)
)
{
    return {
        {QStringLiteral("sessionSelector"), selector},
        {QStringLiteral("observedKind"), kind},
        {QStringLiteral("activeKeymap"), activeKeymap},
    };
}

[[nodiscard]] QJsonObject publicRoot(
    const QJsonArray &records = {},
    const quint32 switches = 0,
    const quint32 pads = 0,
    const quint32 tools = 0
)
{
    return {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("inventoryDigest"), QString(64, QLatin1Char('a'))},
        {QStringLiteral("records"), records},
        {QStringLiteral("unaddressable"), QJsonObject{
             {QStringLiteral("switches"), static_cast<qint64>(switches)},
             {QStringLiteral("tabletPads"), static_cast<qint64>(pads)},
             {QStringLiteral("tabletTools"), static_cast<qint64>(tools)},
         }},
    };
}

} // namespace

class HyprlandInputDeviceInventoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsPinnedWireAndEmitsFilteredCanonicalInventory()
    {
        const auto parsed = parse(compact(fullRoot()));
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        const auto &inventory = *parsed.value;

        QCOMPARE(inventory.records.size(), 4);
        QCOMPARE(inventory.records.at(0).sessionSelector,
                 QStringLiteral("keyboard-main"));
        QCOMPARE(inventory.records.at(0).observedKind,
                 ConnectedInputDeviceKind::Keyboard);
        QVERIFY(inventory.records.at(0).activeKeymap.has_value());
        QCOMPARE(*inventory.records.at(0).activeKeymap,
                 QStringLiteral("English (US)"));
        QCOMPARE(inventory.records.at(1).sessionSelector,
                 QStringLiteral("pointer-main"));
        QCOMPARE(inventory.records.at(1).observedKind,
                 ConnectedInputDeviceKind::Pointer);
        QVERIFY(!inventory.records.at(1).activeKeymap.has_value());
        QCOMPARE(inventory.records.at(2).sessionSelector,
                 QStringLiteral("touch-main"));
        QCOMPARE(inventory.records.at(2).observedKind,
                 ConnectedInputDeviceKind::Touch);
        QCOMPARE(inventory.records.at(3).sessionSelector,
                 QStringLiteral("tablet-main"));
        QCOMPARE(inventory.records.at(3).observedKind,
                 ConnectedInputDeviceKind::Tablet);
        QCOMPARE(inventory.unaddressable.switches, quint32(1));
        QCOMPARE(inventory.unaddressable.tabletPads, quint32(1));
        QCOMPARE(inventory.unaddressable.tabletTools, quint32(1));
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                    .match(inventory.inventoryDigest).hasMatch());
        QVERIFY(inventory.document.endsWith('\n'));
        QVERIFY(!inventory.document.endsWith("\n\n"));

        for (const auto forbidden : {
                 QByteArrayLiteral("0x10"), QByteArrayLiteral("0x20"),
                 QByteArrayLiteral("0x30"), QByteArrayLiteral("0x31"),
                 QByteArrayLiteral("0x32"), QByteArrayLiteral("0x40"),
                 QByteArrayLiteral("0x50"), QByteArray(runtimeIdentity),
                 QByteArrayLiteral("65706f63682d61"),
                 QByteArrayLiteral("defaultSpeed"),
                 QByteArrayLiteral("scrollFactor"),
                 QByteArrayLiteral("active_layout_index")}) {
            QVERIFY2(!inventory.document.contains(forbidden), forbidden.constData());
        }

        const auto publicParsed = parseConnectedInputDeviceInventoryDocument(
            inventory.document
        );
        QVERIFY2(publicParsed, qPrintable(describeErrors(publicParsed.errors)));
        QCOMPARE(*publicParsed.value, inventory);
    }

    void acceptsEmptyInventoryAndTabletPadParentWireVariants()
    {
        auto parsed = parse(compact(emptyRoot()));
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QVERIFY(parsed.value->records.isEmpty());
        QCOMPARE(parsed.value->unaddressable,
                 UnaddressableInputDeviceCounts{});

        auto root = emptyRoot();
        root.insert(QStringLiteral("tablets"), QJsonArray{
            tabletRecord(),
            tabletPadRecord(QStringLiteral("0x31"), QStringLiteral("0x0"),
                            QString()),
            tabletPadRecord(QStringLiteral("0x32"), QStringLiteral("0x30"),
                            QStringLiteral("tablet-main")),
        });
        parsed = parse(compact(root));
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QCOMPARE(parsed.value->records.size(), 1);
        QCOMPARE(parsed.value->unaddressable.tabletPads, quint32(2));
    }

    void bareNoneValueMatrix_data()
    {
        QTest::addColumn<QByteArray>("replacement");
        QTest::addColumn<bool>("accepted");

        QTest::newRow("exact-none")
            << QByteArrayLiteral("\"active_layout_index\":none") << true;
        QTest::newRow("exact-none-with-json-whitespace")
            << QByteArrayLiteral("\"active_layout_index\" \t:\r\n none \t")
            << true;
        QTest::newRow("null")
            << QByteArrayLiteral("\"active_layout_index\":null") << true;
        QTest::newRow("integer-zero")
            << QByteArrayLiteral("\"active_layout_index\":0") << true;
        QTest::newRow("integer-255")
            << QByteArrayLiteral("\"active_layout_index\":255") << true;
        QTest::newRow("capitalized-none")
            << QByteArrayLiteral("\"active_layout_index\":None") << false;
        QTest::newRow("uppercase-none")
            << QByteArrayLiteral("\"active_layout_index\":NONE") << false;
        QTest::newRow("none-suffix")
            << QByteArrayLiteral("\"active_layout_index\":none0") << false;
        QTest::newRow("quoted-none")
            << QByteArrayLiteral("\"active_layout_index\":\"none\"") << false;
        QTest::newRow("comment-after-none")
            << QByteArrayLiteral("\"active_layout_index\":none/*comment*/")
            << false;
        QTest::newRow("wrong-bare-token")
            << QByteArrayLiteral("\"active_layout_index\":undefined")
            << false;
        QTest::newRow("negative-integer")
            << QByteArrayLiteral("\"active_layout_index\":-1") << false;
        QTest::newRow("integer-over-bound")
            << QByteArrayLiteral("\"active_layout_index\":256") << false;
        QTest::newRow("fraction")
            << QByteArrayLiteral("\"active_layout_index\":1.5") << false;
        QTest::newRow("boolean")
            << QByteArrayLiteral("\"active_layout_index\":true") << false;
    }

    void bareNoneValueMatrix()
    {
        QFETCH(QByteArray, replacement);
        QFETCH(bool, accepted);
        auto reply = compact(fullRoot());
        const auto marker = QByteArrayLiteral("\"active_layout_index\":null");
        QCOMPARE(reply.count(marker), 1);
        reply.replace(marker, replacement);
        const auto parsed = parse(reply);
        QCOMPARE(parsed.ok(), accepted);
    }

    void acceptsExactBareNoneBeforeClosingObject()
    {
        const auto reply = QByteArrayLiteral(
            R"json({"mice":[],"keyboards":[{"address":"0x10","name":"keyboard-main","rules":"","model":"","layout":"us","variant":"","options":"","active_keymap":"English (US)","capsLock":false,"numLock":false,"main":true,"active_layout_index":none}],"tablets":[],"touch":[],"switches":[]})json"
        );
        const auto parsed = parse(reply);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QVERIFY(parsed.value->records.front().activeKeymap.has_value());
        QCOMPARE(*parsed.value->records.front().activeKeymap,
                 QStringLiteral("English (US)"));
    }

    void scannerRejectsKeyAndDuplicateTricks()
    {
        const auto base = compact(fullRoot());
        const auto marker = QByteArrayLiteral("\"active_layout_index\":null");
        QCOMPARE(base.count(marker), 1);

        const auto rejectReplacement = [&](const QByteArrayView replacement) {
            auto reply = base;
            reply.replace(marker, replacement.toByteArray());
            const auto parsed = parse(reply);
            QVERIFY2(!parsed, qPrintable(reply));
        };

        rejectReplacement(QByteArrayView(
            "\"Active_layout_index\":none"
        ));
        rejectReplacement(QByteArrayView(
            "\"active_layout_\\u0069ndex\":none"
        ));
        rejectReplacement(QByteArrayView(
            "\"active_layout_index\":none]"
        ));
        rejectReplacement(QByteArrayView(
            "\"active_layout_index\":none,"
            "\"active_layout_index\":0"
        ));
        rejectReplacement(QByteArrayView(
            "\"active_layout_index\":null,"
            "\"active_layout_\\u0069ndex\":0"
        ));

        auto wrongField = base;
        wrongField.replace(
            QByteArrayLiteral("\"active_keymap\":\"English (US)\""),
            QByteArrayLiteral("\"active_keymap\":none")
        );
        QVERIFY(!parse(wrongField));

        auto quotedOccurrence = fullRoot();
        quotedOccurrence.insert(
            QStringLiteral("keyboards"),
            QJsonArray{keyboardRecord(
                QStringLiteral("0x10"),
                QStringLiteral("text \"active_layout_index\": none, remains")
            )}
        );
        const auto parsed = parse(compact(quotedOccurrence));
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QCOMPARE(parsed.value->records.front().sessionSelector,
                 QStringLiteral("text \"active_layout_index\": none, remains"));
    }

    void rejectsClosedWireShapeViolations()
    {
        const auto reject = [](const QJsonObject &root, const QStringView code) {
            const auto parsed = parse(compact(root));
            QVERIFY2(!parsed, qPrintable(describeErrors(parsed.errors)));
            QVERIFY2(hasCode(parsed.errors, code),
                     qPrintable(describeErrors(parsed.errors)));
        };

        auto root = fullRoot();
        root.remove(QStringLiteral("switches"));
        reject(root, u"input-devices.invalid-root-shape");

        root = fullRoot();
        root.insert(QStringLiteral("unknown"), QJsonArray{});
        reject(root, u"input-devices.invalid-root-shape");

        root = fullRoot();
        root.insert(QStringLiteral("mice"), QJsonObject{});
        reject(root, u"input-devices.array-required");

        root = fullRoot();
        auto pointer = pointerRecord();
        pointer.insert(QStringLiteral("extra"), true);
        root.insert(QStringLiteral("mice"), QJsonArray{pointer});
        reject(root, u"input-devices.invalid-pointer");

        root = fullRoot();
        auto keyboard = keyboardRecord();
        keyboard.remove(QStringLiteral("main"));
        root.insert(QStringLiteral("keyboards"), QJsonArray{keyboard});
        reject(root, u"input-devices.invalid-keyboard");

        root = fullRoot();
        auto tablet = tabletToolRecord();
        tablet.insert(QStringLiteral("type"), QStringLiteral("unknown"));
        root.insert(QStringLiteral("tablets"), QJsonArray{tablet});
        reject(root, u"input-devices.invalid-tablet-variant");

        root = fullRoot();
        auto pad = tabletPadRecord();
        pad.insert(QStringLiteral("belongsTo"), QJsonObject{
            {QStringLiteral("address"), QStringLiteral("0x0")},
            {QStringLiteral("name"), QString()},
            {QStringLiteral("extra"), true},
        });
        root.insert(QStringLiteral("tablets"), QJsonArray{pad});
        reject(root, u"input-devices.invalid-tablet-pad");

        root = fullRoot();
        auto touch = touchRecord();
        touch.insert(QStringLiteral("extra"), true);
        root.insert(QStringLiteral("touch"), QJsonArray{touch});
        reject(root, u"input-devices.invalid-touch");

        root = fullRoot();
        auto deviceSwitch = switchRecord();
        deviceSwitch.insert(QStringLiteral("extra"), true);
        root.insert(QStringLiteral("switches"), QJsonArray{deviceSwitch});
        reject(root, u"input-devices.invalid-switch");

        auto duplicateRoot = compact(fullRoot());
        duplicateRoot.replace(
            QByteArrayLiteral("{\"keyboards\":"),
            QByteArrayLiteral("{\"mice\":[],\"keyboards\":")
        );
        const auto duplicate = parse(duplicateRoot);
        QVERIFY(!duplicate);
        QVERIFY(hasCode(duplicate.errors, u"json.duplicate-key"));
    }

    void rejectsPrivateAddressSelectorAndUnicodeViolations()
    {
        const auto reject = [](const QJsonObject &root, const QStringView code) {
            const auto parsed = parse(compact(root));
            QVERIFY2(!parsed, qPrintable(describeErrors(parsed.errors)));
            QVERIFY2(hasCode(parsed.errors, code),
                     qPrintable(describeErrors(parsed.errors)));
        };

        for (const auto &address : {
                 QStringLiteral("0x0"), QStringLiteral("10"),
                 QStringLiteral("0xA"), QStringLiteral("0x01"),
                 QStringLiteral("0x10000000000000000")}) {
            auto root = emptyRoot();
            root.insert(QStringLiteral("mice"),
                        QJsonArray{pointerRecord(address)});
            reject(root, u"input-devices.invalid-address");
        }

        auto root = emptyRoot();
        root.insert(QStringLiteral("mice"),
                    QJsonArray{pointerRecord(QStringLiteral("0x10"))});
        root.insert(QStringLiteral("keyboards"),
                    QJsonArray{keyboardRecord(QStringLiteral("0x10"))});
        reject(root, u"input-devices.duplicate-address");

        root = emptyRoot();
        root.insert(QStringLiteral("mice"), QJsonArray{pointerRecord(
            QStringLiteral("0x20"), QStringLiteral("same-selector")
        )});
        root.insert(QStringLiteral("keyboards"), QJsonArray{keyboardRecord(
            QStringLiteral("0x10"), QStringLiteral("same-selector")
        )});
        reject(root, u"input-devices.duplicate-selector");

        const QString decomposed = QStringLiteral("e\u0301");
        root = emptyRoot();
        root.insert(QStringLiteral("mice"),
                    QJsonArray{pointerRecord(QStringLiteral("0x20"), decomposed)});
        reject(root, u"input-devices.invalid-pointer");

        for (const auto &selector : {
                 QStringLiteral("bad\nselector"),
                 QStringLiteral("bad\u200dselector"),
                 QString(257, QLatin1Char('x'))}) {
            root = emptyRoot();
            root.insert(QStringLiteral("mice"), QJsonArray{pointerRecord(
                QStringLiteral("0x20"), selector
            )});
            reject(root, u"input-devices.invalid-pointer");
        }

        root = emptyRoot();
        root.insert(QStringLiteral("keyboards"), QJsonArray{keyboardRecord(
            QStringLiteral("0x10"), QStringLiteral("keyboard-main"),
            QJsonValue(QJsonValue::Null), QString(513, QLatin1Char('x'))
        )});
        reject(root, u"input-devices.invalid-keyboard");

        root = emptyRoot();
        root.insert(QStringLiteral("keyboards"), QJsonArray{keyboardRecord(
            QStringLiteral("0x10"), QStringLiteral("keyboard-main"),
            QJsonValue(QJsonValue::Null), decomposed
        )});
        reject(root, u"input-devices.invalid-keyboard");

        root = emptyRoot();
        root.insert(QStringLiteral("mice"), QJsonArray{pointerRecord(
            QStringLiteral("0x20"), QString(256, QLatin1Char('x'))
        )});
        const auto maximum = parse(compact(root));
        QVERIFY2(maximum, qPrintable(describeErrors(maximum.errors)));
    }

    void enforcesAggregateAndAuthorityBounds()
    {
        auto root = emptyRoot();
        QJsonArray switches;
        for (int index = 1; index <= 257; ++index) {
            switches.append(switchRecord(
                QStringLiteral("0x%1").arg(index, 0, 16)
            ));
        }
        root.insert(QStringLiteral("switches"), switches);
        auto parsed = parse(compact(root));
        QVERIFY(!parsed);
        QVERIFY(hasCode(parsed.errors, u"input-devices.too-many-devices"));

        parsed = parse(compact(emptyRoot()), QStringView(), serviceEpoch);
        QVERIFY(!parsed);
        QVERIFY(hasCode(
            parsed.errors, u"input-devices.invalid-fingerprint-authority"
        ));
        parsed = parse(compact(emptyRoot()), QStringView(u"runtime-a"), {});
        QVERIFY(!parsed);
        QVERIFY(hasCode(
            parsed.errors, u"input-devices.invalid-fingerprint-authority"
        ));
        const QByteArray oversizedEpoch(513, 'e');
        parsed = parse(
            compact(emptyRoot()), QStringView(u"runtime-a"), oversizedEpoch
        );
        QVERIFY(!parsed);
        QVERIFY(hasCode(
            parsed.errors, u"input-devices.invalid-fingerprint-authority"
        ));

        const QByteArray oversizedReply(
            maximumInputDeviceInventoryBytes + 1, 'x'
        );
        parsed = parse(oversizedReply);
        QVERIFY(!parsed);
        QVERIFY(hasCode(parsed.errors, u"input-devices.reply-too-large"));
    }

    void privateFingerprintIsStableAndSensitiveAtExactBoundary()
    {
        const auto base = parse(compact(fullRoot()));
        QVERIFY2(base, qPrintable(describeErrors(base.errors)));

        // This literal freezes the private schema, sorting, epoch encoding,
        // and digest algorithm without exposing those inputs publicly.
        QCOMPARE(base.value->inventoryDigest,
                 QStringLiteral("8d6c19600f35b792e6cc4d5cf65cf4107917d47aa273933424feef5fd916fe8e"));

        auto diagnosticsRoot = fullRoot();
        diagnosticsRoot.insert(
            QStringLiteral("keyboards"),
            QJsonArray{keyboardRecord(
                QStringLiteral("0x10"), QStringLiteral("keyboard-main"),
                17, QStringLiteral("Different keymap")
            )}
        );
        auto pointer = pointerRecord();
        pointer.insert(QStringLiteral("defaultSpeed"), -0.75);
        pointer.insert(QStringLiteral("scrollFactor"), 4.0);
        diagnosticsRoot.insert(QStringLiteral("mice"), QJsonArray{pointer});
        const auto diagnostics = parse(compact(diagnosticsRoot));
        QVERIFY2(diagnostics,
                 qPrintable(describeErrors(diagnostics.errors)));
        QCOMPARE(diagnostics.value->inventoryDigest,
                 base.value->inventoryDigest);
        QVERIFY(diagnostics.value->document != base.value->document);

        auto orderedRoot = fullRoot();
        orderedRoot.insert(QStringLiteral("mice"), QJsonArray{
            pointerRecord(QStringLiteral("0x21"), QStringLiteral("pointer-z")),
            pointerRecord(QStringLiteral("0x20"), QStringLiteral("pointer-a")),
        });
        auto reversedRoot = orderedRoot;
        reversedRoot.insert(QStringLiteral("mice"), QJsonArray{
            pointerRecord(QStringLiteral("0x20"), QStringLiteral("pointer-a")),
            pointerRecord(QStringLiteral("0x21"), QStringLiteral("pointer-z")),
        });
        const auto ordered = parse(compact(orderedRoot));
        const auto reversed = parse(compact(reversedRoot));
        QVERIFY2(ordered, qPrintable(describeErrors(ordered.errors)));
        QVERIFY2(reversed, qPrintable(describeErrors(reversed.errors)));
        QCOMPARE(ordered.value->inventoryDigest,
                 reversed.value->inventoryDigest);
        QCOMPARE(ordered.value->document, reversed.value->document);

        const auto differentIdentity = parse(
            compact(fullRoot()), QStringView(u"runtime-b"), serviceEpoch
        );
        const auto differentEpoch = parse(
            compact(fullRoot()), QStringView(u"runtime-a"),
            QByteArrayView("epoch-b")
        );
        QVERIFY(differentIdentity);
        QVERIFY(differentEpoch);
        QVERIFY(differentIdentity.value->inventoryDigest
                != base.value->inventoryDigest);
        QVERIFY(differentEpoch.value->inventoryDigest
                != base.value->inventoryDigest);

        const auto requireDifferent = [&](QJsonObject root) {
            const auto changed = parse(compact(root));
            QVERIFY2(changed, qPrintable(describeErrors(changed.errors)));
            QVERIFY(changed.value->inventoryDigest
                    != base.value->inventoryDigest);
        };

        auto root = fullRoot();
        root.insert(QStringLiteral("mice"), QJsonArray{pointerRecord(
            QStringLiteral("0x21"), QStringLiteral("pointer-main")
        )});
        requireDifferent(root);

        root = fullRoot();
        root.insert(QStringLiteral("mice"), QJsonArray{pointerRecord(
            QStringLiteral("0x20"), QStringLiteral("pointer-renamed")
        )});
        requireDifferent(root);

        root = fullRoot();
        root.insert(QStringLiteral("mice"), QJsonArray{});
        root.insert(QStringLiteral("touch"), QJsonArray{
            touchRecord(QStringLiteral("0x20"), QStringLiteral("pointer-main")),
            touchRecord(),
        });
        requireDifferent(root);

        root = fullRoot();
        root.insert(QStringLiteral("switches"),
                    QJsonArray{switchRecord(QStringLiteral("0x51"))});
        requireDifferent(root);

        root = fullRoot();
        root.insert(QStringLiteral("tablets"), QJsonArray{
            tabletToolRecord(),
            tabletPadRecord(QStringLiteral("0x31"), QStringLiteral("0x30"),
                            QStringLiteral("different-parent")),
            tabletRecord(),
        });
        requireDifferent(root);
    }

    void publicParserRequiresExactCanonicalDocument()
    {
        const QJsonArray validRecords{
            publicRecord(
                QStringLiteral("keyboard-a"), QStringLiteral("keyboard"),
                QStringLiteral("English (US)")
            ),
            publicRecord(QStringLiteral("pointer-a"),
                         QStringLiteral("pointer")),
            publicRecord(QStringLiteral("touch-a"), QStringLiteral("touch")),
            publicRecord(QStringLiteral("tablet-a"),
                         QStringLiteral("tablet")),
        };
        const auto validObject = publicRoot(validRecords, 1, 1, 1);
        const auto valid = canonicalDocument(validObject);
        auto parsed = parseConnectedInputDeviceInventoryDocument(valid);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QCOMPARE(parsed.value->records.size(), 4);
        QCOMPARE(parsed.value->unaddressable.switches, quint32(1));
        QCOMPARE(parsed.value->document, valid);

        const auto nullableKeyboard = canonicalDocument(publicRoot(QJsonArray{
            publicRecord(
                QStringLiteral("keyboard-a"), QStringLiteral("keyboard")
            ),
        }));
        parsed = parseConnectedInputDeviceInventoryDocument(nullableKeyboard);
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QVERIFY(!parsed.value->records.front().activeKeymap.has_value());

        const auto reject = [](const QByteArray &document,
                               const QStringView expectedCode) {
            const auto rejected = parseConnectedInputDeviceInventoryDocument(
                document
            );
            QVERIFY2(!rejected, qPrintable(document.left(512)));
            QVERIFY2(hasCode(rejected.errors, expectedCode),
                     qPrintable(describeErrors(rejected.errors)));
        };

        auto changed = valid;
        changed.chop(1);
        reject(changed, u"input-devices.noncanonical-document");
        changed = valid + '\n';
        reject(changed, u"input-devices.noncanonical-document");
        changed = valid;
        changed.chop(1);
        changed.append(" \n");
        reject(changed, u"input-devices.noncanonical-document");
        changed = QJsonDocument(validObject).toJson(QJsonDocument::Indented);
        reject(changed, u"input-devices.noncanonical-document");

        auto object = validObject;
        object.insert(QStringLiteral("extra"), true);
        reject(canonicalDocument(object),
               u"input-devices.invalid-document-shape");
        object = validObject;
        object.remove(QStringLiteral("records"));
        reject(canonicalDocument(object),
               u"input-devices.invalid-document-shape");
        object = validObject;
        object.insert(QStringLiteral("formatVersion"), 2);
        reject(canonicalDocument(object),
               u"input-devices.unsupported-document-version");
        object = validObject;
        object.insert(QStringLiteral("inventoryDigest"),
                      QString(64, QLatin1Char('A')));
        reject(canonicalDocument(object),
               u"input-devices.invalid-inventory-digest");

        object = publicRoot(QJsonArray{
            publicRecord(QStringLiteral("same"), QStringLiteral("keyboard")),
            publicRecord(QStringLiteral("same"), QStringLiteral("pointer")),
        });
        reject(canonicalDocument(object), u"input-devices.duplicate-selector");

        object = publicRoot(QJsonArray{
            publicRecord(QStringLiteral("pointer-a"),
                         QStringLiteral("pointer")),
            publicRecord(QStringLiteral("keyboard-a"),
                         QStringLiteral("keyboard")),
        });
        reject(canonicalDocument(object),
               u"input-devices.invalid-record-order");
        object = publicRoot(QJsonArray{
            publicRecord(QStringLiteral("pointer-z"),
                         QStringLiteral("pointer")),
            publicRecord(QStringLiteral("pointer-a"),
                         QStringLiteral("pointer")),
        });
        reject(canonicalDocument(object),
               u"input-devices.invalid-record-order");

        object = publicRoot(QJsonArray{publicRecord(
            QStringLiteral("pointer-a"), QStringLiteral("pointer"),
            QStringLiteral("not-null")
        )});
        reject(canonicalDocument(object),
               u"input-devices.unexpected-active-keymap");
        object = publicRoot(QJsonArray{publicRecord(
            QStringLiteral("keyboard-a"), QStringLiteral("keyboard"),
            QStringLiteral("e\u0301")
        )});
        reject(canonicalDocument(object),
               u"input-devices.invalid-active-keymap");
        object = publicRoot(QJsonArray{publicRecord(
            QStringLiteral("keyboard-a"), QStringLiteral("keyboard"),
            QString(513, QLatin1Char('x'))
        )});
        reject(canonicalDocument(object),
               u"input-devices.invalid-active-keymap");
        object = publicRoot(QJsonArray{publicRecord(
            QStringLiteral("keyboard-a"), QStringLiteral("keyboard"),
            QStringLiteral("bad\u200ddiagnostic")
        )});
        reject(canonicalDocument(object),
               u"input-devices.invalid-active-keymap");
        object = publicRoot(QJsonArray{publicRecord(
            QStringLiteral("keyboard-a"), QStringLiteral("unknown")
        )});
        reject(canonicalDocument(object),
               u"input-devices.invalid-public-kind");

        for (const auto &selector : {
                 QStringLiteral("e\u0301"),
                 QStringLiteral("bad\u200dselector"),
                 QString(257, QLatin1Char('x'))}) {
            object = publicRoot(QJsonArray{publicRecord(
                selector, QStringLiteral("pointer")
            )});
            reject(canonicalDocument(object),
                   u"input-devices.invalid-public-record");
        }

        auto malformedRecord = publicRecord(
            QStringLiteral("pointer-a"), QStringLiteral("pointer")
        );
        malformedRecord.insert(QStringLiteral("extra"), true);
        object = publicRoot(QJsonArray{malformedRecord});
        reject(canonicalDocument(object),
               u"input-devices.invalid-public-record");

        object = publicRoot({}, 256, 1, 0);
        reject(canonicalDocument(object),
               u"input-devices.invalid-unaddressable-counts");
        object = publicRoot();
        auto counts = object.value(QStringLiteral("unaddressable")).toObject();
        counts.insert(QStringLiteral("switches"), 0.5);
        object.insert(QStringLiteral("unaddressable"), counts);
        reject(canonicalDocument(object),
               u"input-devices.invalid-unaddressable-counts");
        counts.remove(QStringLiteral("tabletTools"));
        object.insert(QStringLiteral("unaddressable"), counts);
        reject(canonicalDocument(object),
               u"input-devices.invalid-unaddressable-shape");

        QJsonArray tooManyRecords;
        for (int index = 0; index < 257; ++index) {
            tooManyRecords.append(publicRecord(
                QStringLiteral("pointer-%1").arg(index, 3, 10, QLatin1Char('0')),
                QStringLiteral("pointer")
            ));
        }
        object = publicRoot(tooManyRecords);
        reject(canonicalDocument(object),
               u"input-devices.invalid-public-records");

        const QByteArray duplicateRoot = QByteArrayLiteral(
            "{\"formatVersion\":1,\"formatVersion\":1,"
            "\"inventoryDigest\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
            "\"records\":[],\"unaddressable\":{\"switches\":0,"
            "\"tabletPads\":0,\"tabletTools\":0}}\n"
        );
        reject(duplicateRoot, u"json.duplicate-key");

        const QByteArray oversized(maximumInputDeviceInventoryBytes + 1, 'x');
        reject(oversized, u"input-devices.document-too-large");
    }
};

QTEST_APPLESS_MAIN(HyprlandInputDeviceInventoryTest)

#include "hyprland_input_device_inventory_test.moc"
