#include "component_plan_source.h"

#include "component/component_contract.h"
#include "component/declarative_document.h"
#include "component/settings_schema.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <utility>

namespace HyprShelld::Components {
namespace {

constexpr qsizetype maximumCatalogComponents = 512;
constexpr qsizetype maximumSettingsSchemaBytes = 256 * 1024;

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

void addDigestField(
    QCryptographicHash &hash,
    const QByteArray &name,
    const QByteArray &value
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

QString deriveCatalogDigest(
    const QStringList &componentIds,
    const QHash<QString, RuntimeCatalogEntry> &entries
)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto &componentId : componentIds) {
        addDigestField(
            hash,
            componentId.toUtf8(),
            entries.value(componentId).packageDigest.toLatin1()
        );
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool isSortedUnique(const QStringList &values)
{
    QSet<QString> seen;
    QString previous;
    for (qsizetype index = 0; index < values.size(); ++index) {
        const auto &value = values.at(index);
        if (seen.contains(value)
            || (index > 0 && value <= previous)) {
            return false;
        }
        seen.insert(value);
        previous = value;
    }
    return true;
}

bool hasUniqueValues(const QStringList &values)
{
    QSet<QString> seen;
    for (const auto &value : values) {
        if (seen.contains(value)) {
            return false;
        }
        seen.insert(value);
    }
    return true;
}

bool boundedString(const QString &value, const qsizetype maximumLength)
{
    return value.size() <= maximumLength;
}

bool boundedStrings(
    const QStringList &values,
    const qsizetype maximumCount,
    const qsizetype maximumLength
)
{
    if (values.size() > maximumCount) {
        return false;
    }
    return std::ranges::all_of(values, [maximumLength](const QString &value) {
        return boundedString(value, maximumLength);
    });
}

bool recordFieldsAreBounded(const RuntimeCatalogComponentRecord &record)
{
    return boundedString(record.componentId, 255)
        && boundedString(record.componentType, 32)
        && boundedString(record.version, 64)
        && boundedString(record.name, 128)
        && boundedString(record.description, 4096)
        && boundedStrings(record.authorNames, 16, 128)
        && boundedStrings(record.authorEmails, 16, 254)
        && boundedStrings(record.authorHomepages, 16, 2048)
        && boundedString(record.license, 128)
        && boundedString(record.homepage, 2048)
        && boundedString(record.source, 2048)
        && boundedString(record.issues, 2048)
        && boundedString(record.componentApiVersion, 32)
        && boundedString(record.runtimeKind, 32)
        && boundedString(record.runtimeFactory, 64)
        && boundedString(record.runtimeEntryPoint, 255)
        && boundedStrings(record.runtimeArguments, 32, 1024)
        && record.declarativeRuntime.size() <= maximumDeclarativeDocumentBytes
        && record.settingsSchema.size() <= maximumSettingsSchemaBytes
        && boundedStrings(record.capabilityIds, 64, 255)
        && boundedStrings(record.capabilityReasons, 64, 1024)
        && boundedStrings(record.dependencyIds, 64, 255)
        && boundedStrings(
            record.dependencyVersionRequirements,
            64,
            256
        )
        && boundedString(record.packageDigest, 64)
        && boundedString(record.origin, 6);
}

ValidationResult<ComponentManifest> validateRecordManifest(
    const RuntimeCatalogComponentRecord &record
)
{
    ValidationResult<ComponentManifest> result;
    const auto origin = record.origin == QStringLiteral("system")
        ? std::optional(ComponentOrigin::System)
        : record.origin == QStringLiteral("user")
            ? std::optional(ComponentOrigin::User)
            : std::nullopt;
    if (!origin.has_value()
        || record.authorNames.size() != record.authorEmails.size()
        || record.authorNames.size() != record.authorHomepages.size()
        || record.capabilityIds.size() != record.capabilityReasons.size()
        || record.dependencyIds.size()
            != record.dependencyVersionRequirements.size()) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("component-runtime.invalid-manifest-arrays"),
            QStringLiteral("The manifest response contains mismatched parallel arrays.")
        );
        return result;
    }

