#include "component_package_bundle.h"

#include "package_integrity.h"
#include "settings_schema.h"
#include "strict_json.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace HyprShelld::Components {
namespace {

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

bool writeAll(QIODevice &device, QByteArrayView bytes)
{
    qsizetype written = 0;
    while (written < bytes.size()) {
        const auto remaining = bytes.sliced(written);
        const auto count = device.write(remaining.data(), remaining.size());
        if (count <= 0) {
            return false;
        }
        written += count;
    }
    return true;
}

bool readExact(QIODevice &device, char *destination, const qsizetype size)
{
    qsizetype read = 0;
    while (read < size) {
        const auto count = device.read(destination + read, size - read);
        if (count <= 0) {
            return false;
        }
        read += count;
    }
    return true;
}

template<typename Integer>
bool writeBigEndian(QIODevice &device, const Integer value)
{
    std::array<uchar, sizeof(Integer)> bytes{};
    qToBigEndian<Integer>(value, bytes.data());
    return writeAll(
        device,
        QByteArrayView(
            reinterpret_cast<const char *>(bytes.data()),
            bytes.size()
        )
    );
}

template<typename Integer>
bool readBigEndian(QIODevice &device, Integer &value)
{
    std::array<uchar, sizeof(Integer)> bytes{};
    if (!readExact(
            device,
            reinterpret_cast<char *>(bytes.data()),
            bytes.size()
        )) {
        return false;
    }
    value = qFromBigEndian<Integer>(bytes.data());
    return true;
}

bool validateFiles(
    const QVector<ComponentPackageBundleFile> &files,
    QString &error
)
{
    if (files.size() < 2 || files.size() > maximumComponentArchiveEntries) {
        error = QStringLiteral("A package bundle must contain two to 512 files.");
        return false;
    }

    QString previous;
    QMap<QString, QString> collisionOwners;
    quint64 total = 0;
    for (const auto &file : files) {
        if (!isCanonicalPackageFilePath(file.path)
            || !isAllowedComponentPackageFilePath(file.path)
            || (!previous.isEmpty() && file.path <= previous)
            || file.contents.size() > maximumComponentFileBytes
            || total > maximumComponentExpandedBytes
                    - static_cast<quint64>(file.contents.size())) {
            error = QStringLiteral("The package bundle violates path, order, or size limits.");
            return false;
        }
        const auto folded = file.path.toCaseFolded();
        if (collisionOwners.contains(folded)) {
            error = QStringLiteral("Package bundle paths collide after case folding.");
            return false;
        }
        collisionOwners.insert(folded, file.path);
        previous = file.path;
        total += static_cast<quint64>(file.contents.size());
    }
    return true;
}

bool prepareDestination(
    const QString &path,
    QString &canonicalRoot,
    QString &error
)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir() || info.isSymLink()) {
        error = QStringLiteral("The materialization destination must be a regular directory.");
        return false;
    }
    const QDir directory(path);
    if (!directory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        error = QStringLiteral("The materialization destination must be empty.");
        return false;
    }
    canonicalRoot = info.canonicalFilePath();
    if (canonicalRoot.isEmpty()) {
        error = QStringLiteral("The materialization destination cannot be resolved.");
        return false;
    }
    return true;
}

