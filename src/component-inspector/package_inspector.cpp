#include "package_inspector.h"

#include "component/component_package_bundle.h"
#include "component/declarative_document.h"
#include "component/package_integrity.h"
#include "component/settings_schema.h"
#include "component/strict_json.h"

#include <zip.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace HyprShelld::Components {
namespace {

constexpr qint64 maximumManifestBytes = 128 * 1024;
constexpr qint64 maximumIntegrityBytes = 512 * 1024;
constexpr qint64 maximumSettingsSchemaBytes = 256 * 1024;
constexpr qint64 maximumIconBytes = 4 * 1024 * 1024;
constexpr int maximumIconDimension = 1024;
constexpr qint64 maximumDecodedIconBytes = 16 * 1024 * 1024;
constexpr quint32 endOfCentralDirectorySignature = 0x06054b50;
constexpr quint32 centralDirectorySignature = 0x02014b50;
constexpr quint32 localFileSignature = 0x04034b50;
constexpr quint16 zip64ExtraFieldId = 0x0001;
constexpr quint16 compressionOptionFlags = 0x0006;
constexpr quint16 dataDescriptorFlag = 0x0008;
constexpr quint16 utf8NameFlag = 0x0800;
constexpr quint16 supportedGeneralPurposeFlags = compressionOptionFlags
    | dataDescriptorFlag | utf8NameFlag;

struct ZipArchiveDeleter final {
    void operator()(zip_t *archive) const
    {
        zip_discard(archive);
    }
};

struct ZipFileDeleter final {
    void operator()(zip_file_t *file) const
    {
        zip_fclose(file);
    }
};

using ZipArchive = std::unique_ptr<zip_t, ZipArchiveDeleter>;
using ZipFile = std::unique_ptr<zip_file_t, ZipFileDeleter>;

struct RawZipEntry final {
    QByteArray name;
    bool directory = false;
    quint16 compressionMethod = 0;
};

void addError(
    ValidationErrors &errors,
    QString path,
    QString code,
    QString message
)
{
    errors.append({
        .path = std::move(path),
        .code = std::move(code),
        .message = std::move(message),
    });
}

PackageInspectionResult singleError(
    QString path,
    QString code,
    QString message
)
{
    PackageInspectionResult result;
    addError(
        result.errors,
        std::move(path),
        std::move(code),
        std::move(message)
    );
    return result;
}

quint16 readU16(const QByteArrayView bytes, const qsizetype offset)
{
    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar *>(bytes.data() + offset)
    );
}

quint32 readU32(const QByteArrayView bytes, const qsizetype offset)
{
    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(bytes.data() + offset)
    );
}

bool generalPurposeFlagsAreSupported(
    const quint16 flags,
    const quint16 compressionMethod
)
{
    return (flags & supportedGeneralPurposeFlags) == flags
        && (compressionMethod == ZIP_CM_DEFLATE
            || (flags & compressionOptionFlags) == 0);
}

bool extraFieldsAreSupported(const QByteArrayView bytes)
{
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        if (bytes.size() - offset < 4) {
            return false;
        }
        const auto identifier = readU16(bytes, offset);
        const auto size = static_cast<qsizetype>(
            readU16(bytes, offset + 2)
        );
        offset += 4;
        if (size > bytes.size() - offset
            || identifier == zip64ExtraFieldId) {
            return false;
        }
        offset += size;
    }
    return true;
}

bool strictUtf8Path(const QByteArray &encoded, QString &path)
{
    if (encoded.isEmpty() || encoded.size() > 255
        || encoded.contains('\0')) {
        return false;
    }
    path = QString::fromUtf8(encoded);
    return path.toUtf8() == encoded;
}