    const auto runtimeKind = runtimeKindFromString(record.runtimeKind);
    if (!runtimeKind.has_value()) {
        addError(
            result.errors,
            QStringLiteral("$.runtime.kind"),
            QStringLiteral("component-runtime.invalid-runtime"),
            QStringLiteral("The runtime kind is unknown.")
        );
        return result;
    }

    QJsonObject runtime{{QStringLiteral("kind"), record.runtimeKind}};
    switch (*runtimeKind) {
    case RuntimeKind::BuiltinV1:
        if (!record.runtimeEntryPoint.isEmpty()
            || !record.runtimeArguments.isEmpty()) {
            addError(
                result.errors,
                QStringLiteral("$.runtime"),
                QStringLiteral("component-runtime.invalid-runtime-fields"),
                QStringLiteral("A built-in runtime may expose only its factory.")
            );
            return result;
        }
        runtime.insert(QStringLiteral("factory"), record.runtimeFactory);
        break;
    case RuntimeKind::DeclarativeV1:
    case RuntimeKind::QmlFullTrustV1:
        if (!record.runtimeFactory.isEmpty()
            || !record.runtimeArguments.isEmpty()) {
            addError(
                result.errors,
                QStringLiteral("$.runtime"),
                QStringLiteral("component-runtime.invalid-runtime-fields"),
                QStringLiteral("A declarative runtime may expose only its entry point.")
            );
            return result;
        }
        runtime.insert(
            QStringLiteral("entrypoint"),
            record.runtimeEntryPoint
        );
        break;
    case RuntimeKind::ProcessV1: {
        if (!record.runtimeFactory.isEmpty()) {
            addError(
                result.errors,
                QStringLiteral("$.runtime.factory"),
                QStringLiteral("component-runtime.invalid-runtime-fields"),
                QStringLiteral("A process runtime cannot expose a built-in factory.")
            );
            return result;
        }
        runtime.insert(
            QStringLiteral("entrypoint"),
            record.runtimeEntryPoint
        );
        QJsonArray arguments;
        for (const auto &argument : record.runtimeArguments) {
            arguments.append(argument);
        }
        runtime.insert(QStringLiteral("arguments"), arguments);
        break;
    }
    }

    QJsonArray authors;
    for (qsizetype index = 0; index < record.authorNames.size(); ++index) {
        QJsonObject author{
            {QStringLiteral("name"), record.authorNames.at(index)},
        };
        if (!record.authorEmails.at(index).isEmpty()) {
            author.insert(
                QStringLiteral("email"),
                record.authorEmails.at(index)
            );
        }
        if (!record.authorHomepages.at(index).isEmpty()) {
            author.insert(
                QStringLiteral("homepage"),
                record.authorHomepages.at(index)
            );
        }
        authors.append(author);
    }

    QJsonArray capabilities;
    for (qsizetype index = 0; index < record.capabilityIds.size(); ++index) {
        capabilities.append(QJsonObject{
            {QStringLiteral("id"), record.capabilityIds.at(index)},
            {QStringLiteral("reason"), record.capabilityReasons.at(index)},
        });
    }
    QJsonArray dependencies;
    for (qsizetype index = 0; index < record.dependencyIds.size(); ++index) {
        dependencies.append(QJsonObject{
            {QStringLiteral("id"), record.dependencyIds.at(index)},
            {
                QStringLiteral("version"),
                record.dependencyVersionRequirements.at(index)
            },
        });
    }

    QJsonObject manifest{
        {QStringLiteral("manifestVersion"),
         static_cast<qint64>(record.manifestVersion)},
        {QStringLiteral("id"), record.componentId},
        {QStringLiteral("version"), record.version},
        {QStringLiteral("type"), record.componentType},
        {QStringLiteral("name"), record.name},
        {QStringLiteral("description"), record.description},
        {QStringLiteral("authors"), authors},
        {QStringLiteral("license"), record.license},
        {QStringLiteral("componentApiVersion"), record.componentApiVersion},
        {QStringLiteral("runtime"), runtime},
        {QStringLiteral("requestedCapabilities"), capabilities},
        {QStringLiteral("dependencies"), dependencies},
    };
    for (const auto &[key, value] : {
             std::pair{QStringLiteral("homepage"), record.homepage},
             std::pair{QStringLiteral("source"), record.source},
             std::pair{QStringLiteral("issues"), record.issues},
         }) {
        if (!value.isEmpty()) {
            manifest.insert(key, value);
        }
    }
    if (!record.settingsSchema.isEmpty()) {
        manifest.insert(
            QStringLiteral("settingsSchema"),
            QStringLiteral("settings.schema.json")
        );
    }

