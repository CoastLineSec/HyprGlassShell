#include "user_package_store.h"

#include "component/component_package_bundle.h"
#include "component/component_configuration.h"
#include "component/declarative_document.h"
#include "component/package_integrity.h"
#include "component/settings_schema.h"
#include "component/strict_json.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld::Components {
namespace {

constexpr qsizetype maximumReceiptBytes = 1024 * 1024;
constexpr auto packageStoreLeaseName = ".package-store.lock";

UserPackageStoreResult failure(
    QString code,
    QString message,
    QString componentId = {}
)
{
    return {
        .success = false,
        .errorCode = std::move(code),
        .errorMessage = std::move(message),
        .componentId = std::move(componentId),
    };
}

bool ensurePrivateDirectory(const QString &path, QString &error)
{
    if (!QDir().mkpath(path)) {
        error = QStringLiteral("Cannot create %1").arg(path);
        return false;
    }
    const QFileInfo info(path);
    if (!info.isDir() || info.isSymLink()) {
        error = QStringLiteral("%1 is not a safe directory").arg(path);
        return false;
    }
    if (!QFile::setPermissions(
            path,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner
                | QFileDevice::ExeOwner
        )) {
        error = QStringLiteral("Cannot protect %1").arg(path);
        return false;
    }
    return true;
}

QString describeValidation(const ValidationErrors &errors)
{
    QStringList details;
    for (const auto &error : errors) {
        details.append(QStringLiteral("%1: %2").arg(error.code, error.message));
        if (details.size() == 4) {
            break;
        }
    }
    return details.join(QStringLiteral("; "));
}

std::optional<PackageInspectionReport> readReceipt(
    const QString &path,
    QString &error
)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() <= 0
        || info.size() > maximumReceiptBytes) {
        error = QStringLiteral("The package receipt is missing, unsafe, or oversized: %1")
                    .arg(path);
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot read package receipt %1: %2")
                    .arg(path, file.errorString());
        return std::nullopt;
    }
    const auto bytes = file.read(maximumReceiptBytes + 1);
    if (bytes.size() > maximumReceiptBytes
        || file.error() != QFileDevice::NoError) {
        error = QStringLiteral("Cannot read bounded package receipt %1")
                    .arg(path);
        return std::nullopt;
    }
    auto parsed = parsePackageInspectionReport(QByteArrayView(bytes));
    if (!parsed) {
        error = QStringLiteral("Invalid package receipt %1: %2")
                    .arg(path, describeValidation(parsed.errors));
        return std::nullopt;
    }
    if (parsed.value->manifest.origin != ComponentOrigin::User
        || isReservedBuiltinId(parsed.value->manifest.id)) {
        error = QStringLiteral("A package receipt claims a protected identity: %1")
                    .arg(path);
        return std::nullopt;
    }
    return std::move(*parsed.value);
}

void addDigestLength(QCryptographicHash &hash, const quint64 size)
{
    std::array<uchar, sizeof(quint64)> encoded{};
    qToBigEndian<quint64>(size, encoded.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(encoded.data()),
        encoded.size()
    ));
}

bool hashFile(
    const QString &path,
    const QString &relativePath,
    quint64 expectedSize,
    QString &digest,
    QCryptographicHash &packageHash
)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() < 0
        || static_cast<quint64>(info.size()) != expectedSize) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto encodedPath = relativePath.toUtf8();
    addDigestLength(packageHash, static_cast<quint64>(encodedPath.size()));
    packageHash.addData(encodedPath);
    addDigestLength(packageHash, expectedSize);
    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    while (true) {
        const auto read = file.read(buffer.data(), buffer.size());
        if (read == 0) {
            break;
        }
        if (read < 0) {
            return false;
        }
        const QByteArrayView chunk(buffer.constData(), read);
        hash.addData(chunk);
        packageHash.addData(chunk);
    }
    digest = QString::fromLatin1(hash.result().toHex());
    return true;
}

bool readBoundedFile(
    const QString &path,
    const qint64 maximumSize,
    QByteArray &bytes
)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() < 0
        || info.size() > maximumSize) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    bytes = file.read(maximumSize + 1);
    return bytes.size() <= maximumSize
        && file.error() == QFileDevice::NoError;
}

