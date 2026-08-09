#include "package_integrity.h"

#include "strict_json.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace HyprShelld::Components {
namespace {

constexpr qsizetype maximumIntegrityBytes = 512 * 1024;
constexpr qsizetype maximumIntegrityFiles = 511;

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
                QStringLiteral("integrity.unknown-field"),
                QStringLiteral("Unknown integrity field: %1")
                    .arg(iterator.key())
            );
        }
    }
}

bool isCanonicalFilePath(const QString &path)
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

} // namespace

ValidationResult<PackageIntegrity> parsePackageIntegrity(
    const QByteArrayView bytes
)
{
    ValidationResult<PackageIntegrity> result;
    const auto parsed = parseStrictJsonObject(
        bytes,
        {
            .maximumBytes = maximumIntegrityBytes,
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
            QStringLiteral("integrityVersion"),
            QStringLiteral("algorithm"),
            QStringLiteral("files"),
        },
        QStringLiteral("$"),
        result.errors
    );

    const auto version = root.value(QStringLiteral("integrityVersion"));
    if (!version.isDouble() || version.toDouble() != 1.0) {
        addError(
            result.errors,
            QStringLiteral("$.integrityVersion"),
            QStringLiteral("integrity.unsupported-version"),
            QStringLiteral("integrityVersion must be exactly 1.")
        );
    }

    const auto algorithm = root.value(QStringLiteral("algorithm"));
    if (!algorithm.isString()
        || algorithm.toString() != QStringLiteral("sha256")) {
        addError(
            result.errors,
            QStringLiteral("$.algorithm"),
            QStringLiteral("integrity.unsupported-algorithm"),
            QStringLiteral("The integrity algorithm must be sha256.")
        );
    }

    const auto filesValue = root.value(QStringLiteral("files"));
    if (!filesValue.isObject() || filesValue.toObject().isEmpty()
        || filesValue.toObject().size() > maximumIntegrityFiles) {
        addError(
            result.errors,
            QStringLiteral("$.files"),
            QStringLiteral("integrity.invalid-file-set"),
            QStringLiteral("files must contain one to 511 digest entries.")
        );
        return result;
    }

    static const QRegularExpression digestPattern(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    PackageIntegrity integrity;
    QMap<QString, QString> collisionOwners;
    const auto files = filesValue.toObject();
    for (auto iterator = files.constBegin(); iterator != files.constEnd();
         ++iterator) {
        const auto path = iterator.key();
        const auto fieldPath = QStringLiteral("$.files.") + path;
        if (!isCanonicalFilePath(path)
            || path == QStringLiteral("integrity.json")
            || path == QStringLiteral("signature.json")) {
            addError(
                result.errors,
                fieldPath,
                QStringLiteral("integrity.invalid-path"),
                QStringLiteral("The integrity entry has an unsafe or reserved path.")
            );
        }

        const auto collisionKey = path.normalized(
            QString::NormalizationForm_C
        ).toCaseFolded();
        const auto existing = collisionOwners.constFind(collisionKey);
        if (existing != collisionOwners.cend() && *existing != path) {
            addError(
                result.errors,
                fieldPath,
                QStringLiteral("integrity.colliding-path"),
                QStringLiteral("Integrity paths collide after Unicode and case normalization.")
            );
        } else {
            collisionOwners.insert(collisionKey, path);
        }

        if (!iterator.value().isString()
            || !digestPattern.match(iterator.value().toString()).hasMatch()) {
            addError(
                result.errors,
                fieldPath,
                QStringLiteral("integrity.invalid-digest"),
                QStringLiteral("Every file digest must be lowercase SHA-256.")
            );
        } else {
            integrity.files.insert(path, iterator.value().toString());
        }
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(integrity);
    }
    return result;
}

} // namespace HyprShelld::Components