    const auto bytes = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    return parseComponentManifest(QByteArrayView(bytes), *origin);
}

} // namespace

ValidationResult<QStringList> validateRuntimeCatalogListing(
    const QStringList &listedComponentIds,
    const QString &catalogDigest
)
{
    ValidationResult<QStringList> result;
    if (!isFullSha256Digest(catalogDigest)) {
        addError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("component-runtime.invalid-catalog-digest"),
            QStringLiteral("The catalog digest must be lowercase SHA-256.")
        );
        return result;
    }
    if (listedComponentIds.isEmpty()
        || listedComponentIds.size() > maximumCatalogComponents
        || !std::ranges::all_of(
            listedComponentIds,
            [](const QString &componentId) {
                return isValidComponentId(componentId);
            }
        )
        || !isSortedUnique(listedComponentIds)) {
        addError(
            result.errors,
            QStringLiteral("$.componentIds"),
            QStringLiteral("component-runtime.invalid-catalog-id-set"),
            QStringLiteral("The component IDs must be a bounded sorted unique set of valid IDs.")
        );
        return result;
    }

    result.value = listedComponentIds;
    return result;
}

ValidationResult<ValidatedRuntimeCatalogRecord>
validateRuntimeCatalogRecord(const RuntimeCatalogComponentRecord &record)
{
    ValidationResult<ValidatedRuntimeCatalogRecord> result;
    if (!isValidComponentId(record.componentId)) {
        addError(
            result.errors,
            QStringLiteral("$.componentId"),
            QStringLiteral("component-runtime.invalid-catalog-record-id"),
            QStringLiteral("The record component ID is invalid.")
        );
        return result;
    }
    if (!recordFieldsAreBounded(record)) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("component-runtime.catalog-record-limit"),
            QStringLiteral("The catalog record contains oversized fields or arrays.")
        );
        return result;
    }

    const auto systemOrigin = record.origin == QStringLiteral("system");
    const auto userOrigin = record.origin == QStringLiteral("user");
    auto manifest = validateRecordManifest(record);
    if (!manifest
        || record.componentApiVersion
            != QLatin1StringView(currentComponentApiVersion)
        || (!systemOrigin && !userOrigin)
        || record.removable != userOrigin
        || !isFullSha256Digest(record.packageDigest)) {
        if (!manifest) {
            result.errors += manifest.errors;
        }
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("component-runtime.invalid-catalog-record"),
            QStringLiteral("The catalog record violates the component authority contract.")
        );
        return result;
    }
    if (record.capabilityIds.size() != record.capabilityReasons.size()
        || !hasUniqueValues(record.capabilityIds)
        || record.dependencyIds.size()
            != record.dependencyVersionRequirements.size()
        || !hasUniqueValues(record.dependencyIds)) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("component-runtime.invalid-catalog-arrays"),
            QStringLiteral("Catalog arrays are mismatched or contain duplicate values.")
        );
        return result;
    }

    QSet<QString> requestedCapabilities;
    bool identifiersValid = true;
    for (const auto &capabilityId : record.capabilityIds) {
        identifiersValid = identifiersValid
            && isValidCapabilityId(capabilityId);
        requestedCapabilities.insert(capabilityId);
    }
    for (const auto &dependencyId : record.dependencyIds) {
        identifiersValid = identifiersValid
            && isValidComponentId(dependencyId)
            && dependencyId != record.componentId;
    }
    if (!identifiersValid) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("component-runtime.invalid-catalog-identifier"),
            QStringLiteral("A capability or dependency identifier is invalid.")
        );
        return result;
    }

    SettingsSchema schema;
    if (!record.settingsSchema.isEmpty()) {
        auto parsedSchema = parseSettingsSchema(
            QByteArrayView(record.settingsSchema)
        );
        if (!parsedSchema) {
            result.errors += parsedSchema.errors;
            return result;
        }
        schema = std::move(*parsedSchema.value);
    }

    std::optional<DeclarativeDocument> declarativeDocument;
    if (!record.declarativeRuntime.isEmpty()) {
        if (manifest.value->runtime.kind != RuntimeKind::DeclarativeV1) {
            addError(
                result.errors,
                QStringLiteral("$.runtime"),
                QStringLiteral("component-runtime.unexpected-declarative-runtime"),
                QStringLiteral("Only declarative-v1 may expose a trusted declarative document.")
            );
            return result;
        }
        auto parsedDocument = parseDeclarativeDocument(
            QByteArrayView(record.declarativeRuntime),
            &schema
        );
        if (!parsedDocument
            || serializeDeclarativeDocument(*parsedDocument.value)
                != record.declarativeRuntime) {
            if (!parsedDocument) {
                result.errors += parsedDocument.errors;
            }
            addError(
                result.errors,
                QStringLiteral("$.runtime"),
                QStringLiteral("component-runtime.invalid-declarative-runtime"),
                QStringLiteral("The declarative runtime must be canonical trusted data.")
            );
            return result;
        }
        declarativeDocument = std::move(*parsedDocument.value);
    } else if (manifest.value->origin == ComponentOrigin::User
        && manifest.value->runtime.kind == RuntimeKind::DeclarativeV1
        && validateCurrentHostSupport(*manifest.value).isEmpty()) {
        addError(
            result.errors,
            QStringLiteral("$.runtime"),
            QStringLiteral("component-runtime.missing-declarative-runtime"),
            QStringLiteral("A supported declarative package must expose its canonical document.")
        );
        return result;
    }

    const auto activationSupported =
        validateCurrentHostSupport(*manifest.value).isEmpty()
        && (manifest.value->runtime.kind != RuntimeKind::DeclarativeV1
            || declarativeDocument.has_value());

    result.value = ValidatedRuntimeCatalogRecord{
        .configurationEntry = {
            .packageDigest = record.packageDigest,
            .type = manifest.value->type,
            .origin = systemOrigin
                ? ComponentOrigin::System
                : ComponentOrigin::User,
            .settingsSchema = std::move(schema),
            .requestedCapabilities = std::move(requestedCapabilities),
            .componentApiVersion = record.componentApiVersion,
            .runtimeKind = *runtimeKindFromString(record.runtimeKind),
            .dependencyIds = record.dependencyIds,
            .activationSupported = activationSupported,
            .declarativeRuntime = record.declarativeRuntime,
        },
        .runtimeEntry = {
            .componentId = record.componentId,
            .componentType = record.componentType,
            .packageDigest = record.packageDigest,
            .origin = record.origin,
            .removable = record.removable,
            .componentApiVersion = record.componentApiVersion,
            .runtimeKind = record.runtimeKind,
            .factory = record.runtimeFactory,
            .runtimeEntryPoint = record.runtimeEntryPoint,
            .runtimeArguments = record.runtimeArguments,
            .declarativeRuntime = record.declarativeRuntime,
            .declarativeDocument = std::move(declarativeDocument),
            .capabilityIds = record.capabilityIds,
            .dependencyIds = record.dependencyIds,
        },
    };
    return result;
}

