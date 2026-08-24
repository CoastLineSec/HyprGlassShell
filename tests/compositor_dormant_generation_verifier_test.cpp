#include "compositord/dormant_generation_verifier.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <functional>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr auto authorityA = "11111111111111111111111111111111";
constexpr auto authorityB = "22222222222222222222222222222222";
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

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

void rebindGeneration(QJsonObject &manifest)
{
    auto generationInput = manifest;
    generationInput.remove(QStringLiteral("generation"));
    manifest.insert(
        QStringLiteral("generation"),
        sha256(JsonSupport::canonicalJson(generationInput))
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

class CompositorDormantGenerationVerifierTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalog;
    ActionCatalog actions;
    DesiredStateV2 state;
    DormantRenderedGenerationV2 rendered;
    QMap<QString, QByteArray> files;
    DormantGenerationV2Expectation expected;

    [[nodiscard]] ValidationResult<VerifiedDormantGenerationV2> verify(
        const QByteArrayView manifestBytes,
        const QMap<QString, QByteArray> &candidateFiles,
        const DormantGenerationV2Expectation &candidateExpected,
        const DesiredStateV2 &candidateState,
        const Catalog &candidateCatalog,
        const ActionCatalog &candidateActions
    ) const
    {
        return verifyDormantGenerationV2(
            manifestBytes,
            candidateFiles,
            candidateExpected,
            candidateState,
            candidateCatalog,
            candidateActions
        );
    }

    [[nodiscard]] ValidationResult<VerifiedDormantGenerationV2> verify(
        const QByteArrayView manifestBytes,
        const QMap<QString, QByteArray> &candidateFiles,
        const DormantGenerationV2Expectation &candidateExpected
    ) const
    {
        return verify(
            manifestBytes,
            candidateFiles,
            candidateExpected,
            state,
            catalog,
            actions
        );
    }

private slots:
    void initTestCase()
    {
        const auto parsedCatalog = parseDormantCatalogV2(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE))
        );
        QVERIFY2(parsedCatalog, qPrintable(describeErrors(parsedCatalog.errors)));
        catalog = *parsedCatalog.value;

        const auto parsedActions = parseDormantActionCatalogV2(
            readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE
            )),
            readBytes(QStringLiteral(
                HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE
            ))
        );
        QVERIFY2(parsedActions, qPrintable(describeErrors(parsedActions.errors)));
        actions = *parsedActions.value;

        auto desiredObject = readObject(QStringLiteral(
            HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE
        ));
        QVERIFY(!desiredObject.isEmpty());
        desiredObject.insert(
            QStringLiteral("authorityId"), QString::fromLatin1(authorityA)
        );
        const auto parsedState = parseDormantDesiredStateV2(
            canonicalObject(desiredObject), catalog, actions
        );
        QVERIFY2(parsedState, qPrintable(describeErrors(parsedState.errors)));
        state = *parsedState.value;

        expected.activationNonce = QString::fromLatin1(nonceA);
        expected.generationRoot =
            QStringLiteral("/tmp/hyprshelld-generation-verifier/")
            + expected.activationNonce;
        expected.userCustomPath = QStringLiteral(
            "/tmp/hyprshelld-generation-verifier/user-custom.lua"
        );
        const auto reference = renderDormantGenerationV2(
            state,
            catalog,
            actions,
            expected.generationRoot,
            expected.userCustomPath,
            expected.activationNonce,
            fixedTime()
        );
        QVERIFY2(reference, qPrintable(describeErrors(reference.errors)));
        rendered = *reference.value;
        for (auto iterator = rendered.files.constBegin();
             iterator != rendered.files.constEnd(); ++iterator) {
            files.insert(iterator.key(), iterator->contents);
        }
        expected.authorityId = rendered.authorityId;
        expected.revision = state.semanticState.revision;
        expected.snapshotDigest = rendered.snapshotDigest;
        expected.generation = rendered.generation;

        QCOMPARE(files.size(), 17);
        QCOMPARE(rendered.manifestBytes.back(), '\n');
        QVERIFY(fixedTime().isValid());
    }

    void acceptsOnlyTheFreshRerenderedProduct()
    {
        const auto verified = verify(
            rendered.manifestBytes, files, expected
        );
        QVERIFY2(verified, qPrintable(describeErrors(verified.errors)));
        QCOMPARE(verified.value->generationRoot, expected.generationRoot);
        QCOMPARE(verified.value->userCustomPath, expected.userCustomPath);
        QCOMPARE(
            verified.value->rendered.manifestBytes,
            rendered.manifestBytes
        );
        QVERIFY(verified.value->rendered.files == rendered.files);
        QCOMPARE(
            verified.value->rendered.activationRequirement,
            rendered.activationRequirement
        );
        QCOMPARE(verified.value->rendered.files.size(), 17);

        const auto desiredBytes = serializeDormantDesiredStateV2(state);
        QVERIFY2(desiredBytes, qPrintable(describeErrors(desiredBytes.errors)));
        QVERIFY(desiredBytes.value->endsWith('\n'));
        auto desiredWithoutLf = *desiredBytes.value;
        desiredWithoutLf.chop(1);
        QCOMPARE(sha256(desiredWithoutLf), rendered.snapshotDigest);
        QVERIFY(sha256(*desiredBytes.value) != rendered.snapshotDigest);

        auto generationInput = rendered.manifest;
        generationInput.remove(QStringLiteral("generation"));
        const auto generationBytes = JsonSupport::canonicalJson(
            generationInput
        );
        QCOMPARE(sha256(generationBytes), rendered.generation);
        auto generationWithLf = generationBytes;
        generationWithLf.append('\n');
        QVERIFY(sha256(generationWithLf) != rendered.generation);
    }

    void rejectsEveryManifestBindingMutation()
    {
        struct Mutation final {
            QString name;
            std::function<void(
                QJsonObject &,
                DormantGenerationV2Expectation &
            )> apply;
            bool rebind = true;
        };

        const QVector<Mutation> mutations{
            {
                QStringLiteral("formatVersion"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(QStringLiteral("formatVersion"), 1);
                },
            },
            {
                QStringLiteral("contractVersion"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(QStringLiteral("contractVersion"), 1);
                },
            },
            {
                QStringLiteral("authorityId"),
                [](QJsonObject &manifest, auto &candidateExpected) {
                    manifest.insert(
                        QStringLiteral("authorityId"),
                        QString::fromLatin1(authorityB)
                    );
                    candidateExpected.authorityId =
                        QString::fromLatin1(authorityB);
                },
            },
            {
                QStringLiteral("generation"),
                [](QJsonObject &manifest, auto &candidateExpected) {
                    const QString digest(64, QLatin1Char('0'));
                    manifest.insert(QStringLiteral("generation"), digest);
                    candidateExpected.generation = digest;
                },
                false,
            },
            {
                QStringLiteral("snapshotDigest"),
                [](QJsonObject &manifest, auto &candidateExpected) {
                    const QString digest(64, QLatin1Char('0'));
                    manifest.insert(QStringLiteral("snapshotDigest"), digest);
                    candidateExpected.snapshotDigest = digest;
                },
            },
            {
                QStringLiteral("sourceManifestDigest"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(
                        QStringLiteral("sourceManifestDigest"),
                        QString(64, QLatin1Char('0'))
                    );
                },
            },
            {
                QStringLiteral("catalogDigest"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(
                        QStringLiteral("catalogDigest"),
                        QString(64, QLatin1Char('0'))
                    );
                },
            },
            {
                QStringLiteral("actionCatalogDigest"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(
                        QStringLiteral("actionCatalogDigest"),
                        QString(64, QLatin1Char('0'))
                    );
                },
            },
            {
                QStringLiteral("revision"),
                [](QJsonObject &manifest, auto &candidateExpected) {
                    manifest.insert(QStringLiteral("revision"), QStringLiteral("1"));
                    candidateExpected.revision = 1;
                },
            },
            {
                QStringLiteral("targetHyprland"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(
                        QStringLiteral("targetHyprland"),
                        QStringLiteral("0.56.3")
                    );
                },
            },
            {
                QStringLiteral("compatibleHyprland"),
                [](QJsonObject &manifest, auto &) {
                    auto compatible = manifest
                                          .value(QStringLiteral(
                                              "compatibleHyprland"
                                          ))
                                          .toObject();
                    compatible.insert(QStringLiteral("maximumPatch"), 3);
                    manifest.insert(
                        QStringLiteral("compatibleHyprland"), compatible
                    );
                },
            },
            {
                QStringLiteral("rendererVersion"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(QStringLiteral("rendererVersion"), 1);
                },
            },
            {
                QStringLiteral("activationNonce"),
                [](QJsonObject &manifest, auto &candidateExpected) {
                    manifest.insert(
                        QStringLiteral("activationNonce"),
                        QString::fromLatin1(nonceB)
                    );
                    candidateExpected.activationNonce =
                        QString::fromLatin1(nonceB);
                    candidateExpected.generationRoot =
                        QStringLiteral("/tmp/hyprshelld-generation-verifier/")
                        + candidateExpected.activationNonce;
                },
            },
            {
                QStringLiteral("createdAt"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(
                        QStringLiteral("createdAt"),
                        QStringLiteral("2026-08-09T12:34:56Z")
                    );
                },
            },
            {
                QStringLiteral("entrypoint"),
                [](QJsonObject &manifest, auto &) {
                    manifest.insert(
                        QStringLiteral("entrypoint"),
                        QStringLiteral("modules/00-session.lua")
                    );
                },
            },
            {
                QStringLiteral("files"),
                [](QJsonObject &manifest, auto &) {
                    auto manifestFiles =
                        manifest.value(QStringLiteral("files")).toObject();
                    const auto path = QStringLiteral("modules/00-session.lua");
                    auto metadata = manifestFiles.value(path).toObject();
                    metadata.insert(
                        QStringLiteral("size"),
                        metadata.value(QStringLiteral("size")).toInteger() + 1
                    );
                    manifestFiles.insert(path, metadata);
                    manifest.insert(QStringLiteral("files"), manifestFiles);
                },
            },
        };

        for (const auto &mutation : mutations) {
            auto manifest = rendered.manifest;
            auto candidateExpected = expected;
            mutation.apply(manifest, candidateExpected);
            if (mutation.rebind) {
                rebindGeneration(manifest);
                candidateExpected.generation = manifest
                                                   .value(QStringLiteral("generation"))
                                                   .toString();
            }
            const auto rejected = verify(
                canonicalObject(manifest), files, candidateExpected
            );
            QVERIFY2(
                !rejected,
                qPrintable(
                    mutation.name + QStringLiteral(": unexpectedly accepted")
                )
            );
        }

        auto noLf = rendered.manifestBytes;
        noLf.chop(1);
        QVERIFY(!verify(noLf, files, expected));
        auto doubleLf = rendered.manifestBytes;
        doubleLf.append('\n');
        QVERIFY(!verify(doubleLf, files, expected));
    }

    void rejectsStrictByteSeamsAndBounds()
    {
        auto bom = QByteArray::fromHex(QByteArrayLiteral("efbbbf"));
        bom.append(rendered.manifestBytes);
        QVERIFY(!verify(bom, files, expected));

        auto duplicate = rendered.manifestBytes;
        const auto authorityField =
            QByteArrayLiteral("\"authorityId\":\"")
            + rendered.authorityId.toLatin1() + QByteArrayLiteral("\"");
        QCOMPARE(duplicate.count(authorityField), 1);
        duplicate.replace(
            authorityField,
            authorityField + QByteArrayLiteral(",") + authorityField
        );
        QVERIFY(!verify(duplicate, files, expected));

        auto unknown = rendered.manifest;
        unknown.insert(QStringLiteral("unexpected"), true);
        rebindGeneration(unknown);
        auto unknownExpected = expected;
        unknownExpected.generation =
            unknown.value(QStringLiteral("generation")).toString();
        QVERIFY(!verify(canonicalObject(unknown), files, unknownExpected));

        const QByteArray oversizedManifest(4 * 1024 * 1024 + 1, 'x');
        const auto manifestRejected = verify(
            oversizedManifest, files, expected
        );
        QVERIFY(!manifestRejected);
        QVERIFY(hasCode(
            manifestRejected.errors,
            QStringLiteral("generation-v2.invalid-manifest-size")
        ));

        auto oversizedFiles = files;
        oversizedFiles[QStringLiteral("modules/00-session.lua")] =
            QByteArray(16 * 1024 * 1024 + 1, 'x');
        const auto fileRejected = verify(
            rendered.manifestBytes, oversizedFiles, expected
        );
        QVERIFY(!fileRejected);
        QVERIFY(hasCode(
            fileRejected.errors,
            QStringLiteral("generation-v2.invalid-file-size")
        ));
    }

    void authorityIdentityChangesManifestButNotLuaBytes()
    {
        const auto stateBytes = serializeDormantDesiredStateV2(state);
        QVERIFY(stateBytes);
        const auto stateDocument = QJsonDocument::fromJson(*stateBytes.value);
        QVERIFY(stateDocument.isObject());
        auto stateObject = stateDocument.object();
        stateObject.insert(
            QStringLiteral("authorityId"), QString::fromLatin1(authorityB)
        );
        const auto parsedOther = parseDormantDesiredStateV2(
            canonicalObject(stateObject), catalog, actions
        );
        QVERIFY2(parsedOther, qPrintable(describeErrors(parsedOther.errors)));

        const auto other = renderDormantGenerationV2(
            *parsedOther.value,
            catalog,
            actions,
            expected.generationRoot,
            expected.userCustomPath,
            expected.activationNonce,
            fixedTime()
        );
        QVERIFY2(other, qPrintable(describeErrors(other.errors)));
        QVERIFY(other.value->files == rendered.files);
        QVERIFY(other.value->snapshotDigest != rendered.snapshotDigest);
        QVERIFY(other.value->generation != rendered.generation);

        QMap<QString, QByteArray> otherFiles;
        for (auto iterator = other.value->files.constBegin();
             iterator != other.value->files.constEnd(); ++iterator) {
            otherFiles.insert(iterator.key(), iterator->contents);
        }
        auto otherExpected = expected;
        otherExpected.authorityId = QString::fromLatin1(authorityB);
        otherExpected.snapshotDigest = other.value->snapshotDigest;
        otherExpected.generation = other.value->generation;
        const auto verified = verify(
            other.value->manifestBytes,
            otherFiles,
            otherExpected,
            *parsedOther.value,
            catalog,
            actions
        );
        QVERIFY2(verified, qPrintable(describeErrors(verified.errors)));

        QVERIFY(!verify(
            rendered.manifestBytes,
            files,
            expected,
            *parsedOther.value,
            catalog,
            actions
        ));
    }

    void rejectsSelfConsistentArbitraryLuaByRerendering()
    {
        auto arbitraryFiles = files;
        const auto path = QStringLiteral("modules/00-session.lua");
        arbitraryFiles[path].append(
            QByteArrayLiteral("-- attacker-controlled but self-consistent\n")
        );

        auto manifest = rendered.manifest;
        auto manifestFiles = manifest.value(QStringLiteral("files")).toObject();
        manifestFiles.insert(
            path,
            QJsonObject{
                {QStringLiteral("sha256"), sha256(arbitraryFiles.value(path))},
                {
                    QStringLiteral("size"),
                    static_cast<qint64>(arbitraryFiles.value(path).size()),
                },
            }
        );
        manifest.insert(QStringLiteral("files"), manifestFiles);
        rebindGeneration(manifest);
        auto candidateExpected = expected;
        candidateExpected.generation =
            manifest.value(QStringLiteral("generation")).toString();

        const auto rejected = verify(
            canonicalObject(manifest), arbitraryFiles, candidateExpected
        );
        QVERIFY(!rejected);
        QVERIFY(hasCode(
            rejected.errors,
            QStringLiteral("generation-v2.rerendered-manifest-mismatch")
        ));
        QVERIFY(hasCode(
            rejected.errors,
            QStringLiteral("generation-v2.rerendered-file-mismatch")
        ));
    }

    void rejectsMissingExtraRenamedAndMutatedFiles()
    {
        auto missing = files;
        missing.remove(QStringLiteral("modules/00-session.lua"));
        QVERIFY(!verify(rendered.manifestBytes, missing, expected));

        auto extra = files;
        extra.insert(QStringLiteral("modules/99-extra.lua"), QByteArray{});
        QVERIFY(!verify(rendered.manifestBytes, extra, expected));

        const auto oldPath = QStringLiteral("modules/00-session.lua");
        const auto newPath = QStringLiteral("modules/00-renamed.lua");
        auto renamed = files;
        const auto renamedBytes = renamed.take(oldPath);
        renamed.insert(newPath, renamedBytes);
        auto renamedManifest = rendered.manifest;
        auto manifestFiles =
            renamedManifest.value(QStringLiteral("files")).toObject();
        const auto renamedMetadata = manifestFiles.take(oldPath);
        manifestFiles.insert(newPath, renamedMetadata);
        renamedManifest.insert(QStringLiteral("files"), manifestFiles);
        rebindGeneration(renamedManifest);
        auto renamedExpected = expected;
        renamedExpected.generation = renamedManifest
                                         .value(QStringLiteral("generation"))
                                         .toString();
        QVERIFY(!verify(
            canonicalObject(renamedManifest), renamed, renamedExpected
        ));

        auto mutated = files;
        mutated[oldPath].append('x');
        const auto rejected = verify(
            rendered.manifestBytes, mutated, expected
        );
        QVERIFY(!rejected);
        QVERIFY(hasCode(
            rejected.errors,
            QStringLiteral("generation-v2.file-digest-mismatch")
        ));
    }

    void rejectsPathAndExplicitExpectationMismatches()
    {
        auto wrongRoot = expected;
        wrongRoot.generationRoot =
            QStringLiteral("/var/tmp/hyprshelld-generation-verifier/")
            + expected.activationNonce;
        QVERIFY(!verify(rendered.manifestBytes, files, wrongRoot));

        auto wrongCustom = expected;
        wrongCustom.userCustomPath = QStringLiteral(
            "/tmp/hyprshelld-generation-verifier/other-custom.lua"
        );
        QVERIFY(!verify(rendered.manifestBytes, files, wrongCustom));

        auto wrong = expected;
        wrong.authorityId = QString::fromLatin1(authorityB);
        QVERIFY(!verify(rendered.manifestBytes, files, wrong));
        wrong = expected;
        wrong.revision += 1;
        QVERIFY(!verify(rendered.manifestBytes, files, wrong));
        wrong = expected;
        wrong.snapshotDigest = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(rendered.manifestBytes, files, wrong));
        wrong = expected;
        wrong.generation = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(rendered.manifestBytes, files, wrong));
        wrong = expected;
        wrong.activationNonce = QString::fromLatin1(nonceB);
        QVERIFY(!verify(rendered.manifestBytes, files, wrong));
    }

    void rejectsDesiredCatalogActionAndSourceAuthorityMismatches()
    {
        auto changedState = state;
        ++changedState.semanticState.revision;
        QVERIFY(!verify(
            rendered.manifestBytes,
            files,
            expected,
            changedState,
            catalog,
            actions
        ));

        auto changedCatalog = catalog;
        changedCatalog.digest = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(
            rendered.manifestBytes,
            files,
            expected,
            state,
            changedCatalog,
            actions
        ));

        auto changedActions = actions;
        changedActions.digest = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(
            rendered.manifestBytes,
            files,
            expected,
            state,
            catalog,
            changedActions
        ));

        changedCatalog = catalog;
        changedCatalog.sourceManifestDigest = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(
            rendered.manifestBytes,
            files,
            expected,
            state,
            changedCatalog,
            actions
        ));

        changedActions = actions;
        changedActions.source.sha256 = QString(64, QLatin1Char('0'));
        QVERIFY(!verify(
            rendered.manifestBytes,
            files,
            expected,
            state,
            catalog,
            changedActions
        ));
    }

    void rejectsSnapshotDigestFromTheWrongLfDomain()
    {
        const auto desiredBytes = serializeDormantDesiredStateV2(state);
        QVERIFY(desiredBytes);
        const auto wrongDigest = sha256(*desiredBytes.value);
        QVERIFY(wrongDigest != rendered.snapshotDigest);

        auto manifest = rendered.manifest;
        manifest.insert(QStringLiteral("snapshotDigest"), wrongDigest);
        rebindGeneration(manifest);
        auto candidateExpected = expected;
        candidateExpected.snapshotDigest = wrongDigest;
        candidateExpected.generation =
            manifest.value(QStringLiteral("generation")).toString();
        const auto rejected = verify(
            canonicalObject(manifest), files, candidateExpected
        );
        QVERIFY(!rejected);
        QVERIFY(hasCode(
            rejected.errors,
            QStringLiteral("generation-v2.snapshot-mismatch")
        ));
    }
};

QTEST_MAIN(CompositorDormantGenerationVerifierTest)

#include "compositor_dormant_generation_verifier_test.moc"
