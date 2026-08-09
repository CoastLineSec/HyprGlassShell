#include "componentd/system_catalog.h"

#include "componentd_test_fixture.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>
#include <QtEndian>

#include <array>

namespace {

const QString sourceComponentDirectory = QStringLiteral(
    HYPRSHELLD_SOURCE_COMPONENT_DIR
);

void addFramedValue(QCryptographicHash &hash, const QByteArray &value)
{
    std::array<uchar, sizeof(quint64)> encodedLength{};
    qToBigEndian<quint64>(
        static_cast<quint64>(value.size()),
        encodedLength.data()
    );
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(encodedLength.data()),
        encodedLength.size()
    ));
    hash.addData(value);
}

QString singleEntryCatalogDigest(
    const QString &componentId,
    const QString &packageDigest
)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    addFramedValue(hash, componentId.toUtf8());
    addFramedValue(hash, packageDigest.toLatin1());
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

class ComponentdCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void validCatalogIsStableAndDerived()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                root.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );

        const auto first = HyprShelld::Components::SystemCatalog::load(
            root.path()
        );
        QVERIFY2(first.ok(), qPrintable(first.error));
        const auto second = HyprShelld::Components::SystemCatalog::load(
            root.path()
        );
        QVERIFY2(second.ok(), qPrintable(second.error));

        const auto id = QString::fromLatin1(
            HyprShelld::Components::workspaceSwitcherId
        );
        QCOMPARE(first.catalog->componentIds(), QStringList{id});
        QCOMPARE(
            first.catalog->catalogDigest(),
            second.catalog->catalogDigest()
        );
        QVERIFY(
            QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                .match(first.catalog->catalogDigest())
                .hasMatch()
        );

        const auto *entry = first.catalog->find(id);
        QVERIFY(entry != nullptr);
        QVERIFY(
            entry->manifest.origin
            == HyprShelld::Components::ComponentOrigin::System
        );
        QCOMPARE(entry->manifest.id, id);
        QVERIFY(!entry->settingsSchema.isEmpty());
        QCOMPARE(entry->packageDigest.size(), 64);
        QVERIFY(
            QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                .match(entry->packageDigest)
                .hasMatch()
        );
        QCOMPARE(
            entry->packageDigest,
            second.catalog->find(id)->packageDigest
        );
        QCOMPARE(
            first.catalog->catalogDigest(),
            singleEntryCatalogDigest(id, entry->packageDigest)
        );
    }

    void schemaBytesParticipateInPackageAndCatalogDigests()
    {
        QTemporaryDir firstRoot;
        QTemporaryDir secondRoot;
        QVERIFY(firstRoot.isValid());
        QVERIFY(secondRoot.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                firstRoot.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                secondRoot.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );

        const auto secondSchema = HyprShelld::Tests::componentDirectory(
                                      secondRoot.path()
                                  )
            + QStringLiteral("/settings.schema.json");
        auto schema = HyprShelld::Tests::readFile(secondSchema, error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        schema.append('\n');
        QVERIFY2(
            HyprShelld::Tests::replaceFile(secondSchema, schema, error),
            qPrintable(error)
        );

        const auto first = HyprShelld::Components::SystemCatalog::load(
            firstRoot.path()
        );
        const auto second = HyprShelld::Components::SystemCatalog::load(
            secondRoot.path()
        );
        QVERIFY2(first.ok(), qPrintable(first.error));
        QVERIFY2(second.ok(), qPrintable(second.error));

        const auto id = QString::fromLatin1(
            HyprShelld::Components::workspaceSwitcherId
        );
        QVERIFY(
            first.catalog->find(id)->packageDigest
            != second.catalog->find(id)->packageDigest
        );
        QVERIFY(
            first.catalog->catalogDigest()
            != second.catalog->catalogDigest()
        );
    }

    void malformedManifestIsRejected()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                root.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );

        const auto path = HyprShelld::Tests::componentDirectory(root.path())
            + QStringLiteral("/manifest.json");
        auto manifest = HyprShelld::Tests::readFile(path, error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto manifestVersion = QByteArrayLiteral(
            "\"manifestVersion\": 1,"
        );
        QVERIFY(manifest.contains(manifestVersion));
        manifest.replace(
            manifestVersion,
            QByteArrayLiteral(
                "\"manifestVersion\": 1,\n  \"manifestVersion\": 1,"
            )
        );
        QVERIFY2(
            HyprShelld::Tests::replaceFile(path, manifest, error),
            qPrintable(error)
        );

        const auto loaded = HyprShelld::Components::SystemCatalog::load(
            root.path()
        );
        QVERIFY(!loaded.ok());
        QVERIFY(loaded.error.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));
    }

    void duplicateManifestIdIsRejected()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                root.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );

        const auto duplicate = root.path()
            + QStringLiteral(
                "/io.github.coastlinesec.hyprshelld.workspace-switcher-copy"
            );
        QVERIFY(QDir().mkpath(duplicate));
        QVERIFY2(
            HyprShelld::Tests::copyFile(
                sourceComponentDirectory + QStringLiteral("/manifest.json"),
                duplicate + QStringLiteral("/manifest.json"),
                error
            ),
            qPrintable(error)
        );
        QVERIFY2(
            HyprShelld::Tests::copyFile(
                sourceComponentDirectory
                    + QStringLiteral("/settings.schema.json"),
                duplicate + QStringLiteral("/settings.schema.json"),
                error
            ),
            qPrintable(error)
        );

        const auto loaded = HyprShelld::Components::SystemCatalog::load(
            root.path()
        );
        QVERIFY(!loaded.ok());
        QVERIFY(loaded.error.contains(QStringLiteral("Duplicate system component ID")));
    }

    void mismatchedDirectoryAndMalformedSchemaAreRejected()
    {
        QTemporaryDir wrongDirectoryRoot;
        QVERIFY(wrongDirectoryRoot.isValid());

        QString error;
        const auto wrongDirectory = wrongDirectoryRoot.path()
            + QStringLiteral(
                "/io.github.coastlinesec.hyprshelld.workspace-switcher-copy"
            );
        QVERIFY(QDir().mkpath(wrongDirectory));
        QVERIFY2(
            HyprShelld::Tests::copyFile(
                sourceComponentDirectory + QStringLiteral("/manifest.json"),
                wrongDirectory + QStringLiteral("/manifest.json"),
                error
            ),
            qPrintable(error)
        );
        QVERIFY2(
            HyprShelld::Tests::copyFile(
                sourceComponentDirectory
                    + QStringLiteral("/settings.schema.json"),
                wrongDirectory + QStringLiteral("/settings.schema.json"),
                error
            ),
            qPrintable(error)
        );

        auto loaded = HyprShelld::Components::SystemCatalog::load(
            wrongDirectoryRoot.path()
        );
        QVERIFY(!loaded.ok());
        QVERIFY(loaded.error.contains(
            QStringLiteral("directory does not match manifest ID")
        ));

        QTemporaryDir malformedSchemaRoot;
        QVERIFY(malformedSchemaRoot.isValid());
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                malformedSchemaRoot.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );
        const auto schemaPath = HyprShelld::Tests::componentDirectory(
                                    malformedSchemaRoot.path()
                                )
            + QStringLiteral("/settings.schema.json");
        QVERIFY2(
            HyprShelld::Tests::replaceFile(
                schemaPath,
                QByteArrayLiteral("{\"schemaVersion\":1,\"schemaVersion\":1}"),
                error
            ),
            qPrintable(error)
        );

        loaded = HyprShelld::Components::SystemCatalog::load(
            malformedSchemaRoot.path()
        );
        QVERIFY(!loaded.ok());
        QVERIFY(loaded.error.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));
    }

    void unexpectedAndLinkedContentIsRejected()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                root.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );

        const auto component = HyprShelld::Tests::componentDirectory(root.path());
        QFile unexpected(component + QStringLiteral("/payload.qml"));
        QVERIFY(unexpected.open(QIODevice::WriteOnly | QIODevice::NewOnly));
        QCOMPARE(unexpected.write(QByteArrayLiteral("Item {}")), 7);
        unexpected.close();

        auto loaded = HyprShelld::Components::SystemCatalog::load(root.path());
        QVERIFY(!loaded.ok());
        QVERIFY(loaded.error.contains(QStringLiteral("Unexpected system component contents")));

        QVERIFY(unexpected.remove());
        const auto schemaPath = component
            + QStringLiteral("/settings.schema.json");
        QVERIFY(QFile::remove(schemaPath));
        QTemporaryFile externalSchema;
        QVERIFY(externalSchema.open());
        QCOMPARE(
            externalSchema.write(
                QByteArrayLiteral("{\"schemaVersion\":1,\"settings\":[]}")
            ),
            33
        );
        QVERIFY(externalSchema.flush());
        QVERIFY(QFile::link(externalSchema.fileName(), schemaPath));

        loaded = HyprShelld::Components::SystemCatalog::load(root.path());
        QVERIFY(!loaded.ok());
        QVERIFY(loaded.error.contains(QStringLiteral("symbolic link")));
    }
};

QTEST_APPLESS_MAIN(ComponentdCatalogTest)

#include "componentd_catalog_test.moc"
