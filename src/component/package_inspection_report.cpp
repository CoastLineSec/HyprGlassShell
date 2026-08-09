#include "package_inspection_report.h"

#include "declarative_document.h"
#include "settings_schema.h"
#include "strict_json.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace HyprShelld::Components {
namespace {

constexpr qsizetype maximumReportBytes = 1024 * 1024;

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

void rejectUnknownFields(
    const QJsonObject &object,
    const QSet<QString> &allowed,
    const QString &path,
    ValidationErrors &errors
)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        if (!allowed.contains(iterator.key())) {
            addError(
                errors,
                path + QLatin1Char('.') + iterator.key(),
                QStringLiteral("inspection-report.unknown-field"),
                QStringLiteral("The inspection report contains an unknown field.")
            );
        }
    }
}

bool isBoundedInteger(
    const QJsonValue &value,
    const qint64 maximum,
    quint64 &result
)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble())
        || std::floor(value.toDouble()) != value.toDouble()
        || value.toDouble() < 0 || value.toDouble() > maximum) {
        return false;
    }
    result = static_cast<quint64>(value.toDouble());
    return true;
}

const QRegularExpression &digestPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return pattern;
}

} // namespace

bool isCanonicalPackageFilePath(const QString &path)
{
    if (path.isEmpty() || path.size() > 255 || path.toUtf8().size() > 255
        || path != path.normalized(QString::NormalizationForm_C)
        || path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char('\\'))
        || path.contains(QChar::Null)) {
        return false;
    }

    static const QRegularExpression drivePrefix(
        QStringLiteral("^[A-Za-z]:")
    );
    if (drivePrefix.match(path).hasMatch()) {
        return false;
    }

    const auto segments = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    if (segments.size() > 32) {
        return false;
    }
    for (const auto &segment : segments) {
        if (segment.isEmpty() || segment == QStringLiteral(".")
            || segment == QStringLiteral("..")
            || std::ranges::any_of(segment.toUcs4(), [](const auto codePoint) {
                   const auto category = QChar::category(
                       static_cast<char32_t>(codePoint)
                   );
                   return category == QChar::Other_Control
                       || category == QChar::Other_Format;
               })) {
            return false;
        }
    }
    return true;
}

bool isAllowedComponentPackageFilePath(const QString &path)
{
    return path == QStringLiteral("manifest.json")
        || path == QStringLiteral("integrity.json")
        || path == QStringLiteral("settings.schema.json")
        || path == QStringLiteral("icon.png")
        || path.startsWith(QStringLiteral("payload/"))
        || path.startsWith(QStringLiteral("LICENSES/"));
}

