#include "compositord/dormant_generation_v1_verifier.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <array>
#include <type_traits>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";
constexpr auto nonceB = "fedcba9876543210fedcba9876543210";
constexpr auto oldCatalogA =
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0";
constexpr auto oldCatalogB =
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388";
constexpr auto oldAction =
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2";

struct Artifact final {
    QByteArray desiredBytes;
    QByteArray manifestBytes;
    QMap<QString, QByteArray> files;
    DormantGenerationV1Expectation expected;
};

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
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

[[nodiscard]] QByteArray canonicalObject(const QJsonObject &object)
{
    auto bytes = JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QJsonObject strictObject(const QByteArrayView bytes)
{
    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumDesiredStateBytes, 64
    );
    return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

void rebindGeneration(QJsonObject &manifest)
{
    manifest.remove(QStringLiteral("generation"));
    const auto generation = sha256(JsonSupport::canonicalJson(manifest));
    manifest.insert(QStringLiteral("generation"), generation);
}

[[nodiscard]] QString describeErrors(const ValidationErrors &errors)
{
    QStringList result;
    for (const auto &error : errors) {
        result.append(error.path + QLatin1Char(':') + error.code);
    }
    return result.join(QStringLiteral(", "));
}

[[nodiscard]] bool hasCode(
    const ValidationErrors &errors,
    const QString &code
)
{
    for (const auto &error : errors) {
        if (error.code == code) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] QDateTime fixedTime()
{
    return QDateTime::fromString(
        QStringLiteral("2026-08-09T12:34:56.789Z"), Qt::ISODateWithMs
    );
}

} // namespace

class CompositorDormantGenerationV1VerifierTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalog_;
    ActionCatalog actions_;
    QJsonObject defaults_;

    [[nodiscard]] QByteArray desiredBytes(
        const QString &catalogDigest =
            QString::fromLatin1(reviewedCatalogDigest),
        const QString &actionDigest =
            QString::fromLatin1(reviewedActionCatalogDigest),
        const bool protectedRule = false
    ) const
    {
        auto root = defaults_;
        root.insert(QStringLiteral("revision"), QStringLiteral("17"));
        root.insert(
            QStringLiteral("targetHyprland"), QStringLiteral("0.56.1")
        );
        root.insert(QStringLiteral("catalogDigest"), catalogDigest);
        root.insert(QStringLiteral("actionCatalogDigest"), actionDigest);
        if (!protectedRule) {
            root.insert(QStringLiteral("workspaceRules"), QJsonArray{});
        }
        return canonicalObject(root);
    }

    [[nodiscard]] DesiredState normalizedState(
        const QByteArrayView source
    ) const
    {
        auto root = strictObject(source);
        root.insert(
            QStringLiteral("catalogDigest"),
            QLatin1String(reviewedCatalogDigest)
        );
        root.insert(
            QStringLiteral("actionCatalogDigest"),
            QLatin1String(reviewedActionCatalogDigest)
        );
        const auto parsed = parseDesiredState(
            canonicalObject(root), catalog_, actions_
        );
        return parsed ? *parsed.value : DesiredState{};
    }

    [[nodiscard]] Artifact artifact(
        const QString &catalogDigest =
            QString::fromLatin1(reviewedCatalogDigest),
        const QString &actionDigest =
            QString::fromLatin1(reviewedActionCatalogDigest),
        const QString &nonce = QString::fromLatin1(nonceA),
        const bool protectedRule = false
    ) const
    {
        Artifact result;
        result.desiredBytes = desiredBytes(
            catalogDigest, actionDigest, protectedRule
        );
        const auto state = normalizedState(result.desiredBytes);
        result.expected.activationNonce = nonce;
        result.expected.generationRoot =
            QStringLiteral("/tmp/hyprshelld-v1-generation-verifier/")
            + nonce;
        result.expected.userCustomPath = QStringLiteral(
            "/tmp/hyprshelld-v1-generation-verifier/user-custom.lua"
        );
        result.expected.revision = state.revision;
        const auto rendered = renderGeneration(
            state,
            catalog_,
            actions_,
            result.expected.generationRoot,
            result.expected.userCustomPath,
            nonce,
            fixedTime()
        );
        if (!rendered) {
            return result;
        }
        for (auto iterator = rendered.value->files.constBegin();
             iterator != rendered.value->files.constEnd(); ++iterator) {
            result.files.insert(iterator.key(), iterator->contents);
        }

        auto manifest = rendered.value->manifest;
        result.expected.snapshotDigest = sha256(result.desiredBytes);
        manifest.insert(
            QStringLiteral("snapshotDigest"), result.expected.snapshotDigest
        );
        manifest.insert(QStringLiteral("catalogDigest"), catalogDigest);
        manifest.insert(
            QStringLiteral("actionCatalogDigest"), actionDigest
        );
        rebindGeneration(manifest);
        result.expected.generation =
            manifest.value(QStringLiteral("generation")).toString();
        result.manifestBytes = canonicalObject(manifest);
        return result;
    }

    [[nodiscard]] ValidationResult<VerifiedDormantGenerationV1> verify(
        const Artifact &candidate
    ) const
    {
        return verifyDormantGenerationV1ForMigration(
            candidate.desiredBytes,
            candidate.manifestBytes,
            candidate.files,
            candidate.expected,
            catalog_,
            actions_
        );
    }

    static void replaceManifest(
        Artifact &candidate,
        QJsonObject manifest,
        const bool rebind = true
    )
    {
        if (rebind) {
            rebindGeneration(manifest);
            candidate.expected.generation =
                manifest.value(QStringLiteral("generation")).toString();
        }
        candidate.manifestBytes = canonicalObject(manifest);
    }

private slots:
    void initTestCase()
    {
        const auto catalog = parseCatalog(readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_CATALOG_FILE
        )));
        const auto actions = parseActionCatalog(
            readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE
            )),
            readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE
            ))
        );
        QVERIFY2(catalog, qPrintable(describeErrors(catalog.errors)));
        QVERIFY2(actions, qPrintable(describeErrors(actions.errors)));
        catalog_ = *catalog.value;
        actions_ = *actions.value;
        defaults_ = readObject(QStringLiteral(
            HYPRSHELLD_HYPRLAND_DEFAULTS_FILE
        ));
        QVERIFY(!defaults_.isEmpty());
    }

    void allSixLineagesNormalizeRenderAndProjectExactly()
    {
        const std::array catalogs{
            QString::fromLatin1(oldCatalogA),
            QString::fromLatin1(oldCatalogB),
            QString::fromLatin1(reviewedCatalogDigest),
        };
        const std::array actions{
            QString::fromLatin1(oldAction),
            QString::fromLatin1(reviewedActionCatalogDigest),
        };
        for (const auto &catalogDigest : catalogs) {
            for (const auto &actionDigest : actions) {
                const auto candidate = artifact(
                    catalogDigest, actionDigest
                );
                QVERIFY(!candidate.manifestBytes.isEmpty());
                const auto checked = verify(candidate);
                QVERIFY2(checked, qPrintable(describeErrors(checked.errors)));
                QCOMPARE(checked.value->snapshotDigest,
                         sha256(candidate.desiredBytes));
                QCOMPARE(
                    checked.value->manifest.value(
                        QStringLiteral("catalogDigest")
                    ).toString(),
                    catalogDigest
                );
                QCOMPARE(
                    checked.value->manifest.value(
                        QStringLiteral("actionCatalogDigest")
                    ).toString(),
                    actionDigest
                );
                QCOMPARE(checked.value->manifestBytes,
                         candidate.manifestBytes);
            }
        }
    }

    void desiredAndGenerationLfDomainsAreFixed()
    {
        const auto candidate = artifact();
        const auto manifest = strictObject(candidate.manifestBytes);
        QCOMPARE(
            manifest.value(QStringLiteral("snapshotDigest")).toString(),
            sha256(candidate.desiredBytes)
        );
        QVERIFY(candidate.desiredBytes.endsWith('\n'));
        QVERIFY(
            sha256(candidate.desiredBytes)
            != sha256(QByteArrayView(candidate.desiredBytes).first(
                candidate.desiredBytes.size() - 1
            ))
        );

        auto generationInput = manifest;
        generationInput.remove(QStringLiteral("generation"));
        const auto withoutLf = JsonSupport::canonicalJson(generationInput);
        QCOMPARE(
            manifest.value(QStringLiteral("generation")).toString(),
            sha256(withoutLf)
        );
        auto withLf = withoutLf;
        withLf.append('\n');
        QVERIFY(sha256(withLf) != sha256(withoutLf));
        QVERIFY2(verify(candidate),
                 qPrintable(describeErrors(verify(candidate).errors)));
    }

    void allZeroAndMaximumLengthLegacyNoncesAreAccepted()
    {
        const std::array nonces{
            QString(32, QLatin1Char('0')),
            QString(128, QLatin1Char('a')),
        };
        for (const auto &nonce : nonces) {
            const auto candidate = artifact(
                QString::fromLatin1(reviewedCatalogDigest),
                QString::fromLatin1(reviewedActionCatalogDigest),
                nonce
            );
            const auto checked = verify(candidate);
            QVERIFY2(checked, qPrintable(describeErrors(checked.errors)));
            QCOMPARE(checked.value->activationNonce, nonce);
        }
    }

    void legacyNonceAlphabetAndLengthBoundsAreExact()
    {
        const auto baseline = artifact();
        const std::array invalidNonces{
            QString(31, QLatin1Char('a')),
            QString(129, QLatin1Char('a')),
            QString(32, QLatin1Char('A')),
            QString(32, QLatin1Char('g')),
        };
        for (const auto &nonce : invalidNonces) {
            auto candidate = baseline;
            auto manifest = strictObject(candidate.manifestBytes);
            manifest.insert(QStringLiteral("activationNonce"), nonce);
            replaceManifest(candidate, manifest);
            candidate.expected.activationNonce = nonce;
            candidate.expected.generationRoot =
                QStringLiteral("/tmp/hyprshelld-v1-generation-verifier/")
                + nonce;
            QVERIFY2(!verify(candidate), qPrintable(nonce));
        }
    }

    void everyManifestIdentityAndContractFieldIsExact()
    {
        const auto baseline = artifact();
        struct Mutation final {
            QString field;
            QJsonValue value;
            bool rebind = true;
        };
        const std::array mutations{
            Mutation{QStringLiteral("formatVersion"), 2},
            Mutation{QStringLiteral("contractVersion"), 2},
            Mutation{QStringLiteral("rendererVersion"), 2},
            Mutation{QStringLiteral("generation"), QString(64, '0'), false},
            Mutation{QStringLiteral("snapshotDigest"), QString(64, '0')},
            Mutation{QStringLiteral("catalogDigest"),
                     QString::fromLatin1(oldCatalogA)},
            Mutation{QStringLiteral("actionCatalogDigest"),
                     QString::fromLatin1(oldAction)},
            Mutation{QStringLiteral("revision"), QStringLiteral("18")},
            Mutation{QStringLiteral("targetHyprland"),
                     QStringLiteral("0.56.0")},
            Mutation{QStringLiteral("rendererVersion"), 1.5},
            Mutation{QStringLiteral("activationNonce"),
                     QString::fromLatin1(nonceB)},
            Mutation{QStringLiteral("entrypoint"),
                     QStringLiteral("modules/00-session.lua")},
        };
        for (const auto &mutation : mutations) {
            auto candidate = baseline;
            auto manifest = strictObject(candidate.manifestBytes);
            manifest.insert(mutation.field, mutation.value);
            replaceManifest(candidate, manifest, mutation.rebind);
            QVERIFY2(!verify(candidate), qPrintable(mutation.field));
        }

        auto candidate = baseline;
        auto manifest = strictObject(candidate.manifestBytes);
        auto compatibility = manifest.value(
            QStringLiteral("compatibleHyprland")
        ).toObject();
        compatibility.insert(QStringLiteral("minimumPatch"), 1);
        manifest.insert(
            QStringLiteral("compatibleHyprland"), compatibility
        );
        replaceManifest(candidate, manifest);
        QVERIFY(!verify(candidate));
    }

    void selfConsistentArbitraryLuaIsRejectedByFreshRerender()
    {
        auto candidate = artifact();
        const auto path = QStringLiteral("modules/90-advanced.lua");
        candidate.files[path].append("-- attacker-controlled but self-consistent\n");
        auto manifest = strictObject(candidate.manifestBytes);
        auto files = manifest.value(QStringLiteral("files")).toObject();
        files.insert(
            path,
            QJsonObject{
                {QStringLiteral("sha256"), sha256(candidate.files[path])},
                {QStringLiteral("size"), candidate.files[path].size()},
            }
        );
        manifest.insert(QStringLiteral("files"), files);
        replaceManifest(candidate, manifest);
        const auto checked = verify(candidate);
        QVERIFY(!checked);
        QVERIFY(hasCode(
            checked.errors,
            QStringLiteral("generation-v1.rerendered-file-mismatch")
        ));
    }

    void documentationFixtureIsNotAQualifiedGeneration()
    {
        auto candidate = artifact();
        candidate.manifestBytes = readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_GENERATION_MANIFEST_FIXTURE_FILE
        ));
        QVERIFY(!candidate.manifestBytes.isEmpty());
        QVERIFY(!verify(candidate));
    }

    void fileSetBytesAndBoundsAreExact()
    {
        const auto baseline = artifact();

        auto missing = baseline;
        missing.files.remove(QStringLiteral("modules/00-session.lua"));
        QVERIFY(hasCode(
            verify(missing).errors,
            QStringLiteral("generation-v1.file-set-mismatch")
        ));

        auto extra = baseline;
        extra.files.insert(QStringLiteral("extra.lua"), QByteArray{});
        QVERIFY(hasCode(
            verify(extra).errors,
            QStringLiteral("generation-v1.file-set-mismatch")
        ));

        auto renamed = baseline;
        const auto renamedBytes = renamed.files.take(
            QStringLiteral("modules/00-session.lua")
        );
        renamed.files.insert(QStringLiteral("modules/00-renamed.lua"),
                             renamedBytes);
        QVERIFY(hasCode(
            verify(renamed).errors,
            QStringLiteral("generation-v1.file-set-mismatch")
        ));

        auto mutated = baseline;
        mutated.files[QStringLiteral("hyprland.lua")].append('x');
        QVERIFY(!verify(mutated));

        auto oversized = baseline;
        oversized.files[QStringLiteral("modules/90-advanced.lua")]
            = QByteArray(16 * 1024 * 1024 + 1, 'x');
        QVERIFY(hasCode(
            verify(oversized).errors,
            QStringLiteral("generation-v1.invalid-file-size")
        ));

        auto badMetadata = baseline;
        auto manifest = strictObject(badMetadata.manifestBytes);
        auto files = manifest.value(QStringLiteral("files")).toObject();
        auto metadata = files.value(
            QStringLiteral("modules/00-session.lua")
        ).toObject();
        metadata.insert(QStringLiteral("extra"), true);
        files.insert(QStringLiteral("modules/00-session.lua"), metadata);
        manifest.insert(QStringLiteral("files"), files);
        replaceManifest(badMetadata, manifest);
        QVERIFY(!verify(badMetadata));
    }

    void desiredStrictJsonCanonicalLfAndBoundsAreEnforced()
    {
        const auto baseline = artifact();
        const std::array variants{
            baseline.desiredBytes.first(baseline.desiredBytes.size() - 1),
            baseline.desiredBytes + QByteArray("\n"),
            QByteArray("\xEF\xBB\xBF") + baseline.desiredBytes,
            QByteArray(" ") + baseline.desiredBytes,
        };
        for (const auto &bytes : variants) {
            auto candidate = baseline;
            candidate.desiredBytes = bytes;
            QVERIFY(!verify(candidate));
        }

        auto duplicate = baseline;
        duplicate.desiredBytes.insert(1, "\"revision\":\"17\",");
        QVERIFY(!verify(duplicate));

        auto unknown = baseline;
        auto root = strictObject(unknown.desiredBytes);
        root.insert(QStringLiteral("unknown"), true);
        unknown.desiredBytes = canonicalObject(root);
        QVERIFY(!verify(unknown));

        auto tooDeep = baseline;
        QByteArray deep = tooDeep.desiredBytes;
        deep.chop(2);
        deep += ",\"unknown\":" + QByteArray(66, '[') + "0"
            + QByteArray(66, ']') + "}\n";
        tooDeep.desiredBytes = deep;
        QVERIFY(!verify(tooDeep));

        auto oversized = baseline;
        oversized.desiredBytes = QByteArray(
            maximumDesiredStateBytes + 1, 'x'
        );
        QVERIFY(hasCode(
            verify(oversized).errors,
            QStringLiteral("generation-v1.invalid-desired-size")
        ));
    }

    void manifestStrictJsonCanonicalLfAndBoundsAreEnforced()
    {
        const auto baseline = artifact();
        const std::array variants{
            baseline.manifestBytes.first(baseline.manifestBytes.size() - 1),
            baseline.manifestBytes + QByteArray("\n"),
            QByteArray("\xEF\xBB\xBF") + baseline.manifestBytes,
            QByteArray(" ") + baseline.manifestBytes,
        };
        for (const auto &bytes : variants) {
            auto candidate = baseline;
            candidate.manifestBytes = bytes;
            QVERIFY(!verify(candidate));
        }

        auto duplicate = baseline;
        duplicate.manifestBytes.insert(
            1, "\"generation\":\"" + QByteArray(64, '0') + "\","
        );
        QVERIFY(!verify(duplicate));

        auto unknown = baseline;
        auto manifest = strictObject(unknown.manifestBytes);
        manifest.insert(QStringLiteral("unknown"), true);
        replaceManifest(unknown, manifest);
        QVERIFY(!verify(unknown));

        auto tooDeep = baseline;
        QByteArray deep = tooDeep.manifestBytes;
        deep.chop(2);
        deep += ",\"unknown\":" + QByteArray(34, '[') + "0"
            + QByteArray(34, ']') + "}\n";
        tooDeep.manifestBytes = deep;
        QVERIFY(!verify(tooDeep));

        auto oversized = baseline;
        oversized.manifestBytes = QByteArray(4 * 1024 * 1024 + 1, 'x');
        QVERIFY(hasCode(
            verify(oversized).errors,
            QStringLiteral("generation-v1.invalid-manifest-size")
        ));
    }

    void creationTimeMustUseExactRendererMillisecondsAndUtc()
    {
        const auto baseline = artifact();
        for (const auto &time : {
                 QStringLiteral("2026-08-09T12:34:56Z"),
                 QStringLiteral("2026-08-09T12:34:56.78Z"),
                 QStringLiteral("2026-08-09T12:34:56.789+00:00"),
                 QStringLiteral("2026-08-09T12:34:56.789z"),
                 QStringLiteral("2026-02-30T12:34:56.789Z"),
             }) {
            auto candidate = baseline;
            auto manifest = strictObject(candidate.manifestBytes);
            manifest.insert(QStringLiteral("createdAt"), time);
            replaceManifest(candidate, manifest);
            QVERIFY2(!verify(candidate), qPrintable(time));
        }
    }

    void explicitPathsAndExpectationsCannotBeSubstituted()
    {
        const auto baseline = artifact();

        auto candidate = baseline;
        candidate.expected.generationRoot = QStringLiteral("relative/path");
        QVERIFY(!verify(candidate));

        candidate = baseline;
        candidate.expected.generationRoot = QStringLiteral(
            "/tmp/hyprshelld-v1-generation-verifier/not-the-nonce"
        );
        QVERIFY(!verify(candidate));

        candidate = baseline;
        candidate.expected.userCustomPath = candidate.expected.generationRoot;
        QVERIFY(!verify(candidate));

        candidate = baseline;
        candidate.expected.userCustomPath =
            candidate.expected.generationRoot + QStringLiteral("/custom.lua");
        QVERIFY(!verify(candidate));

        candidate = baseline;
        candidate.expected.userCustomPath = QStringLiteral("relative.lua");
        QVERIFY(!verify(candidate));

        candidate = baseline;
        ++candidate.expected.revision;
        QVERIFY(!verify(candidate));

        candidate = baseline;
        candidate.expected.snapshotDigest = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(candidate));

        candidate = baseline;
        candidate.expected.generation = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(candidate));

        candidate = baseline;
        candidate.expected.activationNonce = QString::fromLatin1(nonceB);
        QVERIFY(!verify(candidate));
    }

    void unknownLineagesAndPreSharedProtectedRuleAreRejected()
    {
        auto unknownCatalog = artifact();
        auto root = strictObject(unknownCatalog.desiredBytes);
        root.insert(QStringLiteral("catalogDigest"), QString(64, 'a'));
        unknownCatalog.desiredBytes = canonicalObject(root);
        QVERIFY(hasCode(
            verify(unknownCatalog).errors,
            QStringLiteral("generation-v1.unknown-lineage")
        ));

        auto unknownAction = artifact();
        root = strictObject(unknownAction.desiredBytes);
        root.insert(QStringLiteral("actionCatalogDigest"), QString(64, 'b'));
        unknownAction.desiredBytes = canonicalObject(root);
        QVERIFY(hasCode(
            verify(unknownAction).errors,
            QStringLiteral("generation-v1.unknown-lineage")
        ));

        const auto protectedPreShared = artifact(
            QString::fromLatin1(reviewedCatalogDigest),
            QString::fromLatin1(oldAction),
            QString::fromLatin1(nonceA),
            true
        );
        QVERIFY(!protectedPreShared.manifestBytes.isEmpty());
        const auto checked = verify(protectedPreShared);
        QVERIFY(!checked);
        QVERIFY(hasCode(
            checked.errors,
            QStringLiteral("generation-v1.pre-shared-protected-rule")
        ));
    }

    void resultContainsOnlyFreshRerenderedQualificationBytes()
    {
        static_assert(!std::is_same_v<
                      VerifiedDormantGenerationV1,
                      RenderedGeneration>);

        auto candidate = artifact();
        const auto originalManifest = candidate.manifestBytes;
        const auto originalEntrypoint = candidate.files.value(
            QStringLiteral("hyprland.lua")
        );
        const auto checked = verify(candidate);
        QVERIFY2(checked, qPrintable(describeErrors(checked.errors)));

        candidate.manifestBytes.fill('x');
        candidate.files[QStringLiteral("hyprland.lua")].fill('x');
        QCOMPARE(checked.value->manifestBytes, originalManifest);
        QCOMPARE(
            checked.value->files.value(QStringLiteral("hyprland.lua")).contents,
            originalEntrypoint
        );
        QCOMPARE(checked.value->generationRoot,
                 QStringLiteral("/tmp/hyprshelld-v1-generation-verifier/")
                     + QString::fromLatin1(nonceA));
        QCOMPARE(checked.value->userCustomPath,
                 QStringLiteral(
                     "/tmp/hyprshelld-v1-generation-verifier/user-custom.lua"
                 ));
        QCOMPARE(checked.value->files.size(), 17);
    }
};

QTEST_GUILESS_MAIN(CompositorDormantGenerationV1VerifierTest)

#include "compositor_dormant_generation_v1_verifier_test.moc"
