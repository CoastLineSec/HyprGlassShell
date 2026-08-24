#include "compositord/canonical_json.h"

#include <QJsonArray>
#include <QProcess>
#include <QTest>

#include <bit>
#include <cmath>
#include <limits>

using namespace HyprShelld::Compositor::CanonicalJson;

namespace {

[[nodiscard]] double fromBits(const quint64 bits)
{
    return std::bit_cast<double>(bits);
}

[[nodiscard]] QString firstCode(const QVector<Error> &errors)
{
    return errors.isEmpty() ? QString{} : errors.front().code;
}

[[nodiscard]] QByteArray encodedScalar(const QJsonValue &value)
{
    const auto result = serialize(value);
    return result ? *result.value : QByteArray{};
}

} // namespace

class CompositorCanonicalJsonTest final : public QObject {
    Q_OBJECT

private slots:
    void appendixBNumbers_data()
    {
        QTest::addColumn<quint64>("bits");
        QTest::addColumn<QByteArray>("expected");

        QTest::newRow("positive zero")
            << quint64{0x0000000000000000} << QByteArrayLiteral("0");
        QTest::newRow("negative zero")
            << quint64{0x8000000000000000} << QByteArrayLiteral("0");
        QTest::newRow("minimum subnormal")
            << quint64{0x0000000000000001} << QByteArrayLiteral("5e-324");
        QTest::newRow("negative minimum subnormal")
            << quint64{0x8000000000000001} << QByteArrayLiteral("-5e-324");
        QTest::newRow("maximum finite")
            << quint64{0x7fefffffffffffff}
            << QByteArrayLiteral("1.7976931348623157e+308");
        QTest::newRow("negative maximum finite")
            << quint64{0xffefffffffffffff}
            << QByteArrayLiteral("-1.7976931348623157e+308");
        QTest::newRow("positive two to 53")
            << quint64{0x4340000000000000}
            << QByteArrayLiteral("9007199254740992");
        QTest::newRow("negative two to 53")
            << quint64{0xc340000000000000}
            << QByteArrayLiteral("-9007199254740992");
        QTest::newRow("large fixed integer")
            << quint64{0x4430000000000000}
            << QByteArrayLiteral("295147905179352830000");
        QTest::newRow("below one e23")
            << quint64{0x44b52d02c7e14af5}
            << QByteArrayLiteral("9.999999999999997e+22");
        QTest::newRow("one e23")
            << quint64{0x44b52d02c7e14af6} << QByteArrayLiteral("1e+23");
        QTest::newRow("above one e23")
            << quint64{0x44b52d02c7e14af7}
            << QByteArrayLiteral("1.0000000000000001e+23");
        QTest::newRow("below fixed upper boundary one")
            << quint64{0x444b1ae4d6e2ef4e}
            << QByteArrayLiteral("999999999999999700000");
        QTest::newRow("below fixed upper boundary two")
            << quint64{0x444b1ae4d6e2ef4f}
            << QByteArrayLiteral("999999999999999900000");
        QTest::newRow("fixed upper boundary")
            << quint64{0x444b1ae4d6e2ef50} << QByteArrayLiteral("1e+21");
        QTest::newRow("below fixed lower boundary")
            << quint64{0x3eb0c6f7a0b5ed8c}
            << QByteArrayLiteral("9.999999999999997e-7");
        QTest::newRow("fixed lower boundary")
            << quint64{0x3eb0c6f7a0b5ed8d}
            << QByteArrayLiteral("0.000001");
        QTest::newRow("rounding row one")
            << quint64{0x41b3de4355555553}
            << QByteArrayLiteral("333333333.3333332");
        QTest::newRow("rounding row two")
            << quint64{0x41b3de4355555554}
            << QByteArrayLiteral("333333333.33333325");
        QTest::newRow("rounding row three")
            << quint64{0x41b3de4355555555}
            << QByteArrayLiteral("333333333.3333333");
        QTest::newRow("rounding row four")
            << quint64{0x41b3de4355555556}
            << QByteArrayLiteral("333333333.3333334");
        QTest::newRow("rounding row five")
            << quint64{0x41b3de4355555557}
            << QByteArrayLiteral("333333333.33333343");
        QTest::newRow("negative fixed lower range")
            << quint64{0xbecbf647612f3696}
            << QByteArrayLiteral("-0.0000033333333333333333");
        QTest::newRow("integer fraction boundary")
            << quint64{0x43143ff3c1cb0959}
            << QByteArrayLiteral("1424953923781206.2");
    }