bool validateInstalledTree(
    const QString &rootPath,
    const PackageInspectionReport &report,
    QString &error
)
{
    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.isDir() || rootInfo.isSymLink()) {
        error = QStringLiteral("The installed package directory is missing or unsafe");
        return false;
    }
    const auto canonicalRoot = rootInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty()) {
        error = QStringLiteral("The installed package directory cannot be resolved");
        return false;
    }

    QStringList actualFiles;
    QDirIterator iterator(
        rootPath,
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories
    );
    while (iterator.hasNext()) {
        iterator.next();
        const auto info = iterator.fileInfo();
        if (info.isSymLink() || (!info.isDir() && !info.isFile())) {
            error = QStringLiteral("The installed package contains an unsafe entry");
            return false;
        }
        const auto relative = QDir(rootPath).relativeFilePath(info.filePath());
        if (info.isDir()) {
            if (!(relative == QStringLiteral("payload")
                  || relative == QStringLiteral("LICENSES")
                  || relative.startsWith(QStringLiteral("payload/"))
                  || relative.startsWith(QStringLiteral("LICENSES/")))) {
                error = QStringLiteral("The installed package contains an unexpected directory");
                return false;
            }
            continue;
        }
        if (!isCanonicalPackageFilePath(relative)
            || !isAllowedComponentPackageFilePath(relative)) {
            error = QStringLiteral("The installed package contains an invalid path");
            return false;
        }
        actualFiles.append(relative);
    }
    actualFiles.sort();
    QStringList expectedFiles;
    for (const auto &file : report.files) {
        expectedFiles.append(file.path);
    }
    if (actualFiles != expectedFiles) {
        error = QStringLiteral("The installed package file set differs from its receipt");
        return false;
    }
    QCryptographicHash packageHash(QCryptographicHash::Sha256);
    QMap<QString, QString> actualDigests;
    for (const auto &file : report.files) {
        QString digest;
        if (!hashFile(
                QDir(rootPath).filePath(file.path),
                file.path,
                file.size,
                digest,
                packageHash
            )
            || digest != file.sha256) {
            error = QStringLiteral("Installed file %1 differs from its receipt")
                        .arg(file.path);
            return false;
        }
        actualDigests.insert(file.path, digest);
    }
    if (QString::fromLatin1(packageHash.result().toHex())
        != report.packageDigest) {
        error = QStringLiteral(
            "The installed package digest differs from its receipt"
        );
        return false;
    }

    QByteArray manifestBytes;
    const auto manifestPath = QDir(rootPath).filePath(
        QStringLiteral("manifest.json")
    );
    if (!readBoundedFile(manifestPath, 128 * 1024, manifestBytes)) {
        error = QStringLiteral("The installed manifest cannot be read safely");
        return false;
    }
    const auto manifest = parseComponentManifest(
        QByteArrayView(manifestBytes),
        ComponentOrigin::User
    );
    const auto manifestObject = parseStrictJsonObject(
        QByteArrayView(manifestBytes),
        {.maximumBytes = 128 * 1024, .maximumDepth = 32}
    );
    if (!manifest || !manifestObject || *manifest.value != report.manifest
        || *manifestObject.value != report.normalizedManifest) {
        error = QStringLiteral(
            "The installed manifest differs from its manager receipt"
        );
        return false;
    }

    std::optional<SettingsSchema> parsedSettingsSchema;
    if (report.normalizedSettingsSchema.has_value()) {
        QByteArray schemaBytes;
        if (!readBoundedFile(
                QDir(rootPath).filePath(
                    QStringLiteral("settings.schema.json")
                ),
                256 * 1024,
                schemaBytes
            )) {
            error = QStringLiteral(
                "The installed settings schema cannot be read safely"
            );
            return false;
        }
        auto schema = parseSettingsSchema(QByteArrayView(schemaBytes));
        const auto schemaObject = parseStrictJsonObject(
            QByteArrayView(schemaBytes),
            {.maximumBytes = 256 * 1024, .maximumDepth = 32}
        );
        if (!schema || !schemaObject
            || *schemaObject.value != *report.normalizedSettingsSchema) {
            error = QStringLiteral(
                "The installed settings schema differs from its manager receipt"
            );
            return false;
        }
        parsedSettingsSchema = std::move(*schema.value);
    }

    if (manifest.value->runtime.kind == RuntimeKind::DeclarativeV1) {
        QByteArray documentBytes;
        if (!readBoundedFile(
                QDir(rootPath).filePath(
                    manifest.value->runtime.entrypoint
                ),
                maximumDeclarativeDocumentBytes,
                documentBytes
            )) {
            error = QStringLiteral(
                "The installed declarative document cannot be read safely"
            );
            return false;
        }
        const auto document = parseDeclarativeDocument(
            documentBytes,
            parsedSettingsSchema.has_value()
                ? &*parsedSettingsSchema
                : nullptr
        );
        if (!document
            || serializeDeclarativeDocument(*document.value)
                != report.declarativeRuntime) {
            error = QStringLiteral(
                "The installed declarative document differs from its manager receipt"
            );
            return false;
        }
    }

    QByteArray integrityBytes;
    if (!readBoundedFile(
            QDir(rootPath).filePath(QStringLiteral("integrity.json")),
            512 * 1024,
            integrityBytes
        )) {
        error = QStringLiteral(
            "The installed integrity metadata cannot be read safely"
        );
        return false;
    }
    const auto integrity = parsePackageIntegrity(
        QByteArrayView(integrityBytes)
    );
    if (!integrity) {
        error = QStringLiteral(
            "The installed integrity metadata is invalid"
        );
        return false;
    }
    actualDigests.remove(QStringLiteral("integrity.json"));
    if (integrity.value->files != actualDigests) {
        error = QStringLiteral(
            "The installed integrity file set differs from its content"
        );
        return false;
    }
    return true;
}