bool materializeFiles(
    const QVector<ComponentPackageBundleFile> &files,
    const QString &destinationDirectory,
    QString &error
)
{
    QString canonicalRoot;
    if (!prepareDestination(destinationDirectory, canonicalRoot, error)) {
        return false;
    }

    const auto rootPrefix = canonicalRoot + QDir::separator();
    for (const auto &file : files) {
        const auto relativeParent = QFileInfo(file.path).path();
        if (relativeParent != QStringLiteral(".")) {
            QDir root(canonicalRoot);
            if (!root.mkpath(relativeParent)) {
                error = QStringLiteral("Cannot create a package directory.");
                return false;
            }
            const auto parentPath = root.filePath(relativeParent);
            const QFileInfo parentInfo(parentPath);
            const auto canonicalParent = parentInfo.canonicalFilePath();
            if (!parentInfo.isDir() || parentInfo.isSymLink()
                || (canonicalParent != canonicalRoot
                    && !canonicalParent.startsWith(rootPrefix))) {
                error = QStringLiteral("A materialized path escaped the destination.");
                return false;
            }
            QFile::setPermissions(
                parentPath,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner
                    | QFileDevice::ExeOwner
            );
        }

        QSaveFile output(QDir(canonicalRoot).filePath(file.path));
        output.setDirectWriteFallback(false);
        if (!output.open(QIODevice::WriteOnly)) {
            error = QStringLiteral("Cannot create package file %1: %2")
                        .arg(file.path, output.errorString());
            return false;
        }
        output.setPermissions(
            QFileDevice::ReadOwner | QFileDevice::WriteOwner
        );
        if (output.write(file.contents) != file.contents.size()
            || !output.commit()) {
            error = QStringLiteral("Cannot commit package file %1: %2")
                        .arg(file.path, output.errorString());
            return false;
        }
    }
    return true;
}

void addDigestFrame(
    QCryptographicHash &hash,
    const QByteArrayView name,
    const QByteArrayView value
)
{
    std::array<uchar, sizeof(quint64)> length{};
    qToBigEndian<quint64>(static_cast<quint64>(name.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(name);
    qToBigEndian<quint64>(static_cast<quint64>(value.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(value);
}

} // namespace

bool writeComponentPackageBundle(
    QIODevice &destination,
    const QVector<ComponentPackageBundleFile> &files,
    QString &error
)
{
    if (!validateFiles(files, error)) {
        return false;
    }
    if (!writeBigEndian<quint32>(
            destination,
            static_cast<quint32>(files.size())
        )) {
        error = QStringLiteral("Cannot write the package bundle header.");
        return false;
    }
    for (const auto &file : files) {
        const auto path = file.path.toUtf8();
        if (!writeBigEndian<quint16>(
                destination,
                static_cast<quint16>(path.size())
            )
            || !writeAll(destination, path)
            || !writeBigEndian<quint64>(
                destination,
                static_cast<quint64>(file.contents.size())
            )
            || !writeAll(destination, file.contents)) {
            error = QStringLiteral("Cannot write the package bundle.");
            return false;
        }
    }
    return true;
}

ValidationResult<QVector<ComponentPackageBundleFile>>
readComponentPackageBundle(QIODevice &source)
{
    ValidationResult<QVector<ComponentPackageBundleFile>> result;
    quint32 count = 0;
    if (!readBigEndian(source, count) || count < 2
        || count > maximumComponentArchiveEntries) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("package-bundle.invalid-count"),
            QStringLiteral("The bundle file count is missing or out of range.")
        );
        return result;
    }

    QVector<ComponentPackageBundleFile> files;
    files.reserve(static_cast<qsizetype>(count));
    quint64 totalSize = 0;
    QString previousPath;
    QMap<QString, QString> collisionOwners;
    for (quint32 index = 0; index < count; ++index) {
        quint16 pathSize = 0;
        if (!readBigEndian(source, pathSize) || pathSize == 0
            || pathSize > 255) {
            addError(
                result.errors,
                QStringLiteral("$.files[%1].path").arg(index),
                QStringLiteral("package-bundle.invalid-path-size"),
                QStringLiteral("The bundle path length is invalid.")
            );
            return result;
        }
        QByteArray encodedPath(pathSize, Qt::Uninitialized);
        if (!readExact(source, encodedPath.data(), encodedPath.size())) {
            addError(
                result.errors,
                QStringLiteral("$.files[%1].path").arg(index),
                QStringLiteral("package-bundle.truncated-path"),
                QStringLiteral("The bundle ended inside a path.")
            );
            return result;
        }
        const auto path = QString::fromUtf8(encodedPath);
        if (path.toUtf8() != encodedPath
            || !isCanonicalPackageFilePath(path)
            || !isAllowedComponentPackageFilePath(path)
            || (!previousPath.isEmpty() && path <= previousPath)) {
            addError(
                result.errors,
                QStringLiteral("$.files[%1].path").arg(index),
                QStringLiteral("package-bundle.invalid-path"),
                QStringLiteral("Bundle paths must be safe, sorted, and unique.")
            );
            return result;
        }
        const auto folded = path.toCaseFolded();
        if (collisionOwners.contains(folded)) {
            addError(
                result.errors,
                QStringLiteral("$.files[%1].path").arg(index),
                QStringLiteral("package-bundle.colliding-path"),
                QStringLiteral("Bundle paths collide after case folding.")
            );
            return result;
        }
        collisionOwners.insert(folded, path);

        quint64 fileSize = 0;
        if (!readBigEndian(source, fileSize)
            || fileSize > static_cast<quint64>(maximumComponentFileBytes)
            || totalSize > static_cast<quint64>(maximumComponentExpandedBytes)
                    - fileSize
            || fileSize
                > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
            addError(
                result.errors,
                QStringLiteral("$.files[%1].size").arg(index),
                QStringLiteral("package-bundle.invalid-size"),
                QStringLiteral("A bundled file exceeds the package limits.")
            );
            return result;
        }
        QByteArray contents(static_cast<qsizetype>(fileSize), Qt::Uninitialized);
        if (!readExact(source, contents.data(), contents.size())) {
            addError(
                result.errors,
                QStringLiteral("$.files[%1]").arg(index),
                QStringLiteral("package-bundle.truncated-file"),
                QStringLiteral("The bundle ended inside a file.")
            );
            return result;
        }
        totalSize += fileSize;
        previousPath = path;
        files.append(ComponentPackageBundleFile{
            .path = path,
            .contents = std::move(contents),
        });
    }

    char trailing = 0;
    if (source.read(&trailing, 1) != 0) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("package-bundle.trailing-data"),
            QStringLiteral("Unexpected bytes follow the package bundle.")
        );
        return result;
    }

    result.value = std::move(files);
    return result;
}

