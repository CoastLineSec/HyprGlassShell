#include "compositord/authority_records.h"
#include "compositord/canonical_json.h"

#include <QJsonObject>
#include <QStringList>
#include <QtTest>

#include <limits>

using namespace HyprShelld::Compositor;

namespace {

const QString authorityId =
    QStringLiteral("11111111111111111111111111111111");
const QString activationNonce =
    QStringLiteral("22222222222222222222222222222222");
const QString snapshotDigest = QString(64, QLatin1Char('a'));
const QString generationDigest = QString(64, QLatin1Char('b'));

[[nodiscard]] AppliedRecordV2 validApplied(
    const quint64 revision = 42,
    const ActivationRequirement requirement = ActivationRequirement::Reload
)
{
    return {
        .authorityId = authorityId,
        .revision = revision,
        .snapshotDigest = snapshotDigest,
        .generation = generationDigest,
        .activationNonce = activationNonce,
        .entrypoint = QStringLiteral("hyprland.lua"),
        .requiredActivation = requirement,
    };
}

[[nodiscard]] QJsonObject appliedObject(const AppliedRecordV2 &record)
{
    return {
        {QStringLiteral("formatVersion"), 2},
        {QStringLiteral("authorityId"), record.authorityId},
        {QStringLiteral("revision"), QString::number(record.revision)},
        {QStringLiteral("snapshotDigest"), record.snapshotDigest},
        {QStringLiteral("generation"), record.generation},
        {QStringLiteral("activationNonce"), record.activationNonce},
        {QStringLiteral("entrypoint"), record.entrypoint},
        {
            QStringLiteral("requiredActivation"),
            durableActivationRequirementName(record.requiredActivation),
        },
    };
}

[[nodiscard]] QByteArray canonicalObject(
    const QJsonObject &object,
    const qsizetype maximumBytes = 4096
)
{
    const auto encoded = CanonicalJson::serialize(
        object,
        CanonicalJson::Framing::OneTrailingLineFeed,
        CanonicalJson::TextPolicy::Rfc8785,
        {
            .maximumBytes = maximumBytes,
            .maximumDepth = 4,
            .maximumValues = 128,
        }
    );
    if (!encoded) {
        QTest::qFail(
            "Test fixture could not be encoded as canonical JSON",
            __FILE__,
            __LINE__
        );
        return {};
    }
    return *encoded.value;
}

} // namespace

class CompositorAuthorityRecordsTest final : public QObject {
    Q_OBJECT

private slots:
    void canonicalUint64AcceptsExactBoundaryValues();
    void canonicalUint64RejectsEveryAliasAndOverflowClass();
    void durableActivationGrammarIsClosed();
    void authorityRecordHasExactCanonicalOneLineShape();
    void authorityRecordRejectsFramingCanonicalAndGrammarDrift();
    void authorityRecordRejectsMissingUnknownAndWrongTypeFields();
    void authoritySerializerRejectsInvalidValues();
    void appliedRecordHasExactCanonicalOneLineShape();
    void appliedRecordRoundTripsEveryDurableActivationAndRevisionEdge();
    void appliedRecordRejectsMissingUnknownAndWrongTypeFields();
    void appliedRecordRejectsEveryScalarContractMutation();
    void appliedRecordRejectsFramingDuplicatesAndOversize();
    void appliedSerializerRejectsEveryInvalidInMemoryField();
};

void CompositorAuthorityRecordsTest::canonicalUint64AcceptsExactBoundaryValues()
{
    const QList<QPair<QString, quint64>> cases{
        {QStringLiteral("0"), 0},
        {QStringLiteral("1"), 1},
        {QStringLiteral("9"), 9},
        {QStringLiteral("10"), 10},
        {
            QStringLiteral("18446744073709551614"),
            std::numeric_limits<quint64>::max() - 1,
        },
        {
            QStringLiteral("18446744073709551615"),
            std::numeric_limits<quint64>::max(),
        },
    };
    for (const auto &[text, expected] : cases) {
        const auto parsed = parseCanonicalUint64(text);
        QVERIFY2(parsed.has_value(), qPrintable(text));
        QCOMPARE(*parsed, expected);
    }
}