ValidationResult<HydratedRuntimeCatalog> hydrateRuntimeCatalog(
    const QStringList &listedComponentIds,
    const QString &catalogDigest,
    const QVector<RuntimeCatalogComponentRecord> &records
)
{
    ValidationResult<HydratedRuntimeCatalog> result;
    const auto listing = validateRuntimeCatalogListing(
        listedComponentIds,
        catalogDigest
    );
    if (!listing) {
        result.errors = listing.errors;
        return result;
    }
    if (records.size() != listedComponentIds.size()) {
        addError(
            result.errors,
            QStringLiteral("$.components"),
            QStringLiteral("component-runtime.incomplete-catalog"),
            QStringLiteral("Every listed component requires exactly one hydrated record.")
        );
        return result;
    }

    HydratedRuntimeCatalog hydrated;
    hydrated.configurationCatalog.digest = catalogDigest;
    hydrated.runtimeCatalog.catalogDigest = catalogDigest;
    hydrated.runtimeCatalog.listedComponentIds = listedComponentIds;

    QSet<QString> recordIds;
    for (qsizetype index = 0; index < records.size(); ++index) {
        const auto &record = records.at(index);
        const auto path = QStringLiteral("$.components[%1]").arg(index);
        if (!isValidComponentId(record.componentId)
            || !listedComponentIds.contains(record.componentId)
            || recordIds.contains(record.componentId)) {
            addError(
                result.errors,
                path + QStringLiteral(".componentId"),
                QStringLiteral("component-runtime.invalid-catalog-record-id"),
                QStringLiteral("A record ID is invalid, duplicated, or was not listed.")
            );
            continue;
        }
        recordIds.insert(record.componentId);

        auto validated = validateRuntimeCatalogRecord(record);
        if (!validated) {
            result.errors += validated.errors;
            continue;
        }

        hydrated.configurationCatalog.entries.insert(
            record.componentId,
            std::move(validated.value->configurationEntry)
        );
        hydrated.runtimeCatalog.entries.insert(
            record.componentId,
            std::move(validated.value->runtimeEntry)
        );
    }

    if (!result.errors.isEmpty()) {
        return result;
    }
    if (recordIds.size() != listedComponentIds.size()
        || hydrated.runtimeCatalog.entries.size()
            != listedComponentIds.size()
        || deriveCatalogDigest(
               listedComponentIds,
               hydrated.runtimeCatalog.entries
           ) != catalogDigest) {
        addError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("component-runtime.catalog-digest-mismatch"),
            QStringLiteral("The hydrated records do not reproduce the catalog digest.")
        );
        return result;
    }

    result.value = std::move(hydrated);
    return result;
}