QString deriveComponentPackageDigest(
    const QVector<ComponentPackageBundleFile> &files
)
{
    QString error;
    if (!validateFiles(files, error)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto &file : files) {
        const auto path = file.path.toUtf8();
        addDigestFrame(hash, path, file.contents);
    }
    return QString::fromLatin1(hash.result().toHex());
}

ValidationErrors materializeComponentPackageBundle(
    QIODevice &source,
    const PackageInspectionReport &expectedReport,
    const QString &destinationDirectory
)
{
    ValidationErrors errors;
    auto parsed = readComponentPackageBundle(source);
    if (!parsed) {
        return parsed.errors;
    }
    const auto &files = *parsed.value;
    if (files.size() != expectedReport.files.size()) {
        addError(
            errors,
            QStringLiteral("$"),
            QStringLiteral("package-bundle.report-mismatch"),
            QStringLiteral("The bundle file count differs from the inspection report.")
        );
        return errors;
    }
    for (qsizetype index = 0; index < files.size(); ++index) {
        const auto &file = files.at(index);
        const auto &expected = expectedReport.files.at(index);
        const auto digest = QString::fromLatin1(
            QCryptographicHash::hash(
                file.contents,
                QCryptographicHash::Sha256
            ).toHex()
        );
        if (file.path != expected.path
            || static_cast<quint64>(file.contents.size()) != expected.size
            || digest != expected.sha256) {
            addError(
                errors,
                QStringLiteral("$.files[%1]").arg(index),
                QStringLiteral("package-bundle.report-mismatch"),
                QStringLiteral("A bundled file differs from the inspection report.")
            );
            return errors;
        }
    }
    if (deriveComponentPackageDigest(files) != expectedReport.packageDigest) {
        addError(
            errors,
            QStringLiteral("$.packageDigest"),
            QStringLiteral("package-bundle.package-digest-mismatch"),
            QStringLiteral("The materialized bundle differs from the package digest.")
        );
        return errors;
    }

    QMap<QString, QByteArray> contentsByPath;
    for (const auto &file : files) {
        contentsByPath.insert(file.path, file.contents);
    }
    const auto manifestBytes = contentsByPath.value(
        QStringLiteral("manifest.json")
    );
    const auto manifest = parseComponentManifest(
        QByteArrayView(manifestBytes),
        ComponentOrigin::User
    );
    const auto manifestObject = parseStrictJsonObject(
        QByteArrayView(manifestBytes),
        {.maximumBytes = 128 * 1024, .maximumDepth = 32}
    );
    if (!manifest || !manifestObject
        || *manifest.value != expectedReport.manifest
        || *manifestObject.value != expectedReport.normalizedManifest) {
        addError(
            errors,
            QStringLiteral("$.manifest"),
            QStringLiteral("package-bundle.manifest-mismatch"),
            QStringLiteral("The bundled manifest differs from the inspection report.")
        );
        return errors;
    }

    const auto schemaBytes = contentsByPath.value(
        QStringLiteral("settings.schema.json")
    );
    if (expectedReport.normalizedSettingsSchema.has_value()) {
        const auto schema = parseSettingsSchema(QByteArrayView(schemaBytes));
        const auto schemaObject = parseStrictJsonObject(
            QByteArrayView(schemaBytes),
            {.maximumBytes = 256 * 1024, .maximumDepth = 32}
        );
        if (!schema || !schemaObject
            || *schemaObject.value
                != *expectedReport.normalizedSettingsSchema) {
            addError(
                errors,
                QStringLiteral("$.settingsSchema"),
                QStringLiteral("package-bundle.schema-mismatch"),
                QStringLiteral("The bundled settings schema differs from the inspection report.")
            );
            return errors;
        }
    } else if (!schemaBytes.isEmpty()) {
        addError(
            errors,
            QStringLiteral("$.settingsSchema"),
            QStringLiteral("package-bundle.schema-mismatch"),
            QStringLiteral("The bundle contains an undeclared settings schema.")
        );
        return errors;
    }

    const auto integrity = parsePackageIntegrity(QByteArrayView(
        contentsByPath.value(QStringLiteral("integrity.json"))
    ));
    if (!integrity) {
        errors += integrity.errors;
        return errors;
    }
    QSet<QString> coveredPaths;
    for (const auto &file : files) {
        if (file.path == QStringLiteral("integrity.json")) {
            continue;
        }
        coveredPaths.insert(file.path);
        const auto expected = integrity.value->files.constFind(file.path);
        const auto digest = QString::fromLatin1(
            QCryptographicHash::hash(
                file.contents,
                QCryptographicHash::Sha256
            ).toHex()
        );
        if (expected == integrity.value->files.cend()
            || *expected != digest) {
            addError(
                errors,
                QStringLiteral("$.integrity"),
                QStringLiteral("package-bundle.integrity-mismatch"),
                QStringLiteral("The bundle does not match integrity.json.")
            );
            return errors;
        }
    }
    QSet<QString> declaredPaths;
    for (auto iterator = integrity.value->files.cbegin();
         iterator != integrity.value->files.cend(); ++iterator) {
        declaredPaths.insert(iterator.key());
    }
    if (coveredPaths != declaredPaths) {
        addError(
            errors,
            QStringLiteral("$.integrity"),
            QStringLiteral("package-bundle.integrity-file-set"),
            QStringLiteral("The bundle and integrity file sets differ.")
        );
        return errors;
    }

    QString error;
    if (!materializeFiles(files, destinationDirectory, error)) {
        addError(
            errors,
            QStringLiteral("$"),
            QStringLiteral("package-bundle.materialization-failed"),
            error
        );
    }
    return errors;
}

} // namespace HyprShelld::Components