QByteArray serializePackageInspectionReport(
    const PackageInspectionReport &report
)
{
    QJsonArray files;
    for (const auto &file : report.files) {
        files.append(QJsonObject{
            {QStringLiteral("path"), file.path},
            {QStringLiteral("size"), static_cast<qint64>(file.size)},
            {QStringLiteral("sha256"), file.sha256},
        });
    }

    const auto activationSupported = !report.declarativeRuntime.isEmpty()
        && validateCurrentHostSupport(report.manifest).isEmpty();
    QJsonObject root{
        {QStringLiteral("reportVersion"), 1},
        {QStringLiteral("inspectionToken"), report.inspectionToken},
        {QStringLiteral("archiveSha256"), report.archiveSha256},
        {QStringLiteral("packageDigest"), report.packageDigest},
        {QStringLiteral("archiveSize"), static_cast<qint64>(report.archiveSize)},
        {QStringLiteral("expandedSize"), static_cast<qint64>(report.expandedSize)},
        {QStringLiteral("activationState"),
         activationSupported ? QStringLiteral("supported")
                             : QStringLiteral("unsupported")},
        {QStringLiteral("manifest"), report.normalizedManifest},
        {QStringLiteral("files"), files},
    };
    if (report.normalizedSettingsSchema.has_value()) {
        root.insert(
            QStringLiteral("settingsSchema"),
            *report.normalizedSettingsSchema
        );
    }
    if (!report.declarativeRuntime.isEmpty()) {
        root.insert(
            QStringLiteral("declarativeRuntime"),
            QJsonDocument::fromJson(report.declarativeRuntime).object()
        );
    }

    auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

ValidationResult<PackageInspectionReport> parsePackageInspectionReport(
    const QByteArrayView bytes
)
{
    ValidationResult<PackageInspectionReport> result;
    const auto parsed = parseStrictJsonObject(
        bytes,
        {
            .maximumBytes = maximumReportBytes,
            .maximumDepth = 32,
        }
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }

    const auto &root = *parsed.value;
    rejectUnknownFields(
        root,
        {
            QStringLiteral("reportVersion"),
            QStringLiteral("inspectionToken"),
            QStringLiteral("archiveSha256"),
            QStringLiteral("packageDigest"),
            QStringLiteral("archiveSize"),
            QStringLiteral("expandedSize"),
            QStringLiteral("activationState"),
            QStringLiteral("manifest"),
            QStringLiteral("settingsSchema"),
            QStringLiteral("declarativeRuntime"),
            QStringLiteral("files"),
        },
        QStringLiteral("$"),
        result.errors
    );

    const auto reportVersion = root.value(QStringLiteral("reportVersion"));
    if (!reportVersion.isDouble() || reportVersion.toDouble() != 1.0) {
        addError(
            result.errors,
            QStringLiteral("$.reportVersion"),
            QStringLiteral("inspection-report.unsupported-version"),
            QStringLiteral("reportVersion must be exactly 1.")
        );
    }

    static const QRegularExpression tokenPattern(
        QStringLiteral("^[0-9a-f]{32}$")
    );
    const auto token = root.value(QStringLiteral("inspectionToken"));
    const auto digest = root.value(QStringLiteral("archiveSha256"));
    const auto packageDigest = root.value(QStringLiteral("packageDigest"));
    const auto activation = root.value(QStringLiteral("activationState"));
    if (!token.isString()
        || !tokenPattern.match(token.toString()).hasMatch()) {
        addError(
            result.errors,
            QStringLiteral("$.inspectionToken"),
            QStringLiteral("inspection-report.invalid-token"),
            QStringLiteral("The inspection token must be a 128-bit lowercase hexadecimal nonce.")
        );
    }
    if (!digest.isString()
        || !digestPattern().match(digest.toString()).hasMatch()) {
        addError(
            result.errors,
            QStringLiteral("$.archiveSha256"),
            QStringLiteral("inspection-report.invalid-archive-digest"),
            QStringLiteral("The archive digest must be lowercase SHA-256.")
        );
    }
    if (!packageDigest.isString()
        || !digestPattern().match(packageDigest.toString()).hasMatch()) {
        addError(
            result.errors,
            QStringLiteral("$.packageDigest"),
            QStringLiteral("inspection-report.invalid-package-digest"),
            QStringLiteral("The package digest must be lowercase SHA-256.")
        );
    }
    if (!activation.isString()
        || (activation.toString() != QStringLiteral("supported")
            && activation.toString() != QStringLiteral("unsupported"))) {
        addError(
            result.errors,
            QStringLiteral("$.activationState"),
            QStringLiteral("inspection-report.invalid-activation-state"),
            QStringLiteral("activationState must be supported or unsupported.")
        );
    }

    PackageInspectionReport report;
    report.inspectionToken = token.toString();
    report.archiveSha256 = digest.toString();
    report.packageDigest = packageDigest.toString();
    if (!isBoundedInteger(
            root.value(QStringLiteral("archiveSize")),
            maximumComponentArchiveBytes,
            report.archiveSize
        ) || report.archiveSize == 0) {
        addError(
            result.errors,
            QStringLiteral("$.archiveSize"),
            QStringLiteral("inspection-report.invalid-archive-size"),
            QStringLiteral("archiveSize is outside the package limit.")
        );
    }
    if (!isBoundedInteger(
            root.value(QStringLiteral("expandedSize")),
            maximumComponentExpandedBytes,
            report.expandedSize
        ) || report.expandedSize == 0) {
        addError(
            result.errors,
            QStringLiteral("$.expandedSize"),
            QStringLiteral("inspection-report.invalid-expanded-size"),
            QStringLiteral("expandedSize is outside the package limit.")
        );
    }

    const auto manifestValue = root.value(QStringLiteral("manifest"));
    if (!manifestValue.isObject()) {
        addError(
            result.errors,
            QStringLiteral("$.manifest"),
            QStringLiteral("inspection-report.manifest-object-required"),
            QStringLiteral("The normalized manifest must be an object.")
        );
    } else {
        report.normalizedManifest = manifestValue.toObject();
        const auto manifestBytes = QJsonDocument(report.normalizedManifest)
                                       .toJson(QJsonDocument::Compact);
        auto manifest = parseComponentManifest(
            QByteArrayView(manifestBytes),
            ComponentOrigin::User
        );
        if (!manifest) {
            result.errors += manifest.errors;
        } else {
            report.manifest = std::move(*manifest.value);
        }
    }

    std::optional<SettingsSchema> parsedSettingsSchema;
    const auto schemaValue = root.value(QStringLiteral("settingsSchema"));
    if (!schemaValue.isUndefined()) {
        if (!schemaValue.isObject()) {
            addError(
                result.errors,
                QStringLiteral("$.settingsSchema"),
                QStringLiteral("inspection-report.schema-object-required"),
                QStringLiteral("The normalized settings schema must be an object.")
            );
        } else {
            report.normalizedSettingsSchema = schemaValue.toObject();
            const auto schemaBytes = QJsonDocument(*report.normalizedSettingsSchema)
                                         .toJson(QJsonDocument::Compact);
            auto schema = parseSettingsSchema(QByteArrayView(schemaBytes));
            if (!schema) {
                result.errors += schema.errors;
            } else {
                parsedSettingsSchema = std::move(*schema.value);
            }
        }
    }

    const auto declarativeValue = root.value(
        QStringLiteral("declarativeRuntime")
    );
    if (report.manifest.runtime.kind == RuntimeKind::DeclarativeV1) {
        if (!declarativeValue.isObject()) {
            addError(
                result.errors,
                QStringLiteral("$.declarativeRuntime"),
                QStringLiteral("inspection-report.declarative-object-required"),
                QStringLiteral("A declarative-v1 report must contain its normalized runtime document.")
            );
        } else {
            const auto documentBytes = QJsonDocument(
                declarativeValue.toObject()
            ).toJson(QJsonDocument::Compact);
            const auto document = parseDeclarativeDocument(
                QByteArrayView(documentBytes),
                parsedSettingsSchema.has_value()
                    ? &*parsedSettingsSchema
                    : nullptr
            );
            if (!document) {
                result.errors += document.errors;
            } else {
                report.declarativeRuntime = serializeDeclarativeDocument(
                    *document.value
                );
            }
        }
    } else if (!declarativeValue.isUndefined()) {
        addError(
            result.errors,
            QStringLiteral("$.declarativeRuntime"),
            QStringLiteral("inspection-report.unexpected-declarative-runtime"),
            QStringLiteral("Only declarative-v1 packages may carry a declarative runtime document.")
        );
    }

    const auto expectedActivation = !report.declarativeRuntime.isEmpty()
            && validateCurrentHostSupport(report.manifest).isEmpty()
        ? QStringLiteral("supported")
        : QStringLiteral("unsupported");
    if (activation.isString()
        && activation.toString() != expectedActivation) {
        addError(
            result.errors,
            QStringLiteral("$.activationState"),
            QStringLiteral("inspection-report.activation-state-mismatch"),
            QStringLiteral("activationState does not match the validated manifest and runtime document.")
        );
    }

    const auto filesValue = root.value(QStringLiteral("files"));
    if (!filesValue.isArray() || filesValue.toArray().size() < 2
        || filesValue.toArray().size() > maximumComponentArchiveEntries) {
        addError(
            result.errors,
            QStringLiteral("$.files"),
            QStringLiteral("inspection-report.invalid-file-list"),
            QStringLiteral("The report must contain two to 512 files.")
        );
    } else {
        QString previousPath;
        quint64 totalSize = 0;
        const auto files = filesValue.toArray();
        for (qsizetype index = 0; index < files.size(); ++index) {
            const auto path = QStringLiteral("$.files[%1]").arg(index);
            if (!files.at(index).isObject()) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("inspection-report.file-object-required"),
                    QStringLiteral("Every reported file must be an object.")
                );
                continue;
            }
            const auto object = files.at(index).toObject();
            rejectUnknownFields(
                object,
                {
                    QStringLiteral("path"),
                    QStringLiteral("size"),
                    QStringLiteral("sha256"),
                },
                path,
                result.errors
            );
            const auto filePath = object.value(QStringLiteral("path"));
            const auto fileDigest = object.value(QStringLiteral("sha256"));
            quint64 fileSize = 0;
            if (!filePath.isString()
                || !isCanonicalPackageFilePath(filePath.toString())
                || !isAllowedComponentPackageFilePath(filePath.toString())
                || (!previousPath.isEmpty()
                    && filePath.toString() <= previousPath)) {
                addError(
                    result.errors,
                    path + QStringLiteral(".path"),
                    QStringLiteral("inspection-report.invalid-file-path"),
                    QStringLiteral("Reported file paths must be allowed, sorted, and unique.")
                );
            }
            if (!isBoundedInteger(
                    object.value(QStringLiteral("size")),
                    maximumComponentFileBytes,
                    fileSize
                )
                || totalSize > maximumComponentExpandedBytes - fileSize) {
                addError(
                    result.errors,
                    path + QStringLiteral(".size"),
                    QStringLiteral("inspection-report.invalid-file-size"),
                    QStringLiteral("A reported file size exceeds the package limits.")
                );
            } else {
                totalSize += fileSize;
            }
            const auto typedPath = filePath.toString();
            if ((typedPath == QStringLiteral("manifest.json")
                    && fileSize > 128 * 1024)
                || (typedPath == QStringLiteral("integrity.json")
                    && fileSize > 512 * 1024)
                || (typedPath == QStringLiteral("settings.schema.json")
                    && fileSize > 256 * 1024)
                || (typedPath == QStringLiteral("icon.png")
                    && fileSize > 4 * 1024 * 1024)
                || (report.manifest.runtime.kind
                        == RuntimeKind::DeclarativeV1
                    && typedPath == report.manifest.runtime.entrypoint
                    && fileSize
                        > static_cast<quint64>(
                            maximumDeclarativeDocumentBytes
                        ))) {
                addError(
                    result.errors,
                    path + QStringLiteral(".size"),
                    QStringLiteral("inspection-report.metadata-too-large"),
                    QStringLiteral("A reported metadata file exceeds its specific limit.")
                );
            }
            if (!fileDigest.isString()
                || !digestPattern().match(fileDigest.toString()).hasMatch()) {
                addError(
                    result.errors,
                    path + QStringLiteral(".sha256"),
                    QStringLiteral("inspection-report.invalid-file-digest"),
                    QStringLiteral("A reported file digest is not lowercase SHA-256.")
                );
            }
            previousPath = filePath.toString();
            report.files.append({
                .path = filePath.toString(),
                .size = fileSize,
                .sha256 = fileDigest.toString(),
            });
        }
        if (totalSize != report.expandedSize) {
            addError(
                result.errors,
                QStringLiteral("$.expandedSize"),
                QStringLiteral("inspection-report.expanded-size-mismatch"),
                QStringLiteral("expandedSize does not equal the reported file sizes.")
            );
        }
    }

    QSet<QString> filePaths;
    QMap<QString, QString> collisionOwners;
    for (const auto &file : report.files) {
        filePaths.insert(file.path);
        const auto folded = file.path.toCaseFolded();
        const auto existing = collisionOwners.constFind(folded);
        if (existing != collisionOwners.cend() && *existing != file.path) {
            addError(
                result.errors,
                QStringLiteral("$.files"),
                QStringLiteral("inspection-report.colliding-path"),
                QStringLiteral("Reported paths collide after case folding.")
            );
        }
        collisionOwners.insert(folded, file.path);
    }
    if (!filePaths.contains(QStringLiteral("manifest.json"))
        || !filePaths.contains(QStringLiteral("integrity.json"))) {
        addError(
            result.errors,
            QStringLiteral("$.files"),
            QStringLiteral("inspection-report.required-file-missing"),
            QStringLiteral("The report is missing required package files.")
        );
    }
    if (!report.normalizedManifest.isEmpty()) {
        const auto declaresSchema = report.manifest.settingsSchema.has_value();
        if (declaresSchema != report.normalizedSettingsSchema.has_value()
            || declaresSchema
                != filePaths.contains(QStringLiteral("settings.schema.json"))) {
            addError(
                result.errors,
                QStringLiteral("$.settingsSchema"),
                QStringLiteral("inspection-report.schema-presence-mismatch"),
                QStringLiteral("The manifest, report, and file list disagree about the settings schema.")
            );
        }
        if (!report.manifest.runtime.entrypoint.isEmpty()
            && !filePaths.contains(report.manifest.runtime.entrypoint)) {
            addError(
                result.errors,
                QStringLiteral("$.manifest.runtime.entrypoint"),
                QStringLiteral("inspection-report.entrypoint-missing"),
                QStringLiteral("The runtime entry point is absent from the file list.")
            );
        }
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(report);
    }
    return result;
}

} // namespace HyprShelld::Components
