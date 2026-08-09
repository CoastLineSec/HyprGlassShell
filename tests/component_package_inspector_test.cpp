#include "component-inspector/package_inspector.h"

#include "component/component_package_bundle.h"
#include "component/declarative_document.h"
#include "component/package_integrity.h"
#include "component/package_inspection_report.h"

#include <zip.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <QtEndian>

#include <array>
#include <sys/stat.h>

using namespace HyprShelld::Components;

namespace {

const QString inspectionToken = QStringLiteral(
    "00112233445566778899aabbccddeeff"
);
constexpr quint16 testExtraFieldId = 0xcafe;
constexpr quint16 zip64ExtraFieldId = 0x0001;

struct ArchiveEntry final {
    QByteArray path;
    QByteArray contents;
    mode_t mode = S_IFREG | 0644;
    zip_int32_t compression = ZIP_CM_DEFLATE;
    bool addTestExtraField = false;
};

QByteArray manifestBytes()
{
    return QByteArrayLiteral(R"({
        "manifestVersion": 1,
        "id": "org.example.clock",
        "version": "1.0.0",
        "type": "bar-widget",
        "name": "Example Clock",
        "description": "A package inspection fixture.",
        "authors": [{"name": "Fixture Author"}],
        "license": "MIT",
        "componentApiVersion": "1.0",
        "runtime": {
            "kind": "declarative-v1",
            "entrypoint": "payload/widget.json"
        },
        "requestedCapabilities": []
    })");
}

QByteArray sha256(const QByteArray &contents)
{
    return QCryptographicHash::hash(
        contents,
        QCryptographicHash::Sha256
    ).toHex();
}

QByteArray integrityBytes(const QVector<ArchiveEntry> &entries)
{
    QJsonObject files;
    for (const auto &entry : entries) {
        if ((entry.mode & S_IFMT) == S_IFREG
            && entry.path != QByteArrayLiteral("integrity.json")) {
            files.insert(
                QString::fromUtf8(entry.path),
                QString::fromLatin1(sha256(entry.contents))
            );
        }
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("integrityVersion"), 1},
        {QStringLiteral("algorithm"), QStringLiteral("sha256")},
        {QStringLiteral("files"), files},
    }).toJson(QJsonDocument::Compact);
}

QVector<ArchiveEntry> validEntries()
{
    QVector<ArchiveEntry> entries{
        {QByteArrayLiteral("manifest.json"), manifestBytes()},
        {QByteArrayLiteral("payload/widget.json"),
         QByteArrayLiteral(
             "{\"documentVersion\":1,\"type\":\"text-pill\","
             "\"text\":{\"literal\":\"12:34\"}}\n"
         )},
    };
    entries.append({
        QByteArrayLiteral("integrity.json"),
        integrityBytes(entries),
    });
    return entries;
}

bool writeArchive(const QString &path, QVector<ArchiveEntry> entries)
{
    std::ranges::sort(entries, {}, &ArchiveEntry::path);
    int openError = 0;
    auto *archive = zip_open(
        QFile::encodeName(path).constData(),
        ZIP_CREATE | ZIP_TRUNCATE,
        &openError
    );
    if (archive == nullptr) {
        return false;
    }
    for (const auto &entry : entries) {
        auto *source = zip_source_buffer(
            archive,
            entry.contents.constData(),
            static_cast<zip_uint64_t>(entry.contents.size()),
            0
        );
        if (source == nullptr) {
            zip_discard(archive);
            return false;
        }
        const auto index = zip_file_add(
            archive,
            entry.path.constData(),
            source,
            ZIP_FL_ENC_UTF_8
        );
        if (index < 0) {
            zip_source_free(source);
            zip_discard(archive);
            return false;
        }
        if (zip_set_file_compression(
                   archive,
                   static_cast<zip_uint64_t>(index),
                   entry.compression,
                   6
               ) != 0
            || zip_file_set_external_attributes(
                   archive,
                   static_cast<zip_uint64_t>(index),
                   0,
                   ZIP_OPSYS_UNIX,
                   static_cast<zip_uint32_t>(entry.mode) << 16U
               ) != 0) {
            zip_discard(archive);
            return false;
        }
        if (entry.addTestExtraField) {
            const std::array<zip_uint8_t, 4> testExtraFieldData{
                0x01,
                0x02,
                0x03,
                0x04,
            };
            for (const auto location : {
                     zip_flags_t(ZIP_FL_CENTRAL),
                     zip_flags_t(ZIP_FL_LOCAL),
                 }) {
                if (zip_file_extra_field_set(
                        archive,
                        static_cast<zip_uint64_t>(index),
                        testExtraFieldId,
                        ZIP_EXTRA_FIELD_NEW,
                        testExtraFieldData.data(),
                        static_cast<zip_uint16_t>(
                            testExtraFieldData.size()
                        ),
                        location
                    ) != 0) {
                    zip_discard(archive);
                    return false;
                }
            }
        }
    }
    return zip_close(archive) == 0;
}