bool isAllowedDirectoryPath(QString path)
{
    if (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return isCanonicalPackageFilePath(path)
        && (path == QStringLiteral("payload")
            || path == QStringLiteral("LICENSES")
            || path.startsWith(QStringLiteral("payload/"))
            || path.startsWith(QStringLiteral("LICENSES/")));
}

bool externalAttributesAreSafe(
    const quint16 operatingSystem,
    const quint32 attributes,
    const bool directory
)
{
    if (operatingSystem == ZIP_OPSYS_UNIX) {
        const auto mode = static_cast<mode_t>(attributes >> 16U);
        const auto type = mode & S_IFMT;
        if ((mode & (S_ISUID | S_ISGID | S_ISVTX)) != 0) {
            return false;
        }
        if (type != 0
            && type != (directory ? S_IFDIR : S_IFREG)) {
            return false;
        }
    } else {
        constexpr quint32 dosDirectory = 0x10;
        constexpr quint32 dosDevice = 0x40;
        const auto dosAttributes = attributes & 0xffU;
        if ((dosAttributes & dosDevice) != 0
            || (((dosAttributes & dosDirectory) != 0) != directory)) {
            return false;
        }
    }
    return true;
}

bool preflightCentralDirectory(
    const QByteArray &archiveBytes,
    QVector<RawZipEntry> &entries,
    QString &error
)
{
    const QByteArrayView bytes(archiveBytes);
    if (bytes.size() < 22) {
        error = QStringLiteral("The ZIP end record is missing.");
        return false;
    }

    const auto earliest = std::max<qsizetype>(0, bytes.size() - 65557);
    qsizetype endOffset = -1;
    for (qsizetype offset = bytes.size() - 22; offset >= earliest; --offset) {
        if (readU32(bytes, offset) != endOfCentralDirectorySignature) {
            continue;
        }
        const auto commentLength = readU16(bytes, offset + 20);
        if (offset + 22 + commentLength == bytes.size()) {
            endOffset = offset;
            break;
        }
    }
    if (endOffset < 0) {
        error = QStringLiteral("The ZIP end record is malformed.");
        return false;
    }

    const auto disk = readU16(bytes, endOffset + 4);
    const auto centralDisk = readU16(bytes, endOffset + 6);
    const auto diskEntries = readU16(bytes, endOffset + 8);
    const auto totalEntries = readU16(bytes, endOffset + 10);
    const auto centralSize = readU32(bytes, endOffset + 12);
    const auto centralOffset = readU32(bytes, endOffset + 16);
    if (disk != 0 || centralDisk != 0 || diskEntries != totalEntries
        || totalEntries < 2
        || totalEntries > maximumComponentArchiveEntries
        || centralSize == 0xffffffffU || centralOffset == 0xffffffffU
        || static_cast<quint64>(centralOffset) + centralSize
            > static_cast<quint64>(endOffset)) {
        error = QStringLiteral("Multipart, ZIP64, empty, or oversized packages are not supported.");
        return false;
    }

    qsizetype offset = centralOffset;
    quint32 earliestLocalOffset = std::numeric_limits<quint32>::max();
    QMap<QString, QString> collisionOwners;
    entries.reserve(totalEntries);
    for (quint16 index = 0; index < totalEntries; ++index) {
        if (offset < 0 || offset + 46 > endOffset
            || readU32(bytes, offset) != centralDirectorySignature) {
            error = QStringLiteral("A central-directory entry is malformed.");
            return false;
        }
        const auto madeBy = readU16(bytes, offset + 4);
        const auto flags = readU16(bytes, offset + 8);
        const auto compressionMethod = readU16(bytes, offset + 10);
        const auto compressedSize = readU32(bytes, offset + 20);
        const auto expandedSize = readU32(bytes, offset + 24);
        const auto nameLength = readU16(bytes, offset + 28);
        const auto extraLength = readU16(bytes, offset + 30);
        const auto commentLength = readU16(bytes, offset + 32);
        const auto startDisk = readU16(bytes, offset + 34);
        const auto externalAttributes = readU32(bytes, offset + 38);
        const auto localOffset = readU32(bytes, offset + 42);
        earliestLocalOffset = std::min(earliestLocalOffset, localOffset);
        const auto completeSize = static_cast<quint64>(46) + nameLength
            + extraLength + commentLength;
        if (startDisk != 0 || nameLength == 0
            || compressedSize == 0xffffffffU || expandedSize == 0xffffffffU
            || localOffset == 0xffffffffU
            || static_cast<quint64>(offset) + completeSize
                > static_cast<quint64>(endOffset)
            || !generalPurposeFlagsAreSupported(flags, compressionMethod)
            || (compressionMethod != ZIP_CM_STORE
                && compressionMethod != ZIP_CM_DEFLATE)) {
            error = QStringLiteral("An entry uses unsupported ZIP features.");
            return false;
        }
        if (!extraFieldsAreSupported(bytes.sliced(
                offset + 46 + nameLength,
                extraLength
            ))) {
            error = QStringLiteral(
                "An entry has malformed or ZIP64 central extra metadata."
            );
            return false;
        }

        const auto rawName = QByteArray(
            bytes.sliced(offset + 46, nameLength)
        );
        QString path;
        if (!strictUtf8Path(rawName, path)) {
            error = QStringLiteral("A ZIP filename is empty, too long, contains NUL, or is not UTF-8.");
            return false;
        }
        const auto directory = path.endsWith(QLatin1Char('/'));
        auto collisionPath = path;
        if (directory) {
            collisionPath.chop(1);
        }
        if ((!directory
                && (!isCanonicalPackageFilePath(path)
                    || !isAllowedComponentPackageFilePath(path)))
            || (directory && !isAllowedDirectoryPath(path))) {
            error = QStringLiteral("A ZIP filename is unsafe or outside the package layout.");
            return false;
        }
        const auto folded = collisionPath.toCaseFolded();
        if (collisionOwners.contains(folded)) {
            error = QStringLiteral("ZIP filenames collide after NFC and case folding.");
            return false;
        }
        collisionOwners.insert(folded, collisionPath);

        const auto operatingSystem = static_cast<quint16>(madeBy >> 8U);
        if (!externalAttributesAreSafe(
                operatingSystem,
                externalAttributes,
                directory
            )) {
            error = QStringLiteral("A ZIP entry has unsafe external attributes.");
            return false;
        }

        if (static_cast<quint64>(localOffset) + 30 + nameLength
                > static_cast<quint64>(archiveBytes.size())
            || readU32(bytes, localOffset) != localFileSignature) {
            error = QStringLiteral("A ZIP local header is missing.");
            return false;
        }
        const auto localNameLength = readU16(bytes, localOffset + 26);
        const auto localExtraLength = readU16(bytes, localOffset + 28);
        const auto localFlags = readU16(bytes, localOffset + 6);
        const auto localCompressionMethod = readU16(bytes, localOffset + 8);
        if (localNameLength != nameLength || localFlags != flags
            || localCompressionMethod != compressionMethod
            || !generalPurposeFlagsAreSupported(
                localFlags,
                localCompressionMethod
            )
            || static_cast<quint64>(localOffset) + 30 + localNameLength
                    + localExtraLength
                > static_cast<quint64>(archiveBytes.size())
            || QByteArray(bytes.sliced(localOffset + 30, localNameLength))
                != rawName) {
            error = QStringLiteral("A ZIP local filename does not match its central entry.");
            return false;
        }
        if (!extraFieldsAreSupported(bytes.sliced(
                localOffset + 30 + localNameLength,
                localExtraLength
            ))) {
            error = QStringLiteral(
                "An entry has malformed or ZIP64 local extra metadata."
            );
            return false;
        }

        entries.append({rawName, directory, compressionMethod});
        offset += static_cast<qsizetype>(completeSize);
    }
    if (offset != static_cast<qsizetype>(centralOffset + centralSize)
        || static_cast<quint64>(centralOffset) + centralSize
            != static_cast<quint64>(endOffset)
        || earliestLocalOffset != 0
        || readU32(bytes, 0) != localFileSignature) {
        error = QStringLiteral("The ZIP central-directory size is inconsistent.");
        return false;
    }
    return true;
}

QString zipError(zip_t *archive)
{
    return QString::fromLocal8Bit(zip_error_strerror(zip_get_error(archive)));
}

bool readZipFile(
    zip_t *archive,
    const zip_uint64_t index,
    const quint64 declaredSize,
    QByteArray &contents,
    QString &error
)
{
    if (declaredSize > static_cast<quint64>(maximumComponentFileBytes)) {
        error = QStringLiteral("The file exceeds the per-file limit.");
        return false;
    }
    ZipFile file(zip_fopen_index(archive, index, ZIP_FL_ENC_RAW));
    if (!file) {
        error = zipError(archive);
        return false;
    }

    contents.clear();
    contents.reserve(static_cast<qsizetype>(declaredSize));
    std::array<char, 64 * 1024> buffer{};
    while (true) {
        const auto count = zip_fread(file.get(), buffer.data(), buffer.size());
        if (count == 0) {
            break;
        }
        if (count < 0) {
            error = QString::fromLocal8Bit(
                zip_file_strerror(file.get())
            );
            return false;
        }
        if (contents.size() > maximumComponentFileBytes - count) {
            error = QStringLiteral("The file streamed past the per-file limit.");
            return false;
        }
        contents.append(buffer.data(), static_cast<qsizetype>(count));
    }
    if (static_cast<quint64>(contents.size()) != declaredSize) {
        error = QStringLiteral("The streamed file size differs from the ZIP directory.");
        return false;
    }

    auto *rawFile = file.release();
    if (zip_fclose(rawFile) != 0) {
        error = QStringLiteral("The ZIP file checksum validation failed.");
        return false;
    }
    return true;
}

bool validateIcon(const QByteArray &contents, QString &error)
{
    if (contents.size() > maximumIconBytes) {
        error = QStringLiteral("icon.png exceeds its encoded size limit.");
        return false;
    }
    QBuffer buffer;
    buffer.setData(contents);
    if (!buffer.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("icon.png cannot be opened.");
        return false;
    }
    QImageReader reader(&buffer, QByteArrayLiteral("png"));
    reader.setDecideFormatFromContent(false);
    const auto size = reader.size();
    if (!size.isValid() || size.width() > maximumIconDimension
        || size.height() > maximumIconDimension
        || static_cast<qint64>(size.width()) * size.height() * 4
            > maximumDecodedIconBytes) {
        error = QStringLiteral("icon.png has invalid or excessive dimensions.");
        return false;
    }
    const auto image = reader.read();
    if (image.isNull() || image.size() != size) {
        error = QStringLiteral("icon.png is malformed.");
        return false;
    }
    return true;
}

} // namespace