bool writeReceipt(
    const QString &path,
    const PackageInspectionReport &report,
    QString &error
)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("Cannot create package receipt: %1")
                    .arg(file.errorString());
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const auto bytes = serializePackageInspectionReport(report);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        error = QStringLiteral("Cannot commit package receipt: %1")
                    .arg(file.errorString());
        return false;
    }
    return true;
}

QString randomName()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool pathEntryExists(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

bool renamePathEntry(
    const QString &source,
    const QString &destination
)
{
    const QFileInfo info(source);
    if (info.isSymLink() || info.isFile()) {
        return QFile::rename(source, destination);
    }
    if (info.isDir()) {
        return QDir().rename(source, destination);
    }
    return false;
}

bool removePathEntry(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        return true;
    }
    if (info.isSymLink() || !info.isDir()) {
        return QFile::remove(path);
    }
    const auto entries = QDir(path).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
            | QDir::System,
        QDir::Name
    );
    for (const auto &entry : entries) {
        if (!removePathEntry(entry.filePath())) {
            return false;
        }
    }
    return QDir().rmdir(path);
}

struct SafeComponentPaths final {
    QString componentRoot;
    QString versionsRoot;
    QString canonicalDataRoot;
    QString canonicalComponentRoot;
    QString canonicalVersionsRoot;
};

bool isCanonicalChildOf(
    const QString &canonicalParent,
    const QString &canonicalChild
)
{
    return !canonicalParent.isEmpty() && !canonicalChild.isEmpty()
        && canonicalChild.startsWith(
            canonicalParent + QDir::separator()
        );
}

bool resolveSafeChildDirectory(
    const QString &parentPath,
    const QString &canonicalParent,
    const QString &name,
    const bool create,
    QString &path,
    QString &canonicalPath,
    QString &error
)
{
    path = QDir(parentPath).filePath(name);
    QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        if (!create || !QDir(parentPath).mkdir(name)) {
            error = QStringLiteral("The package directory is missing: %1")
                        .arg(path);
            return false;
        }
        info.refresh();
    }
    if (!info.isDir() || info.isSymLink()) {
        error = QStringLiteral("The package directory is unsafe: %1")
                    .arg(path);
        return false;
    }
    canonicalPath = info.canonicalFilePath();
    if (info.dir().canonicalPath() != canonicalParent
        || !isCanonicalChildOf(canonicalParent, canonicalPath)) {
        error = QStringLiteral("The package directory escapes its store root: %1")
                    .arg(path);
        return false;
    }
    if (create
        && !QFile::setPermissions(
            path,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner
                | QFileDevice::ExeOwner
        )) {
        error = QStringLiteral("Cannot protect package directory %1")
                    .arg(path);
        return false;
    }
    return true;
}

bool resolveSafeComponentPaths(
    const QString &dataRoot,
    const QString &componentId,
    const bool create,
    SafeComponentPaths &paths,
    QString &error
)
{
    const QFileInfo dataInfo(dataRoot);
    if (!dataInfo.isDir() || dataInfo.isSymLink()) {
        error = QStringLiteral("The user component data root is missing or unsafe");
        return false;
    }
    paths.canonicalDataRoot = dataInfo.canonicalFilePath();
    if (paths.canonicalDataRoot.isEmpty()) {
        error = QStringLiteral("The user component data root cannot be resolved");
        return false;
    }
    if (!resolveSafeChildDirectory(
            dataRoot,
            paths.canonicalDataRoot,
            componentId,
            create,
            paths.componentRoot,
            paths.canonicalComponentRoot,
            error
        )) {
        return false;
    }
    if (!resolveSafeChildDirectory(
            paths.componentRoot,
            paths.canonicalComponentRoot,
            QStringLiteral("versions"),
            create,
            paths.versionsRoot,
            paths.canonicalVersionsRoot,
            error
        )) {
        return false;
    }
    if (!isCanonicalChildOf(
            paths.canonicalDataRoot,
            paths.canonicalVersionsRoot
        )) {
        error = QStringLiteral("The package version root escapes its data root");
        return false;
    }
    return true;
}

