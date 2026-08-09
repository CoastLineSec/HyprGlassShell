#include "system_catalog.h"

#include "component/component_configuration.h"
#include "component/declarative_document.h"
#include "component/settings_schema.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <array>

namespace HyprShelld::Components {
namespace {

constexpr qsizetype maximumSystemComponents = 512;
constexpr qint64 maximumManifestBytes = 128 * 1024;
constexpr qint64 maximumSettingsSchemaBytes = 256 * 1024;

QString describeErrors(
    const QString &componentPath,
    const ValidationErrors &errors
)
{
    QStringList details;
    details.reserve(errors.size());

    for (const auto &error : errors) {
        details.append(
            QStringLiteral("%1 [%2]: %3")
                .arg(error.path, error.code, error.message)
        );
    }

    return QStringLiteral("Invalid system component at %1: %2")
        .arg(componentPath, details.join(QStringLiteral("; ")));
}

bool readRegularFile(
    const QString &path,
    const QString &requiredParent,
    qint64 maximumBytes,
    QByteArray &contents,
    QString &error
)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.isSymLink()) {
        error = QStringLiteral("%1 must be a regular file and not a symbolic link")
                    .arg(path);
        return false;
    }
    if (info.canonicalPath() != QFileInfo(requiredParent).canonicalFilePath()) {
        error = QStringLiteral("%1 escapes its system component directory")
                    .arg(path);
        return false;
    }

    if (info.size() < 0 || info.size() > maximumBytes) {
        error = QStringLiteral("%1 exceeds the %2-byte size limit")
                    .arg(path)
                    .arg(maximumBytes);
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot read %1: %2")
                    .arg(path, file.errorString());
        return false;
    }

    contents = file.read(maximumBytes + 1);
    if (contents.size() > maximumBytes) {
        error = QStringLiteral("%1 exceeds the %2-byte size limit")
                    .arg(path)
                    .arg(maximumBytes);
        return false;
    }
    if (file.error() != QFileDevice::NoError) {
        error = QStringLiteral("Cannot read %1: %2")
                    .arg(path, file.errorString());
        return false;
    }

    return true;
}

void addDigestFile(
    QCryptographicHash &hash,
    const QByteArray &relativePath,
    const QByteArray &contents
)
{
    std::array<uchar, sizeof(quint64)> encodedLength{};

    qToBigEndian<quint64>(
        static_cast<quint64>(relativePath.size()),
        encodedLength.data()
    );
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(encodedLength.data()),
        encodedLength.size()
    ));
    hash.addData(relativePath);

    qToBigEndian<quint64>(
        static_cast<quint64>(contents.size()),
        encodedLength.data()
    );
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(encodedLength.data()),
        encodedLength.size()
    ));
    hash.addData(contents);
}

QString packageDigest(
    const QByteArray &manifest,
    const QByteArray &settingsSchema
)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    addDigestFile(hash, QByteArrayLiteral("manifest.json"), manifest);
    if (!settingsSchema.isEmpty()) {
        addDigestFile(
            hash,
            QByteArrayLiteral("settings.schema.json"),
            settingsSchema
        );
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString deriveCatalogDigest(const QHash<QString, CatalogEntry> &entries)
{
    auto ids = entries.keys();
    ids.sort();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto &id : ids) {
        addDigestFile(
            hash,
            id.toUtf8(),
            entries.value(id).packageDigest.toLatin1()
        );
    }

    return QString::fromLatin1(hash.result().toHex());
}

bool validateDirectoryContents(
    const QString &componentPath,
    bool hasSettingsSchema,
    QString &error
)
{
    QDir directory(componentPath);
    const auto children = directory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::Name
    );

    const QStringList expected = hasSettingsSchema
        ? QStringList{
              QStringLiteral("manifest.json"),
              QStringLiteral("settings.schema.json"),
          }
        : QStringList{QStringLiteral("manifest.json")};

    QStringList actual;
    actual.reserve(children.size());
    for (const auto &child : children) {
        if (!child.isFile() || child.isSymLink()) {
            error = QStringLiteral("Unexpected non-regular entry in %1: %2")
                        .arg(componentPath, child.fileName());
            return false;
        }
        actual.append(child.fileName());
    }
    actual.sort();

    auto sortedExpected = expected;
    sortedExpected.sort();
    if (actual != sortedExpected) {
        error = QStringLiteral("Unexpected system component contents in %1")
                    .arg(componentPath);
        return false;
    }

    return true;
}

} // namespace