PackageInspectionResult inspectComponentPackage(
    const int archiveFileDescriptor,
    const QString &inspectionToken,
    const QString &expectedArchiveSha256,
    QIODevice *materializedBundle
)
{
    static const QRegularExpression tokenPattern(
        QStringLiteral("^[0-9a-f]{32}$")
    );
    static const QRegularExpression digestPattern(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    if (!tokenPattern.match(inspectionToken).hasMatch()
        || !digestPattern.match(expectedArchiveSha256).hasMatch()) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.invalid-expectation"),
            QStringLiteral("The expected token or archive digest is malformed.")
        );
    }

    const auto ownedDescriptor = fcntl(
        archiveFileDescriptor,
        F_DUPFD_CLOEXEC,
        3
    );
    if (ownedDescriptor < 0) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.archive-descriptor"),
            QStringLiteral("Cannot duplicate the archive descriptor: %1")
                .arg(QString::fromLocal8Bit(std::strerror(errno)))
        );
    }
    struct stat status {};
    if (fstat(ownedDescriptor, &status) != 0 || !S_ISREG(status.st_mode)
        || status.st_size <= 0
        || status.st_size > maximumComponentArchiveBytes) {
        close(ownedDescriptor);
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.archive-file"),
            QStringLiteral("The package must be a nonempty regular file no larger than 32 MiB.")
        );
    }

    QFile source;
    if (!source.open(
            ownedDescriptor,
            QIODevice::ReadOnly,
            QFileDevice::AutoCloseHandle
        )) {
        close(ownedDescriptor);
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.archive-open"),
            QStringLiteral("Cannot open the package descriptor.")
        );
    }
    const auto archiveBytes = source.read(maximumComponentArchiveBytes + 1);
    if (archiveBytes.size() != status.st_size
        || source.error() != QFileDevice::NoError) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.archive-read"),
            QStringLiteral("Cannot read the exact bounded package bytes.")
        );
    }
    const auto archiveSha256 = QString::fromLatin1(
        QCryptographicHash::hash(
            archiveBytes,
            QCryptographicHash::Sha256
        ).toHex()
    );
    if (archiveSha256 != expectedArchiveSha256) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.archive-digest-mismatch"),
            QStringLiteral("The package bytes differ from the expected archive digest.")
        );
    }

    QVector<RawZipEntry> rawEntries;
    QString rawError;
    if (!preflightCentralDirectory(archiveBytes, rawEntries, rawError)) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.invalid-zip"),
            rawError
        );
    }

    zip_error_t openError;
    zip_error_init(&openError);
    auto *zipSource = zip_source_buffer_create(
        archiveBytes.constData(),
        static_cast<zip_uint64_t>(archiveBytes.size()),
        0,
        &openError
    );
    if (zipSource == nullptr) {
        const auto message = QString::fromLocal8Bit(
            zip_error_strerror(&openError)
        );
        zip_error_fini(&openError);
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.invalid-zip"),
            message
        );
    }
    auto *opened = zip_open_from_source(
        zipSource,
        ZIP_RDONLY | ZIP_CHECKCONS,
        &openError
    );
    if (opened == nullptr) {
        const auto message = QString::fromLocal8Bit(
            zip_error_strerror(&openError)
        );
        zip_source_free(zipSource);
        zip_error_fini(&openError);
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.invalid-zip"),
            message
        );
    }
    zip_error_fini(&openError);
    ZipArchive archive(opened);

    const auto entryCount = zip_get_num_entries(archive.get(), 0);
    if (entryCount != rawEntries.size()) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.zip-entry-mismatch"),
            QStringLiteral("The ZIP parser and raw directory disagree about entry count.")
        );
    }

    QVector<ComponentPackageBundleFile> files;
    QMap<QString, qsizetype> fileIndexes;
    quint64 expandedSize = 0;
    for (zip_uint64_t index = 0;
         index < static_cast<zip_uint64_t>(entryCount);
         ++index) {
        const auto &raw = rawEntries.at(static_cast<qsizetype>(index));
        const auto *name = zip_get_name(archive.get(), index, ZIP_FL_ENC_RAW);
        if (name == nullptr || QByteArray(name) != raw.name) {
            return singleError(
                QStringLiteral("$"),
                QStringLiteral("package.zip-entry-mismatch"),
                QStringLiteral("The ZIP parser changed an entry filename.")
            );
        }

        zip_stat_t stat;
        zip_stat_init(&stat);
        if (zip_stat_index(archive.get(), index, ZIP_FL_ENC_RAW, &stat) != 0
            || (stat.valid
                & (ZIP_STAT_SIZE | ZIP_STAT_COMP_SIZE | ZIP_STAT_COMP_METHOD
                   | ZIP_STAT_ENCRYPTION_METHOD))
                != (ZIP_STAT_SIZE | ZIP_STAT_COMP_SIZE | ZIP_STAT_COMP_METHOD
                    | ZIP_STAT_ENCRYPTION_METHOD)
            || stat.comp_method != raw.compressionMethod
            || (stat.comp_method != ZIP_CM_STORE
                && stat.comp_method != ZIP_CM_DEFLATE)
            || stat.encryption_method != ZIP_EM_NONE) {
            return singleError(
                QStringLiteral("$"),
                QStringLiteral("package.unsupported-zip-entry"),
                QStringLiteral("A ZIP entry uses unsupported compression or encryption.")
            );
        }
        if (raw.directory) {
            if (stat.size != 0) {
                return singleError(
                    QStringLiteral("$"),
                    QStringLiteral("package.invalid-directory"),
                    QStringLiteral("ZIP directory entries must be empty.")
                );
            }
            continue;
        }

        QString path;
        if (!strictUtf8Path(raw.name, path)) {
            return singleError(
                QStringLiteral("$"),
                QStringLiteral("package.invalid-path"),
                QStringLiteral("A ZIP filename is invalid.")
            );
        }
        if (stat.size > static_cast<quint64>(maximumComponentFileBytes)
            || expandedSize
                > static_cast<quint64>(maximumComponentExpandedBytes)
                    - stat.size
            || (path == QStringLiteral("manifest.json")
                && stat.size > static_cast<quint64>(maximumManifestBytes))
            || (path == QStringLiteral("integrity.json")
                && stat.size > static_cast<quint64>(maximumIntegrityBytes))
            || (path == QStringLiteral("settings.schema.json")
                && stat.size
                    > static_cast<quint64>(maximumSettingsSchemaBytes))
            || (path == QStringLiteral("icon.png")
                && stat.size > static_cast<quint64>(maximumIconBytes))) {
            return singleError(
                path,
                QStringLiteral("package.expanded-size"),
                QStringLiteral("A ZIP entry exceeds its metadata, file, or expanded-size limit.")
            );
        }

        QByteArray contents;
        QString readError;
        if (!readZipFile(
                archive.get(),
                index,
                stat.size,
                contents,
                readError
            )) {
            return singleError(
                path,
                QStringLiteral("package.zip-read"),
                readError
            );
        }
        expandedSize += static_cast<quint64>(contents.size());
        fileIndexes.insert(path, files.size());
        files.append({path, std::move(contents)});
    }

    const auto manifestIndex = fileIndexes.constFind(
        QStringLiteral("manifest.json")
    );
    const auto integrityIndex = fileIndexes.constFind(
        QStringLiteral("integrity.json")
    );
    if (manifestIndex == fileIndexes.cend()
        || integrityIndex == fileIndexes.cend()) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.required-file-missing"),
            QStringLiteral("manifest.json and integrity.json are required.")
        );
    }

    const auto &manifestBytes = files.at(*manifestIndex).contents;
    auto manifest = parseComponentManifest(
        QByteArrayView(manifestBytes),
        ComponentOrigin::User
    );
    if (!manifest) {
        return {std::nullopt, std::move(manifest.errors)};
    }
    auto normalizedManifest = parseStrictJsonObject(
        QByteArrayView(manifestBytes),
        {.maximumBytes = maximumManifestBytes, .maximumDepth = 32}
    );
    if (!normalizedManifest) {
        return {std::nullopt, std::move(normalizedManifest.errors)};
    }

    const auto &integrityBytes = files.at(*integrityIndex).contents;
    auto integrity = parsePackageIntegrity(QByteArrayView(integrityBytes));
    if (!integrity) {
        return {std::nullopt, std::move(integrity.errors)};
    }
    QSet<QString> actualCoveredFiles;
    for (const auto &file : files) {
        if (file.path == QStringLiteral("integrity.json")) {
            continue;
        }
        actualCoveredFiles.insert(file.path);
        const auto expected = integrity.value->files.constFind(file.path);
        const auto actual = QString::fromLatin1(
            QCryptographicHash::hash(
                file.contents,
                QCryptographicHash::Sha256
            ).toHex()
        );
        if (expected == integrity.value->files.cend() || *expected != actual) {
            return singleError(
                file.path,
                QStringLiteral("package.integrity-mismatch"),
                QStringLiteral("A package file is unlisted or differs from its declared digest.")
            );
        }
    }
    const QSet<QString> declaredCoveredFiles(
        integrity.value->files.keyBegin(),
        integrity.value->files.keyEnd()
    );
    if (actualCoveredFiles != declaredCoveredFiles) {
        return singleError(
            QStringLiteral("integrity.json"),
            QStringLiteral("package.integrity-file-set"),
            QStringLiteral("The integrity entries do not exactly match package files.")
        );
    }

    const auto schemaIndex = fileIndexes.constFind(
        QStringLiteral("settings.schema.json")
    );
    const auto declaresSchema = manifest.value->settingsSchema.has_value();
    if (declaresSchema != (schemaIndex != fileIndexes.cend())) {
        return singleError(
            QStringLiteral("settings.schema.json"),
            QStringLiteral("package.settings-schema-presence"),
            QStringLiteral("The settings schema declaration and file must agree.")
        );
    }
    std::optional<QJsonObject> normalizedSchema;
    std::optional<SettingsSchema> parsedSettingsSchema;
    if (schemaIndex != fileIndexes.cend()) {
        const auto &schemaBytes = files.at(*schemaIndex).contents;
        auto schema = parseSettingsSchema(QByteArrayView(schemaBytes));
        if (!schema) {
            return {std::nullopt, std::move(schema.errors)};
        }
        auto schemaObject = parseStrictJsonObject(
            QByteArrayView(schemaBytes),
            {.maximumBytes = maximumSettingsSchemaBytes, .maximumDepth = 32}
        );
        if (!schemaObject) {
            return {std::nullopt, std::move(schemaObject.errors)};
        }
        parsedSettingsSchema = std::move(*schema.value);
        normalizedSchema = std::move(*schemaObject.value);
    }

    if (!manifest.value->runtime.entrypoint.isEmpty()
        && !fileIndexes.contains(manifest.value->runtime.entrypoint)) {
        return singleError(
            QStringLiteral("$.runtime.entrypoint"),
            QStringLiteral("package.entrypoint-missing"),
            QStringLiteral("The declared entry point is not a regular package file.")
        );
    }
    QByteArray declarativeRuntime;
    if (manifest.value->runtime.kind == RuntimeKind::DeclarativeV1) {
        const auto entrypointIndex = fileIndexes.constFind(
            manifest.value->runtime.entrypoint
        );
        Q_ASSERT(entrypointIndex != fileIndexes.cend());
        const auto document = parseDeclarativeDocument(
            QByteArrayView(files.at(*entrypointIndex).contents),
            parsedSettingsSchema.has_value()
                ? &*parsedSettingsSchema
                : nullptr
        );
        if (!document) {
            return {std::nullopt, std::move(document.errors)};
        }
        declarativeRuntime = serializeDeclarativeDocument(*document.value);
    }
    const auto iconIndex = fileIndexes.constFind(QStringLiteral("icon.png"));
    if (iconIndex != fileIndexes.cend()) {
        QString iconError;
        if (!validateIcon(files.at(*iconIndex).contents, iconError)) {
            return singleError(
                QStringLiteral("icon.png"),
                QStringLiteral("package.invalid-icon"),
                iconError
            );
        }
    }

    std::ranges::sort(files, {}, &ComponentPackageBundleFile::path);
    PackageInspectionReport report{
        .inspectionToken = inspectionToken,
        .archiveSha256 = archiveSha256,
        .packageDigest = deriveComponentPackageDigest(files),
        .archiveSize = static_cast<quint64>(archiveBytes.size()),
        .expandedSize = expandedSize,
        .manifest = std::move(*manifest.value),
        .normalizedManifest = std::move(*normalizedManifest.value),
        .normalizedSettingsSchema = std::move(normalizedSchema),
        .declarativeRuntime = std::move(declarativeRuntime),
    };
    report.files.reserve(files.size());
    for (const auto &file : files) {
        report.files.append({
            .path = file.path,
            .size = static_cast<quint64>(file.contents.size()),
            .sha256 = QString::fromLatin1(
                QCryptographicHash::hash(
                    file.contents,
                    QCryptographicHash::Sha256
                ).toHex()
            ),
        });
    }

    if (report.packageDigest.isEmpty()) {
        return singleError(
            QStringLiteral("$"),
            QStringLiteral("package.digest-failed"),
            QStringLiteral("Cannot derive the normalized package digest.")
        );
    }
    const auto reparsed = parsePackageInspectionReport(
        serializePackageInspectionReport(report)
    );
    if (!reparsed) {
        return {std::nullopt, std::move(reparsed.errors)};
    }
    if (materializedBundle != nullptr) {
        QString bundleError;
        if (!writeComponentPackageBundle(
                *materializedBundle,
                files,
                bundleError
            )) {
            return singleError(
                QStringLiteral("$"),
                QStringLiteral("package.bundle-write"),
                bundleError
            );
        }
    }
    return {std::move(report), {}};
}

} // namespace HyprShelld::Components