bool resolveSafeVersionPath(
    const SafeComponentPaths &paths,
    const QString &version,
    const bool allowMissing,
    QString &versionPath,
    QString &error
)
{
    versionPath = QDir(paths.versionsRoot).filePath(version);
    const QFileInfo info(versionPath);
    if (!info.exists() && !info.isSymLink()) {
        if (allowMissing) {
            return true;
        }
        error = QStringLiteral("The installed package version is missing: %1")
                    .arg(versionPath);
        return false;
    }
    if (!info.isDir() || info.isSymLink()) {
        error = QStringLiteral("The installed package version is unsafe: %1")
                    .arg(versionPath);
        return false;
    }
    const auto canonicalVersion = info.canonicalFilePath();
    if (info.dir().canonicalPath() != paths.canonicalVersionsRoot
        || !isCanonicalChildOf(
            paths.canonicalDataRoot,
            canonicalVersion
        )) {
        error = QStringLiteral("The installed package version escapes its store root: %1")
                    .arg(versionPath);
        return false;
    }
    return true;
}

bool pruneInactiveVersions(
    const QString &dataRoot,
    const QString &componentId,
    const QString &currentVersion,
    QString &error
)
{
    SafeComponentPaths paths;
    if (!resolveSafeComponentPaths(
            dataRoot,
            componentId,
            false,
            paths,
            error
        )) {
        return false;
    }
    QString currentPath;
    if (!resolveSafeVersionPath(
            paths,
            currentVersion,
            false,
            currentPath,
            error
        )) {
        return false;
    }
    const auto entries = QDir(paths.versionsRoot).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
            | QDir::System,
        QDir::Name
    );
    for (const auto &entry : entries) {
        if (entry.fileName() == currentVersion) {
            continue;
        }
        if (!removePathEntry(entry.filePath())) {
            error = QStringLiteral("Cannot prune inactive version entry %1")
                        .arg(entry.filePath());
            return false;
        }
    }
    return true;
}

bool removeDirectoryContents(const QString &path, QString &error)
{
    const QFileInfo rootInfo(path);
    if (!rootInfo.exists() && !rootInfo.isSymLink()) {
        return true;
    }
    if (!rootInfo.isDir() || rootInfo.isSymLink()) {
        error = QStringLiteral("The recovery directory is unsafe: %1")
                    .arg(path);
        return false;
    }
    const auto entries = QDir(path).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
            | QDir::System,
        QDir::Name
    );
    for (const auto &entry : entries) {
        if (!removePathEntry(entry.filePath())) {
            error = QStringLiteral("Cannot remove stale transaction entry %1")
                        .arg(entry.filePath());
            return false;
        }
    }
    return true;
}

} // namespace

UserPackageStore::UserPackageStore(QString dataRoot, QString stateRoot)
    : dataRoot_(std::move(dataRoot))
    , stateRoot_(std::move(stateRoot))
{
    QString error;
    if (!ensurePrivateDirectory(stateRoot_, error)) {
        leaseError_ = QStringLiteral(
            "The user package store lease is unavailable: %1"
        ).arg(error);
        return;
    }
    const auto leasePath = QDir(stateRoot_).filePath(
        QString::fromLatin1(packageStoreLeaseName)
    );
    const auto encodedLeasePath = QFile::encodeName(leasePath);
    const auto fd = ::open(
        encodedLeasePath.constData(),
        O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
        S_IRUSR | S_IWUSR
    );
    if (fd < 0) {
        leaseError_ = QStringLiteral(
            "Cannot open the user package store lease: %1"
        ).arg(QString::fromLocal8Bit(std::strerror(errno)));
        return;
    }

    struct stat status {};
    if (::fstat(fd, &status) != 0 || !S_ISREG(status.st_mode)
        || status.st_uid != ::geteuid() || status.st_nlink != 1) {
        leaseError_ = QStringLiteral(
            "The user package store lease file is unsafe"
        );
        ::close(fd);
        return;
    }
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const auto lockError = errno;
        leaseError_ = lockError == EWOULDBLOCK
            ? QStringLiteral(
                "The user package store is already owned by another manager"
            )
            : QStringLiteral("Cannot lock the user package store lease: %1")
                  .arg(QString::fromLocal8Bit(std::strerror(lockError)));
        ::close(fd);
        return;
    }
    if (::fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
        const auto permissionError = errno;
        leaseError_ = QStringLiteral(
            "Cannot protect the user package store lease: %1"
        ).arg(QString::fromLocal8Bit(std::strerror(permissionError)));
        ::close(fd);
        return;
    }
    leaseFd_ = fd;
}

UserPackageStore::~UserPackageStore()
{
    if (leaseFd_ >= 0) {
        ::close(leaseFd_);
    }
}

QString UserPackageStore::receiptPath(const QString &componentId) const
{
    return QDir(stateRoot_ + QStringLiteral("/receipts"))
        .filePath(componentId + QStringLiteral(".json"));
}