ValidationResult<RuntimeConfigurationSnapshot> hydrateRuntimeConfiguration(
    const QByteArrayView snapshotBytes,
    const quint64 snapshotRevision,
    const QString &snapshotCatalogDigest,
    const ConfigurationCatalog &catalog
)
{
    ValidationResult<RuntimeConfigurationSnapshot> result;
    if (!isFullSha256Digest(snapshotCatalogDigest)
        || snapshotCatalogDigest != catalog.digest) {
        addError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("component-runtime.configuration-catalog-mismatch"),
            QStringLiteral("The configuration snapshot belongs to another catalog.")
        );
        return result;
    }

    auto parsed = parseComponentConfiguration(snapshotBytes, catalog);
    if (!parsed) {
        result.errors = std::move(parsed.errors);
        return result;
    }
    if (parsed.value->revision != snapshotRevision) {
        addError(
            result.errors,
            QStringLiteral("$.revision"),
            QStringLiteral("component-runtime.configuration-revision-mismatch"),
            QStringLiteral("The embedded and D-Bus snapshot revisions differ.")
        );
        return result;
    }

    RuntimeConfigurationSnapshot runtime;
    runtime.catalogDigest = snapshotCatalogDigest;
    runtime.revision = snapshotRevision;
    for (auto iterator = parsed.value->components.cbegin();
         iterator != parsed.value->components.cend(); ++iterator) {
        runtime.components.insert(iterator.key(), {
            .enabled = iterator->enabled,
            .packageDigest = iterator->packageDigest,
            .grantedCapabilities = iterator->grantedCapabilities,
            .settings = iterator->settings,
        });
    }
    for (auto iterator = parsed.value->instances.cbegin();
         iterator != parsed.value->instances.cend(); ++iterator) {
        runtime.instances.insert(iterator.key(), {
            .componentId = iterator->componentId,
            .enabled = iterator->enabled,
            .settings = iterator->settings,
        });
    }
    for (auto iterator = parsed.value->bars.cbegin();
         iterator != parsed.value->bars.cend(); ++iterator) {
        runtime.barLayouts.insert(iterator.key(), {
            .outputMode = iterator->outputs.mode,
            .start = iterator->start,
            .center = iterator->center,
            .end = iterator->end,
        });
    }

    result.value = std::move(runtime);
    return result;
}

} // namespace HyprShelld::Components