CatalogLoadResult SystemCatalog::load(const QString &rootPath)
{
    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()) {
        return {
            std::nullopt,
            QStringLiteral("System component catalog is not a regular directory: %1")
                .arg(rootPath),
        };
    }

    QDir root(rootPath);
    const auto canonicalRoot = rootInfo.canonicalFilePath();
    const auto componentDirectories = root.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::Name
    );
    if (componentDirectories.isEmpty()) {
        return {
            std::nullopt,
            QStringLiteral("System component catalog is empty: %1").arg(rootPath),
        };
    }
    if (componentDirectories.size() > maximumSystemComponents) {
        return {
            std::nullopt,
            QStringLiteral("System component catalog contains more than %1 entries")
                .arg(maximumSystemComponents),
        };
    }

    SystemCatalog catalog;
    for (const auto &directoryInfo : componentDirectories) {
        if (!directoryInfo.isDir() || directoryInfo.isSymLink()) {
            return {
                std::nullopt,
                QStringLiteral("Unexpected entry in system component catalog: %1")
                    .arg(directoryInfo.filePath()),
            };
        }
        if (directoryInfo.canonicalPath() != canonicalRoot) {
            return {
                std::nullopt,
                QStringLiteral("System component escaped the protected catalog: %1")
                    .arg(directoryInfo.filePath()),
            };
        }

        const auto componentPath = directoryInfo.filePath();
        QByteArray manifestBytes;
        QString error;
        if (!readRegularFile(
                componentPath + QStringLiteral("/manifest.json"),
                componentPath,
                maximumManifestBytes,
                manifestBytes,
                error
            )) {
            return {std::nullopt, error};
        }

        auto parsed = parseComponentManifest(
            QByteArrayView(manifestBytes),
            ComponentOrigin::System
        );
        if (!parsed) {
            return {
                std::nullopt,
                describeErrors(componentPath, parsed.errors),
            };
        }

        auto supportErrors = validateCurrentHostSupport(*parsed.value);
        if (!supportErrors.isEmpty()) {
            return {
                std::nullopt,
                describeErrors(componentPath, supportErrors),
            };
        }

        const auto &manifest = *parsed.value;
        if (catalog.entries_.contains(manifest.id)) {
            return {
                std::nullopt,
                QStringLiteral("Duplicate system component ID: %1").arg(manifest.id),
            };
        }
        if (manifest.id != directoryInfo.fileName()) {
            return {
                std::nullopt,
                QStringLiteral("System component directory does not match manifest ID: %1")
                    .arg(componentPath),
            };
        }

        QByteArray settingsSchemaBytes;
        if (manifest.settingsSchema.has_value()) {
            if (*manifest.settingsSchema != QStringLiteral("settings.schema.json")) {
                return {
                    std::nullopt,
                    QStringLiteral("Unsupported settings schema path in %1")
                        .arg(componentPath),
                };
            }

            if (!readRegularFile(
                    componentPath + QStringLiteral("/settings.schema.json"),
                    componentPath,
                    maximumSettingsSchemaBytes,
                    settingsSchemaBytes,
                    error
                )) {
                return {std::nullopt, error};
            }

            auto schema = parseSettingsSchema(QByteArrayView(settingsSchemaBytes));
            if (!schema) {
                return {
                    std::nullopt,
                    describeErrors(componentPath, schema.errors),
                };
            }
        }

        if (!validateDirectoryContents(
                componentPath,
                manifest.settingsSchema.has_value(),
                error
            )) {
            return {std::nullopt, error};
        }

        const auto digest = packageDigest(manifestBytes, settingsSchemaBytes);
        CatalogEntry entry{
            .manifest = std::move(*parsed.value),
            .settingsSchema = std::move(settingsSchemaBytes),
            .declarativeRuntime = {},
            .packageDigest = digest,
        };
        catalog.entries_.insert(entry.manifest.id, std::move(entry));
    }

    if (!catalog.entries_.contains(QString::fromLatin1(workspaceSwitcherId))) {
        return {
            std::nullopt,
            QStringLiteral("System component catalog is missing the workspace switcher"),
        };
    }

    catalog.catalogDigest_ = deriveCatalogDigest(catalog.entries_);
    return {std::move(catalog), QString()};
}