    void appendixBNumbers()
    {
        QFETCH(quint64, bits);
        QFETCH(QByteArray, expected);
        const auto result = serialize(QJsonValue(fromBits(bits)));
        QVERIFY2(result, qPrintable(firstCode(result.errors)));
        QCOMPARE(*result.value, expected);
        const auto objectBytes = QByteArrayLiteral("{\"n\":") + expected + '}';
        const auto parsed = parseCanonicalObject(objectBytes);
        QVERIFY2(parsed, qPrintable(firstCode(parsed.errors)));
    }

    void officialObjectVector()
    {
        const QJsonArray numbers{
            333333333.33333329, 1E30, 4.50, 2e-3, 1e-27,
        };
        QString string;
        string.append(QChar(0x20ac));
        string.append(QLatin1Char('$'));
        string.append(QChar(0x000f));
        string.append(QLatin1Char('\n'));
        string.append(QStringLiteral("A'B"));
        string.append(QLatin1Char('"'));
        string.append(QLatin1Char('\\'));
        string.append(QLatin1Char('\\'));
        string.append(QLatin1Char('"'));
        string.append(QLatin1Char('/'));

        const QJsonObject object{
            {QStringLiteral("numbers"), numbers},
            {QStringLiteral("string"), string},
            {QStringLiteral("literals"), QJsonArray{QJsonValue::Null, true, false}},
        };
        auto expected = QByteArrayLiteral(
            "{\"literals\":[null,true,false],\"numbers\":[333333333.3333333,1e+30,4.5,0.002,1e-27],\"string\":\""
        );
        expected.append(QStringLiteral("\u20ac").toUtf8());
        expected.append(QByteArrayLiteral("$\\u000f\\nA'B\\\"\\\\\\\\\\\"/\"}"));

        const auto result = serialize(object);
        QVERIFY2(result, qPrintable(firstCode(result.errors)));
        QCOMPARE(*result.value, expected);
        const auto parsed = parseCanonicalObject(expected);
        QVERIFY2(parsed, qPrintable(firstCode(parsed.errors)));
        QCOMPARE(*parsed.value, object);
    }

    void usesUtf16MemberOrdering()
    {
        const QString carriageReturn(QChar(0x000d));
        const QString one = QStringLiteral("1");
        const QString control(QChar(0x0080));
        const QString oUmlaut(QChar(0x00f6));
        const QString euro(QChar(0x20ac));
        const QString emoji = QString::fromUcs4(U"\U0001f600");
        const QString ligature(QChar(0xfb33));

        QJsonObject object;
        object.insert(ligature, 7);
        object.insert(emoji, 6);
        object.insert(euro, 5);
        object.insert(oUmlaut, 4);
        object.insert(control, 3);
        object.insert(one, 2);
        object.insert(carriageReturn, 1);

        auto expected = QByteArrayLiteral("{\"\\r\":1,\"1\":2,\"");
        expected.append(control.toUtf8());
        expected.append(QByteArrayLiteral("\":3,\""));
        expected.append(oUmlaut.toUtf8());
        expected.append(QByteArrayLiteral("\":4,\""));
        expected.append(euro.toUtf8());
        expected.append(QByteArrayLiteral("\":5,\""));
        expected.append(emoji.toUtf8());
        expected.append(QByteArrayLiteral("\":6,\""));
        expected.append(ligature.toUtf8());
        expected.append(QByteArrayLiteral("\":7}"));

        const auto encoded = serialize(object);
        QVERIFY(encoded);
        QCOMPARE(*encoded.value, expected);

        const auto supplementary = QString::fromUcs4(U"\U00010000");
        const QString bmpPrivate(QChar(0xe000));
        QJsonObject boundary;
        boundary.insert(bmpPrivate, 2);
        boundary.insert(supplementary, 1);
        auto boundaryExpected = QByteArrayLiteral("{\"");
        boundaryExpected.append(supplementary.toUtf8());
        boundaryExpected.append(QByteArrayLiteral("\":1,\""));
        boundaryExpected.append(bmpPrivate.toUtf8());
        boundaryExpected.append(QByteArrayLiteral("\":2}"));
        const auto boundaryEncoded = serialize(boundary);
        QVERIFY(boundaryEncoded);
        QCOMPARE(*boundaryEncoded.value, boundaryExpected);
    }