void CompositorAuthorityRecordsTest::canonicalUint64RejectsEveryAliasAndOverflowClass()
{
    QString embeddedNull = QStringLiteral("10");
    embeddedNull[1] = QChar::Null;
    const QStringList invalid{
        QString(),
        QStringLiteral("00"),
        QStringLiteral("01"),
        QStringLiteral("-0"),
        QStringLiteral("-1"),
        QStringLiteral("+1"),
        QStringLiteral(" 1"),
        QStringLiteral("1 "),
        QStringLiteral("1\n"),
        QStringLiteral("1.0"),
        QStringLiteral("1e0"),
        QStringLiteral("18446744073709551616"),
        QStringLiteral("99999999999999999999"),
        QStringLiteral("100000000000000000000"),
        QString(QChar(0x0661)),
        embeddedNull,
    };
    for (const auto &text : invalid) {
        QVERIFY2(!parseCanonicalUint64(text), qPrintable(text));
    }
}

void CompositorAuthorityRecordsTest::durableActivationGrammarIsClosed()
{
    const QList<QPair<QString, ActivationRequirement>> accepted{
        {QStringLiteral("reload"), ActivationRequirement::Reload},
        {QStringLiteral("restart"), ActivationRequirement::Restart},
        {QStringLiteral("session"), ActivationRequirement::Session},
    };
    for (const auto &[name, requirement] : accepted) {
        const auto parsed = durableActivationRequirementFromName(name);
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, requirement);
        QCOMPARE(durableActivationRequirementName(requirement), name);
    }

    const QStringList rejected{
        QString(),
        QStringLiteral("none"),
        QStringLiteral("Reload"),
        QStringLiteral("restart "),
        QStringLiteral("session\n"),
        QStringLiteral("reboot"),
    };
    for (const auto &name : rejected) {
        QVERIFY(!durableActivationRequirementFromName(name));
    }
    QVERIFY(durableActivationRequirementName(
        ActivationRequirement::None
    ).isEmpty());
    QVERIFY(durableActivationRequirementName(
        static_cast<ActivationRequirement>(999)
    ).isEmpty());
}

void CompositorAuthorityRecordsTest::authorityRecordHasExactCanonicalOneLineShape()
{
    const AuthorityRecordV2 record{.authorityId = authorityId};
    const auto encoded = serializeAuthorityRecordV2(record);
    QVERIFY(encoded);
    QCOMPARE(
        *encoded.value,
        QByteArrayLiteral("{\"authorityId\":\"11111111111111111111111111111111\",\"formatVersion\":2}\n")
    );
    QVERIFY(encoded.value->size() <= maximumAuthorityRecordV2Bytes);

    const auto parsed = parseAuthorityRecordV2(*encoded.value);
    QVERIFY(parsed);
    QCOMPARE(*parsed.value, record);
}

void CompositorAuthorityRecordsTest::authorityRecordRejectsFramingCanonicalAndGrammarDrift()
{
    const auto valid = *serializeAuthorityRecordV2(
        AuthorityRecordV2{.authorityId = authorityId}
    ).value;
    QList<QByteArray> invalid{
        valid.chopped(1),
        valid + '\n',
        QByteArrayLiteral("\xef\xbb\xbf") + valid,
        QByteArrayLiteral(" {\"authorityId\":\"11111111111111111111111111111111\",\"formatVersion\":2}\n"),
        QByteArrayLiteral("{\"formatVersion\":2,\"authorityId\":\"11111111111111111111111111111111\"}\n"),
        QByteArrayLiteral("{\"authorityId\":\"11111111111111111111111111111111\",\"authorityId\":\"22222222222222222222222222222222\",\"formatVersion\":2}\n"),
        canonicalObject(QJsonObject{
            {QStringLiteral("authorityId"), QString(32, QLatin1Char('0'))},
            {QStringLiteral("formatVersion"), 2},
        }),
        canonicalObject(QJsonObject{
            {QStringLiteral("authorityId"), QString(32, QLatin1Char('A'))},
            {QStringLiteral("formatVersion"), 2},
        }),
        canonicalObject(QJsonObject{
            {QStringLiteral("authorityId"), authorityId},
            {QStringLiteral("formatVersion"), 1},
        }),
    };
    for (const auto &bytes : invalid) {
        QVERIFY2(!parseAuthorityRecordV2(bytes), bytes.constData());
    }

    const auto oversized = canonicalObject(QJsonObject{
        {QStringLiteral("authorityId"), QString(256, QLatin1Char('1'))},
        {QStringLiteral("formatVersion"), 2},
    });
    QVERIFY(oversized.size() > maximumAuthorityRecordV2Bytes);
    QVERIFY(!parseAuthorityRecordV2(oversized));
}