CatalogLoadResult SystemCatalog::withUserEntries(
    SystemCatalog systemCatalog,
    QVector<CatalogEntry> userEntries
)
{
    if (systemCatalog.entries_.size() + userEntries.size()
        > maximumSystemComponents) {
        return {
            std::nullopt,
            QStringLiteral("The combined component catalog contains more than %1 entries")
                .arg(maximumSystemComponents),
        };
    }

    for (auto &entry : userEntries) {
        const auto &manifest = entry.manifest;
        if (manifest.origin != ComponentOrigin::User
            || isReservedBuiltinId(manifest.id)) {
            return {
                std::nullopt,
                QStringLiteral("A user component attempted to use a protected identity: %1")
                    .arg(manifest.id),
            };
        }
        if (systemCatalog.entries_.contains(manifest.id)) {
            return {
                std::nullopt,
                QStringLiteral("Duplicate component ID across system and user catalogs: %1")
                    .arg(manifest.id),
            };
        }
        if (!isFullSha256Digest(entry.packageDigest)) {
            return {
                std::nullopt,
                QStringLiteral("A user component has an invalid package digest: %1")
                    .arg(manifest.id),
            };
        }
        if (manifest.settingsSchema.has_value()
            != !entry.settingsSchema.isEmpty()) {
            return {
                std::nullopt,
                QStringLiteral("A user component disagrees with its settings schema bytes: %1")
                    .arg(manifest.id),
            };
        }
        std::optional<SettingsSchema> settingsSchema;
        if (!entry.settingsSchema.isEmpty()) {
            auto parsed = parseSettingsSchema(entry.settingsSchema);
            if (!parsed) {
                return {
                    std::nullopt,
                    QStringLiteral("A user component has an invalid settings schema: %1")
                        .arg(manifest.id),
                };
            }
            settingsSchema = std::move(*parsed.value);
        }
        if (manifest.runtime.kind == RuntimeKind::DeclarativeV1) {
            const auto document = parseDeclarativeDocument(
                entry.declarativeRuntime,
                settingsSchema.has_value() ? &*settingsSchema : nullptr
            );
            if (!document
                || serializeDeclarativeDocument(*document.value)
                    != entry.declarativeRuntime) {
                return {
                    std::nullopt,
                    QStringLiteral("A user component has a missing or non-canonical declarative runtime: %1")
                        .arg(manifest.id),
                };
            }
        } else if (!entry.declarativeRuntime.isEmpty()) {
            return {
                std::nullopt,
                QStringLiteral("A non-declarative user component carries a declarative runtime: %1")
                    .arg(manifest.id),
            };
        }
        systemCatalog.entries_.insert(manifest.id, std::move(entry));
    }

    systemCatalog.catalogDigest_ = deriveCatalogDigest(systemCatalog.entries_);
    return {std::move(systemCatalog), QString()};
}

const QString &SystemCatalog::catalogDigest() const
{
    return catalogDigest_;
}

QStringList SystemCatalog::componentIds() const
{
    auto ids = entries_.keys();
    ids.sort();
    return ids;
}

const CatalogEntry *SystemCatalog::find(const QString &componentId) const
{
    const auto found = entries_.constFind(componentId);
    return found == entries_.constEnd() ? nullptr : &found.value();
}

} // namespace HyprShelld::Components