UserPackageLoadResult UserPackageStore::load() const
{
    UserPackageLoadResult result;
    if (leaseFd_ < 0) {
        result.error = leaseError_.isEmpty()
            ? QStringLiteral("The user package store lease is unavailable")
            : leaseError_;
        return result;
    }
    const auto receiptsPath = stateRoot_ + QStringLiteral("/receipts");
    const QFileInfo receiptsInfo(receiptsPath);
    if (!receiptsInfo.exists()) {
        return result;
    }
    if (!receiptsInfo.isDir() || receiptsInfo.isSymLink()) {
        result.error = QStringLiteral("The user component receipt root is unsafe");
        return result;
    }
    const auto receipts = QDir(receiptsPath).entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name
    );
    if (receipts.size() > 511) {
        result.error = QStringLiteral("The user component catalog exceeds 511 packages");
        return result;
    }
    result.receiptCount = receipts.size();

    QSet<QString> ids;
    QVector<PackageInspectionReport> reports;
    reports.reserve(receipts.size());
    for (const auto &info : receipts) {
        if (info.isSymLink() || info.suffix() != QStringLiteral("json")) {
            result.warnings.append(
                QStringLiteral("Ignored unexpected receipt entry %1")
                    .arg(info.fileName())
            );
            continue;
        }
        QString error;
        auto report = readReceipt(info.filePath(), error);
        if (!report.has_value()) {
            result.warnings.append(error);
            continue;
        }
        const auto &manifest = report->manifest;
        if (info.completeBaseName() != manifest.id || ids.contains(manifest.id)) {
            result.warnings.append(
                QStringLiteral("Ignored a receipt with a mismatched or duplicate ID: %1")
                    .arg(info.fileName())
            );
            continue;
        }
        ids.insert(manifest.id);
        reports.append(std::move(*report));
    }

    for (const auto &report : reports) {
        const auto &manifest = report.manifest;
        QString error;
        SafeComponentPaths paths;
        QString installedVersionPath;
        if (!resolveSafeComponentPaths(
                dataRoot_,
                manifest.id,
                false,
                paths,
                error
            )
            || !resolveSafeVersionPath(
                paths,
                manifest.version,
                false,
                installedVersionPath,
                error
            )) {
            result.warnings.append(
                QStringLiteral("Ignored invalid installed package %1: %2")
                    .arg(manifest.id, error)
            );
            continue;
        }
        if (!validateInstalledTree(
                installedVersionPath,
                report,
                error
            )) {
            result.warnings.append(
                QStringLiteral("Ignored invalid installed package %1: %2")
                    .arg(manifest.id, error)
            );
            continue;
        }
        if (result.totalExpandedBytes
                > maximumUserPackageCatalogBytes - report.expandedSize) {
            result.warnings.append(QStringLiteral(
                "Ignored %1 because the installed user component catalog is limited to 512 MiB"
            ).arg(manifest.id));
            continue;
        }
        result.totalExpandedBytes += report.expandedSize;
        QByteArray settingsSchema;
        if (report.normalizedSettingsSchema.has_value()) {
            settingsSchema = QJsonDocument(*report.normalizedSettingsSchema)
                                 .toJson(QJsonDocument::Compact);
        }
        result.entries.append({
            .manifest = manifest,
            .settingsSchema = std::move(settingsSchema),
            .declarativeRuntime = report.declarativeRuntime,
            .packageDigest = report.packageDigest,
        });
    }
    return result;
}