QString archiveDigest(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromLatin1(sha256(file.readAll()));
}

qsizetype centralRecordOffset(
    const QByteArrayView archive,
    const QByteArrayView expectedName
)
{
    constexpr quint32 endSignature = 0x06054b50;
    constexpr quint32 centralSignature = 0x02014b50;
    for (qsizetype end = archive.size() - 22; end >= 0; --end) {
        if (qFromLittleEndian<quint32>(
                reinterpret_cast<const uchar *>(archive.data() + end)
            ) != endSignature) {
            continue;
        }
        const auto centralOffset = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar *>(archive.data() + end + 16)
        );
        auto offset = static_cast<qsizetype>(centralOffset);
        const auto count = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(archive.data() + end + 10)
        );
        for (quint16 index = 0; index < count; ++index) {
            if (offset + 46 > end
                || qFromLittleEndian<quint32>(
                       reinterpret_cast<const uchar *>(archive.data() + offset)
                   ) != centralSignature) {
                return -1;
            }
            const auto nameLength = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar *>(archive.data() + offset + 28)
            );
            const auto extraLength = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar *>(archive.data() + offset + 30)
            );
            const auto commentLength = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar *>(archive.data() + offset + 32)
            );
            if (QByteArrayView(archive.data() + offset + 46, nameLength)
                == expectedName) {
                return offset;
            }
            offset += 46 + nameLength + extraLength + commentLength;
        }
        return -1;
    }
    return -1;
}

qsizetype localRecordOffset(
    const QByteArrayView archive,
    const QByteArrayView expectedName
)
{
    const auto central = centralRecordOffset(archive, expectedName);
    if (central < 0) {
        return -1;
    }
    return static_cast<qsizetype>(qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(archive.data() + central + 42)
    ));
}

qsizetype extraFieldOffset(
    const QByteArrayView archive,
    const qsizetype start,
    const qsizetype length,
    const quint16 identifier
)
{
    if (start < 0 || length < 0 || start > archive.size()
        || length > archive.size() - start) {
        return -1;
    }
    const auto end = start + length;
    auto offset = start;
    while (offset < end) {
        if (end - offset < 4) {
            return -1;
        }
        const auto fieldIdentifier = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(archive.data() + offset)
        );
        const auto fieldSize = static_cast<qsizetype>(
            qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar *>(archive.data() + offset + 2)
            )
        );
        if (fieldSize > end - offset - 4) {
            return -1;
        }
        if (fieldIdentifier == identifier) {
            return offset;
        }
        offset += 4 + fieldSize;
    }
    return -1;
}

bool rewriteArchive(const QString &path, const QByteArray &bytes)
{
    QFile archive(path);
    return archive.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && archive.write(bytes) == bytes.size() && archive.flush();
}

PackageInspectionResult inspect(
    const QString &path,
    QByteArray *bundleBytes = nullptr
)
{
    QFile archive(path);
    if (!archive.open(QIODevice::ReadOnly)) {
        return {};
    }
    QBuffer bundle;
    if (bundleBytes != nullptr) {
        bundle.setBuffer(bundleBytes);
        bundle.open(QIODevice::WriteOnly);
    }
    return inspectComponentPackage(
        archive.handle(),
        inspectionToken,
        archiveDigest(path),
        bundleBytes == nullptr ? nullptr : &bundle
    );
}

bool hasErrorCode(
    const PackageInspectionResult &result,
    const QString &code
)
{
    return std::ranges::any_of(
        result.errors,
        [&code](const ValidationError &error) {
            return error.code == code;
        }
    );
}

} // namespace

class ComponentPackageInspectorTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsValidDeclarativePackage()
    {
        QTemporaryDir archiveDirectory;
        QVERIFY(archiveDirectory.isValid());
        const auto archivePath = archiveDirectory.filePath(
            QStringLiteral("clock.hyprshelld-component")
        );
        QVERIFY(writeArchive(archivePath, validEntries()));

        QByteArray bundleBytes;
        const auto result = inspect(archivePath, &bundleBytes);
        QVERIFY2(result, qPrintable(
            result.errors.isEmpty() ? QString() : result.errors.first().message
        ));
        QCOMPARE(result.report->inspectionToken, inspectionToken);
        QCOMPARE(result.report->archiveSha256, archiveDigest(archivePath));
        QCOMPARE(result.report->manifest.id, QStringLiteral("org.example.clock"));
        QCOMPARE(result.report->files.size(), 3);
        QVERIFY(!result.report->packageDigest.isEmpty());
        QCOMPARE(
            result.report->declarativeRuntime,
            QByteArrayLiteral(
                "{\"documentVersion\":1,\"text\":{\"literal\":\"12:34\"},"
                "\"type\":\"text-pill\"}"
            )
        );

        const auto reportBytes = serializePackageInspectionReport(
            *result.report
        );
        QCOMPARE(
            QJsonDocument::fromJson(reportBytes).object().value(
                QStringLiteral("activationState")
            ).toString(),
            QStringLiteral("supported")
        );
        const auto parsedReport = parsePackageInspectionReport(reportBytes);
        QVERIFY(parsedReport);
        QCOMPARE(parsedReport.value->packageDigest, result.report->packageDigest);

        QBuffer bundleReader(&bundleBytes);
        QVERIFY(bundleReader.open(QIODevice::ReadOnly));
        const auto parsedBundle = readComponentPackageBundle(bundleReader);
        QVERIFY(parsedBundle);
        QCOMPARE(parsedBundle.value->size(), 3);
        QCOMPARE(
            deriveComponentPackageDigest(*parsedBundle.value),
            result.report->packageDigest
        );

        QTemporaryDir destination;
        QVERIFY(destination.isValid());
        QBuffer materializeReader(&bundleBytes);
        QVERIFY(materializeReader.open(QIODevice::ReadOnly));
        const auto errors = materializeComponentPackageBundle(
            materializeReader,
            *result.report,
            destination.path()
        );
        QVERIFY(errors.isEmpty());
        const QFileInfo payload(destination.filePath(
            QStringLiteral("payload/widget.json")
        ));
        QVERIFY(payload.isFile());
        QVERIFY(!(payload.permissions() & QFileDevice::ExeOwner));

        auto mismatchedReport = *result.report;
        const auto otherDocument = parseDeclarativeDocument(
            R"({"documentVersion":1,"type":"text-pill","text":{"literal":"Different"}})"
        );
        QVERIFY(otherDocument);
        mismatchedReport.declarativeRuntime = serializeDeclarativeDocument(
            *otherDocument.value
        );
        QTemporaryDir rejectedDestination;
        QVERIFY(rejectedDestination.isValid());
        QBuffer rejectedReader(&bundleBytes);
        QVERIFY(rejectedReader.open(QIODevice::ReadOnly));
        const auto rejectedErrors = materializeComponentPackageBundle(
            rejectedReader,
            mismatchedReport,
            rejectedDestination.path()
        );
        QVERIFY(std::ranges::any_of(
            rejectedErrors,
            [](const ValidationError &error) {
                return error.code == QStringLiteral(
                    "package-bundle.declarative-runtime-mismatch"
                );
            }
        ));
    }

    void rejectsUntrustedDeclarativeFields()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("action.zip"));
        auto entries = validEntries();
        entries[1].contents = QByteArrayLiteral(
            "{\"documentVersion\":1,\"type\":\"text-pill\","
            "\"text\":{\"literal\":\"Clock\"},"
            "\"action\":{\"command\":\"date\"}}"
        );
        entries[2].contents = integrityBytes(entries.sliced(0, 2));
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(
            result,
            QStringLiteral("declarative.unknown-field")
        ));
    }

    void validatesSettingBindingsAgainstTheTrustedSchema()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("setting.zip"));
        auto manifest = QJsonDocument::fromJson(manifestBytes()).object();
        manifest.insert(
            QStringLiteral("settingsSchema"),
            QStringLiteral("settings.schema.json")
        );
        const auto schema = QByteArrayLiteral(R"({
          "schemaVersion":1,
          "settings":[
            {"key":"displayMode","scope":"component","type":"string","label":"Display mode","description":"Text shown by the pill.","group":"general","order":1,"default":"Clock","minimumLength":1,"maximumLength":64}
          ]
        })");
        QVector<ArchiveEntry> entries{
            {QByteArrayLiteral("manifest.json"),
             QJsonDocument(manifest).toJson(QJsonDocument::Compact)},
            {QByteArrayLiteral("payload/widget.json"),
             QByteArrayLiteral(
                 "{\"documentVersion\":1,\"type\":\"text-pill\","
                 "\"text\":{\"setting\":\"displayMode\"}}"
             )},
            {QByteArrayLiteral("settings.schema.json"), schema},
        };
        entries.append({
            QByteArrayLiteral("integrity.json"),
            integrityBytes(entries),
        });
        QVERIFY(writeArchive(path, entries));
        auto result = inspect(path);
        QVERIFY2(result, qPrintable(
            result.errors.isEmpty() ? QString() : result.errors.first().message
        ));

        entries[1].contents.replace("displayMode", "missingMode");
        entries.last().contents = integrityBytes(entries.sliced(0, 3));
        QVERIFY(writeArchive(path, entries));
        result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(
            result,
            QStringLiteral("declarative.unknown-setting")
        ));
    }

    void requestedCapabilitiesKeepDeclarativePackageInert()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("capability.zip"));
        auto entries = validEntries();
        auto manifest = QJsonDocument::fromJson(entries[0].contents).object();
        manifest.insert(
            QStringLiteral("requestedCapabilities"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("example.clock.read")},
                {QStringLiteral("reason"),
                 QStringLiteral("Read an external clock source.")},
            }}
        );
        entries[0].contents = QJsonDocument(manifest).toJson(
            QJsonDocument::Compact
        );
        entries[2].contents = integrityBytes(entries.sliced(0, 2));
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(result);
        auto reportBytes = serializePackageInspectionReport(*result.report);
        QCOMPARE(
            QJsonDocument::fromJson(reportBytes).object().value(
                QStringLiteral("activationState")
            ).toString(),
            QStringLiteral("unsupported")
        );
        reportBytes.replace(
            QByteArrayLiteral("\"activationState\":\"unsupported\""),
            QByteArrayLiteral("\"activationState\":\"supported\"")
        );
        const auto tampered = parsePackageInspectionReport(reportBytes);
        QVERIFY(!tampered);
        QVERIFY(std::ranges::any_of(
            tampered.errors,
            [](const ValidationError &error) {
                return error.code == QStringLiteral(
                    "inspection-report.activation-state-mismatch"
                );
            }
        ));
    }

    void rejectsIntegrityMismatch()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("bad.zip"));
        auto entries = validEntries();
        entries[0].contents.append(' ');
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.integrity-mismatch")));
    }

    void rejectsTraversal()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("traversal.zip"));
        auto entries = validEntries();
        entries.append({QByteArrayLiteral("payload/../escape"), QByteArrayLiteral("x")});
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-zip")));
    }

    void rejectsUnicodeFormatControlsInPackageAndIntegrityPaths()
    {
        const auto rightToLeftOverride = QStringLiteral(
            "payload/safe\u202Egnp.exe"
        );
        const auto zeroWidthJoiner = QStringLiteral(
            "payload/safe\u200Dname.json"
        );
        const auto supplementaryFormat = QStringLiteral(
            "payload/safe\U000E0001name.json"
        );
        QVERIFY(!isCanonicalPackageFilePath(rightToLeftOverride));
        QVERIFY(!isCanonicalPackageFilePath(zeroWidthJoiner));
        QVERIFY(!isCanonicalPackageFilePath(supplementaryFormat));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto archivePath = directory.filePath(
            QStringLiteral("format-control.zip")
        );
        auto entries = validEntries();
        entries.append({
            rightToLeftOverride.toUtf8(),
            QByteArrayLiteral("untrusted"),
        });
        QVERIFY(writeArchive(archivePath, entries));
        const auto inspected = inspect(archivePath);
        QVERIFY(!inspected);
        QVERIFY(hasErrorCode(
            inspected,
            QStringLiteral("package.invalid-zip")
        ));

        const auto integrityDocument = QJsonDocument(QJsonObject{
            {QStringLiteral("integrityVersion"), 1},
            {QStringLiteral("algorithm"), QStringLiteral("sha256")},
            {QStringLiteral("files"), QJsonObject{
                 {rightToLeftOverride, QString(64, QLatin1Char('0'))},
             }},
        }).toJson(QJsonDocument::Compact);
        const auto integrity = parsePackageIntegrity(
            QByteArrayView(integrityDocument)
        );
        QVERIFY(!integrity.ok());
        QVERIFY(std::ranges::any_of(
            integrity.errors,
            [](const ValidationError &error) {
                return error.code == QStringLiteral("integrity.invalid-path");
            }
        ));
    }

    void rejectsCaseFoldCollision()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("collision.zip"));
        auto entries = validEntries();
        entries.append({QByteArrayLiteral("payload/Widget.json"), QByteArrayLiteral("x")});
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-zip")));
    }

    void rejectsSymbolicLinkAttributes()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("link.zip"));
        auto entries = validEntries();
        entries.append({
            QByteArrayLiteral("payload/link"),
            QByteArrayLiteral("widget.json"),
            S_IFLNK | 0777,
        });
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-zip")));
    }

    void rejectsEmbeddedNulFilename()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("nul.zip"));
        auto entries = validEntries();
        entries[1].path = QByteArrayLiteral("payload/abcdefghijk");
        entries[0].contents.replace(
            QByteArrayLiteral("payload/widget.json"),
            QByteArrayLiteral("payload/abcdefghijk")
        );
        entries[2].contents = integrityBytes(entries.sliced(0, 2));
        QVERIFY(writeArchive(path, entries));

        QFile archive(path);
        QVERIFY(archive.open(QIODevice::ReadWrite));
        auto bytes = archive.readAll();
        const auto needle = QByteArrayLiteral("payload/abcdefghijk");
        int replacements = 0;
        qsizetype offset = 0;
        while ((offset = bytes.indexOf(needle, offset)) >= 0) {
            bytes[offset + 9] = '\0';
            ++replacements;
            offset += needle.size();
        }
        QCOMPARE(replacements, 2);
        QVERIFY(archive.resize(0));
        QVERIFY(archive.seek(0));
        QCOMPARE(archive.write(bytes), bytes.size());
        archive.close();

        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-zip")));
    }

    void rejectsLocalCentralFilenameMismatch()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("name-mismatch.zip"));
        auto entries = validEntries();
        for (auto &entry : entries) {
            entry.compression = ZIP_CM_STORE;
        }
        QVERIFY(writeArchive(path, entries));

        QFile archive(path);
        QVERIFY(archive.open(QIODevice::ReadOnly));
        auto bytes = archive.readAll();
        archive.close();
        const auto name = QByteArrayLiteral("payload/widget.json");
        const auto local = localRecordOffset(bytes, name);
        QVERIFY(local >= 0);
        bytes[local + 30] = 'q';
        QVERIFY(rewriteArchive(path, bytes));

        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-zip")));
    }

    void rejectsUnsupportedGeneralPurposeFlags()
    {
        struct Scenario final {
            const char *name;
            quint16 flag;
            bool store = false;
        };
        const std::array scenarios{
            Scenario{"reserved", 0x0010, false},
            Scenario{"patched-data", 0x0020, false},
            Scenario{"masked-header", 0x2000, false},
            Scenario{"stored-compression-option", 0x0002, true},
        };

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto entryName = QByteArrayLiteral("payload/widget.json");
        for (const auto &scenario : scenarios) {
            const auto path = directory.filePath(
                QString::fromLatin1(scenario.name) + QStringLiteral(".zip")
            );
            auto entries = validEntries();
            if (scenario.store) {
                for (auto &entry : entries) {
                    entry.compression = ZIP_CM_STORE;
                }
            }
            QVERIFY(writeArchive(path, entries));

            QFile archive(path);
            QVERIFY(archive.open(QIODevice::ReadOnly));
            auto bytes = archive.readAll();
            archive.close();
            const auto central = centralRecordOffset(bytes, entryName);
            const auto local = localRecordOffset(bytes, entryName);
            QVERIFY(central >= 0);
            QVERIFY(local >= 0);
            const auto originalFlags = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar *>(
                    bytes.constData() + central + 8
                )
            );
            const auto unsupportedFlags = static_cast<quint16>(
                originalFlags | scenario.flag
            );
            qToLittleEndian<quint16>(
                unsupportedFlags,
                reinterpret_cast<uchar *>(bytes.data() + central + 8)
            );
            qToLittleEndian<quint16>(
                unsupportedFlags,
                reinterpret_cast<uchar *>(bytes.data() + local + 6)
            );
            QVERIFY(rewriteArchive(path, bytes));

            const auto result = inspect(path);
            QVERIFY(!result);
            QVERIFY(hasErrorCode(
                result,
                QStringLiteral("package.invalid-zip")
            ));
            QCOMPARE(
                result.errors.first().message,
                QStringLiteral("An entry uses unsupported ZIP features.")
            );
        }
    }

    void acceptsSupportedDeflateAndUtf8Flags()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(
            QStringLiteral("supported-flags.zip")
        );
        QVERIFY(writeArchive(path, validEntries()));

        QFile archive(path);
        QVERIFY(archive.open(QIODevice::ReadOnly));
        auto bytes = archive.readAll();
        archive.close();
        const auto entryName = QByteArrayLiteral("payload/widget.json");
        const auto central = centralRecordOffset(bytes, entryName);
        const auto local = localRecordOffset(bytes, entryName);
        QVERIFY(central >= 0);
        QVERIFY(local >= 0);
        const auto originalFlags = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(bytes.constData() + central + 8)
        );
        constexpr quint16 maximumDeflateAndUtf8 = 0x0806;
        const auto supportedFlags = static_cast<quint16>(
            originalFlags | maximumDeflateAndUtf8
        );
        qToLittleEndian<quint16>(
            supportedFlags,
            reinterpret_cast<uchar *>(bytes.data() + central + 8)
        );
        qToLittleEndian<quint16>(
            supportedFlags,
            reinterpret_cast<uchar *>(bytes.data() + local + 6)
        );
        QVERIFY(rewriteArchive(path, bytes));

        const auto result = inspect(path);
        QVERIFY2(result, qPrintable(
            result.errors.isEmpty()
                ? QString()
                : result.errors.first().message
        ));
    }

    void rejectsZip64ExtraFieldsInCentralAndLocalHeaders()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto entryName = QByteArrayLiteral("payload/widget.json");
        for (const auto centralHeader : {true, false}) {
            const auto path = directory.filePath(
                centralHeader
                    ? QStringLiteral("zip64-central.zip")
                    : QStringLiteral("zip64-local.zip")
            );
            auto entries = validEntries();
            entries[1].addTestExtraField = true;
            QVERIFY(writeArchive(path, entries));
            const auto baseline = inspect(path);
            QVERIFY2(baseline, qPrintable(
                baseline.errors.isEmpty()
                    ? QString()
                    : baseline.errors.first().message
            ));

            QFile archive(path);
            QVERIFY(archive.open(QIODevice::ReadOnly));
            auto bytes = archive.readAll();
            archive.close();

            qsizetype field = -1;
            if (centralHeader) {
                const auto central = centralRecordOffset(bytes, entryName);
                QVERIFY(central >= 0);
                const auto nameLength = qFromLittleEndian<quint16>(
                    reinterpret_cast<const uchar *>(
                        bytes.constData() + central + 28
                    )
                );
                const auto extraLength = qFromLittleEndian<quint16>(
                    reinterpret_cast<const uchar *>(
                        bytes.constData() + central + 30
                    )
                );
                field = extraFieldOffset(
                    bytes,
                    central + 46 + nameLength,
                    extraLength,
                    testExtraFieldId
                );
            } else {
                const auto local = localRecordOffset(bytes, entryName);
                QVERIFY(local >= 0);
                const auto nameLength = qFromLittleEndian<quint16>(
                    reinterpret_cast<const uchar *>(
                        bytes.constData() + local + 26
                    )
                );
                const auto extraLength = qFromLittleEndian<quint16>(
                    reinterpret_cast<const uchar *>(
                        bytes.constData() + local + 28
                    )
                );
                field = extraFieldOffset(
                    bytes,
                    local + 30 + nameLength,
                    extraLength,
                    testExtraFieldId
                );
            }
            QVERIFY(field >= 0);
            qToLittleEndian<quint16>(
                zip64ExtraFieldId,
                reinterpret_cast<uchar *>(bytes.data() + field)
            );
            QVERIFY(rewriteArchive(path, bytes));

            const auto result = inspect(path);
            QVERIFY(!result);
            QVERIFY(hasErrorCode(
                result,
                QStringLiteral("package.invalid-zip")
            ));
            QCOMPARE(
                result.errors.first().message,
                centralHeader
                    ? QStringLiteral(
                        "An entry has malformed or ZIP64 central extra metadata."
                    )
                    : QStringLiteral(
                        "An entry has malformed or ZIP64 local extra metadata."
                    )
            );
        }
    }

    void rejectsCrcMismatch()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("crc.zip"));
        auto entries = validEntries();
        for (auto &entry : entries) {
            entry.compression = ZIP_CM_STORE;
        }
        QVERIFY(writeArchive(path, entries));

        QFile archive(path);
        QVERIFY(archive.open(QIODevice::ReadOnly));
        auto bytes = archive.readAll();
        archive.close();
        const auto name = QByteArrayLiteral("payload/widget.json");
        const auto local = localRecordOffset(bytes, name);
        QVERIFY(local >= 0);
        const auto nameLength = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(bytes.constData() + local + 26)
        );
        const auto extraLength = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(bytes.constData() + local + 28)
        );
        const auto data = local + 30 + nameLength + extraLength;
        QVERIFY(data < bytes.size());
        bytes[data] ^= 0x01;
        QVERIFY(rewriteArchive(path, bytes));

        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(
            hasErrorCode(result, QStringLiteral("package.invalid-zip"))
            || hasErrorCode(result, QStringLiteral("package.zip-read"))
        );
    }

    void rejectsIntegrityMetadataOver512KiB()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("integrity-large.zip"));
        auto entries = validEntries();
        entries.last().contents = QByteArray(512 * 1024 + 1, ' ');
        QVERIFY(writeArchive(path, entries));

        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.expanded-size")));
    }

    void rejectsMalformedIcon()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("icon.zip"));
        auto entries = validEntries();
        entries.removeLast();
        entries.append({QByteArrayLiteral("icon.png"), QByteArrayLiteral("not-png")});
        entries.append({QByteArrayLiteral("integrity.json"), integrityBytes(entries)});
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-icon")));
    }

    void rejectsSetuidAttributes()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("setuid.zip"));
        auto entries = validEntries();
        entries.append({
            QByteArrayLiteral("payload/privileged"),
            QByteArrayLiteral("x"),
            S_IFREG | S_ISUID | 0755,
        });
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-zip")));
    }

    void rejectsMoreThan512Entries()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("many.zip"));
        auto entries = validEntries();
        for (int index = 0; index < 510; ++index) {
            entries.append({
                QByteArrayLiteral("payload/extra-")
                    + QByteArray::number(index).rightJustified(3, '0'),
                QByteArrayLiteral("x"),
            });
        }
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.invalid-zip")));
    }

    void rejectsExpandedFileLimit()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("large.zip"));
        auto entries = validEntries();
        entries.append({
            QByteArrayLiteral("payload/large.bin"),
            QByteArray(maximumComponentFileBytes + 1, 'x'),
        });
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(hasErrorCode(result, QStringLiteral("package.expanded-size")));
    }

    void neverExecutesPayload()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("no-exec.zip"));
        const auto marker = directory.filePath(QStringLiteral("executed"));
        auto entries = validEntries();
        entries[1].contents = QByteArrayLiteral("#!/bin/sh\ntouch \"")
            + QFile::encodeName(marker) + QByteArrayLiteral("\"\n");
        entries[2].contents = integrityBytes(entries.sliced(0, 2));
        QVERIFY(writeArchive(path, entries));
        const auto result = inspect(path);
        QVERIFY(!result);
        QVERIFY(!result.errors.isEmpty());
        QVERIFY(!QFileInfo::exists(marker));
    }

    void rejectsArchiveDigestMismatch()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("digest.zip"));
        QVERIFY(writeArchive(path, validEntries()));
        QFile archive(path);
        QVERIFY(archive.open(QIODevice::ReadOnly));
        const auto result = inspectComponentPackage(
            archive.handle(),
            inspectionToken,
            QString(64, QLatin1Char('0'))
        );
        QVERIFY(!result);
        QVERIFY(hasErrorCode(
            result,
            QStringLiteral("package.archive-digest-mismatch")
        ));
    }
};

QTEST_GUILESS_MAIN(ComponentPackageInspectorTest)

#include "component_package_inspector_test.moc"