    void stringEscapingIsExact()
    {
        QString value;
        value.append(QChar(0x0000));
        value.append(QChar(0x0008));
        value.append(QChar(0x0009));
        value.append(QChar(0x000a));
        value.append(QChar(0x000c));
        value.append(QChar(0x000d));
        value.append(QChar(0x001f));
        value.append(QChar(0x007f));
        value.append(QChar(0x0080));
        value.append(QLatin1Char('"'));
        value.append(QLatin1Char('/'));
        value.append(QLatin1Char('\\'));
        value.append(QChar(0x2028));
        value.append(QChar(0x2029));
        value.append(QString::fromUcs4(U"\U0001f600"));

        auto expected = QByteArrayLiteral(
            "\"\\u0000\\b\\t\\n\\f\\r\\u001f\\\"/\\\\"
        );
        expected.insert(
            expected.indexOf(QByteArrayLiteral("\\\"/")),
            QString(QChar(0x007f)).toUtf8()
                + QString(QChar(0x0080)).toUtf8()
        );
        expected.append(QString(QChar(0x2028)).toUtf8());
        expected.append(QString(QChar(0x2029)).toUtf8());
        expected.append(QString::fromUcs4(U"\U0001f600").toUtf8());
        expected.append('"');
        const auto encoded = serialize(value);
        QVERIFY(encoded);
        QCOMPARE(*encoded.value, expected);
    }

    void baseAndProjectTextPoliciesAreSeparated()
    {
        const QString decomposed = QStringLiteral("e\u0301");
        const QString composed = QStringLiteral("\u00e9");
        const QString control(QChar(0x000f));
        const QString format(QChar(0x200d));

        const QJsonObject decomposedObject{{QStringLiteral("text"), decomposed}};
        const auto base = serialize(decomposedObject);
        QVERIFY(base);
        QVERIFY(parseCanonicalObject(*base.value));

        const auto rejectedDecomposed = serialize(
            decomposedObject, Framing::Bare, TextPolicy::ProjectReceipt
        );
        QVERIFY(!rejectedDecomposed);
        QCOMPARE(firstCode(rejectedDecomposed.errors),
                 QStringLiteral("jcs.non-nfc-text"));
        const auto parsedDecomposed = parseCanonicalObject(
            *base.value, Framing::Bare, TextPolicy::ProjectReceipt
        );
        QVERIFY(!parsedDecomposed);
        QCOMPARE(firstCode(parsedDecomposed.errors),
                 QStringLiteral("jcs.non-nfc-text"));

        const auto acceptedComposed = serialize(
            QJsonObject{{QStringLiteral("text"), composed}},
            Framing::Bare, TextPolicy::ProjectReceipt
        );
        QVERIFY(acceptedComposed);
        QVERIFY(parseCanonicalObject(
            *acceptedComposed.value,
            Framing::Bare,
            TextPolicy::ProjectReceipt
        ));

        for (const auto &text : {control, QString(QChar(0x007f)), format}) {
            const auto baseControl = serialize(text);
            QVERIFY(baseControl);
            const auto receiptControl = serialize(
                text, Framing::Bare, TextPolicy::ProjectReceipt
            );
            QVERIFY(!receiptControl);
            QCOMPARE(firstCode(receiptControl.errors),
                     QStringLiteral("jcs.forbidden-control"));
        }

        const auto baseControlObject = serialize(
            QJsonObject{{QStringLiteral("text"), control}}
        );
        QVERIFY(baseControlObject);
        const auto parsedControl = parseCanonicalObject(
            *baseControlObject.value,
            Framing::Bare,
            TextPolicy::ProjectReceipt
        );
        QVERIFY(!parsedControl);
        QCOMPARE(firstCode(parsedControl.errors),
                 QStringLiteral("jcs.forbidden-control"));

        QJsonObject decomposedKey;
        decomposedKey.insert(decomposed, true);
        const auto rejectedKey = serialize(
            decomposedKey, Framing::Bare, TextPolicy::ProjectReceipt
        );
        QVERIFY(!rejectedKey);
        QCOMPARE(firstCode(rejectedKey.errors),
                 QStringLiteral("jcs.non-nfc-text"));
    }