bool UserPackageStore::recover(
    QString &error,
    QStringList *warnings
)
{
    error.clear();
    if (leaseFd_ < 0) {
        error = leaseError_.isEmpty()
            ? QStringLiteral("The user package store lease is unavailable")
            : leaseError_;
        return false;
    }
    for (const auto &root : {dataRoot_, stateRoot_}) {
        if (!ensurePrivateDirectory(root, error)) {
            return false;
        }
    }
    for (const auto &transactionRoot : {
             dataRoot_ + QStringLiteral("/.staging"),
             dataRoot_ + QStringLiteral("/.trash"),
             stateRoot_ + QStringLiteral("/.trash"),
         }) {
        if (!removeDirectoryContents(transactionRoot, error)) {
            return false;
        }
    }

    QSet<QString> receiptIds;
    QMap<QString, QString> committedVersions;
    const auto receiptsRoot = stateRoot_ + QStringLiteral("/receipts");
    const QFileInfo receiptsRootInfo(receiptsRoot);
    if (receiptsRootInfo.exists() || receiptsRootInfo.isSymLink()) {
        if (!receiptsRootInfo.isDir() || receiptsRootInfo.isSymLink()) {
            error = QStringLiteral("The user component receipt root is unsafe");
            return false;
        }
        const auto receipts = QDir(receiptsRoot).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
                | QDir::System,
            QDir::Name
        );
        for (const auto &receiptInfo : receipts) {
            if (receiptInfo.suffix() != QStringLiteral("json")) {
                continue;
            }
            const auto componentId = receiptInfo.completeBaseName();
            if (!isValidComponentId(componentId)
                || isReservedBuiltinId(componentId)) {
                continue;
            }
            receiptIds.insert(componentId);
            QString receiptError;
            auto report = readReceipt(receiptInfo.filePath(), receiptError);
            if (report.has_value()
                && report->manifest.id == componentId) {
                committedVersions.insert(
                    componentId,
                    report->manifest.version
                );
            }
        }
    }

    const auto componentRoots = QDir(dataRoot_).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::Name
    );
    for (const auto &componentRoot : componentRoots) {
        const auto componentId = componentRoot.fileName();
        if (!isValidComponentId(componentId)
            || isReservedBuiltinId(componentId)) {
            continue;
        }
        if (componentRoot.isSymLink()) {
            if (warnings != nullptr) {
                warnings->append(QStringLiteral(
                    "Preserved unsafe package storage for %1: the component root is a symbolic link"
                ).arg(componentId));
            }
            continue;
        }
        const auto versionsRoot = QDir(componentRoot.filePath()).filePath(
            QStringLiteral("versions")
        );
        const QFileInfo versionsRootInfo(versionsRoot);
        if (!versionsRootInfo.exists() && !versionsRootInfo.isSymLink()) {
            continue;
        }
        SafeComponentPaths paths;
        QString componentError;
        if (!resolveSafeComponentPaths(
                dataRoot_,
                componentId,
                false,
                paths,
                componentError
            )) {
            if (warnings != nullptr) {
                warnings->append(QStringLiteral(
                    "Preserved unsafe package storage for %1: %2"
                ).arg(componentId, componentError));
            }
            continue;
        }
        if (!receiptIds.contains(componentId)) {
            if (!removePathEntry(paths.versionsRoot)
                && warnings != nullptr) {
                warnings->append(QStringLiteral(
                    "Could not remove orphaned package versions for %1"
                ).arg(componentId));
            }
            continue;
        }
        const auto committed = committedVersions.constFind(componentId);
        QString pruneError;
        if (committed != committedVersions.cend()
            && !pruneInactiveVersions(
                dataRoot_,
                componentId,
                *committed,
                pruneError
            )
            && warnings != nullptr) {
            warnings->append(QStringLiteral(
                "Could not prune inactive package versions for %1: %2"
            ).arg(componentId, pruneError));
        }
    }
    return true;
}

