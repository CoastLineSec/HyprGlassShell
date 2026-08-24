#include "compositord/legacy_transaction_records.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include <array>
#include <limits>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr char preDeviceCatalogDigest[] =
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0";
constexpr char preBindingsCatalogDigest[] =
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388";
constexpr char preSharedActionDigest[] =
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2";

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool isLowerSha256(const QStringView value)
{
    if (value.size() != 64) {
        return false;
    }
    for (const auto character : value) {
        if (!((character >= QLatin1Char('0')
               && character <= QLatin1Char('9'))
              || (character >= QLatin1Char('a')
                  && character <= QLatin1Char('f')))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] QJsonObject objectFromBytes(
    const QByteArrayView bytes,
    const qsizetype maximumBytes = 4 * 1024 * 1024,
    const int maximumDepth = 64
)
{
    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumBytes, maximumDepth
    );
    return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QByteArray canonicalBytes(const QJsonObject &object)
{
    auto bytes = JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QJsonValue nestedValue(const int depth)
{
    QJsonValue value = QStringLiteral("leaf");
    for (int index = 0; index < depth; ++index) {
        value = QJsonArray{value};
    }
    return value;
}

[[nodiscard]] LegacyAppliedRecordV1 appliedRecord(
    const quint64 revision,
    const QString &snapshotDigest,
    const ActivationRequirement requirement = ActivationRequirement::Reload
)
{
    return {
        .revision = revision,
        .snapshotDigest = snapshotDigest,
        .generation = QString(64, QLatin1Char('g')),
        .activationNonce =
            QStringLiteral("0123456789abcdef0123456789abcdef"),
        .entrypoint = QStringLiteral("hyprland.lua"),
        .requiredActivation = requirement,
    };
}

} // namespace

class CompositorLegacyTransactionRecordsTest final : public QObject {
    Q_OBJECT

private:
    Catalog catalog_;
    ActionCatalog actions_;
    DesiredState defaults_;

    [[nodiscard]] DesiredState candidate(
        const quint64 revision,
        const QString &catalogDigest = {},
        const QString &actionDigest = {}
    ) const
    {
        auto result = defaults_;
        result.revision = revision;
        if (!catalogDigest.isEmpty()) {
            result.catalogDigest = catalogDigest;
        }
        if (!actionDigest.isEmpty()) {
            result.actionCatalogDigest = actionDigest;
        }
        if (result.actionCatalogDigest
            == QLatin1String(preSharedActionDigest)) {
            result.workspaceRules.clear();
        }
        return result;
    }

    [[nodiscard]] LegacyOrdinaryPendingRecordV1 pendingRecord(
        const LegacyOrdinaryPendingKindV1 kind =
            LegacyOrdinaryPendingKindV1::Apply,
        const LegacyOrdinaryPendingPhaseV1 phase =
            LegacyOrdinaryPendingPhaseV1::Prepared,
        const bool withBefore = false,
        const quint64 expectedRevision = 7,
        const QString &catalogDigest = {},
        const QString &actionDigest = {}
    ) const
    {
        const auto revision = kind == LegacyOrdinaryPendingKindV1::Apply
            ? expectedRevision
            : expectedRevision + 1;
        auto state = candidate(revision, catalogDigest, actionDigest);
        auto standalone = serializeDesiredState(state);
        const auto digest = sha256(standalone);
        auto after = appliedRecord(
            revision, digest, ActivationRequirement::Restart
        );
        after.generation = QString(64, QLatin1Char('A'));
        after.activationNonce =
            QStringLiteral("after nonce accepts spaces.........");
        after.entrypoint = QStringLiteral("relative/legacy.lua");

        std::optional<LegacyAppliedRecordV1> before;
        if (withBefore) {
            before = appliedRecord(
                expectedRevision == 0 ? 0 : expectedRevision - 1,
                QString(64, QLatin1Char('?')),
                ActivationRequirement::Session
            );
            before->generation = QString(64, QLatin1Char('Z'));
            before->activationNonce =
                QStringLiteral("before nonce is deliberately lax..");
            before->entrypoint = QStringLiteral("../prior entry.lua");
        }

        return {
            .kind = kind,
            .phase = phase,
            .expectedRevision = expectedRevision,
            .beforeDesiredDigest =
                kind == LegacyOrdinaryPendingKindV1::Apply
                    ? digest
                    : QString(64, QLatin1Char('b')),
            .candidateSnapshot = std::move(state),
            .candidateSnapshotBytes = std::move(standalone),
            .snapshotDigest = digest,
            .afterActivation = std::move(after),
            .beforeActivation = std::move(before),
        };
    }

    [[nodiscard]] QByteArray encode(
        const LegacyOrdinaryPendingRecordV1 &record
    ) const
    {
        const auto encoded = serializeLegacyOrdinaryPendingRecordV1(
            record, catalog_, actions_
        );
        Q_ASSERT(encoded.has_value());
        return *encoded;
    }

    [[nodiscard]] std::optional<LegacyOrdinaryPendingRecordV1> decode(
        const QByteArrayView bytes
    ) const
    {
        return parseLegacyOrdinaryPendingRecordV1(
            bytes, catalog_, actions_
        );
    }

    // Differential helper proposal: the active transaction test can pass the
    // bytes it captures from its private v1 writer into this exact shape. The
    // helper deliberately compares raw bytes on both sides; parsing equality
    // alone would miss an encoder-domain drift.
    void verifyActiveByteFixture(
        const QByteArrayView activeAppliedBytes,
        const QByteArrayView activePendingBytes
    ) const
    {
        const auto applied = parseLegacyAppliedRecordV1(activeAppliedBytes);
        QVERIFY(applied);
        const auto appliedRoundTrip = serializeLegacyAppliedRecordV1(*applied);
        QVERIFY(appliedRoundTrip);
        QCOMPARE(*appliedRoundTrip, activeAppliedBytes.toByteArray());

        const auto pending = decode(activePendingBytes);
        QVERIFY(pending);
        const auto pendingRoundTrip = serializeLegacyOrdinaryPendingRecordV1(
            *pending, catalog_, actions_
        );
        QVERIFY(pendingRoundTrip);
        QCOMPARE(*pendingRoundTrip, activePendingBytes.toByteArray());
    }

private slots:
    void initTestCase()
    {
        const auto catalog = parseCatalog(readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V1_CATALOG_FILE
        )));
        QVERIFY(catalog);
        catalog_ = *catalog.value;

        const auto actions = parseActionCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_ACTION_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V1_SCHEMA_FILE))
        );
        QVERIFY(actions);
        actions_ = *actions.value;
        defaults_ = defaultDesiredState(catalog_, actions_);
    }

    void appliedExactBytesHashAndLaxRecoveredGrammar()
    {
        const LegacyAppliedRecordV1 record{
            .revision = 7,
            .snapshotDigest = QString(64, QLatin1Char('s')),
            .generation = QString(64, QLatin1Char('G')),
            .activationNonce =
                QStringLiteral("nonce with spaces ................"),
            .entrypoint = QStringLiteral("../odd entry.lua"),
            .requiredActivation = ActivationRequirement::Restart,
        };
        const auto encoded = serializeLegacyAppliedRecordV1(record);
        QVERIFY(encoded);
        const auto expected = QByteArrayLiteral(
            "{\"activationNonce\":\"nonce with spaces ................\","
            "\"entrypoint\":\"../odd entry.lua\",\"formatVersion\":1,"
            "\"generation\":\"GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG"
            "GGGGGGGGGGGGGGGGGGGGGGGG\",\"requiredActivation\":\"restart\","
            "\"revision\":\"7\",\"snapshotDigest\":\"ssssssssssssssssssssssss"
            "ssssssssssssssssssssssssssssssssssssssss\"}\n"
        );
        QCOMPARE(*encoded, expected);
        QCOMPARE(
            sha256(*encoded),
            QStringLiteral(
                "8092ca1f9fbaffa7360e901800191fde341c35e98358c8e64be5b2854b1c65b9"
            )
        );
        const auto parsed = parseLegacyAppliedRecordV1(*encoded);
        QVERIFY(parsed);
        QCOMPARE(*parsed, record);

        // These fields are intentionally accepted because this codec recovers
        // the exact active v1 grammar. Qualification must separately reject
        // them before adoption as v2 evidence.
        QVERIFY(!isLowerSha256(record.snapshotDigest));
        QVERIFY(!isLowerSha256(record.generation));
        QVERIFY(record.entrypoint != QStringLiteral("hyprland.lua"));
    }

    void appliedAllKeysTypesEnumsAndRevisionGrammar()
    {
        const auto validRecord = appliedRecord(
            9, QString(64, QLatin1Char('d'))
        );
        const auto valid = serializeLegacyAppliedRecordV1(validRecord);
        QVERIFY(valid);
        const auto base = objectFromBytes(*valid);
        QVERIFY(!base.isEmpty());

        const QStringList keys{
            QStringLiteral("formatVersion"),
            QStringLiteral("revision"),
            QStringLiteral("snapshotDigest"),
            QStringLiteral("generation"),
            QStringLiteral("activationNonce"),
            QStringLiteral("entrypoint"),
            QStringLiteral("requiredActivation"),
        };
        for (const auto &key : keys) {
            auto missing = base;
            missing.remove(key);
            QVERIFY2(
                !parseLegacyAppliedRecordV1(canonicalBytes(missing)),
                qPrintable(QStringLiteral("missing %1").arg(key))
            );
        }

        const std::array<std::pair<QString, QJsonValue>, 7> wrongTypes{{
            {QStringLiteral("formatVersion"), QStringLiteral("1")},
            {QStringLiteral("revision"), 9},
            {QStringLiteral("snapshotDigest"), true},
            {QStringLiteral("generation"), QJsonArray{}},
            {QStringLiteral("activationNonce"), QJsonObject{}},
            {QStringLiteral("entrypoint"), QJsonValue::Null},
            {QStringLiteral("requiredActivation"), 1},
        }};
        for (const auto &[key, value] : wrongTypes) {
            auto mutated = base;
            mutated.insert(key, value);
            QVERIFY2(
                !parseLegacyAppliedRecordV1(canonicalBytes(mutated)),
                qPrintable(QStringLiteral("wrong type %1").arg(key))
            );
        }

        for (const auto &name : {
                 QStringLiteral("reload"),
                 QStringLiteral("restart"),
                 QStringLiteral("session"),
             }) {
            auto object = base;
            object.insert(QStringLiteral("requiredActivation"), name);
            QVERIFY(parseLegacyAppliedRecordV1(canonicalBytes(object)));
        }
        for (const auto &name : {
                 QString(),
                 QStringLiteral("none"),
                 QStringLiteral("Reload"),
             }) {
            auto object = base;
            object.insert(QStringLiteral("requiredActivation"), name);
            QVERIFY(!parseLegacyAppliedRecordV1(canonicalBytes(object)));
        }

        const QStringList invalidRevisions{
            QString(),
            QStringLiteral("00"),
            QStringLiteral("01"),
            QStringLiteral("+1"),
            QStringLiteral("-1"),
            QStringLiteral(" 1"),
            QStringLiteral("1 "),
            QStringLiteral("18446744073709551616"),
            QStringLiteral("1a"),
        };
        for (const auto &revision : invalidRevisions) {
            auto object = base;
            object.insert(QStringLiteral("revision"), revision);
            QVERIFY2(
                !parseLegacyAppliedRecordV1(canonicalBytes(object)),
                qPrintable(QStringLiteral("revision %1").arg(revision))
            );
        }
        auto maximum = base;
        maximum.insert(
            QStringLiteral("revision"),
            QStringLiteral("18446744073709551615")
        );
        const auto maximumParsed = parseLegacyAppliedRecordV1(
            canonicalBytes(maximum)
        );
        QVERIFY(maximumParsed);
        QCOMPARE(
            maximumParsed->revision,
            std::numeric_limits<quint64>::max()
        );

        auto unknown = base;
        unknown.insert(QStringLiteral("authorityId"), QStringLiteral("x"));
        QVERIFY(!parseLegacyAppliedRecordV1(canonicalBytes(unknown)));
        auto wrongVersion = base;
        wrongVersion.insert(QStringLiteral("formatVersion"), 2);
        QVERIFY(!parseLegacyAppliedRecordV1(canonicalBytes(wrongVersion)));
    }

    void appliedCanonicalFramingDuplicateBomDepthAndBounds()
    {
        auto record = appliedRecord(1, QString(64, QLatin1Char('d')));
        const auto encoded = serializeLegacyAppliedRecordV1(record);
        QVERIFY(encoded);

        QVERIFY(!parseLegacyAppliedRecordV1(encoded->chopped(1)));
        QVERIFY(!parseLegacyAppliedRecordV1(*encoded + '\n'));
        QVERIFY(!parseLegacyAppliedRecordV1(*encoded + '\r'));
        QVERIFY(!parseLegacyAppliedRecordV1(QByteArrayLiteral(" ") + *encoded));
        QVERIFY(!parseLegacyAppliedRecordV1(
            QByteArray::fromHex("efbbbf") + *encoded
        ));

        auto pretty = *encoded;
        pretty.insert(1, ' ');
        QVERIFY(!parseLegacyAppliedRecordV1(pretty));

        auto duplicate = *encoded;
        duplicate.insert(1, QByteArrayLiteral("\"revision\":\"1\","));
        QVERIFY(!parseLegacyAppliedRecordV1(duplicate));

        auto tooDeep = objectFromBytes(*encoded);
        tooDeep.insert(QStringLiteral("extra"), nestedValue(17));
        QVERIFY(!parseLegacyAppliedRecordV1(canonicalBytes(tooDeep)));

        auto shortestNonce = record;
        shortestNonce.activationNonce = QString(32, QLatin1Char('n'));
        QVERIFY(serializeLegacyAppliedRecordV1(shortestNonce));
        auto longestNonce = record;
        longestNonce.activationNonce = QString(128, QLatin1Char('n'));
        QVERIFY(serializeLegacyAppliedRecordV1(longestNonce));
        auto shortNonce = record;
        shortNonce.activationNonce = QString(31, QLatin1Char('n'));
        QVERIFY(!serializeLegacyAppliedRecordV1(shortNonce));
        auto longNonce = record;
        longNonce.activationNonce = QString(129, QLatin1Char('n'));
        QVERIFY(!serializeLegacyAppliedRecordV1(longNonce));
        auto shortDigest = record;
        shortDigest.snapshotDigest.chop(1);
        QVERIFY(!serializeLegacyAppliedRecordV1(shortDigest));
        auto longGeneration = record;
        longGeneration.generation.append(QLatin1Char('g'));
        QVERIFY(!serializeLegacyAppliedRecordV1(longGeneration));
        auto emptyEntrypoint = record;
        emptyEntrypoint.entrypoint.clear();
        QVERIFY(!serializeLegacyAppliedRecordV1(emptyEntrypoint));
        auto none = record;
        none.requiredActivation = ActivationRequirement::None;
        QVERIFY(!serializeLegacyAppliedRecordV1(none));

        const auto base = serializeLegacyAppliedRecordV1(record);
        QVERIFY(base);
        record.entrypoint.append(QString(
            maximumLegacyAppliedRecordV1Bytes - base->size(),
            QLatin1Char('x')
        ));
        const auto exactMaximum = serializeLegacyAppliedRecordV1(record);
        QVERIFY(exactMaximum);
        QCOMPARE(
            exactMaximum->size(), maximumLegacyAppliedRecordV1Bytes
        );
        QVERIFY(parseLegacyAppliedRecordV1(*exactMaximum));
        record.entrypoint.append(QLatin1Char('x'));
        QVERIFY(!serializeLegacyAppliedRecordV1(record));
        QVERIFY(!parseLegacyAppliedRecordV1(
            *exactMaximum + QByteArrayLiteral("x")
        ));
    }

    void pendingAllKindsPhasesAndNullableBeforeRoundTrip()
    {
        const std::array kinds{
            LegacyOrdinaryPendingKindV1::Apply,
            LegacyOrdinaryPendingKindV1::Recovery,
            LegacyOrdinaryPendingKindV1::DisplayPreview,
        };
        const std::array phases{
            LegacyOrdinaryPendingPhaseV1::Prepared,
            LegacyOrdinaryPendingPhaseV1::Committing,
        };
        for (const auto kind : kinds) {
            for (const auto phase : phases) {
                for (const bool withBefore : {false, true}) {
                    const auto record = pendingRecord(
                        kind, phase, withBefore
                    );
                    const auto encoded = serializeLegacyOrdinaryPendingRecordV1(
                        record, catalog_, actions_
                    );
                    QVERIFY(encoded);
                    const auto parsed = decode(*encoded);
                    QVERIFY(parsed);
                    QCOMPARE(*parsed, record);
                    const auto roundTrip =
                        serializeLegacyOrdinaryPendingRecordV1(
                            *parsed, catalog_, actions_
                        );
                    QVERIFY(roundTrip);
                    QCOMPARE(*roundTrip, *encoded);
                }
            }
        }
    }

    void pendingExactBytesHashesAndDifferentialFixture()
    {
        const auto record = pendingRecord(
            LegacyOrdinaryPendingKindV1::Apply,
            LegacyOrdinaryPendingPhaseV1::Committing,
            true,
            17
        );
        const auto pending = encode(record);
        const auto applied = serializeLegacyAppliedRecordV1(
            record.afterActivation
        );
        QVERIFY(applied);

        const auto root = objectFromBytes(pending);
        QCOMPARE(root.value(QStringLiteral("formatVersion")).toInt(), 1);
        QCOMPARE(root.value(QStringLiteral("kind")).toString(),
                 QStringLiteral("apply"));
        QCOMPARE(root.value(QStringLiteral("phase")).toString(),
                 QStringLiteral("committing"));
        QCOMPARE(root.value(QStringLiteral("expectedRevision")).toString(),
                 QStringLiteral("17"));
        QCOMPARE(
            root.value(QStringLiteral("candidateSnapshot")).toObject(),
            objectFromBytes(record.candidateSnapshotBytes)
        );
        QCOMPARE(pending, canonicalBytes(root));
        QCOMPARE(
            record.snapshotDigest,
            sha256(record.candidateSnapshotBytes)
        );
        QVERIFY(pending.endsWith('\n'));

        // These receipts pin the recovered canonicalJson and LF-included
        // domains. If an authenticated predecessor fixture changes, update
        // only after comparing it to bytes emitted by active transaction.cpp.
        QCOMPARE(
            sha256(*applied),
            QStringLiteral(
                "f926a390402fa9b7eda6a2992d829fb26d39b471549d842aa16ff9fec59e415f"
            )
        );
        QCOMPARE(
            sha256(pending),
            QStringLiteral(
                "7f13bec32e09e4138e1483731d1d7e691d5ddee4f09f68c1ac3c9ba4d93b0300"
            )
        );

        verifyActiveByteFixture(*applied, pending);
    }

    void pendingAllKeysTypesEnumsAndCanonicalFraming()
    {
        const auto valid = encode(pendingRecord());
        const auto base = objectFromBytes(valid);
        QVERIFY(!base.isEmpty());
        const QStringList keys{
            QStringLiteral("formatVersion"),
            QStringLiteral("kind"),
            QStringLiteral("phase"),
            QStringLiteral("expectedRevision"),
            QStringLiteral("beforeDesiredDigest"),
            QStringLiteral("candidateSnapshot"),
            QStringLiteral("snapshotDigest"),
            QStringLiteral("afterActivation"),
            QStringLiteral("beforeActivation"),
        };
        for (const auto &key : keys) {
            auto missing = base;
            missing.remove(key);
            QVERIFY2(
                !decode(canonicalBytes(missing)),
                qPrintable(QStringLiteral("missing %1").arg(key))
            );
        }

        const std::array<std::pair<QString, QJsonValue>, 9> wrongTypes{{
            {QStringLiteral("formatVersion"), QStringLiteral("1")},
            {QStringLiteral("kind"), 1},
            {QStringLiteral("phase"), false},
            {QStringLiteral("expectedRevision"), 7},
            {QStringLiteral("beforeDesiredDigest"), QJsonArray{}},
            {QStringLiteral("candidateSnapshot"), QStringLiteral("object")},
            {QStringLiteral("snapshotDigest"), QJsonObject{}},
            {QStringLiteral("afterActivation"), QJsonValue::Null},
            {QStringLiteral("beforeActivation"), 1},
        }};
        for (const auto &[key, value] : wrongTypes) {
            auto mutated = base;
            mutated.insert(key, value);
            QVERIFY2(
                !decode(canonicalBytes(mutated)),
                qPrintable(QStringLiteral("wrong type %1").arg(key))
            );
        }

        for (const auto &name : {
                 QStringLiteral("apply"),
                 QStringLiteral("recovery"),
                 QStringLiteral("display-preview"),
             }) {
            auto record = pendingRecord(
                name == QStringLiteral("apply")
                    ? LegacyOrdinaryPendingKindV1::Apply
                    : name == QStringLiteral("recovery")
                        ? LegacyOrdinaryPendingKindV1::Recovery
                        : LegacyOrdinaryPendingKindV1::DisplayPreview
            );
            auto object = objectFromBytes(encode(record));
            QCOMPARE(object.value(QStringLiteral("kind")).toString(), name);
            QVERIFY(decode(canonicalBytes(object)));
        }
        for (const auto &name : {
                 QString(),
                 QStringLiteral("Apply"),
                 QStringLiteral("display_preview"),
                 QStringLiteral("journal"),
             }) {
            auto mutated = base;
            mutated.insert(QStringLiteral("kind"), name);
            QVERIFY(!decode(canonicalBytes(mutated)));
        }
        for (const auto &name : {
                 QStringLiteral("prepared"),
                 QStringLiteral("committing"),
             }) {
            auto mutated = base;
            mutated.insert(QStringLiteral("phase"), name);
            QVERIFY(decode(canonicalBytes(mutated)));
        }
        for (const auto &name : {
                 QString(),
                 QStringLiteral("Prepared"),
                 QStringLiteral("committed"),
             }) {
            auto mutated = base;
            mutated.insert(QStringLiteral("phase"), name);
            QVERIFY(!decode(canonicalBytes(mutated)));
        }

        auto unknown = base;
        unknown.insert(QStringLiteral("authorityId"), QStringLiteral("x"));
        QVERIFY(!decode(canonicalBytes(unknown)));
        auto wrongVersion = base;
        wrongVersion.insert(QStringLiteral("formatVersion"), 2);
        QVERIFY(!decode(canonicalBytes(wrongVersion)));

        QVERIFY(!decode(valid.chopped(1)));
        QVERIFY(!decode(valid + '\n'));
        QVERIFY(!decode(valid + '\r'));
        QVERIFY(!decode(QByteArrayLiteral(" ") + valid));
        QVERIFY(!decode(QByteArray::fromHex("efbbbf") + valid));
        auto pretty = valid;
        pretty.insert(1, ' ');
        QVERIFY(!decode(pretty));
        auto duplicate = valid;
        duplicate.insert(1, QByteArrayLiteral("\"kind\":\"apply\","));
        QVERIFY(!decode(duplicate));
        auto tooDeep = base;
        tooDeep.insert(QStringLiteral("extra"), nestedValue(65));
        QVERIFY(!decode(canonicalBytes(tooDeep)));

        const QStringList invalidRevisions{
            QString(),
            QStringLiteral("00"),
            QStringLiteral("+1"),
            QStringLiteral("-1"),
            QStringLiteral("18446744073709551616"),
        };
        for (const auto &revision : invalidRevisions) {
            auto mutated = base;
            mutated.insert(QStringLiteral("expectedRevision"), revision);
            QVERIFY(!decode(canonicalBytes(mutated)));
        }
    }

    void pendingFiniteAuthorityLineagesAndExactSourceBytes()
    {
        const std::array catalogDigests{
            QString::fromLatin1(reviewedCatalogDigest),
            QString::fromLatin1(preDeviceCatalogDigest),
            QString::fromLatin1(preBindingsCatalogDigest),
        };
        const std::array actionDigests{
            QString::fromLatin1(reviewedActionCatalogDigest),
            QString::fromLatin1(preSharedActionDigest),
        };

        for (const auto &catalogDigest : catalogDigests) {
            for (const auto &actionDigest : actionDigests) {
                const auto record = pendingRecord(
                    LegacyOrdinaryPendingKindV1::Apply,
                    LegacyOrdinaryPendingPhaseV1::Prepared,
                    false,
                    21,
                    catalogDigest,
                    actionDigest
                );
                const auto encoded = serializeLegacyOrdinaryPendingRecordV1(
                    record, catalog_, actions_
                );
                QVERIFY2(
                    encoded,
                    qPrintable(catalogDigest + QLatin1Char('/') + actionDigest)
                );
                const auto parsed = decode(*encoded);
                QVERIFY(parsed);
                QCOMPARE(parsed->candidateSnapshotBytes,
                         record.candidateSnapshotBytes);
                QCOMPARE(serializeDesiredState(parsed->candidateSnapshot),
                         record.candidateSnapshotBytes);
                QCOMPARE(*serializeLegacyOrdinaryPendingRecordV1(
                             *parsed, catalog_, actions_
                         ),
                         *encoded);
            }
        }

        const auto rejectUnknown = [this](
            const QString &catalogDigest,
            const QString &actionDigest
        ) {
            const auto record = pendingRecord(
                LegacyOrdinaryPendingKindV1::Apply,
                LegacyOrdinaryPendingPhaseV1::Prepared,
                false,
                21,
                catalogDigest,
                actionDigest
            );
            return serializeLegacyOrdinaryPendingRecordV1(
                record, catalog_, actions_
            );
        };
        QVERIFY(!rejectUnknown(
            QString(64, QLatin1Char('0')),
            QString::fromLatin1(reviewedActionCatalogDigest)
        ));
        QVERIFY(!rejectUnknown(
            QString::fromLatin1(reviewedCatalogDigest),
            QString(64, QLatin1Char('0'))
        ));

        auto protectedCollision = pendingRecord(
            LegacyOrdinaryPendingKindV1::Apply,
            LegacyOrdinaryPendingPhaseV1::Prepared,
            false,
            21,
            QString::fromLatin1(reviewedCatalogDigest),
            QString::fromLatin1(preSharedActionDigest)
        );
        protectedCollision.candidateSnapshot.workspaceRules =
            defaults_.workspaceRules;
        protectedCollision.candidateSnapshotBytes = serializeDesiredState(
            protectedCollision.candidateSnapshot
        );
        const auto collisionDigest = sha256(
            protectedCollision.candidateSnapshotBytes
        );
        protectedCollision.snapshotDigest = collisionDigest;
        protectedCollision.beforeDesiredDigest = collisionDigest;
        protectedCollision.afterActivation.snapshotDigest = collisionDigest;
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            protectedCollision, catalog_, actions_
        ));
    }

    void pendingLfIncludedDigestRevisionAndOverflowRelations()
    {
        const auto apply = pendingRecord();
        const auto fullDigest = sha256(apply.candidateSnapshotBytes);
        const auto noLfDigest = sha256(QByteArrayView(
            apply.candidateSnapshotBytes
        ).first(apply.candidateSnapshotBytes.size() - 1));
        QVERIFY(fullDigest != noLfDigest);
        QCOMPARE(apply.snapshotDigest, fullDigest);

        auto wrongDomain = objectFromBytes(encode(apply));
        wrongDomain.insert(QStringLiteral("snapshotDigest"), noLfDigest);
        auto after = wrongDomain.value(
            QStringLiteral("afterActivation")
        ).toObject();
        after.insert(QStringLiteral("snapshotDigest"), noLfDigest);
        wrongDomain.insert(QStringLiteral("afterActivation"), after);
        wrongDomain.insert(QStringLiteral("beforeDesiredDigest"), noLfDigest);
        QVERIFY(!decode(canonicalBytes(wrongDomain)));

        auto applyWrongRevision = objectFromBytes(encode(apply));
        applyWrongRevision.insert(
            QStringLiteral("expectedRevision"), QStringLiteral("6")
        );
        QVERIFY(!decode(canonicalBytes(applyWrongRevision)));

        for (const auto kind : {
                 LegacyOrdinaryPendingKindV1::Recovery,
                 LegacyOrdinaryPendingKindV1::DisplayPreview,
             }) {
            const auto advancing = pendingRecord(
                kind,
                LegacyOrdinaryPendingPhaseV1::Prepared,
                false,
                7
            );
            auto wrongRevision = objectFromBytes(encode(advancing));
            wrongRevision.insert(
                QStringLiteral("expectedRevision"), QStringLiteral("8")
            );
            QVERIFY(!decode(canonicalBytes(wrongRevision)));

            auto overflow = objectFromBytes(encode(advancing));
            overflow.insert(
                QStringLiteral("expectedRevision"),
                QStringLiteral("18446744073709551615")
            );
            QVERIFY(!decode(canonicalBytes(overflow)));

            const auto maximum = pendingRecord(
                kind,
                LegacyOrdinaryPendingPhaseV1::Prepared,
                true,
                std::numeric_limits<quint64>::max() - 1
            );
            const auto maximumBytes =
                serializeLegacyOrdinaryPendingRecordV1(
                    maximum, catalog_, actions_
                );
            QVERIFY(maximumBytes);
            const auto parsed = decode(*maximumBytes);
            QVERIFY(parsed);
            QCOMPARE(
                parsed->candidateSnapshot.revision,
                std::numeric_limits<quint64>::max()
            );
        }

        const auto maximumApply = pendingRecord(
            LegacyOrdinaryPendingKindV1::Apply,
            LegacyOrdinaryPendingPhaseV1::Prepared,
            false,
            std::numeric_limits<quint64>::max()
        );
        QVERIFY(serializeLegacyOrdinaryPendingRecordV1(
            maximumApply, catalog_, actions_
        ));
    }

    void pendingTypedCoherenceAppliedMutationAndSizeBoundary()
    {
        auto record = pendingRecord(
            LegacyOrdinaryPendingKindV1::Apply,
            LegacyOrdinaryPendingPhaseV1::Prepared,
            true
        );
        auto wrongBytes = record;
        wrongBytes.candidateSnapshotBytes.append('\n');
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            wrongBytes, catalog_, actions_
        ));
        auto wrongTyped = record;
        ++wrongTyped.candidateSnapshot.revision;
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            wrongTyped, catalog_, actions_
        ));
        auto wrongOuter = record;
        wrongOuter.snapshotDigest = QString(64, QLatin1Char('0'));
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            wrongOuter, catalog_, actions_
        ));
        auto wrongAfterRevision = record;
        ++wrongAfterRevision.afterActivation.revision;
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            wrongAfterRevision, catalog_, actions_
        ));
        auto wrongAfterDigest = record;
        wrongAfterDigest.afterActivation.snapshotDigest =
            QString(64, QLatin1Char('0'));
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            wrongAfterDigest, catalog_, actions_
        ));
        auto invalidBeforeDigest = record;
        invalidBeforeDigest.beforeDesiredDigest =
            QString(64, QLatin1Char('G'));
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            invalidBeforeDigest, catalog_, actions_
        ));

        auto invalidBefore = record;
        invalidBefore.beforeActivation->activationNonce =
            QString(31, QLatin1Char('n'));
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            invalidBefore, catalog_, actions_
        ));

        const auto base = serializeLegacyOrdinaryPendingRecordV1(
            record, catalog_, actions_
        );
        QVERIFY(base);
        record.beforeActivation->entrypoint.append(QString(
            maximumLegacyOrdinaryPendingRecordV1Bytes - base->size(),
            QLatin1Char('x')
        ));
        const auto exactMaximum = serializeLegacyOrdinaryPendingRecordV1(
            record, catalog_, actions_
        );
        QVERIFY(exactMaximum);
        QCOMPARE(
            exactMaximum->size(),
            maximumLegacyOrdinaryPendingRecordV1Bytes
        );
        QVERIFY(decode(*exactMaximum));
        record.beforeActivation->entrypoint.append(QLatin1Char('x'));
        QVERIFY(!serializeLegacyOrdinaryPendingRecordV1(
            record, catalog_, actions_
        ));
    }
};

QTEST_MAIN(CompositorLegacyTransactionRecordsTest)

#include "compositor_legacy_transaction_records_test.moc"