void CompositorAuthorityRecordsTest::authorityRecordRejectsMissingUnknownAndWrongTypeFields()
{
    const QJsonObject base{
        {QStringLiteral("authorityId"), authorityId},
        {QStringLiteral("formatVersion"), 2},
    };
    for (const auto &key : base.keys()) {
        auto missing = base;
        missing.remove(key);
        QVERIFY(!parseAuthorityRecordV2(canonicalObject(missing)));

        auto wrongType = base;
        wrongType.insert(key, QJsonValue::Null);
        QVERIFY(!parseAuthorityRecordV2(canonicalObject(wrongType)));
    }
    auto unknown = base;
    unknown.insert(QStringLiteral("revision"), QStringLiteral("0"));
    QVERIFY(!parseAuthorityRecordV2(canonicalObject(unknown)));
}

void CompositorAuthorityRecordsTest::authoritySerializerRejectsInvalidValues()
{
    const QStringList invalid{
        QString(),
        QString(32, QLatin1Char('0')),
        QString(31, QLatin1Char('1')),
        QString(33, QLatin1Char('1')),
        QString(32, QLatin1Char('A')),
        QStringLiteral("11111111-1111-1111-1111-111111111111"),
    };
    for (const auto &id : invalid) {
        QVERIFY(!serializeAuthorityRecordV2({.authorityId = id}));
    }
}

void CompositorAuthorityRecordsTest::appliedRecordHasExactCanonicalOneLineShape()
{
    const auto encoded = serializeAppliedRecordV2(validApplied());
    QVERIFY(encoded);
    QCOMPARE(
        *encoded.value,
        QByteArrayLiteral(
            "{\"activationNonce\":\"22222222222222222222222222222222\","
            "\"authorityId\":\"11111111111111111111111111111111\","
            "\"entrypoint\":\"hyprland.lua\",\"formatVersion\":2,"
            "\"generation\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
            "\"requiredActivation\":\"reload\",\"revision\":\"42\","
            "\"snapshotDigest\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}\n"
        )
    );
}

void CompositorAuthorityRecordsTest::appliedRecordRoundTripsEveryDurableActivationAndRevisionEdge()
{
    const QList<quint64> revisions{
        0,
        1,
        std::numeric_limits<quint64>::max() - 1,
        std::numeric_limits<quint64>::max(),
    };
    const QList<ActivationRequirement> requirements{
        ActivationRequirement::Reload,
        ActivationRequirement::Restart,
        ActivationRequirement::Session,
    };
    for (const auto revision : revisions) {
        for (const auto requirement : requirements) {
            const auto record = validApplied(revision, requirement);
            const auto encoded = serializeAppliedRecordV2(record);
            QVERIFY(encoded);
            QVERIFY(encoded.value->endsWith('\n'));
            QVERIFY(!encoded.value->endsWith("\n\n"));
            QVERIFY(encoded.value->size() <= maximumAppliedRecordV2Bytes);
            const auto parsed = parseAppliedRecordV2(*encoded.value);
            QVERIFY(parsed);
            QCOMPARE(*parsed.value, record);
        }
    }
}

void CompositorAuthorityRecordsTest::appliedRecordRejectsMissingUnknownAndWrongTypeFields()
{
    const auto base = appliedObject(validApplied());
    for (const auto &key : base.keys()) {
        auto missing = base;
        missing.remove(key);
        QVERIFY2(!parseAppliedRecordV2(canonicalObject(missing)),
                 qPrintable(key));

        auto wrongType = base;
        wrongType.insert(key, QJsonValue::Null);
        QVERIFY2(!parseAppliedRecordV2(canonicalObject(wrongType)),
                 qPrintable(key));
    }
    auto unknown = base;
    unknown.insert(QStringLiteral("createdAt"), QStringLiteral("never"));
    QVERIFY(!parseAppliedRecordV2(canonicalObject(unknown)));
}