UserPackageStoreResult UserPackageStore::install(
    const PackageInspectionReport &report,
    const QString &materializedBundlePath
)
{
    const auto &manifest = report.manifest;
    if (leaseFd_ < 0) {
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            leaseError_.isEmpty()
                ? QStringLiteral("The user package store lease is unavailable")
                : leaseError_,
            manifest.id
        );
    }
    if (manifest.origin != ComponentOrigin::User
        || isReservedBuiltinId(manifest.id)
        || !isFullSha256Digest(report.packageDigest)) {
        return failure(
            QStringLiteral("ReservedComponentId"),
            QStringLiteral("A third-party package cannot use a protected identity"),
            manifest.id
        );
    }

    const auto currentCatalog = load();
    if (!currentCatalog.ok()) {
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            currentCatalog.error,
            manifest.id
        );
    }
    const auto alreadyInstalled = std::ranges::any_of(
        currentCatalog.entries,
        [&manifest](const CatalogEntry &entry) {
            return entry.manifest.id == manifest.id;
        }
    );
    const auto existingReceiptPath = receiptPath(manifest.id);
    const auto hasExistingReceiptEntry = pathEntryExists(existingReceiptPath);
    if (!alreadyInstalled && !hasExistingReceiptEntry
        && currentCatalog.receiptCount >= 511) {
        return failure(
            QStringLiteral("PackageCatalogFull"),
            QStringLiteral(
                "The per-user component catalog already contains 511 packages"
            ),
            manifest.id
        );
    }

    QString error;
    const auto receiptsRoot = stateRoot_ + QStringLiteral("/receipts");
    const auto stateTrash = stateRoot_ + QStringLiteral("/.trash");
    const auto dataStaging = dataRoot_ + QStringLiteral("/.staging");
    const auto dataTrash = dataRoot_ + QStringLiteral("/.trash");
    for (const auto &directory : {
             dataRoot_, stateRoot_, receiptsRoot, stateTrash,
             dataStaging, dataTrash,
         }) {
        if (!ensurePrivateDirectory(directory, error)) {
            return failure(
                QStringLiteral("PackageTransactionFailed"),
                error,
                manifest.id
            );
        }
    }

    std::optional<PackageInspectionReport> existing;
    if (hasExistingReceiptEntry) {
        existing = readReceipt(existingReceiptPath, error);
        if (!existing.has_value()) {
            const QFileInfo receiptInfo(existingReceiptPath);
            if (!receiptInfo.isFile() || receiptInfo.isSymLink()) {
                return failure(
                    QStringLiteral("PackageTransactionFailed"),
                    error,
                    manifest.id
                );
            }
        } else if (existing->manifest.id != manifest.id) {
            existing.reset();
        }
        if (existing.has_value()
            && existing->manifest.version == manifest.version
            && existing->packageDigest != report.packageDigest) {
            return failure(
                QStringLiteral("SameVersionDifferentDigest"),
                QStringLiteral("The installed version has different package bytes"),
                manifest.id
            );
        }
        if (existing.has_value()
            && existing->packageDigest == report.packageDigest
            && alreadyInstalled) {
            QString pruneError;
            pruneInactiveVersions(
                dataRoot_,
                manifest.id,
                manifest.version,
                pruneError
            );
            return {
                .success = true,
                .componentId = manifest.id,
                .packageDigest = report.packageDigest,
                .catalogEntries = currentCatalog.entries,
            };
        }
    }

    const auto existingExpandedBytes = alreadyInstalled
            && existing.has_value()
        ? existing->expandedSize : 0;
    if (currentCatalog.totalExpandedBytes < existingExpandedBytes
        || currentCatalog.totalExpandedBytes - existingExpandedBytes
                > maximumUserPackageCatalogBytes - report.expandedSize) {
        return failure(
            QStringLiteral("PackageCatalogFull"),
            QStringLiteral(
                "Installing this package would exceed the 512 MiB user component catalog limit"
            ),
            manifest.id
        );
    }

    const QFileInfo bundleInfo(materializedBundlePath);
    if (!bundleInfo.isFile() || bundleInfo.isSymLink()
        || bundleInfo.size() <= 0
        || bundleInfo.size() > maximumComponentExpandedBytes
                + 512 * 1024) {
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            QStringLiteral("The inspected materialization bundle is unavailable"),
            manifest.id
        );
    }

    const auto stagingRoot = QDir(dataStaging).filePath(randomName());
    if (!ensurePrivateDirectory(stagingRoot, error)) {
        return failure(
            QStringLiteral("PackageTransactionFailed"), error, manifest.id
        );
    }
    const auto contentRoot = QDir(stagingRoot).filePath(QStringLiteral("content"));
    if (!ensurePrivateDirectory(contentRoot, error)) {
        removePathEntry(stagingRoot);
        return failure(
            QStringLiteral("PackageTransactionFailed"), error, manifest.id
        );
    }

    QFile bundle(materializedBundlePath);
    if (!bundle.open(QIODevice::ReadOnly)) {
        removePathEntry(stagingRoot);
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            QStringLiteral("Cannot read the inspected materialization bundle"),
            manifest.id
        );
    }
    const auto materializationErrors = materializeComponentPackageBundle(
        bundle,
        report,
        contentRoot
    );
    if (!materializationErrors.isEmpty()) {
        removePathEntry(stagingRoot);
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            describeValidation(materializationErrors),
            manifest.id
        );
    }

    SafeComponentPaths componentPaths;
    QString destination;
    if (!resolveSafeComponentPaths(
            dataRoot_,
            manifest.id,
            true,
            componentPaths,
            error
        )
        || !resolveSafeVersionPath(
            componentPaths,
            manifest.version,
            true,
            destination,
            error
        )) {
        removePathEntry(stagingRoot);
        return failure(
            QStringLiteral("PackageTransactionFailed"), error, manifest.id
        );
    }
    QString retiredDestination;
    bool publishedDestination = false;
    if (pathEntryExists(destination)) {
        if (validateInstalledTree(destination, report, error)) {
            removePathEntry(stagingRoot);
        } else {
            retiredDestination = QDir(dataTrash).filePath(randomName());
            if (!renamePathEntry(destination, retiredDestination)) {
                removePathEntry(stagingRoot);
                return failure(
                    QStringLiteral("PackageTransactionFailed"),
                    QStringLiteral("Cannot safely retire the invalid installed package"),
                    manifest.id
                );
            }
            if (!QDir().rename(contentRoot, destination)) {
                renamePathEntry(retiredDestination, destination);
                removePathEntry(stagingRoot);
                return failure(
                    QStringLiteral("PackageTransactionFailed"),
                    QStringLiteral("Cannot atomically publish the repaired package"),
                    manifest.id
                );
            }
            publishedDestination = true;
            removePathEntry(stagingRoot);
        }
    } else if (!QDir().rename(contentRoot, destination)) {
        removePathEntry(stagingRoot);
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            QStringLiteral("Cannot atomically publish the inspected package"),
            manifest.id
        );
    } else {
        publishedDestination = true;
        removePathEntry(stagingRoot);
    }

    if (!writeReceipt(existingReceiptPath, report, error)) {
        if (publishedDestination) {
            removePathEntry(destination);
        }
        if (!retiredDestination.isEmpty()) {
            renamePathEntry(retiredDestination, destination);
        }
        return failure(
            QStringLiteral("PackageTransactionFailed"), error, manifest.id
        );
    }
    if (!retiredDestination.isEmpty()) {
        removePathEntry(retiredDestination);
    }
    QString pruneError;
    pruneInactiveVersions(
        dataRoot_,
        manifest.id,
        manifest.version,
        pruneError
    );
    QByteArray settingsSchema;
    if (report.normalizedSettingsSchema.has_value()) {
        settingsSchema = QJsonDocument(*report.normalizedSettingsSchema)
                             .toJson(QJsonDocument::Compact);
    }
    CatalogEntry installedEntry{
        .manifest = report.manifest,
        .settingsSchema = std::move(settingsSchema),
        .declarativeRuntime = report.declarativeRuntime,
        .packageDigest = report.packageDigest,
    };
    auto catalogEntries = currentCatalog.entries;
    const auto existingEntry = std::ranges::find_if(
        catalogEntries,
        [&manifest](const CatalogEntry &entry) {
            return entry.manifest.id == manifest.id;
        }
    );
    if (existingEntry == catalogEntries.end()) {
        catalogEntries.append(std::move(installedEntry));
    } else {
        *existingEntry = std::move(installedEntry);
    }
    std::ranges::sort(catalogEntries, {}, [](const CatalogEntry &entry) {
        return entry.manifest.id;
    });
    return {
        .success = true,
        .componentId = manifest.id,
        .packageDigest = report.packageDigest,
        .catalogEntries = std::move(catalogEntries),
    };
}