    void duplicateAndUnicodeFailures_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("code");
        QTest::newRow("literal duplicate")
            << QByteArrayLiteral("{\"a\":1,\"a\":2}")
            << QStringLiteral("jcs.duplicate-key");
        QTest::newRow("escaped equivalent duplicate")
            << QByteArrayLiteral("{\"a\":1,\"\\u0061\":2}")
            << QStringLiteral("jcs.duplicate-key");
        QTest::newRow("lone high surrogate")
            << QByteArrayLiteral("{\"x\":\"\\ud800\"}")
            << QStringLiteral("jcs.invalid-unicode");
        QTest::newRow("lone low surrogate")
            << QByteArrayLiteral("{\"x\":\"\\udc00\"}")
            << QStringLiteral("jcs.invalid-unicode");
        QTest::newRow("high then non-low")
            << QByteArrayLiteral("{\"x\":\"\\ud800\\u0061\"}")
            << QStringLiteral("jcs.invalid-unicode");
        QTest::newRow("truncated unicode escape")
            << QByteArrayLiteral("{\"x\":\"\\u12")
            << QStringLiteral("jcs.invalid-unicode-escape");
        QTest::newRow("bad escape")
            << QByteArrayLiteral("{\"x\":\"\\q\"}")
            << QStringLiteral("jcs.invalid-escape");
    }

    void duplicateAndUnicodeFailures()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(QString, code);
        const auto parsed = parseCanonicalObject(bytes);
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), code);
    }

    void invalidUtf8AndBomFail()
    {
        QByteArray invalid = QByteArrayLiteral("{\"x\":\"");
        invalid.append(char(0xc3));
        invalid.append(char(0x28));
        invalid.append(QByteArrayLiteral("\"}"));
        auto parsed = parseCanonicalObject(invalid);
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.invalid-utf8"));

        QByteArray surrogateUtf8 = QByteArrayLiteral("{\"x\":\"");
        surrogateUtf8.append(char(0xed));
        surrogateUtf8.append(char(0xa0));
        surrogateUtf8.append(char(0x80));
        surrogateUtf8.append(QByteArrayLiteral("\"}"));
        parsed = parseCanonicalObject(surrogateUtf8);
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.invalid-utf8"));

        QByteArray bom("\xef\xbb\xbf", 3);
        bom.append(QByteArrayLiteral("{}"));
        parsed = parseCanonicalObject(bom);
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.bom-forbidden"));
    }

    void noncanonicalFormsFail_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("member order") << QByteArrayLiteral("{\"b\":1,\"a\":2}");
        QTest::newRow("space") << QByteArrayLiteral("{ \"a\":1}");
        QTest::newRow("escaped solidus") << QByteArrayLiteral("{\"a\":\"\\/\"}");
        QTest::newRow("escaped ascii") << QByteArrayLiteral("{\"a\":\"\\u0061\"}");
        QTest::newRow("uppercase control hex") << QByteArrayLiteral("{\"a\":\"\\u000F\"}");
        QTest::newRow("negative zero") << QByteArrayLiteral("{\"n\":-0}");
        QTest::newRow("redundant fraction") << QByteArrayLiteral("{\"n\":1.0}");
        QTest::newRow("uppercase exponent") << QByteArrayLiteral("{\"n\":1E+30}");
        QTest::newRow("rounded input") << QByteArrayLiteral("{\"n\":333333333.33333329}");
        QTest::newRow("lossy integer") << QByteArrayLiteral("{\"n\":9007199254740993}");
    }

    void noncanonicalFormsFail()
    {
        QFETCH(QByteArray, bytes);
        const auto parsed = parseCanonicalObject(bytes);
        QVERIFY(!parsed);
        QVERIFY(!parsed.errors.isEmpty());
    }

    void framingIsExplicit()
    {
        QVERIFY(parseCanonicalObject(QByteArrayLiteral("{}"), Framing::Bare));
        QVERIFY(parseCanonicalObject(
            QByteArrayLiteral("{}\n"), Framing::OneTrailingLineFeed
        ));
        auto parsed = parseCanonicalObject(
            QByteArrayLiteral("{}"), Framing::OneTrailingLineFeed
        );
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.missing-line-feed"));

        struct Row { QByteArray bytes; Framing framing; };
        const QVector<Row> rows{
            {QByteArrayLiteral("{}\n"), Framing::Bare},
            {QByteArrayLiteral("{}\r\n"), Framing::OneTrailingLineFeed},
            {QByteArrayLiteral("{}\n\n"), Framing::OneTrailingLineFeed},
        };
        for (const auto &row : rows) {
            parsed = parseCanonicalObject(row.bytes, row.framing);
            QVERIFY(!parsed);
            QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.noncanonical"));
        }

        QByteArray nul = QByteArrayLiteral("{}");
        nul.append('\0');
        parsed = parseCanonicalObject(nul);
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.trailing-data"));

        const auto unknown = static_cast<Framing>(-1);
        auto encoded = serialize(QJsonObject{}, unknown);
        QVERIFY(!encoded);
        QCOMPARE(firstCode(encoded.errors), QStringLiteral("jcs.unknown-framing"));
        parsed = parseCanonicalObject(QByteArrayLiteral("{}"), unknown);
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.unknown-framing"));

        const auto bare = serialize(QJsonObject{}, Framing::Bare);
        const auto line = serialize(QJsonObject{}, Framing::OneTrailingLineFeed);
        QVERIFY(bare);
        QVERIFY(line);
        QCOMPARE(*bare.value, QByteArrayLiteral("{}"));
        QCOMPARE(*line.value, QByteArrayLiteral("{}\n"));
    }

    void literalAndNumberTruncationFail_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        for (const auto &token : {
                 QByteArrayLiteral("t"), QByteArrayLiteral("tr"),
                 QByteArrayLiteral("tru"), QByteArrayLiteral("f"),
                 QByteArrayLiteral("fa"), QByteArrayLiteral("fal"),
                 QByteArrayLiteral("fals"), QByteArrayLiteral("n"),
                 QByteArrayLiteral("nu"), QByteArrayLiteral("nul"),
                 QByteArrayLiteral("-"), QByteArrayLiteral("1."),
                 QByteArrayLiteral("1e"), QByteArrayLiteral("1e+"),
                 QByteArrayLiteral("1e-"),
             }) {
            const auto row = QByteArrayLiteral("token-") + token;
            QTest::newRow(row.constData())
                << (QByteArrayLiteral("{\"x\":") + token);
        }
        QTest::newRow("unterminated string") << QByteArrayLiteral("{\"x\":\"");
        QTest::newRow("terminal backslash") << QByteArrayLiteral("{\"x\":\"\\");
        QTest::newRow("high escape marker") << QByteArrayLiteral("{\"x\":\"\\ud800\\");
    }

    void literalAndNumberTruncationFail()
    {
        QFETCH(QByteArray, bytes);
        const auto parsed = parseCanonicalObject(bytes);
        QVERIFY(!parsed);
        QVERIFY(!parsed.errors.isEmpty());
    }

    void nonIJsonNumbersFail()
    {
        for (const auto &bytes : {
                 QByteArrayLiteral("{\"n\":1e400}"),
                 QByteArrayLiteral("{\"n\":1e-4000}"),
                 QByteArrayLiteral("{\"n\":NaN}"),
                 QByteArrayLiteral("{\"n\":Infinity}"),
                 QByteArrayLiteral("{\"n\":01}"),
             }) {
            const auto parsed = parseCanonicalObject(bytes);
            QVERIFY2(!parsed, bytes.constData());
        }
        for (const auto number : {
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            const auto encoded = serialize(QJsonValue(number));
            QVERIFY(!encoded);
            QCOMPARE(firstCode(encoded.errors),
                     QStringLiteral("jcs.non-finite-number"));
        }
    }

    void integerBackedQtValuesAreLosslessOnly()
    {
        constexpr qint64 safe = 9'007'199'254'740'991;
        constexpr qint64 twoTo53 = 9'007'199'254'740'992;
        const auto safePositive = serialize(QJsonValue(safe));
        const auto safeNegative = serialize(QJsonValue(-safe));
        const auto exactPositive = serialize(QJsonValue(twoTo53));
        const auto exactNegative = serialize(QJsonValue(-twoTo53));
        QVERIFY(safePositive);
        QVERIFY(safeNegative);
        QVERIFY(exactPositive);
        QVERIFY(exactNegative);
        QCOMPARE(*safePositive.value, QByteArray::number(safe));
        QCOMPARE(*safeNegative.value, QByteArray::number(-safe));
        QCOMPARE(*exactPositive.value, QByteArray::number(twoTo53));
        QCOMPARE(*exactNegative.value, QByteArray::number(-twoTo53));

        for (const auto integer : {
                 twoTo53 + 1,
                 -twoTo53 - 1,
                 std::numeric_limits<qint64>::max(),
             }) {
            const auto rejected = serialize(QJsonValue(integer));
            QVERIFY(!rejected);
            QCOMPARE(firstCode(rejected.errors),
                     QStringLiteral("jcs.lossy-integer"));
        }
        const auto minimum = serialize(QJsonValue(std::numeric_limits<qint64>::min()));
        QVERIFY(minimum);
        QCOMPARE(*minimum.value, QByteArrayLiteral("-9223372036854776000"));
        QCOMPARE(encodedScalar(QJsonValue(42.0)), QByteArrayLiteral("42"));
    }

    void parseAndProgrammaticLimitsAreEnforced()
    {
        const QJsonObject nested{
            {QStringLiteral("outer"),
             QJsonArray{QJsonObject{{QStringLiteral("inner"), true}}}},
        };
        const auto encoded = serialize(nested);
        QVERIFY(encoded);
        QVERIFY(parseCanonicalObject(*encoded.value));

        Limits depthTwo;
        depthTwo.maximumDepth = 2;
        auto rejectedBytes = serialize(
            nested, Framing::Bare, TextPolicy::Rfc8785, depthTwo
        );
        QVERIFY(!rejectedBytes);
        QCOMPARE(firstCode(rejectedBytes.errors), QStringLiteral("jcs.depth-limit"));
        auto rejectedObject = parseCanonicalObject(
            *encoded.value, Framing::Bare, TextPolicy::Rfc8785, depthTwo
        );
        QVERIFY(!rejectedObject);
        QCOMPARE(firstCode(rejectedObject.errors), QStringLiteral("jcs.depth-limit"));

        Limits outputOne;
        outputOne.maximumBytes = 1;
        rejectedBytes = serialize(
            QJsonObject{}, Framing::Bare, TextPolicy::Rfc8785, outputOne
        );
        QVERIFY(!rejectedBytes);
        QCOMPARE(firstCode(rejectedBytes.errors), QStringLiteral("jcs.output-limit"));
        Limits outputTwo;
        outputTwo.maximumBytes = 2;
        QVERIFY(serialize(QJsonObject{}, Framing::Bare,
                          TextPolicy::Rfc8785, outputTwo));
        rejectedBytes = serialize(
            QJsonObject{}, Framing::OneTrailingLineFeed,
            TextPolicy::Rfc8785, outputTwo
        );
        QVERIFY(!rejectedBytes);

        rejectedObject = parseCanonicalObject(
            QByteArrayLiteral("{}"), Framing::Bare,
            TextPolicy::Rfc8785, outputOne
        );
        QVERIFY(!rejectedObject);
        QCOMPARE(firstCode(rejectedObject.errors), QStringLiteral("jcs.input-limit"));

        Limits oneValue;
        oneValue.maximumValues = 1;
        rejectedBytes = serialize(
            QJsonObject{{QStringLiteral("x"), true}}, Framing::Bare,
            TextPolicy::Rfc8785, oneValue
        );
        QVERIFY(!rejectedBytes);
        QCOMPARE(firstCode(rejectedBytes.errors), QStringLiteral("jcs.value-limit"));
        rejectedObject = parseCanonicalObject(
            QByteArrayLiteral("{\"x\":true}"), Framing::Bare,
            TextPolicy::Rfc8785, oneValue
        );
        QVERIFY(!rejectedObject);
        QCOMPARE(firstCode(rejectedObject.errors), QStringLiteral("jcs.value-limit"));

        Limits invalid;
        invalid.maximumDepth = -1;
        rejectedBytes = serialize(
            QJsonObject{}, Framing::Bare, TextPolicy::Rfc8785, invalid
        );
        QVERIFY(!rejectedBytes);
        QCOMPARE(firstCode(rejectedBytes.errors), QStringLiteral("jcs.invalid-limits"));
    }

    void undefinedAndUnknownPoliciesFail()
    {
        auto encoded = serialize(QJsonValue(QJsonValue::Undefined));
        QVERIFY(!encoded);
        QCOMPARE(firstCode(encoded.errors), QStringLiteral("jcs.undefined-value"));
        const auto unknown = static_cast<TextPolicy>(-1);
        encoded = serialize(QJsonObject{}, Framing::Bare, unknown);
        QVERIFY(!encoded);
        QCOMPARE(firstCode(encoded.errors), QStringLiteral("jcs.unknown-text-policy"));
        const auto parsed = parseCanonicalObject(
            QByteArrayLiteral("{}"), Framing::Bare, unknown
        );
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors), QStringLiteral("jcs.unknown-text-policy"));

        QString loneHigh;
        loneHigh.append(QChar(0xd800));
        encoded = serialize(loneHigh);
        QVERIFY(!encoded);
        QCOMPARE(firstCode(encoded.errors), QStringLiteral("jcs.invalid-unicode"));
    }

    void rootObjectValidationAndArraySerializationAreDistinct()
    {
        const auto array = serialize(
            QJsonArray{1, QJsonObject{{QStringLiteral("x"), 2}}}
        );
        QVERIFY(array);
        QCOMPARE(*array.value, QByteArrayLiteral("[1,{\"x\":2}]"));
        const auto parsed = parseCanonicalObject(*array.value);
        QVERIFY(!parsed);
        QCOMPARE(firstCode(parsed.errors),
                 QStringLiteral("jcs.root-object-required"));
    }

    void deterministicNumbersMatchNodeJsonStringify()
    {
        constexpr qsizetype sampleCount = 4096;
        QByteArray input;
        QVector<double> numbers;
        numbers.reserve(sampleCount);
        quint64 state = 0x6a09e667f3bcc909ULL;
        while (numbers.size() < sampleCount) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            if (((state >> 52) & 0x7ff) == 0x7ff) {
                continue;
            }
            numbers.append(fromBits(state));
            input.append(QByteArray::number(state, 16).rightJustified(16, '0'));
            input.append('\n');
        }

        const QString script = QStringLiteral(
            "const fs=require('fs');"
            "const lines=fs.readFileSync(0,'utf8').trim().split(/\\n/);"
            "for(const h of lines){"
            "const b=Buffer.alloc(8);"
            "b.writeBigUInt64BE(BigInt('0x'+h));"
            "process.stdout.write(JSON.stringify(b.readDoubleBE(0))+'\\n');"
            "}"
        );
        QProcess node;
        node.start(
            QStringLiteral(HYPRSHELLD_NODE_EXECUTABLE),
            {QStringLiteral("-e"), script}
        );
        QVERIFY2(node.waitForStarted(), qPrintable(node.errorString()));
        QCOMPARE(node.write(input), qint64(input.size()));
        node.closeWriteChannel();
        QVERIFY2(node.waitForFinished(15'000), qPrintable(node.errorString()));
        QCOMPARE(node.exitStatus(), QProcess::NormalExit);
        QCOMPARE(node.exitCode(), 0);

        auto lines = node.readAllStandardOutput().split('\n');
        if (!lines.isEmpty() && lines.back().isEmpty()) {
            lines.removeLast();
        }
        QCOMPARE(lines.size(), numbers.size());
        for (qsizetype index = 0; index < numbers.size(); ++index) {
            const auto encoded = serialize(QJsonValue(numbers.at(index)));
            QVERIFY2(encoded, qPrintable(firstCode(encoded.errors)));
            QCOMPARE(*encoded.value, lines.at(index));
        }
    }
};

QTEST_APPLESS_MAIN(CompositorCanonicalJsonTest)

#include "compositor_canonical_json_test.moc"