void CompositorAuthorityRecordsTest::appliedRecordRejectsEveryScalarContractMutation()
{
    const auto base = appliedObject(validApplied());
    const QList<QPair<QString, QJsonValue>> mutations{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("formatVersion"), 3},
        {QStringLiteral("authorityId"), QString(32, QLatin1Char('0'))},
        {QStringLiteral("authorityId"), QString(32, QLatin1Char('A'))},
        {QStringLiteral("revision"), QStringLiteral("00")},
        {QStringLiteral("revision"), QStringLiteral("+1")},
        {
            QStringLiteral("revision"),
            QStringLiteral("18446744073709551616"),
        },
        {QStringLiteral("snapshotDigest"), QString(63, QLatin1Char('a'))},
        {QStringLiteral("snapshotDigest"), QString(64, QLatin1Char('A'))},
        {QStringLiteral("generation"), QString(65, QLatin1Char('b'))},
        {QStringLiteral("generation"), QString(64, QLatin1Char('G'))},
        {QStringLiteral("activationNonce"), QString(32, QLatin1Char('0'))},
        {QStringLiteral("activationNonce"), authorityId},
        {QStringLiteral("entrypoint"), QStringLiteral("/hyprland.lua")},
        {QStringLiteral("entrypoint"), QStringLiteral("modules/hyprland.lua")},
        {QStringLiteral("entrypoint"), QStringLiteral("Hyprland.lua")},
        {QStringLiteral("requiredActivation"), QStringLiteral("none")},
        {QStringLiteral("requiredActivation"), QStringLiteral("Reload")},
        {QStringLiteral("requiredActivation"), QStringLiteral("reboot")},
    };
    for (const auto &[key, value] : mutations) {
        auto mutated = base;
        mutated.insert(key, value);
        QVERIFY2(!parseAppliedRecordV2(canonicalObject(mutated)),
                 qPrintable(key + QLatin1Char(':') + value.toVariant().toString()));
    }
}

void CompositorAuthorityRecordsTest::appliedRecordRejectsFramingDuplicatesAndOversize()
{
    const auto valid = *serializeAppliedRecordV2(validApplied()).value;
    QVERIFY(!parseAppliedRecordV2(valid.chopped(1)));
    QVERIFY(!parseAppliedRecordV2(valid + '\n'));
    QVERIFY(!parseAppliedRecordV2(QByteArrayLiteral(" ") + valid));

    auto duplicate = valid;
    const auto member = QByteArrayLiteral(
        "\"activationNonce\":\"22222222222222222222222222222222\","
    );
    duplicate.insert(1, member);
    QVERIFY(!parseAppliedRecordV2(duplicate));

    auto oversized = appliedObject(validApplied());
    oversized.insert(QStringLiteral("unknown"), QString(1024, QLatin1Char('x')));
    const auto oversizedBytes = canonicalObject(oversized, 4096);
    QVERIFY(oversizedBytes.size() > maximumAppliedRecordV2Bytes);
    QVERIFY(!parseAppliedRecordV2(oversizedBytes));
}

void CompositorAuthorityRecordsTest::appliedSerializerRejectsEveryInvalidInMemoryField()
{
    {
        auto record = validApplied();
        record.authorityId = QString(32, QLatin1Char('0'));
        QVERIFY(!serializeAppliedRecordV2(record));
    }
    {
        auto record = validApplied();
        record.snapshotDigest = QString(63, QLatin1Char('a'));
        QVERIFY(!serializeAppliedRecordV2(record));
    }
    {
        auto record = validApplied();
        record.generation = QString(64, QLatin1Char('G'));
        QVERIFY(!serializeAppliedRecordV2(record));
    }
    {
        auto record = validApplied();
        record.activationNonce = record.authorityId;
        QVERIFY(!serializeAppliedRecordV2(record));
    }
    {
        auto record = validApplied();
        record.entrypoint = QStringLiteral("/hyprland.lua");
        QVERIFY(!serializeAppliedRecordV2(record));
    }
    {
        auto record = validApplied();
        record.requiredActivation = ActivationRequirement::None;
        QVERIFY(!serializeAppliedRecordV2(record));
    }
}

QTEST_APPLESS_MAIN(CompositorAuthorityRecordsTest)

#include "compositor_authority_records_test.moc"