UserPackageStoreResult UserPackageStore::remove(
    const QString &componentId,
    const QString &expectedPackageDigest
)
{
    if (leaseFd_ < 0) {
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            leaseError_.isEmpty()
                ? QStringLiteral("The user package store lease is unavailable")
                : leaseError_,
            componentId
        );
    }
    if (!isValidComponentId(componentId)
        || isReservedBuiltinId(componentId)
        || !isFullSha256Digest(expectedPackageDigest)) {
        return failure(
            QStringLiteral("BuiltinRemovalForbidden"),
            QStringLiteral("Only a valid third-party package can be removed"),
            componentId
        );
    }

    auto loaded = load();
    if (!loaded.ok()) {
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            loaded.error,
            componentId
        );
    }
    const auto found = std::ranges::find_if(
        loaded.entries,
        [&componentId](const CatalogEntry &entry) {
            return entry.manifest.id == componentId;
        }
    );
    if (found == loaded.entries.cend()) {
        return failure(
            QStringLiteral("UnknownComponent"),
            QStringLiteral("The third-party package is not installed"),
            componentId
        );
    }
    if (found->packageDigest != expectedPackageDigest) {
        return failure(
            QStringLiteral("PackageDigestMismatch"),
            QStringLiteral("The installed package changed; refresh and review it again"),
            componentId
        );
    }
    for (const auto &entry : loaded.entries) {
        if (entry.manifest.id == componentId) {
            continue;
        }
        if (std::ranges::any_of(
                entry.manifest.dependencies,
                [&componentId](const ComponentDependency &dependency) {
                    return dependency.id == componentId;
                }
            )) {
            return failure(
                QStringLiteral("PackageHasDependents"),
                QStringLiteral("Another installed component depends on this package"),
                componentId
            );
        }
    }

    QString error;
    SafeComponentPaths componentPaths;
    QString installedVersionPath;
    if (!resolveSafeComponentPaths(
            dataRoot_,
            componentId,
            false,
            componentPaths,
            error
        )
        || !resolveSafeVersionPath(
            componentPaths,
            found->manifest.version,
            false,
            installedVersionPath,
            error
        )) {
        return failure(
            QStringLiteral("PackageTransactionFailed"), error, componentId
        );
    }
    const auto stateTrashRoot = stateRoot_ + QStringLiteral("/.trash");
    const auto dataTrashRoot = dataRoot_ + QStringLiteral("/.trash");
    if (!ensurePrivateDirectory(stateTrashRoot, error)
        || !ensurePrivateDirectory(dataTrashRoot, error)) {
        return failure(
            QStringLiteral("PackageTransactionFailed"), error, componentId
        );
    }
    const auto nonce = randomName();
    const auto receipt = receiptPath(componentId);
    const auto trashedReceipt = QDir(stateTrashRoot).filePath(
        nonce + QStringLiteral(".receipt")
    );
    if (!QFile::rename(receipt, trashedReceipt)) {
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            QStringLiteral("Cannot atomically retire the package receipt"),
            componentId
        );
    }

    const auto trashedData = QDir(dataTrashRoot).filePath(nonce);
    if (!renamePathEntry(componentPaths.componentRoot, trashedData)) {
        QFile::rename(trashedReceipt, receipt);
        return failure(
            QStringLiteral("PackageTransactionFailed"),
            QStringLiteral("Cannot retire the package content"),
            componentId
        );
    }

    QFile::remove(trashedReceipt);
    removePathEntry(trashedData);
    auto catalogEntries = std::move(loaded.entries);
    catalogEntries.removeIf([&componentId](const CatalogEntry &entry) {
        return entry.manifest.id == componentId;
    });
    return {
        .success = true,
        .componentId = componentId,
        .packageDigest = expectedPackageDigest,
        .catalogEntries = std::move(catalogEntries),
    };
}

} // namespace HyprShelld::Components
